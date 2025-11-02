# Week 2 Syscall Monitoring - Document Index

**Milestone**: 0.5 Phase 2 Week 2
**Status**: ✅ Design Complete - Ready for Implementation
**Created**: 2025-11-02

---

## Quick Navigation

### 🚀 Start Here

**If you're implementing syscall monitoring, start with**:
1. Read `WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md` (16KB) - Overview and decisions
2. Skim `SYSCALL_MONITORING_SUMMARY.md` (8.2KB) - Key points and roadmap
3. Reference `SYSCALL_MONITORING_DESIGN.md` (40KB) - Full specification during implementation

---

## Document Structure

### 1. WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md (16KB)

**Purpose**: Completion report for design phase

**Contents**:
- Executive summary
- Design highlights (7 key decisions)
- Implementation roadmap (Week 2 Days 1-6)
- Success criteria
- Autonomous decision log
- Risk assessment

**Audience**: Project managers, stakeholders, code reviewers

**When to read**: After design phase, before implementation kickoff

---

### 2. SYSCALL_MONITORING_SUMMARY.md (8.2KB)

**Purpose**: Quick reference for implementers

**Contents**:
- Key decisions (6 major choices)
- Integration code snippet
- Metric mapping table (excerpt)
- Implementation roadmap (checklist format)
- Testing plan
- Open questions (all resolved)

**Audience**: Implementation team, quick onboarding

**When to read**: Daily reference during Week 2 implementation

---

### 3. SYSCALL_MONITORING_DESIGN.md (40KB, 1,176 lines)

**Purpose**: Complete architecture specification

**Contents**:
- Section 1: Architecture Overview (flow diagram)
- Section 2: Pipe I/O Strategy (poll() design)
- Section 3: Event Format Specification (nsjail logs)
- Section 4: Parser Design (state machine)
- Section 5: Metric Mapping Table (40+ syscalls, complete)
- Section 6: Performance Considerations (optimization strategies)
- Section 7: Error Handling (graceful degradation)
- Section 8: Integration into analyze_nsjail() (complete implementation)
- Section 9: Testing Strategy (unit + integration tests)
- Section 10: Open Questions (all resolved)

**Audience**: Implementers, deep technical reference

**When to read**: During implementation, section-by-section as needed

---

## Document Relationships

```
WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md
    ↓ (summarizes)
SYSCALL_MONITORING_DESIGN.md (full spec)
    ↓ (distills to)
SYSCALL_MONITORING_SUMMARY.md (quick ref)
```

**Reading Strategy**:
- **First time**: Report → Summary → Design (sections as needed)
- **Implementation**: Summary (daily), Design (section reference)
- **Code review**: Report (context), Design (technical details)

---

## Key Design Decisions (Quick Lookup)

### Decision #1: I/O Strategy
✅ **Non-blocking poll()** with 100ms interval
- Prevents deadlock, single-threaded, standard POSIX
- See: Design Section 2, Summary "Key Decisions #1"

### Decision #2: Parser Scope
✅ **Name-only parsing** in Phase 2
- 80% value, defer arguments to Phase 3
- See: Design Section 4, Summary "Key Decisions #3"

### Decision #3: Metric Mapping
✅ **Weighted syscall counters** (weights 1-5)
- `ptrace` = 5 (critical), `mmap` = 1 (benign)
- See: Design Section 5 (full table), Summary (excerpt)

### Decision #4: Error Handling
✅ **Graceful degradation** philosophy
- Partial metrics > complete failure
- See: Design Section 7, Report "Design Highlights #4"

### Decision #5: Performance Targets
✅ **< 5s total, < 500ms monitoring overhead**
- 100ms poll, 4KB buffers, early termination
- See: Design Section 6, Summary "Performance Targets"

### Decision #6: Integration Pattern
✅ **Main loop with timeout enforcement**
- poll() + check_timeout() + check_process_status()
- See: Design Section 8, Summary "Integration into analyze_nsjail()"

---

## Implementation Roadmap (Checklist)

### Week 2 Day 1-2: Core Monitoring
- [ ] Implement `monitor_syscall_events()` with poll()
- [ ] Implement `read_and_parse_syscall_events()` with line buffering
- [ ] Test: Verify non-blocking I/O works

### Week 2 Day 2-3: Parser
- [ ] Implement `parse_syscall_line()` with regex
- [ ] Extract syscall name (ignore arguments)
- [ ] Test: Parse all syscall formats

### Week 2 Day 3-4: Metric Mapping
- [ ] Implement `update_metrics_from_syscall()` with weight table
- [ ] Map all 40+ syscalls to metrics
- [ ] Test: Verify counters increment correctly

### Week 2 Day 4-5: Integration
- [ ] Integrate into `analyze_nsjail()` main loop
- [ ] Add timeout enforcement during monitoring
- [ ] Add process status checking
- [ ] Test: End-to-end analysis

### Week 2 Day 5-6: Testing & Optimization
- [ ] Unit tests (parser, metric updates)
- [ ] Integration tests (real binaries)
- [ ] Performance benchmarking
- [ ] Error handling tests

**See**: Summary "Implementation Roadmap" for detailed checklist

---

## Code References

### Current Implementation (Stub)
- `BehavioralAnalyzer.h` (lines 228-242) - Method stubs for syscall monitoring
- `BehavioralAnalyzer.cpp` (lines 358-370) - Stub `analyze_nsjail()` method

### Week 1 Completion (Infrastructure)
- `BehavioralAnalyzer.cpp` (lines 560-665) - `launch_nsjail_sandbox()` (complete)
- `BehavioralAnalyzer.cpp` (lines 667-791) - Timeout enforcement (complete)
- `BehavioralAnalyzer.cpp` (lines 95-184) - File handling (complete)

### Configuration
- `configs/malware-sandbox.cfg` (lines 185-389) - Seccomp-BPF LOG policy
- `configs/malware-sandbox.kafel` - Kafel DSL policy (alternative)

**See**: Report "References" section for full file paths

---

## Testing Plan

### Unit Tests (TestBehavioralAnalyzer.cpp)
```cpp
TEST_CASE(parse_syscall_event_socket)        // Parse "socket(...)"
TEST_CASE(parse_syscall_event_fork)          // Parse "fork()"
TEST_CASE(parse_syscall_event_invalid)       // Invalid lines ignored
TEST_CASE(metric_update_network)             // socket/connect counters
TEST_CASE(metric_update_process)             // fork/execve counters
TEST_CASE(buffer_overflow_resilience)        // 10,000 events
```

### Integration Tests (test_syscall_monitoring.sh)
```bash
# Test 1: /bin/ls → < 1s, 10-50 syscalls
# Test 2: Network beacon → < 2s, network_operations > 5
# Test 3: Ransomware sim → < 4s, file_operations > 100
# Test 4: Timeout test → 5s timeout, timed_out=true
```

**See**: Design Section 9, Summary "Testing Plan"

---

## Performance Targets

| Metric | Target | Measured |
|--------|--------|----------|
| **Total analysis time** | < 5s (95th percentile) | TBD |
| **Monitoring overhead** | < 500ms | TBD |
| **Parse rate** | > 10,000 syscalls/second | TBD |
| **Memory usage** | < 10MB | TBD |

**Benchmarking**: See Design Section 6 "Benchmarking Plan"

---

## Related Documents

### Week 1 Deliverables
- `WEEK1_DAY4-5_COMPLETION_REPORT.md` - nsjail launcher implementation
- `NSJAIL_LAUNCHER_SUMMARY.md` - Process management infrastructure

### Implementation Context
- `BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md` - Week 2 section
- `BehavioralAnalyzer.{h,cpp}` - Current stub implementation

### Specification
- `docs/BEHAVIORAL_ANALYSIS_SPEC.md` - High-level behavioral analysis spec
- `docs/BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md` - API design

---

## Document Change Log

| Date | Document | Change |
|------|----------|--------|
| 2025-11-02 | SYSCALL_MONITORING_DESIGN.md | Initial creation (40KB, 1,176 lines) |
| 2025-11-02 | SYSCALL_MONITORING_SUMMARY.md | Initial creation (8.2KB) |
| 2025-11-02 | WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md | Completion report (16KB) |
| 2025-11-02 | WEEK2_INDEX.md | This index document |

---

## Next Steps

### For Implementation Team
1. ✅ Read `WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md` for context
2. ✅ Bookmark `SYSCALL_MONITORING_SUMMARY.md` for daily reference
3. ⏭️ Begin Week 2 Day 1 implementation (core monitoring loop)
4. ⏭️ Reference `SYSCALL_MONITORING_DESIGN.md` section-by-section during implementation

### For Code Reviewers
1. ✅ Read `WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md` for design rationale
2. ⏭️ Review implementation against `SYSCALL_MONITORING_DESIGN.md` (Section 8)
3. ⏭️ Verify test coverage matches `SYSCALL_MONITORING_DESIGN.md` (Section 9)

### For Project Managers
1. ✅ Read `WEEK2_SYSCALL_MONITORING_DESIGN_REPORT.md` for status
2. ✅ Track progress against roadmap in report
3. ⏭️ Midpoint check-in: Week 2 Day 3
4. ⏭️ Week 2 completion review: Week 2 Day 6

---

## Quick Answers

**Q: Where's the metric mapping table?**
A: Full table in `SYSCALL_MONITORING_DESIGN.md` Section 5, excerpt in `SYSCALL_MONITORING_SUMMARY.md` Section 4

**Q: How do I implement the main loop?**
A: Complete implementation in `SYSCALL_MONITORING_DESIGN.md` Section 8, pseudocode in `SYSCALL_MONITORING_SUMMARY.md`

**Q: What are the performance targets?**
A: `SYSCALL_MONITORING_DESIGN.md` Section 6, summary in `SYSCALL_MONITORING_SUMMARY.md` Section 6

**Q: How do I test syscall monitoring?**
A: `SYSCALL_MONITORING_DESIGN.md` Section 9 (complete), `SYSCALL_MONITORING_SUMMARY.md` "Testing Plan" (summary)

**Q: What if I encounter an error scenario?**
A: `SYSCALL_MONITORING_DESIGN.md` Section 7 "Error Handling" (complete table)

**Q: Which syscalls map to which metrics?**
A: `SYSCALL_MONITORING_DESIGN.md` Section 5 (40+ syscalls mapped)

---

**Index Status**: ✅ Complete
**Design Status**: ✅ Ready for Implementation
**Next Milestone**: Week 2 Day 6 - Syscall monitoring implementation complete
