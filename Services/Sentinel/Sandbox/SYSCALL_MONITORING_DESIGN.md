# Syscall Monitoring Design for BehavioralAnalyzer

**Milestone**: 0.5 Phase 2 Week 2 - Syscall Monitoring Architecture
**Status**: Design Complete
**Created**: 2025-11-02
**Author**: Security Research Team

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Pipe I/O Strategy](#pipe-io-strategy)
3. [Event Format Specification](#event-format-specification)
4. [Parser Design](#parser-design)
5. [Metric Mapping Table](#metric-mapping-table)
6. [Performance Considerations](#performance-considerations)
7. [Error Handling](#error-handling)
8. [Integration into analyze_nsjail()](#integration-into-analyze_nsjail)
9. [Testing Strategy](#testing-strategy)
10. [Open Questions](#open-questions)

---

## 1. Architecture Overview

### High-Level Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      BehavioralAnalyzer                         │
│                                                                 │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ analyze_nsjail()                                       │   │
│  │                                                        │   │
│  │  1. Write file to sandbox directory                   │   │
│  │  2. Build nsjail command with seccomp policy          │   │
│  │  3. Launch nsjail (fork/exec) → SandboxProcess        │   │
│  │     - stdin_fd (write)                                │   │
│  │     - stdout_fd (read)  [normal output]               │   │
│  │     - stderr_fd (read)  [SYSCALL EVENTS]              │   │
│  │                                                        │   │
│  │  4. Main monitoring loop (non-blocking)               │   │
│  │     ┌─────────────────────────────────────────┐       │   │
│  │     │ while (process running) {               │       │   │
│  │     │   poll([stderr_fd], timeout=100ms)      │       │   │
│  │     │                                         │       │   │
│  │     │   if (data available) {                │       │   │
│  │     │     read_syscall_events()              │       │   │
│  │     │     parse_syscall_events()             │       │   │
│  │     │     update_behavioral_metrics()        │       │   │
│  │     │   }                                     │       │   │
│  │     │                                         │       │   │
│  │     │   check_timeout()                      │       │   │
│  │     │   check_process_status()               │       │   │
│  │     │ }                                       │       │   │
│  │     └─────────────────────────────────────────┘       │   │
│  │                                                        │   │
│  │  5. Collect final metrics                             │   │
│  │  6. Calculate threat score                            │   │
│  │  7. Cleanup (kill process, close FDs, remove files)   │   │
│  │  8. Return BehavioralMetrics                          │   │
│  └────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘

                            ↓ stderr_fd

┌─────────────────────────────────────────────────────────────────┐
│                    nsjail Process (PID=X)                       │
│                                                                 │
│  Seccomp-BPF Filter (malware-sandbox.cfg)                      │
│  ┌──────────────────────────────────────────────┐              │
│  │ LOG {                                        │              │
│  │   socket, connect, fork, execve, ptrace...   │              │
│  │ }                                            │              │
│  │                                              │              │
│  │ → Logs to stderr:                            │              │
│  │   "[SYSCALL] socket(AF_INET, SOCK_STREAM)"   │              │
│  │   "[SYSCALL] connect(fd=3, ...)"             │              │
│  │   "[SYSCALL] fork()"                         │              │
│  └──────────────────────────────────────────────┘              │
│                                                                 │
│  Sandboxed Malware (analyzed binary)                           │
│  - Executes in isolated namespaces                             │
│  - Resource limits enforced (CPU, memory)                      │
│  - Network disabled                                            │
└─────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

1. **SandboxProcess**: Container for PID and 3 file descriptors (stdin, stdout, stderr)
2. **Syscall Event Parser**: Parse stderr lines into syscall name + arguments
3. **Metric Updater**: Map syscall events to BehavioralMetrics counters
4. **Main Loop**: Non-blocking I/O, timeout enforcement, process status checking

---

## 2. Pipe I/O Strategy

### Choice: Non-Blocking I/O with poll()

**Decision**: Use **poll()** with non-blocking file descriptors for stderr monitoring.

**Rationale**:
- ✅ Prevents deadlock from full pipe buffers
- ✅ Allows simultaneous timeout checking and process status monitoring
- ✅ Single-threaded design (simpler, less overhead)
- ✅ Standard POSIX API (portable across Linux distributions)
- ✅ Timeout support built-in (100ms poll interval)

**Alternatives Considered**:

| Approach | Pros | Cons | Verdict |
|----------|------|------|---------|
| **Blocking read()** | Simple | Deadlock risk if pipe fills | ❌ Rejected |
| **select()** | Portable | FD_SETSIZE limit (1024), less efficient | ❌ Rejected |
| **poll()** | No FD limit, efficient | None | ✅ **Selected** |
| **epoll()** | Highest performance | Linux-only, overkill for 1 FD | ❌ Overkill |
| **Separate thread** | Parallel reading | Complex, synchronization overhead | ❌ Unnecessary |

### Implementation Details

```cpp
ErrorOr<void> BehavioralAnalyzer::monitor_syscall_events(
    int stderr_fd,
    BehavioralMetrics& metrics,
    Duration timeout)
{
    // Set stderr_fd to non-blocking mode
    int flags = TRY(Core::System::fcntl(stderr_fd, F_GETFL));
    TRY(Core::System::fcntl(stderr_fd, F_SETFL, flags | O_NONBLOCK));

    // Poll structure for stderr monitoring
    struct pollfd pfd = {
        .fd = stderr_fd,
        .events = POLLIN,  // Wait for data available
        .revents = 0
    };

    // Accumulator buffer for partial lines
    ByteBuffer line_buffer;

    auto start_time = MonotonicTime::now();

    while (true) {
        // Check timeout
        auto elapsed = MonotonicTime::now() - start_time;
        if (elapsed > timeout) {
            metrics.timed_out = true;
            break;
        }

        // Poll with 100ms timeout (allows responsive timeout checking)
        int poll_result = poll(&pfd, 1, 100);

        if (poll_result < 0) {
            if (errno == EINTR) continue;  // Interrupted by signal, retry
            return Error::from_errno(errno);
        }

        if (poll_result == 0) {
            // Timeout - no data available
            // Continue loop to check process status and overall timeout
            continue;
        }

        // Data available - read from stderr
        if (pfd.revents & POLLIN) {
            TRY(read_and_parse_syscall_events(stderr_fd, line_buffer, metrics));
        }

        if (pfd.revents & (POLLHUP | POLLERR)) {
            // Pipe closed or error - process likely exited
            break;
        }
    }

    return {};
}
```

### Buffer Management

**Line Buffering Strategy**:
- Accumulate partial reads into `line_buffer` until newline (`\n`) found
- Parse complete lines immediately
- Prevents blocking on incomplete syscall event strings
- Maximum line buffer size: 4096 bytes (discard if exceeded)

```cpp
ErrorOr<void> read_and_parse_syscall_events(
    int stderr_fd,
    ByteBuffer& line_buffer,
    BehavioralMetrics& metrics)
{
    // Read up to 4096 bytes at a time
    char read_buffer[4096];
    ssize_t bytes_read = read(stderr_fd, read_buffer, sizeof(read_buffer));

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return {};  // No data available (non-blocking)
        }
        return Error::from_errno(errno);
    }

    if (bytes_read == 0) {
        return {};  // EOF
    }

    // Append to line buffer
    TRY(line_buffer.try_append(read_buffer, bytes_read));

    // Process all complete lines
    while (true) {
        // Find newline
        auto newline_pos = line_buffer.bytes().find_byte_offset('\n');
        if (!newline_pos.has_value()) {
            break;  // No complete line yet
        }

        // Extract line
        auto line_bytes = line_buffer.bytes().slice(0, newline_pos.value());
        StringView line { line_bytes };

        // Parse and update metrics
        parse_syscall_line(line, metrics);

        // Remove processed line from buffer (including newline)
        line_buffer = TRY(ByteBuffer::copy(
            line_buffer.bytes().slice(newline_pos.value() + 1)));
    }

    // Prevent buffer from growing indefinitely
    if (line_buffer.size() > 4096) {
        dbgln("BehavioralAnalyzer: Line buffer overflow, discarding");
        line_buffer.clear();
    }

    return {};
}
```

---

## 3. Event Format Specification

### nsjail Seccomp Log Format

Based on nsjail's seccomp-bpf LOG action, events are written to **stderr** in format:

```
[SYSCALL] syscall_name(arg1, arg2, ...)
```

### Examples from malware-sandbox.cfg

```plaintext
# Network operations
[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)
[SYSCALL] connect(fd=3, addr=192.168.1.1:8080)
[SYSCALL] sendto(fd=3, len=256)
[SYSCALL] recvfrom(fd=3, len=1024)

# Process operations
[SYSCALL] fork()
[SYSCALL] execve(path="/bin/sh", argv=["/bin/sh", "-c", "malware.sh"])
[SYSCALL] clone(flags=CLONE_VM|CLONE_FS)

# Debugging attempts
[SYSCALL] ptrace(PTRACE_ATTACH, pid=1234)
[SYSCALL] process_vm_readv(pid=1234, iov_cnt=1)

# File system modifications
[SYSCALL] unlink(path="/tmp/file.txt")
[SYSCALL] rename(oldpath="/a", newpath="/b")
[SYSCALL] mkdir(path="/tmp/hidden", mode=0700)

# Privilege escalation (returns EPERM, but logged)
[SYSCALL] setuid(uid=0)
[SYSCALL] capset(hdrp=..., datap=...)
```

### Format Variations

| Scenario | Format | Example |
|----------|--------|---------|
| **No arguments** | `[SYSCALL] name()` | `[SYSCALL] fork()` |
| **Simple args** | `[SYSCALL] name(arg1, arg2)` | `[SYSCALL] socket(2, 1, 0)` |
| **Named args** | `[SYSCALL] name(key=value)` | `[SYSCALL] connect(fd=3)` |
| **String args** | `[SYSCALL] name(path="/tmp")` | `[SYSCALL] unlink(path="/tmp/file")` |
| **Mixed args** | `[SYSCALL] name(arg1, key=value)` | `[SYSCALL] execve("/bin/sh", argv=...)` |

### Noise Filtering

**Non-syscall stderr output to ignore**:
- nsjail startup messages: `[I][timestamp] ...`
- Debug messages: `[D][timestamp] ...`
- Empty lines
- Process stdout accidentally mixed in

**Filter Strategy**: Only parse lines matching `^\[SYSCALL\] \w+\(.*\)$`

---

## 4. Parser Design

### Parser State Machine

```
Input: "[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)\n"

State 1: Check prefix "[SYSCALL] "
  ↓ Match → Continue
  ↓ No match → Ignore line

State 2: Extract syscall name (until '(')
  Result: "socket"

State 3: Extract arguments (between '(' and ')')
  Result: "AF_INET, SOCK_STREAM, 0"

State 4: (Optional) Parse arguments into key-value pairs
  Result: ["AF_INET", "SOCK_STREAM", "0"]

Output: SyscallEvent { name: "socket", args: [...] }
```

### Parser Implementation

```cpp
struct SyscallEvent {
    String name;
    Vector<String> args;
    MonotonicTime timestamp;
};

ErrorOr<Optional<SyscallEvent>> parse_syscall_line(StringView line)
{
    // 1. Check for "[SYSCALL] " prefix
    if (!line.starts_with("[SYSCALL] "sv)) {
        return Optional<SyscallEvent> {};  // Not a syscall event
    }

    // 2. Skip prefix
    auto syscall_part = line.substring_view(10);  // Skip "[SYSCALL] "

    // 3. Find opening parenthesis
    auto paren_pos = syscall_part.find('(');
    if (!paren_pos.has_value()) {
        dbgln_if(false, "BehavioralAnalyzer: Malformed syscall line: {}", line);
        return Optional<SyscallEvent> {};
    }

    // 4. Extract syscall name
    auto syscall_name = syscall_part.substring_view(0, paren_pos.value());
    auto name = TRY(String::from_utf8(syscall_name));

    // 5. Extract arguments (between '(' and ')')
    auto args_start = paren_pos.value() + 1;
    auto close_paren = syscall_part.find(')', args_start);
    if (!close_paren.has_value()) {
        dbgln_if(false, "BehavioralAnalyzer: Unclosed parenthesis: {}", line);
        return Optional<SyscallEvent> {};
    }

    auto args_str = syscall_part.substring_view(
        args_start,
        close_paren.value() - args_start
    );

    // 6. Parse arguments (split by comma, handle quoted strings)
    Vector<String> args;
    if (args_str.length() > 0) {
        // Simple split by comma (enhance later for quoted strings)
        auto arg_parts = args_str.split_view(',');
        for (auto arg : arg_parts) {
            auto trimmed = arg.trim_whitespace();
            TRY(args.try_append(TRY(String::from_utf8(trimmed))));
        }
    }

    return SyscallEvent {
        .name = move(name),
        .args = move(args),
        .timestamp = MonotonicTime::now()
    };
}
```

### Argument Parsing Enhancement (Future)

For Phase 2, **syscall name only** is sufficient. Arguments can be parsed in future phases:

- **Phase 2 (Week 2)**: Parse syscall name only
- **Phase 3 (Later)**: Parse argument values for advanced detection:
  - Socket addresses for C2 detection
  - File paths for ransomware targeting
  - Process names for injection victims

---

## 5. Metric Mapping Table

### Syscall to BehavioralMetrics Mapping

This table defines which syscalls increment which metric counters.

#### Category 1: File System Behavior

| Syscall | Metric | Weight | Notes |
|---------|--------|--------|-------|
| `open`, `openat`, `openat2` | `file_operations++` | 1 | Basic file access |
| `read`, `write`, `pread64`, `pwrite64` | `file_operations++` | 1 | File I/O |
| `readv`, `writev` | `file_operations++` | 1 | Vectored I/O |
| `unlink`, `unlinkat`, `rmdir` | `file_operations++` | 2 | **Deletion (suspicious)** |
| `rename`, `renameat` | `file_operations++` | 2 | **File renaming (ransomware)** |
| `mkdir`, `mkdirat` | `temp_file_creates++` (if /tmp) | 1 | Temp directory creation |
| `truncate`, `ftruncate` | `file_operations++` | 2 | **File destruction** |
| `chmod`, `fchmod` | `executable_drops++` (if +x) | 3 | **Executable creation** |

**Detection Logic**:
```cpp
if (syscall_name == "unlink" || syscall_name == "unlinkat") {
    metrics.file_operations += 2;  // Deletion is more suspicious
}

if (syscall_name == "chmod" && args_contain_executable_bit(args)) {
    metrics.executable_drops++;
}
```

#### Category 2: Process & Execution

| Syscall | Metric | Weight | Notes |
|---------|--------|--------|-------|
| `fork`, `vfork`, `clone`, `clone3` | `process_operations++` | 2 | Process creation |
| `execve`, `execveat` | `process_operations++` | 3 | **Execution (critical)** |
| `ptrace` | `code_injection_attempts++` | 5 | **Debugging/injection** |
| `process_vm_readv`, `process_vm_writev` | `code_injection_attempts++` | 5 | **Memory injection** |

**Detection Logic**:
```cpp
if (syscall_name == "ptrace") {
    metrics.code_injection_attempts++;
    metrics.process_operations += 2;
    TRY(metrics.suspicious_behaviors.try_append(
        "Process injection attempted (ptrace)"_string));
}

if (syscall_name == "execve") {
    metrics.process_operations += 3;
    // TODO: Parse argv to detect suspicious commands (/bin/sh -c, curl|bash)
}
```

#### Category 3: Network Behavior

| Syscall | Metric | Weight | Notes |
|---------|--------|--------|-------|
| `socket` | `network_operations++` | 1 | Network initialization |
| `connect` | `outbound_connections++` | 3 | **Outbound C2 connection** |
| `bind`, `listen`, `accept`, `accept4` | `network_operations++` | 2 | Server operations |
| `sendto`, `recvfrom`, `sendmsg`, `recvmsg` | `network_operations++` | 1 | Data transfer |
| `sendmmsg`, `recvmmsg` | `http_requests++` | 2 | **Bulk network I/O** |

**Detection Logic**:
```cpp
if (syscall_name == "connect") {
    metrics.outbound_connections++;
    metrics.network_operations++;
    // TODO: Parse sockaddr to extract IP:port for C2 detection
}

if (syscall_name == "socket") {
    metrics.network_operations++;
}
```

#### Category 4: System & Registry (Linux-specific)

| Syscall | Metric | Weight | Notes |
|---------|--------|--------|-------|
| `setuid`, `setgid`, `setreuid`, `setregid` | `privilege_escalation_attempts++` | 5 | **Privilege escalation** |
| `setresuid`, `setresgid`, `setfsuid`, `setfsgid` | `privilege_escalation_attempts++` | 5 | **UID/GID manipulation** |
| `capset` | `privilege_escalation_attempts++` | 5 | **Capability manipulation** |
| `init_module`, `finit_module` | `persistence_mechanisms++` | 5 | **Kernel module loading** |
| `mount`, `umount`, `umount2` | `privilege_escalation_attempts++` | 4 | **Filesystem manipulation** |

**Detection Logic**:
```cpp
if (syscall_name == "setuid" || syscall_name == "capset") {
    metrics.privilege_escalation_attempts++;
    TRY(metrics.suspicious_behaviors.try_append(
        "Privilege escalation attempted"_string));
}
```

#### Category 5: Memory Behavior

| Syscall | Metric | Weight | Notes |
|---------|--------|--------|-------|
| `mmap`, `mmap2`, `mremap` | `memory_operations++` | 1 | Memory allocation |
| `mprotect` | `self_modification_attempts++` (if RWX) | 4 | **Code page modification** |
| `munmap` | `memory_operations++` | 1 | Memory deallocation |

**Detection Logic**:
```cpp
if (syscall_name == "mprotect") {
    metrics.memory_operations++;
    // TODO: Parse protection flags to detect PROT_READ|PROT_WRITE|PROT_EXEC
    if (args_contain_rwx_protection(args)) {
        metrics.self_modification_attempts++;
        TRY(metrics.suspicious_behaviors.try_append(
            "RWX memory page detected (shellcode execution)"_string));
    }
}
```

### Implementation: Metric Update Function

```cpp
void BehavioralAnalyzer::update_metrics_from_syscall(
    StringView syscall_name,
    BehavioralMetrics& metrics)
{
    // Category 1: File System
    if (syscall_name == "open" || syscall_name == "openat" || syscall_name == "openat2" ||
        syscall_name == "read" || syscall_name == "write" ||
        syscall_name == "pread64" || syscall_name == "pwrite64" ||
        syscall_name == "readv" || syscall_name == "writev") {
        metrics.file_operations++;
    } else if (syscall_name == "unlink" || syscall_name == "unlinkat" ||
               syscall_name == "rmdir" || syscall_name == "rename" ||
               syscall_name == "renameat" || syscall_name == "truncate" ||
               syscall_name == "ftruncate") {
        metrics.file_operations += 2;  // Destructive operations
    }

    // Category 2: Process
    if (syscall_name == "fork" || syscall_name == "vfork" ||
        syscall_name == "clone" || syscall_name == "clone3") {
        metrics.process_operations += 2;
    } else if (syscall_name == "execve" || syscall_name == "execveat") {
        metrics.process_operations += 3;
    } else if (syscall_name == "ptrace" || syscall_name == "process_vm_readv" ||
               syscall_name == "process_vm_writev") {
        metrics.code_injection_attempts++;
        metrics.process_operations += 2;
    }

    // Category 3: Network
    if (syscall_name == "socket") {
        metrics.network_operations++;
    } else if (syscall_name == "connect") {
        metrics.outbound_connections++;
        metrics.network_operations++;
    } else if (syscall_name == "bind" || syscall_name == "listen" ||
               syscall_name == "accept" || syscall_name == "accept4") {
        metrics.network_operations += 2;
    } else if (syscall_name == "sendto" || syscall_name == "recvfrom" ||
               syscall_name == "sendmsg" || syscall_name == "recvmsg") {
        metrics.network_operations++;
    } else if (syscall_name == "sendmmsg" || syscall_name == "recvmmsg") {
        metrics.http_requests += 2;
    }

    // Category 4: Privilege Escalation
    if (syscall_name == "setuid" || syscall_name == "setuid32" ||
        syscall_name == "setgid" || syscall_name == "setgid32" ||
        syscall_name == "setreuid" || syscall_name == "setregid" ||
        syscall_name == "setresuid" || syscall_name == "setresgid" ||
        syscall_name == "capset") {
        metrics.privilege_escalation_attempts++;
    } else if (syscall_name == "init_module" || syscall_name == "finit_module") {
        metrics.persistence_mechanisms++;
    }

    // Category 5: Memory
    if (syscall_name == "mmap" || syscall_name == "mmap2" ||
        syscall_name == "munmap" || syscall_name == "mremap") {
        metrics.memory_operations++;
    } else if (syscall_name == "mprotect") {
        metrics.memory_operations++;
        // TODO: Detect RWX pages in Phase 3
        metrics.self_modification_attempts++;
    }
}
```

---

## 6. Performance Considerations

### Target Performance

- **Total analysis time**: < 5 seconds (95th percentile)
- **Syscall monitoring overhead**: < 500ms
- **Memory usage**: < 10MB for event buffer

### Optimization Strategies

#### 1. Polling Frequency

**Choice**: 100ms poll interval

**Rationale**:
- ✅ Responsive enough for 5-second timeout (50 checks max)
- ✅ Low CPU usage (< 1% polling overhead)
- ✅ Sufficient for capturing bursts of syscalls (nsjail buffers events)

**Alternative considered**: 10ms polling → 10x more syscalls, minimal benefit

#### 2. Buffer Sizes

```cpp
// Per-read buffer: 4096 bytes (one page)
char read_buffer[4096];

// Line accumulator: Maximum 4096 bytes
// Discard if exceeded (prevents memory exhaustion)
if (line_buffer.size() > 4096) {
    line_buffer.clear();
}

// Syscall event limit: 10,000 events max
// Typical malware: 100-1000 events in 5 seconds
// Worst case: 10,000 events = 100KB memory
```

#### 3. Early Termination

```cpp
// If critical threat detected early, skip remaining monitoring
if (metrics.code_injection_attempts > 5 ||
    metrics.privilege_escalation_attempts > 3 ||
    metrics.outbound_connections > 10) {

    dbgln("BehavioralAnalyzer: Critical threat detected, terminating early");
    metrics.threat_score = 1.0f;

    // Kill sandbox immediately
    kill(sandbox_pid, SIGKILL);
    return metrics;
}
```

#### 4. String Parsing Optimization

```cpp
// Avoid allocations in hot path
// Use StringView for parsing, only allocate String when storing

ErrorOr<void> parse_syscall_line_fast(StringView line, BehavioralMetrics& metrics)
{
    // Fast path: Check prefix without allocation
    if (!line.starts_with("[SYSCALL] "sv)) {
        return {};
    }

    // Extract syscall name (no allocation)
    auto syscall_part = line.substring_view(10);
    auto paren_pos = syscall_part.find('(');
    if (!paren_pos.has_value()) {
        return {};
    }

    auto syscall_name = syscall_part.substring_view(0, paren_pos.value());

    // Update metrics (inline comparison, no hash map lookup)
    update_metrics_from_syscall(syscall_name, metrics);

    return {};
}
```

### Benchmarking Plan

```bash
# Test 1: Benign executable (/bin/ls)
time ./analyze_nsjail /bin/ls
# Expected: < 1 second, 10-50 syscalls

# Test 2: Network beaconing simulator
time ./analyze_nsjail ./test_samples/network_beacon.sh
# Expected: < 2 seconds, 100-500 syscalls

# Test 3: Ransomware simulator (1000 file operations)
time ./analyze_nsjail ./test_samples/ransomware_sim.sh
# Expected: < 4 seconds, 1000-2000 syscalls

# Test 4: Fork bomb (stress test)
time ./analyze_nsjail ./test_samples/fork_bomb.sh
# Expected: < 5 seconds (timeout), 100-500 syscalls before kill
```

---

## 7. Error Handling

### Error Scenarios and Recovery

| Error | Detection | Recovery | Impact |
|-------|-----------|----------|--------|
| **Pipe full (EAGAIN)** | `read() returns -1` | Skip read, continue next poll | ⚠️ Lost events (acceptable) |
| **Pipe closed (POLLHUP)** | `poll() revents & POLLHUP` | Exit loop, finalize metrics | ✅ Normal termination |
| **Pipe error (POLLERR)** | `poll() revents & POLLERR` | Exit loop, log warning | ⚠️ Incomplete data |
| **Parse error (malformed line)** | Regex mismatch | Log warning, skip line | ✅ Continue monitoring |
| **Buffer overflow** | `line_buffer.size() > 4096` | Clear buffer, log warning | ⚠️ Lost partial line |
| **Process timeout** | `elapsed > timeout` | SIGKILL sandbox, set `timed_out=true` | ✅ Controlled termination |
| **Process crash** | `WIFSIGNALED(status)` | Set `exit_code`, increment `m_stats.crashes` | ✅ Normal handling |

### Error Handling Code

```cpp
ErrorOr<void> read_syscall_events_safe(
    int stderr_fd,
    ByteBuffer& line_buffer,
    BehavioralMetrics& metrics)
{
    char read_buffer[4096];
    ssize_t bytes_read = read(stderr_fd, read_buffer, sizeof(read_buffer));

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Non-blocking: no data available (expected)
            return {};
        }
        if (errno == EINTR) {
            // Interrupted by signal (expected during timeout handling)
            return {};
        }
        // Unexpected error
        dbgln("BehavioralAnalyzer: Warning - Read error on stderr: {}", strerror(errno));
        return Error::from_errno(errno);
    }

    if (bytes_read == 0) {
        // EOF - pipe closed (expected when process exits)
        return {};
    }

    // Append to buffer with overflow protection
    if (line_buffer.size() + bytes_read > 65536) {  // 64KB max
        dbgln("BehavioralAnalyzer: Warning - Event buffer overflow, discarding old data");
        line_buffer.clear();
    }

    TRY(line_buffer.try_append(read_buffer, bytes_read));

    // Process lines with error resilience
    while (auto newline_pos = line_buffer.bytes().find_byte_offset('\n')) {
        auto line_bytes = line_buffer.bytes().slice(0, newline_pos.value());
        StringView line { line_bytes };

        // Parse line (ignore errors)
        auto result = parse_syscall_line_fast(line, metrics);
        if (result.is_error()) {
            dbgln_if(false, "BehavioralAnalyzer: Failed to parse line: {}", line);
        }

        // Remove processed line
        line_buffer = TRY(ByteBuffer::copy(
            line_buffer.bytes().slice(newline_pos.value() + 1)));
    }

    return {};
}
```

### Graceful Degradation

**Philosophy**: Syscall monitoring is **best-effort**. Incomplete data is acceptable.

```cpp
// Even if syscall monitoring fails, return partial metrics
if (monitoring_result.is_error()) {
    dbgln("BehavioralAnalyzer: Syscall monitoring failed: {}",
          monitoring_result.error());

    // Still return metrics with whatever was collected
    metrics.suspicious_behaviors.try_append(
        "Syscall monitoring incomplete (limited visibility)"_string);

    // Lower confidence in threat score
    metrics.confidence = 0.5f;

    return metrics;  // Partial results better than complete failure
}
```

---

## 8. Integration into analyze_nsjail()

### Complete Implementation

```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail(
    ByteBuffer const& file_data,
    String const& filename)
{
    dbgln_if(false, "BehavioralAnalyzer: Starting nsjail analysis for '{}'", filename);

    BehavioralMetrics metrics;
    auto start_time = MonotonicTime::now();

    // Step 1: Write file to sandbox directory
    auto file_path = TRY(write_file_to_sandbox(m_sandbox_dir, file_data, filename));
    TRY(make_executable(file_path));

    // Step 2: Build nsjail command
    auto nsjail_command = TRY(build_nsjail_command(file_path));

    // Step 3: Launch nsjail with IPC pipes
    auto sandbox_process = TRY(launch_nsjail_sandbox(nsjail_command));

    // Cleanup on exit
    ScopeGuard cleanup_guard = [&] {
        // Close file descriptors
        (void)Core::System::close(sandbox_process.stdin_fd);
        (void)Core::System::close(sandbox_process.stdout_fd);
        (void)Core::System::close(sandbox_process.stderr_fd);

        // Kill sandbox process if still running
        kill(sandbox_process.pid, SIGKILL);
        waitpid(sandbox_process.pid, nullptr, WNOHANG);

        // Remove temp file
        (void)Core::System::unlink(file_path.bytes_as_string_view());
    };

    // Step 4: Set stderr to non-blocking mode
    int flags = TRY(Core::System::fcntl(sandbox_process.stderr_fd, F_GETFL));
    TRY(Core::System::fcntl(sandbox_process.stderr_fd, F_SETFL, flags | O_NONBLOCK));

    // Step 5: Main monitoring loop
    struct pollfd pfd = {
        .fd = sandbox_process.stderr_fd,
        .events = POLLIN,
        .revents = 0
    };

    ByteBuffer line_buffer;
    Duration timeout = m_config.timeout;
    bool process_exited = false;

    while (!process_exited && !metrics.timed_out) {
        // Check timeout
        auto elapsed = MonotonicTime::now() - start_time;
        if (elapsed > timeout) {
            dbgln("BehavioralAnalyzer: Analysis timeout after {}ms", elapsed.to_milliseconds());
            metrics.timed_out = true;
            m_stats.timeouts++;
            kill(sandbox_process.pid, SIGKILL);
            break;
        }

        // Poll stderr with 100ms timeout
        int poll_result = poll(&pfd, 1, 100);

        if (poll_result < 0) {
            if (errno == EINTR) continue;
            return Error::from_errno(errno);
        }

        if (poll_result > 0) {
            // Data available
            if (pfd.revents & POLLIN) {
                auto read_result = read_syscall_events_safe(
                    sandbox_process.stderr_fd, line_buffer, metrics);
                if (read_result.is_error()) {
                    dbgln("BehavioralAnalyzer: Warning - Failed to read events: {}",
                          read_result.error());
                }
            }

            if (pfd.revents & (POLLHUP | POLLERR)) {
                dbgln_if(false, "BehavioralAnalyzer: Pipe closed (process exited)");
                process_exited = true;
                break;
            }
        }

        // Check process status (non-blocking)
        int status = 0;
        pid_t wait_result = waitpid(sandbox_process.pid, &status, WNOHANG);
        if (wait_result > 0) {
            // Process exited
            process_exited = true;

            if (WIFEXITED(status)) {
                metrics.exit_code = WEXITSTATUS(status);
                dbgln_if(false, "BehavioralAnalyzer: Process exited with code {}",
                         metrics.exit_code);
            } else if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                metrics.exit_code = 128 + signal;
                dbgln_if(false, "BehavioralAnalyzer: Process killed by signal {}",
                         signal);

                if (signal != SIGKILL) {
                    m_stats.crashes++;
                }
            }
        }

        // Early termination check (optional optimization)
        if (metrics.code_injection_attempts > 5 ||
            metrics.privilege_escalation_attempts > 3) {
            dbgln("BehavioralAnalyzer: Critical threat detected, terminating early");
            kill(sandbox_process.pid, SIGKILL);
            break;
        }
    }

    // Step 6: Calculate final metrics
    metrics.execution_time = MonotonicTime::now() - start_time;

    dbgln_if(false, "BehavioralAnalyzer: Analysis complete - Metrics collected:");
    dbgln_if(false, "  File operations: {}", metrics.file_operations);
    dbgln_if(false, "  Process operations: {}", metrics.process_operations);
    dbgln_if(false, "  Network operations: {}", metrics.network_operations);
    dbgln_if(false, "  Code injection attempts: {}", metrics.code_injection_attempts);
    dbgln_if(false, "  Privilege escalation: {}", metrics.privilege_escalation_attempts);
    dbgln_if(false, "  Execution time: {}ms", metrics.execution_time.to_milliseconds());

    return metrics;
}
```

---

## 9. Testing Strategy

### Unit Tests

```cpp
// Services/Sentinel/Sandbox/TestBehavioralAnalyzer.cpp

TEST_CASE(parse_syscall_event_socket)
{
    auto event = parse_syscall_line("[SYSCALL] socket(AF_INET, SOCK_STREAM, 0)\n");
    EXPECT(event.has_value());
    EXPECT_EQ(event->name, "socket"sv);
    EXPECT_EQ(event->args.size(), 3u);
}

TEST_CASE(parse_syscall_event_fork)
{
    auto event = parse_syscall_line("[SYSCALL] fork()\n");
    EXPECT(event.has_value());
    EXPECT_EQ(event->name, "fork"sv);
    EXPECT_EQ(event->args.size(), 0u);
}

TEST_CASE(parse_syscall_event_invalid)
{
    auto event = parse_syscall_line("Invalid line\n");
    EXPECT(!event.has_value());
}

TEST_CASE(metric_update_network)
{
    BehavioralMetrics metrics;
    update_metrics_from_syscall("socket"sv, metrics);
    update_metrics_from_syscall("connect"sv, metrics);

    EXPECT_EQ(metrics.network_operations, 2u);
    EXPECT_EQ(metrics.outbound_connections, 1u);
}

TEST_CASE(metric_update_process)
{
    BehavioralMetrics metrics;
    update_metrics_from_syscall("fork"sv, metrics);
    update_metrics_from_syscall("execve"sv, metrics);

    EXPECT_EQ(metrics.process_operations, 5u);  // fork=2, execve=3
}
```

### Integration Tests

```bash
#!/bin/bash
# Services/Sentinel/Sandbox/test_syscall_monitoring.sh

set -e

# Test 1: Network operations
echo "Test 1: Network beaconing"
./test_samples/network_beacon.sh &
PID=$!
sleep 1
kill $PID

# Expected metrics:
# - network_operations > 5
# - outbound_connections > 2

# Test 2: Process spawning
echo "Test 2: Process chain"
./test_samples/fork_chain.sh

# Expected metrics:
# - process_operations > 10

# Test 3: File operations
echo "Test 3: Ransomware simulation"
./test_samples/ransomware_sim.sh

# Expected metrics:
# - file_operations > 100
# - threat_score > 0.6
```

### Stress Tests

```cpp
TEST_CASE(syscall_flood_resilience)
{
    // Generate 10,000 syscall events rapidly
    // Verify: No buffer overflow, no deadlock

    ByteBuffer test_events;
    for (int i = 0; i < 10000; i++) {
        test_events.append("[SYSCALL] socket()\n");
    }

    BehavioralMetrics metrics;
    ByteBuffer line_buffer;

    // Simulate flooding
    auto result = process_syscall_buffer(test_events, line_buffer, metrics);
    EXPECT(!result.is_error());

    // Verify metrics updated
    EXPECT_GE(metrics.network_operations, 10000u);
}
```

---

## 10. Open Questions

### Q1: Should we implement argument parsing in Phase 2?

**Answer**: **No, defer to Phase 3**.

**Rationale**:
- Phase 2 goal: Get syscall monitoring working end-to-end
- Syscall name alone provides 80% detection value
- Argument parsing adds complexity (quoted strings, struct parsing)
- Phase 3 can enhance with IP extraction, path analysis, etc.

**Decision**: Parse syscall name only in Phase 2. Add argument parsing in Phase 3.

---

### Q2: How do we handle nsjail's own syscalls?

**Answer**: **Filter by PID in Phase 3, ignore for Phase 2**.

**Rationale**:
- nsjail parent process also generates syscalls (setup, monitoring)
- Sandboxed child process (malware) generates the interesting syscalls
- nsjail logs include PID in verbose mode: `[SYSCALL:1234] socket()`

**Decision**:
- Phase 2: Accept all syscalls (slight noise, acceptable)
- Phase 3: Parse PID from logs, filter to child process only

---

### Q3: What if stderr buffer fills up?

**Answer**: **Non-blocking I/O prevents deadlock, lost events are acceptable**.

**Rationale**:
- nsjail stderr buffer: 64KB (kernel pipe buffer)
- If analyzer can't keep up, new events overwrite old ones
- Non-blocking read prevents deadlock
- Lost events reduce detection granularity but don't break analysis

**Decision**:
- Use non-blocking I/O
- Log warning if buffer full
- Proceed with partial metrics (graceful degradation)

---

### Q4: Should we use a separate thread for syscall monitoring?

**Answer**: **No, single-threaded with poll() is sufficient**.

**Rationale**:
- poll() provides concurrent I/O without threads
- No synchronization overhead
- Simpler debugging (no race conditions)
- Performance target easily met with single thread

**Decision**: Single-threaded with poll() for Phase 2.

---

### Q5: How do we benchmark syscall monitoring performance?

**Answer**: **Use integration tests with known syscall counts**.

**Plan**:
```bash
# Benchmark script
./benchmark_syscall_monitoring.sh

Test 1: 10 syscalls (/bin/echo)    → < 500ms
Test 2: 100 syscalls (/bin/ls -lR) → < 1000ms
Test 3: 1000 syscalls (find /)     → < 2000ms
Test 4: 10000 syscalls (fork bomb) → < 5000ms (timeout)
```

**Metrics**:
- Parse rate: > 10,000 syscalls/second
- Memory overhead: < 10MB
- CPU overhead: < 10% of single core

---

## Summary

### Key Design Decisions

1. ✅ **Non-blocking poll()** for pipe I/O (prevents deadlock)
2. ✅ **Line buffering** with 4096-byte max (prevents memory exhaustion)
3. ✅ **Syscall name only** parsing in Phase 2 (simplicity)
4. ✅ **Metric mapping table** with weights (tunable detection)
5. ✅ **Graceful degradation** for all error cases (reliability)
6. ✅ **Single-threaded** monitoring (simplicity)
7. ✅ **100ms poll interval** (performance vs responsiveness)

### Implementation Priority

**Week 2 Tasks**:

1. **Day 1-2**: Implement `monitor_syscall_events()` with poll()
2. **Day 2-3**: Implement `parse_syscall_line()` with regex
3. **Day 3-4**: Implement `update_metrics_from_syscall()` mapping
4. **Day 4-5**: Integrate into `analyze_nsjail()` main loop
5. **Day 5**: Unit tests + integration tests
6. **Day 6**: Performance benchmarking + optimization

### Success Criteria

- ✅ Parse syscall events from nsjail stderr
- ✅ Update BehavioralMetrics counters correctly
- ✅ Complete analysis in < 5 seconds
- ✅ No deadlocks or buffer overflows
- ✅ Graceful error handling for all edge cases

---

## References

- **Implementation Checklist**: `BEHAVIORAL_ANALYZER_IMPLEMENTATION_CHECKLIST.md`
- **BehavioralAnalyzer API**: `BehavioralAnalyzer.{h,cpp}`
- **nsjail Config**: `configs/malware-sandbox.cfg`
- **Seccomp Policy**: `configs/malware-sandbox.kafel`
- **Week 1 Report**: `WEEK1_DAY4-5_COMPLETION_REPORT.md`

---

**Document Status**: ✅ Design Complete
**Next Step**: Begin Week 2 implementation (syscall monitoring core)
**Owner**: Security Research Team
**Review Date**: 2025-11-03
