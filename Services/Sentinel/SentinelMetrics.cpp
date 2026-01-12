/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/NumberFormat.h>
#include <AK/StringBuilder.h>
#include <LibCore/File.h>
#include <Services/Sentinel/SentinelMetrics.h>

namespace Sentinel {

String SentinelMetrics::to_json() const
{
    JsonObject json;

    // Scan statistics
    json.set("total_downloads_scanned"sv, total_downloads_scanned);
    json.set("threats_detected"sv, threats_detected);
    json.set("threats_blocked"sv, threats_blocked);
    json.set("threats_quarantined"sv, threats_quarantined);
    json.set("threats_allowed"sv, threats_allowed);

    // Policy statistics
    json.set("policies_enforced"sv, policies_enforced);
    json.set("policies_created"sv, policies_created);
    json.set("total_policies"sv, total_policies);

    // Cache statistics
    json.set("cache_hits"sv, cache_hits);
    json.set("cache_misses"sv, cache_misses);
    json.set("cache_hit_rate"sv, cache_hit_rate());

    // Performance metrics
    json.set("average_scan_time_ms"sv, average_scan_time().to_milliseconds());
    json.set("average_query_time_ms"sv, average_query_time().to_milliseconds());
    json.set("scan_count"sv, scan_count);
    json.set("query_count"sv, query_count);

    // Storage statistics
    json.set("database_size_bytes"sv, database_size_bytes);
    json.set("quarantine_size_bytes"sv, quarantine_size_bytes);
    json.set("quarantine_file_count"sv, quarantine_file_count);

    // System health
    json.set("started_at"sv, started_at.seconds_since_epoch());
    json.set("uptime_seconds"sv, uptime().to_seconds());
    json.set("last_scan"sv, last_scan.seconds_since_epoch());
    json.set("last_threat"sv, last_threat.seconds_since_epoch());

    return json.serialized();
}

ErrorOr<String> SentinelMetrics::to_human_readable() const
{
    StringBuilder builder;

    builder.append("=== Sentinel Metrics ===\n\n"sv);

    // Scan statistics
    builder.append("Download Scans:\n"sv);
    builder.appendff("  Total scanned:       {}\n", total_downloads_scanned);
    builder.appendff("  Threats detected:    {}\n", threats_detected);
    builder.appendff("  - Blocked:           {}\n", threats_blocked);
    builder.appendff("  - Quarantined:       {}\n", threats_quarantined);
    builder.appendff("  - Allowed:           {}\n", threats_allowed);
    builder.append("\n"sv);

    // Policy statistics
    builder.append("Policies:\n"sv);
    builder.appendff("  Total policies:      {}\n", total_policies);
    builder.appendff("  Policies created:    {}\n", policies_created);
    builder.appendff("  Policies enforced:   {}\n", policies_enforced);
    builder.append("\n"sv);

    // Cache statistics
    builder.append("Cache Performance:\n"sv);
    builder.appendff("  Cache hits:          {}\n", cache_hits);
    builder.appendff("  Cache misses:        {}\n", cache_misses);
    builder.appendff("  Hit rate:            {:.1f}%\n", cache_hit_rate() * 100);
    builder.append("\n"sv);

    // Performance metrics
    builder.append("Performance:\n"sv);
    builder.appendff("  Avg scan time:       {} ms\n", average_scan_time().to_milliseconds());
    builder.appendff("  Avg query time:      {} ms\n", average_query_time().to_milliseconds());
    builder.append("\n"sv);

    // Storage statistics
    builder.append("Storage:\n"sv);
    builder.appendff("  Database size:       {}\n", AK::human_readable_size(database_size_bytes));
    builder.appendff("  Quarantine size:     {}\n", AK::human_readable_size(quarantine_size_bytes));
    builder.appendff("  Quarantined files:   {}\n", quarantine_file_count);
    builder.append("\n"sv);

    // System health
    builder.append("System Health:\n"sv);
    builder.appendff("  Uptime:              {} seconds\n", uptime().to_seconds());
    builder.appendff("  Last scan:           {} seconds ago\n",
        Duration::from_seconds(UnixDateTime::now().seconds_since_epoch() - last_scan.seconds_since_epoch()).to_seconds());
    if (last_threat.seconds_since_epoch() > 0) {
        builder.appendff("  Last threat:         {} seconds ago\n",
            Duration::from_seconds(UnixDateTime::now().seconds_since_epoch() - last_threat.seconds_since_epoch()).to_seconds());
    } else {
        builder.append("  Last threat:         Never\n"sv);
    }

    // Convert to String with error propagation
    return TRY(builder.to_string());
}

MetricsCollector& MetricsCollector::the()
{
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::record_download_scan(Duration scan_time)
{
    m_metrics.total_downloads_scanned++;
    m_metrics.total_scan_time = m_metrics.total_scan_time + scan_time;
    m_metrics.scan_count++;
    m_metrics.last_scan = UnixDateTime::now();
}

void MetricsCollector::record_threat_detected(String const& action)
{
    m_metrics.threats_detected++;
    m_metrics.last_threat = UnixDateTime::now();

    if (action == "block"sv) {
        m_metrics.threats_blocked++;
    } else if (action == "quarantine"sv) {
        m_metrics.threats_quarantined++;
    } else if (action == "allow"sv) {
        m_metrics.threats_allowed++;
    }
}

void MetricsCollector::record_policy_query(Duration query_time, bool cache_hit)
{
    m_metrics.total_query_time = m_metrics.total_query_time + query_time;
    m_metrics.query_count++;

    if (cache_hit) {
        m_metrics.cache_hits++;
    } else {
        m_metrics.cache_misses++;
    }
}

void MetricsCollector::record_policy_created()
{
    m_metrics.policies_created++;
}

void MetricsCollector::record_policy_enforced()
{
    m_metrics.policies_enforced++;
}

void MetricsCollector::update_database_size(size_t bytes)
{
    m_metrics.database_size_bytes = bytes;
}

void MetricsCollector::update_quarantine_stats(size_t bytes, size_t file_count)
{
    m_metrics.quarantine_size_bytes = bytes;
    m_metrics.quarantine_file_count = file_count;
}

void MetricsCollector::update_total_policies(size_t count)
{
    m_metrics.total_policies = count;
}

SentinelMetrics MetricsCollector::snapshot() const
{
    return m_metrics;
}

ErrorOr<void> MetricsCollector::save_to_file(String const& path)
{
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Write));
    auto json = m_metrics.to_json();
    TRY(file->write_until_depleted(json.bytes()));
    return {};
}

ErrorOr<void> MetricsCollector::load_from_file(String const& path)
{
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());
    auto json = TRY(JsonValue::from_string(contents));

    if (!json.is_object())
        return Error::from_string_literal("Invalid metrics file format");

    auto const& obj = json.as_object();

    // Load scan statistics
    m_metrics.total_downloads_scanned = obj.get_u64("total_downloads_scanned"sv).value_or(0);
    m_metrics.threats_detected = obj.get_u64("threats_detected"sv).value_or(0);
    m_metrics.threats_blocked = obj.get_u64("threats_blocked"sv).value_or(0);
    m_metrics.threats_quarantined = obj.get_u64("threats_quarantined"sv).value_or(0);
    m_metrics.threats_allowed = obj.get_u64("threats_allowed"sv).value_or(0);

    // Load policy statistics
    m_metrics.policies_enforced = obj.get_u64("policies_enforced"sv).value_or(0);
    m_metrics.policies_created = obj.get_u64("policies_created"sv).value_or(0);
    m_metrics.total_policies = obj.get_u64("total_policies"sv).value_or(0);

    // Load cache statistics
    m_metrics.cache_hits = obj.get_u64("cache_hits"sv).value_or(0);
    m_metrics.cache_misses = obj.get_u64("cache_misses"sv).value_or(0);

    // Load performance metrics
    m_metrics.scan_count = obj.get_u64("scan_count"sv).value_or(0);
    m_metrics.query_count = obj.get_u64("query_count"sv).value_or(0);

    // Load storage statistics
    m_metrics.database_size_bytes = obj.get_u64("database_size_bytes"sv).value_or(0);
    m_metrics.quarantine_size_bytes = obj.get_u64("quarantine_size_bytes"sv).value_or(0);
    m_metrics.quarantine_file_count = obj.get_u64("quarantine_file_count"sv).value_or(0);

    return {};
}

void MetricsCollector::reset_session_metrics()
{
    m_metrics.total_scan_time = Duration::zero();
    m_metrics.total_query_time = Duration::zero();
    m_metrics.scan_count = 0;
    m_metrics.query_count = 0;
    m_metrics.started_at = UnixDateTime::now();
}

void MetricsCollector::reset_all_metrics()
{
    m_metrics = SentinelMetrics {};
}

}
