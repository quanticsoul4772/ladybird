/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SentinelServer.h"
#include "InputValidator.h"
#include "MalwareML.h"
#include "PolicyGraph.h"
#include "ThreatFeed.h"
#include <AK/Base64.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibIPC/BufferedIPCReader.h>
#include <LibIPC/BufferedIPCWriter.h>
#include <sys/stat.h>
#include <yara.h>

// Undefine YARA macros that conflict with AK classes
#ifdef get_string
#    undef get_string
#endif
#ifdef set
#    undef set
#endif

namespace Sentinel {

static YR_RULES* s_yara_rules = nullptr;

static ErrorOr<void> initialize_yara()
{
    dbgln("Sentinel: Initializing YARA");

    int result = yr_initialize();
    if (result != ERROR_SUCCESS)
        return Error::from_string_literal("Failed to initialize YARA");

    YR_COMPILER* compiler = nullptr;
    result = yr_compiler_create(&compiler);
    if (result != ERROR_SUCCESS) {
        yr_finalize();
        return Error::from_string_literal("Failed to create YARA compiler");
    }

    // Load default rules from Services/Sentinel/rules/default.yar
    ByteString rules_path = "/home/rbsmith4/ladybird/Services/Sentinel/rules/default.yar"sv;
    auto rules_file = Core::File::open(rules_path, Core::File::OpenMode::Read);

    if (rules_file.is_error()) {
        dbgln("Sentinel: Failed to open YARA rules at {}: {}", rules_path, rules_file.error());
        yr_compiler_destroy(compiler);
        yr_finalize();
        return Error::from_string_literal("Failed to open YARA rules file");
    }

    auto rules_content = rules_file.value()->read_until_eof();
    if (rules_content.is_error()) {
        dbgln("Sentinel: Failed to read YARA rules: {}", rules_content.error());
        yr_compiler_destroy(compiler);
        yr_finalize();
        return Error::from_string_literal("Failed to read YARA rules file");
    }

    // Add rules to compiler
    result = yr_compiler_add_string(compiler,
        reinterpret_cast<char const*>(rules_content.value().data()),
        nullptr);

    if (result != 0) {
        dbgln("Sentinel: Failed to compile YARA rules");
        yr_compiler_destroy(compiler);
        yr_finalize();
        return Error::from_string_literal("Failed to compile YARA rules");
    }

    // Get compiled rules
    result = yr_compiler_get_rules(compiler, &s_yara_rules);
    if (result != ERROR_SUCCESS) {
        yr_compiler_destroy(compiler);
        yr_finalize();
        return Error::from_string_literal("Failed to get compiled YARA rules");
    }

    yr_compiler_destroy(compiler);

    dbgln("Sentinel: YARA initialized successfully");
    return {};
}

ErrorOr<NonnullOwnPtr<SentinelServer>> SentinelServer::create()
{
    // Initialize YARA rules
    TRY(initialize_yara());

    // Remove stale socket file if it exists (from previous unclean shutdown)
    auto socket_path = "/tmp/sentinel.sock"sv;
    if (FileSystem::exists(socket_path)) {
        // Socket exists - try to remove it
        auto remove_result = FileSystem::remove(socket_path, FileSystem::RecursionMode::Disallowed);
        if (remove_result.is_error()) {
            dbgln("Sentinel: Warning: Could not remove stale socket: {}", remove_result.error());
            // Continue anyway - maybe we can still bind
        } else {
            dbgln("Sentinel: Removed stale socket file");
        }
    }

    auto server = Core::LocalServer::construct();
    if (!server->listen(socket_path))
        return Error::from_string_literal("Failed to listen on /tmp/sentinel.sock");

    auto sentinel_server = adopt_own(*new SentinelServer(move(server)));

    // Initialize ML-based malware detection (Milestone 0.4)
    // For now, use a dummy model path - will be replaced with actual model later
    auto ml_detector_result = MalwareMLDetector::create("/tmp/dummy_model.tflite");
    if (ml_detector_result.is_error()) {
        dbgln("Sentinel: Warning: Failed to initialize ML detector: {}", ml_detector_result.error());
        dbgln("Sentinel: Continuing with YARA-only scanning");
        // Continue without ML detection - graceful degradation
    } else {
        sentinel_server->m_ml_detector = ml_detector_result.release_value();
        dbgln("Sentinel: ML-based malware detection initialized (version {})",
            sentinel_server->m_ml_detector->model_version());
    }

    // Initialize federated threat intelligence (Milestone 0.4 Phase 3)
    auto threat_feed_result = ThreatFeed::create();
    if (threat_feed_result.is_error()) {
        dbgln("Sentinel: Warning: Failed to initialize threat feed: {}", threat_feed_result.error());
        dbgln("Sentinel: Continuing without federated threat intelligence");
        // Continue without threat feed - graceful degradation
    } else {
        sentinel_server->m_threat_feed = threat_feed_result.release_value();

        // Start auto-sync if enabled
        if (sentinel_server->m_threat_feed->is_auto_sync_enabled()) {
            auto sync_result = sentinel_server->m_threat_feed->sync_from_peers();
            if (sync_result.is_error()) {
                dbgln("Sentinel: Initial threat sync failed: {}", sync_result.error());
            }
        }

        auto stats = sentinel_server->m_threat_feed->get_statistics();
        dbgln("Sentinel: Threat feed initialized with {} threats, FPR={}%",
            stats.total_threats, stats.false_positive_rate * 100);
    }

    return sentinel_server;
}

SentinelServer::SentinelServer(NonnullRefPtr<Core::LocalServer> server)
    : m_server(move(server))
{
    m_server->on_accept = [this](NonnullOwnPtr<Core::Socket> client_socket) {
        handle_client(move(client_socket));
    };
}

int SentinelServer::get_client_id(Core::Socket const* socket)
{
    auto it = m_socket_to_client_id.find(socket);
    if (it != m_socket_to_client_id.end())
        return it->value;

    // Assign new client ID
    int client_id = m_next_client_id++;
    m_socket_to_client_id.set(socket, client_id);
    dbgln("Sentinel: Assigned client ID {} to socket", client_id);
    return client_id;
}

void SentinelServer::handle_client(NonnullOwnPtr<Core::Socket> socket)
{
    dbgln("Sentinel: Client connected");

    // Create a buffered reader for this client
    auto* socket_ptr = socket.ptr();
    m_client_readers.set(socket_ptr, IPC::BufferedIPCReader {});

    socket->on_ready_to_read = [this, sock = socket_ptr]() {
        // Get the buffered reader for this client
        auto reader_it = m_client_readers.find(sock);
        if (reader_it == m_client_readers.end()) {
            dbgln("Sentinel: ERROR: No reader found for client socket");
            return;
        }

        auto& reader = reader_it->value;

        // Try to read a complete message (handles partial reads internally)
        auto message_result = reader.read_complete_message(*sock);

        if (message_result.is_error()) {
            dbgln("Sentinel: Read error: {}", message_result.error());
            // Clean up reader on error
            m_client_readers.remove(sock);
            return;
        }

        auto message_buffer = message_result.release_value();

        // Convert message to String with UTF-8 validation
        auto message_string_result = String::from_utf8(StringView(
            reinterpret_cast<char const*>(message_buffer.data()),
            message_buffer.size()));

        if (message_string_result.is_error()) {
            dbgln("Sentinel: Invalid UTF-8 in message, sending error response");

            // Send error response using BufferedIPCWriter
            IPC::BufferedIPCWriter writer;
            JsonObject error_response;
            error_response.set("status"sv, "error"sv);
            error_response.set("error"sv, "Invalid UTF-8 encoding in message"sv);
            auto error_json = error_response.serialized();

            auto write_result = writer.write_message(*sock, error_json.bytes_as_string_view());
            if (write_result.is_error()) {
                dbgln("Sentinel: Failed to send error response: {}", write_result.error());
            }
            return;
        }

        auto message = message_string_result.release_value();
        auto process_result = process_message(*sock, message);

        if (process_result.is_error()) {
            dbgln("Sentinel: Failed to process message: {}", process_result.error());
        }
    };

    m_clients.append(move(socket));
}

ErrorOr<void> SentinelServer::process_message(Core::Socket& socket, String const& message)
{
    // Get client ID for rate limiting
    int client_id = get_client_id(&socket);

    // Parse JSON message
    auto json_result = JsonValue::from_string(message);
    if (json_result.is_error())
        return Error::from_string_literal("Invalid JSON");

    auto json = json_result.value();
    if (!json.is_object())
        return Error::from_string_literal("Expected JSON object");

    auto obj = json.as_object();
    auto action = obj.get_string("action"sv);
    if (!action.has_value())
        return Error::from_string_literal("Missing 'action' field");

    JsonObject response;
    auto request_id = obj.get_string("request_id"sv);
    response.set("request_id"sv, request_id.has_value() ? JsonValue(request_id.value()) : JsonValue("unknown"sv));

    // Health check endpoints
    if (action.value() == "health"sv) {
        auto report_result = m_health_check.check_all_components();
        if (report_result.is_error()) {
            response.set("status"sv, "error"sv);
            response.set("error"sv, MUST(String::formatted("Health check failed: {}", report_result.error())));
        } else {
            response.set("status"sv, "success"sv);
            response.set("health"sv, report_result.value().to_json());
        }

        auto response_str = response.serialized();
        IPC::BufferedIPCWriter writer;
        TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
        return {};
    }

    if (action.value() == "health_live"sv) {
        auto liveness = m_health_check.check_liveness();
        response.set("status"sv, "success"sv);
        response.set("liveness"sv, liveness.to_json());

        auto response_str = response.serialized();
        IPC::BufferedIPCWriter writer;
        TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
        return {};
    }

    if (action.value() == "health_ready"sv) {
        auto readiness = m_health_check.check_readiness();
        response.set("status"sv, "success"sv);
        response.set("readiness"sv, readiness.to_json());

        auto response_str = response.serialized();
        IPC::BufferedIPCWriter writer;
        TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
        return {};
    }

    if (action.value() == "metrics"sv) {
        auto metrics_str = m_health_check.get_metrics_prometheus_format();
        response.set("status"sv, "success"sv);
        response.set("metrics"sv, metrics_str);

        auto response_str = response.serialized();
        IPC::BufferedIPCWriter writer;
        TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
        return {};
    }

    if (action.value() == "scan_file"sv) {
        // SECURITY: Rate limiting check for scan requests
        auto rate_check = m_rate_limiter.check_scan_request(client_id);
        if (rate_check.is_error()) {
            dbgln("Sentinel: Rate limit exceeded for client {} (scan_file)", client_id);
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Rate limit exceeded. Too many scan requests. Please try again later."sv);
            auto response_str = response.serialized();
            IPC::BufferedIPCWriter writer;
            TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
            return {};
        }

        // SECURITY: Check concurrent scan limit
        auto concurrent_check = m_rate_limiter.check_concurrent_scans(client_id);
        if (concurrent_check.is_error()) {
            dbgln("Sentinel: Concurrent scan limit exceeded for client {}", client_id);
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Concurrent scan limit exceeded. Please wait for ongoing scans to complete."sv);
            auto response_str = response.serialized();
            IPC::BufferedIPCWriter writer;
            TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
            return {};
        }

        auto file_path = obj.get_string("file_path"sv);
        if (!file_path.has_value()) {
            m_rate_limiter.release_scan_slot(client_id); // Release slot on early exit
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Missing 'file_path' field"sv);
        } else {
            // Validate file_path using InputValidator
            auto validation_result = InputValidator::validate_file_path(file_path.value());
            if (!validation_result.is_valid) {
                m_rate_limiter.release_scan_slot(client_id); // Release slot on early exit
                response.set("status"sv, "error"sv);
                response.set("error"sv, validation_result.error_message);
            } else {
                // Convert String to ByteString for scan_file
                auto result = scan_file(ByteString(file_path.value().bytes_as_string_view()));
                m_rate_limiter.release_scan_slot(client_id); // Release slot after scan completes

                if (result.is_error()) {
                    response.set("status"sv, "error"sv);
                    response.set("error"sv, result.error().string_literal());
                } else {
                    // Convert scan result to UTF-8 string - handle errors gracefully
                    auto result_string = String::from_utf8(StringView(result.value()));
                    if (result_string.is_error()) {
                        response.set("status"sv, "error"sv);
                        response.set("error"sv, "Scan result contains invalid UTF-8"sv);
                    } else {
                        response.set("status"sv, "success"sv);
                        response.set("result"sv, result_string.release_value());
                    }
                }
            }
        }
    } else if (action.value() == "scan_content"sv) {
        // SECURITY: Rate limiting check for scan requests
        auto rate_check = m_rate_limiter.check_scan_request(client_id);
        if (rate_check.is_error()) {
            dbgln("Sentinel: Rate limit exceeded for client {} (scan_content)", client_id);
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Rate limit exceeded. Too many scan requests. Please try again later."sv);
            auto response_str = response.serialized();
            IPC::BufferedIPCWriter writer;
            TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
            return {};
        }

        // SECURITY: Check concurrent scan limit
        auto concurrent_check = m_rate_limiter.check_concurrent_scans(client_id);
        if (concurrent_check.is_error()) {
            dbgln("Sentinel: Concurrent scan limit exceeded for client {}", client_id);
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Concurrent scan limit exceeded. Please wait for ongoing scans to complete."sv);
            auto response_str = response.serialized();
            IPC::BufferedIPCWriter writer;
            TRY(writer.write_message(socket, response_str.bytes_as_string_view()));
            return {};
        }
        auto content = obj.get_string("content"sv);
        if (!content.has_value()) {
            m_rate_limiter.release_scan_slot(client_id); // Release slot on early exit
            response.set("status"sv, "error"sv);
            response.set("error"sv, "Missing 'content' field"sv);
        } else {
            // Validate content length before decoding (prevent DoS via huge base64)
            constexpr size_t MAX_BASE64_SIZE = 300 * 1024 * 1024; // 300MB base64 (~200MB decoded)
            if (content.value().bytes().size() > MAX_BASE64_SIZE) {
                m_rate_limiter.release_scan_slot(client_id); // Release slot on early exit
                response.set("status"sv, "error"sv);
                response.set("error"sv, "Content too large for scanning (max 200MB after decode)"sv);
            } else {
                // Decode base64 content before scanning
                auto decoded_result = decode_base64(content.value().bytes_as_string_view());
                if (decoded_result.is_error()) {
                    m_rate_limiter.release_scan_slot(client_id); // Release slot on early exit
                    response.set("status"sv, "error"sv);
                    response.set("error"sv, "Failed to decode base64 content"sv);
                } else {
                    auto result = scan_content(decoded_result.value().bytes());
                    m_rate_limiter.release_scan_slot(client_id); // Release slot after scan completes

                    if (result.is_error()) {
                        response.set("status"sv, "error"sv);
                        response.set("error"sv, result.error().string_literal());
                    } else {
                        // Convert scan result to UTF-8 string - handle errors gracefully
                        auto result_string = String::from_utf8(StringView(result.value()));
                        if (result_string.is_error()) {
                            response.set("status"sv, "error"sv);
                            response.set("error"sv, "Scan result contains invalid UTF-8"sv);
                        } else {
                            response.set("status"sv, "success"sv);
                            response.set("result"sv, result_string.release_value());
                        }
                    }
                }
            }
        }
    } else {
        response.set("status"sv, "error"sv);
        response.set("error"sv, "Unknown action"sv);
    }

    auto response_str = response.serialized();

    // Use BufferedIPCWriter to send response with proper framing
    IPC::BufferedIPCWriter writer;
    TRY(writer.write_message(socket, response_str.bytes_as_string_view()));

    return {};
}

static ErrorOr<ByteString> validate_scan_path(StringView file_path)
{
    // Resolve canonical path to prevent directory traversal
    auto canonical = TRY(FileSystem::real_path(file_path));

    // Check for directory traversal attempts in canonical path
    if (canonical.contains(".."sv))
        return Error::from_string_literal("Path traversal detected");

    // Whitelist allowed directories - only scan files in user-accessible locations
    Vector<StringView> allowed_prefixes = {
        "/home"sv,      // User home directories
        "/tmp"sv,       // Temporary downloads
        "/var/tmp"sv,   // Alternative temp directory
    };

    bool allowed = false;
    for (auto& prefix : allowed_prefixes) {
        if (canonical.view().starts_with(prefix)) {
            allowed = true;
            break;
        }
    }

    if (!allowed)
        return Error::from_string_literal("File path not in allowed directory");

    // Check file is not a symlink (prevent symlink attacks)
    auto stat_result = TRY(Core::System::lstat(canonical));
    if (S_ISLNK(stat_result.st_mode))
        return Error::from_string_literal("Cannot scan symlinks");

    // Check file is a regular file (prevent scanning device files, pipes, etc.)
    if (!S_ISREG(stat_result.st_mode))
        return Error::from_string_literal("Can only scan regular files");

    return canonical;
}

ErrorOr<ByteString> SentinelServer::scan_file(ByteString const& file_path)
{
    // Validate and canonicalize the file path for security
    auto validated_path = TRY(validate_scan_path(file_path));

    // Check file size before reading to prevent memory exhaustion
    auto stat_result = TRY(Core::System::stat(validated_path));
    constexpr size_t MAX_FILE_SIZE = 200 * 1024 * 1024; // 200MB
    if (static_cast<size_t>(stat_result.st_size) > MAX_FILE_SIZE)
        return Error::from_string_literal("File too large to scan");

    // Read file content using validated path
    auto file = TRY(Core::File::open(validated_path, Core::File::OpenMode::Read));
    auto content = TRY(file->read_until_eof());

    return scan_content(content.bytes());
}

struct YaraMatchData {
    Vector<ByteString> rule_names;
    Vector<JsonObject> rule_details;
};

static int yara_callback([[maybe_unused]] YR_SCAN_CONTEXT* context, int message, void* message_data, void* user_data)
{
    if (message == CALLBACK_MSG_RULE_MATCHING) {
        auto* rule = static_cast<YR_RULE*>(message_data);
        auto* match_data = static_cast<YaraMatchData*>(user_data);

        match_data->rule_names.append(ByteString(rule->identifier));

        // Extract rule metadata
        JsonObject rule_obj;
        rule_obj.set("rule_name"sv, JsonValue(ByteString(rule->identifier)));

        // Get metadata with safety limit to prevent infinite loops
        YR_META* meta = rule->metas;
        constexpr int MAX_METADATA_ENTRIES = 100; // Safety limit
        int meta_count = 0;

        while (!META_IS_LAST_IN_RULE(meta) && meta_count < MAX_METADATA_ENTRIES) {
            if (meta->type == META_TYPE_STRING) {
                if (strcmp(meta->identifier, "description") == 0)
                    rule_obj.set("description"sv, JsonValue(ByteString(meta->string)));
                else if (strcmp(meta->identifier, "severity") == 0)
                    rule_obj.set("severity"sv, JsonValue(ByteString(meta->string)));
                else if (strcmp(meta->identifier, "author") == 0)
                    rule_obj.set("author"sv, JsonValue(ByteString(meta->string)));
            }
            meta++;
            meta_count++;
        }

        // Handle the last metadata entry (only if we didn't hit the safety limit)
        if (meta_count < MAX_METADATA_ENTRIES && meta->type == META_TYPE_STRING) {
            if (strcmp(meta->identifier, "description") == 0)
                rule_obj.set("description"sv, JsonValue(ByteString(meta->string)));
            else if (strcmp(meta->identifier, "severity") == 0)
                rule_obj.set("severity"sv, JsonValue(ByteString(meta->string)));
            else if (strcmp(meta->identifier, "author") == 0)
                rule_obj.set("author"sv, JsonValue(ByteString(meta->string)));
        }

        match_data->rule_details.append(move(rule_obj));
    }
    return CALLBACK_CONTINUE;
}

ErrorOr<ByteString> SentinelServer::scan_content(ReadonlyBytes content)
{
    if (!s_yara_rules)
        return Error::from_string_literal("YARA rules not initialized");

    // Check bloom filter first for quick threat detection (Milestone 0.4 Phase 3)
    bool bloom_filter_hit = false;
    if (m_threat_feed) {
        bloom_filter_hit = m_threat_feed->probably_malicious(content);
        if (bloom_filter_hit) {
            dbgln("Sentinel: Bloom filter hit - file hash matches known threat");
        }
    }

    // For large files (> 10MB), use streaming scan to reduce memory pressure
    constexpr size_t STREAMING_THRESHOLD = 10 * 1024 * 1024; // 10MB
    constexpr size_t CHUNK_SIZE = 1 * 1024 * 1024; // 1MB chunks

    if (content.size() > STREAMING_THRESHOLD) {
        dbgln("SentinelServer: Using streaming scan for large file ({}MB)", content.size() / (1024 * 1024));

        // Scan in chunks with overlap to catch patterns spanning chunk boundaries
        constexpr size_t OVERLAP_SIZE = 4096; // 4KB overlap
        YaraMatchData match_data;

        size_t offset = 0;
        while (offset < content.size()) {
            size_t chunk_size = min(CHUNK_SIZE, content.size() - offset);
            auto chunk = content.slice(offset, chunk_size);

            int result = yr_rules_scan_mem(
                s_yara_rules,
                reinterpret_cast<uint8_t const*>(chunk.data()),
                chunk.size(),
                0,
                yara_callback,
                &match_data,
                0);

            if (result != ERROR_SUCCESS)
                return Error::from_string_literal("YARA scan failed");

            // If we found matches, we can stop early
            if (!match_data.rule_names.is_empty())
                break;

            // Move to next chunk with overlap
            if (offset + chunk_size >= content.size())
                break;

            offset += chunk_size - OVERLAP_SIZE;
        }

        bool yara_threat = !match_data.rule_names.is_empty();

        // Run ML-based malware detection on full content (Milestone 0.4)
        // For large files, we analyze the full content for ML to avoid false negatives
        bool ml_threat = false;
        Optional<MalwareMLDetector::Prediction> ml_prediction;

        if (m_ml_detector) {
            ByteBuffer content_buffer;
            auto buffer_result = content_buffer.try_append(content);
            if (!buffer_result.is_error()) {
                auto prediction_result = m_ml_detector->analyze_file(content_buffer);
                if (!prediction_result.is_error()) {
                    ml_prediction = prediction_result.release_value();
                    ml_threat = ml_prediction->malware_probability > 0.5f;
                } else {
                    dbgln("Sentinel: ML analysis failed (streaming): {}", prediction_result.error());
                }
            }
        }

        bool threat_detected = yara_threat || ml_threat || bloom_filter_hit;

        if (!threat_detected)
            return ByteString("clean"sv);

        // Format matches as detailed JSON response
        JsonObject result_obj;
        result_obj.set("threat_detected"sv, true);

        // YARA results
        if (yara_threat) {
            JsonArray matched_rules_array;
            for (auto const& rule_detail : match_data.rule_details) {
                matched_rules_array.must_append(rule_detail);
            }
            result_obj.set("matched_rules"sv, move(matched_rules_array));
            result_obj.set("match_count"sv, static_cast<i64>(match_data.rule_names.size()));
        }

        // ML results
        if (ml_prediction.has_value()) {
            JsonObject ml_obj;
            ml_obj.set("malware_probability"sv, static_cast<double>(ml_prediction->malware_probability));
            ml_obj.set("confidence"sv, static_cast<double>(ml_prediction->confidence));
            ml_obj.set("explanation"sv, ml_prediction->explanation.bytes_as_string_view());
            result_obj.set("ml_prediction"sv, move(ml_obj));
        }

        // Bloom filter results
        if (bloom_filter_hit) {
            result_obj.set("bloom_filter_hit"sv, true);
            result_obj.set("known_threat"sv, "File hash matches federated threat database"sv);
        }

        auto json_string = result_obj.serialized();
        return ByteString(json_string.bytes_as_string_view());
    }

    // For smaller files, scan entire content at once (original behavior)
    YaraMatchData match_data;
    int result = yr_rules_scan_mem(
        s_yara_rules,
        reinterpret_cast<uint8_t const*>(content.data()),
        content.size(),
        0,
        yara_callback,
        &match_data,
        0);

    if (result != ERROR_SUCCESS)
        return Error::from_string_literal("YARA scan failed");

    bool yara_threat = !match_data.rule_names.is_empty();

    // Run ML-based malware detection (Milestone 0.4)
    bool ml_threat = false;
    Optional<MalwareMLDetector::Prediction> ml_prediction;

    if (m_ml_detector) {
        ByteBuffer content_buffer;
        // Try to create ByteBuffer from content - if it fails, skip ML analysis
        auto buffer_result = content_buffer.try_append(content);
        if (!buffer_result.is_error()) {
            auto prediction_result = m_ml_detector->analyze_file(content_buffer);
            if (!prediction_result.is_error()) {
                ml_prediction = prediction_result.release_value();
                ml_threat = ml_prediction->malware_probability > 0.5f;
            } else {
                dbgln("Sentinel: ML analysis failed: {}", prediction_result.error());
            }
        }
    }

    // Combine YARA, ML, and bloom filter results
    bool threat_detected = yara_threat || ml_threat || bloom_filter_hit;

    if (!threat_detected)
        return ByteString("clean"sv);

    // Format matches as detailed JSON response
    JsonObject result_obj;
    result_obj.set("threat_detected"sv, true);

    // YARA results
    if (yara_threat) {
        JsonArray matched_rules_array;
        for (auto const& rule_detail : match_data.rule_details) {
            matched_rules_array.must_append(rule_detail);
        }
        result_obj.set("matched_rules"sv, move(matched_rules_array));
        result_obj.set("match_count"sv, static_cast<i64>(match_data.rule_names.size()));
    }

    // ML results
    if (ml_prediction.has_value()) {
        JsonObject ml_obj;
        ml_obj.set("malware_probability"sv, static_cast<double>(ml_prediction->malware_probability));
        ml_obj.set("confidence"sv, static_cast<double>(ml_prediction->confidence));
        ml_obj.set("explanation"sv, ml_prediction->explanation.bytes_as_string_view());
        result_obj.set("ml_prediction"sv, move(ml_obj));
    }

    // Bloom filter results
    if (bloom_filter_hit) {
        result_obj.set("bloom_filter_hit"sv, true);
        result_obj.set("known_threat"sv, "File hash matches federated threat database"sv);
    }

    auto json_string = result_obj.serialized();
    return ByteString(json_string.bytes_as_string_view());
}

void SentinelServer::initialize_health_checks(PolicyGraph* policy_graph)
{
    dbgln("Sentinel: Initializing health check system");

    // Register database health check
    if (policy_graph) {
        m_health_check.register_check("database"_string, [policy_graph]() -> ErrorOr<HealthCheck::ComponentHealth> {
            return HealthCheck::check_database_health(policy_graph);
        });
    }

    // Register YARA health check
    m_health_check.register_check("yara"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        return HealthCheck::check_yara_health();
    });

    // Register quarantine health check
    m_health_check.register_check("quarantine"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        return HealthCheck::check_quarantine_health();
    });

    // Register disk space health check
    m_health_check.register_check("disk"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        return HealthCheck::check_disk_space();
    });

    // Register memory health check
    m_health_check.register_check("memory"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        return HealthCheck::check_memory_usage();
    });

    // Register IPC health check
    m_health_check.register_check("ipc"_string, [this]() -> ErrorOr<HealthCheck::ComponentHealth> {
        return HealthCheck::check_ipc_health(active_connection_count());
    });

    // Start periodic health checks every 30 seconds
    m_health_check.start_periodic_checks(Duration::from_seconds(30));

    dbgln("Sentinel: Health check system initialized with {} components", m_health_check.get_metrics().get("sentinel_registered_components"_string).value_or(0));
}

}
