/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "HealthCheck.h"
#include "PolicyGraph.h"
#include <AK/JsonArray.h>
#include <AK/StringBuilder.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <sys/statvfs.h>
#include <yara.h>

// Undefine YARA macros that conflict with AK classes
#ifdef get_string
#    undef get_string
#endif
#ifdef set
#    undef set
#endif

namespace Sentinel {

// ComponentHealth JSON serialization
JsonObject HealthCheck::ComponentHealth::to_json() const
{
    JsonObject obj;
    obj.set("component"sv, component_name);
    obj.set("status"sv, health_status_to_string(status));

    if (message.has_value())
        obj.set("message"sv, message.value());

    obj.set("last_check"sv, static_cast<i64>(last_check.seconds_since_epoch()));
    obj.set("response_time_ms"sv, static_cast<i64>(response_time.to_milliseconds()));

    if (details.has_value())
        obj.set("details"sv, details.value());

    return obj;
}

// LivenessProbe JSON serialization
JsonObject HealthCheck::LivenessProbe::to_json() const
{
    JsonObject obj;
    obj.set("alive"sv, alive);

    if (reason.has_value())
        obj.set("reason"sv, reason.value());

    return obj;
}

// ReadinessProbe JSON serialization
JsonObject HealthCheck::ReadinessProbe::to_json() const
{
    JsonObject obj;
    obj.set("ready"sv, ready);

    if (reason.has_value())
        obj.set("reason"sv, reason.value());

    if (!blocking_components.is_empty()) {
        JsonArray components_array;
        for (auto const& component : blocking_components)
            components_array.must_append(component);
        obj.set("blocking_components"sv, move(components_array));
    }

    return obj;
}

// HealthReport JSON serialization
JsonObject HealthCheck::HealthReport::to_json() const
{
    JsonObject obj;
    obj.set("status"sv, health_status_to_string(overall_status));
    obj.set("timestamp"sv, static_cast<i64>(timestamp.seconds_since_epoch()));
    obj.set("uptime_seconds"sv, static_cast<i64>(uptime.to_seconds()));

    // Components
    JsonObject components_obj;
    for (auto const& component : components) {
        components_obj.set(component.component_name, component.to_json());
    }
    obj.set("components"sv, move(components_obj));

    // Metrics
    if (!metrics.is_empty()) {
        JsonObject metrics_obj;
        for (auto const& [key, value] : metrics) {
            metrics_obj.set(key, value);
        }
        obj.set("metrics"sv, move(metrics_obj));
    }

    return obj;
}

HealthCheck::HealthCheck()
{
    m_startup_time = UnixDateTime::now();
}

void HealthCheck::register_check(String component_name, HealthCheckFunc callback)
{
    m_health_checks.set(move(component_name), move(callback));
}

void HealthCheck::unregister_check(String const& component_name)
{
    m_health_checks.remove(component_name);
    m_cached_results.remove(component_name);
}

ErrorOr<HealthCheck::Status> HealthCheck::check_health()
{
    auto report = TRY(check_all_components());
    return report.overall_status;
}

ErrorOr<HealthCheck::HealthReport> HealthCheck::check_all_components()
{
    m_total_checks++;

    HealthReport report;
    report.timestamp = UnixDateTime::now();
    report.uptime = get_uptime();
    report.overall_status = Status::Healthy;

    // Execute all registered health checks
    for (auto const& [component_name, check_func] : m_health_checks) {
        auto start_time = UnixDateTime::now();

        auto result = check_func();

        ComponentHealth component_health;
        if (result.is_error()) {
            // Health check itself failed - mark as unhealthy
            component_health.component_name = component_name;
            component_health.status = Status::Unhealthy;
            component_health.message = MUST(String::formatted("Health check failed: {}", result.error()));
            component_health.last_check = start_time;
            component_health.response_time = UnixDateTime::now() - start_time;
        } else {
            component_health = result.release_value();
            component_health.last_check = start_time;
            if (component_health.response_time == Duration::zero())
                component_health.response_time = UnixDateTime::now() - start_time;
        }

        // Update cached result
        m_cached_results.set(component_name, component_health);
        report.components.append(component_health);

        // Update overall status (worst status wins)
        report.overall_status = worst_status(report.overall_status, component_health.status);

        // Update metrics
        switch (component_health.status) {
        case Status::Healthy:
            m_healthy_checks++;
            break;
        case Status::Degraded:
            m_degraded_checks++;
            break;
        case Status::Unhealthy:
            m_unhealthy_checks++;
            break;
        }
    }

    // Collect metrics
    report.metrics = get_metrics();

    // Cache the report
    m_last_report = report;
    m_last_full_check = report.timestamp;

    return report;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_component(String const& component_name)
{
    auto it = m_health_checks.find(component_name);
    if (it == m_health_checks.end())
        return Error::from_string_literal("Component not registered");

    auto start_time = UnixDateTime::now();
    auto result = TRY(it->value());
    result.last_check = start_time;

    if (result.response_time == Duration::zero())
        result.response_time = UnixDateTime::now() - start_time;

    // Update cache
    m_cached_results.set(component_name, result);

    return result;
}

HealthCheck::LivenessProbe HealthCheck::check_liveness() const
{
    LivenessProbe probe;

    // Process is alive if this code is executing
    probe.alive = true;

    // Could add additional checks here (e.g., check if event loop is responsive)

    return probe;
}

HealthCheck::ReadinessProbe HealthCheck::check_readiness()
{
    ReadinessProbe probe;
    probe.ready = true;

    // Check critical components for readiness
    // A service is ready if critical components are not unhealthy

    Vector<String> critical_components = {
        "database"_string,
        "yara"_string,
        "quarantine"_string
    };

    for (auto const& component_name : critical_components) {
        auto cached = get_cached_result(component_name);

        if (cached.has_value() && cached.value().status == Status::Unhealthy) {
            probe.ready = false;
            probe.blocking_components.append(component_name);
        }
    }

    if (!probe.ready) {
        StringBuilder reason_builder;
        reason_builder.append("Critical components unhealthy: "sv);
        for (size_t i = 0; i < probe.blocking_components.size(); i++) {
            if (i > 0)
                reason_builder.append(", "sv);
            reason_builder.append(probe.blocking_components[i]);
        }
        probe.reason = MUST(reason_builder.to_string());
    }

    return probe;
}

Optional<HealthCheck::ComponentHealth> HealthCheck::get_cached_result(String const& component_name) const
{
    auto it = m_cached_results.find(component_name);
    if (it != m_cached_results.end())
        return it->value;
    return {};
}

void HealthCheck::start_periodic_checks(Duration interval)
{
    m_periodic_checks_enabled = true;
    m_check_interval = interval;
    m_next_check_time = UnixDateTime::now() + interval;
}

void HealthCheck::stop_periodic_checks()
{
    m_periodic_checks_enabled = false;
    m_next_check_time = {};
}

HashMap<String, i64> HealthCheck::get_metrics() const
{
    HashMap<String, i64> metrics;

    metrics.set("sentinel_uptime_seconds"_string, static_cast<i64>(get_uptime().to_seconds()));
    metrics.set("sentinel_health_checks_total"_string, m_total_checks);
    metrics.set("sentinel_health_checks_healthy"_string, m_healthy_checks);
    metrics.set("sentinel_health_checks_degraded"_string, m_degraded_checks);
    metrics.set("sentinel_health_checks_unhealthy"_string, m_unhealthy_checks);

    // Component count
    metrics.set("sentinel_registered_components"_string, static_cast<i64>(m_health_checks.size()));

    return metrics;
}

String HealthCheck::get_metrics_prometheus_format() const
{
    StringBuilder builder;

    auto metrics = get_metrics();

    builder.append("# HELP sentinel_uptime_seconds Time since Sentinel service started\n"sv);
    builder.append("# TYPE sentinel_uptime_seconds gauge\n"sv);

    builder.append("# HELP sentinel_health_checks_total Total number of health checks performed\n"sv);
    builder.append("# TYPE sentinel_health_checks_total counter\n"sv);

    builder.append("# HELP sentinel_health_checks_healthy Number of healthy health checks\n"sv);
    builder.append("# TYPE sentinel_health_checks_healthy counter\n"sv);

    builder.append("# HELP sentinel_health_checks_degraded Number of degraded health checks\n"sv);
    builder.append("# TYPE sentinel_health_checks_degraded counter\n"sv);

    builder.append("# HELP sentinel_health_checks_unhealthy Number of unhealthy health checks\n"sv);
    builder.append("# TYPE sentinel_health_checks_unhealthy counter\n"sv);

    builder.append("# HELP sentinel_registered_components Number of registered health check components\n"sv);
    builder.append("# TYPE sentinel_registered_components gauge\n"sv);

    for (auto const& [key, value] : metrics) {
        builder.appendff("{} {}\n", key, value);
    }

    return MUST(builder.to_string());
}

void HealthCheck::clear_cache()
{
    m_cached_results.clear();
}

Duration HealthCheck::get_uptime() const
{
    return UnixDateTime::now() - m_startup_time;
}

StringView HealthCheck::status_to_string(Status status)
{
    switch (status) {
    case Status::Healthy:
        return "healthy"sv;
    case Status::Degraded:
        return "degraded"sv;
    case Status::Unhealthy:
        return "unhealthy"sv;
    }
    VERIFY_NOT_REACHED();
}

HealthCheck::Status HealthCheck::worst_status(Status a, Status b)
{
    // Unhealthy is worst, then Degraded, then Healthy
    if (a == Status::Unhealthy || b == Status::Unhealthy)
        return Status::Unhealthy;
    if (a == Status::Degraded || b == Status::Degraded)
        return Status::Degraded;
    return Status::Healthy;
}

// Static health check implementations

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_database_health(PolicyGraph* policy_graph)
{
    ComponentHealth health;
    health.component_name = "database"_string;

    if (!policy_graph) {
        health.status = Status::Unhealthy;
        health.message = "PolicyGraph not initialized"_string;
        return health;
    }

    auto start_time = UnixDateTime::now();

    // Check database integrity
    auto integrity_result = policy_graph->verify_database_integrity();
    if (integrity_result.is_error()) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("Database integrity check failed: {}", integrity_result.error()));
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    // Check if database is healthy
    if (!policy_graph->is_database_healthy()) {
        health.status = Status::Degraded;
        health.message = "Database marked as unhealthy"_string;
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    // Try to get policy count (simple query test)
    auto count_result = policy_graph->get_policy_count();
    if (count_result.is_error()) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("Failed to query database: {}", count_result.error()));
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    health.status = Status::Healthy;
    health.response_time = UnixDateTime::now() - start_time;

    // Add details
    JsonObject details;
    details.set("policy_count"sv, static_cast<i64>(count_result.value()));

    auto threat_count_result = policy_graph->get_threat_count();
    if (!threat_count_result.is_error())
        details.set("threat_count"sv, static_cast<i64>(threat_count_result.value()));

    // Cache metrics
    auto cache_metrics = policy_graph->get_cache_metrics();
    details.set("cache_hit_rate"sv, static_cast<i64>(cache_metrics.hit_rate() * 100));
    // details.set("cache_size"sv, static_cast<i64>(cache_metrics.total_accesses));

    health.details = move(details);

    return health;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_yara_health()
{
    ComponentHealth health;
    health.component_name = "yara"_string;

    auto start_time = UnixDateTime::now();

    // Check if YARA is initialized by attempting a simple operation
    YR_COMPILER* compiler = nullptr;
    int result = yr_compiler_create(&compiler);

    if (result != ERROR_SUCCESS) {
        health.status = Status::Unhealthy;
        health.message = "YARA compiler creation failed"_string;
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    yr_compiler_destroy(compiler);

    health.status = Status::Healthy;
    health.response_time = UnixDateTime::now() - start_time;
    health.message = "YARA scanner operational"_string;

    // Add details
    JsonObject details;
    details.set("version"sv, "YARA 4.x"sv);
    health.details = move(details);

    return health;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_quarantine_health()
{
    ComponentHealth health;
    health.component_name = "quarantine"_string;

    auto start_time = UnixDateTime::now();

    // Check quarantine directory
    ByteString quarantine_dir = "/tmp/sentinel-quarantine"sv;

    // Check if directory exists
    if (!FileSystem::exists(quarantine_dir)) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("Quarantine directory does not exist: {}", quarantine_dir));
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    // Check if directory is writable
    // Check disk space
    struct statvfs stat;
    if (statvfs(quarantine_dir.characters(), &stat) != 0) {
        health.status = Status::Degraded;
        health.message = "Failed to check quarantine disk space"_string;
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    u64 available_bytes = static_cast<u64>(stat.f_bavail) * stat.f_frsize;
    u64 available_mb = available_bytes / (1024 * 1024);

    // Warn if less than 1GB available
    if (available_mb < 1024) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("Low disk space in quarantine: {}MB available", available_mb));
    } else {
        health.status = Status::Healthy;
        health.message = "Quarantine directory accessible"_string;
    }

    health.response_time = UnixDateTime::now() - start_time;

    // Add details
    JsonObject details;
    details.set("path"sv, quarantine_dir.view());
    details.set("available_mb"sv, static_cast<i64>(available_mb));
    health.details = move(details);

    return health;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_disk_space()
{
    ComponentHealth health;
    health.component_name = "disk"_string;

    auto start_time = UnixDateTime::now();

    // Check /tmp disk space
    struct statvfs stat;
    if (statvfs("/tmp", &stat) != 0) {
        health.status = Status::Degraded;
        health.message = "Failed to check /tmp disk space"_string;
        health.response_time = UnixDateTime::now() - start_time;
        return health;
    }

    u64 available_bytes = static_cast<u64>(stat.f_bavail) * stat.f_frsize;
    u64 available_gb = available_bytes / (1024 * 1024 * 1024);

    JsonObject details;
    details.set("tmp_available_gb"sv, static_cast<i64>(available_gb));

    // Check /home disk space if different
    struct statvfs home_stat;
    if (statvfs("/home", &home_stat) == 0) {
        u64 home_available_bytes = static_cast<u64>(home_stat.f_bavail) * home_stat.f_frsize;
        u64 home_available_gb = home_available_bytes / (1024 * 1024 * 1024);
        details.set("home_available_gb"sv, static_cast<i64>(home_available_gb));

        // Check if either is critically low
        if (available_gb < 1 || home_available_gb < 1) {
            health.status = Status::Unhealthy;
            health.message = "Critical: Less than 1GB free disk space"_string;
        } else if (available_gb < 5 || home_available_gb < 5) {
            health.status = Status::Degraded;
            health.message = "Warning: Less than 5GB free disk space"_string;
        } else {
            health.status = Status::Healthy;
        }
    } else {
        // Only /tmp available
        if (available_gb < 1) {
            health.status = Status::Unhealthy;
            health.message = "Critical: Less than 1GB free in /tmp"_string;
        } else if (available_gb < 5) {
            health.status = Status::Degraded;
            health.message = "Warning: Less than 5GB free in /tmp"_string;
        } else {
            health.status = Status::Healthy;
        }
    }

    health.response_time = UnixDateTime::now() - start_time;
    health.details = move(details);

    return health;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_memory_usage()
{
    ComponentHealth health;
    health.component_name = "memory"_string;

    auto start_time = UnixDateTime::now();

    // Read /proc/self/status for memory info
    auto status_file = TRY(Core::File::open("/proc/self/status"sv, Core::File::OpenMode::Read));
    auto content = TRY(status_file->read_until_eof());
    auto content_str = StringView(content);

    // Parse VmRSS (Resident Set Size)
    u64 rss_kb = 0;
    for (auto line : content_str.split_view('\n')) {
        if (line.starts_with("VmRSS:"sv)) {
            auto parts = line.split_view(' ', SplitBehavior::KeepEmpty);
            for (auto part : parts) {
                part = part.trim_whitespace();
                if (part.is_empty())
                    continue;
                auto parsed = part.to_number<u64>();
                if (parsed.has_value()) {
                    rss_kb = parsed.value();
                    break;
                }
            }
            break;
        }
    }

    u64 rss_mb = rss_kb / 1024;

    JsonObject details;
    details.set("rss_mb"sv, static_cast<i64>(rss_mb));

    // Thresholds: >2GB is unhealthy, >1GB is degraded
    if (rss_mb > 2048) {
        health.status = Status::Unhealthy;
        health.message = MUST(String::formatted("High memory usage: {}MB", rss_mb));
    } else if (rss_mb > 1024) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("Elevated memory usage: {}MB", rss_mb));
    } else {
        health.status = Status::Healthy;
        health.message = MUST(String::formatted("Memory usage: {}MB", rss_mb));
    }

    health.response_time = UnixDateTime::now() - start_time;
    health.details = move(details);

    return health;
}

ErrorOr<HealthCheck::ComponentHealth> HealthCheck::check_ipc_health(size_t active_connections)
{
    ComponentHealth health;
    health.component_name = "ipc"_string;

    auto start_time = UnixDateTime::now();

    JsonObject details;
    details.set("active_connections"sv, static_cast<i64>(active_connections));

    // Thresholds: >1000 connections is unhealthy, >500 is degraded
    if (active_connections > 1000) {
        health.status = Status::Unhealthy;
        health.message = MUST(String::formatted("Too many IPC connections: {}", active_connections));
    } else if (active_connections > 500) {
        health.status = Status::Degraded;
        health.message = MUST(String::formatted("High IPC connection count: {}", active_connections));
    } else {
        health.status = Status::Healthy;
        health.message = MUST(String::formatted("{} active connections", active_connections));
    }

    health.response_time = UnixDateTime::now() - start_time;
    health.details = move(details);

    return health;
}

// Helper functions

StringView health_status_to_string(HealthCheck::Status status)
{
    return HealthCheck::status_to_string(status);
}

Optional<HealthCheck::Status> string_to_health_status(StringView status_str)
{
    if (status_str == "healthy"sv)
        return HealthCheck::Status::Healthy;
    if (status_str == "degraded"sv)
        return HealthCheck::Status::Degraded;
    if (status_str == "unhealthy"sv)
        return HealthCheck::Status::Unhealthy;
    return {};
}

}
