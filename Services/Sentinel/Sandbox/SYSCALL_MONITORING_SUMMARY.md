# Syscall Monitoring Architecture - Executive Summary

**Document**: Week 2 Syscall Monitoring Design
**Status**: ✅ Design Complete - Ready for Implementation
**Created**: 2025-11-02
**Full Design**: `SYSCALL_MONITORING_DESIGN.md` (1,176 lines)

---

## Key Decisions

### 1. I/O Strategy: Non-Blocking poll()

✅ **DECISION**: Use `poll()` with non-blocking stderr FD

**Why**:
- Prevents deadlock from full pipe buffers
- Single-threaded (no synchronization overhead)
- 100ms poll interval balances responsiveness vs CPU usage
- Standard POSIX API (portable)

**Code Pattern**:
```cpp
struct pollfd pfd = { .fd = stderr_fd, .events = POLLIN };
poll(&pfd, 1, 100);  // 100ms timeout
if (pfd.revents & POLLIN) {
    read_and_parse_events();
}
```

---

### 2. Event Format: nsjail stderr Logs

✅ **EXPECTED FORMAT**: `[SYSCALL] syscall_name(args...)\n`

**Examples**:
```
[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)
[SYSCALL] fork()
[SYSCALL] execve(path="/bin/sh", argv=...)
[SYSCALL] ptrace(PTRACE_ATTACH, pid=1234)
```

**Source**: nsjail seccomp-bpf LOG action (configured in `malware-sandbox.cfg`)

---

### 3. Parser Design: Name-Only (Phase 2)

✅ **DECISION**: Parse syscall **name only** in Phase 2, defer argument parsing to Phase 3

**Rationale**:
- 80% detection value from syscall name alone
- Simpler implementation (no quoted string handling)
- Argument parsing adds complexity (defer optimization)

**Parser Logic**:
```cpp
// Input: "[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)\n"
// Output: SyscallEvent { name: "socket", args: [...] }

1. Check prefix "[SYSCALL] "
2. Extract name until '('
3. (Phase 2) Ignore arguments
4. (Phase 3) Parse arguments for advanced detection
```

---

### 4. Metric Mapping: Weighted Syscalls

✅ **DECISION**: Each syscall increments specific BehavioralMetrics counters with weights

**Key Mappings**:

| Syscall | Metric | Weight | Reason |
|---------|--------|--------|--------|
| `connect` | `outbound_connections` | +3 | **C2 communication** |
| `ptrace` | `code_injection_attempts` | +5 | **Process injection** |
| `execve` | `process_operations` | +3 | **Execution** |
| `unlink` | `file_operations` | +2 | **Destructive file op** |
| `setuid` | `privilege_escalation_attempts` | +5 | **Privilege escalation** |
| `mprotect` | `self_modification_attempts` | +4 | **RWX memory pages** |

**Full Table**: See Section 5 of `SYSCALL_MONITORING_DESIGN.md`

---

### 5. Performance Targets

✅ **REQUIREMENTS**:

- **Total analysis time**: < 5 seconds (95th percentile)
- **Syscall monitoring overhead**: < 500ms
- **Parse rate**: > 10,000 syscalls/second
- **Memory usage**: < 10MB for event buffer

**Optimization Strategies**:
1. 100ms poll interval (not 10ms)
2. 4KB read buffer (one page)
3. Early termination on critical threats
4. StringView parsing (no allocations in hot path)

---

### 6. Error Handling: Graceful Degradation

✅ **PHILOSOPHY**: Partial metrics are better than complete failure

**Error Scenarios**:

| Error | Recovery | Impact |
|-------|----------|--------|
| Pipe full (EAGAIN) | Skip read, continue | ⚠️ Lost events (acceptable) |
| Parse error | Log warning, skip line | ✅ Continue monitoring |
| Buffer overflow | Clear buffer | ⚠️ Lost partial line |
| Process timeout | SIGKILL, set `timed_out=true` | ✅ Controlled termination |

**Code Pattern**:
```cpp
auto result = parse_syscall_line(line);
if (result.is_error()) {
    dbgln("Parse error: {}", result.error());
    // Continue with next line (don't abort)
}
```

---

## Integration into analyze_nsjail()

### Main Loop Structure

```cpp
ErrorOr<BehavioralMetrics> analyze_nsjail(file_data, filename) {
    // 1. Write file to sandbox directory
    auto file_path = write_file_to_sandbox(...);

    // 2. Build nsjail command
    auto nsjail_command = build_nsjail_command(file_path);

    // 3. Launch nsjail → SandboxProcess { pid, stdin_fd, stdout_fd, stderr_fd }
    auto process = launch_nsjail_sandbox(nsjail_command);

    // 4. Set stderr to non-blocking
    fcntl(process.stderr_fd, F_SETFL, O_NONBLOCK);

    // 5. Main monitoring loop
    struct pollfd pfd = { .fd = process.stderr_fd, .events = POLLIN };
    ByteBuffer line_buffer;
    BehavioralMetrics metrics;

    while (!process_exited && !timed_out) {
        // Poll stderr (100ms timeout)
        poll(&pfd, 1, 100);

        if (pfd.revents & POLLIN) {
            read_and_parse_syscall_events(process.stderr_fd, line_buffer, metrics);
        }

        // Check timeout and process status
        check_timeout();
        check_process_status();
    }

    // 6. Return metrics
    return metrics;
}
```

---

## Open Questions (Resolved)

### Q1: Should we parse arguments in Phase 2?
**Answer**: ❌ No, defer to Phase 3
- Phase 2: Syscall name only (80% value, simpler)
- Phase 3: Add IP extraction, path analysis, etc.

### Q2: How to handle nsjail's own syscalls?
**Answer**: Phase 2: Accept all syscalls (slight noise, acceptable)
- Phase 3: Parse PID from logs, filter to child process

### Q3: What if stderr buffer fills up?
**Answer**: Non-blocking I/O prevents deadlock
- Lost events reduce granularity but don't break analysis
- Graceful degradation

### Q4: Separate thread for monitoring?
**Answer**: ❌ No, single-threaded with poll() is sufficient
- No synchronization overhead
- Simpler debugging
- Performance target easily met

---

## Implementation Roadmap (Week 2)

### Day 1-2: Core Monitoring
- [ ] Implement `monitor_syscall_events()` with poll()
- [ ] Implement `read_and_parse_syscall_events()` with line buffering
- [ ] Test: Verify non-blocking I/O works

### Day 2-3: Parser
- [ ] Implement `parse_syscall_line()` with regex
- [ ] Extract syscall name (ignore arguments for now)
- [ ] Test: Parse all syscall formats from `malware-sandbox.cfg`

### Day 3-4: Metric Mapping
- [ ] Implement `update_metrics_from_syscall()` with weight table
- [ ] Map all LOG syscalls to BehavioralMetrics fields
- [ ] Test: Verify counters increment correctly

### Day 4-5: Integration
- [ ] Integrate into `analyze_nsjail()` main loop
- [ ] Add timeout enforcement during monitoring
- [ ] Add process status checking (waitpid WNOHANG)
- [ ] Test: End-to-end analysis with real binaries

### Day 5-6: Testing & Optimization
- [ ] Unit tests for parser and metric updates
- [ ] Integration tests with test samples
- [ ] Performance benchmarking (< 5s target)
- [ ] Error handling tests (buffer overflow, parse errors)

---

## Success Criteria

✅ **Functional**:
- Parse syscall events from nsjail stderr
- Update BehavioralMetrics correctly for all categories
- Complete analysis in < 5 seconds
- No deadlocks or buffer overflows

✅ **Quality**:
- All unit tests pass
- All integration tests pass
- Graceful error handling for edge cases
- Debug logging for troubleshooting

✅ **Performance**:
- Syscall monitoring overhead < 500ms
- Parse rate > 10,000 events/second
- Memory usage < 10MB

---

## Testing Plan

### Unit Tests
```cpp
TEST_CASE(parse_syscall_socket)
TEST_CASE(parse_syscall_fork)
TEST_CASE(parse_syscall_invalid)
TEST_CASE(metric_update_network)
TEST_CASE(metric_update_process)
TEST_CASE(buffer_overflow_resilience)
```

### Integration Tests
```bash
# Test 1: Benign binary
./analyze_nsjail /bin/ls
# Expected: < 1s, 10-50 syscalls

# Test 2: Network beacon
./analyze_nsjail ./test_samples/network_beacon.sh
# Expected: < 2s, network_operations > 5

# Test 3: Ransomware sim
./analyze_nsjail ./test_samples/ransomware_sim.sh
# Expected: < 4s, file_operations > 100

# Test 4: Timeout test
./analyze_nsjail ./test_samples/infinite_loop.sh
# Expected: Timeout at 5s, timed_out=true
```

---

## References

- **Full Design**: `SYSCALL_MONITORING_DESIGN.md` (1,176 lines)
- **Implementation Checklist**: `BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md` Week 2
- **nsjail Config**: `configs/malware-sandbox.cfg`
- **BehavioralAnalyzer API**: `BehavioralAnalyzer.{h,cpp}`

---

## Next Steps

1. ✅ **Design complete** - Review with team
2. ⏭️ **Begin Week 2 Day 1** - Implement core monitoring loop
3. ⏭️ **Parallel work possible** - Parser and metric mapping can be developed concurrently

---

**Status**: 🟢 Ready for Implementation
**Blockers**: None
**Est. Completion**: Week 2 (5-6 days)
**Owner**: Security Research Team
