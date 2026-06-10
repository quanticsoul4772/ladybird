/*
 * Copyright (c) 2026, quanticsoul4772
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "C2ThreatMonitor.h"
#include <AK/Debug.h>
#include <ctime>

namespace WebContent {

ErrorOr<NonnullOwnPtr<C2ThreatMonitor>> C2ThreatMonitor::create()
{
    auto detector = TRY(Sentinel::C2Detector::create());
    return adopt_nonnull_own_or_enomem(new (nothrow) C2ThreatMonitor(move(detector)));
}

C2ThreatMonitor::C2ThreatMonitor(NonnullOwnPtr<Sentinel::C2Detector> detector)
    : m_detector(move(detector))
{
}

void C2ThreatMonitor::record_navigation(URL::URL const& url)
{
    auto scheme = url.scheme();
    if (scheme != "http" && scheme != "https")
        return;

    auto domain = extract_domain(url);
    if (domain.is_empty())
        return;

    // Evict oldest entry when tracking table is full.
    if (m_domain_stats.size() >= MaxTrackedDomains && !m_domain_stats.contains(domain)) {
        auto it = m_domain_stats.begin();
        if (it != m_domain_stats.end())
            m_domain_stats.remove(it->key);
    }

    auto& stats = m_domain_stats.ensure(domain);
    stats.timestamps.append(static_cast<double>(time(nullptr)));

    if (stats.timestamps.size() >= AnalysisTriggerCount) {
        maybe_analyze(domain, stats);
        // Keep only the most recent AnalysisTriggerCount timestamps to bound memory.
        auto keep_from = stats.timestamps.size() - AnalysisTriggerCount;
        stats.timestamps = stats.timestamps.slice(keep_from);
    }
}

void C2ThreatMonitor::maybe_analyze(ByteString const& domain, DomainStats& stats)
{
    // Compute inter-request intervals (seconds).
    Vector<float> intervals;
    intervals.ensure_capacity(stats.timestamps.size() - 1);
    for (size_t i = 1; i < stats.timestamps.size(); ++i) {
        float interval = static_cast<float>(stats.timestamps[i] - stats.timestamps[i - 1]);
        if (interval >= 0.0f)
            intervals.append(interval);
    }

    if (intervals.size() < Sentinel::C2Detector::MinBeaconingRequests)
        return;

    auto result = m_detector->analyze_beaconing(intervals);
    if (result.is_error()) {
        dbgln("C2ThreatMonitor: analyze_beaconing error for {}: {}", domain, result.error());
        return;
    }

    auto const& analysis = result.value();
    if (analysis.is_beaconing) {
        dbgln("C2ThreatMonitor: ⚠ BEACONING DETECTED — domain={} confidence={:.2f} cv={:.3f} requests={}",
            domain, analysis.confidence, analysis.interval_regularity, analysis.request_count);
        dbgln("C2ThreatMonitor: explanation: {}", analysis.explanation);
        // TODO Phase 2: emit a PageClient notification so the UI can show a warning.
        // For now, dbgln is sufficient for security audit logs.
    } else {
        dbgln_if(URL_VERDICT_DEBUG, "C2ThreatMonitor: {} — no beaconing (cv={:.3f})", domain, analysis.interval_regularity);
    }
}

ByteString C2ThreatMonitor::extract_domain(URL::URL const& url)
{
    auto host = url.serialized_host();
    if (!host.has_value())
        return {};
    return MUST(host.value().to_lowercase()).to_byte_string();
}

} // namespace WebContent
