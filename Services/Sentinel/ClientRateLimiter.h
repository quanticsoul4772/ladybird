/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <LibCore/RateLimiter.h>
#include <LibThreading/Mutex.h>

namespace Sentinel {

// Configuration for per-client rate limits
struct ClientLimits {
    // Scan requests per second (e.g., scan_file, scan_content)
    size_t scan_requests_per_second { 10 };

    // Policy queries per second (future use)
    size_t policy_queries_per_second { 100 };

    // Maximum concurrent scans per client
    size_t max_concurrent_scans { 5 };

    // Burst capacity (allow bursts up to this many requests)
    size_t scan_burst_capacity { 20 };
    size_t policy_burst_capacity { 200 };
};

// Per-client rate limiter using token bucket algorithm
// Isolates rate limits between clients to prevent one client from starving others
// Thread-safe for concurrent access
class ClientRateLimiter {
public:
    explicit ClientRateLimiter(ClientLimits limits = {});

    // Check if scan request is allowed for this client
    // Returns error if rate limit exceeded
    // Thread-safe: Can be called from multiple threads
    ErrorOr<void> check_scan_request(int client_id);

    // Check if policy query is allowed for this client (future use)
    // Returns error if rate limit exceeded
    // Thread-safe: Can be called from multiple threads
    ErrorOr<void> check_policy_query(int client_id);

    // Check if concurrent scan limit is exceeded for this client
    // Returns error if limit exceeded
    // Must call release_scan_slot() when scan completes
    // Thread-safe: Can be called from multiple threads
    ErrorOr<void> check_concurrent_scans(int client_id);

    // Release a concurrent scan slot for this client
    // Thread-safe: Can be called from multiple threads
    void release_scan_slot(int client_id);

    // Get telemetry: Total requests rejected across all clients
    size_t get_total_rejected() const;

    // Get telemetry: Rejected requests per client
    HashMap<int, size_t> get_per_client_rejected() const;

    // Get telemetry: Current concurrent scans per client
    HashMap<int, size_t> get_concurrent_scans() const;

    // Reset telemetry (for testing)
    void reset_telemetry();

    // Reset a specific client's rate limiter (for testing or admin override)
    void reset_client(int client_id);

private:
    // Get or create scan rate limiter for client
    Core::TokenBucketRateLimiter& get_scan_limiter(int client_id);

    // Get or create policy rate limiter for client
    Core::TokenBucketRateLimiter& get_policy_limiter(int client_id);

    ClientLimits m_limits;

    // Per-client rate limiters
    HashMap<int, Core::TokenBucketRateLimiter> m_scan_limiters;
    HashMap<int, Core::TokenBucketRateLimiter> m_policy_limiters;

    // Per-client concurrent scan tracking
    HashMap<int, size_t> m_concurrent_scans;

    // Telemetry: Total rejected requests per client
    HashMap<int, size_t> m_rejected_counts;
    size_t m_total_rejected { 0 };

    // Mutex for thread safety
    mutable Threading::Mutex m_mutex;
};

}
