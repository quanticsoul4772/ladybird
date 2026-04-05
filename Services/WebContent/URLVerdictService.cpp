/*
 * Copyright (c) 2026, quanticsoul4772
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "URLVerdictService.h"
#include <AK/Debug.h>
#include <AK/StringBuilder.h>
#include <Sentinel/PhishingURLAnalyzer.h>
#include <ctime>

namespace WebContent {

static OwnPtr<Sentinel::PhishingURLAnalyzer> s_analyzer;

URLVerdictService& URLVerdictService::the()
{
    static URLVerdictService s_instance;
    return s_instance;
}

URLVerdictService::URLVerdictService()
{
    // Create analyzer at first use; log but don't crash if it fails.
    auto result = Sentinel::PhishingURLAnalyzer::create();
    if (result.is_error()) {
        dbgln("URLVerdictService: failed to create PhishingURLAnalyzer: {}", result.error());
    } else {
        s_analyzer = result.release_value();
    }
}

URLVerdict URLVerdictService::check(URL::URL const& url)
{
    // Fail-open: disabled or no analyzer → Clean.
    if (!m_enabled || !s_analyzer) {
        return {};
    }

    // Only check http/https — skip file://, about:, data:, blob:, etc.
    auto scheme = url.scheme();
    if (scheme != "http" && scheme != "https") {
        return {};
    }

    auto domain = extract_domain(url);
    if (domain.is_empty()) {
        return {};
    }

    // Cache hit?
    if (auto cached = cache_get(domain); cached.has_value()) {
        dbgln_if(URL_VERDICT_DEBUG, "URLVerdictService: cache hit for {} → level={}", domain, (int)cached->level);
        return cached.value();
    }

    // Analyze.
    auto result = s_analyzer->analyze_url(url.serialize());
    if (result.is_error()) {
        dbgln("URLVerdictService: analyzer error for {}: {}", domain, result.error());
        // Fail-open.
        return {};
    }

    auto const& analysis = result.value();

    // Map float score to threat level using Sentinel's thresholds.
    URLThreatLevel level;
    if (analysis.phishing_score >= 0.8f)
        level = URLThreatLevel::Critical;
    else if (analysis.phishing_score >= 0.6f)
        level = URLThreatLevel::Malicious;
    else if (analysis.phishing_score >= 0.3f)
        level = URLThreatLevel::Suspicious;
    else
        level = URLThreatLevel::Clean;

    URLVerdict verdict {
        .level = level,
        .score = analysis.phishing_score,
        .explanation = analysis.explanation.to_byte_string(),
    };

    dbgln_if(URL_VERDICT_DEBUG, "URLVerdictService: {} → score={:.2f} level={}", domain, analysis.phishing_score, (int)level);

    cache_store(domain, verdict);
    return verdict;
}

ByteString URLVerdictService::extract_domain(URL::URL const& url)
{
    auto host = url.serialized_host();
    if (!host.has_value())
        return {};
    // Lowercase for consistent cache keying.
    return MUST(host.value().to_lowercase()).to_byte_string();
}

Optional<URLVerdict> URLVerdictService::cache_get(ByteString const& domain)
{
    auto it = m_cache.find(domain);
    if (it == m_cache.end())
        return {};
    u64 now = static_cast<u64>(time(nullptr));
    if (now - it->value.stored_at_s > TTL_SECONDS) {
        m_cache.remove(domain);
        return {};
    }
    return it->value.verdict;
}

void URLVerdictService::cache_store(ByteString const& domain, URLVerdict const& verdict)
{
    // Simple eviction: drop oldest half when full.
    if (m_cache.size() >= CACHE_MAX_ENTRIES) {
        // HashMap doesn't guarantee order — just clear half arbitrarily.
        // A proper LRU can be added if needed; this is a safety valve.
        u32 to_remove = CACHE_MAX_ENTRIES / 2;
        Vector<ByteString> keys_to_remove;
        keys_to_remove.ensure_capacity(to_remove);
        for (auto const& entry : m_cache) {
            keys_to_remove.append(entry.key);
            if (keys_to_remove.size() >= to_remove)
                break;
        }
        for (auto const& key : keys_to_remove)
            m_cache.remove(key);
    }
    m_cache.set(domain, CacheEntry {
        .verdict = verdict,
        .stored_at_s = static_cast<u64>(time(nullptr)),
    });
}

} // namespace WebContent
