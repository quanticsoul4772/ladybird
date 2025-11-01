/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/QuickSort.h>
#include <AK/Time.h>
#include <Services/RequestServer/TrafficMonitor.h>
#include <Services/Sentinel/C2Detector.h>
#include <Services/Sentinel/DNSAnalyzer.h>

namespace RequestServer {

TrafficMonitor::TrafficMonitor()
{
}

TrafficMonitor::~TrafficMonitor() = default;

ErrorOr<NonnullOwnPtr<TrafficMonitor>> TrafficMonitor::create()
{
    auto monitor = TRY(adopt_nonnull_own_or_enomem(new (nothrow) TrafficMonitor()));
    TRY(monitor->initialize_detectors());
    return monitor;
}

ErrorOr<void> TrafficMonitor::initialize_detectors()
{
    // Attempt to create detectors - graceful degradation if they fail
    auto dns_result = Sentinel::DNSAnalyzer::create();
    if (dns_result.is_error()) {
        dbgln("TrafficMonitor: Warning - DNSAnalyzer creation failed: {}", dns_result.error());
        // Continue without DNS analyzer
    } else {
        m_dns_analyzer = dns_result.release_value();
    }

    auto c2_result = Sentinel::C2Detector::create();
    if (c2_result.is_error()) {
        dbgln("TrafficMonitor: Warning - C2Detector creation failed: {}", c2_result.error());
        // Continue without C2 detector
    } else {
        m_c2_detector = c2_result.release_value();
    }

    if (!m_dns_analyzer && !m_c2_detector) {
        dbgln("TrafficMonitor: Warning - No detectors available, monitoring will be limited");
    }

    return {};
}

ErrorOr<void> TrafficMonitor::record_request(StringView domain, u64 bytes_sent, u64 bytes_received)
{
    // Validate input
    if (domain.is_empty())
        return Error::from_string_literal("Domain cannot be empty");

    // Get or create pattern for this domain
    String domain_string = TRY(String::from_utf8(domain));

    // Enforce MAX_PATTERNS limit using LRU eviction
    if (!m_patterns.contains(domain_string) && m_patterns.size() >= MAX_PATTERNS) {
        evict_oldest_pattern();
    }

    // Get or create pattern
    auto& pattern = m_patterns.ensure(domain_string, [&]() {
        ConnectionPattern p;
        p.domain = domain_string;
        return p;
    });

    // Update pattern metrics
    pattern.request_count++;
    pattern.bytes_sent += bytes_sent;
    pattern.bytes_received += bytes_received;

    // Record timestamp (Unix epoch in seconds as double)
    auto now = UnixDateTime::now();
    double timestamp = static_cast<double>(now.seconds_since_epoch());
    TRY(pattern.request_timestamps.try_append(timestamp));

    return {};
}

ErrorOr<Optional<TrafficAlert>> TrafficMonitor::analyze_pattern(StringView domain)
{
    // Validate input
    if (domain.is_empty())
        return Optional<TrafficAlert> {};

    // Find pattern
    String domain_string = TRY(String::from_utf8(domain));
    auto it = m_patterns.find(domain_string);
    if (it == m_patterns.end())
        return Optional<TrafficAlert> {}; // No pattern for this domain

    auto& pattern = it->value;

    // Check if analysis is needed
    auto now = UnixDateTime::now();
    double current_time = static_cast<double>(now.seconds_since_epoch());

    // Skip if analyzed recently (within ANALYSIS_INTERVAL)
    if (pattern.last_analyzed > 0.0 && (current_time - pattern.last_analyzed) < ANALYSIS_INTERVAL)
        return Optional<TrafficAlert> {};

    // Skip if not enough data for meaningful analysis
    if (pattern.request_count < MIN_REQUESTS_FOR_ANALYSIS)
        return Optional<TrafficAlert> {};

    // Initialize scores
    float dga_score = 0.0f;
    float beaconing_score = 0.0f;
    float exfiltration_score = 0.0f;

    // Run DNSAnalyzer if available
    if (m_dns_analyzer) {
        auto dga_result = m_dns_analyzer->analyze_dga(domain);
        if (!dga_result.is_error()) {
            auto dga_analysis = dga_result.release_value();
            // Calculate DGA score from analysis results
            // High entropy + unusual n-grams + high consonant ratio = higher score
            if (dga_analysis.is_dga) {
                dga_score = dga_analysis.confidence;
            } else {
                // Calculate score from components even if not flagged as DGA
                float entropy_factor = min(dga_analysis.entropy / 5.0f, 1.0f);      // Normalize entropy
                float ngram_factor = dga_analysis.ngram_score;
                float consonant_factor = dga_analysis.consonant_ratio > 0.65f ? 1.0f : 0.0f;
                dga_score = (entropy_factor * 0.5f + ngram_factor * 0.3f + consonant_factor * 0.2f);
            }
        }
    }

    // Run C2Detector if available (beaconing analysis)
    if (m_c2_detector && pattern.request_timestamps.size() >= 2) {
        // Calculate request intervals for beaconing detection (convert double to float)
        Vector<float> intervals;
        for (size_t i = 1; i < pattern.request_timestamps.size(); i++) {
            float interval = static_cast<float>(pattern.request_timestamps[i] - pattern.request_timestamps[i - 1]);
            if (auto result = intervals.try_append(interval); result.is_error()) {
                dbgln("TrafficMonitor: Failed to append interval: {}", result.error());
            }
        }

        if (!intervals.is_empty()) {
            auto beaconing_result = m_c2_detector->analyze_beaconing(intervals);
            if (!beaconing_result.is_error()) {
                auto beaconing_analysis = beaconing_result.release_value();
                // Calculate beaconing score from analysis
                if (beaconing_analysis.is_beaconing) {
                    beaconing_score = beaconing_analysis.confidence;
                } else {
                    // Lower score for less regular patterns
                    // CV < 0.2 = very regular, CV > 0.4 = irregular
                    float cv = beaconing_analysis.interval_regularity;
                    if (cv < 0.4f) {
                        beaconing_score = (0.4f - cv) / 0.4f; // Scale 0-1
                    }
                }
            }
        }

        // Exfiltration analysis
        auto exfil_result = m_c2_detector->analyze_exfiltration(
            pattern.bytes_sent,
            pattern.bytes_received,
            domain);

        if (!exfil_result.is_error()) {
            auto exfil_analysis = exfil_result.release_value();
            // Calculate exfiltration score
            if (exfil_analysis.is_exfiltration) {
                exfiltration_score = exfil_analysis.confidence;
            } else {
                // Partial score based on upload ratio
                if (exfil_analysis.upload_ratio > 0.7f) {
                    exfiltration_score = (exfil_analysis.upload_ratio - 0.7f) / 0.3f; // Scale 0-1
                }
            }
        }
    }

    // Update last_analyzed timestamp
    pattern.last_analyzed = current_time;

    // Calculate composite score
    auto composite_score = TRY(calculate_composite_score(domain, pattern));

    // Generate alert if score exceeds threshold
    if (composite_score >= ALERT_THRESHOLD) {
        auto alert = TRY(generate_alert(domain, pattern, dga_score, beaconing_score, exfiltration_score));

        // Add to alerts list (enforce MAX_ALERTS limit)
        if (m_alerts.size() >= MAX_ALERTS) {
            m_alerts.remove(0); // Remove oldest (FIFO)
        }
        TRY(m_alerts.try_append(alert));

        return alert;
    }

    return Optional<TrafficAlert> {};
}

ErrorOr<Vector<TrafficAlert>> TrafficMonitor::get_recent_alerts(size_t max_count)
{
    Vector<TrafficAlert> result;

    // Return most recent alerts (up to max_count)
    size_t start_index = 0;
    if (m_alerts.size() > max_count) {
        start_index = m_alerts.size() - max_count;
    }

    for (size_t i = start_index; i < m_alerts.size(); i++) {
        TRY(result.try_append(m_alerts[i]));
    }

    return result;
}

void TrafficMonitor::clear_old_patterns(double max_age_seconds)
{
    auto now = UnixDateTime::now();
    double current_time = static_cast<double>(now.seconds_since_epoch());

    // Find patterns older than max_age_seconds
    Vector<String> domains_to_remove;
    for (auto const& [domain, pattern] : m_patterns) {
        if (pattern.request_timestamps.is_empty())
            continue;

        // Get last request timestamp
        double last_request_time = pattern.request_timestamps.last();
        double age = current_time - last_request_time;

        if (age > max_age_seconds) {
            if (auto result = domains_to_remove.try_append(domain); result.is_error()) {
                dbgln("TrafficMonitor: Failed to append domain for removal: {}", result.error());
            }
        }
    }

    // Remove old patterns
    for (auto const& domain : domains_to_remove) {
        m_patterns.remove(domain);
    }

    if (!domains_to_remove.is_empty()) {
        dbgln("TrafficMonitor: Cleared {} old patterns (age > {}s)", domains_to_remove.size(), max_age_seconds);
    }
}

ErrorOr<float> TrafficMonitor::calculate_composite_score(StringView domain, ConnectionPattern const& pattern)
{
    float composite = 0.0f;
    float dga_score = 0.0f;
    float beaconing_score = 0.0f;
    float exfiltration_score = 0.0f;
    float dns_tunneling_score = 0.0f;

    // DGA detection
    if (m_dns_analyzer) {
        auto dga_result = m_dns_analyzer->analyze_dga(domain);
        if (!dga_result.is_error()) {
            auto dga_analysis = dga_result.value();
            if (dga_analysis.is_dga) {
                dga_score = dga_analysis.confidence;
            } else {
                // Calculate partial score from components
                float entropy_factor = min(dga_analysis.entropy / 5.0f, 1.0f);
                float ngram_factor = dga_analysis.ngram_score;
                float consonant_factor = dga_analysis.consonant_ratio > 0.65f ? 1.0f : 0.0f;
                dga_score = (entropy_factor * 0.5f + ngram_factor * 0.3f + consonant_factor * 0.2f);
            }
        }
    }

    // Beaconing detection (requires at least 2 requests)
    if (m_c2_detector && pattern.request_timestamps.size() >= 2) {
        // Calculate intervals
        Vector<float> intervals;
        for (size_t i = 1; i < pattern.request_timestamps.size(); i++) {
            float interval = static_cast<float>(pattern.request_timestamps[i] - pattern.request_timestamps[i - 1]);
            if (auto result = intervals.try_append(interval); result.is_error()) {
                dbgln("TrafficMonitor: Failed to append interval: {}", result.error());
            }
        }

        if (!intervals.is_empty()) {
            auto beaconing_result = m_c2_detector->analyze_beaconing(intervals);
            if (!beaconing_result.is_error()) {
                auto beaconing_analysis = beaconing_result.value();
                if (beaconing_analysis.is_beaconing) {
                    beaconing_score = beaconing_analysis.confidence;
                } else {
                    float cv = beaconing_analysis.interval_regularity;
                    if (cv < 0.4f) {
                        beaconing_score = (0.4f - cv) / 0.4f;
                    }
                }
            }
        }

        // Exfiltration detection
        auto exfil_result = m_c2_detector->analyze_exfiltration(
            pattern.bytes_sent,
            pattern.bytes_received,
            domain);

        if (!exfil_result.is_error()) {
            auto exfil_analysis = exfil_result.value();
            if (exfil_analysis.is_exfiltration) {
                exfiltration_score = exfil_analysis.confidence;
            } else {
                if (exfil_analysis.upload_ratio > 0.7f) {
                    exfiltration_score = (exfil_analysis.upload_ratio - 0.7f) / 0.3f;
                }
            }
        }
    }

    // Calculate weighted composite score
    composite = (DGA_WEIGHT * dga_score) +
        (BEACONING_WEIGHT * beaconing_score) +
        (EXFILTRATION_WEIGHT * exfiltration_score) +
        (DNS_TUNNELING_WEIGHT * dns_tunneling_score);

    return composite;
}

ErrorOr<TrafficAlert> TrafficMonitor::generate_alert(StringView domain, ConnectionPattern const& pattern,
    float dga_score, float beaconing_score, float exfiltration_score)
{
    TrafficAlert alert;
    alert.domain = TRY(String::from_utf8(domain));
    alert.type = determine_alert_type(dga_score, beaconing_score, exfiltration_score);

    // Calculate severity (max of individual scores)
    alert.severity = max(max(dga_score, beaconing_score), exfiltration_score);

    // Build explanation
    Vector<String> explanations;

    if (dga_score > ALERT_THRESHOLD) {
        TRY(explanations.try_append(TRY(String::formatted("DGA detected (score: {:.2f})", dga_score))));
    }

    if (beaconing_score > ALERT_THRESHOLD) {
        TRY(explanations.try_append(TRY(String::formatted("Beaconing pattern detected (score: {:.2f})", beaconing_score))));
    }

    if (exfiltration_score > ALERT_THRESHOLD) {
        TRY(explanations.try_append(TRY(String::formatted("Data exfiltration detected (score: {:.2f})", exfiltration_score))));
    }

    // Generate indicators
    alert.indicators = move(explanations);

    // Build combined explanation
    StringBuilder explanation_builder;
    TRY(explanation_builder.try_appendff("Suspicious traffic detected for domain '{}': ", domain));
    TRY(explanation_builder.try_appendff("{} requests, {} bytes sent, {} bytes received",
        pattern.request_count, pattern.bytes_sent, pattern.bytes_received));

    alert.explanation = TRY(explanation_builder.to_string());

    return alert;
}

TrafficAlert::Type TrafficMonitor::determine_alert_type(float dga_score, float beaconing_score, float exfiltration_score) const
{
    // Count how many scores exceed threshold
    int high_scores = 0;
    if (dga_score > ALERT_THRESHOLD)
        high_scores++;
    if (beaconing_score > ALERT_THRESHOLD)
        high_scores++;
    if (exfiltration_score > ALERT_THRESHOLD)
        high_scores++;

    // If multiple threats detected, return Combined
    if (high_scores > 1)
        return TrafficAlert::Type::Combined;

    // Return the dominant threat type
    float max_score = max(max(dga_score, beaconing_score), exfiltration_score);

    if (max_score == dga_score)
        return TrafficAlert::Type::DGA;
    if (max_score == beaconing_score)
        return TrafficAlert::Type::Beaconing;
    if (max_score == exfiltration_score)
        return TrafficAlert::Type::Exfiltration;

    // Fallback
    return TrafficAlert::Type::Combined;
}

void TrafficMonitor::evict_oldest_pattern()
{
    if (m_patterns.is_empty())
        return;

    // Find pattern with oldest last_analyzed timestamp
    String oldest_domain;
    double oldest_time = __builtin_huge_val(); // Positive infinity

    for (auto const& [domain, pattern] : m_patterns) {
        if (pattern.last_analyzed < oldest_time) {
            oldest_time = pattern.last_analyzed;
            oldest_domain = domain;
        }
    }

    if (!oldest_domain.is_empty()) {
        m_patterns.remove(oldest_domain);
        dbgln("TrafficMonitor: Evicted oldest pattern: {}", oldest_domain);
    }
}

}
