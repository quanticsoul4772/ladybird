/*
 * Copyright (c) 2026, quanticsoul4772
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <LibURL/URL.h>

namespace WebContent {

// Threat levels mirroring Sentinel's scoring thresholds.
// Defined here to avoid a deep dependency on Sentinel::Sandbox internals.
enum class URLThreatLevel : u8 {
    Clean = 0,      // phishing_score < 0.3
    Suspicious = 1, // 0.3 – 0.6
    Malicious = 2,  // 0.6 – 0.8
    Critical = 3,   // >= 0.8
};

struct URLVerdict {
    URLThreatLevel level { URLThreatLevel::Clean };
    float score { 0.0f };
    ByteString explanation;
};

// URLVerdictService — synchronous, in-process URL phishing check via
// Sentinel::PhishingURLAnalyzer (already linked as sentinelservice).
//
// Usage:
//   auto verdict = URLVerdictService::the().check(url);
//   if (verdict.level >= URLThreatLevel::Suspicious) { ... }
//
// Fail-open: any error from the analyzer returns Clean to avoid blocking
// navigation when Sentinel subsystems are unavailable.
class URLVerdictService {
public:
    static URLVerdictService& the();

    // Synchronous verdict. Returns immediately from cache when available.
    // Always returns a valid verdict (never propagates analyzer errors).
    URLVerdict check(URL::URL const& url);

    // Bypass the feature gate (for testing).
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

private:
    URLVerdictService();

    // Domain-keyed LRU cache: key = lowercase eTLD+1, value = cached verdict.
    // Entries expire after TTL_SECONDS.
    static constexpr u32 CACHE_MAX_ENTRIES = 1000;
    static constexpr u64 TTL_SECONDS = 300; // 5 minutes

    struct CacheEntry {
        URLVerdict verdict;
        u64 stored_at_s; // time(nullptr) at store time
    };

    Optional<URLVerdict> cache_get(ByteString const& domain);
    void cache_store(ByteString const& domain, URLVerdict const&);

    // Returns lowercase hostname from URL, or empty string on failure.
    static ByteString extract_domain(URL::URL const& url);

    HashMap<ByteString, CacheEntry> m_cache;
    bool m_enabled { true };
};

} // namespace WebContent
