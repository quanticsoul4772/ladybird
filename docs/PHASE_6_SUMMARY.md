# Phase 6: Network Behavioral Analysis - Architecture Summary

**Version**: 1.0
**Date**: 2025-11-01
**Status**: Architecture Design Complete - Ready for Implementation

---

## Quick Reference

### Documents
1. **[PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md](PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md)** - Main architecture design (39KB)
2. **[PHASE_6_ARCHITECTURE_DIAGRAMS.md](PHASE_6_ARCHITECTURE_DIAGRAMS.md)** - Detailed diagrams and flows (28KB)

### Key Components

| Component | Location | Lines of Code (Est.) | Complexity |
|-----------|----------|----------------------|------------|
| **TrafficMonitor** | `Services/RequestServer/TrafficMonitor.{h,cpp}` | ~800 LOC | Medium |
| **DNSAnalyzer** | `Services/Sentinel/DNSAnalyzer.{h,cpp}` | ~600 LOC | Medium |
| **C2Detector** | `Services/Sentinel/C2Detector.{h,cpp}` | ~600 LOC | Medium |
| **IPC Integration** | `Services/RequestServer/RequestClient.ipc` | ~10 LOC | Low |
| **Unit Tests** | `Services/Sentinel/Test*.cpp` | ~1200 LOC | Low |
| **Total** | | **~3210 LOC** | |

---

## Architecture at a Glance

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Web Page (WebContent)                     │
│                  Initiates network requests                  │
└─────────────────────┬───────────────────────────────────────┘
                      │ IPC: start_request()
                      ↓
┌─────────────────────────────────────────────────────────────┐
│              RequestServer (Per WebContent)                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  ConnectionFromClient                               │   │
│  │    └→ TrafficMonitor (Orchestrator)                 │   │
│  │         ├→ record_request()      [Main Thread]      │   │
│  │         ├→ record_dns_query()    [Main Thread]      │   │
│  │         └→ analyze_domain()      [Async]            │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────────┘
                      │ Analysis Tasks
                      ↓
┌─────────────────────────────────────────────────────────────┐
│              Sentinel Components (Background)                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────┐   │
│  │  DNSAnalyzer    │  │   C2Detector    │  │ Policy   │   │
│  │  ├─ DGA         │  │  ├─ Beaconing   │  │  Graph   │   │
│  │  └─ Tunneling   │  │  └─ Exfil       │  │  (SQLite)│   │
│  └─────────────────┘  └─────────────────┘  └──────────┘   │
└─────────────────────┬───────────────────────────────────────┘
                      │ Analysis Results
                      ↓
┌─────────────────────────────────────────────────────────────┐
│                    Alert UI (WebContent)                     │
│         "Suspicious network behavior detected"               │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Detection Algorithms

### 1. DGA Detection (DNS Generation Algorithm)

**Input**: Domain name (e.g., "xk3j9f2l.com")

**Algorithm**:
```
1. Calculate Shannon entropy: H(X) = -Σ p(x) log2 p(x)
2. Calculate consonant/vowel ratios
3. Calculate N-gram score (bigram/trigram frequency)
4. Check against whitelist (popular domains)
5. Score = weighted combination (0.0-1.0)
```

**Thresholds**:
- Entropy > 3.5 → +0.4 score
- Consonants > 60% → +0.3 score
- Vowels < 20% or > 60% → +0.2 score
- **Score > 0.7 = DGA detected**

**Performance**: <1ms per domain

---

### 2. Beaconing Detection (C2 Communication)

**Input**: List of request timestamps [t₁, t₂, ..., tₙ]

**Algorithm**:
```
1. Calculate intervals: Δt = [t₂-t₁, t₃-t₂, ..., tₙ-tₙ₋₁]
2. Calculate mean interval: μ = mean(Δt)
3. Calculate standard deviation: σ = stddev(Δt)
4. Calculate coefficient of variation: CV = σ / μ
5. Score = 1.0 - CV (lower CV = more regular)
```

**Thresholds**:
- CV < 0.2 → Highly regular (score = 0.8-1.0)
- CV < 0.5 → Somewhat regular (score = 0.5-0.8)
- **Score > 0.7 = Beaconing detected**

**Minimum Sample Size**: 5 requests

**Performance**: <2ms per analysis

---

### 3. Data Exfiltration Detection

**Input**: Bytes sent, bytes received

**Algorithm**:
```
1. Calculate upload ratio: R = bytes_sent / (bytes_sent + bytes_received)
2. Check if known upload service (AWS, GCP, Dropbox, etc.)
3. Check volume threshold (> 10MB)
4. Score = R * volume_factor * (!known_service ? 1.0 : 0.5)
```

**Thresholds**:
- Upload ratio > 0.7 → +0.4 score
- Upload ratio > 0.9 → +0.3 score (total 0.7)
- Volume > 100MB → +0.3 score (total 1.0)
- **Score > 0.7 = Exfiltration suspected**

**Performance**: <0.5ms per analysis

---

## Privacy Guarantees

| Privacy Principle | Implementation |
|-------------------|----------------|
| **Opt-In** | Disabled by default, user must enable in Settings |
| **Local-Only** | All analysis on-device, no external servers |
| **Minimal Data** | Domain, byte counts, timestamps ONLY (no URLs, headers, bodies) |
| **Data Retention** | 1 hour maximum, ring buffers with fixed capacity |
| **Privacy Mode** | Strict mode: no persistence, no PolicyGraph writes |
| **No PII** | No IP addresses, user agents, cookies, or user identity |
| **Anonymization** | Session-only alerts, no long-term tracking |

**Data Minimization**:
- ✅ Tracks: Domain names, byte counts, request timestamps
- ❌ Does NOT track: Full URLs, headers, bodies, IP addresses, cookies

---

## Performance Budget

| Metric | Target | Maximum |
|--------|--------|---------|
| **Per-Request Overhead** | <1ms | <5ms |
| **Memory Footprint** | ~500KB | <1MB |
| **CPU Usage** | <1% sustained | <5% burst |
| **Page Load Impact** | <1% | <5% |
| **Analysis Latency** | <2ms average | <10ms max |
| **Cache Hit Rate** | >80% | >70% |

**Optimization Strategies**:
1. Async analysis (background threads)
2. Caching (5-minute TTL)
3. Sampling (10% for high-traffic sites)
4. Lazy evaluation (≥5 requests before analysis)
5. LRU eviction (max 500 patterns)

---

## Implementation Phases

### Phase 6A: Foundation (Week 1)
**Focus**: TrafficMonitor core infrastructure

**Deliverables**:
- `TrafficMonitor.{h,cpp}` - Event aggregation, alert orchestration
- Request lifecycle hooks (record_request)
- IPC message definitions
- Unit tests (pattern tracking, DNS ring buffer)

**Success Criteria**: All requests tracked, no performance regression

---

### Phase 6B: DNS Analysis (Week 2)
**Focus**: DGA and DNS tunneling detection

**Deliverables**:
- `DNSAnalyzer.{h,cpp}` - DGA detection, entropy analysis
- Popular domain whitelist (top 1000)
- Integration with TrafficMonitor
- Unit tests (DGA detection, whitelist enforcement)

**Success Criteria**: Detect known DGA families with >90% accuracy

---

### Phase 6C: C2 Detection (Week 3)
**Focus**: Beaconing and exfiltration detection

**Deliverables**:
- `C2Detector.{h,cpp}` - Beaconing, exfiltration, port scanning
- Known upload service whitelist
- Integration with TrafficMonitor
- Unit tests (beaconing, exfiltration)

**Success Criteria**: Detect known C2 patterns with >85% accuracy

---

### Phase 6D: PolicyGraph & UI (Week 4)
**Focus**: User interaction and policy management

**Deliverables**:
- PolicyGraph schema updates (network_behavior_policies table)
- Alert UI in WebContent
- Settings UI (enable/disable, privacy mode)
- about:network-monitor dashboard (optional)

**Success Criteria**: User can allow/block domains, policies persist

---

### Phase 6E: Testing & Documentation (Week 5)
**Focus**: End-to-end testing and documentation

**Deliverables**:
- Browser tests (DGA, beaconing, exfiltration scenarios)
- Performance benchmarking (<5% overhead)
- User documentation (USER_GUIDE_NETWORK_MONITORING.md)
- Developer documentation updates

**Success Criteria**: All tests pass, performance budget met, false positive rate <1%

---

## Integration Points

### Modified Files

| File | Changes | Complexity |
|------|---------|------------|
| `Services/RequestServer/ConnectionFromClient.{h,cpp}` | Add TrafficMonitor instance, alert callback | Low |
| `Services/RequestServer/Request.{h,cpp}` | Add lifecycle hooks (on_headers_received, etc.) | Low |
| `Services/RequestServer/RequestClient.ipc` | Add network_behavior_alert message | Trivial |
| `Services/RequestServer/CMakeLists.txt` | Add TrafficMonitor compilation | Trivial |
| `Services/Sentinel/CMakeLists.txt` | Add DNSAnalyzer, C2Detector compilation | Trivial |

### New Files

| File | Purpose | LOC |
|------|---------|-----|
| `Services/RequestServer/TrafficMonitor.{h,cpp}` | Event aggregation, orchestration | ~800 |
| `Services/Sentinel/DNSAnalyzer.{h,cpp}` | DGA and DNS tunneling detection | ~600 |
| `Services/Sentinel/C2Detector.{h,cpp}` | Beaconing and exfiltration detection | ~600 |
| `Services/Sentinel/TestDNSAnalyzer.cpp` | DNS analyzer unit tests | ~400 |
| `Services/Sentinel/TestC2Detector.cpp` | C2 detector unit tests | ~400 |
| `Services/RequestServer/TestTrafficMonitor.cpp` | Traffic monitor unit tests | ~400 |

---

## Success Criteria

### Functional Requirements
✅ DGA detection: >90% accuracy on known DGA families
✅ Beaconing detection: >85% accuracy on known C2 patterns
✅ Exfiltration detection: >80% accuracy on known malware
✅ False positive rate: <1% on top 1000 websites
✅ Privacy mode: Zero persistence, zero external communication
✅ Graceful degradation: Browser works if monitoring disabled/fails

### Performance Requirements
✅ Per-request overhead: <1ms average
✅ Page load impact: <5%
✅ Memory footprint: <1MB per RequestServer
✅ CPU usage: <1% sustained
✅ Async analysis: No blocking of network requests

### Security Requirements
✅ No sensitive data logged (URLs, headers, bodies)
✅ No external network communication
✅ Opt-in by default
✅ PolicyGraph integration for user decisions
✅ Alert rate limiting (max 1 per domain per session)

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| High false positives | High | Conservative thresholds, extensive whitelist, user feedback |
| Performance degradation | Medium | Async analysis, caching, sampling, monitoring |
| Privacy concerns | High | Opt-in, local-only, minimal data, privacy mode |
| Evasion techniques | Medium | Continuous improvement, ML (future), threat feeds |
| User alert fatigue | Medium | Deduplication, severity filtering, PolicyGraph |

---

## Next Steps

1. **Review** architecture with stakeholders (security team, privacy team)
2. **Prepare** development environment (test datasets, DGA samples)
3. **Implement** Phase 6A (TrafficMonitor foundation)
4. **Test** with real C2/DGA samples
5. **Iterate** based on user feedback

---

## References

### Related Documents
- [MILESTONE_0.4_PLAN.md](MILESTONE_0.4_PLAN.md) - Milestone overview
- [MILESTONE_0.4_TECHNICAL_SPECS.md](MILESTONE_0.4_TECHNICAL_SPECS.md) - Technical specifications
- [FINGERPRINTING_DETECTION_ARCHITECTURE.md](FINGERPRINTING_DETECTION_ARCHITECTURE.md) - Phase 4 architecture
- [PHISHING_DETECTION_ARCHITECTURE.md](PHISHING_DETECTION_ARCHITECTURE.md) - Phase 5 architecture

### External Resources
- DGA Detection Research (2025): 97% accuracy using data/control plane analysis
- C2 Communication Patterns: Beaconing, DNS tunneling, exfiltration
- Privacy-Preserving Analytics: Local-only, minimal data collection

---

**Status**: Architecture Design Complete ✅
**Estimated Implementation Time**: 5 weeks (Phases 6A-6E)
**Total Code**: ~3210 lines of code (excluding tests)
**Memory Budget**: <1MB per RequestServer process
**Performance Budget**: <5% page load impact

---

*Created: 2025-11-01*
*Ladybird Browser - Sentinel Phase 6 Architecture Summary*
