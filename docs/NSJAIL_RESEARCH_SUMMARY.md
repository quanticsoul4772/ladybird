# NsJail Research Summary for Behavioral Analysis

**Author**: Behavioral Analysis Team
**Date**: 2025-11-02
**Purpose**: Research nsjail as sandboxing technology for Tier 2 malware behavioral analysis
**Target**: Phase 2 BehavioralAnalyzer implementation (Milestone 0.5)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Installation Guide](#installation-guide)
3. [Key Features and Capabilities](#key-features-and-capabilities)
4. [Recommended Configuration for Malware Analysis](#recommended-configuration-for-malware-analysis)
5. [Syscall Monitoring Approach](#syscall-monitoring-approach)
6. [Integration Strategy with BehavioralAnalyzer](#integration-strategy-with-behavioralanalyzer)
7. [Performance Considerations](#performance-considerations)
8. [Known Limitations and Workarounds](#known-limitations-and-workarounds)
9. [Alternative Technologies](#alternative-technologies)
10. [Recommendations](#recommendations)

---

## Executive Summary

**nsjail** is a lightweight process isolation tool developed by Google that leverages Linux kernel features for sandboxing. It is **highly suitable** for implementing Tier 2 behavioral analysis in Ladybird's Sentinel malware detection system.

### Key Strengths
- **Multi-layered isolation**: Combines namespaces, seccomp-bpf, cgroups, and rlimits
- **Fine-grained control**: Expressive Kafel language for syscall filtering
- **Resource limiting**: CPU, memory, file descriptor constraints prevent resource exhaustion
- **Network isolation**: Full network namespace isolation or cloned MACVLAN interfaces
- **Actively maintained**: Google-backed project with regular updates

### Suitability for Malware Sandboxing
- ✅ **Process isolation**: Full namespace isolation prevents escape
- ✅ **Filesystem containment**: Read-only bind mounts + tmpfs prevent host modification
- ✅ **Syscall filtering**: Granular allow/deny lists for dangerous operations
- ✅ **Resource limits**: Time/memory limits prevent DoS attacks
- ✅ **Network control**: Complete network isolation or monitored network access
- ⚠️ **Syscall monitoring**: Requires external tools (strace/ptrace) for detailed logging

### Recommendation
**Proceed with nsjail** for Phase 2 BehavioralAnalyzer implementation. It provides the best balance of security, flexibility, and performance for malware analysis use cases.

---

## Installation Guide

### System Requirements

**Minimum Kernel Version**: Linux 3.8+ (for user namespaces)
**Recommended Kernel**: Linux 4.6+ (for cgroup namespaces)
**Optimal Kernel**: Linux 4.14+ (for seccomp logging)

### Distribution-Specific Installation

#### Ubuntu/Debian

**Option 1: Install from Package** (if available)
```bash
# Check if available in repositories (Ubuntu 22.04+)
apt search nsjail

# Install if available
sudo apt install nsjail
```

**Option 2: Build from Source** (recommended)
```bash
# Install build dependencies
sudo apt update
sudo apt install -y \
    autoconf \
    bison \
    flex \
    gcc \
    g++ \
    git \
    libprotobuf-dev \
    libnl-route-3-dev \
    libtool \
    make \
    pkg-config \
    protobuf-compiler

# Clone repository
cd /tmp
git clone https://github.com/google/nsjail.git
cd nsjail

# Build
make

# Install binary
sudo cp nsjail /usr/local/bin/
sudo chmod 755 /usr/local/bin/nsjail

# Verify installation
nsjail --help
```

#### Fedora/RHEL/CentOS

```bash
# Install build dependencies
sudo dnf install -y \
    autoconf \
    bison \
    flex \
    gcc \
    gcc-c++ \
    git \
    libnl3-devel \
    libtool \
    make \
    pkgconfig \
    protobuf-compiler \
    protobuf-devel

# Clone and build (same as Ubuntu)
cd /tmp
git clone https://github.com/google/nsjail.git
cd nsjail
make
sudo cp nsjail /usr/local/bin/
```

#### Arch Linux

```bash
# Install from extra repository (preferred)
sudo pacman -S nsjail

# Or build from AUR
yay -S nsjail-git
```

### Verification

```bash
# Test basic functionality
nsjail --version

# Test namespace support
nsjail -Mo --chroot /tmp -- /bin/echo "Sandbox works!"

# Check kernel capabilities
grep CONFIG_USER_NS /boot/config-$(uname -r)
grep CONFIG_NAMESPACES /boot/config-$(uname -r)
grep CONFIG_SECCOMP /boot/config-$(uname -r)
```

### Docker-Based Installation (Alternative)

```bash
# Build Docker image
git clone https://github.com/google/nsjail.git
cd nsjail
docker build -t nsjail-container .

# Run via Docker
docker run --privileged --rm -it nsjail-container nsjail \
    --user 99999 --group 99999 --disable_proc --chroot / \
    --time_limit 30 /bin/bash
```

---

## Key Features and Capabilities

### 1. Linux Namespace Isolation

nsjail leverages all major Linux namespace types:

| Namespace | Flag | Purpose | Malware Analysis Benefit |
|-----------|------|---------|--------------------------|
| **UTS** | `clone_newuts` | Hostname isolation | Prevents hostname enumeration |
| **MOUNT** | `clone_newns` | Filesystem isolation | Prevents unauthorized file access |
| **PID** | `clone_newpid` | Process tree isolation | Prevents process enumeration |
| **IPC** | `clone_newipc` | IPC resource isolation | Prevents IPC-based attacks |
| **NET** | `clone_newnet` | Network isolation | Prevents network-based C&C |
| **USER** | `clone_newuser` | UID/GID mapping | Unprivileged execution |
| **CGROUP** | `clone_newcgroup` | Resource control | Prevents resource exhaustion |
| **TIME** | `clone_newtime` | Time namespace | Prevents time-based evasion |

### 2. Filesystem Containment

**Bind Mounts**:
- Read-only mounts (`--bindmount_ro` / `-R`) for system directories
- Read-write mounts (`--bindmount` / `-B`) for specific paths
- Temporary filesystems (`--tmpfsmount` / `-T`) for isolated storage

**Mount Options**:
- `nosuid`: Prevents setuid execution
- `nodev`: Disallows device files
- `noexec`: Prevents executable execution

**Chroot/Pivot Root**:
- `--chroot` / `-c`: Change root directory
- `--no_pivotroot`: Use chroot instead of pivot_root (if pivot_root fails)

### 3. Resource Limits (RLimits)

Prevents resource exhaustion attacks:

```bash
--rlimit_as 128       # Virtual memory (MB)
--rlimit_cpu 10       # CPU time (seconds)
--rlimit_fsize 0      # File size (MB) - 0 prevents file creation
--rlimit_nofile 32    # Open file descriptors
--rlimit_nproc SOFT   # Max processes
--rlimit_core 0       # Core dump size
--rlimit_stack 1      # Stack size (MB)
```

### 4. Seccomp-BPF Syscall Filtering

**Kafel Language** for expressive syscall policies:

**Actions**:
- `ALLOW`: Permit syscall
- `KILL`/`DENY`: Terminate process
- `ERRNO(n)`: Return error code n
- `LOG`: Log syscall (requires Linux 4.14+)
- `TRAP(n)`: Send signal n
- `TRACE(n)`: Invoke tracer

**Example Policy**:
```
POLICY malware_analysis {
    # Allow essential syscalls
    ALLOW {
        read, write, open, openat, close, stat, fstat, lstat,
        lseek, mmap, munmap, mprotect, brk,
        rt_sigaction, rt_sigprocmask, rt_sigreturn,
        exit, exit_group, wait4
    }

    # Log suspicious syscalls
    LOG {
        socket, connect, bind, listen, accept,
        execve, fork, vfork, clone,
        ptrace, process_vm_readv, process_vm_writev
    }

    # Block dangerous syscalls
    ERRNO(EPERM) {
        reboot, kexec_load, init_module, delete_module,
        iopl, ioperm, modify_ldt
    }
}
DEFAULT KILL
```

**Configuration Methods**:
```bash
# Via file
--seccomp_policy /path/to/policy.kafel

# Via string
--seccomp_string 'ALLOW { read, write } DEFAULT KILL'

# Enable logging
--seccomp_log
```

### 5. Cgroup Resource Control

**Memory Limits**:
```bash
--cgroup_mem_max 256        # Maximum memory (MB)
--cgroup_mem_memsw_max 512  # Memory + swap (MB)
--cgroup_mem_swap_max 256   # Swap limit (MB)
```

**CPU Limits**:
```bash
--cgroup_cpu_ms_per_sec 500  # CPU time (ms/sec) - 50% of one core
```

**Process Limits**:
```bash
--cgroup_pids_max 10  # Maximum processes in cgroup
```

### 6. Network Isolation

**Full Isolation**:
```bash
--clone_newnet  # Create isolated network namespace
--iface_no_lo   # Disable loopback interface
```

**Monitored Network Access**:
```bash
--macvlan_iface eth0       # Clone interface eth0
--macvlan_vs_ip 10.0.0.2   # Assign virtual IP
--macvlan_vs_nm 24         # Network mask
--macvlan_vs_gw 10.0.0.1   # Gateway
```

### 7. User/Group Mapping

```bash
# Map inside UID 0 to outside UID 1000
--uidmap '0:1000:1'

# Map inside GID 0 to outside GID 1000
--gidmap '0:1000:1'

# Unprivileged execution
--user 99999
--group 99999
```

### 8. Execution Modes

| Mode | Flag | Description | Use Case |
|------|------|-------------|----------|
| **ONCE** | `-Mo` | Single execution | One-shot analysis |
| **RERUN** | `-Mr` | Repeated execution | Continuous monitoring |
| **LISTEN** | `-Ml` | Network service | Network-based analysis |
| **EXECVE** | `-Me` | Direct execution | Low-overhead execution |

### 9. Additional Security Features

**Capabilities**:
```bash
--keep_caps           # Keep all capabilities (dangerous)
--cap CAP_PTRACE      # Keep specific capability (can repeat)
```

**Process Personality**:
```bash
--persona_addr_no_randomize  # Disable ASLR (for debugging)
--disable_tsc                # Disable timestamp counter
```

**Logging**:
```bash
--log_fd 2                # Log to stderr
--log_file /tmp/jail.log  # Log to file
--log_level DEBUG         # Log level (DEBUG/INFO/WARNING/ERROR/FATAL)
```

---

## Recommended Configuration for Malware Analysis

### Configuration File Format

nsjail uses **Protocol Buffer** format (`.cfg` or `.json`).

### Malware Sandbox Configuration

File: `/etc/sentinel/nsjail-malware-sandbox.cfg`

```protobuf
name: "Sentinel Malware Sandbox"
description: "Tier 2 behavioral analysis sandbox for Ladybird Sentinel"

mode: ONCE
hostname: "malware-sandbox"
cwd: "/sandbox"

time_limit: 30
log_level: INFO

# Namespace isolation
clone_newnet: true
clone_newuser: true
clone_newns: true
clone_newpid: true
clone_newipc: true
clone_newuts: true
clone_newcgroup: true

# User mapping (unprivileged)
uidmap {
    inside_id: "0"
    outside_id: "1000"
    count: 1
}
gidmap {
    inside_id: "0"
    outside_id: "1000"
    count: 1
}

# Filesystem isolation
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
    src: "/bin"
    dst: "/bin"
    is_bind: true
    rw: false
}
mount {
    src: "/usr"
    dst: "/usr"
    is_bind: true
    rw: false
}
mount {
    dst: "/tmp"
    fstype: "tmpfs"
    rw: true
    noexec: true
    nodev: true
    nosuid: true
}
mount {
    dst: "/sandbox"
    fstype: "tmpfs"
    rw: true
    noexec: false  # Allow execution for analysis
    nodev: true
    nosuid: true
}
mount {
    src: "/dev/null"
    dst: "/dev/null"
    is_bind: true
    rw: true
}
mount {
    dst: "/proc"
    fstype: "proc"
    rw: false
}

# Resource limits
rlimit_as: 512
rlimit_as_type: VALUE
rlimit_cpu: 30
rlimit_cpu_type: VALUE
rlimit_fsize: 10
rlimit_fsize_type: VALUE
rlimit_nofile: 64
rlimit_nofile_type: VALUE
rlimit_nproc_type: SOFT

# Cgroup limits
cgroup_mem_max: 512
cgroup_pids_max: 50
cgroup_cpu_ms_per_sec: 800  # 80% of one core

# Seccomp policy
seccomp_policy_file: "/etc/sentinel/malware-sandbox.kafel"
seccomp_log: true

# Disable capabilities
keep_caps: false

# Network isolation (no network access)
iface_no_lo: true

# Execution
exec_bin {
    path: "/bin/sh"
    arg: "-c"
}
```

### Seccomp Policy File

File: `/etc/sentinel/malware-sandbox.kafel`

```kafel
# Kafel seccomp policy for malware analysis
# Allows common operations but logs suspicious activity

POLICY malware_analysis {
    # === File Operations (ALLOW) ===
    ALLOW {
        # File descriptors
        read, write, readv, writev, pread64, pwrite64,
        open, openat, openat2, close, creat,
        stat, fstat, lstat, newfstatat, statx,
        lseek, dup, dup2, dup3, fcntl,

        # Directory operations
        getdents, getdents64, readdir, mkdir, mkdirat,
        rmdir, unlink, unlinkat, rename, renameat,

        # File attributes
        access, faccessat, chmod, fchmod, fchmodat,
        chown, fchown, lchown, fchownat,

        # Links
        readlink, readlinkat, symlink, symlinkat,
        link, linkat
    }

    # === Memory Operations (ALLOW) ===
    ALLOW {
        mmap, munmap, mprotect, mremap, madvise,
        brk, mlock, munlock, mlockall, munlockall
    }

    # === Process Operations (LOG - Suspicious) ===
    LOG {
        # Process creation
        fork, vfork, clone, clone3, execve, execveat,

        # Process debugging
        ptrace, process_vm_readv, process_vm_writev,

        # Process information
        getpid, getppid, gettid, getuid, getgid,
        geteuid, getegid, getpgid, getpgrp, getsid
    }

    # === Network Operations (LOG - Suspicious) ===
    LOG {
        socket, socketpair, bind, connect, listen, accept, accept4,
        sendto, recvfrom, sendmsg, recvmsg, sendmmsg, recvmmsg,
        shutdown, setsockopt, getsockopt, getpeername, getsockname
    }

    # === Signals (ALLOW) ===
    ALLOW {
        rt_sigaction, rt_sigprocmask, rt_sigreturn,
        rt_sigpending, rt_sigsuspend, rt_sigtimedwait,
        sigaltstack, kill, tkill, tgkill
    }

    # === Time Operations (ALLOW) ===
    ALLOW {
        clock_gettime, clock_getres, clock_nanosleep,
        gettimeofday, time, nanosleep
    }

    # === System Information (ALLOW) ===
    ALLOW {
        uname, sysinfo, getrusage, times, getrlimit, setrlimit
    }

    # === IPC (LOG - Suspicious) ===
    LOG {
        # Shared memory
        shmget, shmat, shmdt, shmctl,

        # Message queues
        msgget, msgsnd, msgrcv, msgctl,

        # Semaphores
        semget, semop, semctl
    }

    # === Threading (ALLOW) ===
    ALLOW {
        set_tid_address, set_robust_list, get_robust_list,
        futex, futex_waitv
    }

    # === Exit (ALLOW) ===
    ALLOW {
        exit, exit_group, wait4, waitid, waitpid
    }

    # === Dangerous Operations (DENY with EPERM) ===
    ERRNO(EPERM) {
        # Kernel module operations
        init_module, finit_module, delete_module,

        # System reboot/shutdown
        reboot, kexec_load, kexec_file_load,

        # Direct I/O access
        iopl, ioperm, ioprio_set,

        # Privileged operations
        mount, umount, umount2, pivot_root, chroot,
        swapon, swapoff, sethostname, setdomainname,

        # Security operations
        setuid, setgid, setreuid, setregid,
        setresuid, setresgid, setfsuid, setfsgid,
        capset, acct
    }
}

# Default action for unlisted syscalls
DEFAULT KILL
```

### Command-Line Usage

**Analyze single file**:
```bash
# Copy file to analysis directory
cp suspicious_file.bin /var/sentinel/analysis/file_to_analyze

# Run analysis
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /var/sentinel/analysis:/sandbox \
    -- /sandbox/file_to_analyze
```

**Analyze with specific timeout**:
```bash
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --time_limit 60 \
    --bindmount /var/sentinel/analysis:/sandbox \
    -- /sandbox/file_to_analyze
```

**Capture output**:
```bash
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --log_file /var/sentinel/logs/analysis_$(date +%s).log \
    --bindmount /var/sentinel/analysis:/sandbox \
    -- /sandbox/file_to_analyze 2>&1 | tee /var/sentinel/output.txt
```

---

## Syscall Monitoring Approach

### Challenge

nsjail provides **syscall filtering** (allow/deny) but **not detailed syscall logging**.

### Solutions

#### Option 1: Seccomp Logging (Kernel 4.14+)

**Enable in nsjail config**:
```protobuf
seccomp_log: true
```

**Log actions in Kafel policy**:
```kafel
LOG {
    socket, connect, execve, fork, ptrace
}
```

**View logs**:
```bash
# View kernel audit logs
sudo ausearch -m SECCOMP

# View via dmesg
dmesg | grep SECCOMP
```

**Limitations**:
- Requires `auditd` or kernel log access
- Only logs syscalls marked with `LOG` action
- No argument details captured

#### Option 2: strace Integration (Recommended)

**Run nsjail under strace**:
```bash
strace -f -o /var/sentinel/strace.log \
    nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /var/sentinel/analysis:/sandbox \
    -- /sandbox/file_to_analyze
```

**Parse strace output**:
```cpp
// Services/Sentinel/BehavioralAnalyzer.cpp
ErrorOr<BehavioralReport> BehavioralAnalyzer::parse_strace_log(StringView log_path)
{
    auto file = TRY(Core::File::open(log_path, Core::File::OpenMode::Read));
    auto buffered = TRY(Core::InputBufferedFile::create(move(file)));

    Vector<SyscallEvent> events;

    while (auto line = TRY(buffered->read_line())) {
        // Parse: PID syscall(args) = result <time>
        // Example: 1234 openat(AT_FDCWD, "/etc/passwd", O_RDONLY) = 3 <0.000123>

        auto event = TRY(parse_strace_line(line));
        events.append(move(event));
    }

    return BehavioralReport { move(events) };
}
```

**Advantages**:
- Full syscall argument capture
- Process tree tracking (`-f` flag)
- Timestamping
- Cross-reference with seccomp denials

**Disadvantages**:
- **100x+ performance overhead** (significant)
- Increases analysis time
- May trigger anti-debugging checks in malware

#### Option 3: ptrace-Based Custom Monitor

**Implement custom tracer**:
```cpp
// Services/Sentinel/SyscallMonitor.cpp
class SyscallMonitor {
public:
    static ErrorOr<NonnullOwnPtr<SyscallMonitor>> create(pid_t target_pid);

    ErrorOr<void> attach();
    ErrorOr<void> monitor_loop();

private:
    ErrorOr<void> handle_syscall_enter(pid_t pid);
    ErrorOr<void> handle_syscall_exit(pid_t pid);

    pid_t m_target_pid;
    Vector<SyscallEvent> m_events;
};

ErrorOr<void> SyscallMonitor::attach()
{
    if (ptrace(PTRACE_ATTACH, m_target_pid, nullptr, nullptr) == -1)
        return Error::from_errno(errno);

    int status;
    waitpid(m_target_pid, &status, 0);

    // Set options
    ptrace(PTRACE_SETOPTIONS, m_target_pid, 0, PTRACE_O_TRACESYSGOOD);

    return {};
}

ErrorOr<void> SyscallMonitor::monitor_loop()
{
    while (true) {
        ptrace(PTRACE_SYSCALL, m_target_pid, 0, 0);

        int status;
        waitpid(m_target_pid, &status, 0);

        if (WIFEXITED(status))
            break;

        if (status >> 8 == (SIGTRAP | 0x80)) {
            // Syscall event
            TRY(handle_syscall_enter(m_target_pid));
        }
    }

    return {};
}
```

**Advantages**:
- Granular control over monitoring
- Can modify syscall arguments/return values
- Integration with nsjail

**Disadvantages**:
- Complex implementation
- Still has performance overhead (50-100x)
- Requires ptrace capability

#### Option 4: eBPF-Based Monitoring (Future)

**Use eBPF for kernel-level tracing**:
```bash
# Install BCC tools
sudo apt install bpftrace

# Trace syscalls for specific PID
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_* /pid == $1/ { printf("%s\n", probe); }' PID
```

**Integration**:
```cpp
// Use libbpf to attach eBPF programs
// Collect events via perf ring buffer
```

**Advantages**:
- **Minimal overhead** (< 5%)
- Kernel-level visibility
- No ptrace required

**Disadvantages**:
- Requires root or `CAP_BPF` capability
- Complex setup
- Kernel version dependency (5.8+)

### Recommended Approach for Sentinel

**Hybrid Strategy**:

1. **Primary**: Use **seccomp LOG** for high-level indicators
   - Log network, process creation, IPC syscalls
   - Minimal overhead
   - Sufficient for threat scoring

2. **Fallback**: Use **strace** for detailed forensics
   - Only when suspicious behavior detected
   - Full argument logging
   - Accept performance overhead for deep analysis

3. **Future**: Migrate to **eBPF** for production
   - Implement in Phase 3 (optimization)
   - Low overhead, high visibility

---

## Integration Strategy with BehavioralAnalyzer

### Architecture

```
BehavioralAnalyzer (Services/Sentinel/)
    ├── prepare_sandbox()          # Create isolated environment
    ├── launch_nsjail()            # Spawn nsjail with config
    ├── monitor_execution()        # Track syscalls, file ops
    ├── collect_artifacts()        # Gather logs, modified files
    ├── analyze_behavior()         # Score behavioral patterns
    └── generate_report()          # Create BehavioralReport

Integration Points:
    ├── FileDescriptor::create_pipe() → stdin/stdout/stderr capture
    ├── Core::Process::spawn()    → nsjail execution
    ├── Core::File::open()         → strace log parsing
    └── PolicyGraph               → Store behavioral signatures
```

### Implementation Outline

#### Phase 1: Basic nsjail Integration

**File**: `Services/Sentinel/BehavioralAnalyzer.h`
```cpp
#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

struct SyscallEvent {
    String syscall_name;
    Vector<String> arguments;
    i32 return_value;
    u64 timestamp_ns;
};

struct FileOperation {
    String operation;  // open, read, write, unlink
    String path;
    String flags;
};

struct NetworkOperation {
    String operation;  // socket, connect, bind, sendto
    String address;
    u16 port;
    String protocol;
};

struct ProcessOperation {
    String operation;  // fork, execve, clone
    pid_t child_pid;
    String command;
};

struct BehavioralReport {
    Vector<SyscallEvent> syscalls;
    Vector<FileOperation> file_ops;
    Vector<NetworkOperation> network_ops;
    Vector<ProcessOperation> process_ops;

    // Behavioral indicators
    bool attempted_network_access { false };
    bool attempted_process_creation { false };
    bool attempted_file_modification { false };
    bool attempted_privileged_escalation { false };

    // Threat score (0-100)
    u8 threat_score { 0 };
};

class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(StringView nsjail_path, StringView config_path);

    // Analyze file in sandbox
    ErrorOr<BehavioralReport> analyze_file(StringView file_path, u32 timeout_seconds = 30);

private:
    BehavioralAnalyzer(String nsjail_path, String config_path);

    ErrorOr<String> prepare_sandbox(StringView file_path);
    ErrorOr<pid_t> launch_nsjail(StringView sandbox_path, StringView file_path);
    ErrorOr<void> monitor_execution(pid_t nsjail_pid);
    ErrorOr<BehavioralReport> collect_and_analyze();

    String m_nsjail_path;
    String m_config_path;
    String m_strace_log_path;
};

}
```

**File**: `Services/Sentinel/BehavioralAnalyzer.cpp`
```cpp
#include "BehavioralAnalyzer.h"
#include <AK/Format.h>
#include <LibCore/File.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> BehavioralAnalyzer::create(
    StringView nsjail_path, StringView config_path)
{
    return adopt_nonnull_own_or_enomem(new (nothrow) BehavioralAnalyzer(
        TRY(String::from_utf8(nsjail_path)),
        TRY(String::from_utf8(config_path))
    ));
}

BehavioralAnalyzer::BehavioralAnalyzer(String nsjail_path, String config_path)
    : m_nsjail_path(move(nsjail_path))
    , m_config_path(move(config_path))
{
}

ErrorOr<BehavioralReport> BehavioralAnalyzer::analyze_file(
    StringView file_path, u32 timeout_seconds)
{
    // 1. Prepare sandbox directory
    auto sandbox_path = TRY(prepare_sandbox(file_path));

    // 2. Launch nsjail with strace
    auto nsjail_pid = TRY(launch_nsjail(sandbox_path, file_path));

    // 3. Monitor execution
    TRY(monitor_execution(nsjail_pid));

    // 4. Collect artifacts and analyze
    auto report = TRY(collect_and_analyze());

    // 5. Cleanup
    TRY(Core::System::rmdir(sandbox_path));

    return report;
}

ErrorOr<String> BehavioralAnalyzer::prepare_sandbox(StringView file_path)
{
    // Create temporary sandbox directory
    auto sandbox_path = TRY(String::formatted("/var/sentinel/sandbox/{}", getpid()));
    TRY(Core::System::mkdir(sandbox_path, 0700));

    // Copy file to sandbox
    auto dest_path = TRY(String::formatted("{}/file_to_analyze", sandbox_path));
    TRY(Core::System::copy_file_or_directory(dest_path, file_path,
        Core::System::RecursionMode::Disallowed,
        Core::System::LinkMode::Disallowed,
        Core::System::AddDuplicateFileMarker::No));

    // Set executable permissions
    TRY(Core::System::chmod(dest_path, 0700));

    return sandbox_path;
}

ErrorOr<pid_t> BehavioralAnalyzer::launch_nsjail(StringView sandbox_path, StringView file_path)
{
    // Prepare strace log path
    m_strace_log_path = TRY(String::formatted("/var/sentinel/logs/strace_{}.log", getpid()));

    // Build nsjail command
    Vector<StringView> args {
        "strace"sv,
        "-f"sv,  // Follow forks
        "-o"sv, m_strace_log_path.bytes_as_string_view(),
        m_nsjail_path.bytes_as_string_view(),
        "--config"sv, m_config_path.bytes_as_string_view(),
        "--bindmount"sv, TRY(String::formatted("{}:/sandbox", sandbox_path)).bytes_as_string_view(),
        "--"sv,
        "/sandbox/file_to_analyze"sv
    };

    // Spawn process
    auto process = TRY(Core::Process::spawn(args[0], args, {}, {}));

    return process.pid();
}

ErrorOr<void> BehavioralAnalyzer::monitor_execution(pid_t nsjail_pid)
{
    int status;
    waitpid(nsjail_pid, &status, 0);

    if (WIFEXITED(status)) {
        dbgln("nsjail exited with status {}", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        dbgln("nsjail killed by signal {}", WTERMSIG(status));
    }

    return {};
}

ErrorOr<BehavioralReport> BehavioralAnalyzer::collect_and_analyze()
{
    BehavioralReport report;

    // Parse strace log
    auto file = TRY(Core::File::open(m_strace_log_path, Core::File::OpenMode::Read));
    auto buffered = TRY(Core::InputBufferedFile::create(move(file)));

    while (auto line = TRY(buffered->read_line())) {
        // Parse syscall format: PID syscall(args) = result <time>
        // Example: 1234 openat(AT_FDCWD, "/etc/passwd", O_RDONLY) = 3 <0.000123>

        // Extract syscall name
        auto syscall_start = line.find('(');
        if (!syscall_start.has_value())
            continue;

        auto syscall_name = line.substring_view(0, syscall_start.value());
        auto trimmed_name = syscall_name.trim_whitespace();

        // Check for suspicious patterns
        if (trimmed_name == "socket"sv || trimmed_name == "connect"sv) {
            report.attempted_network_access = true;
        } else if (trimmed_name == "execve"sv || trimmed_name == "fork"sv || trimmed_name == "clone"sv) {
            report.attempted_process_creation = true;
        } else if (trimmed_name == "unlink"sv || trimmed_name == "write"sv) {
            report.attempted_file_modification = true;
        }

        // TODO: More sophisticated parsing and pattern matching
    }

    // Calculate threat score based on indicators
    u8 score = 0;
    if (report.attempted_network_access)
        score += 30;
    if (report.attempted_process_creation)
        score += 20;
    if (report.attempted_file_modification)
        score += 15;
    if (report.attempted_privileged_escalation)
        score += 35;

    report.threat_score = score;

    return report;
}

}
```

#### Phase 2: PolicyGraph Integration

Store behavioral signatures:
```cpp
// Store behavioral signature
m_policy_graph->store_behavioral_signature(
    file_hash,
    report.threat_score,
    report.syscalls,
    report.network_ops
);

// Check for known malware patterns
if (auto signature = m_policy_graph->lookup_behavioral_signature(file_hash)) {
    // Compare with known patterns
}
```

#### Phase 3: Advanced Monitoring

Migrate to eBPF for production:
```cpp
class eBPFSyscallMonitor {
public:
    static ErrorOr<NonnullOwnPtr<eBPFSyscallMonitor>> create();
    ErrorOr<void> attach_to_pid(pid_t pid);
    ErrorOr<Vector<SyscallEvent>> collect_events();
};
```

---

## Performance Considerations

### Overhead Analysis

| Component | Overhead | Impact | Mitigation |
|-----------|----------|--------|------------|
| **Namespaces** | < 1% | Minimal | None needed |
| **Seccomp-BPF** | < 1% | Minimal | None needed |
| **Cgroups** | < 5% | Low | Acceptable for analysis |
| **Bind mounts** | < 1% | Minimal | Cache mount configurations |
| **strace** | 100-200x | **Critical** | Use selectively |
| **ptrace** | 50-100x | High | Avoid in production |
| **eBPF** | < 5% | Low | Future migration target |

### Optimization Strategies

#### 1. Lazy strace Activation

```cpp
// Only enable strace for high-risk files
ErrorOr<BehavioralReport> BehavioralAnalyzer::analyze_file(StringView file_path, bool enable_strace)
{
    if (enable_strace) {
        // Detailed monitoring with strace
        return analyze_with_strace(file_path);
    } else {
        // Fast analysis with seccomp logging only
        return analyze_with_seccomp_log(file_path);
    }
}
```

#### 2. Timeout Configuration

```cpp
// Aggressive timeout for untrusted files
nsjail --time_limit 10  # 10 seconds for quick analysis

// Longer timeout for whitelisted sources
nsjail --time_limit 60  # 60 seconds for thorough analysis
```

#### 3. Resource Limit Tuning

```cpp
// Minimal resources for initial scan
rlimit_as: 128         # 128 MB
rlimit_cpu: 5          # 5 seconds CPU
cgroup_mem_max: 256    # 256 MB total

// Escalate resources if needed
if (requires_deeper_analysis) {
    rlimit_as: 512
    rlimit_cpu: 30
    cgroup_mem_max: 1024
}
```

#### 4. Parallel Analysis

```cpp
// Analyze multiple files concurrently
Vector<NonnullOwnPtr<BehavioralAnalyzer>> analyzers;
for (auto& file : suspicious_files) {
    auto analyzer = TRY(BehavioralAnalyzer::create(nsjail_path, config_path));
    // Spawn analysis in separate process
}
```

### Benchmark Data (Estimated)

| Configuration | Execution Time | CPU Usage | Memory |
|---------------|----------------|-----------|--------|
| **Bare nsjail** (no strace) | 0.05s overhead | 2% | 10 MB |
| **nsjail + seccomp log** | 0.08s overhead | 3% | 12 MB |
| **nsjail + strace** | 5-10s overhead | 100% | 50 MB |
| **nsjail + eBPF** (future) | 0.1s overhead | 5% | 15 MB |

**Recommendation**: Start with **seccomp logging only**, escalate to **strace** for suspicious files.

---

## Known Limitations and Workarounds

### 1. User Namespace Restrictions

**Problem**: Some distributions disable unprivileged user namespaces for security.

**Detection**:
```bash
# Check if unprivileged user namespaces are enabled
sysctl kernel.unprivileged_userns_clone
```

**Workaround**:
```bash
# Enable unprivileged user namespaces (Ubuntu/Debian)
sudo sysctl -w kernel.unprivileged_userns_clone=1

# Make permanent
echo "kernel.unprivileged_userns_clone=1" | sudo tee -a /etc/sysctl.conf

# Alternative: Run nsjail as root (not recommended)
sudo nsjail ...
```

### 2. AppArmor/SELinux Conflicts

**Problem**: Security modules may block namespace creation.

**Detection**:
```bash
# Check AppArmor status
sudo aa-status

# Check SELinux status
getenforce
```

**Workaround**:
```bash
# Temporarily disable AppArmor for nsjail
sudo aa-complain /usr/local/bin/nsjail

# Or create AppArmor profile
sudo nano /etc/apparmor.d/usr.local.bin.nsjail
```

### 3. Cgroup v2 Compatibility

**Problem**: Modern distributions use cgroup v2, which has different API.

**Detection**:
```bash
# Check cgroup version
grep cgroup /proc/filesystems
```

**Workaround**:
```protobuf
# In nsjail config
use_cgroupv2: true
cgroupv2_mount: "/sys/fs/cgroup"
```

### 4. Filesystem Pivot Failures

**Problem**: `pivot_root` may fail on some filesystems.

**Workaround**:
```protobuf
# In nsjail config
no_pivotroot: true  # Use chroot instead
```

### 5. Nested Sandboxing

**Problem**: Running nsjail inside Docker/containers may fail.

**Workaround**:
```bash
# Run Docker with privileged mode
docker run --privileged ...

# Or grant specific capabilities
docker run --cap-add=SYS_ADMIN --cap-add=SYS_CHROOT ...
```

### 6. strace Performance Impact

**Problem**: strace slows execution by 100-200x.

**Workaround**:
- Use strace selectively (only for high-risk files)
- Filter syscalls: `strace -e trace=network,process,file`
- Consider eBPF alternative (future)

### 7. No Built-in Syscall Logging

**Problem**: nsjail only filters syscalls, doesn't log arguments.

**Workaround**:
- Use seccomp `LOG` action (limited visibility)
- Integrate strace (high overhead)
- Implement ptrace monitor (medium overhead)
- **Recommended**: Migrate to eBPF (low overhead, future)

### 8. Kernel Version Dependencies

**Problem**: Old kernels lack namespace/seccomp features.

**Requirements**:
- Linux 3.8+ (user namespaces)
- Linux 4.6+ (cgroup namespaces)
- Linux 4.14+ (seccomp logging)

**Workaround**: Upgrade kernel or run in Docker with newer kernel.

---

## Alternative Technologies

### Comparison Table

| Feature | **nsjail** | **Firejail** | **Bubblewrap** | **Docker** |
|---------|-----------|-------------|----------------|------------|
| **Purpose** | Process isolation | Desktop app sandboxing | Flatpak sandboxing | Containerization |
| **Namespaces** | All (UTS, MOUNT, PID, IPC, NET, USER, CGROUP, TIME) | All | All | All |
| **Seccomp** | ✅ Kafel DSL | ✅ Profiles | ✅ JSON | ✅ JSON |
| **Cgroups** | ✅ v1 + v2 | ⚠️ Limited | ⚠️ Limited | ✅ v1 + v2 |
| **Resource Limits** | ✅ RLimits + cgroups | ✅ RLimits | ⚠️ RLimits only | ✅ Cgroups |
| **Configuration** | Protobuf files | Profiles (.profile) | CLI only | Dockerfile |
| **Ease of Use** | Medium | Easy (pre-built profiles) | Easy (simple CLI) | Easy |
| **Flexibility** | ✅ Highly flexible | ⚠️ Profile-based | ⚠️ Limited | ✅ Flexible |
| **Performance** | Fast | Fast | **Fastest** | Medium |
| **Network Isolation** | ✅ Full + MACVLAN | ✅ Full | ✅ Full | ✅ Full + bridge |
| **SUID Binary** | ❌ No | ⚠️ Yes (security risk) | ❌ No | ❌ No (daemon) |
| **Maintenance** | ✅ Google-backed | ⚠️ Small team | ✅ Red Hat-backed | ✅ Docker Inc. |
| **Malware Analysis** | ✅ **Excellent** | ⚠️ Acceptable | ⚠️ Limited | ✅ Good |
| **Integration Complexity** | Low | Medium | **Very Low** | Medium-High |

### Detailed Comparison

#### **Firejail**

**Pros**:
- Pre-built profiles for common applications (Firefox, Chrome, etc.)
- User-friendly (just `firejail firefox`)
- AppArmor integration
- Desktop-oriented (X11, Wayland, PulseAudio support)

**Cons**:
- **SUID binary** (historical security vulnerabilities)
- Less flexible for custom workloads
- Profile-based approach (less fine-grained control)
- Not designed for malware analysis

**Use Case**: Desktop application sandboxing (not recommended for Sentinel)

#### **Bubblewrap**

**Pros**:
- **Simplest** implementation (minimal code, easy to audit)
- No SUID (security advantage)
- Used by Flatpak (battle-tested)
- Very fast

**Cons**:
- **No configuration files** (CLI-only)
- **No cgroup support** (only rlimits)
- **No seccomp DSL** (must provide JSON)
- Limited flexibility

**Use Case**: Embedding in other tools (like Flatpak), simple sandboxing

**Example**:
```bash
bwrap --ro-bind /usr /usr --tmpfs /tmp --proc /proc --dev /dev \
    --unshare-all --die-with-parent \
    /bin/bash
```

#### **Docker**

**Pros**:
- Industry-standard containerization
- Rich ecosystem (images, registries)
- Built-in networking, volumes, orchestration
- Well-documented

**Cons**:
- **Heavyweight** (daemon required, overlay filesystem)
- **Higher overhead** than nsjail
- Designed for services, not single-process analysis
- More complex setup

**Use Case**: Long-running services, complex multi-container setups

**Example**:
```bash
docker run --rm -it --network=none --memory=512m --cpus=0.5 \
    -v /path/to/file:/file:ro \
    alpine:latest /file
```

### Recommendation Matrix

| Use Case | Recommended Tool | Reason |
|----------|-----------------|--------|
| **Malware Analysis** | **nsjail** | Best flexibility, seccomp DSL, resource limits |
| **Desktop App** | **Firejail** | Pre-built profiles, X11/Wayland support |
| **Flatpak Alternative** | **Bubblewrap** | Lightweight, simple, no SUID |
| **Service Deployment** | **Docker** | Orchestration, networking, persistence |
| **CTF/Pwnable Hosting** | **nsjail** | Network inetd mode, time limits |

### Why nsjail for Sentinel?

1. **Kafel DSL**: Expressive syscall filtering without JSON verbosity
2. **Cgroup Support**: Full resource control (CPU, memory, PIDs)
3. **Configuration Files**: Protobuf format for versioned configs
4. **Flexibility**: Supports arbitrary workloads (not app-specific)
5. **Battle-Tested**: Used by Google for CTF challenges (kCTF)
6. **No SUID**: More secure than Firejail
7. **Active Development**: Regular updates and improvements

---

## Recommendations

### Immediate Actions (Phase 2 Implementation)

1. **Install nsjail** on development systems:
   ```bash
   cd /tmp
   git clone https://github.com/google/nsjail.git
   cd nsjail
   make -j$(nproc)
   sudo cp nsjail /usr/local/bin/
   ```

2. **Create malware sandbox config**:
   ```bash
   sudo mkdir -p /etc/sentinel
   sudo cp docs/configs/malware-sandbox.cfg /etc/sentinel/nsjail-malware-sandbox.cfg
   sudo cp docs/configs/malware-sandbox.kafel /etc/sentinel/malware-sandbox.kafel
   ```

3. **Implement BehavioralAnalyzer** skeleton:
   ```bash
   touch Services/Sentinel/BehavioralAnalyzer.{h,cpp}
   # Add to Services/Sentinel/CMakeLists.txt
   ```

4. **Test basic integration**:
   ```cpp
   // Services/Sentinel/TestBehavioralAnalyzer.cpp
   auto analyzer = TRY(BehavioralAnalyzer::create("/usr/local/bin/nsjail",
                                                   "/etc/sentinel/nsjail-malware-sandbox.cfg"));
   auto report = TRY(analyzer->analyze_file("/tmp/test_file"));
   EXPECT(report.threat_score < 50);
   ```

### Short-Term Optimizations (Phase 2-3)

1. **Implement seccomp-log parser**:
   - Parse `/var/log/audit/audit.log` for SECCOMP events
   - Extract syscall names and PIDs
   - Correlate with nsjail executions

2. **Add selective strace**:
   - Only enable strace for files scoring > 30 in Tier 1
   - Parse strace output for detailed forensics

3. **Create behavioral signature database**:
   - Store common malware patterns in PolicyGraph
   - Compare new samples against known signatures

### Long-Term Enhancements (Phase 4+)

1. **Migrate to eBPF**:
   - Implement eBPF-based syscall monitoring
   - Reduce overhead from 100x to < 5%
   - Enable production-scale analysis

2. **Distributed analysis**:
   - Run nsjail on dedicated analysis nodes
   - Queue files for analysis
   - Return results asynchronously

3. **GPU-accelerated ML**:
   - Combine behavioral analysis with ML models
   - Use GPU for feature extraction
   - Real-time threat classification

### Security Best Practices

1. **Principle of Least Privilege**:
   - Run nsjail as unprivileged user (not root)
   - Use user namespaces for UID/GID mapping
   - Drop all capabilities (`keep_caps: false`)

2. **Defense in Depth**:
   - Enable all namespaces (NET, PID, IPC, UTS, CGROUP)
   - Use seccomp-bpf with default-deny policy
   - Enforce resource limits (CPU, memory, processes)

3. **Isolation Boundaries**:
   - Network isolation (no network access by default)
   - Filesystem isolation (read-only system directories)
   - Process isolation (separate PID namespace)

4. **Monitoring and Alerting**:
   - Log all sandbox escapes to PolicyGraph
   - Alert on unexpected seccomp violations
   - Track resource exhaustion attempts

5. **Regular Updates**:
   - Update nsjail from upstream regularly
   - Review and update Kafel policies
   - Test against new malware samples

### Testing Strategy

1. **Unit Tests**:
   ```bash
   # Test nsjail spawning
   ./Build/release/bin/TestBehavioralAnalyzer
   ```

2. **Integration Tests**:
   ```bash
   # Test with benign files
   ./test_behavioral_analyzer /bin/ls

   # Test with simulated malware
   ./test_behavioral_analyzer /tmp/fake_malware.sh
   ```

3. **Regression Tests**:
   ```bash
   # Test against known malware samples (quarantined)
   for sample in /var/sentinel/quarantine/*; do
       ./test_behavioral_analyzer "$sample"
   done
   ```

4. **Performance Tests**:
   ```bash
   # Benchmark analysis time
   hyperfine './analyze_with_strace /tmp/file' './analyze_without_strace /tmp/file'
   ```

### Documentation Updates

1. **User Guide** (`docs/SENTINEL_USER_GUIDE.md`):
   - Add Tier 2 behavioral analysis section
   - Explain threat score interpretation
   - Provide troubleshooting steps

2. **Architecture Guide** (`docs/SENTINEL_ARCHITECTURE.md`):
   - Document nsjail integration
   - Diagram BehavioralAnalyzer workflow
   - Explain syscall monitoring approach

3. **Developer Guide** (`CLAUDE.md`):
   - Add BehavioralAnalyzer API documentation
   - Provide example usage
   - Include testing commands

---

## Appendices

### Appendix A: Complete Example Configuration

**File**: `/etc/sentinel/nsjail-malware-sandbox.cfg`

```protobuf
name: "Sentinel Malware Sandbox - Production"
description: "Tier 2 behavioral analysis sandbox for Ladybird Sentinel (nsjail + strace)"

# Execution mode
mode: ONCE
hostname: "malware-sandbox"
cwd: "/sandbox"

# Time limits
time_limit: 30
daemon: false

# Logging
log_level: WARNING
log_file: "/var/sentinel/logs/nsjail.log"

# Namespace isolation (all enabled)
clone_newnet: true
clone_newuser: true
clone_newns: true
clone_newpid: true
clone_newipc: true
clone_newuts: true
clone_newcgroup: true

# User/group mapping (unprivileged)
uidmap {
    inside_id: "0"
    outside_id: "1000"
    count: 1
}
gidmap {
    inside_id: "0"
    outside_id: "1000"
    count: 1
}

# Filesystem isolation
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
    mandatory: false  # May not exist on all systems
}
mount {
    src: "/lib32"
    dst: "/lib32"
    is_bind: true
    rw: false
    mandatory: false
}
mount {
    src: "/bin"
    dst: "/bin"
    is_bind: true
    rw: false
}
mount {
    src: "/sbin"
    dst: "/sbin"
    is_bind: true
    rw: false
}
mount {
    src: "/usr"
    dst: "/usr"
    is_bind: true
    rw: false
}
mount {
    dst: "/tmp"
    fstype: "tmpfs"
    rw: true
    noexec: true
    nodev: true
    nosuid: true
}
mount {
    dst: "/sandbox"
    fstype: "tmpfs"
    rw: true
    noexec: false
    nodev: true
    nosuid: true
}
mount {
    src: "/dev/null"
    dst: "/dev/null"
    is_bind: true
    rw: true
}
mount {
    src: "/dev/zero"
    dst: "/dev/zero"
    is_bind: true
    rw: true
}
mount {
    src: "/dev/urandom"
    dst: "/dev/urandom"
    is_bind: true
    rw: true
}
mount {
    dst: "/proc"
    fstype: "proc"
    rw: false
}

# Resource limits (rlimits)
rlimit_as: 512
rlimit_as_type: VALUE
rlimit_core: 0
rlimit_core_type: VALUE
rlimit_cpu: 30
rlimit_cpu_type: VALUE
rlimit_fsize: 10
rlimit_fsize_type: VALUE
rlimit_nofile: 64
rlimit_nofile_type: VALUE
rlimit_nproc_type: SOFT
rlimit_stack: 8
rlimit_stack_type: VALUE

# Cgroup limits
cgroup_mem_max: 512
cgroup_mem_memsw_max: 768
cgroup_pids_max: 50
cgroup_cpu_ms_per_sec: 800

# Seccomp policy
seccomp_policy_file: "/etc/sentinel/malware-sandbox.kafel"
seccomp_log: true

# Capabilities (drop all)
keep_caps: false

# Network isolation (no network access)
iface_no_lo: true

# Disable new privileges
disable_no_new_privs: false

# Process personality
persona_addr_no_randomize: false  # Keep ASLR enabled
disable_tsc: true  # Disable timestamp counter

# Execution
exec_bin {
    path: "/bin/sh"
    arg: "-c"
}
```

### Appendix B: Complete Kafel Policy

**File**: `/etc/sentinel/malware-sandbox.kafel`

```kafel
# ===============================================================
# Sentinel Malware Sandbox - Seccomp Policy
# ===============================================================
# This policy allows common file/memory operations while logging
# suspicious activities (network, process creation, IPC).
# Default action: KILL (unlisted syscalls terminate the process)
# ===============================================================

POLICY malware_analysis {
    # ===========================
    # File Operations (ALLOW)
    # ===========================
    ALLOW {
        # File descriptors
        read, write, readv, writev, pread64, pwrite64,
        preadv, pwritev, preadv2, pwritev2,
        open, openat, openat2, close, creat,

        # File status
        stat, fstat, lstat, newfstatat, statx,
        lseek, dup, dup2, dup3, fcntl, fcntl64,
        ioctl,  # Limited ioctl (may need filtering)

        # Directory operations
        getdents, getdents64, readdir,
        mkdir, mkdirat, rmdir,
        unlink, unlinkat,
        rename, renameat, renameat2,

        # File attributes
        access, faccessat, faccessat2,
        chmod, fchmod, fchmodat,
        chown, fchown, lchown, fchownat,
        utime, utimes, utimensat, futimens,

        # Links
        readlink, readlinkat,
        symlink, symlinkat,
        link, linkat
    }

    # ===========================
    # Memory Operations (ALLOW)
    # ===========================
    ALLOW {
        mmap, mmap2, munmap, mremap,
        mprotect, madvise, msync,
        brk, sbrk,
        mlock, mlock2, munlock,
        mlockall, munlockall,
        mincore, remap_file_pages
    }

    # ===========================
    # Process Operations (LOG)
    # ===========================
    LOG {
        # Process creation (suspicious for malware)
        fork, vfork, clone, clone3,
        execve, execveat,

        # Process debugging (highly suspicious)
        ptrace,
        process_vm_readv, process_vm_writev,

        # Process information
        getpid, getppid, gettid,
        getuid, getgid, geteuid, getegid,
        getresuid, getresgid,
        getpgid, setpgid, getpgrp,
        getsid, setsid
    }

    # ===========================
    # Network Operations (LOG)
    # ===========================
    LOG {
        # Socket operations (C&C communication)
        socket, socketpair,
        bind, connect, listen,
        accept, accept4,

        # Data transfer
        sendto, recvfrom,
        sendmsg, recvmsg,
        sendmmsg, recvmmsg,

        # Socket options
        shutdown, setsockopt, getsockopt,
        getpeername, getsockname
    }

    # ===========================
    # Signals (ALLOW)
    # ===========================
    ALLOW {
        rt_sigaction, rt_sigprocmask, rt_sigreturn,
        rt_sigpending, rt_sigsuspend, rt_sigtimedwait,
        rt_sigqueueinfo, rt_tgsigqueueinfo,
        sigaltstack,
        kill, tkill, tgkill,
        pause, sigsuspend, rt_sigsuspend
    }

    # ===========================
    # Time Operations (ALLOW)
    # ===========================
    ALLOW {
        clock_gettime, clock_getres, clock_nanosleep,
        clock_settime, clock_adjtime,
        gettimeofday, settimeofday,
        time, stime,
        nanosleep, clock_nanosleep,
        alarm, setitimer, getitimer,
        timer_create, timer_settime, timer_gettime,
        timer_delete, timer_getoverrun,
        timerfd_create, timerfd_settime, timerfd_gettime
    }

    # ===========================
    # System Information (ALLOW)
    # ===========================
    ALLOW {
        uname, sysinfo, syslog,
        getrusage, times,
        getrlimit, setrlimit, prlimit64,
        getpriority, setpriority,
        sched_getscheduler, sched_setscheduler,
        sched_getparam, sched_setparam,
        sched_get_priority_max, sched_get_priority_min,
        sched_yield, sched_getaffinity, sched_setaffinity,
        getcpu, get_mempolicy
    }

    # ===========================
    # IPC Operations (LOG)
    # ===========================
    LOG {
        # Shared memory (suspicious for IPC)
        shmget, shmat, shmdt, shmctl,

        # Message queues
        msgget, msgsnd, msgrcv, msgctl,

        # Semaphores
        semget, semop, semctl, semtimedop
    }

    # ===========================
    # Threading (ALLOW)
    # ===========================
    ALLOW {
        set_tid_address, gettid,
        set_robust_list, get_robust_list,
        futex, futex_waitv,
        get_thread_area, set_thread_area
    }

    # ===========================
    # Exit Operations (ALLOW)
    # ===========================
    ALLOW {
        exit, exit_group,
        wait4, waitid, waitpid
    }

    # ===========================
    # Polling/Events (ALLOW)
    # ===========================
    ALLOW {
        poll, ppoll, ppoll_time64,
        select, pselect6, pselect6_time64,
        epoll_create, epoll_create1,
        epoll_ctl, epoll_wait, epoll_pwait,
        eventfd, eventfd2
    }

    # ===========================
    # Pipe Operations (ALLOW)
    # ===========================
    ALLOW {
        pipe, pipe2
    }

    # ===========================
    # User/Group Operations (ERRNO)
    # ===========================
    ERRNO(EPERM) {
        # Privilege escalation (blocked)
        setuid, setgid, setreuid, setregid,
        setresuid, setresgid, setfsuid, setfsgid,
        setgroups, getgroups
    }

    # ===========================
    # Dangerous Kernel Operations (ERRNO)
    # ===========================
    ERRNO(EPERM) {
        # Kernel module operations
        init_module, finit_module, delete_module,

        # System reboot/shutdown
        reboot, kexec_load, kexec_file_load,

        # Direct I/O access
        iopl, ioperm, ioprio_set, ioprio_get,

        # Filesystem operations
        mount, umount, umount2, pivot_root, chroot,
        swapon, swapoff,

        # Hostname/domain
        sethostname, setdomainname,

        # Security
        capset, capget,
        acct, quotactl,
        lookup_dcookie, perf_event_open,
        bpf, userfaultfd
    }
}

# ===========================
# Default Action: KILL
# ===========================
# Any syscall not explicitly listed above will terminate the process.
# This ensures a whitelist-based approach for maximum security.
DEFAULT KILL
```

### Appendix C: Usage Examples

**Example 1: Analyze single file**
```bash
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /var/sentinel/analysis/sample1:/sandbox/file_to_analyze \
    -- /sandbox/file_to_analyze
```

**Example 2: Analyze with strace**
```bash
strace -f -o /var/sentinel/logs/strace_sample1.log \
    nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /var/sentinel/analysis/sample1:/sandbox/file_to_analyze \
    -- /sandbox/file_to_analyze
```

**Example 3: Analyze with network monitoring**
```bash
# Enable network namespace but monitor traffic
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --macvlan_iface eth0 \
    --macvlan_vs_ip 10.99.0.2 \
    --macvlan_vs_nm 24 \
    --bindmount /var/sentinel/analysis/sample1:/sandbox/file_to_analyze \
    -- /sandbox/file_to_analyze
```

**Example 4: Programmatic usage**
```cpp
// Services/Sentinel/main.cpp
auto analyzer = TRY(BehavioralAnalyzer::create(
    "/usr/local/bin/nsjail",
    "/etc/sentinel/nsjail-malware-sandbox.cfg"
));

auto report = TRY(analyzer->analyze_file("/tmp/suspicious_file.bin", 30));

if (report.threat_score > 70) {
    dbgln("HIGH THREAT DETECTED: score={}", report.threat_score);
    if (report.attempted_network_access)
        dbgln("  - Attempted network access");
    if (report.attempted_process_creation)
        dbgln("  - Attempted process creation");
}
```

---

## Conclusion

**nsjail is the recommended sandboxing technology** for Sentinel's Tier 2 behavioral analysis due to:

1. **Security**: Multi-layered isolation (namespaces, seccomp, cgroups)
2. **Flexibility**: Expressive Kafel DSL for syscall policies
3. **Performance**: Minimal overhead (< 5%) without strace
4. **Maintainability**: Google-backed, actively developed
5. **Integration**: Simple C++ API via `Core::Process::spawn()`

**Implementation Roadmap**:

- **Phase 2**: Basic nsjail integration with seccomp logging
- **Phase 3**: Add selective strace for detailed forensics
- **Phase 4**: Migrate to eBPF for production-scale monitoring

This research provides all necessary information to proceed with BehavioralAnalyzer implementation.

---

**References**:
- nsjail GitHub: https://github.com/google/nsjail
- Kafel Documentation: https://google.github.io/kafel/
- Linux Namespaces: https://man7.org/linux/man-pages/man7/namespaces.7.html
- Seccomp BPF: https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html
- kCTF (Google CTF using nsjail): https://google.github.io/kctf/

---

**Document Version**: 1.0
**Last Updated**: 2025-11-02
**Status**: Research Complete - Ready for Implementation
