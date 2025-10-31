/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SecurityTap.h"
#include "YARAScanWorkerPool.h"
#include <AK/Base64.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Socket.h>
#include <LibThreading/BackgroundAction.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace RequestServer {

ErrorOr<NonnullOwnPtr<SecurityTap>> SecurityTap::create()
{
    // Connect to Sentinel daemon
    auto socket = TRY(Core::LocalSocket::connect("/tmp/sentinel.sock"_string.to_byte_string()));

    // Set socket timeout to 5 seconds (fail-fast if Sentinel hangs)
    // FIXME: Add timeout configuration

    auto security_tap = adopt_own(*new SecurityTap(move(socket)));
    dbgln("SecurityTap: Connected to Sentinel daemon");

    // Initialize worker pool for async YARA scanning
    auto worker_pool = TRY(YARAScanWorkerPool::create(security_tap.ptr(), 4));
    TRY(worker_pool->start());
    security_tap->m_worker_pool = move(worker_pool);
    dbgln("SecurityTap: Worker pool started with 4 threads");

    return security_tap;
}

SecurityTap::SecurityTap(NonnullOwnPtr<Core::Socket> socket)
    : m_sentinel_socket(move(socket))
    , m_connection_failed(false)
{
}

SecurityTap::~SecurityTap()
{
    // Stop worker pool on destruction
    if (m_worker_pool) {
        auto result = m_worker_pool->stop();
        if (result.is_error()) {
            dbgln("SecurityTap: Error stopping worker pool: {}", result.error());
        }
    }
}

bool SecurityTap::is_connected() const
{
    return !m_connection_failed && m_sentinel_socket->is_open();
}

ErrorOr<void> SecurityTap::reconnect()
{
    dbgln("SecurityTap: Attempting to reconnect to Sentinel...");

    // Try to connect to Sentinel daemon
    auto socket_result = Core::LocalSocket::connect("/tmp/sentinel.sock"_string.to_byte_string());
    if (socket_result.is_error()) {
        dbgln("SecurityTap: Reconnection failed: {}", socket_result.error());
        return socket_result.release_error();
    }

    m_sentinel_socket = socket_result.release_value();
    m_connection_failed = false;

    dbgln("SecurityTap: Successfully reconnected to Sentinel");
    return {};
}

ErrorOr<ByteString> SecurityTap::compute_sha256(ReadonlyBytes data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
        return Error::from_string_literal("Failed to create EVP context");

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return Error::from_string_literal("Failed to initialize SHA256");
    }

    if (EVP_DigestUpdate(context, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(context);
        return Error::from_string_literal("Failed to update SHA256");
    }

    if (EVP_DigestFinal_ex(context, hash, nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return Error::from_string_literal("Failed to finalize SHA256");
    }

    EVP_MD_CTX_free(context);

    StringBuilder hex_builder;
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++)
        hex_builder.appendff("{:02x}", hash[i]);

    return hex_builder.to_byte_string();
}

ErrorOr<SecurityTap::ScanResult> SecurityTap::inspect_download(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Use size-based scanning strategy
    auto start_time = Core::ElapsedTimer::start_new();
    auto result = TRY(scan_with_size_limits(metadata, content));
    auto elapsed_ms = start_time.elapsed_milliseconds();

    // Update telemetry
    if (m_scan_size_config.enable_telemetry) {
        m_telemetry.total_files_scanned++;
        m_telemetry.total_scan_time_ms += elapsed_ms;
    }

    return result;
}

ErrorOr<SecurityTap::ScanResult> SecurityTap::scan_with_size_limits(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    auto file_size = content.size();
    auto tier = m_scan_size_config.get_tier_name(file_size);

    dbgln("SecurityTap: Scanning file '{}' (size: {}MB, tier: {})",
        metadata.filename, file_size / (1024 * 1024), tier);

    // Small files: Load into memory, scan normally (fast path)
    if (file_size <= m_scan_size_config.small_file_threshold) {
        if (m_scan_size_config.enable_telemetry)
            m_telemetry.scans_small++;
        return scan_small_file(metadata, content);
    }

    // Medium files: Stream in chunks, scan incrementally
    if (file_size <= m_scan_size_config.medium_file_threshold) {
        if (m_scan_size_config.enable_telemetry)
            m_telemetry.scans_medium++;
        return scan_medium_file_streaming(metadata, content);
    }

    // Large files: Partial scanning or skip
    if (file_size <= m_scan_size_config.max_scan_size) {
        if (m_scan_size_config.scan_large_files_partially) {
            if (m_scan_size_config.enable_telemetry)
                m_telemetry.scans_large_partial++;
            return scan_large_file_partial(metadata, content);
        } else {
            dbgln("SecurityTap: Large file scanning disabled, skipping file ({}MB)",
                file_size / (1024 * 1024));
            if (m_scan_size_config.enable_telemetry)
                m_telemetry.scans_oversized_skipped++;
            return ScanResult { .is_threat = false, .alert_json = {} };
        }
    }

    // Oversized files: Skip scanning
    dbgln("SecurityTap: File exceeds maximum scan size ({}MB > {}MB), skipping",
        file_size / (1024 * 1024), m_scan_size_config.max_scan_size / (1024 * 1024));
    if (m_scan_size_config.enable_telemetry)
        m_telemetry.scans_oversized_skipped++;
    return ScanResult { .is_threat = false, .alert_json = {} };
}

ErrorOr<SecurityTap::ScanResult> SecurityTap::scan_small_file(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Fast path: Send entire file to Sentinel
    auto response_json_result = send_scan_request(metadata, content);

    // Handle Sentinel communication failures with fail-open policy
    if (response_json_result.is_error()) {
        dbgln("SecurityTap: Sentinel scan request failed: {}", response_json_result.error());
        dbgln("SecurityTap: Allowing download without scanning (fail-open)");
        // Fail-open: Allow download when Sentinel is unavailable
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    auto response_json = response_json_result.release_value();

    // Parse response
    auto json_result = JsonValue::from_string(response_json);
    if (json_result.is_error()) {
        dbgln("SecurityTap: Failed to parse Sentinel response: {}", json_result.error());
        dbgln("SecurityTap: Allowing download without scanning (parse error)");
        // Fail-open: Allow download when response is malformed
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    auto json = json_result.value();
    if (!json.is_object()) {
        dbgln("SecurityTap: Sentinel response is not a JSON object");
        dbgln("SecurityTap: Allowing download without scanning (invalid response)");
        // Fail-open: Allow download when response format is invalid
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    auto obj = json.as_object();

    // Check status
    auto status = obj.get_string("status"sv);
    if (!status.has_value()) {
        dbgln("SecurityTap: Missing 'status' field in Sentinel response");
        dbgln("SecurityTap: Allowing download without scanning (missing status)");
        // Fail-open: Allow download when status field is missing
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    if (status.value() != "success"sv) {
        auto error_str = obj.get_string("error"sv);
        auto error = error_str.has_value() ? error_str.value() : "Unknown error"_string;
        dbgln("SecurityTap: Sentinel scan failed: {}", error);
        dbgln("SecurityTap: Allowing download without scanning (scan error)");
        // Fail-open: Allow download when scan fails (e.g., YARA rules compilation error)
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    // Check result
    auto result = obj.get_string("result"sv);
    if (!result.has_value()) {
        dbgln("SecurityTap: Missing 'result' field in Sentinel response");
        dbgln("SecurityTap: Allowing download without scanning (missing result)");
        // Fail-open: Allow download when result field is missing
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    // If result is "clean", no threat detected
    if (result.value() == "clean"sv) {
        dbgln("SecurityTap: File clean: {}", metadata.filename);
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    // Otherwise, result contains threat detection JSON
    dbgln("SecurityTap: Threat detected in {}: {}", metadata.filename, result.value());

    return ScanResult {
        .is_threat = true,
        .alert_json = ByteString(result.value())
    };
}

ErrorOr<ByteString> SecurityTap::send_scan_request(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Check if connection is alive, attempt reconnect if needed
    if (m_connection_failed) {
        auto reconnect_result = reconnect();
        if (reconnect_result.is_error()) {
            return Error::from_string_literal("Sentinel connection lost and reconnection failed");
        }
    }

    // Build JSON request for Sentinel
    JsonObject request;
    request.set("action"sv, JsonValue("scan_content"sv));
    request.set("request_id"sv, JsonValue(ByteString::formatted("download_{}", metadata.sha256)));

    // Base64 encode content for JSON transport
    auto content_base64 = TRY(encode_base64(content));
    request.set("content"sv, JsonValue(content_base64));

    // Serialize and send
    auto request_json = request.serialized();
    auto request_bytes = request_json.bytes();

    auto write_result = m_sentinel_socket->write_until_depleted(request_bytes);
    if (write_result.is_error()) {
        m_connection_failed = true;
        dbgln("SecurityTap: Failed to write to Sentinel socket: {}", write_result.error());
        return write_result.release_error();
    }

    // Add newline delimiter (Sentinel expects JSON Lines format)
    u8 newline = '\n';
    auto newline_result = m_sentinel_socket->write_until_depleted({ &newline, 1 });
    if (newline_result.is_error()) {
        m_connection_failed = true;
        dbgln("SecurityTap: Failed to write newline to Sentinel socket: {}", newline_result.error());
        return newline_result.release_error();
    }

    // Read response (blocking with timeout)
    auto response_buffer = TRY(ByteBuffer::create_uninitialized(4096));
    auto bytes_read_result = m_sentinel_socket->read_some(response_buffer);

    if (bytes_read_result.is_error()) {
        m_connection_failed = true;
        dbgln("SecurityTap: Failed to read from Sentinel socket: {}", bytes_read_result.error());
        return bytes_read_result.release_error();
    }

    auto bytes_read = bytes_read_result.release_value();

    if (bytes_read.is_empty()) {
        m_connection_failed = true;
        return Error::from_string_literal("Sentinel socket closed");
    }

    return ByteString(bytes_read);
}

ErrorOr<SecurityTap::ScanResult> SecurityTap::scan_medium_file_streaming(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Stream file in chunks to reduce memory usage
    dbgln("SecurityTap: Streaming scan for medium file ({}MB)",
        content.size() / (1024 * 1024));

    auto chunk_size = m_scan_size_config.chunk_size;
    auto overlap_size = m_scan_size_config.chunk_overlap_size;

    size_t offset = 0;
    size_t peak_memory = 0;

    while (offset < content.size()) {
        // Calculate chunk boundaries
        size_t remaining = content.size() - offset;
        size_t current_chunk_size = min(chunk_size, remaining);

        // Extract chunk
        auto chunk = content.slice(offset, current_chunk_size);

        // Track memory usage
        size_t current_memory = chunk.size();
        if (current_memory > peak_memory)
            peak_memory = current_memory;

        // Send chunk to Sentinel for scanning
        auto response_json_result = send_scan_request(metadata, chunk);

        if (response_json_result.is_error()) {
            dbgln("SecurityTap: Chunk scan failed at offset {}: {}",
                offset, response_json_result.error());
            dbgln("SecurityTap: Allowing download without complete scanning (fail-open)");
            return ScanResult { .is_threat = false, .alert_json = {} };
        }

        auto response_json = response_json_result.release_value();

        // Parse response to check for threats
        auto json_result = JsonValue::from_string(response_json);
        if (json_result.is_error()) {
            dbgln("SecurityTap: Failed to parse chunk scan response");
            continue;
        }

        auto json = json_result.value();
        if (!json.is_object())
            continue;

        auto obj = json.as_object();
        auto result = obj.get_string("result"sv);

        // If threat detected in this chunk, stop and report
        if (result.has_value() && result.value() != "clean"sv) {
            dbgln("SecurityTap: Threat detected in chunk at offset {}", offset);
            if (m_scan_size_config.enable_telemetry) {
                m_telemetry.total_bytes_scanned += (offset + current_chunk_size);
                if (peak_memory > m_telemetry.peak_memory_usage)
                    m_telemetry.peak_memory_usage = peak_memory;
            }
            return ScanResult {
                .is_threat = true,
                .alert_json = ByteString(result.value())
            };
        }

        // Move to next chunk with overlap to catch patterns at boundaries
        if (offset + current_chunk_size >= content.size())
            break;

        offset += current_chunk_size;
        if (offset > overlap_size)
            offset -= overlap_size;
    }

    // No threats found in any chunk
    if (m_scan_size_config.enable_telemetry) {
        m_telemetry.total_bytes_scanned += content.size();
        if (peak_memory > m_telemetry.peak_memory_usage)
            m_telemetry.peak_memory_usage = peak_memory;
    }

    dbgln("SecurityTap: Medium file clean after streaming scan (peak memory: {}KB)",
        peak_memory / 1024);
    return ScanResult { .is_threat = false, .alert_json = {} };
}

ErrorOr<SecurityTap::ScanResult> SecurityTap::scan_large_file_partial(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Partial scanning: scan first and last portions only
    // Rationale: Most malware is at beginning (headers, payloads) or end (appended data)
    auto scan_bytes = m_scan_size_config.large_file_scan_bytes;
    auto file_size = content.size();

    dbgln("SecurityTap: Partial scan for large file ({}MB): scanning first+last {}MB",
        file_size / (1024 * 1024), (2 * scan_bytes) / (1024 * 1024));

    // Scan first portion
    size_t first_portion_size = min(scan_bytes, file_size);
    auto first_portion = content.slice(0, first_portion_size);

    auto first_result = send_scan_request(metadata, first_portion);
    if (first_result.is_error()) {
        dbgln("SecurityTap: First portion scan failed: {}", first_result.error());
        dbgln("SecurityTap: Allowing download without scanning (fail-open)");
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    // Check first portion result
    auto first_json = JsonValue::from_string(first_result.value());
    if (!first_json.is_error() && first_json.value().is_object()) {
        auto obj = first_json.value().as_object();
        auto result = obj.get_string("result"sv);
        if (result.has_value() && result.value() != "clean"sv) {
            dbgln("SecurityTap: Threat detected in first portion of large file");
            if (m_scan_size_config.enable_telemetry)
                m_telemetry.total_bytes_scanned += first_portion_size;
            return ScanResult {
                .is_threat = true,
                .alert_json = ByteString(result.value())
            };
        }
    }

    // Scan last portion (if file is large enough)
    if (file_size > scan_bytes) {
        size_t last_offset = file_size - min(scan_bytes, file_size);
        size_t last_portion_size = file_size - last_offset;
        auto last_portion = content.slice(last_offset, last_portion_size);

        auto last_result = send_scan_request(metadata, last_portion);
        if (last_result.is_error()) {
            dbgln("SecurityTap: Last portion scan failed: {}", last_result.error());
            dbgln("SecurityTap: Allowing download without complete scanning (fail-open)");
            return ScanResult { .is_threat = false, .alert_json = {} };
        }

        // Check last portion result
        auto last_json = JsonValue::from_string(last_result.value());
        if (!last_json.is_error() && last_json.value().is_object()) {
            auto obj = last_json.value().as_object();
            auto result = obj.get_string("result"sv);
            if (result.has_value() && result.value() != "clean"sv) {
                dbgln("SecurityTap: Threat detected in last portion of large file");
                if (m_scan_size_config.enable_telemetry)
                    m_telemetry.total_bytes_scanned += (first_portion_size + last_portion_size);
                return ScanResult {
                    .is_threat = true,
                    .alert_json = ByteString(result.value())
                };
            }
        }
    }

    // No threats found in partial scan
    if (m_scan_size_config.enable_telemetry) {
        size_t bytes_scanned = first_portion_size;
        if (file_size > scan_bytes)
            bytes_scanned += min(scan_bytes, file_size - scan_bytes);
        m_telemetry.total_bytes_scanned += bytes_scanned;
    }

    dbgln("SecurityTap: Large file clean after partial scan (scanned {}MB of {}MB)",
        (first_portion_size + (file_size > scan_bytes ? min(scan_bytes, file_size - scan_bytes) : 0)) / (1024 * 1024),
        file_size / (1024 * 1024));
    return ScanResult { .is_threat = false, .alert_json = {} };
}

void SecurityTap::async_inspect_download(
    DownloadMetadata const& metadata,
    ReadonlyBytes content,
    ScanCallback callback)
{
    // Use size-based limits for async scanning too
    if (content.size() > m_scan_size_config.max_scan_size) {
        dbgln("SecurityTap: Skipping scan for oversized file ({}MB) - async", content.size() / (1024 * 1024));
        Core::EventLoop::current().deferred_invoke([callback = move(callback)]() mutable {
            callback(ScanResult { .is_threat = false, .alert_json = {} });
        });
        return;
    }

    // If worker pool is available, use it for async scanning
    if (m_worker_pool) {
        // Copy content for worker thread
        auto content_buffer_result = ByteBuffer::copy(content);
        if (content_buffer_result.is_error()) {
            Core::EventLoop::current().deferred_invoke([callback = move(callback), error = content_buffer_result.release_error()]() mutable {
                callback(move(error));
            });
            return;
        }

        // Enqueue scan request to worker pool
        auto enqueue_result = m_worker_pool->enqueue_scan(metadata, content_buffer_result.release_value(), move(callback));
        if (enqueue_result.is_error()) {
            // Queue is full or worker pool error - fail-open with warning
            dbgln("SecurityTap: Failed to enqueue scan ({}), allowing download", enqueue_result.error());
            Core::EventLoop::current().deferred_invoke([callback = move(callback)]() mutable {
                callback(ScanResult { .is_threat = false, .alert_json = {} });
            });
            return;
        }

        // Successfully enqueued - callback will be invoked by worker thread
        return;
    }

    // Fallback: Use Threading::BackgroundAction if worker pool not available
    // This preserves backward compatibility
    DownloadMetadata metadata_copy = metadata;
    auto content_buffer_result = ByteBuffer::copy(content);
    if (content_buffer_result.is_error()) {
        Core::EventLoop::current().deferred_invoke([callback = move(callback), error = content_buffer_result.release_error()]() mutable {
            callback(move(error));
        });
        return;
    }
    auto content_buffer = content_buffer_result.release_value();

    [[maybe_unused]] auto action = Threading::BackgroundAction<ScanResult>::construct(
        [this, metadata_copy = move(metadata_copy), content_buffer = move(content_buffer)](auto&) -> ErrorOr<ScanResult> {
            return inspect_download(metadata_copy, content_buffer.bytes());
        },
        [callback = move(callback)](ScanResult result) -> ErrorOr<void> {
            callback(move(result));
            return {};
        }
    );
}

Optional<SecurityTap::WorkerPoolTelemetry> SecurityTap::get_worker_pool_telemetry() const
{
    if (!m_worker_pool) {
        return {};
    }

    auto telemetry = m_worker_pool->get_telemetry();

    double avg_scan_time_ms = 0.0;
    if (telemetry.total_scans_completed > 0) {
        avg_scan_time_ms = static_cast<double>(telemetry.total_scan_time.to_milliseconds()) /
                          static_cast<double>(telemetry.total_scans_completed);
    }

    return WorkerPoolTelemetry {
        .total_scans_completed = telemetry.total_scans_completed,
        .total_scans_failed = telemetry.total_scans_failed,
        .current_queue_depth = telemetry.current_queue_depth,
        .active_workers = telemetry.active_workers,
        .avg_scan_time_ms = avg_scan_time_ms,
    };
}

}
