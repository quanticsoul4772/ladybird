/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Orchestrator.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel::Sandbox {

// Inline seccomp-BPF policy for malware sandbox
// This provides a fallback when external Kafel policy file is unavailable.
// Matches malware-sandbox.kafel policy specification.
constexpr StringView INLINE_SECCOMP_POLICY = R"(
POLICY malware_sandbox {
  # Allow basic file I/O
  ALLOW {
    read, write, pread64, pwrite64, readv, writev,
    open, openat, openat2, close, close_range,
    stat, fstat, lstat, stat64, fstat64, lstat64, newfstatat, statx,
    lseek, _llseek, dup, dup2, dup3, fcntl, fcntl64, ioctl
  },

  # Allow memory management
  ALLOW {
    mmap, mmap2, munmap, mprotect, mremap, brk, madvise, mincore, msync
  },

  # Allow process control (limited)
  ALLOW {
    exit, exit_group, getpid, getppid, gettid,
    getuid, geteuid, getgid, getegid, getresuid, getresgid,
    getuid32, geteuid32, getgid32, getegid32
  },

  # Allow signal handling
  ALLOW {
    rt_sigreturn, rt_sigprocmask, rt_sigaction, rt_sigsuspend,
    sigreturn, sigprocmask, sigaction
  },

  # Allow directory operations
  ALLOW {
    getcwd, chdir, getdents, getdents64, readdir
  },

  # Allow time operations
  ALLOW {
    clock_gettime, gettimeofday, time, nanosleep, clock_nanosleep
  },

  # Allow I/O multiplexing
  ALLOW {
    select, pselect6, poll, ppoll,
    epoll_create, epoll_create1, epoll_ctl, epoll_wait, epoll_pwait
  },

  # Allow file metadata operations
  ALLOW {
    access, faccessat, faccessat2, readlink, readlinkat
  },

  # Allow thread-local storage
  ALLOW {
    set_thread_area, get_thread_area, set_tid_address, arch_prctl
  },

  # Allow resource queries
  ALLOW {
    getrlimit, ugetrlimit, prlimit64, getrusage
  },

  # Allow futex operations
  ALLOW {
    futex, set_robust_list, get_robust_list
  },

  # Log suspicious network operations
  LOG {
    socket, connect, bind, listen, accept, accept4,
    sendto, recvfrom, sendmsg, recvmsg, shutdown, setsockopt
  },

  # Log process creation attempts
  LOG {
    execve, execveat, fork, vfork, clone, clone3
  },

  # Log debugging attempts
  LOG {
    ptrace, process_vm_readv, process_vm_writev
  },

  # Log file system modifications
  LOG {
    unlink, unlinkat, rmdir, rename, renameat, mkdir, mkdirat,
    chmod, fchmod, chown, fchown, truncate, ftruncate
  },

  # Return EPERM for privilege escalation
  ERRNO(1) {
    setuid, setuid32, setgid, setgid32, setreuid, setregid,
    setresuid, setresgid, setfsuid, setfsgid, capset
  },

  # Return EPERM for filesystem modification
  ERRNO(1) {
    mount, umount, umount2, pivot_root, chroot, unshare, setns
  },

  # Kill on dangerous operations
  KILL {
    reboot, kexec_load, init_module, delete_module,
    ioperm, iopl, syslog, quotactl
  },

  # Default action: KILL process on unknown syscall
  DEFAULT KILL
}
)"sv;

// Behavioral metrics from Tier 2 native sandbox execution
// Based on BEHAVIORAL_ANALYSIS_SPEC.md - 16 metrics across 5 categories
struct BehavioralMetrics {
    // Category 1: File System Behavior (4 metrics)
    u32 file_operations { 0 };                    // open/read/write/delete
    u32 temp_file_creates { 0 };                  // Files in /tmp, %TEMP%
    u32 hidden_file_creates { 0 };                // .dotfiles, HIDDEN attribute
    u32 executable_drops { 0 };                   // Created .exe/.sh/.bat files

    // Category 2: Process & Execution (3 metrics)
    u32 process_operations { 0 };                 // fork/exec/CreateProcess
    u32 self_modification_attempts { 0 };         // Code injection, runtime patching
    u32 persistence_mechanisms { 0 };             // Cron, startup folder, registry Run keys

    // Category 3: Network Behavior (4 metrics)
    u32 network_operations { 0 };                 // socket/connect/send/recv
    u32 outbound_connections { 0 };               // Unique remote IPs
    u32 dns_queries { 0 };                        // DNS lookups
    u32 http_requests { 0 };                      // HTTP GET/POST

    // Category 4: System & Registry (3 metrics, Windows-specific)
    u32 registry_operations { 0 };                // Windows registry reads/writes
    u32 service_modifications { 0 };              // Service creation/modification
    u32 privilege_escalation_attempts { 0 };      // UAC bypass, SUID abuse

    // Category 5: Memory Behavior (2 metrics)
    u32 memory_operations { 0 };                  // VirtualAlloc, mmap, mprotect
    u32 code_injection_attempts { 0 };            // WriteProcessMemory, ptrace

    // Aggregated scores
    float threat_score { 0.0f };                  // 0.0-1.0 composite behavioral score
    Vector<String> suspicious_behaviors;          // Human-readable detections

    // Execution metadata
    Duration execution_time;
    bool timed_out { false };
    i32 exit_code { 0 };
};

// Syscall monitoring filter for specific operations
struct SyscallFilter {
    bool monitor_file_ops { true };
    bool monitor_process_ops { true };
    bool monitor_network_ops { true };
    bool monitor_registry_ops { true };           // Windows only
    bool monitor_memory_ops { true };
};

// Process information for launched sandbox
struct SandboxProcess {
    pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
};

// Parsed syscall event from nsjail stderr output
// Format: [SYSCALL] name(args...)
struct SyscallEvent {
    String name;              // Syscall name (e.g., "open", "socket", "write")
    Vector<String> args;      // Optional parsed arguments
    u64 timestamp_ns { 0 };   // Optional timestamp (reserved for future use)
};

// BehavioralAnalyzer - Tier 2 deep analysis with OS-level syscall monitoring
// Milestone 0.5 Phase 1: Real-time Sandboxing
//
// This component executes suspicious files in a native sandbox with full
// syscall monitoring and behavioral analysis. Target: <5 seconds.
//
// Implementation Strategy:
// - Phase 1a: Mock implementation (simulates behavioral analysis)
// - Phase 1b: nsjail + seccomp-BPF integration (real sandbox)
//
// The mock allows testing the infrastructure without nsjail setup.
class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(SandboxConfig const& config);
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(SandboxConfig const& config, SyscallFilter const& filter);

    ~BehavioralAnalyzer();

    // Execute file in native sandbox with behavioral monitoring
    // Returns comprehensive behavioral metrics
    ErrorOr<BehavioralMetrics> analyze(
        ByteBuffer const& file_data,
        String const& filename,
        Duration timeout);

    // Statistics
    struct Statistics {
        u64 total_analyses { 0 };
        u64 timeouts { 0 };
        u64 crashes { 0 };                        // Sandbox process crashed
        u64 blocked_operations { 0 };             // Syscalls blocked by seccomp
        Duration average_execution_time;
        Duration max_execution_time;
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

    // Configuration
    void update_filter(SyscallFilter const& filter) { m_syscall_filter = filter; }
    SyscallFilter const& filter() const { return m_syscall_filter; }

private:
    explicit BehavioralAnalyzer(SandboxConfig const& config, SyscallFilter const& filter);

    ErrorOr<void> initialize_sandbox();
    ErrorOr<void> setup_seccomp_filter();

    // Execution helpers
    ErrorOr<BehavioralMetrics> analyze_mock(ByteBuffer const& file_data, String const& filename);
    ErrorOr<BehavioralMetrics> analyze_nsjail(ByteBuffer const& file_data, String const& filename);

    // Syscall monitoring (real implementation)
    ErrorOr<void> start_syscall_tracing(int pid);
    ErrorOr<void> process_syscall_events();
    void update_metrics_from_syscall(StringView syscall_name, BehavioralMetrics& metrics);

    // Timeout enforcement and process cleanup (Week 1 Day 4)
    ErrorOr<void> enforce_sandbox_timeout(pid_t sandbox_pid, Duration timeout);
    ErrorOr<int> wait_for_sandbox_completion(pid_t sandbox_pid);

    // File handling infrastructure (Week 1 Day 3-4)
    ErrorOr<String> create_temp_sandbox_directory();
    ErrorOr<void> cleanup_temp_directory(String const& directory);
    ErrorOr<String> write_file_to_sandbox(String const& sandbox_dir, ByteBuffer const& file_data, String const& filename);
    ErrorOr<void> make_executable(String const& file_path);

    // Behavioral heuristics (mock implementation)
    ErrorOr<void> analyze_file_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics);
    ErrorOr<void> analyze_process_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics);
    ErrorOr<void> analyze_network_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics);
    ErrorOr<float> calculate_threat_score(BehavioralMetrics const& metrics);
    ErrorOr<Vector<String>> generate_suspicious_behaviors(BehavioralMetrics const& metrics);

    // Advanced malware pattern detection (Week 2 Task 2)
    bool detect_ransomware_pattern(BehavioralMetrics const& metrics);
    bool detect_keylogger_pattern(BehavioralMetrics const& metrics);
    bool detect_rootkit_pattern(BehavioralMetrics const& metrics);
    bool detect_cryptominer_pattern(BehavioralMetrics const& metrics);
    bool detect_process_injector_pattern(BehavioralMetrics const& metrics);

    // nsjail command building (real implementation)
    ErrorOr<Vector<String>> build_nsjail_command(String const& executable_path, Vector<String> const& args = {});
    ErrorOr<String> locate_nsjail_config_file();

    // Process management (real implementation)
    ErrorOr<SandboxProcess> launch_nsjail_sandbox(Vector<String> const& nsjail_command);

    // Pipe I/O helpers for reading nsjail stderr (Week 1 Day 5)
    ErrorOr<ByteBuffer> read_pipe_nonblocking(int fd, Duration timeout = Duration::from_seconds(1));
    ErrorOr<Vector<String>> read_pipe_lines(int fd, Duration timeout = Duration::from_seconds(1));

    // Syscall event parsing (Week 1 Day 6)
    ErrorOr<Optional<SyscallEvent>> parse_syscall_event(StringView line);

    SandboxConfig m_config;
    SyscallFilter m_syscall_filter;
    Statistics m_stats;

    // Native sandbox state
    bool m_use_mock { true };                     // True if nsjail not available
    String m_sandbox_dir;                         // Temporary directory for sandbox

    // Syscall tracing state (real implementation)
    HashMap<int, u32> m_syscall_counts;           // syscall_number -> count
};

}
