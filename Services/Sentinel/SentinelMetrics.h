/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <LibCore/File.h>

using AK::Duration;

namespace Sentinel {

struct SentinelMetrics {
    // Scan statistics
    size_t total_downloads_scanned { 0 };
    size_t threats_detected { 0 };
    size_t threats_blocked { 0 };
    size_t threats_quarantined { 0 };
    size_t threats_allowed { 0 };

    // Policy statistics
    size_t policies_enforced { 0 };
    size_t policies_created { 0 };
    size_t total_policies { 0 };

    // Cache statistics
    size_t cache_hits { 0 };
    size_t cache_misses { 0 };

    // Performance metrics
    Duration total_scan_time { Duration::zero() };
    Duration total_query_time { Duration::zero() };
    size_t scan_count { 0 };
    size_t query_count { 0 };

    // Storage statistics
    size_t database_size_bytes { 0 };
    size_t quarantine_size_bytes { 0 };
    size_t quarantine_file_count { 0 };

    // System health
    UnixDateTime started_at { UnixDateTime::now() };
    UnixDateTime last_scan { UnixDateTime::epoch() };
    UnixDateTime last_threat { UnixDateTime::epoch() };

    // Computed metrics
    [[nodiscard]] Duration average_scan_time() const
    {
        return scan_count > 0 ? Duration::from_nanoseconds(total_scan_time.to_nanoseconds() / scan_count) : Duration::zero();
    }

    [[nodiscard]] Duration average_query_time() const
    {
        return query_count > 0 ? Duration::from_nanoseconds(total_query_time.to_nanoseconds() / query_count) : Duration::zero();
    }

    [[nodiscard]] double cache_hit_rate() const
    {
        auto total = cache_hits + cache_misses;
        return total > 0 ? static_cast<double>(cache_hits) / total : 0.0;
    }

    [[nodiscard]] Duration uptime() const
    {
        return Duration::from_seconds(UnixDateTime::now().seconds_since_epoch() - started_at.seconds_since_epoch());
    }

    // Serialization
    [[nodiscard]] String to_json() const;
    [[nodiscard]] ErrorOr<String> to_human_readable() const;
};

class MetricsCollector {
public:
    static MetricsCollector& the();

    // Event recording
    void record_download_scan(Duration scan_time);
    void record_threat_detected(String const& action);
    void record_policy_query(Duration query_time, bool cache_hit);
    void record_policy_created();
    void record_policy_enforced();

    // Storage updates
    void update_database_size(size_t bytes);
    void update_quarantine_stats(size_t bytes, size_t file_count);
    void update_total_policies(size_t count);

    // Retrieval
    [[nodiscard]] SentinelMetrics snapshot() const;

    // Persistence
    ErrorOr<void> save_to_file(String const& path);
    ErrorOr<void> load_from_file(String const& path);

    // Reset
    void reset_session_metrics();
    void reset_all_metrics();

private:
    MetricsCollector() = default;

    mutable SentinelMetrics m_metrics;
};

}
