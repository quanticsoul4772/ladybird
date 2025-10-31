/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <Services/Sentinel/GracefulDegradation.h>

using namespace Sentinel;

TEST_CASE(test_initial_state)
{
    GracefulDegradation degradation;

    // Initially, system should be in normal state
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Normal);

    // No services should be degraded or failed
    EXPECT_EQ(degradation.get_degraded_services().size(), 0u);
    EXPECT_EQ(degradation.get_failed_services().size(), 0u);

    // Unknown service should be healthy
    EXPECT_EQ(degradation.get_service_state("unknown"_string), GracefulDegradation::ServiceState::Healthy);
}

TEST_CASE(test_service_degradation)
{
    GracefulDegradation degradation;

    // Mark a service as degraded
    degradation.set_service_state(
        "TestService"_string,
        GracefulDegradation::ServiceState::Degraded,
        "Test degradation"_string,
        GracefulDegradation::FallbackStrategy::UseCache
    );

    // Check service state
    EXPECT_EQ(degradation.get_service_state("TestService"_string), GracefulDegradation::ServiceState::Degraded);

    // System should be degraded
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Degraded);

    // Service should appear in degraded list
    auto degraded = degradation.get_degraded_services();
    EXPECT_EQ(degraded.size(), 1u);
    EXPECT_EQ(degraded[0], "TestService"_string);

    // Should use fallback
    EXPECT(degradation.should_use_fallback("TestService"_string));

    // Check fallback strategy
    auto strategy = degradation.get_fallback_strategy("TestService"_string);
    EXPECT(strategy.has_value());
    EXPECT_EQ(strategy.value(), GracefulDegradation::FallbackStrategy::UseCache);

    // Check fallback reason
    auto reason = degradation.get_fallback_reason("TestService"_string);
    EXPECT(reason.has_value());
    EXPECT_EQ(reason.value(), "Test degradation"_string);
}

TEST_CASE(test_service_failure)
{
    GracefulDegradation degradation;

    // Mark a service as failed
    degradation.set_service_state(
        "DatabaseService"_string,
        GracefulDegradation::ServiceState::Failed,
        "Connection lost"_string,
        GracefulDegradation::FallbackStrategy::UseCache
    );

    // Check service state
    EXPECT_EQ(degradation.get_service_state("DatabaseService"_string), GracefulDegradation::ServiceState::Failed);

    // System should be degraded (not critical yet)
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Degraded);

    // Service should appear in failed list
    auto failed = degradation.get_failed_services();
    EXPECT_EQ(failed.size(), 1u);
    EXPECT_EQ(failed[0], "DatabaseService"_string);
}

TEST_CASE(test_critical_failure)
{
    GracefulDegradation degradation;

    // Mark a service as critical
    degradation.set_service_state(
        "CoreService"_string,
        GracefulDegradation::ServiceState::Critical,
        "Unrecoverable error"_string,
        GracefulDegradation::FallbackStrategy::None
    );

    // System should be in critical failure state
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::CriticalFailure);

    // Service should appear in failed list
    auto failed = degradation.get_failed_services();
    EXPECT_EQ(failed.size(), 1u);
}

TEST_CASE(test_service_recovery)
{
    GracefulDegradation degradation;

    // Mark service as failed
    degradation.set_service_state(
        "RecoverableService"_string,
        GracefulDegradation::ServiceState::Failed,
        "Temporary failure"_string
    );

    EXPECT_EQ(degradation.get_service_state("RecoverableService"_string), GracefulDegradation::ServiceState::Failed);

    // Mark service as recovered
    degradation.mark_service_recovered("RecoverableService"_string);

    // Service should be healthy now
    EXPECT_EQ(degradation.get_service_state("RecoverableService"_string), GracefulDegradation::ServiceState::Healthy);

    // System should be normal
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Normal);

    // Should not use fallback
    EXPECT(!degradation.should_use_fallback("RecoverableService"_string));
}

TEST_CASE(test_multiple_service_failures)
{
    GracefulDegradation degradation;

    // Mark multiple services as degraded/failed
    degradation.set_service_state("Service1"_string, GracefulDegradation::ServiceState::Degraded, "Issue 1"_string);
    degradation.set_service_state("Service2"_string, GracefulDegradation::ServiceState::Failed, "Issue 2"_string);
    degradation.set_service_state("Service3"_string, GracefulDegradation::ServiceState::Degraded, "Issue 3"_string);

    // Check counts
    EXPECT_EQ(degradation.get_degraded_services().size(), 2u);
    EXPECT_EQ(degradation.get_failed_services().size(), 1u);
    EXPECT_EQ(degradation.get_all_service_failures().size(), 3u);

    // System should be degraded
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Degraded);
}

TEST_CASE(test_recovery_attempts)
{
    GracefulDegradation degradation;
    degradation.set_recovery_attempt_limit(3);

    // Mark service as failed
    degradation.set_service_state(
        "FailingService"_string,
        GracefulDegradation::ServiceState::Failed,
        "Connection error"_string
    );

    // Attempt recovery multiple times
    degradation.attempt_recovery("FailingService"_string);
    EXPECT(degradation.is_recovery_in_progress("FailingService"_string));

    degradation.attempt_recovery("FailingService"_string);
    EXPECT(degradation.is_recovery_in_progress("FailingService"_string));

    // Third attempt should mark as critical
    degradation.attempt_recovery("FailingService"_string);

    // Service should be critical now
    EXPECT_EQ(degradation.get_service_state("FailingService"_string), GracefulDegradation::ServiceState::Critical);
    EXPECT(!degradation.is_recovery_in_progress("FailingService"_string));
}

TEST_CASE(test_auto_recovery_configuration)
{
    GracefulDegradation degradation;

    // Default should be enabled
    EXPECT(degradation.is_auto_recovery_enabled("TestService"_string));

    // Disable auto-recovery
    degradation.enable_auto_recovery("TestService"_string, false);
    EXPECT(!degradation.is_auto_recovery_enabled("TestService"_string));

    // Re-enable auto-recovery
    degradation.enable_auto_recovery("TestService"_string, true);
    EXPECT(degradation.is_auto_recovery_enabled("TestService"_string));
}

TEST_CASE(test_degradation_callbacks)
{
    GracefulDegradation degradation;

    bool callback_invoked = false;
    String callback_service_name;
    GracefulDegradation::ServiceState callback_old_state = GracefulDegradation::ServiceState::Healthy;
    GracefulDegradation::ServiceState callback_new_state = GracefulDegradation::ServiceState::Healthy;

    // Register callback
    degradation.register_degradation_callback([&](GracefulDegradation::DegradationEvent const& event) {
        callback_invoked = true;
        callback_service_name = event.service_name;
        callback_old_state = event.old_state;
        callback_new_state = event.new_state;
    });

    // Trigger state change
    degradation.set_service_state(
        "CallbackTest"_string,
        GracefulDegradation::ServiceState::Degraded,
        "Test callback"_string
    );

    // Verify callback was invoked
    EXPECT(callback_invoked);
    EXPECT_EQ(callback_service_name, "CallbackTest"_string);
    EXPECT_EQ(callback_old_state, GracefulDegradation::ServiceState::Healthy);
    EXPECT_EQ(callback_new_state, GracefulDegradation::ServiceState::Degraded);
}

TEST_CASE(test_metrics)
{
    GracefulDegradation degradation;

    // Initial metrics
    auto metrics = degradation.get_metrics();
    EXPECT_EQ(metrics.total_failures, 0u);
    EXPECT_EQ(metrics.total_recoveries, 0u);
    EXPECT_EQ(metrics.system_level, GracefulDegradation::DegradationLevel::Normal);

    // Add some failures
    degradation.set_service_state("Service1"_string, GracefulDegradation::ServiceState::Degraded, "Test"_string);
    degradation.set_service_state("Service2"_string, GracefulDegradation::ServiceState::Failed, "Test"_string);

    metrics = degradation.get_metrics();
    EXPECT_EQ(metrics.total_services, 2u);
    EXPECT_EQ(metrics.healthy_services, 0u);
    EXPECT_EQ(metrics.degraded_services, 1u);
    EXPECT_EQ(metrics.failed_services, 1u);
    EXPECT_EQ(metrics.total_failures, 2u);
    EXPECT(metrics.last_failure_time.has_value());

    // Recover one service
    degradation.mark_service_recovered("Service1"_string);

    metrics = degradation.get_metrics();
    EXPECT_EQ(metrics.healthy_services, 1u);
    EXPECT_EQ(metrics.degraded_services, 0u);
    EXPECT_EQ(metrics.total_recoveries, 1u);
    EXPECT(metrics.last_recovery_time.has_value());
}

TEST_CASE(test_health_status)
{
    GracefulDegradation degradation;

    // Initial health status
    auto health = degradation.get_health_status();
    EXPECT(health.is_healthy);
    EXPECT_EQ(health.level, GracefulDegradation::DegradationLevel::Normal);
    EXPECT_EQ(health.degraded_services.size(), 0u);
    EXPECT_EQ(health.failed_services.size(), 0u);
    EXPECT(!health.critical_message.has_value());

    // Add degraded service
    degradation.set_service_state("Service1"_string, GracefulDegradation::ServiceState::Degraded, "Test"_string);

    health = degradation.get_health_status();
    EXPECT(!health.is_healthy);
    EXPECT_EQ(health.level, GracefulDegradation::DegradationLevel::Degraded);
    EXPECT_EQ(health.degraded_services.size(), 1u);

    // Add critical failure
    degradation.set_service_state("Service2"_string, GracefulDegradation::ServiceState::Critical, "Critical test"_string);

    health = degradation.get_health_status();
    EXPECT(!health.is_healthy);
    EXPECT_EQ(health.level, GracefulDegradation::DegradationLevel::CriticalFailure);
    EXPECT(health.critical_message.has_value());
}

TEST_CASE(test_fallback_strategies)
{
    GracefulDegradation degradation;

    // Test each fallback strategy
    auto strategies = {
        GracefulDegradation::FallbackStrategy::None,
        GracefulDegradation::FallbackStrategy::UseCache,
        GracefulDegradation::FallbackStrategy::AllowWithWarning,
        GracefulDegradation::FallbackStrategy::SkipWithLog,
        GracefulDegradation::FallbackStrategy::RetryWithBackoff,
        GracefulDegradation::FallbackStrategy::QueueForRetry
    };

    int i = 0;
    for (auto strategy : strategies) {
        auto service_name = String::formatted("Service{}", i++);
        degradation.set_service_state(
            service_name,
            GracefulDegradation::ServiceState::Failed,
            "Testing strategy"_string,
            strategy
        );

        auto retrieved_strategy = degradation.get_fallback_strategy(service_name);
        EXPECT(retrieved_strategy.has_value());
        EXPECT_EQ(retrieved_strategy.value(), strategy);
    }
}

TEST_CASE(test_metrics_reset)
{
    GracefulDegradation degradation;

    // Generate some metrics
    degradation.set_service_state("Service1"_string, GracefulDegradation::ServiceState::Failed, "Test"_string);
    degradation.mark_service_recovered("Service1"_string);

    auto metrics = degradation.get_metrics();
    EXPECT(metrics.total_failures > 0);
    EXPECT(metrics.total_recoveries > 0);

    // Reset metrics
    degradation.reset_metrics();

    metrics = degradation.get_metrics();
    EXPECT_EQ(metrics.total_failures, 0u);
    EXPECT_EQ(metrics.total_recoveries, 0u);
    EXPECT(!metrics.last_failure_time.has_value());
    EXPECT(!metrics.last_recovery_time.has_value());
}

TEST_CASE(test_helper_functions)
{
    // Test enum to string conversions
    EXPECT_EQ(degradation_level_to_string(GracefulDegradation::DegradationLevel::Normal), "Normal"_string);
    EXPECT_EQ(degradation_level_to_string(GracefulDegradation::DegradationLevel::Degraded), "Degraded"_string);
    EXPECT_EQ(degradation_level_to_string(GracefulDegradation::DegradationLevel::CriticalFailure), "CriticalFailure"_string);

    EXPECT_EQ(service_state_to_string(GracefulDegradation::ServiceState::Healthy), "Healthy"_string);
    EXPECT_EQ(service_state_to_string(GracefulDegradation::ServiceState::Degraded), "Degraded"_string);
    EXPECT_EQ(service_state_to_string(GracefulDegradation::ServiceState::Failed), "Failed"_string);
    EXPECT_EQ(service_state_to_string(GracefulDegradation::ServiceState::Critical), "Critical"_string);

    EXPECT_EQ(fallback_strategy_to_string(GracefulDegradation::FallbackStrategy::None), "None"_string);
    EXPECT_EQ(fallback_strategy_to_string(GracefulDegradation::FallbackStrategy::UseCache), "UseCache"_string);
    EXPECT_EQ(fallback_strategy_to_string(GracefulDegradation::FallbackStrategy::AllowWithWarning), "AllowWithWarning"_string);
}

TEST_CASE(test_cascade_failure_prevention)
{
    GracefulDegradation degradation;

    // Simulate cascade failure scenario
    // Service A fails, then B, then C
    degradation.set_service_state("ServiceA"_string, GracefulDegradation::ServiceState::Failed, "Primary failure"_string);
    degradation.set_service_state("ServiceB"_string, GracefulDegradation::ServiceState::Failed, "Cascade from A"_string);
    degradation.set_service_state("ServiceC"_string, GracefulDegradation::ServiceState::Failed, "Cascade from B"_string);

    // System should handle multiple failures gracefully
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Degraded);
    EXPECT_EQ(degradation.get_failed_services().size(), 3u);

    // Recovery should be tracked independently
    degradation.mark_service_recovered("ServiceA"_string);
    EXPECT_EQ(degradation.get_failed_services().size(), 2u);

    degradation.mark_service_recovered("ServiceB"_string);
    EXPECT_EQ(degradation.get_failed_services().size(), 1u);

    degradation.mark_service_recovered("ServiceC"_string);
    EXPECT_EQ(degradation.get_failed_services().size(), 0u);
    EXPECT_EQ(degradation.get_system_degradation_level(), GracefulDegradation::DegradationLevel::Normal);
}

TEST_CASE(test_predefined_service_names)
{
    // Test that predefined service names are accessible
    EXPECT_EQ(Services::PolicyGraph, "PolicyGraph"sv);
    EXPECT_EQ(Services::YARAScanner, "YARAScanner"sv);
    EXPECT_EQ(Services::IPCServer, "IPCServer"sv);
    EXPECT_EQ(Services::Database, "Database"sv);
    EXPECT_EQ(Services::Quarantine, "Quarantine"sv);
    EXPECT_EQ(Services::SecurityTap, "SecurityTap"sv);
    EXPECT_EQ(Services::NetworkLayer, "NetworkLayer"sv);
}
