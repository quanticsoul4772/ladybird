/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/CircuitBreaker.h>
#include <LibTest/TestCase.h>
#include <AK/Time.h>

using AK::Duration;
using AK::String;

TEST_CASE(circuit_breaker_starts_closed)
{
    Core::CircuitBreaker cb("test"sv);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
    EXPECT(cb.is_request_allowed());
}

TEST_CASE(circuit_breaker_opens_after_threshold_failures)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 3,
        .timeout = Duration::from_seconds(1),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // First 2 failures should keep circuit closed
    for (int i = 0; i < 2; i++) {
        auto result = cb.execute([]() -> ErrorOr<int> {
            return Error::from_string_literal("Simulated failure");
        });
        EXPECT(result.is_error());
        EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
    }

    // Third failure should open the circuit
    auto result = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);
}

TEST_CASE(circuit_breaker_rejects_requests_when_open)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_seconds(10),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Trip the circuit
    auto result1 = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result1.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);

    // Next request should be rejected immediately without executing
    bool executed = false;
    auto result2 = cb.execute([&]() -> ErrorOr<int> {
        executed = true;
        return 42;
    });
    EXPECT(result2.is_error());
    EXPECT(!executed); // Function should not have been called
}

TEST_CASE(circuit_breaker_transitions_to_half_open_after_timeout)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_milliseconds(100),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Trip the circuit
    auto result1 = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result1.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);

    // Wait for timeout
    usleep(150000); // 150ms

    // Next request should transition to half-open and execute
    bool executed = false;
    auto result2 = cb.execute([&]() -> ErrorOr<int> {
        executed = true;
        return 42;
    });
    EXPECT(!result2.is_error());
    EXPECT(executed);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed); // Success closes circuit
}

TEST_CASE(circuit_breaker_closes_after_success_threshold)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_milliseconds(100),
        .success_threshold = 3,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Trip the circuit
    auto result1 = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result1.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);

    // Wait for timeout
    usleep(150000); // 150ms

    // First success should transition to half-open
    auto result2 = cb.execute([]() -> ErrorOr<int> {
        return 1;
    });
    EXPECT(!result2.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::HalfOpen);

    // Second success
    auto result3 = cb.execute([]() -> ErrorOr<int> {
        return 2;
    });
    EXPECT(!result3.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::HalfOpen);

    // Third success should close the circuit
    auto result4 = cb.execute([]() -> ErrorOr<int> {
        return 3;
    });
    EXPECT(!result4.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
}

TEST_CASE(circuit_breaker_reopens_on_half_open_failure)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_milliseconds(100),
        .success_threshold = 2,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Trip the circuit
    auto result1 = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result1.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);

    // Wait for timeout
    usleep(150000); // 150ms

    // First success transitions to half-open
    auto result2 = cb.execute([]() -> ErrorOr<int> {
        return 1;
    });
    EXPECT(!result2.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::HalfOpen);

    // Failure in half-open should reopen circuit
    auto result3 = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result3.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);
}

TEST_CASE(circuit_breaker_manual_trip)
{
    Core::CircuitBreaker cb("test"sv);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);

    cb.trip();
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);
}

TEST_CASE(circuit_breaker_manual_reset)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_seconds(100),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Trip the circuit
    auto result = cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });
    EXPECT(result.is_error());
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Open);

    // Manual reset
    cb.reset();
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
}

TEST_CASE(circuit_breaker_metrics_tracking)
{
    Core::CircuitBreaker cb("test"sv);

    // Execute some successful operations
    for (int i = 0; i < 3; i++) {
        auto result = cb.execute([]() -> ErrorOr<int> {
            return 42;
        });
        EXPECT(!result.is_error());
    }

    // Execute some failed operations
    for (int i = 0; i < 2; i++) {
        auto result = cb.execute([]() -> ErrorOr<int> {
            return Error::from_string_literal("Simulated failure");
        });
        EXPECT(result.is_error());
    }

    auto metrics = cb.get_metrics();
    EXPECT_EQ(metrics.total_successes, 3u);
    EXPECT_EQ(metrics.total_failures, 2u);
    EXPECT_EQ(metrics.consecutive_failures, 2u);
}

TEST_CASE(circuit_breaker_presets_database)
{
    auto cb = Core::CircuitBreakerPresets::database();
    EXPECT_EQ(cb.name(), "database"sv);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
}

TEST_CASE(circuit_breaker_presets_yara)
{
    auto cb = Core::CircuitBreakerPresets::yara_scanner();
    EXPECT_EQ(cb.name(), "yara_scanner"sv);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
}

TEST_CASE(circuit_breaker_presets_ipc)
{
    auto cb = Core::CircuitBreakerPresets::ipc_client();
    EXPECT_EQ(cb.name(), "ipc_client"sv);
    EXPECT_EQ(cb.state(), Core::CircuitBreaker::State::Closed);
}

TEST_CASE(circuit_breaker_consecutive_counter_reset_on_success)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 3,
        .timeout = Duration::from_seconds(1),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    // Two failures
    for (int i = 0; i < 2; i++) {
        auto result = cb.execute([]() -> ErrorOr<int> {
            return Error::from_string_literal("Simulated failure");
        });
        EXPECT(result.is_error());
    }

    auto metrics1 = cb.get_metrics();
    EXPECT_EQ(metrics1.consecutive_failures, 2u);

    // Success should reset consecutive failures
    auto result = cb.execute([]() -> ErrorOr<int> {
        return 42;
    });
    EXPECT(!result.is_error());

    auto metrics2 = cb.get_metrics();
    EXPECT_EQ(metrics2.consecutive_failures, 0u);
    EXPECT_EQ(metrics2.consecutive_successes, 1u);
}

TEST_CASE(circuit_breaker_state_change_tracking)
{
    Core::CircuitBreaker cb(Core::CircuitBreaker::Config {
        .failure_threshold = 1,
        .timeout = Duration::from_milliseconds(100),
        .success_threshold = 1,
        .name = MUST(String::from_utf8("test"sv)),
    });

    auto metrics1 = cb.get_metrics();
    EXPECT_EQ(metrics1.state_changes, 0u);

    // Trip circuit (Closed -> Open)
    (void)cb.execute([]() -> ErrorOr<int> {
        return Error::from_string_literal("Simulated failure");
    });

    auto metrics2 = cb.get_metrics();
    EXPECT_EQ(metrics2.state_changes, 1u);

    // Wait for timeout and execute success (Open -> HalfOpen -> Closed)
    usleep(150000); // 150ms
    (void)cb.execute([]() -> ErrorOr<int> {
        return 42;
    });

    auto metrics3 = cb.get_metrics();
    EXPECT_EQ(metrics3.state_changes, 3u); // Open->HalfOpen, HalfOpen->Closed
}
