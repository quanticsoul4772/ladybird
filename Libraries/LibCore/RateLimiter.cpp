/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/RateLimiter.h>

namespace Core {

TokenBucketRateLimiter::TokenBucketRateLimiter(size_t capacity, size_t refill_rate_per_second)
    : m_capacity(capacity)
    , m_tokens(capacity) // Start with full bucket
    , m_refill_rate(refill_rate_per_second)
    , m_last_refill(UnixDateTime::now())
{
}

void TokenBucketRateLimiter::refill_tokens_locked()
{
    auto now = UnixDateTime::now();
    auto elapsed = now.seconds_since_epoch() - m_last_refill.seconds_since_epoch();

    if (elapsed <= 0)
        return;

    // Calculate tokens to add based on elapsed time
    // tokens_to_add = elapsed_seconds * refill_rate
    size_t tokens_to_add = static_cast<size_t>(elapsed) * m_refill_rate;

    if (tokens_to_add > 0) {
        m_tokens = min(m_capacity, m_tokens + tokens_to_add);
        m_last_refill = now;
    }
}

bool TokenBucketRateLimiter::try_consume(size_t tokens)
{
    Threading::MutexLocker locker(m_mutex);

    // Refill tokens based on time elapsed
    refill_tokens_locked();

    // Check if we have enough tokens
    if (m_tokens >= tokens) {
        m_tokens -= tokens;
        return true; // Allowed
    }

    return false; // Rate limit exceeded
}

bool TokenBucketRateLimiter::would_allow(size_t tokens) const
{
    Threading::MutexLocker locker(m_mutex);

    // Temporarily refill to check (without modifying state)
    auto now = UnixDateTime::now();
    auto elapsed = now.seconds_since_epoch() - m_last_refill.seconds_since_epoch();

    size_t tokens_after_refill = m_tokens;
    if (elapsed > 0) {
        size_t tokens_to_add = static_cast<size_t>(elapsed) * m_refill_rate;
        tokens_after_refill = min(m_capacity, m_tokens + tokens_to_add);
    }

    return tokens_after_refill >= tokens;
}

size_t TokenBucketRateLimiter::available_tokens() const
{
    Threading::MutexLocker locker(m_mutex);

    // Return tokens after potential refill
    auto now = UnixDateTime::now();
    auto elapsed = now.seconds_since_epoch() - m_last_refill.seconds_since_epoch();

    if (elapsed > 0) {
        size_t tokens_to_add = static_cast<size_t>(elapsed) * m_refill_rate;
        return min(m_capacity, m_tokens + tokens_to_add);
    }

    return m_tokens;
}

void TokenBucketRateLimiter::reset()
{
    Threading::MutexLocker locker(m_mutex);
    m_tokens = m_capacity;
    m_last_refill = UnixDateTime::now();
}

}
