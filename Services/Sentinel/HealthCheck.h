/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/JsonObject.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel {

using AK::Duration;

// Forward declarations for internal components
class PolicyGraph;

class HealthCheck {
public:
    // Overall health status
    enum class Status {
        Healthy,    // All systems operational
        Degraded,   // Some services degraded but functioning
        Unhealthy   // Critical services failed
    };

    // Individual component health information
    struct ComponentHealth {
        String component_name;
        Status status { Status::Healthy };
        Optional<String> message;
        UnixDateTime last_check;
        Duration response_time { Duration::zero() };
        Optional<JsonObject> details;  // Component-specific metrics

        // Helper to convert to JSON
        JsonObject to_json() const;
    };

    // Liveness and Readiness probe results
    struct LivenessProbe {
        bool alive { true };
        Optional<String> reason;

        JsonObject to_json() const;
    };

    struct ReadinessProbe {
        bool ready { true };
        Optional<String> reason;
        Vector<String> blocking_components;

        JsonObject to_json() const;
    };

    // Health check result aggregate
    struct HealthReport {
        Status overall_status { Status::Healthy };
        UnixDateTime timestamp;
        Duration uptime { Duration::zero() };
        Vector<ComponentHealth> components;
        HashMap<String, i64> metrics;

        JsonObject to_json() const;
    };

    HealthCheck();

    // Component registration
    using HealthCheckFunc = Function<ErrorOr<ComponentHealth>()>;
    void register_check(String component_name, HealthCheckFunc callback);
    void unregister_check(String const& component_name);

    // Perform health checks
    ErrorOr<Status> check_health();
    ErrorOr<HealthReport> check_all_components();
    ErrorOr<ComponentHealth> check_component(String const& component_name);

    // Liveness vs Readiness
    LivenessProbe check_liveness() const;
    ReadinessProbe check_readiness();

    // Get cached results (avoid re-checking too frequently)
    Optional<ComponentHealth> get_cached_result(String const& component_name) const;
    Optional<HealthReport> get_last_report() const { return m_last_report; }

    // Periodic health checks
    void start_periodic_checks(Duration interval = Duration::from_seconds(30));
    void stop_periodic_checks();

    // Metrics
    HashMap<String, i64> get_metrics() const;
    String get_metrics_prometheus_format() const;

    // Utility
    void clear_cache();
    Duration get_uptime() const;

    // Static health check implementations for common components
    static ErrorOr<ComponentHealth> check_database_health(PolicyGraph* policy_graph);
    static ErrorOr<ComponentHealth> check_yara_health();
    static ErrorOr<ComponentHealth> check_quarantine_health();
    static ErrorOr<ComponentHealth> check_disk_space();
    static ErrorOr<ComponentHealth> check_memory_usage();
    static ErrorOr<ComponentHealth> check_ipc_health(size_t active_connections);

    // Convert Status to string
    static StringView status_to_string(Status status);

private:
    static Status worst_status(Status a, Status b);

    // Component health checks
    HashMap<String, HealthCheckFunc> m_health_checks;
    HashMap<String, ComponentHealth> m_cached_results;

    // Last full report
    Optional<HealthReport> m_last_report;
    UnixDateTime m_last_full_check { UnixDateTime::now() };

    // Service startup time
    UnixDateTime m_startup_time { UnixDateTime::now() };

    // Periodic check state
    bool m_periodic_checks_enabled { false };
    Duration m_check_interval { Duration::from_seconds(30) };
    Optional<UnixDateTime> m_next_check_time;

    // Metrics
    mutable i64 m_total_checks { 0 };
    mutable i64 m_healthy_checks { 0 };
    mutable i64 m_degraded_checks { 0 };
    mutable i64 m_unhealthy_checks { 0 };
};

// Helper functions for status conversion
StringView health_status_to_string(HealthCheck::Status status);
Optional<HealthCheck::Status> string_to_health_status(StringView status_str);

}
