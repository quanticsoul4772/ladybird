/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "RateLimiter.h"
#include <AK/Time.h>
#include <LibCore/System.h>

namespace Sentinel::ThreatIntelligence {

ErrorOr<NonnullOwnPtr<RateLimiter>> RateLimiter::create(
    u32 max_requests,
    Duration time_window)
{
    if (max_requests == 0)
        return Error::from_string_literal("RateLimiter: max_requests must be > 0");

    if (time_window.is_zero())
        return Error::from_string_literal("RateLimiter: time_window must be > 0");

    return adopt_nonnull_own_or_enomem(new (nothrow) RateLimiter(max_requests, time_window));
}

RateLimiter::RateLimiter(u32 max_requests, Duration time_window)
    : m_max_requests(max_requests)
    , m_time_window(time_window)
    , m_current_tokens(max_requests)
    , m_last_refill(UnixDateTime::now())
{
    m_stats.current_tokens = max_requests;
    m_stats.last_refill = m_last_refill;
}

RateLimiter::~RateLimiter() = default;

void RateLimiter::refill_tokens()
{
    auto now = UnixDateTime::now();
    auto elapsed = Duration::from_milliseconds(
        (now.milliseconds_since_epoch() - m_last_refill.milliseconds_since_epoch()));

    // Calculate tokens to add based on time elapsed
    // tokens_to_add = (elapsed / time_window) * max_requests
    if (elapsed >= m_time_window) {
        // Full refill if time window has passed
        m_current_tokens = m_max_requests;
        m_last_refill = now;
        m_stats.current_tokens = m_current_tokens;
        m_stats.last_refill = m_last_refill;
    } else {
        // Partial refill based on time elapsed
        // Example: If 50% of time_window elapsed, refill 50% of max_requests
        double time_fraction = static_cast<double>(elapsed.to_milliseconds()) /
            static_cast<double>(m_time_window.to_milliseconds());
        u32 tokens_to_add = static_cast<u32>(time_fraction * m_max_requests);

        if (tokens_to_add > 0) {
            m_current_tokens = AK::min(m_current_tokens + tokens_to_add, m_max_requests);
            m_last_refill = now;
            m_stats.current_tokens = m_current_tokens;
            m_stats.last_refill = m_last_refill;
        }
    }
}

bool RateLimiter::try_acquire()
{
    refill_tokens();

    m_stats.total_requests++;

    if (m_current_tokens > 0) {
        m_current_tokens--;
        m_stats.current_tokens = m_current_tokens;
        m_stats.allowed_requests++;
        return true;
    }

    m_stats.denied_requests++;
    return false;
}

ErrorOr<void> RateLimiter::acquire_blocking()
{
    while (!try_acquire()) {
        auto wait_time = time_until_next_token();

        // Sleep until next token is available
        // Use LibCore's sleep_ms which accepts milliseconds
        TRY(Core::System::sleep_ms(static_cast<u32>(wait_time.to_milliseconds())));
    }

    return {};
}

Duration RateLimiter::time_until_next_token() const
{
    if (m_current_tokens > 0)
        return Duration::from_seconds(0);

    auto now = UnixDateTime::now();
    auto elapsed = Duration::from_milliseconds(
        now.milliseconds_since_epoch() - m_last_refill.milliseconds_since_epoch());

    // Time remaining until one token is available
    auto time_per_token = Duration::from_milliseconds(
        m_time_window.to_milliseconds() / m_max_requests);

    if (elapsed >= time_per_token)
        return Duration::from_seconds(0);

    return Duration::from_milliseconds(
        time_per_token.to_milliseconds() - elapsed.to_milliseconds());
}

void RateLimiter::reset_statistics()
{
    m_stats.total_requests = 0;
    m_stats.allowed_requests = 0;
    m_stats.denied_requests = 0;
    // Don't reset current_tokens or last_refill - those are state, not stats
}

}
