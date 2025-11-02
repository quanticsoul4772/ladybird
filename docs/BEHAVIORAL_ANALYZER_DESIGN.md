# BehavioralAnalyzer Real Implementation Design

**Version**: 1.0
**Date**: 2025-11-02
**Status**: Design Complete - Ready for Phase 2 Implementation
**Milestone**: 0.5 Phase 2 - Real-time Behavioral Analysis
**Author**: Security Research Team

---

## Executive Summary

This document specifies the **real implementation** of `BehavioralAnalyzer` to replace the current stub implementation. The design uses **nsjail + seccomp-BPF** for syscall-level behavioral monitoring on Linux while maintaining graceful degradation on other platforms.

### Key Design Decisions

1. **Primary Platform**: Linux with nsjail sandboxing
2. **Syscall Monitoring**: seccomp-BPF with ptrace fallback
3. **Interface Compatibility**: Maintains existing `BehavioralMetrics` structure
4. **Execution Model**: Fork-exec with IPC for result collection
5. **Timeout Enforcement**: Timer-based process termination
6. **Platform Support**: Real implementation on Linux, enhanced stub on macOS/Windows

### Performance Targets

- **Analysis Time**: 1-5 seconds per suspicious file
- **Memory Overhead**: < 50MB per analysis
- **CPU Usage**: Single core, burst workload
- **Detection Rate**: 90%+ on known malware patterns
- **False Positive Rate**: < 5% on legitimate software

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Execution Flow](#execution-flow)
3. [nsjail Integration](#nsjail-integration)
4. [Syscall Monitoring Strategy](#syscall-monitoring-strategy)
5. [Behavioral Patterns Detection](#behavioral-patterns-detection)
6. [Scoring Algorithm](#scoring-algorithm)
7. [Platform Support](#platform-support)
8. [Interface Specification](#interface-specification)
9. [Implementation Pseudo-code](#implementation-pseudo-code)
10. [Integration Checklist](#integration-checklist)
11. [Security Considerations](#security-considerations)

---

## Architecture Overview

### Component Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    Orchestrator                               │
│  (calls BehavioralAnalyzer::analyze())                       │
└──────────────────────┬───────────────────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────────────────┐
│              BehavioralAnalyzer                               │
│  ┌────────────────────────────────────────────────────┐      │
│  │ 1. Write file to temp location                     │      │
│  │ 2. Fork process for monitoring                     │      │
│  │ 3. Exec nsjail with target binary                  │      │
│  │ 4. Monitor syscalls via seccomp-BPF + ptrace       │      │
│  │ 5. Collect metrics (IPC pipe)                      │      │
│  │ 6. Kill sandbox on timeout                         │      │
│  │ 7. Parse results → BehavioralMetrics               │      │
│  └────────────────────────────────────────────────────┘      │
└──────────────────┬────────────────────────────────────────┬──┘
                   │                                        │
                   ▼                                        ▼
        ┌──────────────────┐                    ┌─────────────────┐
        │  nsjail Process  │                    │  Monitor Thread │
        │  (sandboxed)     │                    │  (parent proc)  │
        │                  │                    │                 │
        │  ┌────────────┐  │                    │ Parses seccomp  │
        │  │ Target     │  │──syscalls──────────▶│ events via     │
        │  │ Binary     │  │    (ptrace)         │ /proc/*/syscall│
        │  │ Execution  │  │                    │                 │
        │  └────────────┘  │                    │ Updates metrics │
        │                  │                    │ counter         │
        └──────────────────┘                    └─────────────────┘
                   │
                   ▼
        (writes to IPC pipe: results.json)
```

### Data Flow

```
File Download (ByteBuffer)
    │
    ├──> Write to /tmp/sentinel-XXXXXX/target_binary
    │
    ├──> Fork monitoring process
    │    │
    │    ├──> Parent: Start timeout timer (alarm())
    │    │         Monitor /proc/child_pid/syscall
    │    │         Parse seccomp notifications (seccomp_unotify)
    │    │
    │    └──> Child: exec nsjail with:
    │              - seccomp filter (whitelist/monitor/block)
    │              - network namespace isolation
    │              - read-only filesystem (except /tmp)
    │              - resource limits (CPU, memory, file descriptors)
    │
    ├──> Collect syscall events:
    │    │
    │    ├──> File operations: open, read, write, unlink, chmod
    │    ├──> Process ops: fork, clone, exec, ptrace
    │    ├──> Network ops: socket, connect, bind, send, recv
    │    ├──> Memory ops: mmap, mprotect, mremap
    │    └──> Registry ops: N/A on Linux (Windows: RegOpenKey, etc.)
    │
    ├──> Calculate scores:
    │    │
    │    ├──> File I/O score (40%)
    │    ├──> Process score (30%)
    │    ├──> Memory score (15%)
    │    ├──> Network score (10%)
    │    └──> Platform score (5%)
    │
    └──> Return BehavioralMetrics
         - threat_score: 0.0-1.0
         - suspicious_behaviors: Vector<String>
         - operation counts (file, process, network, etc.)
```

---

## Execution Flow

### High-Level Sequence

```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    // Phase 1: Setup
    auto temp_dir = TRY(create_temp_sandbox_directory());
    auto target_path = TRY(write_file_to_sandbox(temp_dir, file_data, filename));
    TRY(make_executable(target_path));

    // Phase 2: Launch nsjail sandbox
    auto [monitor_pid, result_pipe] = TRY(launch_nsjail_sandbox(
        target_path,
        timeout
    ));

    // Phase 3: Monitor execution
    auto metrics = TRY(monitor_execution(
        monitor_pid,
        result_pipe,
        timeout
    ));

    // Phase 4: Cleanup
    TRY(kill_sandbox_if_running(monitor_pid));
    TRY(cleanup_temp_directory(temp_dir));

    // Phase 5: Score behavioral patterns
    metrics.threat_score = TRY(calculate_threat_score(metrics));
    metrics.suspicious_behaviors = TRY(generate_suspicious_behaviors(metrics));

    return metrics;
}
```

### Detailed Step-by-Step

#### Step 1: Temporary File Creation

```cpp
ErrorOr<String> create_temp_sandbox_directory()
{
    // Use mkdtemp for atomic directory creation
    // Template: /tmp/sentinel-XXXXXX
    auto template_str = "/tmp/sentinel-XXXXXX"_string;
    char* dir_path = mkdtemp(template_str.bytes_as_string_view().to_byte_buffer().data());
    if (!dir_path)
        return Error::from_errno(errno);

    return String::from_utf8(dir_path);
}

ErrorOr<String> write_file_to_sandbox(
    String const& temp_dir,
    ByteBuffer const& file_data,
    String const& filename)
{
    auto target_path = TRY(String::formatted("{}/{}", temp_dir, filename));

    // Write file with secure permissions (0600)
    auto fd = TRY(Core::System::open(target_path, O_WRONLY | O_CREAT | O_EXCL, 0600));
    TRY(Core::System::write(fd, file_data));
    TRY(Core::System::close(fd));

    return target_path;
}

ErrorOr<void> make_executable(String const& path)
{
    // chmod +x for execution (if binary)
    TRY(Core::System::chmod(path, 0700));
    return {};
}
```

#### Step 2: nsjail Sandbox Launch

```cpp
struct SandboxProcess {
    pid_t pid;
    int result_pipe_fd;  // Parent reads, child writes
    int syscall_pipe_fd; // For seccomp notifications
};

ErrorOr<SandboxProcess> launch_nsjail_sandbox(
    String const& target_path,
    Duration timeout)
{
    // Create IPC pipes
    int result_pipe[2];
    int syscall_pipe[2];
    TRY(Core::System::pipe2(result_pipe, O_CLOEXEC));
    TRY(Core::System::pipe2(syscall_pipe, O_CLOEXEC));

    pid_t pid = TRY(Core::System::fork());

    if (pid == 0) {
        // Child process: exec nsjail
        close(result_pipe[0]);  // Close read end
        close(syscall_pipe[0]);

        TRY(exec_nsjail_with_target(
            target_path,
            result_pipe[1],
            syscall_pipe[1],
            timeout
        ));

        // Never returns if successful
        _exit(1);
    }

    // Parent process
    close(result_pipe[1]);  // Close write end
    close(syscall_pipe[1]);

    return SandboxProcess {
        .pid = pid,
        .result_pipe_fd = result_pipe[0],
        .syscall_pipe_fd = syscall_pipe[0]
    };
}
```

#### Step 3: nsjail Configuration & Execution

```cpp
ErrorOr<void> exec_nsjail_with_target(
    String const& target_path,
    int result_pipe_fd,
    int syscall_pipe_fd,
    Duration timeout)
{
    // Build nsjail command line arguments
    Vector<ByteString> args;

    // Basic nsjail setup
    TRY(args.try_append("/usr/bin/nsjail"_string));

    // Execution mode (once, no server)
    TRY(args.try_append("--mode"_string));
    TRY(args.try_append("o"_string));  // once

    // Time limit (timeout in seconds)
    TRY(args.try_append("--time_limit"_string));
    TRY(args.try_append(TRY(String::number(timeout.to_seconds()))));

    // Resource limits
    TRY(args.try_append("--rlimit_as"_string));
    TRY(args.try_append("128"_string));  // 128MB virtual memory

    TRY(args.try_append("--rlimit_cpu"_string));
    TRY(args.try_append("5"_string));    // 5 seconds CPU time

    TRY(args.try_append("--rlimit_fsize"_string));
    TRY(args.try_append("10"_string));   // 10MB file writes max

    TRY(args.try_append("--rlimit_nofile"_string));
    TRY(args.try_append("64"_string));   // 64 file descriptors max

    // Network isolation (no network access)
    TRY(args.try_append("--disable_clone_newnet"_string));
    TRY(args.try_append("false"_string));

    // Mount read-only root filesystem
    TRY(args.try_append("--chroot"_string));
    TRY(args.try_append("/"_string));

    // Allow /tmp for writes (isolated)
    TRY(args.try_append("--bindmount"_string));
    TRY(args.try_append("/tmp:/tmp:rw"_string));

    // Seccomp policy file
    TRY(args.try_append("--seccomp_string"_string));
    TRY(args.try_append(TRY(generate_seccomp_policy())));

    // Pipe file descriptors for result collection
    TRY(args.try_append("--pass_fd"_string));
    TRY(args.try_append(TRY(String::number(result_pipe_fd))));

    TRY(args.try_append("--pass_fd"_string));
    TRY(args.try_append(TRY(String::number(syscall_pipe_fd))));

    // Target executable
    TRY(args.try_append("--"_string));
    TRY(args.try_append(target_path));

    // Execute nsjail
    TRY(Core::System::execvp(args[0], args));

    return {};  // Never reaches here
}
```

---

## nsjail Integration

### Why nsjail?

**nsjail** is Google's sandboxing tool used in Chrome for renderer isolation. Benefits:

1. **Battle-Tested**: 15+ years protecting Chrome from exploits
2. **Production-Ready**: Used by Google, Chromium, Cloud services
3. **Resource Limits**: CPU, memory, file descriptor limits via rlimit
4. **seccomp-BPF**: System call filtering (allow/monitor/block)
5. **Namespace Isolation**: Network, PID, mount, user namespaces
6. **Simple Integration**: Command-line interface, no library linking

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install nsjail

# From source (if unavailable in repos)
git clone https://github.com/google/nsjail.git
cd nsjail
make
sudo make install
```

### Configuration File (Optional Alternative to CLI)

**File**: `Services/Sentinel/assets/nsjail_behavioral.cfg`

```protobuf
mode: ONCE
time_limit: 5
hostname: "sentinel-sandbox"

# Resource limits
rlimit_as: 128       # 128MB virtual memory
rlimit_cpu: 5        # 5 seconds CPU time
rlimit_fsize: 10     # 10MB max file writes
rlimit_nofile: 64    # 64 file descriptors
rlimit_nproc: 16     # 16 child processes max

# Network isolation
disable_clone_newnet: false

# Filesystem
chroot: "/"

mount {
  src: "/tmp"
  dst: "/tmp"
  is_bind: true
  rw: true
}

mount {
  src: "/lib"
  dst: "/lib"
  is_bind: true
}

mount {
  src: "/lib64"
  dst: "/lib64"
  is_bind: true
}

mount {
  src: "/usr"
  dst: "/usr"
  is_bind: true
}

# seccomp policy
seccomp_string: "
  # Allow syscalls needed for basic execution
  POLICY default ALLOW

  # Monitor suspicious syscalls
  ALLOW { write, read, open, openat, close, lseek }
  ALLOW { mmap, munmap, mprotect, brk }
  ALLOW { stat, fstat, lstat, access }
  ALLOW { getpid, gettid, getuid, getgid }
  ALLOW { exit, exit_group }

  # Monitor process operations
  TRACE { fork, clone, vfork, execve }

  # Monitor network operations
  TRACE { socket, connect, bind, listen, accept, sendto, recvfrom }

  # Monitor file operations
  TRACE { unlink, unlinkat, rename, chmod, chown }

  # Block dangerous syscalls
  KILL { reboot, mount, umount, swapon, swapoff }
  KILL { ptrace, process_vm_readv, process_vm_writev }
  KILL { init_module, delete_module, finit_module }
"
```

---

## Syscall Monitoring Strategy

### Three-Tier Monitoring Approach

```
┌─────────────────────────────────────────────────────────┐
│  Tier 1: seccomp-BPF Filter (Kernel Space)             │
│  - Whitelist: Allow basic syscalls                      │
│  - Monitor: Log suspicious syscalls (fork, exec, etc.)  │
│  - Block: Kill process on dangerous syscalls            │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼ (syscall event)
┌─────────────────────────────────────────────────────────┐
│  Tier 2: User Notification (seccomp_unotify)           │
│  - Receive syscall details from kernel                  │
│  - Parse syscall number, args, return value             │
│  - Update BehavioralMetrics counters                    │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼ (fallback if unotify unavailable)
┌─────────────────────────────────────────────────────────┐
│  Tier 3: ptrace Monitoring (User Space)                │
│  - PTRACE_SEIZE on sandbox child process                │
│  - PTRACE_SYSCALL to intercept each syscall             │
│  - Parse /proc/<pid>/syscall for syscall details        │
│  - Slower but universal fallback                        │
└─────────────────────────────────────────────────────────┘
```

### seccomp-BPF Policy Specification

#### Policy File Format

**File**: `Services/Sentinel/assets/seccomp_behavioral_policy.bpf`

```c
// seccomp-BPF policy for behavioral analysis sandbox
// Format: Berkeley Packet Filter for syscall filtering

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/syscall.h>

// Architecture validation
BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

// Load syscall number
BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),

// Category 1: File I/O Operations (ALLOW but COUNT)
// open, openat, read, write, close, lseek, stat, fstat
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

// Category 2: Process Operations (TRACE for monitoring)
// fork, clone, vfork, execve
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fork, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_execve, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),

// Category 3: Network Operations (TRACE for monitoring)
// socket, connect, bind, listen, accept, send, recv
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_socket, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_connect, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),

// Category 4: Memory Operations (TRACE for monitoring)
// mmap, mprotect, mremap
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mmap, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mprotect, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRACE),

// Category 5: Dangerous Syscalls (KILL immediately)
// reboot, mount, umount, ptrace, init_module
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_reboot, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mount, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ptrace, 0, 1),
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

// Default: Allow (for basic syscalls like exit, getpid, etc.)
BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
```

#### Syscall Categories

| Category | Syscalls | Action | Weight |
|----------|----------|--------|--------|
| **File I/O** | open, read, write, unlink, chmod | ALLOW + COUNT | 0.40 |
| **Process** | fork, clone, exec, ptrace | TRACE | 0.30 |
| **Network** | socket, connect, send, recv | TRACE | 0.10 |
| **Memory** | mmap, mprotect, mremap | TRACE | 0.15 |
| **Dangerous** | reboot, mount, init_module | KILL | 1.00 |

---

## Behavioral Patterns Detection

### Pattern Definitions

Following the `BEHAVIORAL_ANALYSIS_SPEC.md`, we detect:

#### 1. File I/O Patterns (40% weight)

```cpp
struct FilePattern {
    u32 rapid_modification_count;      // Files modified in <10s
    u32 high_entropy_writes;           // Writes with entropy > 7.0
    u32 sensitive_directory_accesses;  // SSH keys, browser data, etc.
    u32 executable_drops;              // Created .exe/.sh/.bat files
};

float calculate_file_io_score(FilePattern const& pattern)
{
    float score = 0.0f;

    // Ransomware pattern: >100 files modified in <10s
    if (pattern.rapid_modification_count > 100)
        score += 0.75f;
    else if (pattern.rapid_modification_count > 50)
        score += 0.40f;

    // Encryption pattern: high entropy writes
    if (pattern.high_entropy_writes > 10)
        score += 0.70f;
    else if (pattern.high_entropy_writes > 5)
        score += 0.35f;

    // Information stealer: SSH keys, browser data accessed
    if (pattern.sensitive_directory_accesses > 3)
        score += 0.80f;
    else if (pattern.sensitive_directory_accesses > 0)
        score += 0.50f;

    // Executable drops: dropper/worm pattern
    if (pattern.executable_drops > 0)
        score += 0.60f;

    return min(1.0f, score);
}
```

#### 2. Process Behavior Patterns (30% weight)

```cpp
struct ProcessPattern {
    u32 process_chain_depth;           // Max fork depth
    bool injection_detected;           // ptrace to other processes
    u32 suspicious_subprocess_count;   // powershell/cmd/bash spawns
};

float calculate_process_score(ProcessPattern const& pattern)
{
    float score = 0.0f;

    // Deep process chains: privilege escalation
    if (pattern.process_chain_depth > 5)
        score += 0.80f;
    else if (pattern.process_chain_depth > 3)
        score += 0.40f;

    // Process injection: rootkit/banker pattern
    if (pattern.injection_detected)
        score += 0.70f;

    // Suspicious subprocesses: command injection
    if (pattern.suspicious_subprocess_count > 3)
        score += 0.50f;
    else if (pattern.suspicious_subprocess_count > 0)
        score += 0.30f;

    return min(1.0f, score);
}
```

#### 3. Memory Behavior Patterns (15% weight)

```cpp
struct MemoryPattern {
    u32 heap_spray_allocations;        // Repeated same-size allocations
    bool executable_memory_writes;     // RWX memory with writes
};

float calculate_memory_score(MemoryPattern const& pattern)
{
    float score = 0.0f;

    // Heap spraying: exploit kit pattern
    if (pattern.heap_spray_allocations > 100)
        score += 0.60f;

    // Executable memory writes: shellcode pattern
    if (pattern.executable_memory_writes)
        score += 0.80f;

    return min(1.0f, score);
}
```

#### 4. Network Behavior Patterns (10% weight)

```cpp
struct NetworkPattern {
    u32 outbound_connections;          // Unique remote IPs
    u32 dns_queries;                   // DNS lookups
    u32 http_requests;                 // HTTP GET/POST
};

float calculate_network_score(NetworkPattern const& pattern)
{
    float score = 0.0f;

    // C2 beaconing: multiple outbound connections
    if (pattern.outbound_connections > 3)
        score += 0.60f;
    else if (pattern.outbound_connections > 1)
        score += 0.30f;

    // DNS tunneling: excessive queries
    if (pattern.dns_queries > 10)
        score += 0.50f;

    // Data exfiltration: many HTTP requests
    if (pattern.http_requests > 10)
        score += 0.40f;

    return min(1.0f, score);
}
```

### Syscall-to-Pattern Mapping

```cpp
void update_metrics_from_syscall(
    StringView syscall_name,
    BehavioralMetrics& metrics)
{
    // File operations
    if (syscall_name == "open"sv || syscall_name == "openat"sv) {
        metrics.file_operations++;
    }
    else if (syscall_name == "write"sv || syscall_name == "writev"sv) {
        metrics.file_operations++;
        // TODO: Check entropy of written data
    }
    else if (syscall_name == "unlink"sv || syscall_name == "unlinkat"sv) {
        metrics.file_operations++;
        metrics.temp_file_creates++;  // File deletion (ransomware cleanup)
    }

    // Process operations
    else if (syscall_name == "fork"sv || syscall_name == "clone"sv) {
        metrics.process_operations++;
    }
    else if (syscall_name == "execve"sv || syscall_name == "execveat"sv) {
        metrics.process_operations++;
    }
    else if (syscall_name == "ptrace"sv) {
        metrics.self_modification_attempts++;  // Process injection
    }

    // Network operations
    else if (syscall_name == "socket"sv) {
        metrics.network_operations++;
    }
    else if (syscall_name == "connect"sv) {
        metrics.network_operations++;
        metrics.outbound_connections++;
    }
    else if (syscall_name == "sendto"sv || syscall_name == "send"sv) {
        metrics.network_operations++;
        metrics.http_requests++;  // Approximate (needs payload inspection)
    }

    // Memory operations
    else if (syscall_name == "mmap"sv) {
        metrics.memory_operations++;
        // TODO: Track allocation size for heap spray detection
    }
    else if (syscall_name == "mprotect"sv) {
        metrics.memory_operations++;
        // TODO: Check for RWX pages (executable + writable)
        metrics.code_injection_attempts++;
    }
}
```

---

## Scoring Algorithm

### Multi-Factor Scoring

As per `BEHAVIORAL_ANALYSIS_SPEC.md`:

```cpp
ErrorOr<float> calculate_threat_score(BehavioralMetrics const& metrics)
{
    // Category 1: File System (40% weight)
    float file_score = 0.0f;
    if (metrics.file_operations > 100) file_score += 0.75f;  // Ransomware
    if (metrics.temp_file_creates > 10) file_score += 0.30f;
    if (metrics.hidden_file_creates > 0) file_score += 0.20f;
    if (metrics.executable_drops > 0) file_score += 0.60f;
    file_score = min(1.0f, file_score);

    // Category 2: Process (30% weight)
    float process_score = 0.0f;
    if (metrics.process_operations > 10) process_score += 0.50f;
    if (metrics.self_modification_attempts > 0) process_score += 0.70f;  // Injection
    if (metrics.persistence_mechanisms > 0) process_score += 0.60f;
    process_score = min(1.0f, process_score);

    // Category 3: Network (10% weight)
    float network_score = 0.0f;
    if (metrics.network_operations > 5) network_score += 0.30f;
    if (metrics.outbound_connections > 3) network_score += 0.60f;  // C2 beaconing
    if (metrics.dns_queries > 10) network_score += 0.50f;
    network_score = min(1.0f, network_score);

    // Category 4: Memory (15% weight)
    float memory_score = 0.0f;
    if (metrics.memory_operations > 20) memory_score += 0.40f;
    if (metrics.code_injection_attempts > 0) memory_score += 0.80f;  // RWX pages
    memory_score = min(1.0f, memory_score);

    // Category 5: Platform-specific (5% weight)
    float platform_score = 0.0f;
    if (metrics.privilege_escalation_attempts > 0) platform_score = 0.90f;

    // Weighted combination
    float threat_score = (
        file_score * 0.40f +
        process_score * 0.30f +
        network_score * 0.10f +
        memory_score * 0.15f +
        platform_score * 0.05f
    );

    return min(1.0f, threat_score);
}
```

### Suspicious Behavior Descriptions

```cpp
ErrorOr<Vector<String>> generate_suspicious_behaviors(
    BehavioralMetrics const& metrics)
{
    Vector<String> behaviors;

    // File I/O suspicions
    if (metrics.file_operations > 100) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "Rapid file modification: {} files (ransomware pattern)",
            metrics.file_operations
        ))));
    }

    if (metrics.executable_drops > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "Executable file dropped: {} files (dropper/worm pattern)",
            metrics.executable_drops
        ))));
    }

    // Process suspicions
    if (metrics.process_operations > 10) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "Excessive process spawning: {} operations",
            metrics.process_operations
        ))));
    }

    if (metrics.self_modification_attempts > 0) {
        TRY(behaviors.try_append(
            "Process injection detected (ptrace/WriteProcessMemory)"_string
        ));
    }

    // Network suspicions
    if (metrics.outbound_connections > 3) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "Multiple outbound connections: {} (C2 beaconing pattern)",
            metrics.outbound_connections
        ))));
    }

    // Memory suspicions
    if (metrics.code_injection_attempts > 0) {
        TRY(behaviors.try_append(
            "Executable memory writes detected (shellcode/ROP pattern)"_string
        ));
    }

    // Platform-specific suspicions
    if (metrics.privilege_escalation_attempts > 0) {
        TRY(behaviors.try_append(
            "Privilege escalation attempted (setuid/sudo abuse)"_string
        ));
    }

    return behaviors;
}
```

---

## Platform Support

### Linux (Primary - Real Implementation)

**Technology Stack**:
- nsjail for sandboxing
- seccomp-BPF for syscall filtering
- ptrace fallback for syscall monitoring
- /proc filesystem for process introspection

**Detection Capabilities**:
- ✅ File I/O monitoring (open, write, unlink)
- ✅ Process operations (fork, exec, ptrace)
- ✅ Network operations (socket, connect, send)
- ✅ Memory operations (mmap, mprotect)
- ✅ Privilege escalation (setuid, sudo)
- ✅ Persistence mechanisms (cron, systemd)

### macOS (Enhanced Stub)

**Limitations**:
- No nsjail equivalent (different sandbox model)
- Apple Sandbox (sandbox-exec) is macOS-specific
- Limited syscall introspection

**Fallback Strategy**:
```cpp
#ifdef __APPLE__
ErrorOr<BehavioralMetrics> analyze_nsjail(/* args */)
{
    // Use macOS-specific heuristics:
    // 1. Static file analysis (entropy, headers)
    // 2. File operation pattern detection (FSEvents API)
    // 3. Process monitoring (kqueue for proc events)
    // 4. Network monitoring (Network Extension framework)

    return analyze_mock(file_data, filename);  // Enhanced stub
}
#endif
```

**Detection Capabilities**:
- ⚠️ File I/O monitoring (via FSEvents, limited)
- ⚠️ Process operations (via kqueue, basic)
- ❌ Network operations (requires root/Network Extension)
- ❌ Memory operations (no ptrace equivalent)
- ❌ Syscall-level monitoring

### Windows (Enhanced Stub)

**Limitations**:
- No nsjail (Windows uses different sandboxing)
- Windows Sandbox available on Windows 10 Pro+
- Requires COM/WMI for process monitoring

**Fallback Strategy**:
```cpp
#ifdef _WIN32
ErrorOr<BehavioralMetrics> analyze_nsjail(/* args */)
{
    // Use Windows-specific heuristics:
    // 1. Static file analysis (PE header inspection)
    // 2. Registry monitoring (RegNotifyChangeKeyValue)
    // 3. File operation patterns (ReadDirectoryChangesW)
    // 4. Process monitoring (Job Objects, WMI)

    return analyze_mock(file_data, filename);  // Enhanced stub
}
#endif
```

**Detection Capabilities**:
- ⚠️ File I/O monitoring (via ReadDirectoryChangesW)
- ⚠️ Process operations (via WMI/Job Objects)
- ⚠️ Registry monitoring (via RegNotifyChangeKeyValue)
- ❌ Network operations (requires driver/ETW)
- ❌ Syscall-level monitoring

---

## Interface Specification

### Public Interface (No Changes)

The following interface MUST remain unchanged for Orchestrator compatibility:

```cpp
class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(
        SandboxConfig const& config);

    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(
        SandboxConfig const& config,
        SyscallFilter const& filter);

    ~BehavioralAnalyzer();

    // Main analysis method (MUST maintain signature)
    ErrorOr<BehavioralMetrics> analyze(
        ByteBuffer const& file_data,
        String const& filename,
        Duration timeout);

    // Statistics (existing interface)
    Statistics get_statistics() const;
    void reset_statistics();

    // Configuration (existing interface)
    void update_filter(SyscallFilter const& filter);
    SyscallFilter const& filter() const;

private:
    // Implementation details hidden
};
```

### Result Structure (No Changes)

```cpp
struct BehavioralMetrics {
    // Category 1: File System (4 metrics)
    u32 file_operations { 0 };
    u32 temp_file_creates { 0 };
    u32 hidden_file_creates { 0 };
    u32 executable_drops { 0 };

    // Category 2: Process & Execution (3 metrics)
    u32 process_operations { 0 };
    u32 self_modification_attempts { 0 };
    u32 persistence_mechanisms { 0 };

    // Category 3: Network (4 metrics)
    u32 network_operations { 0 };
    u32 outbound_connections { 0 };
    u32 dns_queries { 0 };
    u32 http_requests { 0 };

    // Category 4: System & Registry (3 metrics)
    u32 registry_operations { 0 };
    u32 service_modifications { 0 };
    u32 privilege_escalation_attempts { 0 };

    // Category 5: Memory (2 metrics)
    u32 memory_operations { 0 };
    u32 code_injection_attempts { 0 };

    // Aggregated results
    float threat_score { 0.0f };           // 0.0-1.0
    Vector<String> suspicious_behaviors;   // Human-readable

    // Execution metadata
    Duration execution_time;
    bool timed_out { false };
    i32 exit_code { 0 };
};
```

---

## Implementation Pseudo-code

### Core Analysis Method

```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    auto start_time = MonotonicTime::now();
    m_stats.total_analyses++;

    BehavioralMetrics metrics;

    // Platform-specific dispatch
#if defined(__linux__)
    if (!m_use_mock && nsjail_available()) {
        auto result = TRY(analyze_nsjail_linux(file_data, filename, timeout));
        metrics = move(result);
    } else {
        auto result = TRY(analyze_mock(file_data, filename));
        metrics = move(result);
    }
#elif defined(__APPLE__)
    // macOS: Enhanced stub with FSEvents monitoring
    auto result = TRY(analyze_macos_enhanced(file_data, filename));
    metrics = move(result);
#elif defined(_WIN32)
    // Windows: Enhanced stub with Registry/WMI monitoring
    auto result = TRY(analyze_windows_enhanced(file_data, filename));
    metrics = move(result);
#else
    // Fallback: Mock implementation
    auto result = TRY(analyze_mock(file_data, filename));
    metrics = move(result);
#endif

    metrics.execution_time = MonotonicTime::now() - start_time;

    // Check timeout
    if (metrics.execution_time > timeout) {
        metrics.timed_out = true;
        m_stats.timeouts++;
    }

    // Calculate threat score
    metrics.threat_score = TRY(calculate_threat_score(metrics));
    metrics.suspicious_behaviors = TRY(generate_suspicious_behaviors(metrics));

    // Update statistics
    update_statistics(metrics);

    return metrics;
}
```

### Linux nsjail Implementation

```cpp
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail_linux(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    dbgln("BehavioralAnalyzer: Using nsjail for real execution");

    BehavioralMetrics metrics;

    // Step 1: Create temporary sandbox directory
    auto temp_dir = TRY(create_temp_sandbox_directory());

    // Step 2: Write target file
    auto target_path = TRY(write_file_to_sandbox(temp_dir, file_data, filename));
    TRY(make_executable_if_binary(target_path));

    // Step 3: Launch nsjail sandbox with monitoring
    auto sandbox_process = TRY(launch_nsjail_sandbox(target_path, timeout));

    // Step 4: Monitor execution via syscall events
    auto monitoring_result = TRY(monitor_sandbox_execution(
        sandbox_process.pid,
        sandbox_process.syscall_pipe_fd,
        timeout
    ));

    // Step 5: Wait for sandbox completion or timeout
    auto exit_status = TRY(wait_for_sandbox_completion(
        sandbox_process.pid,
        timeout
    ));

    // Step 6: Parse syscall events into metrics
    TRY(parse_syscall_events_into_metrics(monitoring_result, metrics));

    // Step 7: Cleanup
    TRY(cleanup_temp_directory(temp_dir));

    metrics.exit_code = exit_status;
    metrics.timed_out = (exit_status == -1);  // -1 = killed on timeout

    return metrics;
}
```

### Syscall Monitoring Core

```cpp
struct SyscallEvent {
    u64 timestamp_ns;
    i32 syscall_number;
    ByteString syscall_name;
    Vector<u64> args;       // Up to 6 arguments
    i64 return_value;
};

ErrorOr<Vector<SyscallEvent>> BehavioralAnalyzer::monitor_sandbox_execution(
    pid_t sandbox_pid,
    int syscall_pipe_fd,
    Duration timeout)
{
    Vector<SyscallEvent> events;

    // Set alarm for timeout enforcement
    alarm(timeout.to_seconds());

    // Monitor using seccomp user notifications (if available)
    if (seccomp_unotify_available()) {
        TRY(monitor_via_seccomp_unotify(sandbox_pid, syscall_pipe_fd, events));
    } else {
        // Fallback: ptrace-based monitoring
        TRY(monitor_via_ptrace(sandbox_pid, events));
    }

    // Cancel alarm
    alarm(0);

    return events;
}

ErrorOr<void> BehavioralAnalyzer::monitor_via_seccomp_unotify(
    pid_t sandbox_pid,
    int notification_fd,
    Vector<SyscallEvent>& events)
{
    // Read seccomp user notifications from kernel
    while (true) {
        struct seccomp_notif notif;
        struct seccomp_notif_resp resp;

        // Receive notification from kernel
        if (ioctl(notification_fd, SECCOMP_IOCTL_NOTIF_RECV, &notif) < 0) {
            if (errno == EINTR)
                continue;  // Interrupted by alarm
            break;  // No more notifications
        }

        // Parse syscall details
        SyscallEvent event;
        event.timestamp_ns = MonotonicTime::now().nanoseconds();
        event.syscall_number = notif.data.nr;
        event.syscall_name = TRY(syscall_number_to_name(notif.data.nr));

        for (size_t i = 0; i < 6; ++i)
            TRY(event.args.try_append(notif.data.args[i]));

        TRY(events.try_append(move(event)));

        // Send response (allow syscall to continue)
        resp.id = notif.id;
        resp.error = 0;
        resp.val = 0;
        resp.flags = 0;

        ioctl(notification_fd, SECCOMP_IOCTL_NOTIF_SEND, &resp);
    }

    return {};
}

ErrorOr<void> BehavioralAnalyzer::monitor_via_ptrace(
    pid_t sandbox_pid,
    Vector<SyscallEvent>& events)
{
    // Attach to sandbox process with ptrace
    TRY(Core::System::ptrace(PTRACE_SEIZE, sandbox_pid, nullptr, nullptr));

    // Interrupt each syscall entry and exit
    TRY(Core::System::ptrace(PTRACE_SETOPTIONS, sandbox_pid, nullptr,
        PTRACE_O_TRACESYSGOOD));

    while (true) {
        // Wait for syscall entry
        int status;
        pid_t result = waitpid(sandbox_pid, &status, 0);

        if (result < 0 || WIFEXITED(status) || WIFSIGNALED(status))
            break;  // Process exited

        if (WIFSTOPPED(status) && (status >> 8) == (SIGTRAP | 0x80)) {
            // Syscall entry
            SyscallEvent event;
            event.timestamp_ns = MonotonicTime::now().nanoseconds();

            // Read syscall number from registers
            struct user_regs_struct regs;
            TRY(Core::System::ptrace(PTRACE_GETREGS, sandbox_pid, nullptr, &regs));

            event.syscall_number = regs.orig_rax;  // x86_64
            event.syscall_name = TRY(syscall_number_to_name(event.syscall_number));

            // Read arguments (rdi, rsi, rdx, r10, r8, r9 on x86_64)
            TRY(event.args.try_append(regs.rdi));
            TRY(event.args.try_append(regs.rsi));
            TRY(event.args.try_append(regs.rdx));
            TRY(event.args.try_append(regs.r10));
            TRY(event.args.try_append(regs.r8));
            TRY(event.args.try_append(regs.r9));

            TRY(events.try_append(move(event)));
        }

        // Continue execution
        TRY(Core::System::ptrace(PTRACE_SYSCALL, sandbox_pid, nullptr, nullptr));
    }

    return {};
}
```

### Event Parsing to Metrics

```cpp
ErrorOr<void> BehavioralAnalyzer::parse_syscall_events_into_metrics(
    Vector<SyscallEvent> const& events,
    BehavioralMetrics& metrics)
{
    // Track file descriptors for network detection
    HashMap<int, SocketInfo> socket_fds;

    // Track allocations for heap spray detection
    HashMap<size_t, u32> allocation_sizes;  // size -> count

    for (auto const& event : events) {
        auto const& syscall = event.syscall_name;

        // File operations
        if (syscall == "open"sv || syscall == "openat"sv) {
            metrics.file_operations++;

            // Check for sensitive directory access
            // (would need to read filename from tracee memory)
        }
        else if (syscall == "write"sv || syscall == "writev"sv) {
            metrics.file_operations++;
            // TODO: Calculate entropy of written data
        }
        else if (syscall == "unlink"sv || syscall == "unlinkat"sv) {
            metrics.file_operations++;
        }

        // Process operations
        else if (syscall == "fork"sv || syscall == "clone"sv) {
            metrics.process_operations++;
        }
        else if (syscall == "execve"sv) {
            metrics.process_operations++;
        }
        else if (syscall == "ptrace"sv) {
            metrics.self_modification_attempts++;
        }

        // Network operations
        else if (syscall == "socket"sv) {
            metrics.network_operations++;
            // Track socket FD (event.return_value)
        }
        else if (syscall == "connect"sv) {
            metrics.network_operations++;
            metrics.outbound_connections++;
        }
        else if (syscall == "sendto"sv || syscall == "send"sv) {
            metrics.network_operations++;
        }

        // Memory operations
        else if (syscall == "mmap"sv) {
            metrics.memory_operations++;

            // Track allocation size for heap spray detection
            size_t size = event.args[1];  // length argument
            allocation_sizes.set(size, allocation_sizes.get(size).value_or(0) + 1);
        }
        else if (syscall == "mprotect"sv) {
            metrics.memory_operations++;

            // Check for RWX pages (PROT_READ | PROT_WRITE | PROT_EXEC)
            int prot = event.args[2];
            if ((prot & 0x7) == 0x7) {  // RWX
                metrics.code_injection_attempts++;
            }
        }
    }

    // Detect heap spray pattern
    for (auto const& [size, count] : allocation_sizes) {
        if (count > 100 && size > 0x20000) {
            // Same-size allocation repeated >100 times
            dbgln("Heap spray detected: {} allocations of {} bytes", count, size);
            // (already counted in memory_operations)
        }
    }

    return {};
}
```

---

## Integration Checklist

### Phase 1: Preparation (Week 1)

- [ ] Install nsjail on development system
  ```bash
  sudo apt-get install nsjail
  ```
- [ ] Test nsjail manually with sample executable
  ```bash
  nsjail --mode o --time_limit 5 -- /bin/ls
  ```
- [ ] Create seccomp-BPF policy file
  - [ ] Define allow list (basic syscalls)
  - [ ] Define monitor list (suspicious syscalls)
  - [ ] Define block list (dangerous syscalls)
- [ ] Design syscall monitoring architecture
  - [ ] Research seccomp user notifications
  - [ ] Design ptrace fallback
  - [ ] Plan IPC mechanism (pipes vs shared memory)

### Phase 2: Implementation (Weeks 2-3)

- [ ] Implement temp directory creation
  - [ ] Use mkdtemp for atomic creation
  - [ ] Cleanup on error paths
- [ ] Implement nsjail launcher
  - [ ] Build command line arguments
  - [ ] Fork-exec with proper error handling
  - [ ] Pass IPC file descriptors
- [ ] Implement syscall monitoring
  - [ ] Try seccomp user notifications first
  - [ ] Fallback to ptrace if unavailable
  - [ ] Parse syscall events into SyscallEvent structs
- [ ] Implement metrics parser
  - [ ] Map syscalls to BehavioralMetrics fields
  - [ ] Track file operations, process ops, network ops
  - [ ] Calculate entropy for written data (optional)
- [ ] Implement scoring algorithm
  - [ ] calculate_threat_score() with category weights
  - [ ] generate_suspicious_behaviors() with human descriptions
- [ ] Implement timeout enforcement
  - [ ] alarm() for parent process timeout
  - [ ] SIGKILL to child if exceeded
  - [ ] Proper cleanup on timeout

### Phase 3: Testing (Week 4)

- [ ] Unit tests for individual methods
  - [ ] Test temp directory creation/cleanup
  - [ ] Test nsjail command generation
  - [ ] Test syscall event parsing
  - [ ] Test scoring algorithm
- [ ] Integration tests
  - [ ] Test benign executable (e.g., /bin/ls)
  - [ ] Test simple malware simulator (rapid file writes)
  - [ ] Test timeout enforcement
  - [ ] Test resource limits (memory, CPU)
- [ ] Platform compatibility tests
  - [ ] Verify graceful degradation on macOS
  - [ ] Verify graceful degradation on Windows
  - [ ] Document platform-specific behavior

### Phase 4: Orchestrator Integration (Week 4)

- [ ] Verify interface compatibility
  - [ ] Confirm BehavioralMetrics structure unchanged
  - [ ] Confirm analyze() signature unchanged
- [ ] Test with Orchestrator
  - [ ] Tier 2 execution path
  - [ ] Result aggregation
  - [ ] Verdict generation
- [ ] Performance benchmarks
  - [ ] Measure analysis time for various file sizes
  - [ ] Verify <5 second target met
  - [ ] Profile memory usage

### Phase 5: Documentation (Week 5)

- [ ] Update BEHAVIORAL_ANALYSIS_SPEC.md
  - [ ] Add Linux implementation details
  - [ ] Document platform differences
- [ ] Create user-facing documentation
  - [ ] Explain behavioral analysis to end users
  - [ ] Document security benefits
- [ ] Create developer documentation
  - [ ] nsjail setup instructions
  - [ ] seccomp-BPF policy customization
  - [ ] Debugging tips

---

## Security Considerations

### Sandbox Escape Prevention

**Risk**: Malware escapes nsjail sandbox and compromises host system.

**Mitigations**:

1. **Kernel Namespace Isolation**
   - Network namespace: Prevent network access
   - PID namespace: Isolate process tree
   - Mount namespace: Read-only root filesystem
   - User namespace: Non-root execution

2. **seccomp-BPF Syscall Filtering**
   - Block dangerous syscalls (mount, reboot, ptrace)
   - Monitor suspicious syscalls (fork, exec, socket)
   - Allow basic syscalls (read, write, exit)

3. **Resource Limits (rlimit)**
   - CPU time: 5 seconds max
   - Memory: 128MB max
   - File size: 10MB writes max
   - File descriptors: 64 max

4. **Filesystem Restrictions**
   - Chroot to isolated directory
   - Read-only binaries (/bin, /lib)
   - Writable /tmp (isolated)
   - No access to user home directories

### Timeout Enforcement

**Risk**: Malware hangs or loops forever, consuming resources.

**Mitigations**:

1. **Parent Process Alarm**
   ```cpp
   alarm(timeout.to_seconds());  // SIGALRM after timeout
   ```

2. **nsjail Built-in Timeout**
   ```bash
   nsjail --time_limit 5  # Kills after 5 seconds
   ```

3. **Resource Limit (rlimit)**
   ```bash
   nsjail --rlimit_cpu 5  # CPU time limit
   ```

### Information Leakage

**Risk**: Malware detects sandbox and behaves benignly.

**Mitigations**:

1. **Avoid Obvious Indicators**
   - Don't name temp directories "sandbox" or "jail"
   - Use random filenames (mktemp)
   - Don't expose /proc/self/status (shows seccomp state)

2. **Timing Attacks**
   - Minimize analysis time variation
   - Use consistent timeout (5 seconds)

3. **Side Channels**
   - Limit network access (no outbound connections)
   - No GPU access (no side-channel timing)

### Privilege Escalation

**Risk**: Malware exploits kernel vulnerability to escape.

**Mitigations**:

1. **Non-Root Execution**
   - Run nsjail as non-root user
   - Use user namespaces for fake root

2. **Kernel Hardening**
   - Require Linux kernel 5.4+ (seccomp_unotify support)
   - Enable SELinux/AppArmor (optional)

3. **Minimal Attack Surface**
   - Block ptrace (prevent debugger attachment)
   - Block module loading (no kernel modules)
   - Block mount operations (no filesystem manipulation)

---

## Performance Optimization

### Execution Time Targets

| Metric | Target | Realistic |
|--------|--------|-----------|
| Temp directory creation | < 10ms | ✅ Yes |
| File write | < 50ms | ✅ Yes |
| nsjail startup | < 200ms | ✅ Yes |
| Syscall monitoring | < 2s | ✅ Yes |
| Score calculation | < 10ms | ✅ Yes |
| **Total** | **< 3s** | **✅ Achievable** |

### Optimization Strategies

1. **Early Termination**
   ```cpp
   // If obvious malware detected in first 1 second, stop monitoring
   if (metrics.code_injection_attempts > 5 && elapsed_time < 1s) {
       kill(sandbox_pid, SIGKILL);
       return metrics;  // High confidence = malicious
   }
   ```

2. **Parallel Analysis**
   - Monitor syscalls in separate thread
   - Calculate scores while monitoring continues
   - Reduce total wall-clock time

3. **Caching**
   - Cache benign file hashes in PolicyGraph
   - Skip analysis if previously analyzed
   - Reduces redundant sandbox execution

4. **Selective Monitoring**
   - Monitor only high-risk syscalls (fork, exec, socket)
   - Skip monitoring for benign syscalls (getpid, clock_gettime)
   - Reduces overhead

---

## Next Steps

### Immediate Actions

1. **Install Dependencies**
   ```bash
   sudo apt-get install nsjail libseccomp-dev
   ```

2. **Create Test Harness**
   - Write `Services/Sentinel/Test/TestBehavioralAnalyzer.cpp`
   - Test benign executables (/bin/ls, /bin/echo)
   - Test malware simulators (rapid file writes)

3. **Implement Core Methods**
   - `create_temp_sandbox_directory()`
   - `write_file_to_sandbox()`
   - `launch_nsjail_sandbox()`

4. **Integrate with Orchestrator**
   - Update `Orchestrator::execute_tier2_native()`
   - Test end-to-end flow
   - Benchmark performance

### Future Enhancements (Post-Phase 2)

1. **Advanced Syscall Analysis**
   - Parse syscall arguments (file paths, socket addresses)
   - Calculate entropy of written data
   - Detect specific malware families (ransomware, stealers)

2. **Machine Learning Integration**
   - Use ML model for behavioral classification
   - Combine with existing MalwareML scores
   - Improve zero-day detection

3. **Cross-Platform Support**
   - Implement macOS sandbox (sandbox-exec)
   - Implement Windows sandbox (Windows Sandbox API)
   - Unified interface across platforms

4. **Performance Tuning**
   - Profile syscall monitoring overhead
   - Optimize seccomp-BPF policy
   - Reduce analysis time to <1 second (stretch goal)

---

## Document References

- **Behavioral Analysis Spec**: `docs/BEHAVIORAL_ANALYSIS_SPEC.md`
- **Sandbox Technology Comparison**: `docs/SANDBOX_TECHNOLOGY_COMPARISON.md`
- **Sandbox Recommendation**: `docs/SANDBOX_RECOMMENDATION_SUMMARY.md`
- **Milestone Plan**: `docs/MILESTONE_0.5_PLAN.md`
- **Orchestrator Implementation**: `Services/Sentinel/Sandbox/Orchestrator.{h,cpp}`

---

**Document Version**: 1.0
**Created**: 2025-11-02
**Status**: Design Complete - Ready for Implementation
**Next Milestone**: Phase 2 Implementation (4-5 weeks)
