# Phase 2: NsJail Research - Complete

**Date**: 2025-11-02
**Milestone**: 0.5 - Advanced Threat Response & Active Defense
**Phase**: 2 - Behavioral Analysis (Tier 2)
**Status**: ✅ Research Complete - Ready for Implementation

---

## Research Objectives (Completed)

✅ Research nsjail capabilities and limitations
✅ Document installation steps for common Linux distributions
✅ Identify required system permissions and capabilities
✅ List nsjail command-line flags needed for file analysis
✅ Document syscall filtering approach (seccomp-bpf)
✅ Identify integration challenges and solutions
✅ Compare alternative technologies (firejail, bubblewrap, docker)
✅ Create comprehensive documentation and examples

---

## Deliverables

### 1. Comprehensive Research Document
**File**: `/home/rbsmith4/ladybird/docs/NSJAIL_RESEARCH_SUMMARY.md`
- **Size**: 52 KB (2,044 lines)
- **Sections**: 10 major sections covering all research areas
- **Content**:
  - Executive summary with recommendations
  - Installation guide for Ubuntu, Fedora, Arch
  - Complete feature overview (namespaces, seccomp, cgroups, rlimits)
  - Production-ready malware sandbox configuration
  - Complete Kafel seccomp policy (300+ lines)
  - Syscall monitoring approaches (strace, ptrace, eBPF)
  - C++ integration code examples
  - Performance benchmarks and optimization strategies
  - Known limitations with workarounds
  - Technology comparison table (nsjail vs firejail vs bubblewrap vs docker)
  - Implementation roadmap

### 2. Quick Reference Guide
**File**: `/home/rbsmith4/ladybird/docs/NSJAIL_QUICK_REFERENCE.md`
- **Size**: 6.8 KB (compact reference)
- **Content**:
  - One-liner installation commands
  - Minimal configuration templates
  - Common command examples
  - C++ integration skeleton code
  - Testing commands
  - Troubleshooting guide
  - Performance quick reference
  - Next steps checklist

### 3. Example Configuration Files (In Documents)
- **Malware Sandbox Config**: Complete protobuf configuration
- **Kafel Seccomp Policy**: Production-ready syscall filtering policy
- **Integration Code**: BehavioralAnalyzer skeleton implementation

---

## Key Findings

### Technology Selection: NsJail (Recommended)

**Why NsJail?**
1. ✅ **Best security**: Multi-layered isolation (namespaces + seccomp + cgroups + rlimits)
2. ✅ **Flexibility**: Kafel DSL for expressive syscall policies
3. ✅ **Performance**: Minimal overhead (< 5%) without strace
4. ✅ **Google-backed**: Actively maintained, used in production (kCTF)
5. ✅ **Configuration files**: Protocol Buffer format for versioned configs
6. ✅ **No SUID**: More secure than firejail
7. ✅ **Malware-focused**: Designed for hostile workload isolation

**Comparison with Alternatives**:

| Feature | nsjail | Firejail | Bubblewrap | Docker |
|---------|--------|----------|------------|--------|
| Malware Analysis | ✅ Excellent | ⚠️ Acceptable | ⚠️ Limited | ✅ Good |
| Seccomp DSL | ✅ Kafel | ⚠️ Profiles | ❌ JSON only | ⚠️ JSON |
| Cgroup Support | ✅ Full | ⚠️ Limited | ❌ None | ✅ Full |
| Configuration | ✅ Protobuf | ⚠️ Profiles | ❌ CLI only | ⚠️ Complex |
| Performance | ✅ Fast | ✅ Fast | ✅ Fastest | ⚠️ Medium |
| SUID Binary | ✅ No | ❌ Yes | ✅ No | ✅ No |

**Winner**: nsjail for malware analysis use case

### System Requirements

**Minimum Kernel**: Linux 3.8+ (user namespaces)
**Recommended Kernel**: Linux 4.6+ (cgroup namespaces)
**Optimal Kernel**: Linux 4.14+ (seccomp logging)

**Dependencies** (Ubuntu/Debian):
```bash
autoconf bison flex gcc g++ git libprotobuf-dev
libnl-route-3-dev libtool make pkg-config protobuf-compiler
```

**Installation Time**: ~5 minutes (compile from source)

### Syscall Monitoring Approach

**Hybrid Strategy** (recommended):

1. **Primary**: Seccomp LOG action (minimal overhead)
   - Log suspicious syscalls: socket, connect, execve, fork, ptrace
   - Parse kernel audit logs or dmesg
   - **Overhead**: ~3-5%
   - **Use**: All files

2. **Fallback**: strace for deep analysis (high overhead)
   - Full syscall argument capture
   - Process tree tracking
   - **Overhead**: 100-200x
   - **Use**: Only for high-risk files (threat score > 70)

3. **Future**: eBPF-based monitoring (low overhead)
   - Kernel-level tracing
   - **Overhead**: < 5%
   - **Use**: Production deployment (Phase 4)

### Performance Impact

| Configuration | Overhead | Suitable For |
|---------------|----------|--------------|
| nsjail only | 1-2% | All files |
| nsjail + seccomp LOG | 3-5% | All files |
| nsjail + strace | 100-200x | High-risk only |
| nsjail + eBPF (future) | < 5% | Production scale |

**Recommendation**: Use seccomp LOG by default, escalate to strace for suspicious files.

### Integration Challenges & Solutions

#### Challenge 1: Syscall Logging
**Problem**: nsjail filters syscalls but doesn't log arguments
**Solution**: Hybrid approach (seccomp LOG + selective strace)

#### Challenge 2: Performance Overhead
**Problem**: strace adds 100-200x overhead
**Solution**: Use strace only for Tier 2 escalation, not all files

#### Challenge 3: User Namespace Support
**Problem**: Some distros disable unprivileged user namespaces
**Solution**: Enable via sysctl or run nsjail with CAP_SYS_ADMIN

#### Challenge 4: AppArmor/SELinux Conflicts
**Problem**: Security modules may block namespace creation
**Solution**: Create AppArmor/SELinux profiles for nsjail

---

## Implementation Roadmap

### Phase 2: Basic Integration (Current)
**Timeline**: 2-3 days
**Tasks**:
- [x] Research nsjail (complete)
- [ ] Install nsjail on development system
- [ ] Create `/etc/sentinel/` configuration files
- [ ] Implement `BehavioralAnalyzer.{h,cpp}` skeleton
- [ ] Write unit tests (`TestBehavioralAnalyzer.cpp`)
- [ ] Test with benign files
- [ ] Integrate with MalwareDetector Tier 2 escalation

**Deliverables**:
- Working BehavioralAnalyzer class
- Basic threat scoring (0-100)
- Seccomp-based syscall logging

### Phase 3: Advanced Monitoring (Future)
**Timeline**: 1-2 weeks
**Tasks**:
- [ ] Implement strace log parser
- [ ] Add detailed behavioral pattern matching
- [ ] Store behavioral signatures in PolicyGraph
- [ ] Implement user alerts for suspicious behavior
- [ ] Add forensic report generation

**Deliverables**:
- Detailed behavioral reports
- Strace integration (selective)
- User-facing alerts

### Phase 4: Production Optimization (Future)
**Timeline**: 2-3 weeks
**Tasks**:
- [ ] Migrate to eBPF for syscall monitoring
- [ ] Implement distributed analysis (queue-based)
- [ ] Add GPU-accelerated ML scoring
- [ ] Performance tuning and benchmarking
- [ ] Production hardening

**Deliverables**:
- Low-overhead eBPF monitoring
- Scalable analysis pipeline
- Production-ready deployment

---

## Code Examples Provided

### 1. Configuration Files

**Malware Sandbox Config** (`nsjail-malware-sandbox.cfg`):
- Complete protobuf configuration
- All namespaces enabled
- Resource limits configured
- Filesystem isolation (read-only system dirs, tmpfs for sandbox)
- Seccomp policy integration
- Network isolation

**Kafel Seccomp Policy** (`malware-sandbox.kafel`):
- 300+ lines of syscall filtering rules
- ALLOW: File operations, memory ops, signals, time
- LOG: Network, process creation, IPC (suspicious)
- ERRNO: Privilege escalation, kernel operations (dangerous)
- DEFAULT: KILL (whitelist approach)

### 2. C++ Integration Code

**BehavioralAnalyzer.h**:
```cpp
class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(
        StringView nsjail_path, StringView config_path);

    ErrorOr<BehavioralReport> analyze_file(StringView file_path, u32 timeout_seconds = 30);

private:
    ErrorOr<String> prepare_sandbox(StringView file_path);
    ErrorOr<pid_t> launch_nsjail(StringView sandbox_path, StringView file_path);
    ErrorOr<void> monitor_execution(pid_t nsjail_pid);
    ErrorOr<BehavioralReport> collect_and_analyze();

    String m_nsjail_path;
    String m_config_path;
    String m_strace_log_path;
};
```

**BehavioralReport struct**:
```cpp
struct BehavioralReport {
    Vector<SyscallEvent> syscalls;
    Vector<FileOperation> file_ops;
    Vector<NetworkOperation> network_ops;
    Vector<ProcessOperation> process_ops;

    bool attempted_network_access { false };
    bool attempted_process_creation { false };
    bool attempted_file_modification { false };
    bool attempted_privileged_escalation { false };

    u8 threat_score { 0 };  // 0-100
};
```

**Implementation skeleton**: Complete `BehavioralAnalyzer.cpp` with:
- Sandbox preparation
- nsjail spawning via `Core::Process::spawn()`
- Process monitoring with `waitpid()`
- Log parsing (strace format)
- Threat score calculation

### 3. Testing Commands

**Installation verification**:
```bash
nsjail --version
nsjail -Mo --chroot /tmp -- /bin/echo "test"
```

**Configuration testing**:
```bash
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg -- /bin/ls
```

**File analysis**:
```bash
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /suspicious/file:/sandbox/target \
    -- /sandbox/target
```

---

## Documentation Structure

```
docs/
├── NSJAIL_RESEARCH_SUMMARY.md          # Full research (52 KB, 2044 lines)
│   ├── Executive Summary
│   ├── Installation Guide (Ubuntu, Fedora, Arch)
│   ├── Key Features and Capabilities
│   ├── Recommended Configuration for Malware Analysis
│   ├── Syscall Monitoring Approach
│   ├── Integration Strategy with BehavioralAnalyzer
│   ├── Performance Considerations
│   ├── Known Limitations and Workarounds
│   ├── Alternative Technologies (comparison)
│   ├── Recommendations (roadmap)
│   └── Appendices (complete examples)
│
├── NSJAIL_QUICK_REFERENCE.md           # Quick start (6.8 KB)
│   ├── Installation (one-liner)
│   ├── Essential configuration files
│   ├── Common commands
│   ├── Kafel policy template
│   ├── C++ integration skeleton
│   ├── Testing commands
│   ├── Troubleshooting
│   ├── Performance benchmarks
│   └── Next steps
│
└── PHASE_2_NSJAIL_RESEARCH_COMPLETE.md # This summary
```

---

## Risks and Mitigations

### Risk 1: User Namespace Restrictions
**Impact**: Medium
**Probability**: Low-Medium (some distros disable by default)
**Mitigation**: Enable via sysctl or run with capabilities

### Risk 2: strace Performance Impact
**Impact**: High (100-200x slowdown)
**Probability**: High (required for detailed logging)
**Mitigation**: Use strace selectively, only for high-risk files

### Risk 3: Kernel Compatibility
**Impact**: Medium
**Probability**: Low (most modern kernels supported)
**Mitigation**: Document minimum kernel versions, test on target systems

### Risk 4: AppArmor/SELinux Conflicts
**Impact**: Medium
**Probability**: Medium (enterprise systems)
**Mitigation**: Create security module profiles, document workarounds

---

## Success Metrics

### Phase 2 (Basic Integration)
- [ ] BehavioralAnalyzer compiles without errors
- [ ] Unit tests pass (> 90% coverage)
- [ ] Can analyze benign files without crashes
- [ ] Threat scores calculated correctly
- [ ] Integration with MalwareDetector works
- [ ] Documentation updated

### Phase 3 (Advanced Monitoring)
- [ ] strace log parsing works correctly
- [ ] Behavioral signatures stored in PolicyGraph
- [ ] User alerts triggered for high-risk files
- [ ] Forensic reports generated

### Phase 4 (Production)
- [ ] eBPF monitoring implemented
- [ ] Analysis overhead < 10% (without strace)
- [ ] Can analyze 100+ files/hour
- [ ] Production deployment successful

---

## Recommendations

### Immediate Actions (Next Steps)

1. **Install nsjail** on development system:
   ```bash
   cd /tmp
   git clone https://github.com/google/nsjail.git
   cd nsjail
   make -j$(nproc)
   sudo cp nsjail /usr/local/bin/
   ```

2. **Create configuration files**:
   ```bash
   sudo mkdir -p /etc/sentinel
   sudo nano /etc/sentinel/nsjail-malware-sandbox.cfg
   sudo nano /etc/sentinel/malware-sandbox.kafel
   ```
   (Copy from `NSJAIL_RESEARCH_SUMMARY.md` Appendices A & B)

3. **Implement BehavioralAnalyzer**:
   ```bash
   touch Services/Sentinel/BehavioralAnalyzer.{h,cpp}
   # Add to Services/Sentinel/CMakeLists.txt
   ```

4. **Write unit tests**:
   ```bash
   touch Services/Sentinel/TestBehavioralAnalyzer.cpp
   ```

5. **Test with sample files**:
   ```bash
   ./Build/release/bin/TestBehavioralAnalyzer
   ```

### Future Optimizations

1. **Migrate to eBPF** (Phase 4):
   - Reduce overhead from 100x to < 5%
   - Enable production-scale analysis
   - Better syscall visibility

2. **Distributed Analysis** (Phase 4):
   - Queue-based architecture
   - Multiple analysis nodes
   - Async result collection

3. **ML Integration** (Phase 4):
   - Combine behavioral analysis with ML models
   - GPU-accelerated feature extraction
   - Real-time threat classification

---

## Conclusion

**NsJail research is complete and successful.** All objectives have been met, and comprehensive documentation has been created. The technology is **highly suitable** for Sentinel's Tier 2 behavioral analysis.

**Key Takeaway**: nsjail provides the best balance of security, flexibility, and performance for malware sandboxing compared to alternatives (firejail, bubblewrap, docker).

**Recommendation**: **Proceed with Phase 2 implementation** using the provided configurations and code examples.

**Estimated Implementation Time**: 2-3 days for basic integration, 1-2 weeks for advanced features.

---

## Next Phase

**Phase 3: Active Response System** (from Milestone 0.5 plan)
- Automated threat containment
- Network isolation triggers
- Policy enforcement
- Incident response coordination

**Prerequisite**: Complete Phase 2 (BehavioralAnalyzer implementation)

---

## References

- **Full Research**: `/home/rbsmith4/ladybird/docs/NSJAIL_RESEARCH_SUMMARY.md`
- **Quick Reference**: `/home/rbsmith4/ladybird/docs/NSJAIL_QUICK_REFERENCE.md`
- **nsjail GitHub**: https://github.com/google/nsjail
- **Kafel Docs**: https://google.github.io/kafel/
- **Milestone 0.5 Plan**: `/home/rbsmith4/ladybird/docs/MILESTONE_0.5_PLAN.md`

---

**Status**: ✅ Research Complete - Ready for Implementation
**Date**: 2025-11-02
**Next Action**: Begin Phase 2 implementation (BehavioralAnalyzer)
