/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ScanSizeConfig.h"
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <LibCore/Socket.h>

namespace RequestServer {

// Forward declaration
class YARAScanWorkerPool;

class SecurityTap {
public:
    static ErrorOr<NonnullOwnPtr<SecurityTap>> create();
    ~SecurityTap();

    struct DownloadMetadata {
        ByteString url;
        ByteString filename;
        ByteString mime_type;
        ByteString sha256;
        size_t size_bytes { 0 };
    };

    struct ScanResult {
        bool is_threat { false };
        Optional<ByteString> alert_json;
    };

    using ScanCallback = Function<void(ErrorOr<ScanResult>)>;

    // Main inspection method - sends download to Sentinel for YARA scanning
    ErrorOr<ScanResult> inspect_download(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    // Async inspection method - non-blocking, returns immediately and calls callback
    void async_inspect_download(
        DownloadMetadata const& metadata,
        ReadonlyBytes content,
        ScanCallback callback
    );

    // Compute SHA256 hash of content
    static ErrorOr<ByteString> compute_sha256(ReadonlyBytes data);

    // Check if connection to Sentinel is still alive
    bool is_connected() const;

    // Attempt to reconnect to Sentinel
    ErrorOr<void> reconnect();

    // Set scan size configuration
    void set_scan_size_config(ScanSizeConfig const& config) { m_scan_size_config = config; }
    ScanSizeConfig const& scan_size_config() const { return m_scan_size_config; }

    // Get scan telemetry
    ScanTelemetry const& telemetry() const { return m_telemetry; }
    void reset_telemetry() { m_telemetry.reset(); }

    // Get worker pool telemetry (for async scans)
    struct WorkerPoolTelemetry {
        size_t total_scans_completed;
        size_t total_scans_failed;
        size_t current_queue_depth;
        size_t active_workers;
        double avg_scan_time_ms;
    };
    Optional<WorkerPoolTelemetry> get_worker_pool_telemetry() const;

private:
    SecurityTap(NonnullOwnPtr<Core::Socket> socket);

    ErrorOr<ByteString> send_scan_request(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    // Size-based scanning methods
    ErrorOr<ScanResult> scan_with_size_limits(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    ErrorOr<ScanResult> scan_small_file(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    ErrorOr<ScanResult> scan_medium_file_streaming(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    ErrorOr<ScanResult> scan_large_file_partial(
        DownloadMetadata const& metadata,
        ReadonlyBytes content
    );

    NonnullOwnPtr<Core::Socket> m_sentinel_socket;
    bool m_connection_failed { false };
    ScanSizeConfig m_scan_size_config { ScanSizeConfig::create_default() };
    ScanTelemetry m_telemetry;
    OwnPtr<YARAScanWorkerPool> m_worker_pool;
};

}
