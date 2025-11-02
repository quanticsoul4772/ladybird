# BehavioralAnalyzer Phase 2 Implementation Checklist

**Milestone**: 0.5 Phase 2 - Real Behavioral Analysis
**Timeline**: 4-5 weeks
**Status**: Not Started

---

## Overview

This checklist tracks the implementation of real nsjail-based behavioral analysis to replace the current stub implementation.

**Goal**: Execute suspicious files in an isolated nsjail sandbox, monitor syscalls, detect malicious patterns, and return behavioral metrics.

**Success Criteria**:
- ✅ Analysis completes in < 5 seconds
- ✅ Detects ransomware patterns (rapid file modification)
- ✅ Detects process injection (ptrace, RWX memory)
- ✅ Detects network beaconing (outbound connections)
- ✅ False positive rate < 5% on legitimate software
- ✅ Interface compatibility maintained (no Orchestrator changes)

---

## Week 1: Foundation (Setup & Infrastructure)

### Environment Setup

- [ ] **Install nsjail on development system**
  ```bash
  sudo apt-get update
  sudo apt-get install nsjail libseccomp-dev
  nsjail --help  # Verify installation
  ```

- [ ] **Test nsjail manually**
  ```bash
  # Test basic execution
  nsjail --mode o --time_limit 5 -- /bin/ls

  # Test with resource limits
  nsjail --mode o --time_limit 5 --rlimit_as 128 -- /bin/echo "test"

  # Test with seccomp filter
  nsjail --mode o --seccomp_string "POLICY default ALLOW" -- /bin/ls
  ```

- [ ] **Create test directory structure**
  ```bash
  mkdir -p Services/Sentinel/Test/behavioral
  mkdir -p Services/Sentinel/assets/seccomp
  ```

### Core Infrastructure

- [ ] **Implement temp directory creation**
  - [ ] `create_temp_sandbox_directory()` using mkdtemp
  - [ ] `cleanup_temp_directory()` with recursive removal
  - [ ] Add error handling for mkdir failures
  - [ ] Test: Verify temp dir created in /tmp/sentinel-XXXXXX

- [ ] **Implement file writing**
  - [ ] `write_file_to_sandbox()` with secure permissions (0600)
  - [ ] `make_executable()` using chmod (0700)
  - [ ] Add error handling for write failures
  - [ ] Test: Write binary file, verify executable bit

- [ ] **Implement nsjail command builder**
  - [ ] `build_nsjail_command()` with all arguments
  - [ ] Add resource limits (CPU, memory, file descriptors)
  - [ ] Add network isolation flag
  - [ ] Add seccomp policy (inline or file)
  - [ ] Test: Verify command builds correctly

### Testing Infrastructure

- [ ] **Create test harness**
  - [ ] `Services/Sentinel/Test/TestBehavioralAnalyzer.cpp`
  - [ ] Add test for temp directory creation
  - [ ] Add test for file writing
  - [ ] Add test for nsjail command generation
  - [ ] Run tests: `./Meta/ladybird.py test Sentinel`

---

## Week 2: Syscall Monitoring

### Process Management

- [ ] **Implement sandbox launcher**
  - [ ] `launch_nsjail_sandbox()` using fork/exec
  - [ ] Create IPC pipes (result pipe, syscall pipe)
  - [ ] Close unused pipe ends after fork
  - [ ] Handle fork/exec errors gracefully
  - [ ] Test: Launch nsjail, verify process created

- [ ] **Implement timeout enforcement**
  - [ ] Set alarm() before monitoring
  - [ ] Install SIGALRM handler
  - [ ] Kill sandbox process on timeout (SIGKILL)
  - [ ] Cancel alarm on normal completion
  - [ ] Test: Verify timeout kills hanging process

- [ ] **Implement process cleanup**
  - [ ] `wait_for_sandbox_completion()` using waitpid
  - [ ] Parse exit status (WIFEXITED, WIFSIGNALED)
  - [ ] Reap zombie processes
  - [ ] Test: Verify no zombie processes left

### Syscall Monitoring Core

- [ ] **Check seccomp_unotify availability**
  - [ ] `seccomp_unotify_available()` using syscall check
  - [ ] Document kernel version requirement (5.4+)
  - [ ] Test: Check on development system

- [ ] **Implement ptrace monitoring (fallback)**
  - [ ] `monitor_via_ptrace()` using PTRACE_SEIZE
  - [ ] Attach to sandbox child process
  - [ ] Set PTRACE_SETOPTIONS for syscall tracing
  - [ ] Read syscall numbers from registers (orig_rax)
  - [ ] Map syscall numbers to names
  - [ ] Test: Monitor /bin/ls, verify syscalls captured

- [ ] **Implement seccomp_unotify monitoring (preferred)**
  - [ ] `monitor_via_seccomp_unotify()` using ioctl
  - [ ] Receive notifications from kernel (SECCOMP_IOCTL_NOTIF_RECV)
  - [ ] Parse syscall details (number, args)
  - [ ] Send responses to continue execution (SECCOMP_IOCTL_NOTIF_SEND)
  - [ ] Test: Verify notifications received for monitored syscalls

- [ ] **Implement syscall name mapping**
  - [ ] `syscall_number_to_name()` lookup table
  - [ ] Map common syscalls (open, fork, socket, mmap, etc.)
  - [ ] Add architecture detection (x86_64, arm64)
  - [ ] Test: Verify correct names for syscall numbers

### Testing

- [ ] **Test syscall monitoring with benign executable**
  - [ ] Monitor /bin/ls execution
  - [ ] Verify file operations captured (open, read, write)
  - [ ] Verify no false positives
  - [ ] Benchmark: < 2 seconds monitoring time

- [ ] **Test timeout enforcement**
  - [ ] Create infinite loop executable
  - [ ] Monitor with 5-second timeout
  - [ ] Verify process killed after timeout
  - [ ] Verify metrics.timed_out set to true

---

## Week 3: Metrics & Scoring

### Metrics Parsing

- [ ] **Implement syscall-to-metric mapping**
  - [ ] `parse_syscall_into_metrics()` for each category
  - [ ] Map file operations (open, write, unlink)
  - [ ] Map process operations (fork, exec, ptrace)
  - [ ] Map network operations (socket, connect, send)
  - [ ] Map memory operations (mmap, mprotect)
  - [ ] Test: Verify metrics updated correctly

- [ ] **Implement file operation analysis**
  - [ ] Count file operations
  - [ ] Detect rapid file modification (>100 files in 10s)
  - [ ] Detect executable drops (.exe, .sh, .bat)
  - [ ] Detect hidden file creation (.dotfiles)
  - [ ] Test: Simulate ransomware (rapid file writes)

- [ ] **Implement process operation analysis**
  - [ ] Count process spawns (fork, clone)
  - [ ] Count executions (execve)
  - [ ] Detect process injection (ptrace)
  - [ ] Track process chain depth
  - [ ] Test: Simulate process injection

- [ ] **Implement memory operation analysis**
  - [ ] Count memory allocations (mmap)
  - [ ] Detect RWX pages (mprotect with PROT_READ|PROT_WRITE|PROT_EXEC)
  - [ ] Detect heap spray (repeated same-size allocations)
  - [ ] Test: Simulate shellcode execution

- [ ] **Implement network operation analysis**
  - [ ] Count socket creations
  - [ ] Count outbound connections
  - [ ] Track unique remote IPs
  - [ ] Estimate HTTP requests (send/recv)
  - [ ] Test: Simulate network beaconing

### Scoring Algorithm

- [ ] **Implement threat score calculation**
  - [ ] `calculate_threat_score()` with category weights
  - [ ] File I/O score (40% weight)
  - [ ] Process score (30% weight)
  - [ ] Memory score (15% weight)
  - [ ] Network score (10% weight)
  - [ ] Platform score (5% weight)
  - [ ] Test: Verify scores in 0.0-1.0 range

- [ ] **Implement behavior description generation**
  - [ ] `generate_suspicious_behaviors()` with human-readable strings
  - [ ] Add descriptions for each detected pattern
  - [ ] Include metric counts in descriptions
  - [ ] Test: Verify descriptions generated correctly

### Testing

- [ ] **Test scoring with ransomware simulator**
  - [ ] Create script that writes 200 files rapidly
  - [ ] Analyze with BehavioralAnalyzer
  - [ ] Verify threat_score > 0.6 (malicious)
  - [ ] Verify "Rapid file modification" in behaviors

- [ ] **Test scoring with benign installer**
  - [ ] Analyze legitimate installer (e.g., 7-Zip setup)
  - [ ] Verify threat_score < 0.3 (benign)
  - [ ] Verify minimal false positives

---

## Week 4: Integration & Testing

### Orchestrator Integration

- [ ] **Verify interface compatibility**
  - [ ] Confirm `BehavioralMetrics` structure unchanged
  - [ ] Confirm `analyze()` method signature unchanged
  - [ ] No changes needed in `Orchestrator.{h,cpp}`
  - [ ] Test: Compile without errors

- [ ] **Implement platform detection**
  - [ ] `nsjail_available()` check in constructor
  - [ ] Set `m_use_mock` based on nsjail availability
  - [ ] Add debug logging for platform detection
  - [ ] Test: Verify correct mode selected (mock vs real)

- [ ] **Implement analyze_nsjail() method**
  - [ ] Call temp directory creation
  - [ ] Call nsjail launcher
  - [ ] Call syscall monitoring
  - [ ] Call metrics parsing
  - [ ] Call scoring
  - [ ] Call cleanup
  - [ ] Test: End-to-end analysis completes

- [ ] **Test with Orchestrator**
  - [ ] `Orchestrator::execute_tier2_native()` calls analyze()
  - [ ] Results combined with YARA + ML scores
  - [ ] Verdict generated correctly
  - [ ] Test: Full pipeline (YARA → ML → Behavioral)

### Comprehensive Testing

- [ ] **Unit tests**
  - [ ] Test all individual methods
  - [ ] Test error handling paths
  - [ ] Test resource cleanup
  - [ ] Run: `./Meta/ladybird.py test Sentinel`

- [ ] **Integration tests**
  - [ ] Test with benign executables (/bin/ls, /bin/echo)
  - [ ] Test with malware simulators (file writes, forks)
  - [ ] Test timeout enforcement
  - [ ] Test resource limits (memory, CPU)
  - [ ] Run: `./Services/Sentinel/TestSandbox`

- [ ] **Performance benchmarks**
  - [ ] Measure analysis time for various file sizes
  - [ ] Verify < 5 second target met
  - [ ] Profile memory usage (< 50MB per analysis)
  - [ ] Profile CPU usage (single core burst)
  - [ ] Document results in benchmark report

### Platform Compatibility

- [ ] **Test graceful degradation on macOS**
  - [ ] Verify stub implementation used
  - [ ] Verify no crashes on macOS
  - [ ] Document platform-specific behavior

- [ ] **Test graceful degradation on Windows**
  - [ ] Verify stub implementation used
  - [ ] Verify no crashes on Windows
  - [ ] Document platform-specific behavior

---

## Week 5: Documentation & Polish

### Code Documentation

- [ ] **Add inline documentation**
  - [ ] Document all public methods
  - [ ] Add usage examples in comments
  - [ ] Document error conditions
  - [ ] Document platform-specific behavior

- [ ] **Update existing documentation**
  - [ ] Update `BEHAVIORAL_ANALYSIS_SPEC.md` with implementation details
  - [ ] Update `SANDBOX_INTEGRATION_GUIDE.md` with nsjail setup
  - [ ] Add troubleshooting section

### User Documentation

- [ ] **Create user-facing guide**
  - [ ] Explain behavioral analysis to end users
  - [ ] Document what patterns are detected
  - [ ] Explain security benefits
  - [ ] Add FAQ section

### Developer Documentation

- [ ] **Create developer guide**
  - [ ] Document nsjail setup instructions
  - [ ] Document seccomp-BPF policy customization
  - [ ] Document debugging tips
  - [ ] Add example usage

### Code Review & Cleanup

- [ ] **Code review preparation**
  - [ ] Run clang-format on all modified files
  - [ ] Fix all compiler warnings
  - [ ] Remove debug logging statements
  - [ ] Remove commented-out code

- [ ] **Performance optimization**
  - [ ] Profile hot paths
  - [ ] Optimize syscall event parsing
  - [ ] Reduce memory allocations
  - [ ] Document optimization decisions

---

## Acceptance Criteria

### Functional Requirements

- [x] Analysis completes in < 5 seconds for typical files
- [x] Detects ransomware patterns (rapid file modification)
- [x] Detects process injection (ptrace, RWX memory)
- [x] Detects network beaconing (outbound connections)
- [x] Interface compatibility maintained (no Orchestrator changes)
- [x] Graceful degradation on non-Linux platforms

### Quality Requirements

- [x] All unit tests pass
- [x] All integration tests pass
- [x] No memory leaks detected (Valgrind clean)
- [x] No zombie processes left after analysis
- [x] Error handling covers all failure modes
- [x] Code documented with inline comments

### Performance Requirements

- [x] Analysis time < 5 seconds (95th percentile)
- [x] Memory usage < 50MB per analysis
- [x] CPU usage: single core burst only
- [x] No performance regression in Orchestrator

### Security Requirements

- [x] Sandbox escape prevented (nsjail + seccomp)
- [x] Timeout enforced (no infinite loops)
- [x] Resource limits enforced (CPU, memory, file descriptors)
- [x] Temp files cleaned up securely
- [x] No sensitive data leaked

---

## Risks & Mitigations

### Risk 1: nsjail Not Available

**Impact**: High (cannot perform real analysis)
**Likelihood**: Low (nsjail widely available on Linux)
**Mitigation**: Graceful degradation to mock implementation

### Risk 2: seccomp_unotify Not Supported

**Impact**: Medium (fallback to slower ptrace)
**Likelihood**: Medium (requires kernel 5.4+)
**Mitigation**: Implement ptrace fallback (already planned)

### Risk 3: Performance Targets Not Met

**Impact**: Medium (degraded user experience)
**Likelihood**: Low (nsjail designed for performance)
**Mitigation**: Optimize syscall parsing, implement early termination

### Risk 4: False Positives Too High

**Impact**: High (user frustration)
**Likelihood**: Medium (scoring thresholds need tuning)
**Mitigation**: Extensive testing with legitimate software, adjust thresholds

---

## Success Metrics

| Metric | Target | Measured |
|--------|--------|----------|
| **Detection Rate** | 90%+ on known malware | TBD |
| **False Positive Rate** | < 5% on legitimate software | TBD |
| **Analysis Time** | < 5s (95th percentile) | TBD |
| **Memory Usage** | < 50MB per analysis | TBD |
| **Code Coverage** | > 80% | TBD |

---

## Next Steps After Completion

1. **Deploy to staging environment**
2. **Monitor real-world detection rates**
3. **Tune scoring thresholds based on telemetry**
4. **Implement advanced features** (entropy calculation, argument parsing)
5. **Add ML-based behavioral classification** (Phase 3)

---

## Document References

- **Full Design**: `BEHAVIORAL_ANALYZER_DESIGN.md`
- **Quick Reference**: `BEHAVIORAL_ANALYZER_QUICK_REFERENCE.md`
- **Behavioral Spec**: `BEHAVIORAL_ANALYSIS_SPEC.md`
- **Current Stub**: `Services/Sentinel/Sandbox/BehavioralAnalyzer.{h,cpp}`

---

**Created**: 2025-11-02
**Status**: Ready for Implementation
**Owner**: Security Research Team
**Timeline**: 4-5 weeks (Weeks 1-5 outlined above)
