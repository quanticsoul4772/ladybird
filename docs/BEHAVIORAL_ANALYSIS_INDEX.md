# Behavioral Analysis Documentation Index

**Milestone 0.5 Phase 1: Real-time Sandboxing**

Complete documentation suite for malicious file behavior detection in isolated sandbox environments.

---

## Document Overview

### 1. BEHAVIORAL_ANALYSIS_SPEC.md (40 KB, 1527 lines)
**Primary Specification Document**

Comprehensive technical specification defining all behavioral metrics, detection logic, and integration architecture.

**Contents**:
- **Overview & Objectives**: Goals, scope, analysis pipeline
- **Behavioral Metrics** (5 categories): File I/O, Process, Memory, Registry, Platform-specific
- **Scoring Algorithm**: Multi-factor scoring, verdict thresholds, confidence calculation
- **Detection Rules**: 6 explicit if-then rules for malware families (ransomware, stealers, cryptominers, rootkits, worms, downloaders)
- **Platform-Specific Implementations**: Windows, Linux, macOS-specific behaviors and detection methods
- **Integration Architecture**: YARA + ML + Behavioral combination, PolicyGraph caching, flow diagrams
- **Performance Considerations**: 5-second budget breakdown, optimization strategies, resource usage
- **Testing & Validation**: Test cases, metrics, benchmarks
- **Appendix**: Reference thresholds table, scoring weights, whitelists

**Target Audience**: Architects, security researchers, lead implementers

**When to Use**: Understanding full specification, design review, reference implementation

---

### 2. BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md (13 KB, 395 lines)
**Quick Start Guide**

Concise reference for developers implementing the system. Covers key concepts, critical metrics, scoring math, and decision logic.

**Contents**:
- **Key Concepts**: What behavioral analysis is, why three detection methods
- **Critical Metrics Table**: 8 HIGH/CRITICAL risk patterns with examples
- **Scoring Quick Math**: Three-factor score formula with ransomware example
- **Implementation Roadmap**: 4-week phase breakdown with file names
- **Platform-Specific Focus Areas**: Windows/Linux/macOS priority metrics
- **False Positive Prevention**: Whitelists, certificate verification
- **Performance Targets**: Critical metrics (5s budget, <1% FP)
- **Decision Tree**: Is this file suspicious? (flowchart)
- **Common Pitfalls**: 5 mistakes to avoid with solutions

**Target Audience**: Developers, QA engineers, new team members

**When to Use**: Quick understanding, implementation reference, debugging decisions

---

### 3. BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md (20 KB, 646 lines)
**C++ API Reference**

Implementation-ready C++ interfaces following Ladybird coding style.

**Contents**:
- **Component Architecture**: Visual structure of monitors
- **BehavioralAnalyzer**: Core interface with full class definition
  - Verdict/Scoring types
  - Analysis methods (file, with timeout)
  - Individual metric analysis methods
  - Event structures (File, Process, Memory, Registry, Platform)
  - Statistics tracking
- **FileIOMonitor.h**: File event monitoring interface
- **Integration Point**: SecurityTap update code example
- **Testing Framework**: TestBehavioralAnalyzer.cpp examples with LibTest
- **Performance Optimization**: Parallel analysis, early termination
- **Platform-Specific Notes**: Windows ETW/WMI, Linux strace/seccomp, macOS FSEvents
- **Graceful Degradation**: Error handling and fallback strategies
- **Integration Checklist**: 13-item task list for implementation

**Target Audience**: C++ developers, systems programmers

**When to Use**: Implementation sprint, API design review, testing framework setup

---

## Key Insights and Design Decisions

### Three-Layer Detection (40-35-25 weights)

```
YARA (40%): Signature matching
  ├─ Pros: High confidence, zero false positives if matched
  └─ Cons: Only detects known malware

ML (35%): Neural network prediction
  ├─ Pros: Good accuracy on zero-days (especially ransomware)
  └─ Cons: ~10-15% false positive rate in isolation

Behavioral (25%): Runtime patterns [NEW]
  ├─ Pros: Detects novel exploits, persistence mechanisms
  └─ Cons: Requires sandbox execution (5 second delay)
```

**Why this weighting?**
- YARA highest (definitive if match)
- ML high (proven 80%+ accuracy)
- Behavioral moderate (emerging technique, valuable for unknowns)

### Critical Risk Thresholds

**CRITICAL behaviors** (score 0.7-0.95 each):
1. **Credential access** (SAM, keychain, SSH keys) → 0.90
2. **Security bypass** (Defender disable, UAC bypass) → 0.95
3. **Process injection** to system processes → 0.70
4. **Privilege escalation** (setuid, kernel modules) → 0.85

**Why isolated sandbox?**
- Prevents actual harm (file not executed by user)
- Accurate behavior measurement (no user interaction)
- Clear verdict before user opens file
- Time-limited (5 seconds) prevents resource exhaustion

### Scoring Algorithm

```
final_score = (yara * 0.40) + (ml * 0.35) + (behavioral * 0.25)

Verdict:
  < 0.30  → BENIGN (safe to open)
  0.30-0.60 → SUSPICIOUS (review before opening)
  ≥ 0.60  → MALICIOUS (block & quarantine)
```

**Example: Ransomware**
- YARA: 0.0 (unknown file)
- ML: 0.72 (looks like malware)
- Behavioral: 0.30 (rapid file mods + encryption)
- **Final: 0.326 → SUSPICIOUS** ✓

### Performance Budget: 5 Seconds

```
Timeline:
  0ms     ├─ File received
  200ms   ├─ Setup sandbox
  500ms   ├─ File I/O monitoring
  1000ms  ├─ Process creation tracking (parallel)
  1500ms  ├─ Memory monitoring (parallel)
  2000ms  ├─ Registry ops (parallel)
  3500ms  ├─ Score calculation
  4500ms  ├─ PolicyGraph cache
  5000ms  └─ Verdict to browser

Optimization: Parallel analysis reduces sequential time
```

### False Positive Mitigation

**Five strategies**:

1. **Whitelisting by Publisher**
   - Files signed by Microsoft → bypass behavioral check
   - Reduces FP on legitimate software

2. **Whitelisting by Pattern**
   - Installers allowed: 100+ files in 30s
   - System utilities allowed: credential access
   - Reduces FP on legitimate installers

3. **Entropy Thresholds**
   - High entropy (7.0+) = encrypted/compressed
   - Media files have high entropy naturally
   - Skip .jpg, .mp4, .zip in entropy check

4. **Directory Whitelisting**
   - System directories allowed rapid mods
   - User directories suspicious
   - Reduces FP on Windows updates

5. **Time Windows**
   - Ransomware: 100+ files in <10 seconds
   - Not just 100+ files in any period
   - Reduces FP on slow file operations

---

## Integration Points

### 1. SecurityTap (RequestServer)
**Where**: `Services/RequestServer/SecurityTap.{h,cpp}`
**What**: Invoke BehavioralAnalyzer for files scoring 0.3-0.6 on YARA+ML
**When**: After YARA scan, if ML score uncertain
**Benefit**: Adds 25% weight to final verdict

### 2. PolicyGraph (Sentinel Database)
**Where**: `Services/Sentinel/PolicyGraph.{h,cpp}`
**What**: Cache behavioral analysis results (24-hour TTL)
**Why**: Avoid reanalyzing known files
**Schema**: New `behavioral_analysis_cache` table (7 columns)

### 3. UI Notifications
**Where**: `about:security` dashboard, download notifications
**What**: Show verdict + explanation to user
**Example**: "Suspicious behavior: rapid file encryption detected"

---

## Implementation Roadmap

### Phase 1: Core Components (Week 1-2)
- [ ] Create `BehavioralAnalyzer.{h,cpp}` (main coordinator)
- [ ] Create `FileIOMonitor.{h,cpp}` (file tracking)
- [ ] Create `ProcessMonitor.{h,cpp}` (process tracking)
- [ ] Create `MemoryMonitor.{h,cpp}` (memory tracking)
- [ ] Create `TestBehavioralAnalyzer.cpp` (unit tests)

### Phase 2: Platform-Specific (Week 2-3)
- [ ] Create `RegistryMonitor.{h,cpp}` (Windows only)
- [ ] Create `PlatformMonitor.{h,cpp}` (Linux privilege escalation)
- [ ] Create `MacOSMonitor.{h,cpp}` (code signing bypass)
- [ ] Platform-specific test cases

### Phase 3: Sandbox Integration (Week 3)
- [ ] Create `Sandbox.{h,cpp}` (execution environment)
- [ ] Integrate with SecurityTap
- [ ] Integration tests (YARA + ML + Behavioral)

### Phase 4: Polish (Week 4)
- [ ] Performance benchmarks (<5s per file)
- [ ] False positive testing (<1% on clean files)
- [ ] Documentation and API review

---

## File Structure

```
docs/
├── BEHAVIORAL_ANALYSIS_SPEC.md                    # Main spec (40KB)
├── BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md         # Quick guide (13KB)
├── BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md    # C++ API (20KB)
└── BEHAVIORAL_ANALYSIS_INDEX.md                   # This file

Services/Sentinel/
├── BehavioralAnalyzer.h/cpp                       # Core coordinator [NEW]
├── FileIOMonitor.h/cpp                            # File events [NEW]
├── ProcessMonitor.h/cpp                           # Process events [NEW]
├── MemoryMonitor.h/cpp                            # Memory events [NEW]
├── RegistryMonitor.h/cpp                          # Registry (Windows) [NEW]
├── PlatformMonitor.h/cpp                          # Platform-specific [NEW]
├── Sandbox.h/cpp                                  # Sandbox execution [NEW]
└── TestBehavioralAnalyzer.cpp                     # Unit tests [NEW]

Services/RequestServer/
├── SecurityTap.h/cpp                             # Updated for behavioral
└── TrafficMonitor.h/cpp                          # Existing (Phase 6)
```

---

## Testing Strategy

### Unit Tests (TestBehavioralAnalyzer.cpp)
```
- Ransomware detection (150 files in 8s)
- Legitimate installer (no FP)
- Information stealer (sensitive dirs)
- Process injection (system process)
- Privilege escalation (Linux)
- Code signing bypass (macOS)
```

### Integration Tests
```
- Full pipeline: YARA + ML + Behavioral
- Timeout handling (<5 seconds)
- Graceful degradation (if sandbox fails)
- PolicyGraph caching
- Cross-platform (Windows, Linux, macOS)
```

### Performance Benchmarks
```
Target: <5 seconds per file
  - Ransomware: ~2 seconds (early termination)
  - Normal file: ~500ms (benign, quick exit)
  - Complex trojan: ~4 seconds (full analysis)
```

### False Positive Baseline
```
Test set: 100 legitimate applications
Target: <1 false positives
  - Windows installers (Office, Visual Studio)
  - System utilities (7-Zip, VLC)
  - Enterprise software (Adobe, JetBrains)
```

---

## Key Metrics for Success

### Security Metrics
- **Detection Rate**: >90% on known malware families
- **False Positive Rate**: <1% on legitimate software
- **Zero-Day Coverage**: Additional 15-20% detection rate (vs YARA+ML alone)

### Performance Metrics
- **Execution Time**: P95 <4 seconds, P99 <5 seconds
- **Memory Usage**: <100 MB per sandbox
- **CPU Overhead**: Single core burst, no sustained load

### User Experience
- **Notification Clarity**: Clear explanation of verdict
- **Decision Support**: Help user make informed choice
- **Performance Impact**: <5% overhead on overall download experience

---

## Related Specifications

### Existing Implementations
- **Phase 6 Network Behavioral Analysis** (`PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md`)
  - Detects C2 beaconing, DNS tunneling, data exfiltration
  - Operates at RequestServer level (post-execution)

- **MalwareML** (`Services/Sentinel/MalwareML.h`)
  - TensorFlow Lite neural network (6 input features)
  - ~80% accuracy on known malware

- **SecurityTap** (`Services/RequestServer/SecurityTap.h`)
  - Existing YARA integration point
  - Where behavioral analyzer should integrate

### Future Enhancements (Milestone 0.6+)
- **Federated Learning**: Distribute malware samples for collaborative training
- **Exploit Detection**: JIT spray, heap spray, ROP gadgets
- **Mobile Support**: Android/iOS behavioral analysis
- **Real-Time Prevention**: Active defense (not just detection)

---

## Architecture Visualization

```
Download Event (RequestServer)
    ↓
┌─────────────────────────────────────┐
│ [1] YARA Signature Scan              │ (existing)
│     ├─ Match → MALICIOUS ✗          │
│     └─ No match → continue           │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ [2] ML Prediction (TFLite)           │ (existing)
│     ├─ Confidence >0.8 → MALICIOUS  │
│     ├─ Confidence 0.3-0.8 → [3]    │
│     └─ Confidence <0.3 → BENIGN ✓  │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ [3] Behavioral Analysis (NEW)        │
│     ├─ Sandbox execution (4s)       │
│     ├─ File I/O patterns            │
│     ├─ Process behavior             │
│     ├─ Memory manipulation          │
│     ├─ Registry ops (Windows)       │
│     └─ Platform-specific            │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ [4] Combined Scoring                │
│     final_score = (YARA*0.4 +      │
│                    ML*0.35 +        │
│                    Behavioral*0.25) │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ [5] Verdict                         │
│     ├─ <0.30 → BENIGN ✓            │
│     ├─ 0.30-0.60 → SUSPICIOUS ⚠   │
│     └─ ≥0.60 → MALICIOUS ✗         │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ [6] PolicyGraph Cache & UI Alert    │
│     ├─ Store result (24h TTL)       │
│     └─ Notify user                  │
└─────────────────────────────────────┘
```

---

## Quick Start for Implementers

### Step 1: Read Documentation
1. **Quick Reference** (20 min read): BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md
2. **Full Spec** (1 hour read): BEHAVIORAL_ANALYSIS_SPEC.md
3. **API Design** (30 min read): BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md

### Step 2: Set Up Repository
```bash
# Create new files
touch Services/Sentinel/BehavioralAnalyzer.{h,cpp}
touch Services/Sentinel/FileIOMonitor.{h,cpp}
touch Services/Sentinel/ProcessMonitor.{h,cpp}
touch Services/Sentinel/MemoryMonitor.{h,cpp}
touch Services/Sentinel/TestBehavioralAnalyzer.cpp

# Update CMakeLists.txt to build new components
vim Services/Sentinel/CMakeLists.txt
```

### Step 3: Implement Phase 1 (Core Components)
```bash
# Copy header from IMPLEMENTATION_GUIDE.md
# Start with BehavioralAnalyzer.h (core interface)
# Implement stubs for all virtual methods
# Create basic unit tests

./Meta/ladybird.py test Sentinel  # Run tests
```

### Step 4: Implement Platform-Specific
```bash
# For each platform (Windows/Linux/macOS)
# Implement specific monitors

# Windows: Use Windows API hooks
# Linux: Use strace/seccomp/auditd
# macOS: Use FSEvents/System Configuration
```

### Step 5: Integrate with SecurityTap
```bash
# Add behavioral analysis call to SecurityTap
# Test full pipeline: YARA → ML → Behavioral

./Meta/ladybird.py run  # Manual testing
```

### Step 6: Performance & Testing
```bash
# Benchmark: ensure <5 second budget
# False positive tests: <1% on clean files
# Malware detection: >90% on known families

./Meta/ladybird.py test Sentinel --benchmark
```

---

## Support and Questions

**If you have questions:**

1. **"What's the overall scoring formula?"**
   - See: BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md → "Scoring Quick Math"

2. **"How do I detect ransomware?"**
   - See: BEHAVIORAL_ANALYSIS_SPEC.md → "Detection Rules" → "DR-1"

3. **"What metrics should I implement?"**
   - See: BEHAVIORAL_ANALYSIS_SPEC.md → "Behavioral Metrics" → all 5 sections

4. **"How do I write unit tests?"**
   - See: BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md → "Testing Framework"

5. **"How does this integrate with YARA?"**
   - See: BEHAVIORAL_ANALYSIS_SPEC.md → "Integration Architecture" → "Integration with YARA"

6. **"What's the C++ API?"**
   - See: BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md → "Core Interface"

---

## Document Statistics

| Document | Size | Lines | Read Time | Content |
|----------|------|-------|-----------|---------|
| BEHAVIORAL_ANALYSIS_SPEC.md | 40 KB | 1527 | 1 hour | Full specification |
| BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md | 13 KB | 395 | 20 min | Quick reference |
| BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md | 20 KB | 646 | 30 min | C++ API |
| **TOTAL** | **73 KB** | **2568** | **2 hours** | **Complete system design** |

---

**Document Version**: 1.0
**Created**: 2025-11-01
**Milestone**: 0.5 Phase 1 - Real-time Sandboxing
**Status**: Ready for Implementation
**Next Step**: Begin Phase 1 Implementation (Week 1)

