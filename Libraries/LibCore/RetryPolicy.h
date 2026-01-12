/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/Time.h>
#include <AK/Types.h>

namespace Core {

using AK::Duration;

// Retry policy for handling transient failures with exponential backoff
//
// Features:
// - Configurable retry attempts (default 3)
// - Exponential backoff (100ms, 200ms, 400ms, ...)
// - Jitter to prevent thundering herd
// - Retry predicate (which errors are retryable)
// - Metrics tracking (total attempts, successes, final failures)
//
// Example usage:
//   RetryPolicy policy(3, Duration::from_milliseconds(100));
//   auto result = TRY(policy.execute([&] {
//       return database->query("SELECT * FROM policies");
//   }));
class RetryPolicy {
public:
    // Predicate to determine if an error is retryable
    using RetryPredicate = Function<bool(Error const&)>;

    // Metrics tracking for monitoring
    struct Metrics {
        size_t total_executions { 0 };       // Total execute() calls
        size_t total_attempts { 0 };         // Total attempts across all executions
        size_t successful_executions { 0 };  // Executions that eventually succeeded
        size_t failed_executions { 0 };      // Executions that failed after all retries
        size_t retried_executions { 0 };     // Executions that needed at least one retry
        UnixDateTime last_execution;
        UnixDateTime last_success;
        UnixDateTime last_failure;
    };

    // Construct a retry policy
    // max_attempts: Maximum number of attempts (including initial try), minimum 1
    // initial_delay: Delay before first retry (e.g., 100ms)
    // max_delay: Maximum delay between retries (e.g., 10 seconds)
    // backoff_multiplier: Multiplier for exponential backoff (default 2.0 = doubles each time)
    // jitter_factor: Random jitter as fraction of delay (0.1 = ±10% randomness)
    RetryPolicy(
        size_t max_attempts = 3,
        Duration initial_delay = Duration::from_milliseconds(100),
        Duration max_delay = Duration::from_seconds(10),
        double backoff_multiplier = 2.0,
        double jitter_factor = 0.1);

    // Execute a function with retry logic
    // The function should return ErrorOr<T> - if it returns an error and the error
    // is retryable (according to the retry predicate), it will be retried with
    // exponential backoff
    template<typename Func>
    ErrorOr<decltype(declval<Func>()())> execute(Func&& func);

    // Set a custom retry predicate to determine which errors should be retried
    // If not set, all errors are considered retryable by default
    void set_retry_predicate(RetryPredicate predicate);

    // Check if an error should be retried (uses the retry predicate)
    bool should_retry_error(Error const& error) const;

    // Calculate the delay for the next retry attempt (with exponential backoff and jitter)
    // attempt: 0-based attempt number (0 = first retry)
    Duration calculate_next_delay(size_t attempt) const;

    // Reset metrics to zero
    void reset_metrics();

    // Get current metrics
    Metrics const& metrics() const { return m_metrics; }

    // Get configuration
    size_t max_attempts() const { return m_max_attempts; }
    Duration initial_delay() const { return m_initial_delay; }
    Duration max_delay() const { return m_max_delay; }
    double backoff_multiplier() const { return m_backoff_multiplier; }
    double jitter_factor() const { return m_jitter_factor; }

    // Default retry predicates for common error scenarios
    static RetryPredicate database_retry_predicate();    // Connection errors, lock timeouts
    static RetryPredicate file_io_retry_predicate();     // EAGAIN, EBUSY
    static RetryPredicate ipc_retry_predicate();         // Connection refused, timeout
    static RetryPredicate network_retry_predicate();     // Connection errors, timeouts

private:
    size_t m_max_attempts;
    Duration m_initial_delay;
    Duration m_max_delay;
    double m_backoff_multiplier;
    double m_jitter_factor;

    Optional<RetryPredicate> m_retry_predicate;
    Metrics m_metrics;

    // Generate random jitter multiplier (e.g., for jitter_factor=0.1, returns 0.9-1.1)
    double generate_jitter() const;
};

// Template implementation must be in header
template<typename Func>
ErrorOr<decltype(declval<Func>()())> RetryPolicy::execute(Func&& func)
{
    m_metrics.total_executions++;
    m_metrics.last_execution = UnixDateTime::now();

    Error last_error = Error::from_string_literal("RetryPolicy: No attempts made");
    bool needed_retry = false;

    for (size_t attempt = 0; attempt < m_max_attempts; ++attempt) {
        m_metrics.total_attempts++;

        // Try to execute the function
        auto result = func();

        if (!result.is_error()) {
            // Success!
            m_metrics.successful_executions++;
            m_metrics.last_success = UnixDateTime::now();
            if (needed_retry)
                m_metrics.retried_executions++;
            return result.release_value();
        }

        // Got an error
        last_error = result.release_error();

        // Check if this is the last attempt
        if (attempt + 1 >= m_max_attempts) {
            break;
        }

        // Check if we should retry this error
        if (!should_retry_error(last_error)) {
            break;
        }

        needed_retry = true;

        // Calculate delay and sleep before retrying
        auto delay = calculate_next_delay(attempt);

        // Sleep for the calculated delay
        // NOTE: Using nanosleep for cross-platform compatibility
        auto timespec = delay.to_timespec();
        nanosleep(&timespec, nullptr);
    }

    // All retries exhausted or non-retryable error
    m_metrics.failed_executions++;
    m_metrics.last_failure = UnixDateTime::now();

    return last_error;
}

}
