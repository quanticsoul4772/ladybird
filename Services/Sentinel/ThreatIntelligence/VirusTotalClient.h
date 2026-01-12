/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "RateLimiter.h"
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel {
    class PolicyGraph;  // Forward declaration for cache integration
}

namespace Sentinel::ThreatIntelligence {

// Vendor verdict from single antivirus engine
struct VendorVerdict {
    String vendor_name;           // e.g., "Kaspersky", "Microsoft"
    String category;              // e.g., "malicious", "suspicious", "undetected"
    String result;                // e.g., "Trojan.Win32.Generic"
};

// VirusTotal API response result
struct VirusTotalResult {
    // Detection counts
    u32 malicious_count { 0 };
    u32 suspicious_count { 0 };
    u32 undetected_count { 0 };
    u32 harmless_count { 0 };
    u32 timeout_count { 0 };
    u32 total_engines { 0 };

    // Calculated metrics
    float detection_ratio { 0.0f };     // malicious / total
    float threat_score { 0.0f };        // (malicious + suspicious*0.5) / total

    // Vendor details
    Vector<VendorVerdict> vendor_verdicts;

    // Metadata
    String scan_date;
    String resource_id;                  // SHA256/URL/domain queried
    bool is_cached { false };            // From PolicyGraph cache?

    // Helper methods
    bool is_malicious() const { return detection_ratio >= 0.1f; }  // 10% threshold
    bool is_suspicious() const { return threat_score >= 0.05f && !is_malicious(); }
    bool is_clean() const { return !is_malicious() && !is_suspicious(); }
};

// VirusTotal API client with rate limiting and caching
// API v3: https://developers.virustotal.com/reference/overview
class VirusTotalClient {
public:
    // Create client with API key
    // Rate limit: 4 requests/minute for free tier (configurable)
    static ErrorOr<NonnullOwnPtr<VirusTotalClient>> create(
        String const& api_key,
        u32 requests_per_minute = 4);

    // Create client with API key and PolicyGraph for caching
    static ErrorOr<NonnullOwnPtr<VirusTotalClient>> create_with_cache(
        String const& api_key,
        PolicyGraph* policy_graph,
        u32 requests_per_minute = 4);

    ~VirusTotalClient();

    // File hash lookup (SHA256)
    // GET /api/v3/files/{sha256}
    ErrorOr<VirusTotalResult> lookup_file_hash(String const& sha256);

    // URL reputation check
    // GET /api/v3/urls/{url_id}
    ErrorOr<VirusTotalResult> lookup_url(String const& url);

    // Domain reputation check
    // GET /api/v3/domains/{domain}
    ErrorOr<VirusTotalResult> lookup_domain(String const& domain);

    // Configuration
    void set_timeout(Duration timeout) { m_timeout = timeout; }
    Duration timeout() const { return m_timeout; }

    void enable_cache(bool enabled) { m_cache_enabled = enabled; }
    bool is_cache_enabled() const { return m_cache_enabled; }

    // Statistics
    struct Statistics {
        u64 total_lookups { 0 };
        u64 cache_hits { 0 };
        u64 cache_misses { 0 };
        u64 api_calls { 0 };
        u64 rate_limited { 0 };
        u64 api_errors { 0 };
        u64 malicious_results { 0 };
        u64 suspicious_results { 0 };
        u64 clean_results { 0 };
        float average_detection_ratio { 0.0f };
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

private:
    VirusTotalClient(
        String const& api_key,
        PolicyGraph* policy_graph,
        NonnullOwnPtr<RateLimiter> rate_limiter);

    // HTTP request helpers
    ErrorOr<String> make_api_request(String const& endpoint);
    ErrorOr<VirusTotalResult> parse_file_response(String const& json_response, String const& resource_id);
    ErrorOr<VirusTotalResult> parse_url_response(String const& json_response, String const& resource_id);
    ErrorOr<VirusTotalResult> parse_domain_response(String const& json_response, String const& resource_id);

    // Cache helpers (PolicyGraph integration)
    ErrorOr<Optional<VirusTotalResult>> get_cached_result(String const& resource_id);
    ErrorOr<void> cache_result(String const& resource_id, VirusTotalResult const& result);

    // Configuration
    String m_api_key;
    PolicyGraph* m_policy_graph { nullptr };
    NonnullOwnPtr<RateLimiter> m_rate_limiter;
    Duration m_timeout { Duration::from_seconds(30) };
    bool m_cache_enabled { true };

    // Statistics
    mutable Statistics m_stats;

    // Constants
    static constexpr StringView VT_API_BASE = "https://www.virustotal.com/api/v3"sv;
    static constexpr u32 CACHE_TTL_DAYS_CLEAN = 7;
    static constexpr u32 CACHE_TTL_DAYS_MALICIOUS = 30;
};

}
