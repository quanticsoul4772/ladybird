/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>

namespace WebView {

struct RateLimitConfig {
    // UI-level policy rate limiting (existing)
    size_t policies_per_minute { 5 };
    size_t rate_window_seconds { 60 };

    // IPC endpoint rate limiting (new for ISSUE-017 - DoS protection)
    bool enabled { true };
    size_t scan_requests_per_second { 10 };
    size_t scan_burst_capacity { 20 };
    size_t policy_queries_per_second { 100 };
    size_t policy_burst_capacity { 200 };
    size_t max_concurrent_scans { 5 };
    bool fail_closed { false }; // false = fail-open (allow), true = fail-closed (block)
};

struct NotificationConfig {
    bool enabled { true };
    size_t auto_dismiss_seconds { 5 };
    size_t max_queue_size { 10 };
};

struct ScanSizeConfig {
    size_t small_file_threshold { 10 * 1024 * 1024 };
    size_t medium_file_threshold { 100 * 1024 * 1024 };
    size_t max_scan_size { 200 * 1024 * 1024 };
    size_t chunk_size { 1 * 1024 * 1024 };
    bool scan_large_files_partially { true };
    size_t large_file_scan_bytes { 10 * 1024 * 1024 };
};

struct AuditLogConfig {
    bool enabled { true };
    String log_path;  // Defaults to ~/.local/share/Ladybird/audit.log
    size_t max_file_size { 100 * 1024 * 1024 };  // 100MB
    size_t max_files { 10 };                      // Keep 10 rotated files
    bool log_clean_scans { false };               // Log only threats by default
    size_t buffer_size { 100 };                   // Flush after 100 events
};

struct SentinelConfig {
    bool enabled { true };
    String yara_rules_path;
    String quarantine_path;
    String database_path;
    size_t policy_cache_size { 1000 };
    size_t threat_retention_days { 30 };
    RateLimitConfig rate_limit;
    NotificationConfig notifications;
    ScanSizeConfig scan_size;
    AuditLogConfig audit_log;

    // Default paths
    static ErrorOr<String> default_yara_rules_path();
    static ErrorOr<String> default_quarantine_path();
    static ErrorOr<String> default_database_path();
    static ErrorOr<String> default_audit_log_path();
    static ErrorOr<String> default_config_path();

    // Config file operations
    static ErrorOr<SentinelConfig> load_from_file(String const& path);
    static ErrorOr<SentinelConfig> load_default();
    ErrorOr<void> save_to_file(String const& path) const;

    // Create default config
    static SentinelConfig create_default();

    // Serialization
    [[nodiscard]] String to_json() const;
    static ErrorOr<SentinelConfig> from_json(String const& json_string);
};

}
