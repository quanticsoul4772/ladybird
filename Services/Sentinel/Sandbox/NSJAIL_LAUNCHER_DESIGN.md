# nsjail Command Builder and Launcher Design

**Document**: NSJAIL_LAUNCHER_DESIGN.md
**Version**: 1.0
**Date**: 2025-11-02
**Status**: Design Approved - Ready for Implementation
**Phase**: Milestone 0.5 Phase 2 - Week 1 Day 4-5

---

## Executive Summary

This document defines the architecture for building and launching nsjail sandboxes for behavioral malware analysis in Ladybird Sentinel. The design prioritizes:

1. **Simplicity**: Use existing nsjail protobuf config files
2. **Reliability**: Robust error handling and process cleanup
3. **Performance**: IPC via pipes, process monitoring via ptrace/seccomp
4. **Maintainability**: Clear separation of command building vs execution
5. **Platform Support**: Linux-only (graceful degradation on other platforms)

**Key Decision**: Use `-C config.cfg` approach with existing protobuf config files rather than building inline command-line arguments. This reduces complexity and leverages nsjail's native configuration format.

---

## 1. Architecture Overview

### 1.1 Component Diagram

```
BehavioralAnalyzer
    ↓
    ├─ build_nsjail_command()          (Command Builder)
    │   ├─ Select config file path
    │   ├─ Append sandbox directory mount
    │   ├─ Append executable path
    │   └─ Return Vector<String> argv
    │
    └─ launch_nsjail_sandbox()         (Process Launcher)
        ├─ Create IPC pipes (stdout, stderr)
        ├─ fork() child process
        ├─ Child: execvp() nsjail
        ├─ Parent: Monitor child (waitpid, read pipes)
        ├─ Parse output for metrics
        └─ Cleanup (kill child, close pipes)
```

### 1.2 Design Philosophy

**Use Configuration Files, Not Inline Args**

We have comprehensive nsjail config files:
- `Services/Sentinel/Sandbox/configs/malware-sandbox.cfg` (protobuf format)
- `Services/Sentinel/Sandbox/configs/malware-sandbox.kafel` (seccomp policy)

These files already define:
- Namespace isolation (PID, mount, network, user, IPC)
- Resource limits (CPU, memory, file descriptors)
- Filesystem mounts (tmpfs, read-only system dirs)
- Seccomp-BPF policy (syscall filtering)
- Environment variables
- Timeout enforcement

**Why use `-C config.cfg` instead of inline args?**

✅ **Pros**:
- Simpler command builder (just 5-10 args vs 30-40)
- Easier to maintain and update policies
- Native nsjail protobuf format (no manual quoting)
- Can version control security policies
- Can hot-reload config changes without recompiling

❌ **Cons of inline args**:
- 40+ arguments to build and validate
- Complex quoting/escaping for paths
- Harder to audit security settings
- Duplicates logic that nsjail config already provides

**Decision**: Use `-C config.cfg` approach with dynamic mount injection for sandbox directory.

---

## 2. Data Structures

### 2.1 Command Arguments Structure

```cpp
// Services/Sentinel/Sandbox/BehavioralAnalyzer.h

struct NsjailCommandArgs {
    String config_file_path;           // Path to .cfg file
    String sandbox_directory;          // Dynamic temp directory (/tmp/sentinel-XXXXXX)
    String executable_path;            // Path to binary inside sandbox
    Vector<String> additional_mounts;  // Optional additional bind mounts

    // Runtime overrides (optional)
    Optional<u32> timeout_override;    // Override config timeout (seconds)
    Optional<u64> memory_override;     // Override config memory limit (bytes)

    bool verbose { false };            // Enable verbose nsjail logging
};
```

**Rationale**:
- `config_file_path`: Points to pre-configured nsjail protobuf config
- `sandbox_directory`: Dynamically created temp directory (Week 1 Day 3-4 complete)
- `executable_path`: Full path to the executable to run inside sandbox
- `additional_mounts`: Allow dynamic mounts (e.g., for specific libraries)
- Overrides: Useful for testing different timeout/memory configurations

### 2.2 IPC Pipe Structure

```cpp
struct SandboxPipes {
    int stdout_pipe[2] { -1, -1 };     // [0] = read end, [1] = write end
    int stderr_pipe[2] { -1, -1 };     // [0] = read end, [1] = write end

    bool stdout_created { false };
    bool stderr_created { false };

    // Helper methods
    ErrorOr<void> create_pipes();
    void close_read_ends();
    void close_write_ends();
    void close_all();

    ~SandboxPipes() {
        close_all();
    }
};
```

**Rationale**:
- Separate pipes for stdout/stderr to distinguish normal output from errors
- RAII pattern ensures pipes are closed even on exceptions
- Helper methods reduce boilerplate in fork/exec code

### 2.3 Sandbox Execution Result

```cpp
struct SandboxExecutionResult {
    int exit_code { -1 };              // Process exit code (0 = success)
    bool timed_out { false };          // True if killed by timeout
    bool killed_by_signal { false };   // True if killed by signal (e.g., SIGKILL)
    int signal_number { 0 };           // Signal that killed process (if any)

    ByteBuffer stdout_output;          // Captured stdout
    ByteBuffer stderr_output;          // Captured stderr (seccomp violations)

    Duration execution_time;           // Wall clock time

    // Syscall monitoring data (parsed from stderr)
    Vector<SyscallEvent> syscall_events;
};

struct SyscallEvent {
    String syscall_name;               // "open", "fork", "socket", etc.
    u64 timestamp_ns { 0 };            // Nanosecond timestamp
    Vector<String> arguments;          // Syscall arguments (if available)
    String action;                     // "ALLOW", "LOG", "ERRNO", "KILL"
};
```

**Rationale**:
- `exit_code`: Distinguish normal exit vs crash vs timeout
- `stdout_output`: Capture program output (may contain behavioral indicators)
- `stderr_output`: nsjail logs seccomp violations here (critical for metrics)
- `syscall_events`: Parsed from stderr, used for behavioral scoring

---

## 3. Command Building Strategy

### 3.1 Command Builder Implementation

```cpp
// Services/Sentinel/Sandbox/BehavioralAnalyzer.cpp

ErrorOr<Vector<String>> BehavioralAnalyzer::build_nsjail_command(
    NsjailCommandArgs const& args)
{
    Vector<String> command;

    // [1] Executable
    TRY(command.try_append("nsjail"_string));

    // [2] Configuration file (primary source of truth)
    TRY(command.try_append("-C"_string));
    TRY(command.try_append(args.config_file_path));

    // [3] Runtime overrides (optional)
    if (args.timeout_override.has_value()) {
        TRY(command.try_append("--time_limit"_string));
        TRY(command.try_append(TRY(String::number(args.timeout_override.value()))));
    }

    if (args.memory_override.has_value()) {
        TRY(command.try_append("--rlimit_as"_string));
        TRY(command.try_append(TRY(String::number(args.memory_override.value()))));
    }

    // [4] Dynamic sandbox directory mount (read-write)
    // Mount host /tmp/sentinel-XXXXXX to container /sandbox
    TRY(command.try_append("--bindmount"_string));
    TRY(command.try_append(TRY(String::formatted("{}:/sandbox", args.sandbox_directory))));

    // [5] Additional mounts (if any)
    for (auto const& mount : args.additional_mounts) {
        TRY(command.try_append("--bindmount"_string));
        TRY(command.try_append(mount));
    }

    // [6] Verbose logging (useful for debugging)
    if (args.verbose) {
        TRY(command.try_append("--verbose"_string));
    }

    // [7] Separator between nsjail args and command to execute
    TRY(command.try_append("--"_string));

    // [8] Executable to run inside sandbox
    TRY(command.try_append(args.executable_path));

    return command;
}
```

**Example Command Generated**:
```bash
nsjail \
  -C /path/to/Services/Sentinel/Sandbox/configs/malware-sandbox.cfg \
  --bindmount /tmp/sentinel-Ab3X9z:/sandbox \
  -- \
  /sandbox/suspicious_binary
```

**Key Features**:
- **Minimal args**: Only 7 arguments (vs 40+ for full inline config)
- **Config first**: All security settings from malware-sandbox.cfg
- **Dynamic mount**: Sandbox directory mounted as `/sandbox` inside container
- **Override support**: Can adjust timeout/memory for specific cases
- **Clear separation**: `--` marks end of nsjail args, start of sandboxed command

### 3.2 Configuration File Selection

```cpp
ErrorOr<String> BehavioralAnalyzer::get_nsjail_config_path()
{
    // Try multiple locations (build directory first, then installed location)
    Vector<StringView> candidate_paths = {
        "/home/rbsmith4/ladybird/Services/Sentinel/Sandbox/configs/malware-sandbox.cfg"sv,
        "/usr/share/ladybird/sentinel/configs/malware-sandbox.cfg"sv,
        "/opt/ladybird/sentinel/configs/malware-sandbox.cfg"sv,
    };

    for (auto const& path : candidate_paths) {
        if (Core::System::access(path, R_OK).is_error())
            continue;
        return TRY(String::from_utf8(path));
    }

    return Error::from_string_literal("nsjail config file not found");
}
```

**Rationale**:
- Check build directory first (development)
- Fall back to installed paths (production)
- Return error if config not found (fail-safe)

---

## 4. IPC Pipe Architecture

### 4.1 Pipe Creation and Management

```cpp
ErrorOr<void> SandboxPipes::create_pipes()
{
    // Create stdout pipe
    if (pipe(stdout_pipe) < 0) {
        return Error::from_errno(errno);
    }
    stdout_created = true;

    // Create stderr pipe
    if (pipe(stderr_pipe) < 0) {
        return Error::from_errno(errno);
    }
    stderr_created = true;

    return {};
}

void SandboxPipes::close_read_ends()
{
    if (stdout_created && stdout_pipe[0] != -1) {
        close(stdout_pipe[0]);
        stdout_pipe[0] = -1;
    }
    if (stderr_created && stderr_pipe[0] != -1) {
        close(stderr_pipe[0]);
        stderr_pipe[0] = -1;
    }
}

void SandboxPipes::close_write_ends()
{
    if (stdout_created && stdout_pipe[1] != -1) {
        close(stdout_pipe[1]);
        stdout_pipe[1] = -1;
    }
    if (stderr_created && stderr_pipe[1] != -1) {
        close(stderr_pipe[1]);
        stderr_pipe[1] = -1;
    }
}

void SandboxPipes::close_all()
{
    close_read_ends();
    close_write_ends();
}
```

**Pipe Usage Pattern**:

```
Parent Process              Child Process (nsjail)
==============              ======================

[Create pipes]

fork() ─────────────────────> [Child created]

[Close write ends]          [Close read ends]

[dup2(pipe[1], STDOUT)]
[dup2(pipe[1], STDERR)]

                            [execvp(nsjail)]
                            [Writes to stdout/stderr]

[read(pipe[0])]
[Parse output]

[waitpid()]                 [exit()]

[close(pipe[0])]
```

### 4.2 Non-blocking I/O for Pipes

**Problem**: If child produces more output than pipe buffer (64KB on Linux), it will block waiting for parent to read.

**Solution**: Use `fcntl` to set pipes to non-blocking mode, then use `select()` or `poll()` to read when data is available.

```cpp
ErrorOr<void> set_nonblocking(int fd)
{
    int flags = TRY(Core::System::fcntl(fd, F_GETFL));
    TRY(Core::System::fcntl(fd, F_SETFL, flags | O_NONBLOCK));
    return {};
}

ErrorOr<ByteBuffer> read_pipe_nonblocking(int fd, Duration timeout)
{
    TRY(set_nonblocking(fd));

    ByteBuffer output;
    auto deadline = MonotonicTime::now() + timeout;

    while (true) {
        // Check if timeout expired
        if (MonotonicTime::now() >= deadline)
            break;

        // Use poll() to wait for data with timeout
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        int remaining_ms = (deadline - MonotonicTime::now()).to_milliseconds();

        int poll_result = poll(&pfd, 1, remaining_ms);
        if (poll_result < 0)
            return Error::from_errno(errno);
        if (poll_result == 0)  // Timeout
            break;

        // Read available data
        u8 buffer[4096];
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;  // No data available yet
            return Error::from_errno(errno);
        }
        if (bytes == 0)  // EOF
            break;

        TRY(output.try_append(buffer, bytes));
    }

    return output;
}
```

**Rationale**:
- Non-blocking I/O prevents deadlocks
- `poll()` provides timeout mechanism
- Incremental reading handles large outputs

---

## 5. Process Launching and Monitoring

### 5.1 Launch Implementation

```cpp
ErrorOr<SandboxExecutionResult> BehavioralAnalyzer::launch_nsjail_sandbox(
    NsjailCommandArgs const& args,
    Duration timeout)
{
    dbgln_if(false, "BehavioralAnalyzer: Launching nsjail sandbox");

    // [1] Build command
    auto command = TRY(build_nsjail_command(args));

    // [2] Create IPC pipes
    SandboxPipes pipes;
    TRY(pipes.create_pipes());

    // [3] Fork child process
    pid_t child_pid = fork();
    if (child_pid < 0) {
        return Error::from_errno(errno);
    }

    // [4] Child process: execute nsjail
    if (child_pid == 0) {
        // Close read ends (child only writes)
        pipes.close_read_ends();

        // Redirect stdout/stderr to pipes
        TRY(Core::System::dup2(pipes.stdout_pipe[1], STDOUT_FILENO));
        TRY(Core::System::dup2(pipes.stderr_pipe[1], STDERR_FILENO));

        // Close original write ends (now using stdout/stderr)
        pipes.close_write_ends();

        // Build argv for execvp
        Vector<char const*> argv;
        for (auto const& arg : command) {
            argv.append(arg.bytes_as_string_view().characters_without_null_termination());
        }
        argv.append(nullptr);  // execvp requires NULL-terminated array

        // Execute nsjail (replaces child process)
        execvp("nsjail", const_cast<char* const*>(argv.data()));

        // If execvp returns, it failed
        perror("execvp failed");
        _exit(127);
    }

    // [5] Parent process: monitor child
    pipes.close_write_ends();  // Parent only reads

    auto start_time = MonotonicTime::now();
    SandboxExecutionResult result;

    // [6] Setup timeout alarm
    auto alarm_handle = TRY(setup_timeout_alarm(child_pid, timeout));

    // [7] Read output from pipes (non-blocking)
    result.stdout_output = TRY(read_pipe_nonblocking(pipes.stdout_pipe[0], timeout));
    result.stderr_output = TRY(read_pipe_nonblocking(pipes.stderr_pipe[0], timeout));

    // [8] Wait for child to exit
    int status = 0;
    pid_t wait_result = waitpid(child_pid, &status, 0);
    if (wait_result < 0) {
        return Error::from_errno(errno);
    }

    // [9] Parse exit status
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.killed_by_signal = true;
        result.signal_number = WTERMSIG(status);
        if (result.signal_number == SIGALRM || result.signal_number == SIGKILL) {
            result.timed_out = true;
        }
    }

    // [10] Calculate execution time
    result.execution_time = MonotonicTime::now() - start_time;

    // [11] Cancel alarm
    TRY(cancel_timeout_alarm(alarm_handle));

    // [12] Parse syscall events from stderr
    result.syscall_events = TRY(parse_syscall_events(result.stderr_output));

    dbgln_if(false, "BehavioralAnalyzer: Sandbox exited with code {}, execution time {}ms",
        result.exit_code, result.execution_time.to_milliseconds());

    return result;
}
```

### 5.2 Timeout Enforcement

**Strategy**: Use `alarm()` with `SIGALRM` handler to kill child after timeout.

```cpp
// Global state for signal handler (Linux limitation)
static pid_t g_sandboxed_child_pid = -1;
static bool g_timeout_triggered = false;

static void sigalrm_handler(int)
{
    g_timeout_triggered = true;

    if (g_sandboxed_child_pid > 0) {
        // Send SIGKILL to child (cannot be caught)
        kill(g_sandboxed_child_pid, SIGKILL);
    }
}

struct AlarmHandle {
    bool active { false };
};

ErrorOr<AlarmHandle> BehavioralAnalyzer::setup_timeout_alarm(pid_t child_pid, Duration timeout)
{
    // Install signal handler
    struct sigaction sa;
    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, nullptr) < 0) {
        return Error::from_errno(errno);
    }

    // Set global state (for signal handler)
    g_sandboxed_child_pid = child_pid;
    g_timeout_triggered = false;

    // Set alarm
    unsigned int seconds = timeout.to_seconds();
    alarm(seconds);

    return AlarmHandle { .active = true };
}

ErrorOr<void> BehavioralAnalyzer::cancel_timeout_alarm(AlarmHandle handle)
{
    if (!handle.active)
        return {};

    // Cancel alarm
    alarm(0);

    // Reset global state
    g_sandboxed_child_pid = -1;

    return {};
}
```

**Rationale**:
- `alarm()` is simple and reliable for timeout enforcement
- `SIGKILL` ensures process is killed (cannot be caught/ignored)
- Global state is safe (only one sandbox runs at a time per BehavioralAnalyzer instance)
- Alarm is canceled on normal completion to prevent false timeouts

**Alternative**: Use `timer_create()` and `timer_settime()` for finer-grained control (millisecond precision), but more complex.

---

## 6. Error Handling Strategy

### 6.1 Error Categories

| Error Type | Severity | Action |
|-----------|----------|--------|
| **nsjail not found** | High | Use mock implementation, log warning |
| **Config file missing** | High | Use inline seccomp policy, log warning |
| **fork() failure** | Critical | Return error to Orchestrator |
| **execvp() failure** | Critical | Child exits with code 127 |
| **pipe() failure** | Medium | Use `/dev/null` for stdout/stderr |
| **Timeout** | Expected | Kill child, mark `timed_out = true` |
| **Child crash** | Expected | Log signal, continue analysis |
| **Zombie process** | Critical | `waitpid()` must be called |

### 6.2 Error Handling Implementation

```cpp
ErrorOr<SandboxExecutionResult> BehavioralAnalyzer::launch_nsjail_sandbox_safe(
    NsjailCommandArgs const& args,
    Duration timeout)
{
    // Check if nsjail is available
    if (!m_nsjail_available) {
        dbgln("Warning: nsjail not available, using mock implementation");
        return Error::from_string_literal("nsjail not available");
    }

    // Validate config file exists
    auto config_path = TRY(get_nsjail_config_path());
    if (!Core::File::exists(config_path)) {
        dbgln("Warning: nsjail config not found: {}", config_path);
        return Error::from_string_literal("nsjail config not found");
    }

    // Launch sandbox with error handling
    auto result = launch_nsjail_sandbox(args, timeout);

    if (result.is_error()) {
        dbgln("Error launching nsjail: {}", result.error());

        // Ensure child process is reaped if fork succeeded
        if (g_sandboxed_child_pid > 0) {
            kill(g_sandboxed_child_pid, SIGKILL);
            waitpid(g_sandboxed_child_pid, nullptr, 0);
            g_sandboxed_child_pid = -1;
        }

        return result.release_error();
    }

    return result.release_value();
}
```

### 6.3 Resource Cleanup Checklist

**MUST be cleaned up on all paths (success, error, timeout)**:

1. ✅ **Child process**: Call `waitpid()` to reap zombie
2. ✅ **Pipe file descriptors**: Close both read and write ends
3. ✅ **Alarm**: Cancel `alarm()` to prevent false timeouts
4. ✅ **Temp directory**: Remove sandbox files (Week 1 Day 3-4 infrastructure)
5. ✅ **Global state**: Reset `g_sandboxed_child_pid`

**RAII Pattern**:

```cpp
// Use destructors to ensure cleanup
struct ScopedSandbox {
    pid_t child_pid { -1 };
    SandboxPipes pipes;
    AlarmHandle alarm;
    String sandbox_dir;

    ~ScopedSandbox() {
        // Kill child if still running
        if (child_pid > 0) {
            kill(child_pid, SIGKILL);
            waitpid(child_pid, nullptr, 0);
        }

        // Cancel alarm
        if (alarm.active)
            alarm(0);

        // Pipes auto-closed by SandboxPipes destructor

        // Cleanup temp directory
        if (!sandbox_dir.is_empty()) {
            cleanup_temp_directory(sandbox_dir).release_value_but_fixme_should_propagate_errors();
        }
    }
};
```

---

## 7. API Design

### 7.1 Public API (BehavioralAnalyzer.h)

```cpp
// Services/Sentinel/Sandbox/BehavioralAnalyzer.h

class BehavioralAnalyzer {
public:
    // ... existing interface ...

    // Main entry point (called by Orchestrator)
    ErrorOr<BehavioralMetrics> analyze(
        ByteBuffer const& file_data,
        String const& filename,
        Duration timeout);

private:
    // Week 1 Day 4-5: nsjail launcher
    ErrorOr<Vector<String>> build_nsjail_command(NsjailCommandArgs const& args);
    ErrorOr<SandboxExecutionResult> launch_nsjail_sandbox(NsjailCommandArgs const& args, Duration timeout);
    ErrorOr<Vector<SyscallEvent>> parse_syscall_events(ByteBuffer const& stderr_output);

    // Helpers
    ErrorOr<String> get_nsjail_config_path();
    ErrorOr<AlarmHandle> setup_timeout_alarm(pid_t child_pid, Duration timeout);
    ErrorOr<void> cancel_timeout_alarm(AlarmHandle handle);

    // Platform detection
    bool m_nsjail_available { false };
    ErrorOr<bool> detect_nsjail_availability();
};
```

### 7.2 Integration with analyze_nsjail()

```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail(
    ByteBuffer const& file_data,
    String const& filename)
{
    dbgln_if(false, "BehavioralAnalyzer: Starting nsjail analysis");

    // [1] Write file to sandbox directory (Week 1 Day 3-4 complete)
    auto file_path = TRY(write_file_to_sandbox(m_sandbox_dir, file_data, filename));
    TRY(make_executable(file_path));

    // [2] Build nsjail command arguments
    NsjailCommandArgs args {
        .config_file_path = TRY(get_nsjail_config_path()),
        .sandbox_directory = m_sandbox_dir,
        .executable_path = TRY(String::formatted("/sandbox/{}", filename)),
        .verbose = false,
    };

    // [3] Launch sandbox
    auto result = TRY(launch_nsjail_sandbox(args, Duration::from_seconds(5)));

    // [4] Parse syscall events into metrics
    BehavioralMetrics metrics;
    for (auto const& event : result.syscall_events) {
        update_metrics_from_syscall(event.syscall_name, metrics);
    }

    // [5] Set execution metadata
    metrics.execution_time = result.execution_time;
    metrics.timed_out = result.timed_out;
    metrics.exit_code = result.exit_code;

    // [6] Cleanup handled by destructor

    return metrics;
}
```

---

## 8. Platform Compatibility

### 8.1 Linux Support

**Minimum Requirements**:
- Linux kernel 5.4+ (for seccomp_unotify, optional)
- Linux kernel 3.8+ (for basic nsjail support)
- nsjail binary installed (`apt-get install nsjail`)
- libseccomp-dev for seccomp-BPF

**Detection**:

```cpp
ErrorOr<bool> BehavioralAnalyzer::detect_nsjail_availability()
{
#if !defined(__linux__)
    return false;  // Only supported on Linux
#else
    // Check if nsjail binary exists
    auto which_result = Core::System::exec("/usr/bin/which"sv, { "nsjail"sv });
    if (which_result.is_error())
        return false;

    // Check if nsjail is executable
    auto nsjail_path = which_result.release_value();
    auto access_result = Core::System::access(nsjail_path, X_OK);
    if (access_result.is_error())
        return false;

    return true;
#endif
}
```

### 8.2 macOS / Windows / Other Platforms

**Strategy**: Graceful degradation to mock implementation.

```cpp
ErrorOr<void> BehavioralAnalyzer::initialize_sandbox()
{
#if defined(__linux__)
    m_nsjail_available = TRY(detect_nsjail_availability());

    if (m_nsjail_available) {
        dbgln("BehavioralAnalyzer: nsjail available, using native sandbox");
        m_use_mock = false;
    } else {
        dbgln("BehavioralAnalyzer: nsjail not available, using mock implementation");
        m_use_mock = true;
    }
#else
    dbgln("BehavioralAnalyzer: Platform not supported, using mock implementation");
    m_use_mock = true;
    m_nsjail_available = false;
#endif

    // Create temp sandbox directory (both mock and real need this)
    m_sandbox_dir = TRY(create_temp_sandbox_directory());

    return {};
}
```

**User Experience**:
- On Linux with nsjail: Full behavioral analysis
- On Linux without nsjail: Mock analysis with warning
- On macOS/Windows: Mock analysis (transparent to user)

---

## 9. Testing Strategy

### 9.1 Unit Tests

```cpp
// Services/Sentinel/Test/TestBehavioralAnalyzer.cpp

TEST_CASE(nsjail_command_builder)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    NsjailCommandArgs args {
        .config_file_path = "/path/to/config.cfg"_string,
        .sandbox_directory = "/tmp/sentinel-test"_string,
        .executable_path = "/sandbox/test_binary"_string,
        .verbose = false,
    };

    auto command = analyzer->build_nsjail_command(args).release_value();

    // Verify command structure
    EXPECT_EQ(command[0], "nsjail"sv);
    EXPECT_EQ(command[1], "-C"sv);
    EXPECT_EQ(command[2], "/path/to/config.cfg"sv);
    EXPECT(command.contains_slow("--bindmount"sv));
    EXPECT_EQ(command.last(), "/sandbox/test_binary"sv);
}

TEST_CASE(pipe_creation)
{
    SandboxPipes pipes;
    auto result = pipes.create_pipes();

    EXPECT(!result.is_error());
    EXPECT(pipes.stdout_created);
    EXPECT(pipes.stderr_created);
    EXPECT_NE(pipes.stdout_pipe[0], -1);
    EXPECT_NE(pipes.stdout_pipe[1], -1);

    // Destructor should close pipes
}

TEST_CASE(nsjail_launch_benign_executable)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Create temp sandbox
    auto sandbox_dir = analyzer->create_temp_sandbox_directory().release_value();

    // Write simple script
    auto script = "#!/bin/sh\necho 'Hello from sandbox'\nexit 0\n"_string;
    auto script_path = analyzer->write_file_to_sandbox(
        sandbox_dir,
        ByteBuffer::copy(script.bytes()).release_value(),
        "test.sh"_string
    ).release_value();
    analyzer->make_executable(script_path).release_value();

    // Launch nsjail
    NsjailCommandArgs args {
        .config_file_path = analyzer->get_nsjail_config_path().release_value(),
        .sandbox_directory = sandbox_dir,
        .executable_path = "/sandbox/test.sh"_string,
    };

    auto result = analyzer->launch_nsjail_sandbox(args, Duration::from_seconds(5)).release_value();

    // Verify successful execution
    EXPECT_EQ(result.exit_code, 0);
    EXPECT(!result.timed_out);
    EXPECT(result.stdout_output.bytes().contains("Hello from sandbox"sv));
}

TEST_CASE(nsjail_timeout_enforcement)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Create infinite loop script
    auto script = "#!/bin/sh\nwhile true; do sleep 1; done\n"_string;
    // ... (similar setup as above)

    auto result = analyzer->launch_nsjail_sandbox(args, Duration::from_seconds(2)).release_value();

    // Verify timeout
    EXPECT(result.timed_out);
    EXPECT(result.killed_by_signal);
}
```

### 9.2 Integration Tests

```bash
# Manual integration test script
# Services/Sentinel/test_nsjail_integration.sh

#!/bin/bash
set -e

echo "Testing nsjail integration..."

# Test 1: Basic execution
nsjail -C malware-sandbox.cfg -- /bin/echo "Test 1 passed"

# Test 2: Timeout enforcement
timeout 6s nsjail -C malware-sandbox.cfg --time_limit 2 -- /bin/sleep 10 || echo "Test 2 passed (timeout)"

# Test 3: Seccomp filtering
nsjail -C malware-sandbox.cfg -- /bin/sh -c 'socket()' 2>&1 | grep -q "SECCOMP" && echo "Test 3 passed (seccomp)"

# Test 4: Resource limits
nsjail -C malware-sandbox.cfg -- /usr/bin/stress --vm 1 --vm-bytes 256M || echo "Test 4 passed (memory limit)"

echo "All integration tests passed"
```

---

## 10. Performance Considerations

### 10.1 Performance Targets

| Metric | Target | Rationale |
|--------|--------|-----------|
| **Command Building** | < 1 ms | Simple string concatenation |
| **Fork + Exec** | < 10 ms | OS overhead |
| **Pipe Setup** | < 1 ms | System call overhead |
| **Sandbox Execution** | < 5 seconds | User-facing timeout |
| **Pipe Reading** | < 100 ms | Typically small output |
| **Total Overhead** | < 200 ms | Acceptable for security |

### 10.2 Optimization Strategies

**1. Reuse Sandbox Directory**:
- Don't create new temp dir for each analysis
- Reuse `m_sandbox_dir` across multiple analyses
- Only cleanup in destructor

**2. Pre-build Command Template**:
- Cache config file path on initialization
- Only substitute dynamic values (sandbox dir, executable)

**3. Lazy Pipe Reading**:
- Only read pipes if child generates output
- Use `poll()` with short timeout to avoid blocking

**4. Early Termination**:
- If seccomp kills process early (dangerous syscall), stop monitoring immediately

**5. Parallel Analysis** (Future):
- Run multiple sandboxes in parallel (different temp dirs)
- Requires thread-safe signal handling

---

## 11. Security Considerations

### 11.1 Threat Model

**Assumptions**:
- Attacker has code execution inside sandbox
- Attacker attempts to escape sandbox
- Attacker attempts to exhaust host resources

**Mitigations**:

| Threat | Mitigation | Layer |
|--------|-----------|-------|
| **Sandbox escape** | nsjail + seccomp-BPF | Container |
| **Resource exhaustion** | rlimits (CPU, memory, fds) | nsjail |
| **Network attacks** | Network namespace isolation | nsjail |
| **Filesystem attacks** | Mount namespace, read-only mounts | nsjail |
| **Privilege escalation** | User namespace (UID mapping to nobody) | nsjail |
| **Timeout bypass** | SIGKILL on timeout (uncatchable) | Orchestrator |
| **Temp directory tampering** | Secure permissions (0700), random names | Week 1 Day 3-4 |

### 11.2 Configuration File Security

**Risk**: Attacker modifies nsjail config file to weaken security.

**Mitigation**:
- Store config files in read-only directories
- Verify config file ownership (root:root)
- Use hardcoded fallback config if file is compromised

```cpp
ErrorOr<void> BehavioralAnalyzer::verify_config_security(String const& config_path)
{
    struct stat st;
    if (stat(config_path.bytes_as_string_view().characters_without_null_termination(), &st) < 0) {
        return Error::from_errno(errno);
    }

    // Verify ownership (root or current user)
    if (st.st_uid != 0 && st.st_uid != getuid()) {
        return Error::from_string_literal("Config file has unexpected owner");
    }

    // Verify not world-writable
    if (st.st_mode & S_IWOTH) {
        return Error::from_string_literal("Config file is world-writable");
    }

    return {};
}
```

---

## 12. Open Questions and Design Decisions

### 12.1 Resolved Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| **Config file vs inline args?** | Use config file (`-C config.cfg`) | Simpler, more maintainable |
| **One config or multiple?** | One config (`malware-sandbox.cfg`) | Sufficient for most cases |
| **Pipe vs shared memory?** | Pipe (stdout/stderr) | Simple, reliable, POSIX standard |
| **ptrace vs seccomp_unotify?** | Both (seccomp primary, ptrace fallback) | Kernel version compatibility |
| **Timeout mechanism?** | `alarm()` + `SIGKILL` | Simple, reliable, portable |
| **Platform support?** | Linux only (graceful degradation) | nsjail is Linux-specific |

### 12.2 Future Enhancements

**Phase 3 (Week 2-3)**:
- Syscall argument parsing (extract file paths, IPs, etc.)
- eBPF-based monitoring (more efficient than ptrace)
- Seccomp_unotify integration (kernel 5.4+)

**Phase 4 (Week 4-5)**:
- Multi-sandbox parallelism (analyze multiple files concurrently)
- Dynamic config generation (per-file security policies)
- Advanced metrics (entropy calculation, network traffic analysis)

---

## 13. Implementation Checklist

### Week 1 Day 4-5 Deliverables

- [ ] **Data Structures**
  - [ ] `NsjailCommandArgs` struct
  - [ ] `SandboxPipes` struct with RAII
  - [ ] `SandboxExecutionResult` struct
  - [ ] `SyscallEvent` struct

- [ ] **Command Builder**
  - [ ] `build_nsjail_command()` implementation
  - [ ] `get_nsjail_config_path()` with fallback paths
  - [ ] Unit tests for command generation

- [ ] **IPC Infrastructure**
  - [ ] `SandboxPipes::create_pipes()`
  - [ ] `SandboxPipes::close_*()` methods
  - [ ] `read_pipe_nonblocking()` with timeout
  - [ ] `set_nonblocking()` helper

- [ ] **Process Launcher**
  - [ ] `launch_nsjail_sandbox()` with fork/exec
  - [ ] Pipe redirection in child process
  - [ ] `waitpid()` and exit status parsing
  - [ ] Resource cleanup on all paths

- [ ] **Timeout Enforcement**
  - [ ] `setup_timeout_alarm()` with signal handler
  - [ ] `cancel_timeout_alarm()`
  - [ ] Global state management
  - [ ] Test timeout enforcement

- [ ] **Error Handling**
  - [ ] Error handling for all syscalls
  - [ ] RAII for resource cleanup
  - [ ] Zombie process prevention
  - [ ] Platform detection

- [ ] **Testing**
  - [ ] Unit tests for command builder
  - [ ] Unit tests for pipe management
  - [ ] Integration test with `/bin/echo`
  - [ ] Integration test with timeout
  - [ ] Integration test with config file

- [ ] **Documentation**
  - [ ] Inline code documentation
  - [ ] Update BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md
  - [ ] Add troubleshooting section
  - [ ] Document platform compatibility

---

## 14. Success Criteria

### Functional Requirements

✅ **Command building**:
- Generates valid nsjail command from config file
- Correctly injects sandbox directory mount
- Supports timeout and memory overrides

✅ **Process launching**:
- Fork/exec succeeds
- Pipes correctly capture stdout/stderr
- Timeout enforced reliably
- Exit status parsed correctly

✅ **Resource cleanup**:
- No zombie processes
- All pipes closed
- Temp directory cleaned up
- Alarm canceled

### Quality Requirements

✅ **Error handling**:
- All syscalls checked for errors
- RAII ensures cleanup on exceptions
- Graceful degradation on missing nsjail

✅ **Testing**:
- All unit tests pass
- Integration tests verify end-to-end flow
- Timeout tests verify SIGKILL behavior

✅ **Performance**:
- Command building < 1 ms
- Fork/exec overhead < 10 ms
- Total overhead < 200 ms

---

## 15. References

### External Documentation

- **nsjail**: https://github.com/google/nsjail
- **nsjail protobuf config**: https://github.com/google/nsjail/blob/master/config.proto
- **Kafel seccomp DSL**: https://github.com/google/kafel
- **seccomp-BPF**: https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt
- **Linux namespaces**: https://man7.org/linux/man-pages/man7/namespaces.7.html

### Internal Documentation

- **BEHAVIORAL_ANALYSIS_SPEC.md**: Full behavioral analysis specification
- **BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md**: Week-by-week checklist
- **SANDBOX_INTEGRATION_GUIDE.md**: Sandbox integration patterns
- **malware-sandbox.cfg**: nsjail protobuf configuration
- **malware-sandbox.kafel**: Seccomp-BPF policy

---

## 16. Approval and Next Steps

**Design Status**: ✅ **APPROVED - Ready for Implementation**

**Reviewed By**: Systems Programming Architect
**Approval Date**: 2025-11-02

**Next Steps**:
1. Implement data structures (1 hour)
2. Implement command builder (2 hours)
3. Implement pipe infrastructure (2 hours)
4. Implement process launcher (4 hours)
5. Implement timeout enforcement (2 hours)
6. Write unit tests (4 hours)
7. Write integration tests (3 hours)
8. Code review and refinement (2 hours)

**Total Estimated Time**: 20 hours (2-3 days)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-02
**Status**: Implementation-Ready
**Contact**: Ladybird Sentinel Security Team
