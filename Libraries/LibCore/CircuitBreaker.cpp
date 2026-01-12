/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/CircuitBreaker.h>
#include <AK/Debug.h>
#include <AK/Format.h>

#ifndef CIRCUIT_BREAKER_DEBUG
#    define CIRCUIT_BREAKER_DEBUG 0
#endif

using AK::Duration;
using AK::String;

namespace Core {

CircuitBreaker::CircuitBreaker(Config config)
    : m_config(move(config))
    , m_state_changed_time(UnixDateTime::now())
{
    dbgln_if(CIRCUIT_BREAKER_DEBUG, "CircuitBreaker: Created '{}' (threshold={}, timeout={}s, success_threshold={})",
        m_config.name, m_config.failure_threshold, m_config.timeout.to_seconds(), m_config.success_threshold);
}

CircuitBreaker::CircuitBreaker(StringView name)
    : CircuitBreaker(Config {
        .failure_threshold = 5,
        .timeout = Duration::from_seconds(30),
        .success_threshold = 2,
        .name = MUST(String::from_utf8(name))
    })
{
}

ErrorOr<void> CircuitBreaker::check_and_update_state()
{
    Threading::MutexLocker locker(m_mutex);

    // If circuit is open, check if we should transition to half-open
    if (m_state == State::Open) {
        if (should_attempt_reset()) {
            transition_to(State::HalfOpen);
        } else {
            return Error::from_string_literal("Circuit breaker is open");
        }
    }

    // If half-open, only allow one request at a time (already in progress check would go here)
    // For simplicity, we allow the request and rely on the caller to handle concurrency

    return {};
}

void CircuitBreaker::record_success()
{
    Threading::MutexLocker locker(m_mutex);

    m_total_successes++;
    m_consecutive_successes++;
    m_consecutive_failures = 0;
    m_last_success_time = UnixDateTime::now();

    dbgln_if(CIRCUIT_BREAKER_DEBUG, "CircuitBreaker: '{}' recorded success (consecutive={}, total={})",
        m_config.name, m_consecutive_successes, m_total_successes);

    // If in half-open state and we've had enough successes, close the circuit
    if (m_state == State::HalfOpen && m_consecutive_successes >= m_config.success_threshold) {
        dbgln("CircuitBreaker: '{}' closing after {} consecutive successes",
            m_config.name, m_consecutive_successes);
        transition_to(State::Closed);
    }
}

void CircuitBreaker::record_failure()
{
    Threading::MutexLocker locker(m_mutex);

    m_total_failures++;
    m_consecutive_failures++;
    m_consecutive_successes = 0;
    m_last_failure_time = UnixDateTime::now();

    dbgln_if(CIRCUIT_BREAKER_DEBUG, "CircuitBreaker: '{}' recorded failure (consecutive={}, total={})",
        m_config.name, m_consecutive_failures, m_total_failures);

    // Check if we should trip the circuit
    if (m_state == State::Closed && m_consecutive_failures >= m_config.failure_threshold) {
        dbgln("CircuitBreaker: '{}' opening after {} consecutive failures",
            m_config.name, m_consecutive_failures);
        transition_to(State::Open);
    } else if (m_state == State::HalfOpen) {
        // Any failure in half-open state reopens the circuit
        dbgln("CircuitBreaker: '{}' reopening after failure in half-open state", m_config.name);
        transition_to(State::Open);
    }
}

void CircuitBreaker::transition_to(State new_state)
{
    // Must be called with mutex held
    if (m_state == new_state)
        return;

    auto old_state = m_state;
    m_state = new_state;
    m_state_changes++;
    m_state_changed_time = UnixDateTime::now();

    // Reset consecutive counters on state change
    if (new_state == State::Closed) {
        m_consecutive_failures = 0;
        m_consecutive_successes = 0;
    } else if (new_state == State::Open) {
        m_consecutive_successes = 0;
        m_open_state_entered = UnixDateTime::now();
    } else if (new_state == State::HalfOpen) {
        m_consecutive_successes = 0;
        m_consecutive_failures = 0;
    }

    dbgln("CircuitBreaker: '{}' state changed: {} -> {}", m_config.name,
        old_state == State::Closed ? "Closed" : old_state == State::Open ? "Open" : "HalfOpen",
        new_state == State::Closed ? "Closed" : new_state == State::Open ? "Open" : "HalfOpen");
}

bool CircuitBreaker::should_attempt_reset() const
{
    // Must be called with mutex held
    if (m_state != State::Open)
        return false;

    auto now = UnixDateTime::now();
    auto time_since_open_ms = now.milliseconds_since_epoch() - m_state_changed_time.milliseconds_since_epoch();
    auto time_since_open = Duration::from_milliseconds(time_since_open_ms);

    return time_since_open >= m_config.timeout;
}

void CircuitBreaker::trip()
{
    Threading::MutexLocker locker(m_mutex);
    dbgln("CircuitBreaker: '{}' manually tripped", m_config.name);
    transition_to(State::Open);
}

void CircuitBreaker::reset()
{
    Threading::MutexLocker locker(m_mutex);
    dbgln("CircuitBreaker: '{}' manually reset", m_config.name);
    m_consecutive_failures = 0;
    m_consecutive_successes = 0;
    transition_to(State::Closed);
}

void CircuitBreaker::reset_metrics()
{
    Threading::MutexLocker locker(m_mutex);
    m_total_failures = 0;
    m_total_successes = 0;
    m_consecutive_failures = 0;
    m_consecutive_successes = 0;
    m_state_changes = 0;
    m_last_failure_time = {};
    m_last_success_time = {};
    dbgln_if(CIRCUIT_BREAKER_DEBUG, "CircuitBreaker: '{}' metrics reset", m_config.name);
}

CircuitBreaker::State CircuitBreaker::state() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_state;
}

CircuitBreaker::Metrics CircuitBreaker::get_metrics() const
{
    Threading::MutexLocker locker(m_mutex);

    Duration total_open_time {};
    if (m_state == State::Open && m_open_state_entered.seconds_since_epoch() > 0) {
        auto now = UnixDateTime::now();
        auto duration_ms = now.milliseconds_since_epoch() - m_open_state_entered.milliseconds_since_epoch();
        total_open_time = Duration::from_milliseconds(duration_ms);
    }

    return Metrics {
        .state = m_state,
        .total_failures = m_total_failures,
        .total_successes = m_total_successes,
        .consecutive_failures = m_consecutive_failures,
        .consecutive_successes = m_consecutive_successes,
        .state_changes = m_state_changes,
        .last_failure_time = m_last_failure_time,
        .last_success_time = m_last_success_time,
        .last_state_change = m_state_changed_time,
        .total_open_time = total_open_time,
    };
}

bool CircuitBreaker::is_request_allowed() const
{
    Threading::MutexLocker locker(m_mutex);

    if (m_state == State::Closed || m_state == State::HalfOpen)
        return true;

    // If open, check if enough time has passed to attempt reset
    return should_attempt_reset();
}

// Preset configurations
namespace CircuitBreakerPresets {

CircuitBreaker database(StringView name)
{
    return CircuitBreaker(CircuitBreaker::Config {
        .failure_threshold = 5,
        .timeout = Duration::from_seconds(30),
        .success_threshold = 2,
        .name = MUST(String::from_utf8(name)),
    });
}

CircuitBreaker yara_scanner(StringView name)
{
    return CircuitBreaker(CircuitBreaker::Config {
        .failure_threshold = 3,
        .timeout = Duration::from_seconds(60),
        .success_threshold = 3,
        .name = MUST(String::from_utf8(name)),
    });
}

CircuitBreaker ipc_client(StringView name)
{
    return CircuitBreaker(CircuitBreaker::Config {
        .failure_threshold = 10,
        .timeout = Duration::from_seconds(10),
        .success_threshold = 1,
        .name = MUST(String::from_utf8(name)),
    });
}

CircuitBreaker external_api(StringView name)
{
    return CircuitBreaker(CircuitBreaker::Config {
        .failure_threshold = 3,
        .timeout = Duration::from_seconds(60),
        .success_threshold = 2,
        .name = MUST(String::from_utf8(name)),
    });
}

}

}
