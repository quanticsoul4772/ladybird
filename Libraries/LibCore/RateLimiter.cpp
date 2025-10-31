/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/RateLimiter.h>

namespace Core {

TokenBucketRateLimiter::TokenBucketRateLimiter(size_t capacity, size_t refill_rate_per_second)
    : m_capacity(capacity)
    , m_tokens(capacity)
    , m_refill_rate(refill_rate_per_second)
    , m_last_refill(UnixDateTime::now())
{
}

void TokenBucketRateLimiter::refill_tokens_locked()
{
    auto now = UnixDateTime::now();
    auto elapsed = now - m_last_refill;

    if (elapsed.to_seconds() <= 0)
        return;

    auto tokens_to_add = static_cast<size_t>(elapsed.to_seconds() * m_refill_rate);

    if (tokens_to_add > 0) {
        m_tokens = min(m_capacity, m_tokens + tokens_to_add);
        m_last_refill = now;
    }
}

bool TokenBucketRateLimiter::try_consume(size_t tokens)
{
    Threading::MutexLocker locker(m_mutex);
    refill_tokens_locked();
    if (m_tokens >= tokens) {
        m_tokens -= tokens;
        return true;
    }
    return false;
}

bool TokenBucketRateLimiter::would_allow(size_t tokens) const
{
    Threading::MutexLocker locker(m_mutex);
    return m_tokens >= tokens;
}

size_t TokenBucketRateLimiter::available_tokens() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_tokens;
}

void TokenBucketRateLimiter::reset()
{
    Threading::MutexLocker locker(m_mutex);
    m_tokens = m_capacity;
    m_last_refill = UnixDateTime::now();
}

}
