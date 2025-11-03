/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Time.h>

namespace Sentinel::ThreatIntelligence {

using AK::Duration;

// Token bucket rate limiter for API calls
// Implements configurable rate limiting with token refill
class RateLimiter {
public:
    // Create rate limiter with max_requests per time_window
    // Example: RateLimiter(4, Duration::from_minutes(1)) = 4 requests/minute
    static ErrorOr<NonnullOwnPtr<RateLimiter>> create(
        u32 max_requests,
        Duration time_window);

    ~RateLimiter();

    // Attempt to acquire a token for one API call
    // Returns true if token acquired, false if rate limit exceeded
    bool try_acquire();

    // Wait until a token is available (blocking)
    // Returns error if interrupted or invalid state
    ErrorOr<void> acquire_blocking();

    // Get time until next token is available
    Duration time_until_next_token() const;

    // Statistics
    struct Statistics {
        u64 total_requests { 0 };
        u64 allowed_requests { 0 };
        u64 denied_requests { 0 };
        u32 current_tokens { 0 };
        UnixDateTime last_refill;
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

    // Configuration access
    u32 max_requests() const { return m_max_requests; }
    Duration time_window() const { return m_time_window; }

private:
    RateLimiter(u32 max_requests, Duration time_window);

    void refill_tokens();

    u32 m_max_requests;
    Duration m_time_window;
    u32 m_current_tokens;
    UnixDateTime m_last_refill;
    mutable Statistics m_stats;
};

}
