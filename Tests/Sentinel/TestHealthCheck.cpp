/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <Services/Sentinel/HealthCheck.h>
#include <AK/String.h>

using namespace Sentinel;

TEST_CASE(health_status_to_string)
{
    EXPECT_EQ(health_status_to_string(HealthCheck::Status::Healthy), "healthy"sv);
    EXPECT_EQ(health_status_to_string(HealthCheck::Status::Degraded), "degraded"sv);
    EXPECT_EQ(health_status_to_string(HealthCheck::Status::Unhealthy), "unhealthy"sv);
}

TEST_CASE(string_to_health_status)
{
    EXPECT_EQ(string_to_health_status("healthy"sv).value(), HealthCheck::Status::Healthy);
    EXPECT_EQ(string_to_health_status("degraded"sv).value(), HealthCheck::Status::Degraded);
    EXPECT_EQ(string_to_health_status("unhealthy"sv).value(), HealthCheck::Status::Unhealthy);
    EXPECT(!string_to_health_status("invalid"sv).has_value());
}

TEST_CASE(health_check_initialization)
{
    HealthCheck health_check;

    // Should start with no registered checks
    auto metrics = health_check.get_metrics();
    EXPECT_EQ(metrics.get("sentinel_registered_components"_string).value(), 0);
}

TEST_CASE(health_check_component_registration)
{
    HealthCheck health_check;

    // Register a simple health check
    health_check.register_check("test_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "test_component"_string;
        health.status = HealthCheck::Status::Healthy;
        health.message = "Test component is healthy"_string;
        return health;
    });

    auto metrics = health_check.get_metrics();
    EXPECT_EQ(metrics.get("sentinel_registered_components"_string).value(), 1);

    // Check the component
    auto result = health_check.check_component("test_component"_string);
    EXPECT(!result.is_error());
    EXPECT_EQ(result.value().component_name, "test_component"_string);
    EXPECT_EQ(result.value().status, HealthCheck::Status::Healthy);
}

TEST_CASE(health_check_unregister)
{
    HealthCheck health_check;

    health_check.register_check("test_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "test_component"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    EXPECT_EQ(health_check.get_metrics().get("sentinel_registered_components"_string).value(), 1);

    health_check.unregister_check("test_component"_string);
    EXPECT_EQ(health_check.get_metrics().get("sentinel_registered_components"_string).value(), 0);

    // Should fail to check unregistered component
    auto result = health_check.check_component("test_component"_string);
    EXPECT(result.is_error());
}

TEST_CASE(health_check_multiple_components)
{
    HealthCheck health_check;

    // Register multiple components with different health statuses
    health_check.register_check("healthy_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "healthy_component"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    health_check.register_check("degraded_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "degraded_component"_string;
        health.status = HealthCheck::Status::Degraded;
        health.message = "Component is degraded"_string;
        return health;
    });

    health_check.register_check("unhealthy_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "unhealthy_component"_string;
        health.status = HealthCheck::Status::Unhealthy;
        health.message = "Component is unhealthy"_string;
        return health;
    });

    // Check all components
    auto report = health_check.check_all_components();
    EXPECT(!report.is_error());

    // Overall status should be unhealthy (worst status)
    EXPECT_EQ(report.value().overall_status, HealthCheck::Status::Unhealthy);
    EXPECT_EQ(report.value().components.size(), 3u);
}

TEST_CASE(health_check_liveness_probe)
{
    HealthCheck health_check;

    auto liveness = health_check.check_liveness();
    EXPECT(liveness.alive);
}

TEST_CASE(health_check_readiness_probe_all_healthy)
{
    HealthCheck health_check;

    // Register critical components as healthy
    health_check.register_check("database"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "database"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    health_check.register_check("yara"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "yara"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    health_check.register_check("quarantine"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "quarantine"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    // Run health checks first to populate cache
    auto report_result = health_check.check_all_components();
    EXPECT(!report_result.is_error());

    auto readiness = health_check.check_readiness();
    EXPECT(readiness.ready);
    EXPECT(!readiness.reason.has_value());
}

TEST_CASE(health_check_readiness_probe_database_unhealthy)
{
    HealthCheck health_check;

    // Register database as unhealthy
    health_check.register_check("database"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "database"_string;
        health.status = HealthCheck::Status::Unhealthy;
        health.message = "Database connection failed"_string;
        return health;
    });

    health_check.register_check("yara"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "yara"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    health_check.register_check("quarantine"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "quarantine"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    // Run health checks first to populate cache
    auto report_result = health_check.check_all_components();
    EXPECT(!report_result.is_error());

    auto readiness = health_check.check_readiness();
    EXPECT(!readiness.ready);
    EXPECT(readiness.reason.has_value());
    EXPECT_EQ(readiness.blocking_components.size(), 1u);
    EXPECT_EQ(readiness.blocking_components[0], "database"_string);
}

TEST_CASE(health_check_cached_results)
{
    HealthCheck health_check;

    size_t call_count = 0;
    health_check.register_check("test_component"_string, [&call_count]() -> ErrorOr<HealthCheck::ComponentHealth> {
        call_count++;
        HealthCheck::ComponentHealth health;
        health.component_name = "test_component"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    // First check should call the function
    auto result1 = health_check.check_component("test_component"_string);
    EXPECT(!result1.is_error());
    EXPECT_EQ(call_count, 1u);

    // Get cached result (doesn't call function)
    auto cached = health_check.get_cached_result("test_component"_string);
    EXPECT(cached.has_value());
    EXPECT_EQ(cached.value().component_name, "test_component"_string);
    EXPECT_EQ(call_count, 1u); // Still 1, not called again
}

TEST_CASE(health_check_clear_cache)
{
    HealthCheck health_check;

    health_check.register_check("test_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "test_component"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    // Check and verify cache
    auto result = health_check.check_component("test_component"_string);
    EXPECT(!result.is_error());

    auto cached = health_check.get_cached_result("test_component"_string);
    EXPECT(cached.has_value());

    // Clear cache
    health_check.clear_cache();

    cached = health_check.get_cached_result("test_component"_string);
    EXPECT(!cached.has_value());
}

TEST_CASE(health_check_uptime)
{
    HealthCheck health_check;

    // Uptime should be very small initially
    auto uptime = health_check.get_uptime();
    EXPECT(uptime.to_seconds() >= 0);
    EXPECT(uptime.to_seconds() < 2); // Should be less than 2 seconds
}

TEST_CASE(health_check_metrics)
{
    HealthCheck health_check;

    auto metrics = health_check.get_metrics();

    // Verify all expected metrics exist
    EXPECT(metrics.contains("sentinel_uptime_seconds"_string));
    EXPECT(metrics.contains("sentinel_health_checks_total"_string));
    EXPECT(metrics.contains("sentinel_health_checks_healthy"_string));
    EXPECT(metrics.contains("sentinel_health_checks_degraded"_string));
    EXPECT(metrics.contains("sentinel_health_checks_unhealthy"_string));
    EXPECT(metrics.contains("sentinel_registered_components"_string));
}

TEST_CASE(health_check_prometheus_format)
{
    HealthCheck health_check;

    auto prometheus_output = health_check.get_metrics_prometheus_format();

    // Verify format contains expected metrics
    EXPECT(prometheus_output.contains("sentinel_uptime_seconds"sv));
    EXPECT(prometheus_output.contains("sentinel_health_checks_total"sv));
    EXPECT(prometheus_output.contains("sentinel_registered_components"sv));

    // Verify it contains HELP and TYPE annotations
    EXPECT(prometheus_output.contains("# HELP"sv));
    EXPECT(prometheus_output.contains("# TYPE"sv));
}

TEST_CASE(health_check_failing_component)
{
    HealthCheck health_check;

    // Register a component that throws an error
    health_check.register_check("failing_component"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        return Error::from_string_literal("Component check failed");
    });

    // Check should handle the error gracefully
    auto report = health_check.check_all_components();
    EXPECT(!report.is_error());

    // Component should be marked as unhealthy
    EXPECT_EQ(report.value().components.size(), 1u);
    EXPECT_EQ(report.value().components[0].status, HealthCheck::Status::Unhealthy);
    EXPECT(report.value().components[0].message.has_value());
}

TEST_CASE(health_check_component_health_json)
{
    HealthCheck::ComponentHealth health;
    health.component_name = "test"_string;
    health.status = HealthCheck::Status::Healthy;
    health.message = "All good"_string;
    health.last_check = UnixDateTime::now();
    health.response_time = Duration::from_milliseconds(10);

    auto json = health.to_json();
    EXPECT_EQ(json.get_string("component"sv).value(), "test"sv);
    EXPECT_EQ(json.get_string("status"sv).value(), "healthy"sv);
    EXPECT_EQ(json.get_string("message"sv).value(), "All good"sv);
    EXPECT(json.has_number("last_check"sv));
    EXPECT_EQ(json.get_i64("response_time_ms"sv).value(), 10);
}

TEST_CASE(health_check_liveness_probe_json)
{
    HealthCheck::LivenessProbe probe;
    probe.alive = true;

    auto json = probe.to_json();
    EXPECT_EQ(json.get_bool("alive"sv).value(), true);

    probe.alive = false;
    probe.reason = "Service shutting down"_string;

    json = probe.to_json();
    EXPECT_EQ(json.get_bool("alive"sv).value(), false);
    EXPECT_EQ(json.get_string("reason"sv).value(), "Service shutting down"sv);
}

TEST_CASE(health_check_readiness_probe_json)
{
    HealthCheck::ReadinessProbe probe;
    probe.ready = true;

    auto json = probe.to_json();
    EXPECT_EQ(json.get_bool("ready"sv).value(), true);

    probe.ready = false;
    probe.reason = "Database unavailable"_string;
    probe.blocking_components.append("database"_string);
    probe.blocking_components.append("cache"_string);

    json = probe.to_json();
    EXPECT_EQ(json.get_bool("ready"sv).value(), false);
    EXPECT_EQ(json.get_string("reason"sv).value(), "Database unavailable"sv);
    EXPECT(json.has("blocking_components"sv));
}

TEST_CASE(health_check_report_json)
{
    HealthCheck health_check;

    health_check.register_check("test"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "test"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    auto report = health_check.check_all_components();
    EXPECT(!report.is_error());

    auto json = report.value().to_json();
    EXPECT_EQ(json.get_string("status"sv).value(), "healthy"sv);
    EXPECT(json.has_number("timestamp"sv));
    EXPECT(json.has_number("uptime_seconds"sv));
    EXPECT(json.has_object("components"sv));
    EXPECT(json.has_object("metrics"sv));
}

TEST_CASE(health_check_yara_health)
{
    // Test YARA health check
    auto result = HealthCheck::check_yara_health();

    // Should not error (YARA is initialized)
    EXPECT(!result.is_error());

    auto health = result.value();
    EXPECT_EQ(health.component_name, "yara"_string);

    // Status depends on whether YARA is actually initialized
    // We just verify the check executes without crashing
}

TEST_CASE(health_check_quarantine_health)
{
    // Test quarantine health check
    auto result = HealthCheck::check_quarantine_health();

    // Should not error
    EXPECT(!result.is_error());

    auto health = result.value();
    EXPECT_EQ(health.component_name, "quarantine"_string);

    // Verify details contain expected fields
    if (health.details.has_value()) {
        auto details = health.details.value();
        EXPECT(details.has_string("path"sv));
        EXPECT(details.has_number("available_mb"sv));
    }
}

TEST_CASE(health_check_disk_space)
{
    // Test disk space health check
    auto result = HealthCheck::check_disk_space();

    // Should not error
    EXPECT(!result.is_error());

    auto health = result.value();
    EXPECT_EQ(health.component_name, "disk"_string);

    // Verify details contain disk space info
    if (health.details.has_value()) {
        auto details = health.details.value();
        EXPECT(details.has_number("tmp_available_gb"sv));
    }
}

TEST_CASE(health_check_memory_usage)
{
    // Test memory usage health check
    auto result = HealthCheck::check_memory_usage();

    // Should not error
    EXPECT(!result.is_error());

    auto health = result.value();
    EXPECT_EQ(health.component_name, "memory"_string);

    // Verify details contain memory info
    if (health.details.has_value()) {
        auto details = health.details.value();
        EXPECT(details.has_number("rss_mb"sv));

        // Memory usage should be reasonable (less than 2GB for test)
        auto rss_mb = details.get_i64("rss_mb"sv).value();
        EXPECT(rss_mb < 2048);
    }
}

TEST_CASE(health_check_ipc_health)
{
    // Test IPC health check with various connection counts
    auto result1 = HealthCheck::check_ipc_health(0);
    EXPECT(!result1.is_error());
    EXPECT_EQ(result1.value().status, HealthCheck::Status::Healthy);

    auto result2 = HealthCheck::check_ipc_health(100);
    EXPECT(!result2.is_error());
    EXPECT_EQ(result2.value().status, HealthCheck::Status::Healthy);

    auto result3 = HealthCheck::check_ipc_health(600);
    EXPECT(!result3.is_error());
    EXPECT_EQ(result3.value().status, HealthCheck::Status::Degraded);

    auto result4 = HealthCheck::check_ipc_health(1200);
    EXPECT(!result4.is_error());
    EXPECT_EQ(result4.value().status, HealthCheck::Status::Unhealthy);
}

TEST_CASE(health_check_periodic_checks)
{
    HealthCheck health_check;

    // Register a simple component
    health_check.register_check("test"_string, []() -> ErrorOr<HealthCheck::ComponentHealth> {
        HealthCheck::ComponentHealth health;
        health.component_name = "test"_string;
        health.status = HealthCheck::Status::Healthy;
        return health;
    });

    // Start periodic checks
    health_check.start_periodic_checks(Duration::from_seconds(5));

    // Verify we can still manually check
    auto result = health_check.check_all_components();
    EXPECT(!result.is_error());

    // Stop periodic checks
    health_check.stop_periodic_checks();
}
