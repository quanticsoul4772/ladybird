/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClientRateLimiter.h"

namespace Sentinel {

ClientRateLimiter::ClientRateLimiter(ClientLimits limits)
    : m_limits(limits)
{
}

Core::TokenBucketRateLimiter& ClientRateLimiter::get_scan_limiter(int client_id)
{
    // Must be called with mutex held
    auto it = m_scan_limiters.find(client_id);
    if (it == m_scan_limiters.end()) {
        // Create new rate limiter for this client
        auto limiter = adopt_own(*new Core::TokenBucketRateLimiter(
            m_limits.scan_burst_capacity,
            m_limits.scan_requests_per_second));
        m_scan_limiters.set(client_id, move(limiter));
        return *m_scan_limiters.find(client_id)->value;
    }
    return *it->value;
}

Core::TokenBucketRateLimiter& ClientRateLimiter::get_policy_limiter(int client_id)
{
    // Must be called with mutex held
    auto it = m_policy_limiters.find(client_id);
    if (it == m_policy_limiters.end()) {
        // Create new rate limiter for this client
        auto limiter = adopt_own(*new Core::TokenBucketRateLimiter(
            m_limits.policy_burst_capacity,
            m_limits.policy_queries_per_second));
        m_policy_limiters.set(client_id, move(limiter));
        return *m_policy_limiters.find(client_id)->value;
    }
    return *it->value;
}

ErrorOr<void> ClientRateLimiter::check_scan_request(int client_id)
{
    Threading::MutexLocker locker(m_mutex);

    auto& limiter = get_scan_limiter(client_id);

    if (!limiter.try_consume(1)) {
        // Rate limit exceeded
        m_rejected_counts.ensure(client_id) += 1;
        m_total_rejected++;

        return Error::from_string_literal("Rate limit exceeded for scan requests");
    }

    return {};
}

ErrorOr<void> ClientRateLimiter::check_policy_query(int client_id)
{
    Threading::MutexLocker locker(m_mutex);

    auto& limiter = get_policy_limiter(client_id);

    if (!limiter.try_consume(1)) {
        // Rate limit exceeded
        m_rejected_counts.ensure(client_id) += 1;
        m_total_rejected++;

        return Error::from_string_literal("Rate limit exceeded for policy queries");
    }

    return {};
}

ErrorOr<void> ClientRateLimiter::check_concurrent_scans(int client_id)
{
    Threading::MutexLocker locker(m_mutex);

    auto current_scans = m_concurrent_scans.get(client_id).value_or(0);

    if (current_scans >= m_limits.max_concurrent_scans) {
        // Concurrent scan limit exceeded
        m_rejected_counts.ensure(client_id) += 1;
        m_total_rejected++;

        return Error::from_string_literal("Concurrent scan limit exceeded");
    }

    // Increment concurrent scan counter
    m_concurrent_scans.set(client_id, current_scans + 1);
    return {};
}

void ClientRateLimiter::release_scan_slot(int client_id)
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_concurrent_scans.find(client_id);
    if (it != m_concurrent_scans.end() && it->value > 0) {
        it->value--;
    }
}

size_t ClientRateLimiter::get_total_rejected() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_total_rejected;
}

HashMap<int, size_t> ClientRateLimiter::get_per_client_rejected() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_rejected_counts;
}

HashMap<int, size_t> ClientRateLimiter::get_concurrent_scans() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_concurrent_scans;
}

void ClientRateLimiter::reset_telemetry()
{
    Threading::MutexLocker locker(m_mutex);
    m_rejected_counts.clear();
    m_total_rejected = 0;
}

void ClientRateLimiter::reset_client(int client_id)
{
    Threading::MutexLocker locker(m_mutex);

    // Reset scan rate limiter
    auto scan_it = m_scan_limiters.find(client_id);
    if (scan_it != m_scan_limiters.end()) {
        scan_it->value->reset();
    }

    // Reset policy rate limiter
    auto policy_it = m_policy_limiters.find(client_id);
    if (policy_it != m_policy_limiters.end()) {
        policy_it->value->reset();
    }

    // Reset concurrent scans
    m_concurrent_scans.remove(client_id);

    // Reset telemetry for this client
    m_rejected_counts.remove(client_id);
}

}
