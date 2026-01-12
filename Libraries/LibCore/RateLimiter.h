/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Time.h>
#include <LibThreading/Mutex.h>

namespace Core {

// Token bucket algorithm for rate limiting
// Thread-safe implementation using mutex protection
class TokenBucketRateLimiter {
public:
    // Create a rate limiter with capacity and refill rate
    // capacity: Maximum number of tokens in the bucket (burst size)
    // refill_rate_per_second: How many tokens to add per second
    TokenBucketRateLimiter(size_t capacity, size_t refill_rate_per_second);

    // Try to consume tokens, returns true if allowed
    // tokens: Number of tokens to consume (default 1)
    // Thread-safe: Can be called from multiple threads
    bool try_consume(size_t tokens = 1);

    // Check if request would be allowed without consuming tokens
    // tokens: Number of tokens to check (default 1)
    // Thread-safe: Can be called from multiple threads
    bool would_allow(size_t tokens = 1) const;

    // Get current token count (may be approximate due to refill timing)
    // Thread-safe: Can be called from multiple threads
    size_t available_tokens() const;

    // Reset bucket to full capacity (for testing or admin override)
    // Thread-safe: Can be called from multiple threads
    void reset();

    // Get configuration
    size_t capacity() const { return m_capacity; }
    size_t refill_rate() const { return m_refill_rate; }

private:
    // Refill tokens based on time elapsed since last refill
    // Must be called with mutex held
    void refill_tokens_locked();

    size_t m_capacity;
    size_t m_tokens;
    size_t m_refill_rate;
    UnixDateTime m_last_refill;

    // Mutex for thread safety
    mutable Threading::Mutex m_mutex;
};

}
