# Sentinel Milestone 0.4 - Machine Learning & Advanced Detection

**Status**: ✅ Complete (Phase 1-5 finished)
**Target**: Milestone 0.4
**Estimated Duration**: 4-6 weeks
**Prerequisites**: Milestone 0.3 Complete ✅

## Overview

Milestone 0.4 focuses on advanced threat detection using machine learning, behavioral analysis, and federated threat intelligence.

## Goals

1. ML-based malware detection (replace/augment YARA rules)
2. Federated threat intelligence sharing
3. Browser fingerprinting detection
4. Advanced phishing URL analysis
5. Network traffic behavioral analysis

## Proposed Features

### Feature 1: ML-Based Malware Detection

**Objective**: Use machine learning to detect malware patterns without relying solely on YARA rules.

**Approach**:
- Lightweight on-device ML model (TensorFlow Lite or ONNX Runtime)
- Feature extraction: file size, entropy, PE header analysis, string patterns
- Binary classification: malicious vs. benign
- Complement existing YARA engine (ensemble approach)

**Benefits**:
- Detect zero-day malware
- Reduce false positives
- Adapt to evolving threats

### Feature 2: Federated Threat Intelligence

**Objective**: Share threat indicators across Ladybird instances while preserving privacy.

**Approach**:
- Federated learning: train models locally, share only gradients
- Decentralized threat feed (IPFS-based)
- Privacy-preserving bloom filters for known threats
- Opt-in participation

**Benefits**:
- Community-driven threat detection
- No central authority required
- Privacy-preserving by design

### Feature 3: Browser Fingerprinting Detection

**Objective**: Detect and block aggressive browser fingerprinting attempts.

**Approach**:
- Monitor canvas fingerprinting, WebGL queries, audio context
- Detect excessive navigator property access
- Track font enumeration attempts
- Score fingerprinting aggressiveness (0.0-1.0)

**Benefits**:
- Enhanced privacy protection
- Reduce tracking surface
- User transparency

### Feature 4: Advanced Phishing URL Analysis

**Objective**: Deep analysis of URLs to detect sophisticated phishing attempts.

**Approach**:
- Homograph attack detection (Unicode lookalikes)
- Levenshtein distance from popular domains
- SSL certificate validation and trust chain analysis
- URL structure entropy analysis
- Integration with Google Safe Browsing API (optional)

**Benefits**:
- Catch sophisticated phishing beyond keyword matching
- Protect against typosquatting
- Real-time URL reputation

### Feature 5: Network Traffic Behavioral Analysis

**Objective**: Zeek-style network traffic inspection for anomaly detection.

**Approach**:
- Passive traffic monitoring (no TLS decryption by default)
- Connection pattern analysis (frequency, volume, destinations)
- DNS query anomalies (DGA detection, DNS tunneling)
- Port scanning detection
- C2 communication patterns

**Benefits**:
- Detect malware communication
- Identify data exfiltration
- Network-level threat visibility

## Technical Considerations

### Performance
- ML inference must be <100ms per file
- Federated learning updates: background, low priority
- Fingerprinting detection: minimal overhead

### Privacy
- All ML training on-device
- Federated learning: differential privacy
- No PII in threat feeds
- User opt-in for all features

### Dependencies
- TensorFlow Lite or ONNX Runtime
- IPFS node (optional, for federated feeds)
- Bloom filter library
- URL parsing and normalization

## Implementation Phases

### Phase 1: ML Infrastructure (Week 1-2) ✅ **COMPLETE**
- ✅ Feature extraction pipeline (6-dimensional vector)
- ✅ Model loading infrastructure (prepared for TFLite)
- ✅ Heuristic-based inference (production-ready)
- ✅ Integration with SentinelServer
- ✅ Performance benchmarking (~1ms avg inference)
- 📝 TensorFlow Lite integration documented (deferred - see TENSORFLOW_LITE_INTEGRATION.md)

**Deliverables**:
- `Services/Sentinel/MalwareML.{h,cpp}` - ML detector implementation
- `Services/Sentinel/TestMalwareML.cpp` - Comprehensive test suite (6/6 passing)
- Integrated with Sentinel's scan_content() pipeline
- Enhanced JSON response with ML predictions

### Phase 2: Malware ML Model (Week 2-3) ✅ **COMPLETE**
- ✅ ML infrastructure with TensorFlow Lite detection
- ✅ ML stub fallback (simulated neural network)
- ✅ Feature engineering (entropy, PE headers, strings, code ratio)
- ✅ MalwareML detector with heuristic-based scoring
- ✅ All 6 TestMalwareML tests passing (ML stub initialization fixed)

**Deliverables**:
- `Services/Sentinel/MalwareML.{h,cpp}` - Production-ready ML detector
- `Services/Sentinel/TestMalwareML.cpp` - Complete test suite (6/6 passing)
- ML stub weight initialization bug resolved
- Statistics struct corruption fixed
- Heuristic-based detection as TFLite fallback

### Phase 3: Federated Intelligence (Week 3-4) ✅ **COMPLETE**
- ✅ IPFS integration for threat feeds (mock mode + real IPFS support)
- ✅ Bloom filter threat cache (100M capacity, differential privacy)
- ✅ Federated learning with Laplace/Gaussian noise
- ✅ Privacy-preserving gradient aggregation (ε=0.1, k-anonymity)
- ✅ TestBloomFilter: 10/10 tests passed

### Phase 4: Fingerprinting Detection (Week 4-5) ✅ **COMPLETE**
- ✅ Canvas/WebGL monitoring hooks (27 total hooks implemented)
- ✅ Navigator property access tracking (6 hooks)
- ✅ Scoring algorithm (threshold-based detection)
- ✅ IPC alert integration (PageClient.cpp)
- 📝 about:privacy dashboard (deferred to UI integration phase)

**Deliverables**:
- `Services/Sentinel/FingerprintingDetector.{h,cpp}` - Core detection engine
- `Services/Sentinel/TestFingerprintingDetector.cpp` - Test suite (10/10 passing)
- WebGL hooks: `WebGLRenderingContextImpl.cpp`, `WebGLRenderingContext.cpp`, `WebGL2RenderingContext.cpp` (12 hooks)
- Audio hooks: `BaseAudioContext.cpp`, `AnalyserNode.cpp` (4 hooks)
- Navigator hooks: `NavigatorID.cpp`, `Navigator.cpp`, `NavigatorLanguage.cpp`, `NavigatorConcurrentHardware.cpp` (6 hooks)
- Canvas hooks: `HTMLCanvasElement.cpp`, `CanvasRenderingContext2D.cpp` (3 existing hooks)
- IPC integration: `PageClient.{h,cpp}`, `PageHost.ipc` (1 alert mechanism)
- 20 files modified total
- Completion date: 2025-11-01

### Phase 5: Phishing URL Analysis (Week 5-6) ✅ **COMPLETE**
- ✅ Unicode homograph detection (ICU spoofchecker integration)
- ✅ Domain similarity scoring (Levenshtein distance, 100 popular domains)
- ✅ Suspicious TLD detection (free/unverified TLDs)
- ✅ Shannon entropy analysis for random domains
- ✅ Multi-factor weighted scoring system
- 📝 SSL certificate validation (deferred to Phase 6)
- 📝 URL reputation integration (deferred to Phase 6)

**Deliverables**:
- `Services/Sentinel/PhishingURLAnalyzer.{h,cpp}` - Complete URL analysis engine
- `Services/Sentinel/TestPhishingURLAnalyzer.cpp` - Comprehensive test suite (7/7 passing)
- ICU library integration for Unicode confusable detection
- Production-ready phishing detection with human-readable explanations

### Phase 6: Network Behavioral Analysis (Week 6+)
- Traffic pattern monitoring
- DNS query analysis
- C2 detection heuristics
- Integration with RequestServer

## Success Criteria

1. ML model achieves >95% detection rate with <5% false positives
2. Federated intelligence shares threats without exposing user data
3. Fingerprinting detection scores correlate with actual tracking attempts
4. Phishing URL analysis catches Unicode homograph attacks
5. Network analysis detects C2 communication patterns
6. All features opt-in with clear privacy explanations
7. Performance impact <5% on page load times

## Risks and Mitigations

### Risk: ML Model Size and Performance
**Mitigation**: Use quantized models, lazy loading, background inference

### Risk: Privacy Concerns with Federated Learning
**Mitigation**: Differential privacy, open-source implementation, third-party audit

### Risk: False Positives Degrading UX
**Mitigation**: Conservative thresholds, user feedback loop, continuous tuning

### Risk: Fingerprinting Detection Overhead
**Mitigation**: Sampling-based monitoring, lightweight scoring, async processing

## Dependencies

- Milestone 0.3 must be complete
- ML framework integrated (TensorFlow Lite or ONNX)
- IPFS node available (for federated features)
- Training dataset access (VirusTotal, PhishTank)

## Future Considerations (Milestone 0.5+)

- Deep packet inspection with TLS decryption (user opt-in)
- Real-time sandboxing for suspicious downloads
- Integration with SIEM systems
- Browser extension ecosystem for custom inspectors
- Mobile platform support

---

*Document version: 1.0*
*Created: 2025-10-31*
*Ladybird Sentinel - Advanced Threat Detection Roadmap*
