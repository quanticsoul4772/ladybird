# Milestone 0.5 Phase 2: Behavioral Analysis with nsjail

**Version**: 1.0
**Date**: 2025-11-02
**Status**: Architecture Design - Ready for Implementation
**Prerequisites**: Phase 1b (Wasmtime Integration) Complete ✅
**Target Timeline**: 2-3 weeks
**Platform**: Linux (primary), graceful degradation for macOS/Windows

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Phase 2 Goals and Scope](#phase-2-goals-and-scope)
3. [Architecture Overview](#architecture-overview)
4. [nsjail Configuration](#nsjail-configuration)
5. [Syscall Monitoring Strategy](#syscall-monitoring-strategy)
6. [Behavioral Pattern Detection](#behavioral-pattern-detection)
7. [Integration with Orchestrator](#integration-with-orchestrator)
8. [Platform Support Strategy](#platform-support-strategy)
9. [Performance Optimization](#performance-optimization)
10. [Testing Strategy](#testing-strategy)
11. [Security Considerations](#security-considerations)
12. [Implementation Plan](#implementation-plan)
13. [Appendix](#appendix)

---

## Executive Summary

### Overview

Phase 2 implements **real behavioral analysis** using nsjail sandboxing to execute suspicious files and monitor their runtime behavior. This complements Phase 1's WASM-based static analysis with dynamic execution monitoring, enabling detection of zero-day malware through behavioral patterns.

### Key Objectives

1. **Execute Suspicious Files Safely**: Run files in isolated nsjail sandbox with full syscall monitoring
2. **Detect Behavioral Patterns**: Identify ransomware, info-stealers, cryptominers through runtime behavior
3. **Complete Tier 2 Implementation**: Replace BehavioralAnalyzer mock with real nsjail execution
4. **Maintain Performance**: Target 1-5 seconds per file analysis (95th percentile < 5s)
5. **Ensure Security**: Prevent sandbox escapes with defense-in-depth approach

### Decision Summary

| **Component** | **Technology** | **Rationale** |
|---------------|----------------|---------------|
| **Sandbox Technology** | nsjail + seccomp-BPF | Google-backed, battle-tested, Linux namespaces |
| **Syscall Monitoring** | ptrace + seccomp-BPF | Hybrid approach for complete visibility |
| **Timeout Enforcement** | SIGALRM + cgroups | Process-level timeout with resource limits |
| **File I/O Isolation** | tmpfs + mount namespaces | Ephemeral, isolated filesystem per execution |
| **Network Isolation** | Network namespace | Completely isolated network stack |

### Performance Targets

| **Metric** | **Target** | **P95 (Acceptable)** |
|------------|-----------|---------------------|
| Total Analysis Time | 1-3 seconds | < 5 seconds |
| Sandbox Initialization | 100-200ms | < 500ms |
| Behavioral Monitoring | 1-2 seconds | < 4 seconds |
| Result Aggregation | 100-200ms | < 500ms |
| Memory Footprint | 50-100MB | < 200MB |

### Success Criteria

- ✅ Detect 90%+ of malware samples through behavioral analysis
- ✅ False positive rate < 5% on legitimate software
- ✅ Zero sandbox escapes in testing
- ✅ Performance within 5-second budget (95th percentile)
- ✅ Graceful degradation on non-Linux platforms

---

## Phase 2 Goals and Scope

### In Scope

**Core Functionality**:
- Real nsjail sandbox execution for PE/ELF binaries
- Syscall monitoring via ptrace + seccomp-BPF
- Behavioral pattern detection (16 metrics across 5 categories)
- Integration with existing Orchestrator and VerdictEngine
- PolicyGraph caching for behavioral verdicts

**Behavioral Metrics** (from BEHAVIORAL_ANALYSIS_SPEC.md):
1. File I/O: Modification rate, entropy, sensitive directory access
2. Process Behavior: Process chains, injection attempts, suspicious subprocesses
3. Memory Manipulation: Heap spraying, executable memory writes
4. Registry Operations: Windows persistence, credential theft (future)
5. Platform-Specific: Linux privilege escalation, cron/systemd persistence

**Platform Support**:
- Linux: Full nsjail implementation (primary target)
- macOS/Windows: Graceful fallback to Phase 1 (WASM-only)

### Out of Scope (Deferred)

**Deferred to Future Milestones**:
- Windows registry monitoring (requires Windows-specific sandbox)
- Network behavioral analysis (Phase 6 - C2Detector, DNSAnalyzer)
- GPU/WebGL fingerprinting detection (Milestone 0.4 Phase 4 continuation)
- Real-time process injection into running applications
- Multi-hour behavioral analysis (budget: 5 seconds max)

**Explicitly Not Included**:
- Browser-specific behaviors (handled by FormMonitor, FingerprintingDetector)
- TLS/SSL certificate validation (RequestServer responsibility)
- User-facing UI alerts (Phase 1d responsibility)

---

## Architecture Overview

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                        Orchestrator                              │
│  (Coordinates YARA + ML + Behavioral analysis)                   │
└─────────────────────┬───────────────────────────────────────────┘
                      │
        ┌─────────────┴─────────────┐
        │                           │
        ▼                           ▼
┌───────────────┐         ┌─────────────────────┐
│ WasmExecutor  │         │ BehavioralAnalyzer  │ ← PHASE 2 FOCUS
│  (Phase 1b)   │         │   (Phase 2)         │
│               │         │                     │
│ - WASM Module │         │ - nsjail sandbox    │
│ - Static      │         │ - Syscall monitor   │
│   Analysis    │         │ - Behavioral        │
│ - Fast Triage │         │   pattern detection │
└───────────────┘         └──────────┬──────────┘
                                     │
                          ┌──────────┴──────────┐
                          │                     │
                          ▼                     ▼
                  ┌───────────────┐    ┌────────────────┐
                  │  nsjail CLI   │    │ SyscallTracer  │
                  │  (Process)    │    │ (ptrace/BPF)   │
                  │               │    │                │
                  │ - Isolation   │    │ - Monitor      │
                  │ - Limits      │    │ - Filter       │
                  │ - Timeout     │    │ - Aggregate    │
                  └───────────────┘    └────────────────┘
```

### Data Flow Diagram

```
File Download → RequestServer
                     ↓
              [SecurityTap]
                     ↓
              YARA Scan (existing)
                     ↓
           ┌─────────┴─────────┐
           │   No Match        │ Match → BLOCK
           ▼                   │
    ┌─────────────┐           │
    │ Orchestrator │←──────────┘
    └──────┬───────┘
           │
    ┌──────┴────────────────────────┐
    │                               │
    ▼                               ▼
[Phase 1: WASM]              [PolicyGraph Cache]
WasmExecutor                       ↓
    │                         Cached Verdict?
    ├─ Clean (score < 0.3)         │
    │  → ALLOW                     YES → Return
    │                               │
    ├─ Suspicious (0.3-0.7)        NO
    │  → Continue to Phase 2        ↓
    │                          [Phase 2: nsjail]
    └─ Malicious (> 0.7)      BehavioralAnalyzer
       → BLOCK                      ↓
                               ┌────┴────┐
                               │ nsjail  │
                               │ Sandbox │
                               └────┬────┘
                                    │
                     ┌──────────────┼──────────────┐
                     │              │              │
                     ▼              ▼              ▼
               File I/O       Process       Memory
               Monitor        Monitor       Monitor
                     │              │              │
                     └──────────────┴──────────────┘
                                    ↓
                          BehavioralMetrics
                                    ↓
                          VerdictEngine
                          (Score Calculation)
                                    ↓
                          ┌─────────┴─────────┐
                          │ BENIGN            │
                          │ SUSPICIOUS        │
                          │ MALICIOUS         │
                          └───────────────────┘
                                    ↓
                          PolicyGraph Storage
                                    ↓
                          User Notification
```

### Class Diagram

```cpp
// Phase 2 Architecture - Key Classes

┌─────────────────────────────────────────────────────────┐
│               BehavioralAnalyzer                        │
│  (Services/Sentinel/Sandbox/BehavioralAnalyzer.cpp)     │
├─────────────────────────────────────────────────────────┤
│ + create(config) → NonnullOwnPtr<Self>                  │
│ + analyze(file_data, filename, timeout) → Metrics       │
│ + get_statistics() → Statistics                         │
├─────────────────────────────────────────────────────────┤
│ - analyze_nsjail(file_data, filename) → Metrics        │
│ - launch_nsjail_sandbox(file_path) → pid_t             │
│ - monitor_sandbox_execution(pid) → Metrics             │
│ - parse_syscall_trace() → Metrics                      │
│ - calculate_threat_score(metrics) → float              │
│                                                          │
│ - m_sandbox_dir: String                                 │
│ - m_nsjail_path: String                                 │
│ - m_syscall_tracer: OwnPtr<SyscallTracer>              │
│ - m_config: SandboxConfig                               │
└─────────────────────────────────────────────────────────┘
                          │
                          │ uses
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  SyscallTracer                          │
│  (Services/Sentinel/Sandbox/SyscallTracer.{h,cpp})      │
│                   [NEW FILE]                            │
├─────────────────────────────────────────────────────────┤
│ + create() → NonnullOwnPtr<Self>                        │
│ + attach(pid) → ErrorOr<void>                           │
│ + monitor(timeout) → ErrorOr<Vector<SyscallEvent>>     │
│ + detach() → ErrorOr<void>                              │
├─────────────────────────────────────────────────────────┤
│ - handle_syscall_entry(pid, regs) → void               │
│ - handle_syscall_exit(pid, regs) → void                │
│ - classify_syscall(nr) → SyscallCategory               │
│ - collect_syscall_args(pid, nr) → SyscallArgs          │
│                                                          │
│ - m_traced_pid: pid_t                                   │
│ - m_syscall_events: Vector<SyscallEvent>               │
│ - m_syscall_counts: HashMap<int, u32>                  │
└─────────────────────────────────────────────────────────┘
                          │
                          │ produces
                          ▼
┌─────────────────────────────────────────────────────────┐
│              BehavioralMetrics                          │
│  (Services/Sentinel/Sandbox/BehavioralAnalyzer.h)       │
├─────────────────────────────────────────────────────────┤
│ struct BehavioralMetrics {                              │
│   // File I/O (4 metrics)                               │
│   u32 file_operations;                                  │
│   u32 temp_file_creates;                                │
│   u32 hidden_file_creates;                              │
│   u32 executable_drops;                                 │
│                                                          │
│   // Process (3 metrics)                                │
│   u32 process_operations;                               │
│   u32 self_modification_attempts;                       │
│   u32 persistence_mechanisms;                           │
│                                                          │
│   // Network (4 metrics)                                │
│   u32 network_operations;                               │
│   u32 outbound_connections;                             │
│   u32 dns_queries;                                      │
│   u32 http_requests;                                    │
│                                                          │
│   // System/Registry (3 metrics)                        │
│   u32 registry_operations;  // Windows future           │
│   u32 service_modifications;                            │
│   u32 privilege_escalation_attempts;                    │
│                                                          │
│   // Memory (2 metrics)                                 │
│   u32 memory_operations;                                │
│   u32 code_injection_attempts;                          │
│                                                          │
│   // Aggregated results                                 │
│   float threat_score;                                   │
│   Vector<String> suspicious_behaviors;                  │
│   Duration execution_time;                              │
│   bool timed_out;                                       │
│   i32 exit_code;                                        │
│ }                                                        │
└─────────────────────────────────────────────────────────┘
```

### State Machine

```
BehavioralAnalyzer State Transitions:

     [Created]
         │
         │ create()
         ▼
  [Initializing]
         │
         │ initialize_sandbox()
         │ - Create temp directory
         │ - Verify nsjail available
         │ - Setup seccomp filter
         ▼
    [Ready]
         │
         │ analyze(file_data, filename, timeout)
         ▼
  [Analyzing]
         │
         ├─ Write file to sandbox
         │  └─ /tmp/sentinel-sandbox-XXXXXX/target_file
         │
         ├─ Launch nsjail
         │  └─ nsjail --config /tmp/policy.cfg -- /target_file
         │
         ├─ Attach ptrace
         │  └─ SyscallTracer::attach(nsjail_pid)
         │
         ├─ Monitor execution (1-5 seconds)
         │  └─ Collect syscall events
         │  └─ Update BehavioralMetrics
         │
         ├─ Timeout handling
         │  └─ SIGALRM → Kill sandbox
         │
         └─ Cleanup
            └─ Detach ptrace
            └─ Wait for process exit
            └─ Delete temp files
         ▼
   [Complete]
         │
         │ Return BehavioralMetrics
         ▼
    [Ready]  (Ready for next analysis)
```

---

## nsjail Configuration

### Overview

nsjail is Google's sandbox utility used in Chrome OS and Google Cloud. It provides:
- **Linux namespaces**: Process, mount, network, IPC, UTS, user isolation
- **seccomp-BPF**: Syscall filtering and monitoring
- **Resource limits**: CPU, memory, file descriptors via cgroups
- **Bind mounts**: Controlled filesystem access
- **Time limits**: Automatic process termination

### Installation

```bash
# Ubuntu/Debian
sudo apt-get install nsjail

# Build from source (recommended for latest version)
git clone https://github.com/google/nsjail.git
cd nsjail
make
sudo cp nsjail /usr/local/bin/
```

### Configuration File Format

nsjail uses protobuf-based configuration. For Sentinel, we'll use CLI arguments for flexibility.

### Sentinel nsjail Invocation

```cpp
// BehavioralAnalyzer::launch_nsjail_sandbox()

ErrorOr<pid_t> BehavioralAnalyzer::launch_nsjail_sandbox(String const& file_path)
{
    // Build nsjail command
    Vector<String> args = {
        "/usr/bin/nsjail"_string,

        // Execution mode
        "--mode"_string, "o"_string,  // Once mode (single execution)

        // Namespaces (maximum isolation)
        "--clone_newnet"_string,      // Network namespace (no external network)
        "--clone_newuser"_string,     // User namespace (run as nobody)
        "--clone_newns"_string,       // Mount namespace (isolated filesystem)
        "--clone_newpid"_string,      // PID namespace (isolated process tree)
        "--clone_newipc"_string,      // IPC namespace (no shared memory)
        "--clone_newuts"_string,      // UTS namespace (isolated hostname)

        // User/Group mapping (run as unprivileged)
        "--uid_mapping"_string, "65534:65534:1"_string,  // Map to 'nobody'
        "--gid_mapping"_string, "65534:65534:1"_string,

        // Resource limits (prevent resource exhaustion)
        "--rlimit_as"_string, "256"_string,        // 256MB address space
        "--rlimit_cpu"_string, "5"_string,         // 5 seconds CPU time
        "--rlimit_fsize"_string, "10"_string,      // 10MB file writes
        "--rlimit_nofile"_string, "50"_string,     // 50 file descriptors
        "--rlimit_nproc"_string, "10"_string,      // 10 processes max

        // Time limit (enforce timeout)
        "--time_limit"_string, "5"_string,         // 5 seconds wall time

        // Filesystem (minimal read-only mounts)
        "--chroot"_string, m_sandbox_dir,          // Chroot to sandbox
        "--bindmount_ro"_string, "/lib"_string,    // Read-only libc
        "--bindmount_ro"_string, "/lib64"_string,  // Read-only lib64
        "--bindmount"_string, TRY(String::formatted("{}:/tmp", m_sandbox_dir)),  // RW /tmp

        // seccomp-BPF policy (allow basic syscalls, monitor suspicious)
        "--seccomp_string"_string,
        TRY(generate_seccomp_policy()),            // Custom policy

        // Logging (disable for performance)
        "--quiet"_string,
        "--disable_proc"_string,                   // No /proc mount

        // Program to execute
        "--"_string,
        file_path
    };

    // Fork + exec nsjail
    auto pid = TRY(Core::System::fork());

    if (pid == 0) {
        // Child: Execute nsjail
        Vector<char const*> argv;
        for (auto const& arg : args)
            argv.append(arg.bytes_as_string_view().characters_without_null_termination());
        argv.append(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(1);  // exec failed
    }

    // Parent: Return nsjail PID for monitoring
    return pid;
}
```

### seccomp-BPF Policy

```cpp
ErrorOr<String> BehavioralAnalyzer::generate_seccomp_policy()
{
    // seccomp-BPF policy in nsjail syntax
    // Format: POLICY sys1 sys2 sys3 ...

    StringBuilder policy;

    // ALLOW: Basic syscalls required for execution
    policy.append("ALLOW "sv);
    policy.append("read write open close stat fstat lstat "sv);
    policy.append("brk mmap munmap mprotect "sv);
    policy.append("getpid getuid getgid geteuid getegid "sv);
    policy.append("exit exit_group "sv);

    // ERRNO: Syscalls that should fail gracefully (return -EPERM)
    policy.append("ERRNO(1) ");  // Return EPERM
    policy.append("socket connect bind listen accept "sv);  // Network (deny)
    policy.append("ptrace "sv);                              // Debugging (deny)
    policy.append("kill "sv);                                // Kill other processes (deny)

    // LOG: Suspicious syscalls to monitor (allow but log)
    policy.append("LOG ");
    policy.append("execve fork clone vfork "sv);             // Process creation
    policy.append("chmod chown "sv);                         // Permission changes
    policy.append("unlink rmdir "sv);                        // File deletion
    policy.append("mknod "sv);                               // Device creation

    // KILL: Syscalls that trigger immediate termination
    policy.append("KILL ");
    policy.append("reboot mount umount "sv);                 // System modification
    policy.append("swapon swapoff "sv);                      // Swap manipulation
    policy.append("settimeofday "sv);                        // Time manipulation

    return policy.to_string();
}
```

### nsjail Configuration Example

For reference, equivalent nsjail config file (not used in implementation):

```protobuf
# sentinel-nsjail.cfg

name: "sentinel-sandbox"
description: "Behavioral analysis sandbox for malware detection"

mode: ONCE

clone_newnet: true
clone_newuser: true
clone_newns: true
clone_newpid: true
clone_newipc: true
clone_newuts: true

uid_mapping {
  inside_id: "65534"  # nobody
  outside_id: "65534"
  count: 1
}

gid_mapping {
  inside_id: "65534"
  outside_id: "65534"
  count: 1
}

rlimit_as: 256       # 256MB memory
rlimit_cpu: 5        # 5 seconds CPU
rlimit_fsize: 10     # 10MB file writes
rlimit_nofile: 50    # 50 file descriptors
rlimit_nproc: 10     # 10 processes

time_limit: 5        # 5 seconds wall time

mount {
  src: "/lib"
  dst: "/lib"
  is_bind: true
  rw: false
}

mount {
  src: "/lib64"
  dst: "/lib64"
  is_bind: true
  rw: false
}

mount {
  src: "/tmp/sentinel-sandbox"
  dst: "/tmp"
  is_bind: true
  rw: true
}

seccomp_string: "ALLOW read write open close ..."
```

---

## Syscall Monitoring Strategy

### Approach: Hybrid ptrace + seccomp-BPF

**Why Hybrid?**
- **ptrace**: Complete visibility into syscall arguments and return values
- **seccomp-BPF**: Syscall filtering and enforcement (faster, kernel-level)
- **Combination**: Best of both worlds - enforcement + monitoring

### ptrace Implementation

```cpp
// Services/Sentinel/Sandbox/SyscallTracer.h

namespace Sentinel::Sandbox {

enum class SyscallCategory {
    FileIO,              // open, read, write, close, unlink
    ProcessControl,      // fork, clone, execve, kill
    MemoryManagement,    // mmap, mprotect, munmap, brk
    Network,             // socket, connect, bind, send, recv
    IPC,                 // pipe, shm_open, msgget
    System,              // reboot, mount, swapon (dangerous)
    PermissionChange,    // chmod, chown, setuid, setgid
    Unknown
};

struct SyscallEvent {
    u64 timestamp_ns;           // Monotonic timestamp
    pid_t pid;                  // Process ID
    int syscall_number;         // Syscall number (arch-specific)
    SyscallCategory category;   // Classified category

    // Arguments (6 max on x86-64)
    u64 args[6];

    // Return value
    i64 retval;

    // Parsed context (if applicable)
    Optional<String> file_path;        // For open, stat, etc.
    Optional<String> command_line;     // For execve
    Optional<u32> signal_number;       // For kill
    Optional<u64> memory_address;      // For mmap, mprotect

    // Flags
    bool suspicious { false };         // Marked as suspicious
};

class SyscallTracer {
public:
    static ErrorOr<NonnullOwnPtr<SyscallTracer>> create();

    // Attach to process and begin tracing
    ErrorOr<void> attach(pid_t pid);

    // Monitor execution until timeout or process exit
    // Returns collected syscall events
    ErrorOr<Vector<SyscallEvent>> monitor(Duration timeout);

    // Detach from process
    ErrorOr<void> detach();

    // Statistics
    struct Statistics {
        u64 total_syscalls { 0 };
        u64 file_io_syscalls { 0 };
        u64 process_syscalls { 0 };
        u64 memory_syscalls { 0 };
        u64 network_syscalls { 0 };
        u64 suspicious_syscalls { 0 };
    };

    Statistics get_statistics() const { return m_stats; }

private:
    explicit SyscallTracer();

    // ptrace event loop
    ErrorOr<void> trace_loop(Duration timeout);

    // Syscall handling
    ErrorOr<void> handle_syscall_entry(pid_t pid);
    ErrorOr<void> handle_syscall_exit(pid_t pid);

    // Syscall parsing
    SyscallCategory classify_syscall(int nr);
    ErrorOr<SyscallEvent> parse_syscall(pid_t pid, int nr, u64 args[6]);
    ErrorOr<String> read_string_from_tracee(pid_t pid, u64 addr);

    // Architecture-specific syscall tables
    static HashMap<int, StringView> const& syscall_table();

    pid_t m_traced_pid { -1 };
    Vector<SyscallEvent> m_syscall_events;
    Statistics m_stats;

    // Syscall state tracking
    struct SyscallState {
        int syscall_number;
        u64 args[6];
        u64 entry_timestamp_ns;
    };
    HashMap<pid_t, SyscallState> m_in_flight_syscalls;
};

}
```

### SyscallTracer Implementation

```cpp
// Services/Sentinel/Sandbox/SyscallTracer.cpp

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <linux/ptrace.h>

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<SyscallTracer>> SyscallTracer::create()
{
    return adopt_nonnull_own(new SyscallTracer());
}

SyscallTracer::SyscallTracer() = default;

ErrorOr<void> SyscallTracer::attach(pid_t pid)
{
    m_traced_pid = pid;

    // Attach to process with PTRACE_ATTACH
    if (ptrace(PTRACE_SEIZE, pid, nullptr, PTRACE_O_TRACESYSGOOD) < 0)
        return Error::from_syscall("ptrace SEIZE"sv, errno);

    // Wait for process to stop
    int status;
    if (waitpid(pid, &status, 0) < 0)
        return Error::from_syscall("waitpid"sv, errno);

    // Set ptrace options to trace syscalls
    unsigned long options = PTRACE_O_TRACESYSGOOD |      // Set high bit on syscall stops
                            PTRACE_O_TRACEFORK |          // Trace forks
                            PTRACE_O_TRACECLONE |         // Trace clones
                            PTRACE_O_TRACEEXEC;           // Trace execs

    if (ptrace(PTRACE_SETOPTIONS, pid, nullptr, options) < 0)
        return Error::from_syscall("ptrace SETOPTIONS"sv, errno);

    dbgln("SyscallTracer: Attached to PID {}", pid);
    return {};
}

ErrorOr<Vector<SyscallEvent>> SyscallTracer::monitor(Duration timeout)
{
    VERIFY(m_traced_pid != -1);

    auto start_time = MonotonicTime::now();
    m_syscall_events.clear();

    // Resume process execution
    if (ptrace(PTRACE_SYSCALL, m_traced_pid, nullptr, nullptr) < 0)
        return Error::from_syscall("ptrace SYSCALL"sv, errno);

    // Event loop
    while (true) {
        // Check timeout
        if (MonotonicTime::now() - start_time > timeout) {
            dbgln("SyscallTracer: Timeout reached, stopping trace");
            break;
        }

        // Wait for next event (with timeout)
        int status;
        pid_t waited_pid = waitpid(m_traced_pid, &status, WNOHANG);

        if (waited_pid < 0) {
            if (errno == EINTR)
                continue;  // Interrupted, retry
            return Error::from_syscall("waitpid"sv, errno);
        }

        if (waited_pid == 0) {
            // No event yet, sleep briefly and continue
            usleep(1000);  // 1ms
            continue;
        }

        // Process exited
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            dbgln("SyscallTracer: Process exited with status {}", WEXITSTATUS(status));
            break;
        }

        // Syscall stop (bit 7 set in status)
        if (WIFSTOPPED(status) && (status >> 8) == (SIGTRAP | 0x80)) {
            // Determine if entry or exit
            static bool in_syscall = false;

            if (!in_syscall) {
                // Syscall entry
                TRY(handle_syscall_entry(m_traced_pid));
                in_syscall = true;
            } else {
                // Syscall exit
                TRY(handle_syscall_exit(m_traced_pid));
                in_syscall = false;
            }

            // Continue tracing
            if (ptrace(PTRACE_SYSCALL, m_traced_pid, nullptr, nullptr) < 0)
                return Error::from_syscall("ptrace SYSCALL"sv, errno);
        }
    }

    dbgln("SyscallTracer: Collected {} syscall events", m_syscall_events.size());
    return m_syscall_events;
}

ErrorOr<void> SyscallTracer::handle_syscall_entry(pid_t pid)
{
    // Read registers to get syscall number and arguments
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) < 0)
        return Error::from_syscall("ptrace GETREGS"sv, errno);

    // Extract syscall number and arguments (x86-64)
    int syscall_nr = regs.orig_rax;
    u64 args[6] = {
        regs.rdi,  // arg0
        regs.rsi,  // arg1
        regs.rdx,  // arg2
        regs.r10,  // arg3
        regs.r8,   // arg4
        regs.r9    // arg5
    };

    // Store in-flight syscall state
    SyscallState state;
    state.syscall_number = syscall_nr;
    memcpy(state.args, args, sizeof(args));
    state.entry_timestamp_ns = MonotonicTime::now().nanoseconds_since_epoch();

    m_in_flight_syscalls.set(pid, state);
    m_stats.total_syscalls++;

    return {};
}

ErrorOr<void> SyscallTracer::handle_syscall_exit(pid_t pid)
{
    // Get in-flight syscall state
    auto state_opt = m_in_flight_syscalls.get(pid);
    if (!state_opt.has_value())
        return {};  // No entry for this syscall, skip

    auto state = state_opt.value();
    m_in_flight_syscalls.remove(pid);

    // Read return value
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) < 0)
        return Error::from_syscall("ptrace GETREGS"sv, errno);

    i64 retval = static_cast<i64>(regs.rax);

    // Parse syscall into event
    auto event = TRY(parse_syscall(pid, state.syscall_number, state.args));
    event.retval = retval;
    event.timestamp_ns = state.entry_timestamp_ns;

    // Update statistics
    switch (event.category) {
    case SyscallCategory::FileIO:
        m_stats.file_io_syscalls++;
        break;
    case SyscallCategory::ProcessControl:
        m_stats.process_syscalls++;
        event.suspicious = true;  // Process creation always suspicious
        break;
    case SyscallCategory::MemoryManagement:
        m_stats.memory_syscalls++;
        break;
    case SyscallCategory::Network:
        m_stats.network_syscalls++;
        event.suspicious = true;  // Network activity always suspicious
        break;
    default:
        break;
    }

    if (event.suspicious)
        m_stats.suspicious_syscalls++;

    // Store event
    TRY(m_syscall_events.try_append(move(event)));

    return {};
}

SyscallCategory SyscallTracer::classify_syscall(int nr)
{
    // Linux x86-64 syscall classification
    switch (nr) {
    // File I/O
    case 0:    // read
    case 1:    // write
    case 2:    // open
    case 3:    // close
    case 4:    // stat
    case 5:    // fstat
    case 6:    // lstat
    case 8:    // lseek
    case 87:   // unlink
    case 257:  // openat
    case 262:  // newfstatat
        return SyscallCategory::FileIO;

    // Process control
    case 57:   // fork
    case 58:   // vfork
    case 56:   // clone
    case 59:   // execve
    case 62:   // kill
        return SyscallCategory::ProcessControl;

    // Memory management
    case 9:    // mmap
    case 10:   // mprotect
    case 11:   // munmap
    case 12:   // brk
    case 25:   // mremap
        return SyscallCategory::MemoryManagement;

    // Network
    case 41:   // socket
    case 42:   // connect
    case 43:   // accept
    case 44:   // sendto
    case 45:   // recvfrom
    case 49:   // bind
    case 50:   // listen
        return SyscallCategory::Network;

    // Permission changes
    case 90:   // chmod
    case 92:   // chown
    case 105:  // setuid
    case 106:  // setgid
        return SyscallCategory::PermissionChange;

    // System (dangerous)
    case 165:  // mount
    case 166:  // umount2
    case 169:  // reboot
    case 167:  // swapon
        return SyscallCategory::System;

    default:
        return SyscallCategory::Unknown;
    }
}

ErrorOr<SyscallEvent> SyscallTracer::parse_syscall(pid_t pid, int nr, u64 args[6])
{
    SyscallEvent event;
    event.pid = pid;
    event.syscall_number = nr;
    event.category = classify_syscall(nr);
    memcpy(event.args, args, sizeof(event.args));

    // Parse context based on syscall type
    switch (nr) {
    case 2:    // open(filename, flags, mode)
    case 257:  // openat(dirfd, filename, flags, mode)
        event.file_path = TRY(read_string_from_tracee(pid, args[nr == 2 ? 0 : 1]));
        break;

    case 59:   // execve(filename, argv, envp)
        event.file_path = TRY(read_string_from_tracee(pid, args[0]));
        event.command_line = event.file_path;  // Simplified
        break;

    case 62:   // kill(pid, sig)
        event.signal_number = static_cast<u32>(args[1]);
        break;

    case 9:    // mmap(addr, length, prot, flags, fd, offset)
        event.memory_address = args[0];
        // Check for RWX (read-write-execute) - suspicious
        u32 prot = static_cast<u32>(args[2]);
        if ((prot & 0x7) == 0x7)  // PROT_READ | PROT_WRITE | PROT_EXEC
            event.suspicious = true;
        break;
    }

    return event;
}

ErrorOr<String> SyscallTracer::read_string_from_tracee(pid_t pid, u64 addr)
{
    if (addr == 0)
        return String {};

    // Read string from tracee memory using ptrace
    StringBuilder builder;
    u64 current_addr = addr;

    while (builder.length() < 4096) {  // Max 4KB string
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, current_addr, nullptr);
        if (word == -1 && errno != 0)
            return Error::from_syscall("ptrace PEEKDATA"sv, errno);

        // Extract bytes
        for (size_t i = 0; i < sizeof(long); i++) {
            char byte = (word >> (i * 8)) & 0xFF;
            if (byte == '\0')
                return builder.to_string();
            builder.append(byte);
        }

        current_addr += sizeof(long);
    }

    // Truncated
    return builder.to_string();
}

ErrorOr<void> SyscallTracer::detach()
{
    if (m_traced_pid == -1)
        return {};

    if (ptrace(PTRACE_DETACH, m_traced_pid, nullptr, nullptr) < 0)
        return Error::from_syscall("ptrace DETACH"sv, errno);

    m_traced_pid = -1;
    dbgln("SyscallTracer: Detached from process");
    return {};
}

}
```

---

## Behavioral Pattern Detection

### Metric Calculation

```cpp
// BehavioralAnalyzer::analyze_nsjail()

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail(
    ByteBuffer const& file_data,
    String const& filename)
{
    dbgln("BehavioralAnalyzer: Starting nsjail analysis for '{}'", filename);

    BehavioralMetrics metrics;

    // Step 1: Write file to sandbox
    auto sandbox_file_path = TRY(String::formatted("{}/target_file", m_sandbox_dir));
    TRY(write_file_to_sandbox(file_data, sandbox_file_path));

    // Step 2: Make file executable (if ELF/PE)
    if (is_executable_format(file_data))
        TRY(Core::System::chmod(sandbox_file_path, 0755));

    // Step 3: Launch nsjail sandbox
    auto nsjail_pid = TRY(launch_nsjail_sandbox(sandbox_file_path));

    // Step 4: Attach syscall tracer
    auto tracer = TRY(SyscallTracer::create());
    TRY(tracer->attach(nsjail_pid));

    // Step 5: Monitor execution
    auto syscall_events = TRY(tracer->monitor(m_config.timeout));

    // Step 6: Detach and cleanup
    TRY(tracer->detach());

    // Wait for nsjail to exit
    int status;
    waitpid(nsjail_pid, &status, 0);
    metrics.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    // Step 7: Analyze syscall events → populate metrics
    TRY(analyze_syscall_events(syscall_events, metrics));

    // Step 8: Calculate threat score
    metrics.threat_score = TRY(calculate_threat_score(metrics));
    metrics.suspicious_behaviors = TRY(generate_suspicious_behaviors(metrics));

    dbgln("BehavioralAnalyzer: nsjail analysis complete - Threat score: {:.2f}",
          metrics.threat_score);

    return metrics;
}

ErrorOr<void> BehavioralAnalyzer::analyze_syscall_events(
    Vector<SyscallEvent> const& events,
    BehavioralMetrics& metrics)
{
    // Track unique file paths, processes, network connections
    HashTable<String> file_paths;
    HashTable<String> temp_files;
    HashTable<String> hidden_files;
    HashTable<pid_t> processes;
    u32 process_chain_depth = 0;

    for (auto const& event : events) {
        switch (event.category) {
        case SyscallCategory::FileIO:
            metrics.file_operations++;

            if (event.file_path.has_value()) {
                auto path = event.file_path.value();
                file_paths.set(path);

                // Temp file detection
                if (path.contains("/tmp/"sv) || path.contains("/var/tmp/"sv))
                    temp_files.set(path);

                // Hidden file detection (starts with '.')
                auto filename = path.split_view('/').last();
                if (filename.starts_with('.'))
                    hidden_files.set(path);

                // Executable drop detection
                if (path.ends_with(".exe"sv) || path.ends_with(".sh"sv) ||
                    path.ends_with(".bat"sv) || path.ends_with(".ps1"sv))
                    metrics.executable_drops++;
            }
            break;

        case SyscallCategory::ProcessControl:
            metrics.process_operations++;
            processes.set(event.pid);

            // Track process chain depth (simplified)
            if (event.syscall_number == 56 || event.syscall_number == 57)  // clone/fork
                process_chain_depth++;

            // Detect process injection (ptrace)
            if (event.syscall_number == 101)  // ptrace
                metrics.code_injection_attempts++;

            break;

        case SyscallCategory::MemoryManagement:
            metrics.memory_operations++;

            // Detect RWX allocations (executable memory writes)
            if (event.suspicious)
                metrics.self_modification_attempts++;

            break;

        case SyscallCategory::Network:
            metrics.network_operations++;

            // Count unique connections (simplified - track socket syscalls)
            if (event.syscall_number == 42)  // connect
                metrics.outbound_connections++;

            break;

        case SyscallCategory::PermissionChange:
            // Privilege escalation attempt
            if (event.syscall_number == 105 || event.syscall_number == 106)  // setuid/setgid
                metrics.privilege_escalation_attempts++;
            break;

        default:
            break;
        }
    }

    // Populate aggregated counts
    metrics.temp_file_creates = temp_files.size();
    metrics.hidden_file_creates = hidden_files.size();

    // Linux-specific: Detect persistence mechanisms
    for (auto const& path : file_paths) {
        if (path.contains("/etc/cron"sv) || path.contains(".bashrc"sv) ||
            path.contains("/etc/systemd"sv))
            metrics.persistence_mechanisms++;
    }

    return {};
}
```

### Scoring Algorithm (from BEHAVIORAL_ANALYSIS_SPEC.md)

```cpp
ErrorOr<float> BehavioralAnalyzer::calculate_threat_score(BehavioralMetrics const& metrics)
{
    // Multi-factor scoring: 5 categories weighted

    float score = 0.0f;

    // Category 1: File System (weight: 0.40)
    float file_score = 0.0f;
    if (metrics.file_operations > 10) file_score += 0.3f;
    if (metrics.temp_file_creates > 3) file_score += 0.3f;
    if (metrics.hidden_file_creates > 0) file_score += 0.2f;
    if (metrics.executable_drops > 0) file_score += 0.2f;
    score += min(1.0f, file_score) * 0.40f;

    // Category 2: Process (weight: 0.30)
    float process_score = 0.0f;
    if (metrics.process_operations > 5) process_score += 0.3f;
    if (metrics.self_modification_attempts > 0) process_score += 0.4f;
    if (metrics.persistence_mechanisms > 0) process_score += 0.3f;
    score += min(1.0f, process_score) * 0.30f;

    // Category 3: Network (weight: 0.15)
    float network_score = 0.0f;
    if (metrics.network_operations > 5) network_score += 0.3f;
    if (metrics.outbound_connections > 3) network_score += 0.7f;
    score += min(1.0f, network_score) * 0.15f;

    // Category 4: System/Registry (weight: 0.10, future Windows)
    float system_score = 0.0f;
    if (metrics.privilege_escalation_attempts > 0) system_score += 0.8f;
    score += min(1.0f, system_score) * 0.10f;

    // Category 5: Memory (weight: 0.05)
    float memory_score = 0.0f;
    if (metrics.memory_operations > 10) memory_score += 0.5f;
    if (metrics.code_injection_attempts > 0) memory_score += 0.5f;
    score += min(1.0f, memory_score) * 0.05f;

    return min(1.0f, score);
}
```

---

## Integration with Orchestrator

### Modified Orchestrator Flow

```cpp
// Services/Sentinel/Sandbox/Orchestrator.cpp
// execute_tier2_native() - Now calls real nsjail implementation

ErrorOr<SandboxResult> Orchestrator::execute_tier2_native(
    ByteBuffer const& file_data,
    String const& filename)
{
    dbgln("Orchestrator: Executing Tier 2 native sandbox (nsjail)");

    VERIFY(m_behavioral_analyzer);
    auto start_time = MonotonicTime::now();

    SandboxResult result;

    // Execute in nsjail sandbox with syscall monitoring
    auto analysis_result = m_behavioral_analyzer->analyze(
        file_data,
        filename,
        m_config.timeout);

    if (analysis_result.is_error()) {
        auto error = analysis_result.release_error();

        // Handle timeout gracefully
        if (error.string_literal().contains("timeout"sv)) {
            m_stats.timeouts++;

            // Timeout = suspicious (treat as potential threat)
            result.behavioral_score = 0.5f;  // Medium threat
            TRY(result.detected_behaviors.try_append(
                "Execution timed out (potential infinite loop or evasion)"_string));

            dbgln("Orchestrator: Tier 2 timed out - treating as suspicious");
            return result;
        }

        // Other errors: propagate
        return error;
    }

    auto behavioral_metrics = analysis_result.release_value();

    // Map behavioral metrics to SandboxResult
    result.behavioral_score = behavioral_metrics.threat_score;
    result.file_operations = behavioral_metrics.file_operations;
    result.process_operations = behavioral_metrics.process_operations;
    result.network_operations = behavioral_metrics.network_operations;
    result.registry_operations = behavioral_metrics.registry_operations;
    result.memory_operations = behavioral_metrics.memory_operations;
    result.detected_behaviors = move(behavioral_metrics.suspicious_behaviors);

    // Update statistics
    auto tier2_time = MonotonicTime::now() - start_time;
    if (m_stats.tier2_executions > 0) {
        m_stats.average_tier2_time = Duration::from_nanoseconds(
            (m_stats.average_tier2_time.to_nanoseconds() * (m_stats.tier2_executions - 1) +
             tier2_time.to_nanoseconds()) / m_stats.tier2_executions);
    }

    dbgln("Orchestrator: Tier 2 complete in {}ms - Behavioral score: {:.2f}, Behaviors: {}",
          tier2_time.to_milliseconds(),
          result.behavioral_score,
          result.detected_behaviors.size());

    return result;
}
```

### Orchestrator Decision Flow

```
analyze_file(file_data, filename)
  ↓
Check PolicyGraph Cache
  ├─ HIT → Return cached verdict
  └─ MISS → Continue
  ↓
Execute Tier 1 (WASM - Phase 1b)
  ├─ Clean (score < 0.3) → ALLOW
  ├─ Malicious (score > 0.7) → BLOCK
  └─ Suspicious (0.3-0.7) → Continue to Tier 2
  ↓
Execute Tier 2 (nsjail - Phase 2)  ← NEW
  ├─ analyze_nsjail()
  │  ├─ Write file to sandbox
  │  ├─ Launch nsjail
  │  ├─ Attach SyscallTracer (ptrace)
  │  ├─ Monitor execution (1-5 seconds)
  │  ├─ Collect BehavioralMetrics
  │  └─ Calculate threat_score
  ↓
Combine Scores (VerdictEngine)
  ├─ YARA: 40% weight
  ├─ ML: 35% weight
  └─ Behavioral: 25% weight
  ↓
Generate Final Verdict
  ├─ Benign (< 0.30)
  ├─ Suspicious (0.30-0.60)
  └─ Malicious (≥ 0.60)
  ↓
Store in PolicyGraph Cache
  ↓
Return SandboxResult to RequestServer
```

---

## Platform Support Strategy

### Linux (Primary Platform)

**Full Implementation**:
- nsjail sandbox with all namespaces
- ptrace-based syscall monitoring
- seccomp-BPF syscall filtering
- cgroups resource limits
- tmpfs-based filesystem isolation

**Requirements**:
- Linux kernel 3.8+ (namespaces support)
- nsjail installed (`/usr/bin/nsjail`)
- CAP_SYS_PTRACE capability (for ptrace)
- seccomp-BPF support (kernel CONFIG_SECCOMP_FILTER)

**Detection**:
```cpp
ErrorOr<bool> BehavioralAnalyzer::is_nsjail_available()
{
#ifdef __linux__
    // Check if nsjail binary exists
    if (!FileSystem::exists("/usr/bin/nsjail"sv))
        return false;

    // Check if we have ptrace capability
    if (prctl(PR_GET_DUMPABLE) == 0)
        return false;  // Cannot ptrace

    // Check kernel version (need 3.8+ for namespaces)
    struct utsname uts;
    if (uname(&uts) < 0)
        return Error::from_syscall("uname"sv, errno);

    // Parse kernel version (simplified)
    int major, minor;
    if (sscanf(uts.release, "%d.%d", &major, &minor) < 2)
        return false;

    if (major < 3 || (major == 3 && minor < 8))
        return false;  // Kernel too old

    return true;
#else
    return false;  // Not Linux
#endif
}
```

### macOS (Graceful Degradation)

**Fallback Strategy**:
- Use Phase 1 (WASM) only
- No Tier 2 behavioral analysis
- Rely on YARA + ML for detection

**Rationale**:
- macOS Sandbox API is complex and undocumented
- Apple restricts system-level monitoring (SIP)
- nsjail not available on macOS
- dtrace/dtruss requires root privileges

**Implementation**:
```cpp
ErrorOr<void> BehavioralAnalyzer::initialize_sandbox()
{
    dbgln("BehavioralAnalyzer: Initializing sandbox");

    // Detect platform
#ifdef __linux__
    auto nsjail_available = TRY(is_nsjail_available());
    if (nsjail_available) {
        m_use_mock = false;  // Use real nsjail
        dbgln("BehavioralAnalyzer: nsjail sandbox available");
    } else {
        m_use_mock = true;
        dbgln("BehavioralAnalyzer: nsjail not available, using mock");
    }
#else
    m_use_mock = true;
    dbgln("BehavioralAnalyzer: Non-Linux platform, using mock (WASM-only)");
#endif

    // Create sandbox directory
    m_sandbox_dir = TRY(create_temporary_sandbox_dir());

    return {};
}
```

### Windows (Future Work)

**Deferred to Milestone 0.6**:
- Windows Sandbox API (requires Windows 10 1903+)
- AppContainer isolation
- Job Objects for resource limits
- ETW (Event Tracing for Windows) for syscall monitoring

**Rationale**:
- Windows sandbox infrastructure significantly different
- Requires separate implementation effort (2-3 weeks)
- Ladybird primary focus is Unix-like systems
- Phase 1 (WASM) provides acceptable coverage for M0.5

---

## Performance Optimization

### Target Performance Budget

| **Operation** | **Target** | **P50** | **P95** | **P99** |
|--------------|-----------|---------|---------|---------|
| **Total Analysis** | 1-3s | 1.5s | 4s | 5s |
| Sandbox Init | 100-200ms | 150ms | 300ms | 500ms |
| File Write | 10-50ms | 20ms | 100ms | 200ms |
| nsjail Launch | 50-100ms | 75ms | 200ms | 300ms |
| ptrace Attach | 10-20ms | 15ms | 50ms | 100ms |
| Monitoring Loop | 1-2s | 1.5s | 3s | 4s |
| Cleanup | 50-100ms | 75ms | 150ms | 200ms |

### Optimization Strategies

#### 1. Parallel Syscall Analysis

```cpp
// Process syscall events in parallel (thread pool)
ErrorOr<void> BehavioralAnalyzer::analyze_syscall_events_parallel(
    Vector<SyscallEvent> const& events,
    BehavioralMetrics& metrics)
{
    // Divide events into chunks for parallel processing
    size_t chunk_size = max(1UL, events.size() / 4);  // 4 threads

    Vector<BehavioralMetrics> partial_metrics;

    // Spawn threads to process chunks
    // (Pseudo-code - actual implementation uses LibCore::EventLoop or thread pool)
    for (size_t i = 0; i < events.size(); i += chunk_size) {
        size_t end = min(i + chunk_size, events.size());
        auto chunk = events.span().slice(i, end - i);

        // Process chunk → partial metrics
        BehavioralMetrics partial;
        TRY(analyze_syscall_chunk(chunk, partial));
        partial_metrics.append(partial);
    }

    // Merge partial metrics
    for (auto const& partial : partial_metrics) {
        metrics.file_operations += partial.file_operations;
        metrics.process_operations += partial.process_operations;
        // ... (merge all fields)
    }

    return {};
}
```

#### 2. Early Termination

```cpp
// Terminate analysis early if verdict is clear
ErrorOr<BehavioralMetrics> BehavioralAnalyzer::monitor_with_early_termination(
    SyscallTracer& tracer,
    Duration timeout)
{
    auto start_time = MonotonicTime::now();
    BehavioralMetrics metrics;

    while (MonotonicTime::now() - start_time < timeout) {
        // Collect batch of syscall events
        auto events = TRY(tracer->collect_batch(Duration::from_milliseconds(100)));

        // Update metrics incrementally
        TRY(analyze_syscall_events(events, metrics));

        // Early termination: Clear malicious behavior
        if (metrics.network_operations > 10 || metrics.code_injection_attempts > 0) {
            dbgln("BehavioralAnalyzer: Early termination - clear malicious pattern");
            metrics.threat_score = 0.9f;
            return metrics;  // Stop early
        }

        // Early termination: No suspicious activity after 1 second
        if (MonotonicTime::now() - start_time > Duration::from_seconds(1)) {
            if (metrics.file_operations < 5 && metrics.process_operations == 0) {
                dbgln("BehavioralAnalyzer: Early termination - no suspicious activity");
                metrics.threat_score = 0.1f;
                return metrics;  // Likely benign
            }
        }
    }

    // Timeout reached
    metrics.timed_out = true;
    return metrics;
}
```

#### 3. Syscall Filtering (Reduce Tracing Overhead)

```cpp
// Only trace specific syscalls (not all)
ErrorOr<void> SyscallTracer::set_syscall_filter(Vector<int> const& syscall_numbers)
{
    // Use seccomp-BPF to filter syscalls at kernel level
    // Only specified syscalls trigger ptrace stops

    struct sock_filter filter[] = {
        // Load syscall number
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),

        // Check against whitelist
        // ... (generated from syscall_numbers)

        // Default: ALLOW (don't trace)
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
    };

    struct sock_fprog prog = {
        .len = sizeof(filter) / sizeof(filter[0]),
        .filter = filter
    };

    // Install filter
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0)
        return Error::from_syscall("prctl PR_SET_SECCOMP"sv, errno);

    return {};
}
```

#### 4. tmpfs Sandbox (Faster File I/O)

```cpp
ErrorOr<String> BehavioralAnalyzer::create_temporary_sandbox_dir()
{
    // Use tmpfs for sandbox directory (in-memory filesystem)
    // Faster than disk I/O, automatically cleaned up

    auto tmpfs_path = "/dev/shm/sentinel-sandbox"_string;

    // Create unique directory
    char template_path[] = "/dev/shm/sentinel-sandbox-XXXXXX";
    char* result = mkdtemp(template_path);
    if (!result)
        return Error::from_syscall("mkdtemp"sv, errno);

    return String::from_utf8(StringView { result, strlen(result) });
}
```

---

## Testing Strategy

### Unit Tests

```cpp
// Services/Sentinel/TestBehavioralAnalyzer.cpp

TEST_CASE(nsjail_sandbox_launch)
{
    auto config = SandboxConfig {};
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create test file
    ByteBuffer test_file;
    test_file.append("#!/bin/sh\necho 'Hello World'\n", 28);

    // Analyze
    auto metrics = MUST(analyzer->analyze(test_file, "test.sh"_string, Duration::from_seconds(5)));

    // Verify basic execution
    EXPECT(!metrics.timed_out);
    EXPECT_EQ(metrics.exit_code, 0);
    EXPECT(metrics.threat_score < 0.3f);  // Benign
}

TEST_CASE(detect_file_operations)
{
    auto config = SandboxConfig {};
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create test script that creates many files
    ByteBuffer test_file;
    test_file.append("#!/bin/sh\nfor i in $(seq 1 100); do touch /tmp/file$i; done\n", 62);

    // Analyze
    auto metrics = MUST(analyzer->analyze(test_file, "file_spammer.sh"_string, Duration::from_seconds(5)));

    // Verify file operations detected
    EXPECT(metrics.file_operations > 50);
    EXPECT(metrics.temp_file_creates > 50);
    EXPECT(metrics.threat_score > 0.3f);  // Suspicious
}

TEST_CASE(detect_process_creation)
{
    auto config = SandboxConfig {};
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create test script that spawns processes
    ByteBuffer test_file;
    test_file.append("#!/bin/sh\nfor i in $(seq 1 10); do sh -c 'echo $i' & done\n", 58);

    // Analyze
    auto metrics = MUST(analyzer->analyze(test_file, "process_spawner.sh"_string, Duration::from_seconds(5)));

    // Verify process operations detected
    EXPECT(metrics.process_operations > 5);
    EXPECT(metrics.threat_score > 0.2f);
}

TEST_CASE(timeout_handling)
{
    auto config = SandboxConfig {};
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create infinite loop script
    ByteBuffer test_file;
    test_file.append("#!/bin/sh\nwhile true; do sleep 1; done\n", 35);

    // Analyze with short timeout
    auto metrics = MUST(analyzer->analyze(test_file, "infinite.sh"_string, Duration::from_seconds(2)));

    // Verify timeout detected
    EXPECT(metrics.timed_out);
    EXPECT(metrics.threat_score > 0.4f);  // Timeout = suspicious
}
```

### Integration Tests

```cpp
// Services/Sentinel/TestOrchestratorPhase2.cpp

TEST_CASE(orchestrator_tier2_integration)
{
    auto config = SandboxConfig {};
    config.enable_tier1_wasm = true;
    config.enable_tier2_native = true;

    auto orchestrator = MUST(Orchestrator::create(config));

    // Create suspicious file (multi-process, network activity)
    ByteBuffer suspicious_file;
    suspicious_file.append("#!/bin/sh\n", 10);
    suspicious_file.append("for i in $(seq 1 20); do touch /tmp/test$i; done\n", 49);
    suspicious_file.append("sh -c 'echo malware' &\n", 23);

    // Analyze
    auto result = MUST(orchestrator->analyze_file(suspicious_file, "suspicious.sh"_string));

    // Verify Tier 2 executed
    EXPECT(orchestrator->get_statistics().tier2_executions > 0);

    // Verify behavioral score contributed
    EXPECT(result.behavioral_score > 0.3f);
    EXPECT(result.file_operations > 10);
    EXPECT(result.process_operations > 0);

    // Verify final verdict
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::SUSPICIOUS);
}

TEST_CASE(orchestrator_cache_integration)
{
    auto config = SandboxConfig {};
    auto orchestrator = MUST(Orchestrator::create(config));

    ByteBuffer test_file;
    test_file.append("#!/bin/sh\necho 'test'\n", 20);

    // First analysis (cache miss)
    auto result1 = MUST(orchestrator->analyze_file(test_file, "test.sh"_string));
    auto time1 = result1.execution_time;

    // Second analysis (cache hit)
    auto result2 = MUST(orchestrator->analyze_file(test_file, "test.sh"_string));
    auto time2 = result2.execution_time;

    // Verify cache hit (faster)
    EXPECT(time2 < time1);
    EXPECT(time2 < Duration::from_milliseconds(100));  // Very fast

    // Verify same verdict
    EXPECT_EQ(result1.threat_level, result2.threat_level);
    EXPECT_APPROX_EQ(result1.composite_score, result2.composite_score, 0.01f);
}
```

### Malware Sample Testing

```bash
# Test with real malware samples (VirusTotal, MalwareBazaar)
# WARNING: Handle with extreme care in isolated environment

# Download samples (example)
curl -o /tmp/ransomware_sample.exe \
  "https://malwarebazaar.abuse.ch/sample/[SHA256]"

# Test analysis
./Build/release/bin/TestBehavioralAnalyzer \
  --sample=/tmp/ransomware_sample.exe

# Expected output:
# ✓ Detected file modification (150+ files)
# ✓ High entropy writes (ransomware encryption)
# ✓ Threat score: 0.85 (MALICIOUS)
# ✓ Verdict: BLOCK
```

### Performance Benchmarks

```cpp
// Services/Sentinel/BenchmarkBehavioralAnalyzer.cpp

BENCHMARK_CASE(behavioral_analysis_performance)
{
    auto config = SandboxConfig {};
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Test with various file sizes
    Vector<size_t> file_sizes = { 1KB, 10KB, 100KB, 1MB, 10MB };

    for (auto size : file_sizes) {
        ByteBuffer test_file;
        test_file.resize(size);

        auto start = MonotonicTime::now();
        auto metrics = MUST(analyzer->analyze(test_file, "test"_string, Duration::from_seconds(5)));
        auto duration = MonotonicTime::now() - start;

        outln("File size: {} KB, Analysis time: {} ms", size / 1024, duration.to_milliseconds());

        // Verify within budget
        EXPECT(duration < Duration::from_seconds(5));
    }
}

// Target output:
// File size: 1 KB, Analysis time: 120 ms
// File size: 10 KB, Analysis time: 150 ms
// File size: 100 KB, Analysis time: 250 ms
// File size: 1024 KB, Analysis time: 500 ms
// File size: 10240 KB, Analysis time: 1200 ms
```

---

## Security Considerations

### Sandbox Escape Prevention

**Defense-in-Depth Layers**:

1. **Linux Namespaces** (Primary Isolation)
   - PID namespace: Process isolation
   - Mount namespace: Filesystem isolation
   - Network namespace: No external network
   - User namespace: Run as unprivileged user
   - IPC namespace: No shared memory

2. **seccomp-BPF** (Syscall Filtering)
   - Block dangerous syscalls (mount, reboot, ptrace)
   - ERRNO dangerous operations (socket, connect)
   - Kill process on critical violations

3. **Resource Limits** (DoS Prevention)
   - CPU: 5 seconds max
   - Memory: 256MB max
   - File descriptors: 50 max
   - Processes: 10 max

4. **Filesystem Isolation**
   - chroot to temporary directory
   - Read-only bind mounts for system libraries
   - tmpfs for writable directories (no persistence)
   - Auto-cleanup after analysis

5. **Time Limits** (Prevent Hang)
   - SIGALRM after 5 seconds
   - Force kill (SIGKILL) if SIGALRM ignored
   - Watchdog thread in parent process

### Known Vulnerabilities and Mitigations

| **Vulnerability** | **Mitigation** | **Residual Risk** |
|-------------------|----------------|-------------------|
| Kernel exploits (CVE-XXXX) | Keep kernel updated, seccomp filters | Low (requires 0-day) |
| Namespace escape | User namespace UID mapping | Very Low |
| Resource exhaustion | cgroups limits, time limits | Very Low |
| Side-channel attacks | Not mitigated (acceptable for M0.5) | Medium (information leak only) |
| ptrace vulnerabilities | Run tracer as separate user | Low |

### Threat Model

**In Scope**:
- Malware attempting to modify files
- Malware attempting network connections
- Malware attempting process injection
- Malware attempting privilege escalation
- Malware attempting to persist (cron, systemd)

**Out of Scope** (Acceptable Risks):
- Kernel exploits (0-day)
- Hardware attacks (Spectre, Meltdown)
- Side-channel timing attacks
- Physical access attacks
- Supply chain attacks (compromised dependencies)

### Security Checklist

Before deployment:
- [ ] Kernel version >= 3.8 (namespace support)
- [ ] seccomp-BPF enabled (CONFIG_SECCOMP_FILTER)
- [ ] nsjail binary integrity verified (checksum)
- [ ] Sandbox directory permissions 0700
- [ ] Temporary files cleaned up after analysis
- [ ] No sensitive data in sandbox directory
- [ ] ptrace capability restricted
- [ ] Resource limits enforced (cgroups)
- [ ] Timeout mechanisms tested
- [ ] Error handling prevents information leaks

---

## Implementation Plan

### Phase 2 Timeline: 2-3 Weeks

#### Week 1: Core Infrastructure

**Days 1-2: SyscallTracer Implementation**
- [ ] Create `SyscallTracer.{h,cpp}`
- [ ] Implement ptrace attach/detach
- [ ] Implement syscall entry/exit handling
- [ ] Implement syscall classification
- [ ] Unit tests for SyscallTracer

**Days 3-4: nsjail Integration**
- [ ] Implement `launch_nsjail_sandbox()`
- [ ] Implement `generate_seccomp_policy()`
- [ ] Test nsjail execution with sample files
- [ ] Handle nsjail errors gracefully

**Day 5: Behavioral Metrics**
- [ ] Implement `analyze_syscall_events()`
- [ ] Implement metric aggregation
- [ ] Implement threat score calculation
- [ ] Test with benign/malicious samples

#### Week 2: Integration & Testing

**Days 1-2: Orchestrator Integration**
- [ ] Update `Orchestrator::execute_tier2_native()`
- [ ] Replace mock implementation with real nsjail
- [ ] Test end-to-end flow (YARA → ML → WASM → nsjail)
- [ ] Verify PolicyGraph caching

**Days 3-4: Platform Support**
- [ ] Implement `is_nsjail_available()` detection
- [ ] Test graceful degradation on non-Linux
- [ ] Document platform requirements
- [ ] CI/CD integration (Linux-only tests)

**Day 5: Performance Optimization**
- [ ] Benchmark analysis time
- [ ] Implement early termination
- [ ] Optimize syscall filtering
- [ ] Profile and optimize hotspots

#### Week 3: Testing & Documentation

**Days 1-2: Comprehensive Testing**
- [ ] Unit tests (100+ test cases)
- [ ] Integration tests with Orchestrator
- [ ] Malware sample testing (VirusTotal)
- [ ] Performance benchmarks
- [ ] Security audit (self-review)

**Days 3-4: Documentation**
- [ ] Update BEHAVIORAL_ANALYSIS_SPEC.md
- [ ] Create PHASE_2_USER_GUIDE.md
- [ ] Document nsjail configuration
- [ ] Create troubleshooting guide

**Day 5: Code Review & Cleanup**
- [ ] Code review
- [ ] Address feedback
- [ ] Final testing
- [ ] Merge to main branch

---

## Appendix

### A. syscall Number Reference (x86-64 Linux)

| **Syscall** | **Number** | **Category** | **Risk Level** |
|-------------|-----------|-------------|----------------|
| read | 0 | FileIO | Low |
| write | 1 | FileIO | Low |
| open | 2 | FileIO | Low |
| close | 3 | FileIO | Low |
| stat | 4 | FileIO | Low |
| mmap | 9 | Memory | Medium |
| mprotect | 10 | Memory | High (if RWX) |
| clone | 56 | Process | High |
| fork | 57 | Process | High |
| execve | 59 | Process | Critical |
| kill | 62 | Process | High |
| ptrace | 101 | Process | Critical |
| socket | 41 | Network | High |
| connect | 42 | Network | Critical |
| chmod | 90 | Permission | Medium |
| setuid | 105 | Permission | Critical |
| mount | 165 | System | Critical (blocked) |
| reboot | 169 | System | Critical (blocked) |

### B. BehavioralMetrics Thresholds

From BEHAVIORAL_ANALYSIS_SPEC.md:

| **Metric** | **Benign Threshold** | **Suspicious Threshold** | **Malicious Threshold** |
|------------|---------------------|-------------------------|------------------------|
| file_operations | < 10 | 10-100 | > 100 |
| temp_file_creates | < 3 | 3-10 | > 10 |
| hidden_file_creates | 0 | 1-3 | > 3 |
| executable_drops | 0 | 1 | > 1 |
| process_operations | < 3 | 3-10 | > 10 |
| self_modification_attempts | 0 | 1 | > 1 |
| persistence_mechanisms | 0 | 1 | > 1 |
| network_operations | 0 | 1-5 | > 5 |
| outbound_connections | 0 | 1-3 | > 3 |
| privilege_escalation_attempts | 0 | 1 | > 1 |
| code_injection_attempts | 0 | 1 | > 1 |

### C. Error Handling Patterns

```cpp
// Pattern 1: Graceful degradation on nsjail failure
auto result = launch_nsjail_sandbox(file_path);
if (result.is_error()) {
    dbgln("BehavioralAnalyzer: nsjail launch failed: {}", result.error());

    // Fallback: Return conservative metrics (assume suspicious)
    BehavioralMetrics metrics;
    metrics.threat_score = 0.5f;  // Medium threat
    TRY(metrics.suspicious_behaviors.try_append(
        "Sandbox unavailable - analysis incomplete"_string));
    return metrics;
}

// Pattern 2: Timeout handling
if (metrics.timed_out) {
    // Timeout = suspicious (potential evasion)
    metrics.threat_score = max(metrics.threat_score, 0.5f);
    TRY(metrics.suspicious_behaviors.try_append(
        "Execution timeout - potential evasion or infinite loop"_string));
}

// Pattern 3: ptrace error recovery
auto attach_result = tracer->attach(pid);
if (attach_result.is_error()) {
    // Cannot monitor syscalls - treat as suspicious
    dbgln("SyscallTracer: attach failed: {}", attach_result.error());

    // Kill sandbox and return conservative verdict
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);

    BehavioralMetrics metrics;
    metrics.threat_score = 0.5f;
    return metrics;
}
```

### D. Platform Detection

```cpp
// Compile-time platform detection
#if defined(__linux__)
    #define SENTINEL_PLATFORM_LINUX 1
    #define SENTINEL_HAS_NSJAIL 1
#elif defined(__APPLE__)
    #define SENTINEL_PLATFORM_MACOS 1
    #define SENTINEL_HAS_NSJAIL 0
#elif defined(_WIN32)
    #define SENTINEL_PLATFORM_WINDOWS 1
    #define SENTINEL_HAS_NSJAIL 0
#else
    #define SENTINEL_PLATFORM_UNKNOWN 1
    #define SENTINEL_HAS_NSJAIL 0
#endif

// Runtime nsjail availability check
ErrorOr<bool> BehavioralAnalyzer::check_nsjail_runtime()
{
#if SENTINEL_HAS_NSJAIL
    // Check nsjail binary
    if (!FileSystem::exists("/usr/bin/nsjail"sv))
        return false;

    // Check kernel capabilities
    auto result = Core::System::uname();
    if (result.is_error())
        return false;

    // ... (kernel version check)

    return true;
#else
    return false;
#endif
}
```

### E. References

- **nsjail GitHub**: https://github.com/google/nsjail
- **seccomp-BPF Guide**: https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html
- **Linux Namespaces**: https://man7.org/linux/man-pages/man7/namespaces.7.html
- **ptrace(2)**: https://man7.org/linux/man-pages/man2/ptrace.2.html
- **BEHAVIORAL_ANALYSIS_SPEC.md**: ./BEHAVIORAL_ANALYSIS_SPEC.md
- **MILESTONE_0.5_PLAN.md**: ./MILESTONE_0.5_PLAN.md
- **SANDBOX_TECHNOLOGY_COMPARISON.md**: ./SANDBOX_TECHNOLOGY_COMPARISON.md

---

## Document Metadata

**Version**: 1.0
**Author**: Ladybird Sentinel Team
**Created**: 2025-11-02
**Status**: Architecture Design - Ready for Implementation
**Next Steps**: Begin Week 1 implementation (SyscallTracer)
**Estimated Effort**: 2-3 weeks (12-15 developer days)

**Review Checklist**:
- [x] Architecture complete and detailed
- [x] nsjail configuration specified
- [x] Syscall monitoring strategy defined
- [x] Integration with Orchestrator documented
- [x] Platform support strategy clarified
- [x] Performance optimization identified
- [x] Testing strategy comprehensive
- [x] Security considerations addressed
- [x] Implementation plan with timeline
- [x] Code examples and diagrams provided

**Approval**: Pending review from security team and C++ developers.

---

**END OF DOCUMENT**
