# BehavioralAnalyzer Design Report

**Date**: 2025-11-02
**Milestone**: 0.5 Phase 2 - Real-time Behavioral Analysis
**Status**: Design Complete

---

## Executive Summary

This report documents the completion of the **BehavioralAnalyzer real implementation design** to replace the current stub with nsjail-based syscall monitoring for Linux.

### Design Objectives Achieved

✅ **Maintain Interface Compatibility**: No changes to `BehavioralMetrics` or `analyze()` signature
✅ **Linux Implementation Specified**: Complete nsjail + seccomp-BPF architecture
✅ **Syscall Monitoring Strategy**: Three-tier approach (seccomp-BPF → unotify → ptrace)
✅ **Behavioral Pattern Detection**: 16 metrics across 5 categories
✅ **Scoring Algorithm Defined**: Multi-factor scoring with category weights
✅ **Platform Support Documented**: Real implementation on Linux, graceful degradation elsewhere
✅ **Performance Targets Set**: < 5 seconds analysis time, < 50MB memory
✅ **Security Considerations**: Sandbox escape prevention, timeout enforcement
✅ **Implementation Roadmap**: 4-5 week timeline with weekly milestones

---

## Deliverables

### 1. Design Document

**File**: `docs/BEHAVIORAL_ANALYZER_DESIGN.md` (19,000+ words)

**Contents**:
- Architecture overview with component diagram
- Execution flow with detailed step-by-step pseudo-code
- nsjail integration specifications
- Syscall monitoring strategy (seccomp-BPF, ptrace)
- Behavioral pattern definitions (16 metrics, 5 categories)
- Scoring algorithm with formula specifications
- Platform support strategy (Linux, macOS, Windows)
- Interface specification (unchanged from stub)
- Implementation pseudo-code for core methods
- Integration checklist (5 phases)
- Security considerations (sandbox escape, timeout, privilege escalation)

**Key Decisions**:

| Decision | Rationale |
|----------|-----------|
| **Primary Technology: nsjail** | Battle-tested in Chrome, Google-backed, production-ready |
| **Syscall Monitoring: seccomp-BPF** | Kernel-level filtering with allow/monitor/block policies |
| **Fallback: ptrace** | Universal syscall monitoring when seccomp_unotify unavailable |
| **Platform: Linux-first** | Real implementation on Linux, stub fallback on macOS/Windows |
| **Timeout: alarm() + nsjail --time_limit** | Dual-layer timeout enforcement for reliability |
| **IPC: Unix pipes** | Simple, reliable parent-child communication |

### 2. Quick Reference Guide

**File**: `docs/BEHAVIORAL_ANALYZER_QUICK_REFERENCE.md` (3,500+ words)

**Contents**:
- TL;DR implementation summary
- Key technologies table
- Installation instructions
- Implementation priority order
- Core methods with code snippets
- Syscall-to-metric mapping table
- Scoring formula quick reference
- Testing strategy examples
- Common pitfalls and solutions
- Debugging tips

**Purpose**: Rapid onboarding for developers implementing Phase 2

### 3. Implementation Checklist

**File**: `docs/BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md` (4,000+ words)

**Contents**:
- 5-week implementation timeline
- Week-by-week task breakdown
- Checkboxes for all implementation tasks
- Testing requirements for each phase
- Acceptance criteria
- Risk assessment and mitigation
- Success metrics

**Purpose**: Project tracking and ensuring nothing is missed

---

## Technical Architecture

### High-Level Design

```
┌──────────────────────────────────────┐
│     Orchestrator                     │
│  (Tier 2 execution path)             │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│  BehavioralAnalyzer::analyze()       │
│  ┌────────────────────────────────┐  │
│  │ 1. Create temp directory       │  │
│  │ 2. Write file to sandbox       │  │
│  │ 3. Launch nsjail with target   │  │
│  │ 4. Monitor syscalls (ptrace)   │  │
│  │ 5. Parse events → metrics      │  │
│  │ 6. Calculate threat score      │  │
│  │ 7. Cleanup sandbox             │  │
│  └────────────────────────────────┘  │
└──────────────┬───────────────────────┘
               │
               ▼
         BehavioralMetrics
         (unchanged interface)
```

### Syscall Monitoring Flow

```
Target Binary Execution
         │
         ▼
    nsjail sandbox
    (namespaces + seccomp)
         │
         ├──> Allow: Basic syscalls (read, write, exit)
         ├──> Monitor: Suspicious syscalls (fork, exec, socket)
         └──> Block: Dangerous syscalls (mount, reboot, ptrace)
         │
         ▼
   seccomp_unotify (kernel → userspace)
   OR ptrace (fallback)
         │
         ▼
   Parent Process (BehavioralAnalyzer)
   - Receives syscall events
   - Updates BehavioralMetrics counters
   - Enforces timeout (alarm + SIGKILL)
```

### Behavioral Pattern Detection

**Categories**:

1. **File I/O** (40% weight)
   - Rapid modification (>100 files in 10s) → Ransomware
   - High entropy writes → Encryption
   - Sensitive directory access → Info stealer
   - Executable drops → Dropper/worm

2. **Process** (30% weight)
   - Deep process chains (>5 levels) → Privilege escalation
   - Process injection (ptrace) → Rootkit/trojan
   - Suspicious subprocesses → Command injection

3. **Memory** (15% weight)
   - Heap spray (repeated allocations) → Exploit kit
   - RWX pages (executable + writable) → Shellcode

4. **Network** (10% weight)
   - Outbound connections (>3) → C2 beaconing
   - DNS queries (>10) → DNS tunneling
   - HTTP requests (>10) → Exfiltration

5. **Platform-specific** (5% weight)
   - Linux: setuid/sudo abuse → Privilege escalation
   - Windows: Registry persistence → Trojan
   - macOS: Code signing bypass → Evasion

### Scoring Algorithm

```cpp
float threat_score = (
    file_io_score * 0.40 +       // Ransomware, stealers
    process_score * 0.30 +       // Rootkits, injectors
    network_score * 0.10 +       // C2 beaconing
    memory_score * 0.15 +        // Exploits, shellcode
    platform_score * 0.05        // OS-specific patterns
);

// Thresholds
if (threat_score < 0.30) → BENIGN
if (0.30 ≤ threat_score < 0.60) → SUSPICIOUS
if (threat_score ≥ 0.60) → MALICIOUS
```

---

## Key Design Features

### 1. Interface Compatibility

**Requirement**: No changes to Orchestrator integration

**Solution**: Maintain exact same interface as stub:

```cpp
// Unchanged public interface
class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(SandboxConfig const&);
    ErrorOr<BehavioralMetrics> analyze(ByteBuffer const&, String const&, Duration);
    Statistics get_statistics() const;
    void reset_statistics();
};

// Unchanged result structure
struct BehavioralMetrics {
    u32 file_operations;
    u32 process_operations;
    u32 network_operations;
    float threat_score;
    Vector<String> suspicious_behaviors;
    // ... (16 metrics total)
};
```

### 2. Platform Support Strategy

**Approach**: Real implementation on Linux, graceful degradation elsewhere

| Platform | Implementation | Detection Capability |
|----------|----------------|---------------------|
| **Linux** | Real (nsjail + seccomp) | ✅ Full syscall monitoring |
| **macOS** | Enhanced stub (FSEvents) | ⚠️ Limited file monitoring |
| **Windows** | Enhanced stub (WMI) | ⚠️ Limited process monitoring |

**Benefit**: Progressive enhancement - works everywhere, best on Linux

### 3. Security Hardening

**Sandbox Escape Prevention**:

1. **Namespace Isolation**
   - Network namespace: No network access
   - PID namespace: Isolated process tree
   - Mount namespace: Read-only root filesystem
   - User namespace: Non-root execution

2. **syscall Filtering**
   - seccomp-BPF policy: Allow/monitor/block at kernel level
   - Block dangerous syscalls: mount, reboot, ptrace, init_module
   - Monitor suspicious syscalls: fork, exec, socket, mprotect

3. **Resource Limits**
   - CPU time: 5 seconds max (rlimit + nsjail)
   - Memory: 128MB max (rlimit)
   - File size: 10MB writes max (rlimit)
   - File descriptors: 64 max (rlimit)

4. **Timeout Enforcement**
   - Parent alarm() → SIGKILL child after timeout
   - nsjail --time_limit for redundancy
   - Prevents infinite loops, resource exhaustion

### 4. Performance Optimization

**Target**: < 5 seconds total analysis time

**Breakdown**:

| Phase | Time Budget | Technique |
|-------|-------------|-----------|
| Temp dir creation | < 10ms | mkdtemp (atomic) |
| File write | < 50ms | Single write() syscall |
| nsjail startup | < 200ms | Pre-built configuration |
| Syscall monitoring | < 2s | ptrace (low overhead) |
| Score calculation | < 10ms | Simple arithmetic |
| Cleanup | < 50ms | Recursive remove |
| **Total** | **< 3s** | **Leaves 2s buffer** |

**Optimizations**:

- Early termination if obvious malware detected
- Parallel monitoring (separate thread)
- Selective syscall filtering (ignore benign syscalls)
- Caching (skip re-analysis of known files)

---

## Implementation Roadmap

### Timeline: 4-5 Weeks

**Week 1**: Foundation
- Install nsjail, test manually
- Implement temp directory creation
- Implement nsjail command builder
- Create test harness

**Week 2**: Syscall Monitoring
- Implement ptrace-based monitoring
- Implement seccomp_unotify (if available)
- Implement timeout enforcement
- Test with benign executables

**Week 3**: Metrics & Scoring
- Implement syscall-to-metric mapping
- Implement behavioral pattern detection
- Implement threat score calculation
- Test with malware simulators

**Week 4**: Integration & Testing
- Integrate with Orchestrator
- Comprehensive testing (unit + integration)
- Performance benchmarking
- Platform compatibility testing

**Week 5**: Documentation & Polish
- Code documentation (inline comments)
- User-facing documentation
- Developer guide
- Code review preparation

### Success Criteria

- ✅ Analysis time < 5 seconds (95th percentile)
- ✅ Detection rate > 90% on known malware
- ✅ False positive rate < 5% on legitimate software
- ✅ Memory usage < 50MB per analysis
- ✅ No interface changes (Orchestrator compatibility)
- ✅ Graceful degradation on non-Linux platforms

---

## Risk Assessment

### High Risks

**Risk**: nsjail not available on target system
- **Impact**: High (cannot perform real analysis)
- **Likelihood**: Low (nsjail widely available)
- **Mitigation**: Graceful degradation to stub implementation

**Risk**: False positives too high
- **Impact**: High (user frustration, loss of trust)
- **Likelihood**: Medium (scoring thresholds need tuning)
- **Mitigation**: Extensive testing, adjustable thresholds, user feedback

### Medium Risks

**Risk**: seccomp_unotify not supported (kernel < 5.4)
- **Impact**: Medium (fallback to slower ptrace)
- **Likelihood**: Medium (depends on deployment environment)
- **Mitigation**: Implement ptrace fallback (already planned)

**Risk**: Performance targets not met
- **Impact**: Medium (degraded user experience)
- **Likelihood**: Low (nsjail designed for performance)
- **Mitigation**: Early termination, parallel monitoring, caching

### Low Risks

**Risk**: Platform-specific bugs
- **Impact**: Low (isolated to specific platforms)
- **Likelihood**: Low (extensive testing planned)
- **Mitigation**: Platform-specific testing, graceful degradation

---

## Next Steps

### Immediate Actions (This Week)

1. **Review Design Documents**
   - Share `BEHAVIORAL_ANALYZER_DESIGN.md` with team
   - Gather feedback on design decisions
   - Address any concerns

2. **Setup Development Environment**
   - Install nsjail: `sudo apt-get install nsjail`
   - Install dependencies: `sudo apt-get install libseccomp-dev`
   - Test nsjail manually: `nsjail --mode o --time_limit 5 -- /bin/ls`

3. **Create Initial Implementation Structure**
   - Create test directory: `Services/Sentinel/Test/behavioral/`
   - Create asset directory: `Services/Sentinel/assets/seccomp/`
   - Stub out core methods in `BehavioralAnalyzer.cpp`

### Phase 2 Implementation (Weeks 1-5)

Follow the detailed checklist in `BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md`:

- **Week 1**: Foundation (temp dirs, file writing, nsjail launcher)
- **Week 2**: Syscall monitoring (ptrace, seccomp_unotify, timeout)
- **Week 3**: Metrics & scoring (parsing, patterns, threat scores)
- **Week 4**: Integration & testing (Orchestrator, benchmarks)
- **Week 5**: Documentation & polish (comments, guides, review)

### Post-Implementation (Future Work)

1. **Telemetry & Tuning**
   - Deploy to staging environment
   - Monitor real-world detection rates
   - Tune scoring thresholds based on data

2. **Advanced Features**
   - Entropy calculation for written data
   - Syscall argument parsing (file paths, IPs)
   - Malware family classification

3. **ML Integration**
   - Train ML model on behavioral patterns
   - Combine with existing MalwareML
   - Improve zero-day detection

---

## Document Index

All design documents have been created in `docs/`:

1. **BEHAVIORAL_ANALYZER_DESIGN.md** (19,000+ words)
   - Complete design specification
   - Architecture, implementation, security

2. **BEHAVIORAL_ANALYZER_QUICK_REFERENCE.md** (3,500+ words)
   - Quick start guide for developers
   - Code snippets, testing, debugging

3. **BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md** (4,000+ words)
   - 5-week implementation roadmap
   - Task-by-task breakdown with checkboxes

4. **BEHAVIORAL_ANALYZER_DESIGN_REPORT.md** (This document)
   - Executive summary of design work
   - Key decisions, deliverables, next steps

### Related Documents

- **BEHAVIORAL_ANALYSIS_SPEC.md**: Original specification (16 metrics, 5 categories)
- **SANDBOX_TECHNOLOGY_COMPARISON.md**: Technology evaluation (nsjail vs alternatives)
- **SANDBOX_RECOMMENDATION_SUMMARY.md**: Quick reference for sandbox choice
- **MILESTONE_0.5_PLAN.md**: Overall milestone plan

---

## Key Design Decisions Summary

| Question | Decision | Rationale |
|----------|----------|-----------|
| **What sandbox technology?** | nsjail | Battle-tested, Google-backed, production-ready |
| **What syscall monitoring?** | seccomp-BPF + ptrace | Kernel-level filtering with universal fallback |
| **What platforms?** | Linux first, stub elsewhere | Real implementation where it matters most |
| **What timeout mechanism?** | alarm() + nsjail | Dual-layer enforcement for reliability |
| **What IPC?** | Unix pipes | Simple, reliable, well-understood |
| **What scoring formula?** | Multi-factor weighted | Aligns with BEHAVIORAL_ANALYSIS_SPEC.md |
| **What performance target?** | < 5 seconds | User-acceptable for suspicious files |
| **What memory target?** | < 50MB | Minimal overhead, scalable |
| **What detection rate target?** | 90%+ | Industry-standard for malware detection |
| **What false positive rate?** | < 5% | User-acceptable for security tools |

---

## Conclusion

The **BehavioralAnalyzer real implementation design** is complete and ready for Phase 2 implementation. The design:

✅ **Maintains compatibility** with existing Orchestrator integration
✅ **Specifies complete architecture** using nsjail + seccomp-BPF
✅ **Defines behavioral patterns** across 16 metrics in 5 categories
✅ **Provides detailed pseudo-code** for all core methods
✅ **Addresses security concerns** (sandbox escape, timeout, privilege escalation)
✅ **Sets realistic performance targets** (< 5s analysis, < 50MB memory)
✅ **Plans for graceful degradation** on non-Linux platforms
✅ **Provides implementation roadmap** with 5-week timeline

**Next Steps**:
1. Review design documents with team
2. Install nsjail and dependencies
3. Begin Week 1 implementation (foundation)
4. Follow checklist in `BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md`

---

**Report Author**: Security Research Team
**Date**: 2025-11-02
**Status**: Design Complete - Ready for Implementation
**Estimated Implementation Time**: 4-5 weeks
**Target Completion**: Mid-December 2025
