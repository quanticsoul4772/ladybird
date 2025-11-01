# Phase 6: Network Behavioral Analysis - Detailed Architecture Diagrams

**Companion Document**: PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md
**Version**: 1.0
**Date**: 2025-11-01

This document provides detailed architectural diagrams for the Phase 6 Network Behavioral Analysis system.

---

## 1. System Architecture Overview

```mermaid
graph TB
    subgraph "Browser UI Process"
        Chrome[Browser Chrome]
        Settings[Settings UI]
        AlertDialog[Security Alert Dialog]
    end

    subgraph "WebContent Process (per tab)"
        Page[Web Page / Script]
        PageClient[PageClient]
    end

    subgraph "RequestServer Process (per WebContent)"
        ConnectionFromClient[ConnectionFromClient]
        TrafficMonitor[TrafficMonitor]
        Request[Request]
        SecurityTap[SecurityTap]
        URLSecurityAnalyzer[URLSecurityAnalyzer]

        subgraph "Network Components"
            CurlMulti[libcurl Multi Handle]
            DNSResolver[DNS Resolver]
        end
    end

    subgraph "Sentinel Components (Services/Sentinel/)"
        DNSAnalyzer[DNSAnalyzer]
        C2Detector[C2Detector]
        PolicyGraph[(PolicyGraph SQLite DB)]
        PhishingURLAnalyzer[PhishingURLAnalyzer]
        FingerprintingDetector[FingerprintingDetector]
        MalwareML[MalwareML]
    end

    subgraph "External Services"
        SentinelDaemon[Sentinel Daemon<br>/tmp/sentinel.sock]
    end

    %% User interactions
    Page -->|User navigates| ConnectionFromClient
    Chrome -->|User configures| Settings
    Settings -->|Enable/disable monitoring| TrafficMonitor

    %% Request flow
    Page -->|IPC: start_request| ConnectionFromClient
    ConnectionFromClient -->|create Request| Request
    Request -->|HTTP request| CurlMulti
    Request -->|DNS query| DNSResolver
    Request -->|on_headers_received| TrafficMonitor
    Request -->|on_data_received| TrafficMonitor

    %% Traffic monitoring flow
    TrafficMonitor -->|record_dns_query| DNSAnalyzer
    TrafficMonitor -->|analyze_connection_pattern| C2Detector
    DNSAnalyzer -->|check policy| PolicyGraph
    C2Detector -->|check policy| PolicyGraph

    %% Alert flow
    TrafficMonitor -->|alert_callback| ConnectionFromClient
    ConnectionFromClient -->|IPC: network_behavior_alert| PageClient
    PageClient -->|show alert| AlertDialog

    %% Download inspection flow
    Request -->|download complete| SecurityTap
    SecurityTap -->|scan| SentinelDaemon

    %% Cross-component correlation
    URLSecurityAnalyzer -.->|correlate phishing| TrafficMonitor
    SecurityTap -.->|correlate malware| TrafficMonitor

    %% Styling
    style TrafficMonitor fill:#e1f5ff,stroke:#0066cc,stroke-width:3px
    style DNSAnalyzer fill:#ffe1e1,stroke:#cc0000,stroke-width:2px
    style C2Detector fill:#ffe1e1,stroke:#cc0000,stroke-width:2px
    style PolicyGraph fill:#f0f0f0,stroke:#666,stroke-width:2px
    style AlertDialog fill:#fff4e1,stroke:#cc9900,stroke-width:2px
```

---

## 2. Component Interaction Matrix

| Component | Calls | Called By | Data Flow |
|-----------|-------|-----------|-----------|
| **TrafficMonitor** | DNSAnalyzer, C2Detector, PolicyGraph | ConnectionFromClient, Request | RequestMetadata → Analysis Results |
| **DNSAnalyzer** | PolicyGraph | TrafficMonitor | Domain → DGAAnalysis |
| **C2Detector** | PolicyGraph | TrafficMonitor | ConnectionPattern → C2Analysis |
| **PolicyGraph** | (SQLite DB) | TrafficMonitor, DNSAnalyzer, C2Detector | Domain → Policy Decision |
| **ConnectionFromClient** | TrafficMonitor, Request | PageClient (IPC) | URL → TrafficAlert |
| **Request** | TrafficMonitor | ConnectionFromClient | Lifecycle events → RequestMetadata |

---

## 3. Detailed Data Flow Diagrams

### 3.1 Full Request Lifecycle with Monitoring

```mermaid
sequenceDiagram
    participant Page as Web Page
    participant CFC as ConnectionFromClient
    participant Req as Request
    participant TM as TrafficMonitor
    participant DNA as DNSAnalyzer
    participant C2D as C2Detector
    participant PG as PolicyGraph
    participant Curl as libcurl

    %% Request initiation
    Page->>CFC: IPC: start_request(url, method, headers, body)
    CFC->>CFC: validate_url(), validate_request_id()

    %% DNS query recording
    CFC->>TM: record_dns_query("example.com")
    TM->>TM: Add to DNS query ring buffer
    TM->>DNA: analyze_for_dga("example.com") [ASYNC]

    %% Request creation
    CFC->>Req: Request::fetch()
    Req->>Curl: curl_multi_add_handle()

    %% DNS analysis (parallel)
    DNA->>DNA: calculate_shannon_entropy()
    DNA->>DNA: calculate_consonant_ratio()
    DNA->>DNA: calculate_ngram_score()
    DNA->>PG: is_trusted_domain("example.com")
    PG-->>DNA: false
    DNA-->>TM: DGAAnalysis{dga_score=0.2, is_random_domain=false}
    Note over TM: Score < 0.6, no alert

    %% HTTP headers received
    Curl-->>Req: CURLINFO_HEADER_IN
    Req->>Req: on_headers_received()
    Req->>TM: record_request(metadata: partial)
    TM->>TM: Store in m_connection_patterns["example.com"]

    %% HTTP body received
    loop Data chunks
        Curl-->>Req: CURLINFO_DATA_IN
        Req->>Req: on_data_received(chunk)
        Req->>Req: m_total_bytes_received += chunk.size()
    end

    %% Request complete
    Curl-->>Req: CURLE_OK
    Req->>TM: record_request(metadata: final)
    TM->>TM: Update m_connection_patterns["example.com"]
    TM->>TM: Check if ≥5 requests for pattern analysis

    alt ≥5 requests to same domain
        TM->>C2D: analyze_connection_pattern() [ASYNC]
        C2D->>C2D: calculate_intervals()
        C2D->>C2D: calculate_interval_regularity()
        C2D->>PG: is_known_service("example.com")
        PG-->>C2D: false
        C2D-->>TM: C2Analysis{beaconing_score=0.3, is_suspicious=false}
        Note over TM: Score < 0.6, no alert
    end

    Req->>CFC: request_complete()
    CFC->>Page: IPC: request_finished()
```

### 3.2 Alert Generation Flow

```mermaid
sequenceDiagram
    participant TM as TrafficMonitor
    participant DNA as DNSAnalyzer
    participant C2D as C2Detector
    participant PG as PolicyGraph
    participant CFC as ConnectionFromClient
    participant Page as WebContent (PageClient)
    participant UI as Alert Dialog

    %% Trigger: Suspicious domain detected
    TM->>DNA: analyze_for_dga("evil-random-xyz123.net")
    DNA->>DNA: calculate_shannon_entropy() → 4.2
    DNA->>DNA: calculate_consonant_ratio() → 0.7
    DNA->>PG: is_trusted_domain("evil-random-xyz123.net")
    PG-->>DNA: false (not in whitelist)
    DNA-->>TM: DGAAnalysis{dga_score=0.9, is_random_domain=true}

    %% Alert decision
    TM->>TM: Check alert deduplication
    alt Domain not yet alerted
        TM->>PG: evaluate_policy("evil-random-xyz123.net", "network_behavior")
        PG-->>TM: No existing policy

        %% Generate alert
        TM->>TM: create TrafficAlert{<br>  severity=High,<br>  type="DGA",<br>  confidence=0.9<br>}
        TM->>TM: Add to m_alerted_domains
        TM->>CFC: alert_callback(TrafficAlert)

        %% Send to UI
        CFC->>CFC: generate_alert_json()
        CFC->>Page: IPC: network_behavior_alert(alert_json)
        Page->>UI: Show security alert dialog

        %% User decision
        UI->>Page: User clicks "Block Domain"
        Page->>CFC: IPC: enforce_security_policy(request_id, "block")
        CFC->>PG: create_policy("evil-random-xyz123.net", "block")
        PG->>PG: INSERT INTO policies (domain, action, created_at)
    else Domain already alerted
        Note over TM: Skip alert (deduplication)
    end
```

### 3.3 Beaconing Detection with Correlation

```mermaid
sequenceDiagram
    participant Malware as Malicious Script
    participant Req as Request
    participant TM as TrafficMonitor
    participant C2D as C2Detector
    participant DNA as DNSAnalyzer
    participant ST as SecurityTap
    participant Page as PageClient

    %% Beaconing pattern (regular intervals)
    loop Every 60 seconds (10 times)
        Malware->>Req: POST /beacon (tiny payload)
        Req->>TM: record_request(bytes_sent=100, bytes_received=50)
        Note over TM: Connection pattern building
    end

    %% Analysis trigger after N requests
    Note over TM: 10 requests detected, analyze pattern
    TM->>TM: get_connection_pattern("c2-server.evil")
    TM->>C2D: analyze_for_beaconing(request_times=[t1...t10])

    C2D->>C2D: calculate_intervals() → [60s, 60s, 59s, 61s, ...]
    C2D->>C2D: calculate_mean_interval() → 60.1s
    C2D->>C2D: calculate_interval_regularity() → CV=0.08
    Note over C2D: CV < 0.2 = very regular = beaconing
    C2D-->>TM: BeaconingAnalysis{<br>  beaconing_score=0.92,<br>  is_beaconing=true<br>}

    %% Cross-component correlation
    TM->>DNA: analyze_for_dga("c2-server.evil")
    DNA-->>TM: DGAAnalysis{dga_score=0.7, is_random_domain=true}
    Note over TM: Beaconing + DGA = high confidence C2

    TM->>ST: check_malware_history("c2-server.evil")
    ST-->>TM: No malware detected (yet)

    %% Combined alert
    TM->>TM: create TrafficAlert{<br>  severity=Critical,<br>  type="C2_Communication",<br>  confidence=0.95,<br>  indicators=["Beaconing", "DGA"]<br>}
    TM->>Page: IPC: network_behavior_alert(alert_json)
    Page->>Page: Show critical alert: "C2 communication detected"
```

---

## 4. Component State Machines

### 4.1 TrafficMonitor State Machine

```mermaid
stateDiagram-v2
    [*] --> Disabled: Initial state (opt-out)
    Disabled --> Enabled: User enables in Settings
    Enabled --> Monitoring: start_request received
    Monitoring --> Analyzing: ≥5 requests to domain
    Analyzing --> Alerting: Anomaly detected (score > 0.6)
    Analyzing --> Monitoring: No anomaly (score < 0.6)
    Alerting --> PolicyCheck: Check PolicyGraph
    PolicyCheck --> Monitoring: Policy exists (allowed)
    PolicyCheck --> UserPrompt: No policy
    UserPrompt --> PolicyStored: User decision
    PolicyStored --> Monitoring: Continue monitoring
    Enabled --> PrivacyMode: User enables privacy mode
    PrivacyMode --> Enabled: User disables privacy mode
    Monitoring --> Disabled: User disables monitoring
    PrivacyMode --> Disabled: User disables monitoring

    note right of PrivacyMode
        Privacy Mode:
        - No PolicyGraph
        - No persistence
        - Limited tracking
    end note

    note right of Alerting
        Alert Types:
        - DGA
        - Beaconing
        - Exfiltration
        - DNS Tunneling
    end note
```

### 4.2 Connection Pattern State Machine

```mermaid
stateDiagram-v2
    [*] --> FirstRequest: record_request()
    FirstRequest --> Building: 1-4 requests
    Building --> Building: record_request()
    Building --> AnalysisReady: ≥5 requests
    AnalysisReady --> Analyzing: analyze_connection_pattern()
    Analyzing --> Benign: score < 0.3
    Analyzing --> Suspicious: 0.3 ≤ score < 0.6
    Analyzing --> Malicious: score ≥ 0.6
    Benign --> Building: continue monitoring
    Suspicious --> Building: continue monitoring
    Malicious --> Alerted: generate alert
    Alerted --> PolicyLookup: check PolicyGraph
    PolicyLookup --> Blocked: policy=block
    PolicyLookup --> Allowed: policy=allow
    PolicyLookup --> UserPrompt: no policy
    UserPrompt --> PolicySet: user decides
    PolicySet --> Blocked: user blocks
    PolicySet --> Allowed: user allows
    Blocked --> [*]: connection terminated
    Allowed --> Building: continue monitoring
    Building --> Expired: 1 hour retention limit
    Expired --> [*]: pattern deleted

    note right of AnalysisReady
        Minimum 5 requests needed
        for statistical significance
    end note

    note right of Malicious
        Score ≥ 0.6:
        - High beaconing (CV < 0.2)
        - High DGA (entropy > 3.5)
        - High exfiltration (ratio > 0.7)
    end note
```

---

## 5. Data Structure Diagrams

### 5.1 TrafficMonitor Memory Layout

```
TrafficMonitor
├── m_connection_patterns: HashMap<String, ConnectionPattern>
│   ├── "example.com" → ConnectionPattern {
│   │   request_count: 15,
│   │   bytes_sent: 50000,
│   │   bytes_received: 200000,
│   │   request_times: [t1, t2, ..., t15],
│   │   intervals: [Δt1, Δt2, ..., Δt14],
│   │   first_seen: 2025-11-01 12:00:00,
│   │   last_seen: 2025-11-01 12:15:00
│   │   }
│   ├── "api.github.com" → ConnectionPattern { ... }
│   └── ... (max 500 patterns, LRU eviction)
│
├── m_dns_queries: Vector<DNSQuery> (ring buffer, max 1000)
│   ├── [0] → DNSQuery { domain: "example.com", timestamp: t1 }
│   ├── [1] → DNSQuery { domain: "evil-dga.net", timestamp: t2 }
│   └── ...
│
├── m_alerted_domains: HashTable<String>
│   ├── "evil-dga.net"
│   ├── "c2-server.net"
│   └── ... (session-only, not persisted)
│
├── m_dns_analyzer: NonnullOwnPtr<DNSAnalyzer>
├── m_c2_detector: NonnullOwnPtr<C2Detector>
├── m_policy_graph: OwnPtr<PolicyGraph>  // nullable
├── m_monitoring_enabled: bool
├── m_privacy_mode: bool
└── m_stats: Statistics
```

### 5.2 ConnectionPattern Structure

```cpp
struct ConnectionPattern {
    String domain;                     // "example.com"
    u64 request_count;                 // Total requests
    u64 bytes_sent;                    // Total uploaded
    u64 bytes_received;                // Total downloaded
    Vector<UnixDateTime> request_times; // Timestamps (for interval analysis)
    Vector<Duration> intervals;        // Time between requests
    UnixDateTime first_seen;           // First request time
    UnixDateTime last_seen;            // Most recent request time
    bool is_websocket;                 // WebSocket connection flag

    // Computed properties
    Duration lifetime() const {
        return last_seen - first_seen;
    }

    float upload_ratio() const {
        auto total = bytes_sent + bytes_received;
        return total > 0 ? (float)bytes_sent / total : 0.0f;
    }

    float requests_per_minute() const {
        auto minutes = lifetime().to_seconds() / 60.0;
        return minutes > 0 ? request_count / minutes : 0.0f;
    }
};
```

### 5.3 PolicyGraph Schema (Network Behavior Policies)

```sql
-- Existing tables from previous milestones
-- policies, trusted_form_relationships, quarantine_entries

-- New table for network behavior policies
CREATE TABLE network_behavior_policies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    domain TEXT NOT NULL UNIQUE,              -- "evil-c2.net"
    action TEXT NOT NULL,                     -- "allow", "block", "alert"
    reason TEXT,                              -- "User allowed", "DGA detected", etc.
    confidence REAL,                          -- Detection confidence (0.0-1.0)
    alert_type TEXT,                          -- "DGA", "Beaconing", "Exfiltration", etc.
    created_at INTEGER NOT NULL,              -- Unix timestamp
    expires_at INTEGER,                       -- Optional expiration (NULL = never)
    user_created BOOLEAN DEFAULT 1            -- 1 = user decision, 0 = auto
);

CREATE INDEX idx_network_behavior_domain ON network_behavior_policies(domain);
CREATE INDEX idx_network_behavior_expires ON network_behavior_policies(expires_at);

-- Query examples
-- Insert user decision
INSERT INTO network_behavior_policies (domain, action, reason, created_at)
VALUES ('evil-c2.net', 'block', 'User blocked after C2 alert', 1730473200);

-- Check existing policy
SELECT action, reason FROM network_behavior_policies
WHERE domain = 'evil-c2.net' AND (expires_at IS NULL OR expires_at > 1730473200);

-- Cleanup expired policies
DELETE FROM network_behavior_policies WHERE expires_at IS NOT NULL AND expires_at < 1730473200;
```

---

## 6. Threading and Concurrency Model

### 6.1 Thread Architecture

```mermaid
graph TB
    subgraph "Main Thread (RequestServer Event Loop)"
        EventLoop[Core::EventLoop]
        IPC[IPC Handler]
        CurlMulti[curl_multi_socket_action]
    end

    subgraph "Background Thread Pool"
        Worker1[Analysis Worker 1]
        Worker2[Analysis Worker 2]
        Worker3[Analysis Worker 3]
        WorkerN[Analysis Worker N]
    end

    subgraph "TrafficMonitor (Main Thread)"
        TM[TrafficMonitor]
        RecordRequest[record_request]
        AnalyzeQueue[Analysis Task Queue]
    end

    subgraph "Analysis Components (Background Threads)"
        DNA[DNSAnalyzer]
        C2D[C2Detector]
        PG[PolicyGraph]
    end

    EventLoop --> IPC
    EventLoop --> CurlMulti
    IPC --> RecordRequest
    CurlMulti --> RecordRequest
    RecordRequest --> AnalyzeQueue

    AnalyzeQueue --> Worker1
    AnalyzeQueue --> Worker2
    AnalyzeQueue --> Worker3
    AnalyzeQueue --> WorkerN

    Worker1 --> DNA
    Worker2 --> C2D
    Worker3 --> PG
    WorkerN --> DNA

    DNA -.->|Result| TM
    C2D -.->|Result| TM
    PG -.->|Policy| TM

    style TM fill:#e1f5ff,stroke:#0066cc,stroke-width:2px
    style AnalyzeQueue fill:#fff4e1,stroke:#cc9900,stroke-width:2px
```

### 6.2 Concurrency Safety

| Component | Thread Safety | Synchronization |
|-----------|---------------|-----------------|
| **TrafficMonitor** | Main thread only | No locks needed (event loop) |
| **m_connection_patterns** | Main thread only | No locks needed |
| **m_dns_queries** | Main thread only | No locks needed |
| **DNSAnalyzer** | Thread-safe (stateless) | No shared state |
| **C2Detector** | Thread-safe (stateless) | No shared state |
| **PolicyGraph** | Thread-safe (SQLite) | SQLite internal locking |
| **Analysis Task Queue** | Multi-producer, multi-consumer | Mutex + condition variable |

**Design Principle**: Minimize locking by keeping TrafficMonitor state in main thread and using stateless analyzers in background threads.

### 6.3 Async Analysis Flow

```cpp
// Main thread (event loop)
void TrafficMonitor::record_request(RequestMetadata const& metadata)
{
    // Update connection pattern (fast, main thread)
    auto& pattern = m_connection_patterns.ensure(metadata.url.host()->serialize());
    pattern.request_count++;
    pattern.bytes_sent += metadata.bytes_sent;
    pattern.bytes_received += metadata.bytes_received;
    pattern.request_times.append(metadata.start_time);

    // Queue async analysis (if enough data)
    if (pattern.request_count >= 5) {
        enqueue_analysis_task([this, domain = metadata.url.host()->serialize()]() {
            // Background thread
            auto analysis = m_c2_detector->analyze_connection_pattern(
                domain,
                get_pattern(domain).request_count,
                get_pattern(domain).bytes_sent,
                get_pattern(domain).bytes_received,
                get_pattern(domain).request_times
            );

            // Return to main thread for alert
            Core::EventLoop::current().post_event([this, domain, analysis]() {
                on_analysis_complete(domain, analysis);
            });
        });
    }
}
```

---

## 7. Performance Optimization Strategies

### 7.1 Caching Strategy

```mermaid
graph LR
    subgraph "TrafficMonitor Cache"
        Cache[DGA Score Cache]
        Entry1["example.com → 0.1 (5 min TTL)"]
        Entry2["github.com → 0.0 (5 min TTL)"]
        EntryN["evil-dga.net → 0.9 (5 min TTL)"]
    end

    Request[New DNS Query] -->|Check cache| Cache
    Cache -->|Cache hit| Return[Return cached score]
    Cache -->|Cache miss| Analyze[Run DNSAnalyzer]
    Analyze -->|Store result| Cache
    Analyze -->|Return| Return

    style Cache fill:#f0f0f0,stroke:#666,stroke-width:2px
    style Return fill:#e1ffe1,stroke:#00cc00,stroke-width:2px
    style Analyze fill:#ffe1e1,stroke:#cc0000,stroke-width:2px
```

**Cache Implementation**:

```cpp
class TrafficMonitor {
private:
    struct CachedAnalysis {
        DNSAnalyzer::DGAAnalysis dga;
        UnixDateTime cached_at;
        static constexpr Duration TTL = Duration::from_seconds(300);  // 5 minutes

        bool is_expired() const {
            return (UnixDateTime::now() - cached_at) > TTL;
        }
    };

    HashMap<String, CachedAnalysis> m_analysis_cache;

    ErrorOr<DNSAnalyzer::DGAAnalysis> get_or_analyze_dga(StringView domain)
    {
        // Check cache
        if (auto cached = m_analysis_cache.get(domain)) {
            if (!cached->is_expired()) {
                m_stats.cache_hits++;
                return cached->dga;
            }
            m_analysis_cache.remove(domain);  // Expired
        }

        // Cache miss - analyze
        m_stats.cache_misses++;
        auto analysis = TRY(m_dns_analyzer->analyze_for_dga(domain));
        m_analysis_cache.set(domain, CachedAnalysis{
            .dga = analysis,
            .cached_at = UnixDateTime::now()
        });

        return analysis;
    }
};
```

### 7.2 Sampling Strategy (High-Traffic Sites)

```cpp
void TrafficMonitor::record_request(RequestMetadata const& metadata)
{
    auto domain = metadata.url.host()->serialize();
    auto& pattern = m_connection_patterns.ensure(domain);

    // Always record basic stats
    pattern.request_count++;
    pattern.bytes_sent += metadata.bytes_sent;
    pattern.bytes_received += metadata.bytes_received;

    // High-traffic site detection (>100 requests/min)
    if (pattern.requests_per_minute() > 100.0f) {
        // Sample 10% of requests for analysis
        if (AK::get_random_uniform(100) < 10) {
            pattern.request_times.append(metadata.start_time);
        }
        // Note: Still track byte counts for all requests
    } else {
        // Normal traffic - track all requests
        pattern.request_times.append(metadata.start_time);
    }
}
```

### 7.3 Memory Management (LRU Eviction)

```cpp
void TrafficMonitor::record_request(RequestMetadata const& metadata)
{
    auto domain = metadata.url.host()->serialize();

    // Check capacity
    if (m_connection_patterns.size() >= MaxConnectionPatterns) {
        // Evict least recently used (LRU)
        auto oldest_domain = find_oldest_pattern();
        m_connection_patterns.remove(oldest_domain);
    }

    auto& pattern = m_connection_patterns.ensure(domain);
    pattern.last_seen = UnixDateTime::now();  // Update LRU timestamp
    // ... rest of recording
}

String TrafficMonitor::find_oldest_pattern() const
{
    String oldest_domain;
    UnixDateTime oldest_time = UnixDateTime::now();

    for (auto const& [domain, pattern] : m_connection_patterns) {
        if (pattern.last_seen < oldest_time) {
            oldest_time = pattern.last_seen;
            oldest_domain = domain;
        }
    }

    return oldest_domain;
}
```

---

## 8. Error Handling and Graceful Degradation

### 8.1 Initialization Failure Handling

```cpp
// RequestServer/ConnectionFromClient.cpp
ConnectionFromClient::ConnectionFromClient(NonnullOwnPtr<IPC::Transport> transport)
{
    // Try to create TrafficMonitor
    auto traffic_monitor = TrafficMonitor::create();
    if (traffic_monitor.is_error()) {
        dbgln("Warning: Failed to initialize TrafficMonitor: {}", traffic_monitor.error());
        m_traffic_monitor = nullptr;
        // Browser continues working normally without network monitoring
    } else {
        m_traffic_monitor = traffic_monitor.release_value();
        dbgln("TrafficMonitor initialized successfully");
    }
}

// Sentinel/DNSAnalyzer.cpp
ErrorOr<NonnullOwnPtr<DNSAnalyzer>> DNSAnalyzer::create()
{
    auto analyzer = adopt_nonnull_own_or_enomem(new (nothrow) DNSAnalyzer());
    if (!analyzer)
        return Error::from_errno(ENOMEM);

    // Load popular domains whitelist
    auto popular = TRY(load_popular_domains());
    if (popular.is_empty()) {
        dbgln("Warning: Failed to load popular domains whitelist");
        // Continue with empty whitelist (may have more false positives)
    }

    return analyzer;
}
```

### 8.2 Analysis Failure Handling

```cpp
void TrafficMonitor::analyze_domain(StringView domain)
{
    // DNS analysis
    auto dga_result = m_dns_analyzer->analyze_for_dga(domain);
    if (dga_result.is_error()) {
        dbgln("Warning: DGA analysis failed for '{}': {}", domain, dga_result.error());
        // Continue without DGA detection for this domain
        return;
    }

    auto dga_analysis = dga_result.release_value();

    // Only proceed if suspicious
    if (dga_analysis.dga_score < 0.6f)
        return;

    // Generate alert (even if some analysis failed)
    TrafficAlert alert {
        .severity = TrafficAlert::Severity::High,
        .alert_type = "DGA"_string,
        .domain = String::from_utf8(domain).release_value_but_fixme_should_propagate_errors(),
        .explanation = dga_analysis.explanation,
        .confidence = dga_analysis.dga_score,
        .detected_at = UnixDateTime::now()
    };

    if (m_alert_callback)
        m_alert_callback(alert);
}
```

---

## 9. Testing Strategy Architecture

### 9.1 Unit Test Architecture

```mermaid
graph TB
    subgraph "Sentinel Unit Tests"
        TestDNS[TestDNSAnalyzer.cpp]
        TestC2[TestC2Detector.cpp]
    end

    subgraph "RequestServer Unit Tests"
        TestTM[TestTrafficMonitor.cpp]
    end

    subgraph "Mock Components"
        MockPG[MockPolicyGraph]
        MockCFC[MockConnectionFromClient]
    end

    TestDNS -->|Test| DNSAnalyzer[DNSAnalyzer]
    TestC2 -->|Test| C2Detector[C2Detector]
    TestTM -->|Test| TrafficMonitor[TrafficMonitor]

    TestTM -->|Uses| MockPG
    TestTM -->|Uses| MockCFC

    DNSAnalyzer -->|No deps| None1[Stateless]
    C2Detector -->|No deps| None2[Stateless]

    style TestDNS fill:#e1ffe1,stroke:#00cc00,stroke-width:2px
    style TestC2 fill:#e1ffe1,stroke:#00cc00,stroke-width:2px
    style TestTM fill:#e1ffe1,stroke:#00cc00,stroke-width:2px
```

### 9.2 Integration Test Scenarios

| Test Scenario | Components | Expected Result |
|---------------|------------|-----------------|
| **DGA Detection** | TrafficMonitor + DNSAnalyzer | Alert generated, score > 0.7 |
| **Beaconing Detection** | TrafficMonitor + C2Detector | Alert after 5 regular requests |
| **Exfiltration Detection** | TrafficMonitor + C2Detector | Alert when upload ratio > 0.7 |
| **Whitelist Enforcement** | DNSAnalyzer + PolicyGraph | No alert for google.com, github.com |
| **Privacy Mode** | TrafficMonitor | No PolicyGraph writes |
| **Alert Deduplication** | TrafficMonitor | Only 1 alert per domain per session |
| **Cache Effectiveness** | TrafficMonitor | >80% cache hit rate |
| **Graceful Degradation** | All components | Browser works if monitoring fails |

---

## 10. Deployment Architecture

### 10.1 Configuration Files

```
/etc/ladybird/
├── sentinel.conf                # Sentinel daemon config
└── network-monitoring.conf      # TrafficMonitor config

~/.config/ladybird/
├── policies.db                  # PolicyGraph SQLite database
└── network-monitoring-prefs.json  # User preferences

/usr/share/ladybird/
├── popular-domains.txt          # Top 1000 domains whitelist
└── upload-services.txt          # Known upload services (CDNs, cloud)
```

**network-monitoring.conf**:

```ini
[TrafficMonitor]
enabled=true
privacy_mode=false
max_connection_patterns=500
max_dns_queries=1000
pattern_retention_hours=1
cache_ttl_seconds=300

[DNSAnalyzer]
dga_entropy_threshold=3.5
dga_consonant_threshold=0.6
tunneling_length_threshold=50
tunneling_frequency_threshold=10

[C2Detector]
beaconing_regularity_threshold=0.2
exfiltration_upload_ratio_threshold=0.7
exfiltration_min_bytes=10485760
port_scan_port_threshold=5
```

### 10.2 Resource Limits

| Resource | Limit | Rationale |
|----------|-------|-----------|
| Memory | <1MB per RequestServer | 500 patterns + 1000 DNS queries |
| CPU | <1% sustained | Async analysis, background threads |
| Disk | <10MB (PolicyGraph) | SQLite database |
| Network | 0 bytes | Local-only analysis |
| File descriptors | +0 | No additional files |

---

## Conclusion

This architecture provides a comprehensive, privacy-preserving network behavioral analysis system that:
- Detects DGA domains, beaconing, exfiltration, and DNS tunneling
- Operates entirely on-device with no external communication
- Maintains <5% performance overhead through async analysis and caching
- Integrates seamlessly with existing Sentinel components
- Supports graceful degradation if monitoring fails or is disabled

The design follows established Ladybird patterns and is ready for implementation in Phase 6A-6E.

---

*Companion Document*: PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md
*Version*: 1.0
*Date*: 2025-11-01
