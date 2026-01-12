/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

// Forward declarations for parallel implementations
namespace Sentinel {
class DNSAnalyzer;
class C2Detector;
}

namespace RequestServer {

// Connection pattern - aggregates traffic per domain
struct ConnectionPattern {
    String domain;                      // Domain name
    u32 request_count { 0 };           // Total requests
    u64 bytes_sent { 0 };              // Total bytes uploaded
    u64 bytes_received { 0 };          // Total bytes downloaded
    Vector<double> request_timestamps; // Request times (Unix epoch seconds)
    double last_analyzed { 0.0 };      // Last analysis timestamp (Unix epoch seconds)
};

// Traffic alert - generated when suspicious patterns detected
struct TrafficAlert {
    enum class Type {
        DGA,           // Domain Generation Algorithm
        Beaconing,     // Regular interval communication
        Exfiltration,  // Large data upload
        DNSTunneling,  // DNS-based data exfiltration
        Combined       // Multiple threat types detected
    };

    Type type;
    String domain;
    float severity;               // 0.0-1.0 (0 = low, 1 = critical)
    String explanation;           // Human-readable description
    Vector<String> indicators;    // Specific threat indicators
};

// TrafficMonitor - central orchestrator for network behavioral analysis
// Aggregates traffic patterns per domain and coordinates DNSAnalyzer + C2Detector
// for threat detection
class TrafficMonitor {
public:
    static ErrorOr<NonnullOwnPtr<TrafficMonitor>> create();
    ~TrafficMonitor();

    // Record a network request event
    ErrorOr<void> record_request(StringView domain, u64 bytes_sent, u64 bytes_received);

    // Analyze a domain's traffic pattern (returns alert if suspicious)
    ErrorOr<Optional<TrafficAlert>> analyze_pattern(StringView domain);

    // Get recent alerts (most recent first)
    ErrorOr<Vector<TrafficAlert>> get_recent_alerts(size_t max_count = 10);

    // Clear old patterns (default: 1 hour)
    void clear_old_patterns(double max_age_seconds = 3600.0);

private:
    TrafficMonitor();

    ErrorOr<void> initialize_detectors();
    ErrorOr<float> calculate_composite_score(StringView domain, ConnectionPattern const& pattern);

    // Generate alert from detection results
    ErrorOr<TrafficAlert> generate_alert(StringView domain, ConnectionPattern const& pattern,
        float dga_score, float beaconing_score, float exfiltration_score);

    // Determine alert type from scores
    TrafficAlert::Type determine_alert_type(float dga_score, float beaconing_score, float exfiltration_score) const;

    // LRU eviction - remove oldest pattern when at capacity
    void evict_oldest_pattern();

    HashMap<String, ConnectionPattern> m_patterns;           // Per-domain patterns
    Vector<TrafficAlert> m_alerts;                           // Recent alerts (FIFO)
    OwnPtr<Sentinel::DNSAnalyzer> m_dns_analyzer;
    OwnPtr<Sentinel::C2Detector> m_c2_detector;

    // Resource limits
    static constexpr size_t MAX_PATTERNS = 500;              // Maximum domains to track
    static constexpr size_t MAX_ALERTS = 100;                // Maximum alerts to store
    static constexpr double ANALYSIS_INTERVAL = 300.0;       // 5 minutes (seconds)
    static constexpr size_t MIN_REQUESTS_FOR_ANALYSIS = 5;   // Minimum requests before analysis
    static constexpr float ALERT_THRESHOLD = 0.7f;           // Minimum score to generate alert

    // Scoring weights (must sum to 1.0)
    static constexpr float DGA_WEIGHT = 0.3f;
    static constexpr float BEACONING_WEIGHT = 0.3f;
    static constexpr float EXFILTRATION_WEIGHT = 0.2f;
    static constexpr float DNS_TUNNELING_WEIGHT = 0.2f;
};

}
