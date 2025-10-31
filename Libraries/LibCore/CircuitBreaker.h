/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <LibThreading/Mutex.h>

namespace Core {

using AK::Duration;
using AK::String;
using AK::StringView;

// Circuit Breaker Pattern Implementation
// Prevents cascading failures by temporarily blocking operations to failing services
//
// States:
// - Closed: Normal operation, requests pass through
// - Open: Service failing, requests immediately rejected
// - HalfOpen: Testing recovery, limited requests allowed
//
// Transitions:
// - Closed -> Open: After reaching failure threshold
// - Open -> HalfOpen: After timeout period expires
// - HalfOpen -> Closed: After success threshold met
// - HalfOpen -> Open: If test request fails
//
// Thread-safe: All operations protected by mutex
class CircuitBreaker {
public:
    enum class State {
        Closed,   // Normal operation - requests pass through
        Open,     // Circuit tripped - requests fail immediately
        HalfOpen  // Testing recovery - limited requests allowed
    };

    struct Config {
        size_t failure_threshold { 5 };        // Open after N failures
        Duration timeout { Duration::from_seconds(30) };  // Time before trying again
        size_t success_threshold { 2 };        // Successes needed to close from half-open
        String name {};           // Circuit breaker name for logging/metrics
    };

    struct Metrics {
        State state { State::Closed };
        size_t total_failures { 0 };
        size_t total_successes { 0 };
        size_t consecutive_failures { 0 };
        size_t consecutive_successes { 0 };
        size_t state_changes { 0 };
        UnixDateTime last_failure_time {};
        UnixDateTime last_success_time {};
        UnixDateTime last_state_change {};
        Duration total_open_time {};
    };

    // Create circuit breaker with custom configuration
    explicit CircuitBreaker(Config config);

    // Create circuit breaker with default configuration and name
    explicit CircuitBreaker(StringView name = "unnamed"sv);

    // Execute function with circuit breaker protection
    // Returns Error if circuit is open
    // Otherwise executes function and updates circuit state based on result
    template<typename Func>
    ErrorOr<decltype(declval<Func>()())> execute(Func&& func)
    {
        // Check if we should allow the request
        TRY(check_and_update_state());

        // Execute the function
        auto result = func();

        // Update state based on result
        if (result.is_error()) {
            record_failure();
            return result;
        }

        record_success();
        return result;
    }

    // Manual control methods
    void trip();              // Force circuit open
    void reset();             // Force circuit closed
    void reset_metrics();     // Reset all metrics counters

    // State inspection
    State state() const;
    Metrics get_metrics() const;
    String const& name() const { return m_config.name; }

    // Check if circuit would allow request (without executing)
    bool is_request_allowed() const;

private:
    ErrorOr<void> check_and_update_state();
    void record_success();
    void record_failure();
    void transition_to(State new_state);
    bool should_attempt_reset() const;

    Config m_config;
    State m_state { State::Closed };

    // Counters
    size_t m_consecutive_failures { 0 };
    size_t m_consecutive_successes { 0 };
    size_t m_total_failures { 0 };
    size_t m_total_successes { 0 };
    size_t m_state_changes { 0 };

    // Timestamps
    UnixDateTime m_last_failure_time {};
    UnixDateTime m_last_success_time {};
    UnixDateTime m_state_changed_time {};
    UnixDateTime m_open_state_entered {};

    // Thread safety
    mutable Threading::Mutex m_mutex;
};

// Helper function to create circuit breakers with common configurations
namespace CircuitBreakerPresets {
    // Database operations: 5 failures, 30s timeout, 2 successes to close
    CircuitBreaker database(StringView name = "database"sv);

    // YARA scanning: 3 failures, 60s timeout, 3 successes to close
    CircuitBreaker yara_scanner(StringView name = "yara_scanner"sv);

    // IPC communication: 10 failures, 10s timeout, 1 success to close
    CircuitBreaker ipc_client(StringView name = "ipc_client"sv);

    // External API: 3 failures, 60s timeout, 2 successes to close
    CircuitBreaker external_api(StringView name = "external_api"sv);
}

}
