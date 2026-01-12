# Phase 6: Network Behavioral Analysis - Quick Reference Card

**For**: Developers implementing Phase 6 features
**Date**: 2025-11-01

---

## Component Quick Reference

### TrafficMonitor (RequestServer Integration)

```cpp
// Location: Services/RequestServer/TrafficMonitor.{h,cpp}

// Main API
class TrafficMonitor {
    // Recording (called by Request lifecycle)
    void record_request(RequestMetadata const& metadata);
    void record_dns_query(StringView domain);

    // Analysis (async, background threads)
    void analyze_domain(StringView domain);
    void analyze_connection_pattern(StringView domain);

    // Alerts
    void set_alert_callback(AlertCallback callback);

    // Configuration
    void enable_monitoring(bool enabled);
    void set_privacy_mode(bool enabled);
};

// Usage in ConnectionFromClient
void ConnectionFromClient::start_request(...)
{
    if (m_traffic_monitor) {
        m_traffic_monitor->record_dns_query(url.host()->serialize());
    }
}

void Request::~Request()
{
    if (m_client.traffic_monitor()) {
        m_client.traffic_monitor()->record_request(metadata);
        m_client.traffic_monitor()->analyze_domain(m_url.host()->serialize());
    }
}
```

---

### DNSAnalyzer (DGA Detection)

```cpp
// Location: Services/Sentinel/DNSAnalyzer.{h,cpp}

// Main API
class DNSAnalyzer {
    // DGA detection
    ErrorOr<DGAAnalysis> analyze_for_dga(StringView domain);

    // DNS tunneling detection
    ErrorOr<TunnelingAnalysis> analyze_for_tunneling(
        StringView domain,
        Vector<UnixDateTime> const& query_times
    );

    // Comprehensive analysis
    ErrorOr<DNSSecurityAnalysis> analyze_domain(
        StringView domain,
        Vector<UnixDateTime> const& query_times
    );

    // Whitelist management
    void add_trusted_domain(StringView domain);
    bool is_trusted_domain(StringView domain) const;
};

// Example usage
auto analysis = TRY(dns_analyzer->analyze_for_dga("evil-random-xyz123.net"));
if (analysis.dga_score > 0.7f) {
    // DGA detected - generate alert
}
```

---

### C2Detector (Beaconing & Exfiltration)

```cpp
// Location: Services/Sentinel/C2Detector.{h,cpp}

// Main API
class C2Detector {
    // Beaconing detection
    ErrorOr<BeaconingAnalysis> analyze_for_beaconing(
        StringView domain,
        Vector<UnixDateTime> const& request_times
    );

    // Exfiltration detection
    ErrorOr<ExfiltrationAnalysis> analyze_for_exfiltration(
        StringView domain,
        u64 bytes_sent,
        u64 bytes_received
    );

    // Comprehensive C2 analysis
    ErrorOr<C2Analysis> analyze_connection_pattern(
        StringView domain,
        u64 request_count,
        u64 bytes_sent,
        u64 bytes_received,
        Vector<UnixDateTime> const& request_times
    );

    // Whitelist management
    void add_known_service(StringView domain);
    bool is_known_service(StringView domain) const;
};

// Example usage
auto analysis = TRY(c2_detector->analyze_for_beaconing(domain, request_times));
if (analysis.beaconing_score > 0.7f) {
    // Beaconing detected - generate alert
}
```

---

## Detection Algorithm Cheat Sheet

### DGA Detection

**Formula**: `score = entropy_score + consonant_score + ngram_score`

```cpp
if (entropy > 3.5)              score += 0.4
if (consonant_ratio > 0.6)      score += 0.3
if (vowel_ratio < 0.2 or > 0.6) score += 0.2
if (ngram_unusual)              score += 0.2
if (is_trusted_domain)          score = 0.0  // override

alert_if score > 0.7
```

**Example**:
- `google.com` → entropy=2.1, consonants=0.5, vowels=0.5 → **score=0.1** (benign)
- `xk3j9f2lm8n.com` → entropy=3.8, consonants=0.7, vowels=0.1 → **score=0.9** (DGA)

---

### Beaconing Detection

**Formula**: `CV = stddev(intervals) / mean(intervals)`

```cpp
intervals = [t2-t1, t3-t2, ..., tn-tn-1]
mean = average(intervals)
stddev = standard_deviation(intervals)
CV = stddev / mean

if (CV < 0.2)      beaconing_score = 1.0 - CV  // Very regular
else if (CV < 0.5) beaconing_score = 0.6       // Somewhat regular
else               beaconing_score = 0.0       // Normal variance

alert_if beaconing_score > 0.7
```

**Example**:
- Regular requests (60s, 60s, 61s, 59s) → CV=0.01 → **score=0.99** (beaconing)
- Normal requests (10s, 45s, 3s, 120s) → CV=0.8 → **score=0.0** (benign)

---

### Exfiltration Detection

**Formula**: `upload_ratio = bytes_sent / (bytes_sent + bytes_received)`

```cpp
upload_ratio = bytes_sent / (bytes_sent + bytes_received)

if (upload_ratio > 0.7)  score += 0.4
if (upload_ratio > 0.9)  score += 0.3
if (bytes_sent > 100MB)  score += 0.3
if (!is_known_upload_service) score *= 1.0
else                          score *= 0.5  // reduce for known services

alert_if score > 0.7
```

**Example**:
- Normal browsing: sent=1KB, received=50KB → ratio=0.02 → **score=0.0** (benign)
- Data exfiltration: sent=100MB, received=1KB → ratio=0.99 → **score=1.0** (alert)

---

## IPC Message Definitions

### RequestClient.ipc (Add to existing file)

```cpp
endpoint RequestClient
{
    // Existing messages...

    // Phase 6: Network behavioral analysis alert
    network_behavior_alert(ByteString alert_json) =|
}
```

### Alert JSON Schema

```json
{
  "alert_type": "dga" | "beaconing" | "exfiltration" | "dns_tunneling",
  "severity": "low" | "medium" | "high" | "critical",
  "domain": "evil-domain.com",
  "confidence": 0.87,
  "explanation": "Human-readable explanation",
  "indicators": ["List", "of", "evidence"],
  "detected_at": "2025-11-01T12:34:56Z",
  "recommended_action": "allow" | "block" | "alert"
}
```

---

## Test Patterns

### Unit Test Structure

```cpp
// Services/Sentinel/TestDNSAnalyzer.cpp
TEST_CASE(dga_detection_high_entropy)
{
    auto analyzer = MUST(DNSAnalyzer::create());

    // Known DGA domain (high entropy, random characters)
    auto result = MUST(analyzer->analyze_for_dga("xk3j9f2lm8n.com"));

    EXPECT(result.dga_score > 0.7f);
    EXPECT(result.is_random_domain);
}

TEST_CASE(dga_whitelist_enforcement)
{
    auto analyzer = MUST(DNSAnalyzer::create());

    // Popular domain should NOT trigger
    auto result = MUST(analyzer->analyze_for_dga("google.com"));

    EXPECT(result.dga_score < 0.3f);
    EXPECT(!result.is_random_domain);
}
```

### Integration Test Scenarios

```cpp
// Tests/LibWeb/Text/input/network-behavior-dga.html
<script>
test(() => {
    // Trigger DGA detection by querying random domain
    fetch("http://xk3j9f2lm8n.com/test")
        .catch(() => {});

    // Expect: TrafficMonitor generates DGA alert
    // Expect: IPC network_behavior_alert sent to WebContent
    // Expect: User sees security alert dialog
}, "DGA detection for random domain");
</script>
```

---

## Performance Checklist

| Item | Target | Verification |
|------|--------|--------------|
| Per-request overhead | <1ms | Use `Core::ElapsedTimer` |
| Memory footprint | <1MB | Check `m_connection_patterns.size()` |
| CPU usage | <1% | Use `top` or `htop` |
| Cache hit rate | >80% | Check `m_stats.cache_hit_rate()` |
| Analysis latency | <2ms | Time DNSAnalyzer/C2Detector calls |

### Performance Testing

```cpp
// Benchmark DGA detection
Core::ElapsedTimer timer;
for (int i = 0; i < 1000; i++) {
    auto result = MUST(dns_analyzer->analyze_for_dga("test-domain.com"));
}
auto elapsed_ms = timer.elapsed_milliseconds();
dbgln("DGA analysis: {:.2f}ms average", elapsed_ms / 1000.0);
// Expected: <1ms average
```

---

## Privacy Checklist

### What to Track
- ✅ Domain names (e.g., "example.com")
- ✅ Request counts per domain
- ✅ Byte counts (sent/received)
- ✅ Request timestamps (for interval analysis)

### What NOT to Track
- ❌ Full URLs (no paths, query params)
- ❌ Request/response headers
- ❌ Request/response bodies
- ❌ IP addresses
- ❌ User agents
- ❌ Cookies
- ❌ User identity

### Privacy Mode Enforcement

```cpp
void TrafficMonitor::set_privacy_mode(bool enabled)
{
    m_privacy_mode = enabled;

    if (enabled) {
        // Clear all tracking data
        m_connection_patterns.clear();
        m_dns_queries.clear();
        m_alerted_domains.clear();

        // Disable PolicyGraph integration
        m_policy_graph = nullptr;
    }
}
```

---

## Common Pitfalls & Solutions

### Pitfall 1: Blocking Main Thread

❌ **Wrong**:
```cpp
void Request::~Request() {
    auto analysis = m_dns_analyzer->analyze_for_dga(domain);  // Blocks!
    if (analysis.dga_score > 0.7)
        alert_user();
}
```

✅ **Correct**:
```cpp
void Request::~Request() {
    m_traffic_monitor->analyze_domain(domain);  // Async!
}

void TrafficMonitor::analyze_domain(StringView domain) {
    enqueue_background_task([this, domain]() {
        auto analysis = m_dns_analyzer->analyze_for_dga(domain);
        Core::EventLoop::current().post_event([this, analysis]() {
            on_analysis_complete(analysis);  // Back to main thread
        });
    });
}
```

---

### Pitfall 2: False Positives on Popular Sites

❌ **Wrong**:
```cpp
if (entropy > 3.0)
    return "DGA detected!";  // Will trigger on github.com, etc.
```

✅ **Correct**:
```cpp
if (is_trusted_domain(domain))
    return 0.0;  // Whitelist check FIRST

if (entropy > 3.5)
    score += 0.4;
```

---

### Pitfall 3: Privacy Violations

❌ **Wrong**:
```cpp
struct RequestMetadata {
    URL::URL full_url;  // Contains sensitive path/query!
    String user_agent;  // PII!
};
```

✅ **Correct**:
```cpp
struct RequestMetadata {
    String domain;  // "example.com" ONLY
    u64 bytes_sent;
    u64 bytes_received;
    UnixDateTime timestamp;
    // NO URLs, headers, bodies, IP addresses!
};
```

---

## File Locations

### Source Files

```
Services/
├── RequestServer/
│   ├── TrafficMonitor.h              # Event aggregation, orchestration
│   ├── TrafficMonitor.cpp
│   ├── TestTrafficMonitor.cpp        # Unit tests
│   └── RequestClient.ipc             # Add network_behavior_alert message
│
└── Sentinel/
    ├── DNSAnalyzer.h                 # DGA and DNS tunneling detection
    ├── DNSAnalyzer.cpp
    ├── TestDNSAnalyzer.cpp           # Unit tests
    ├── C2Detector.h                  # Beaconing and exfiltration detection
    ├── C2Detector.cpp
    └── TestC2Detector.cpp            # Unit tests
```

### Data Files

```
/usr/share/ladybird/
├── popular-domains.txt               # Top 1000 domains whitelist
└── upload-services.txt               # Known upload services (CDNs, cloud)

~/.config/ladybird/
└── policies.db                       # PolicyGraph SQLite (network_behavior_policies table)
```

---

## Implementation Phases (5 Weeks)

```
Week 1: TrafficMonitor Foundation
  ├─ Event recording infrastructure
  ├─ Request lifecycle integration
  ├─ IPC message definitions
  └─ Unit tests

Week 2: DNSAnalyzer
  ├─ DGA detection algorithms
  ├─ DNS tunneling detection
  ├─ Whitelist management
  └─ Unit tests

Week 3: C2Detector
  ├─ Beaconing detection
  ├─ Exfiltration detection
  ├─ Port scanning detection (optional)
  └─ Unit tests

Week 4: PolicyGraph & UI
  ├─ PolicyGraph schema updates
  ├─ Alert UI (WebContent)
  ├─ Settings UI (enable/disable, privacy mode)
  └─ about:network-monitor dashboard

Week 5: Testing & Documentation
  ├─ End-to-end browser tests
  ├─ Performance benchmarking
  ├─ User documentation
  └─ Developer documentation
```

---

## Quick Commands

### Build
```bash
./Meta/ladybird.py build
```

### Run Tests
```bash
./Build/release/bin/TestDNSAnalyzer
./Build/release/bin/TestC2Detector
./Build/release/bin/TestTrafficMonitor
```

### Debug Logging
```bash
WEBCONTENT_DEBUG=1 REQUESTSERVER_DEBUG=1 ./Build/release/bin/Ladybird
```

### Performance Profiling
```bash
./Meta/ladybird.py profile ladybird
```

---

## Resources

### Documents
- [PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md](PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md) - Full architecture (39KB)
- [PHASE_6_ARCHITECTURE_DIAGRAMS.md](PHASE_6_ARCHITECTURE_DIAGRAMS.md) - Detailed diagrams (28KB)
- [PHASE_6_SUMMARY.md](PHASE_6_SUMMARY.md) - Executive summary (14KB)

### Related Components
- [FingerprintingDetector.h](../Services/Sentinel/FingerprintingDetector.h) - Phase 4 pattern reference
- [PhishingURLAnalyzer.h](../Services/Sentinel/PhishingURLAnalyzer.h) - Phase 5 pattern reference
- [MalwareML.h](../Services/Sentinel/MalwareML.h) - Phase 2 ML infrastructure
- [SecurityTap.h](../Services/RequestServer/SecurityTap.h) - RequestServer integration pattern

---

**Quick Start**: Read PHASE_6_SUMMARY.md → Review architecture → Implement Phase 6A → Test → Iterate

**Questions?** Review full architecture docs or consult existing Sentinel components for patterns.

---

*Created: 2025-11-01*
*Ladybird Browser - Sentinel Phase 6 Quick Reference*
