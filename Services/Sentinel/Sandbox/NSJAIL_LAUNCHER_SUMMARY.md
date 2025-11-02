# nsjail Launcher Design - Quick Summary

**Document**: NSJAIL_LAUNCHER_SUMMARY.md
**Full Design**: NSJAIL_LAUNCHER_DESIGN.md (1200 lines)
**Date**: 2025-11-02
**Status**: ✅ Approved for Implementation

---

## Key Design Decisions

### 1. Configuration Strategy: Use Config Files ✅

**Decision**: Use `-C config.cfg` with existing protobuf config files

**Command Template**:
```bash
nsjail -C malware-sandbox.cfg --bindmount /tmp/sentinel-XXX:/sandbox -- /sandbox/binary
```

**Why?**
- ✅ Simpler: 7 args vs 40+ inline args
- ✅ Maintainable: Security policies in version control
- ✅ Native format: No manual escaping/quoting
- ✅ Hot-reloadable: Change policy without recompiling

**Rejected Alternative**: Build 40+ inline command-line arguments (too complex, error-prone)

---

## 2. Data Structures

### NsjailCommandArgs
```cpp
struct NsjailCommandArgs {
    String config_file_path;           // /path/to/malware-sandbox.cfg
    String sandbox_directory;          // /tmp/sentinel-XXXXXX (dynamic)
    String executable_path;            // /sandbox/binary (inside container)
    Vector<String> additional_mounts;  // Optional extra mounts
    Optional<u32> timeout_override;    // Override config timeout
    Optional<u64> memory_override;     // Override config memory limit
    bool verbose { false };
};
```

### SandboxPipes (RAII)
```cpp
struct SandboxPipes {
    int stdout_pipe[2] { -1, -1 };
    int stderr_pipe[2] { -1, -1 };

    ErrorOr<void> create_pipes();
    void close_read_ends();   // Parent before fork
    void close_write_ends();  // Child before exec
    void close_all();         // Cleanup

    ~SandboxPipes() { close_all(); }
};
```

### SandboxExecutionResult
```cpp
struct SandboxExecutionResult {
    int exit_code { -1 };
    bool timed_out { false };
    bool killed_by_signal { false };
    int signal_number { 0 };

    ByteBuffer stdout_output;         // Program output
    ByteBuffer stderr_output;         // nsjail logs + seccomp violations
    Duration execution_time;

    Vector<SyscallEvent> syscall_events;  // Parsed from stderr
};
```

---

## 3. API Design

### Command Builder
```cpp
ErrorOr<Vector<String>> build_nsjail_command(NsjailCommandArgs const& args)
{
    Vector<String> command;
    TRY(command.try_append("nsjail"_string));
    TRY(command.try_append("-C"_string));
    TRY(command.try_append(args.config_file_path));
    TRY(command.try_append("--bindmount"_string));
    TRY(command.try_append(TRY(String::formatted("{}:/sandbox", args.sandbox_directory))));
    TRY(command.try_append("--"_string));
    TRY(command.try_append(args.executable_path));
    return command;
}
```

### Process Launcher
```cpp
ErrorOr<SandboxExecutionResult> launch_nsjail_sandbox(
    NsjailCommandArgs const& args,
    Duration timeout)
{
    // 1. Build command
    auto command = TRY(build_nsjail_command(args));

    // 2. Create pipes
    SandboxPipes pipes;
    TRY(pipes.create_pipes());

    // 3. Fork child
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child: redirect stdout/stderr to pipes, exec nsjail
        TRY(Core::System::dup2(pipes.stdout_pipe[1], STDOUT_FILENO));
        TRY(Core::System::dup2(pipes.stderr_pipe[1], STDERR_FILENO));
        execvp("nsjail", argv);
        _exit(127);
    }

    // 4. Parent: setup timeout, read pipes, wait for child
    auto alarm_handle = TRY(setup_timeout_alarm(child_pid, timeout));
    auto stdout_data = TRY(read_pipe_nonblocking(pipes.stdout_pipe[0], timeout));
    auto stderr_data = TRY(read_pipe_nonblocking(pipes.stderr_pipe[0], timeout));

    int status;
    waitpid(child_pid, &status, 0);

    // 5. Parse result
    SandboxExecutionResult result;
    result.stdout_output = move(stdout_data);
    result.stderr_output = move(stderr_data);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.timed_out = (WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);

    return result;
}
```

---

## 4. IPC Architecture

### Pipe Flow Diagram
```
Parent Process              Child Process (nsjail)
==============              ======================

[Create pipes]
  stdout_pipe[2]
  stderr_pipe[2]

fork() ───────────────────> [Child created]

[Close write ends]          [Close read ends]
  close(pipe[1])              close(pipe[0])

                            [Redirect stdout/stderr]
                            dup2(pipe[1], STDOUT_FILENO)
                            dup2(pipe[1], STDERR_FILENO)

                            [execvp(nsjail)]
                            [Program writes to stdout/stderr]

[Read from pipes]
  poll(pipe[0])
  read(pipe[0], buf)

[Parse output]
  syscall_events = parse(stderr)

[Wait for child]
  waitpid(child_pid)

[Cleanup]
  close(pipes)
  cleanup_temp_dir()
```

### Non-blocking Pipe Reading
```cpp
ErrorOr<ByteBuffer> read_pipe_nonblocking(int fd, Duration timeout)
{
    TRY(set_nonblocking(fd));  // fcntl(fd, F_SETFL, O_NONBLOCK)

    ByteBuffer output;
    auto deadline = MonotonicTime::now() + timeout;

    while (MonotonicTime::now() < deadline) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int remaining_ms = (deadline - MonotonicTime::now()).to_milliseconds();

        int poll_result = poll(&pfd, 1, remaining_ms);
        if (poll_result <= 0) break;  // Timeout or error

        u8 buffer[4096];
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes <= 0) break;  // EOF or error

        TRY(output.try_append(buffer, bytes));
    }

    return output;
}
```

---

## 5. Timeout Enforcement

### alarm() + SIGKILL Strategy
```cpp
// Global state for signal handler (Linux limitation)
static pid_t g_sandboxed_child_pid = -1;

static void sigalrm_handler(int) {
    if (g_sandboxed_child_pid > 0)
        kill(g_sandboxed_child_pid, SIGKILL);  // Uncatchable
}

ErrorOr<AlarmHandle> setup_timeout_alarm(pid_t child_pid, Duration timeout)
{
    struct sigaction sa;
    sa.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa, nullptr);

    g_sandboxed_child_pid = child_pid;
    alarm(timeout.to_seconds());

    return AlarmHandle { .active = true };
}

ErrorOr<void> cancel_timeout_alarm(AlarmHandle handle)
{
    if (handle.active) {
        alarm(0);  // Cancel alarm
        g_sandboxed_child_pid = -1;
    }
    return {};
}
```

**Why alarm()?**
- ✅ Simple and reliable
- ✅ SIGKILL cannot be caught/ignored
- ✅ Portable across Unix systems
- ✅ Proven pattern (used by timeout(1) command)

---

## 6. Error Handling

### Error Categories

| Error | Severity | Action |
|-------|----------|--------|
| nsjail not found | High | Use mock, log warning |
| Config file missing | High | Use inline policy fallback |
| fork() failure | Critical | Return error to Orchestrator |
| execvp() failure | Critical | Child exits 127 |
| pipe() failure | Medium | Use /dev/null |
| Timeout | Expected | Kill child, mark timed_out=true |
| Zombie process | Critical | MUST call waitpid() |

### Resource Cleanup Checklist
✅ **MUST cleanup on all paths (success, error, timeout)**:
1. Child process → `waitpid()` to reap zombie
2. Pipe FDs → Close both read and write ends
3. Alarm → Cancel `alarm()` to prevent false timeouts
4. Temp directory → Remove sandbox files
5. Global state → Reset `g_sandboxed_child_pid`

### RAII Pattern
```cpp
struct ScopedSandbox {
    pid_t child_pid { -1 };
    SandboxPipes pipes;
    AlarmHandle alarm;
    String sandbox_dir;

    ~ScopedSandbox() {
        if (child_pid > 0) {
            kill(child_pid, SIGKILL);
            waitpid(child_pid, nullptr, 0);
        }
        if (alarm.active) alarm(0);
        if (!sandbox_dir.is_empty()) cleanup_temp_directory(sandbox_dir);
    }
};
```

---

## 7. Platform Compatibility

### Linux Support ✅
- **Minimum**: Linux kernel 3.8+ (basic nsjail)
- **Recommended**: Linux kernel 5.4+ (seccomp_unotify)
- **Dependencies**: nsjail, libseccomp-dev

### Detection Logic
```cpp
ErrorOr<bool> detect_nsjail_availability()
{
#if !defined(__linux__)
    return false;  // Only Linux supported
#else
    // Check if nsjail binary exists and is executable
    auto which_result = Core::System::exec("/usr/bin/which"sv, { "nsjail"sv });
    if (which_result.is_error()) return false;

    auto nsjail_path = which_result.release_value();
    return Core::System::access(nsjail_path, X_OK).is_error() ? false : true;
#endif
}
```

### Graceful Degradation
```cpp
ErrorOr<void> initialize_sandbox()
{
#if defined(__linux__)
    m_nsjail_available = TRY(detect_nsjail_availability());
    m_use_mock = !m_nsjail_available;
#else
    m_use_mock = true;
    m_nsjail_available = false;
#endif

    m_sandbox_dir = TRY(create_temp_sandbox_directory());
    return {};
}
```

**User Experience**:
- Linux + nsjail → Full behavioral analysis
- Linux - nsjail → Mock analysis with warning
- macOS/Windows → Mock analysis (transparent)

---

## 8. Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Command building | < 1 ms | String concatenation |
| Fork + exec | < 10 ms | OS overhead |
| Pipe setup | < 1 ms | System call |
| Sandbox execution | < 5 sec | User-facing timeout |
| Pipe reading | < 100 ms | Small output typically |
| **Total overhead** | **< 200 ms** | Acceptable for security |

---

## 9. Security Guarantees

| Threat | Mitigation | Layer |
|--------|-----------|-------|
| Sandbox escape | nsjail + seccomp-BPF | Container |
| Resource exhaustion | rlimits (CPU, memory, FDs) | nsjail config |
| Network attacks | Network namespace isolation | nsjail config |
| Filesystem attacks | Mount namespace, RO mounts | nsjail config |
| Privilege escalation | User namespace (UID→nobody) | nsjail config |
| Timeout bypass | SIGKILL (uncatchable) | Orchestrator |
| Temp dir tampering | Secure perms (0700), random names | Week 1 Day 3-4 |

---

## 10. Implementation Checklist (Week 1 Day 4-5)

### Data Structures (1 hour)
- [ ] `NsjailCommandArgs` struct
- [ ] `SandboxPipes` struct with RAII
- [ ] `SandboxExecutionResult` struct
- [ ] `SyscallEvent` struct

### Command Builder (2 hours)
- [ ] `build_nsjail_command()` implementation
- [ ] `get_nsjail_config_path()` with fallback paths
- [ ] Unit tests for command generation

### IPC Infrastructure (2 hours)
- [ ] `SandboxPipes::create_pipes()`
- [ ] `SandboxPipes::close_*()` methods
- [ ] `read_pipe_nonblocking()` with timeout
- [ ] `set_nonblocking()` helper

### Process Launcher (4 hours)
- [ ] `launch_nsjail_sandbox()` with fork/exec
- [ ] Pipe redirection in child
- [ ] `waitpid()` and exit status parsing
- [ ] Resource cleanup on all paths

### Timeout Enforcement (2 hours)
- [ ] `setup_timeout_alarm()` with signal handler
- [ ] `cancel_timeout_alarm()`
- [ ] Global state management
- [ ] Test timeout with infinite loop

### Error Handling (2 hours)
- [ ] Error handling for all syscalls
- [ ] RAII for resource cleanup
- [ ] Zombie process prevention
- [ ] Platform detection

### Testing (7 hours)
- [ ] Unit tests: command builder
- [ ] Unit tests: pipe management
- [ ] Integration: `/bin/echo` execution
- [ ] Integration: timeout enforcement
- [ ] Integration: config file loading
- [ ] Integration: stderr parsing

### Documentation (2 hours)
- [ ] Inline code documentation
- [ ] Update BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md
- [ ] Troubleshooting section
- [ ] Platform compatibility notes

**Total Estimated Time**: 20 hours (2-3 days)

---

## 11. Integration Example

### From analyze() to nsjail
```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    // Use mock or real implementation
    if (m_use_mock) {
        return analyze_mock(file_data, filename);
    } else {
        return analyze_nsjail(file_data, filename);
    }
}

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail(
    ByteBuffer const& file_data,
    String const& filename)
{
    // [1] Write file to sandbox (Week 1 Day 3-4 complete)
    auto file_path = TRY(write_file_to_sandbox(m_sandbox_dir, file_data, filename));
    TRY(make_executable(file_path));

    // [2] Build nsjail command (Week 1 Day 4-5)
    NsjailCommandArgs args {
        .config_file_path = TRY(get_nsjail_config_path()),
        .sandbox_directory = m_sandbox_dir,
        .executable_path = TRY(String::formatted("/sandbox/{}", filename)),
    };

    // [3] Launch sandbox (Week 1 Day 4-5)
    auto result = TRY(launch_nsjail_sandbox(args, Duration::from_seconds(5)));

    // [4] Parse syscall events into metrics (Week 2)
    BehavioralMetrics metrics;
    for (auto const& event : result.syscall_events) {
        update_metrics_from_syscall(event.syscall_name, metrics);
    }

    // [5] Set execution metadata
    metrics.execution_time = result.execution_time;
    metrics.timed_out = result.timed_out;
    metrics.exit_code = result.exit_code;

    return metrics;
}
```

---

## 12. Testing Examples

### Unit Test: Command Builder
```cpp
TEST_CASE(nsjail_command_builder)
{
    NsjailCommandArgs args {
        .config_file_path = "/path/to/config.cfg"_string,
        .sandbox_directory = "/tmp/sentinel-test"_string,
        .executable_path = "/sandbox/binary"_string,
    };

    auto command = build_nsjail_command(args).release_value();

    EXPECT_EQ(command[0], "nsjail"sv);
    EXPECT_EQ(command[1], "-C"sv);
    EXPECT_EQ(command[2], "/path/to/config.cfg"sv);
    EXPECT(command.contains_slow("--bindmount"sv));
    EXPECT_EQ(command.last(), "/sandbox/binary"sv);
}
```

### Integration Test: Benign Execution
```cpp
TEST_CASE(nsjail_benign_execution)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Write simple script
    auto script = "#!/bin/sh\necho 'Hello'\nexit 0\n"_string;
    auto sandbox_dir = analyzer->create_temp_sandbox_directory().release_value();
    auto script_path = analyzer->write_file_to_sandbox(
        sandbox_dir, ByteBuffer::copy(script.bytes()).release_value(), "test.sh"_string
    ).release_value();
    analyzer->make_executable(script_path).release_value();

    // Launch nsjail
    NsjailCommandArgs args {
        .config_file_path = "/path/to/malware-sandbox.cfg"_string,
        .sandbox_directory = sandbox_dir,
        .executable_path = "/sandbox/test.sh"_string,
    };

    auto result = analyzer->launch_nsjail_sandbox(args, Duration::from_seconds(5)).release_value();

    EXPECT_EQ(result.exit_code, 0);
    EXPECT(!result.timed_out);
    EXPECT(result.stdout_output.bytes().contains("Hello"sv));
}
```

---

## 13. Key Files

### Configuration Files (Already Exist)
- `Services/Sentinel/Sandbox/configs/malware-sandbox.cfg` - nsjail protobuf config
- `Services/Sentinel/Sandbox/configs/malware-sandbox.kafel` - Seccomp-BPF policy

### Source Files (To Implement)
- `Services/Sentinel/Sandbox/BehavioralAnalyzer.h` - Add new methods
- `Services/Sentinel/Sandbox/BehavioralAnalyzer.cpp` - Implement launcher

### Test Files (To Create)
- `Services/Sentinel/Test/TestBehavioralAnalyzer.cpp` - Unit tests
- `Services/Sentinel/test_nsjail_integration.sh` - Integration tests

---

## 14. Success Criteria

✅ **Functional**:
- Command builds correctly from config file
- Fork/exec launches nsjail successfully
- Pipes capture stdout/stderr
- Timeout kills process reliably
- Exit status parsed correctly
- No zombie processes

✅ **Quality**:
- All syscalls checked for errors
- RAII ensures cleanup on exceptions
- Unit tests pass
- Integration tests verify end-to-end
- Code documented with inline comments

✅ **Performance**:
- Command building < 1 ms
- Fork/exec overhead < 10 ms
- Total overhead < 200 ms

---

## 15. Next Steps After Week 1

**Week 2**: Syscall Monitoring
- ptrace-based syscall tracing
- seccomp_unotify integration (kernel 5.4+)
- Syscall name mapping
- Event parsing from stderr

**Week 3**: Metrics & Scoring
- Parse syscalls into behavioral metrics
- Implement threat scoring algorithm
- Generate human-readable behaviors

**Week 4-5**: Integration & Testing
- Full Orchestrator integration
- Comprehensive testing
- Documentation and polish

---

## 16. Contact and Resources

**Full Design**: `/home/rbsmith4/ladybird/Services/Sentinel/Sandbox/NSJAIL_LAUNCHER_DESIGN.md` (1200 lines)

**References**:
- nsjail: https://github.com/google/nsjail
- Kafel: https://github.com/google/kafel
- Checklist: `docs/BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md`

**Status**: ✅ **APPROVED - Ready for Implementation**

---

**Document Version**: 1.0
**Last Updated**: 2025-11-02
**Estimated Implementation Time**: 20 hours (2-3 days)
