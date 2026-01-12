/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Math.h>
#include <AK/Random.h>
#include <AK/String.h>
#include <LibCore/RetryPolicy.h>
#include <errno.h>
#include <netdb.h>

using AK::Duration;

namespace Core {

RetryPolicy::RetryPolicy(
    size_t max_attempts,
    Duration initial_delay,
    Duration max_delay,
    double backoff_multiplier,
    double jitter_factor)
    : m_max_attempts(max(max_attempts, 1uz))  // Ensure at least 1 attempt
    , m_initial_delay(initial_delay)
    , m_max_delay(max_delay)
    , m_backoff_multiplier(backoff_multiplier)
    , m_jitter_factor(jitter_factor)
{
    VERIFY(backoff_multiplier >= 1.0);
    VERIFY(jitter_factor >= 0.0 && jitter_factor <= 1.0);
    VERIFY(initial_delay <= max_delay);
}

void RetryPolicy::set_retry_predicate(RetryPredicate predicate)
{
    m_retry_predicate = move(predicate);
}

bool RetryPolicy::should_retry_error(Error const& error) const
{
    // If a custom predicate is set, use it
    if (m_retry_predicate.has_value())
        return m_retry_predicate.value()(error);

    // By default, retry all errors
    return true;
}

Duration RetryPolicy::calculate_next_delay(size_t attempt) const
{
    // Calculate base delay: initial_delay * (backoff_multiplier ^ attempt)
    // For example, with initial_delay=100ms and multiplier=2.0:
    // Attempt 0 (first retry):  100ms * 2^0 = 100ms
    // Attempt 1 (second retry): 100ms * 2^1 = 200ms
    // Attempt 2 (third retry):  100ms * 2^2 = 400ms

    double initial_ms = static_cast<double>(m_initial_delay.to_milliseconds());
    double multiplier = AK::pow(m_backoff_multiplier, static_cast<double>(attempt));
    double base_delay_ms = initial_ms * multiplier;

    // Cap at max_delay
    i64 max_delay_ms = m_max_delay.to_milliseconds();
    if (base_delay_ms > static_cast<double>(max_delay_ms))
        base_delay_ms = static_cast<double>(max_delay_ms);

    // Add jitter to prevent thundering herd
    // Jitter multiplier is in range [1-jitter_factor, 1+jitter_factor]
    // For example, with jitter_factor=0.1: multiplier is in [0.9, 1.1]
    double jitter_multiplier = generate_jitter();
    double final_delay_ms = base_delay_ms * jitter_multiplier;

    // Ensure we don't exceed max_delay after jitter
    if (final_delay_ms > static_cast<double>(max_delay_ms))
        final_delay_ms = static_cast<double>(max_delay_ms);

    // Convert back to Duration
    return Duration::from_milliseconds(static_cast<i64>(final_delay_ms));
}

double RetryPolicy::generate_jitter() const
{
    if (m_jitter_factor == 0.0)
        return 1.0;

    // Generate random value in range [-jitter_factor, +jitter_factor]
    // For example, with jitter_factor=0.1, range is [-0.1, +0.1]
    // Then add 1.0 to get multiplier in range [0.9, 1.1]

    // get_random_uniform(n) returns value in [0, n), scale to [0, 1)
    double random_value = static_cast<double>(get_random_uniform(1000000)) / 1000000.0;

    // Scale to [-jitter_factor, +jitter_factor]
    double jitter = (random_value * 2.0 - 1.0) * m_jitter_factor;

    return 1.0 + jitter;
}

void RetryPolicy::reset_metrics()
{
    m_metrics = Metrics {};
}

// Default retry predicates for common error scenarios

RetryPolicy::RetryPredicate RetryPolicy::database_retry_predicate()
{
    return [](Error const& error) -> bool {
        // Retry on errno-based errors that are transient
        if (!error.is_errno())
            return false;

        int code = error.code();

        // Connection errors (transient)
        if (code == ECONNREFUSED || code == ECONNRESET || code == ECONNABORTED)
            return true;

        // Network errors (transient)
        if (code == ENETDOWN || code == ENETUNREACH || code == EHOSTDOWN || code == EHOSTUNREACH)
            return true;

        // Timeout (transient)
        if (code == ETIMEDOUT)
            return true;

        // Resource temporarily unavailable (transient)
#if EAGAIN == EWOULDBLOCK
        if (code == EAGAIN)
#else
        if (code == EAGAIN || code == EWOULDBLOCK)
#endif
            return true;

        // Interrupted system call (transient)
        if (code == EINTR)
            return true;

        // Database-specific: busy or locked (transient)
        // Note: SQLite SQLITE_BUSY is typically mapped to EBUSY
        if (code == EBUSY)
            return true;

        // Do NOT retry:
        // - EACCES (permission denied) - permanent
        // - ENOENT (file not found) - permanent
        // - EINVAL (invalid argument) - permanent
        // - ENOSPC (no space left) - permanent
        // - EIO (I/O error) - potentially permanent
        // - Constraint violations - permanent
        // - Syntax errors - permanent

        return false;
    };
}

RetryPolicy::RetryPredicate RetryPolicy::file_io_retry_predicate()
{
    return [](Error const& error) -> bool {
        if (!error.is_errno())
            return false;

        int code = error.code();

        // Resource temporarily unavailable (transient)
#if EAGAIN == EWOULDBLOCK
        if (code == EAGAIN)
#else
        if (code == EAGAIN || code == EWOULDBLOCK)
#endif
            return true;

        // Resource busy (transient - file locked by another process)
        if (code == EBUSY)
            return true;

        // Interrupted system call (transient)
        if (code == EINTR)
            return true;

        // Text file busy (transient - executable being modified)
        if (code == ETXTBSY)
            return true;

        // Do NOT retry:
        // - ENOENT (file not found) - permanent
        // - EACCES (permission denied) - permanent
        // - ENOSPC (no space left) - permanent
        // - EROFS (read-only filesystem) - permanent
        // - EISDIR (is a directory) - permanent
        // - ENOTDIR (not a directory) - permanent
        // - ENAMETOOLONG (name too long) - permanent
        // - ELOOP (too many symbolic links) - permanent

        return false;
    };
}

RetryPolicy::RetryPredicate RetryPolicy::ipc_retry_predicate()
{
    return [](Error const& error) -> bool {
        if (!error.is_errno())
            return false;

        int code = error.code();

        // Connection errors (transient)
        if (code == ECONNREFUSED || code == ECONNRESET || code == ECONNABORTED)
            return true;

        // Network errors (transient)
        if (code == ENETDOWN || code == ENETUNREACH)
            return true;

        // Timeout (transient)
        if (code == ETIMEDOUT)
            return true;

        // Resource temporarily unavailable (transient)
#if EAGAIN == EWOULDBLOCK
        if (code == EAGAIN)
#else
        if (code == EAGAIN || code == EWOULDBLOCK)
#endif
            return true;

        // Interrupted system call (transient)
        if (code == EINTR)
            return true;

        // Pipe/socket errors (transient)
        if (code == EPIPE)
            return true;

        // Do NOT retry:
        // - EPROTO (protocol error) - permanent
        // - EPROTOTYPE (wrong protocol type) - permanent
        // - EAFNOSUPPORT (address family not supported) - permanent
        // - EADDRINUSE (address already in use) - permanent (until released)
        // - EINVAL (invalid argument) - permanent

        return false;
    };
}

RetryPolicy::RetryPredicate RetryPolicy::network_retry_predicate()
{
    return [](Error const& error) -> bool {
        if (!error.is_errno())
            return false;

        int code = error.code();

        // Connection errors (transient)
        if (code == ECONNREFUSED || code == ECONNRESET || code == ECONNABORTED)
            return true;

        // Network errors (transient)
        if (code == ENETDOWN || code == ENETUNREACH || code == EHOSTDOWN || code == EHOSTUNREACH)
            return true;

        // Timeout (transient)
        if (code == ETIMEDOUT)
            return true;

        // Resource temporarily unavailable (transient)
#if EAGAIN == EWOULDBLOCK
        if (code == EAGAIN)
#else
        if (code == EAGAIN || code == EWOULDBLOCK)
#endif
            return true;

        // Interrupted system call (transient)
        if (code == EINTR)
            return true;

        // DNS resolution errors (transient)
        if (code == EAI_AGAIN || code == EAI_NONAME)
            return true;

        // Temporary failure in name resolution (transient)
#ifdef EAI_ADDRFAMILY
        if (code == EAI_ADDRFAMILY)
            return true;
#endif

        // Do NOT retry:
        // - EPROTO (protocol error) - permanent
        // - EPROTOTYPE (wrong protocol type) - permanent
        // - EAFNOSUPPORT (address family not supported) - permanent
        // - HTTP 4xx errors - permanent (client error)
        // - DNS NXDOMAIN - permanent (domain doesn't exist)

        return false;
    };
}

}
