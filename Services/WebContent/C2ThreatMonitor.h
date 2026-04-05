/*
 * Copyright (c) 2026, quanticsoul4772
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/HashMap.h>
#include <AK/Vector.h>
#include <LibURL/URL.h>
#include <Services/Sentinel/C2Detector.h>

namespace WebContent {

// C2ThreatMonitor — accumulates per-domain page navigation timestamps and
// triggers Sentinel::C2Detector::analyze_beaconing when enough data is collected.
//
// Rationale: C2 beaconing detection requires ≥5 inter-request intervals.
// This class is the accumulation layer; it tracks when each domain was navigated
// to and triggers analysis once sufficient data is available.
//
// Threat model: a compromised or malicious page that uses JS to trigger
// rapid same-domain navigations (e.g. XHR + location.href loops) to exfiltrate
// data via the URL or perform C2 check-ins at regular intervals.
//
// Usage (in PageClient::page_did_finish_loading):
//   m_c2_monitor->record_navigation(url);
class C2ThreatMonitor {
public:
    static ErrorOr<NonnullOwnPtr<C2ThreatMonitor>> create();

    // Record a completed page navigation. Triggers analysis when
    // MinBeaconingRequests intervals are available for the domain.
    void record_navigation(URL::URL const& url);

private:
    C2ThreatMonitor(NonnullOwnPtr<Sentinel::C2Detector> detector);

    struct DomainStats {
        Vector<double> timestamps; // time(nullptr) cast to double, seconds
    };

    // Returns lowercase hostname from URL, or empty string.
    static ByteString extract_domain(URL::URL const& url);

    // Runs beaconing analysis if we have enough data; logs if detected.
    void maybe_analyze(ByteString const& domain, DomainStats& stats);

    NonnullOwnPtr<Sentinel::C2Detector> m_detector;
    HashMap<ByteString, DomainStats> m_domain_stats;

    // Maximum domains to track (memory bound).
    static constexpr size_t MaxTrackedDomains = 500;
    // Analysis fires when this many navigations are recorded for a domain.
    static constexpr size_t AnalysisTriggerCount = 6; // gives 5 intervals
};

} // namespace WebContent
