# BehavioralAnalyzer Implementation Quick Reference

**Quick Start**: Implementation guide for Phase 2 developers
**Full Design**: See `BEHAVIORAL_ANALYZER_DESIGN.md`

---

## TL;DR: What We're Building

Replace the stub `BehavioralAnalyzer` with real nsjail-based syscall monitoring for Linux.

### Current State (Stub)

```cpp
// Current: Heuristic analysis without execution
ErrorOr<BehavioralMetrics> analyze_mock(ByteBuffer const& file_data) {
    // Static analysis only - no actual execution
    // Checks for suspicious strings, entropy, headers
    return metrics;
}
```

### Target State (Real)

```cpp
// Target: Real execution in nsjail sandbox
ErrorOr<BehavioralMetrics> analyze_nsjail(ByteBuffer const& file_data) {
    // 1. Write file to /tmp/sentinel-XXXXXX/
    // 2. Launch nsjail with seccomp filter
    // 3. Monitor syscalls (open, fork, socket, etc.)
    // 4. Kill after timeout (5 seconds)
    // 5. Return behavioral metrics
    return metrics;
}
```

---

## Key Technologies

| Component | Purpose | Why |
|-----------|---------|-----|
| **nsjail** | Sandbox runtime | Google-backed, battle-tested in Chrome |
| **seccomp-BPF** | Syscall filtering | Allow/monitor/block syscalls at kernel level |
| **ptrace** | Syscall monitoring | Fallback if seccomp_unotify unavailable |
| **Linux namespaces** | Process isolation | Network, PID, mount, user namespaces |

---

## Installation

```bash
# Ubuntu/Debian
sudo apt-get install nsjail libseccomp-dev

# Verify installation
nsjail --help
which nsjail  # Should output /usr/bin/nsjail
```

---

## Implementation Priority Order

### Week 1: Foundation

```cpp
// 1. Temp directory creation
ErrorOr<String> create_temp_sandbox_directory();
ErrorOr<String> write_file_to_sandbox(String const& dir, ByteBuffer const& data);
ErrorOr<void> make_executable(String const& path);

// 2. nsjail launcher
ErrorOr<pid_t> launch_nsjail_sandbox(String const& target_path, Duration timeout);

// 3. Basic monitoring
ErrorOr<BehavioralMetrics> monitor_execution(pid_t sandbox_pid, Duration timeout);
```

### Week 2: Syscall Monitoring

```cpp
// 4. Syscall event parsing
ErrorOr<Vector<SyscallEvent>> capture_syscall_events(pid_t sandbox_pid);
ErrorOr<void> parse_syscall_into_metrics(SyscallEvent const& event, BehavioralMetrics& metrics);

// 5. Scoring
ErrorOr<float> calculate_threat_score(BehavioralMetrics const& metrics);
ErrorOr<Vector<String>> generate_suspicious_behaviors(BehavioralMetrics const& metrics);
```

### Week 3: Integration

```cpp
// 6. Platform detection
bool nsjail_available();
bool seccomp_unotify_available();

// 7. Full pipeline
ErrorOr<BehavioralMetrics> analyze_nsjail(ByteBuffer const& file_data, String const& filename, Duration timeout);
```

---

## Core Methods to Implement

### 1. Temp Directory Setup

```cpp
ErrorOr<String> create_temp_sandbox_directory()
{
    char template_path[] = "/tmp/sentinel-XXXXXX";
    char* dir = mkdtemp(template_path);
    if (!dir)
        return Error::from_errno(errno);

    return String::from_utf8(dir);
}
```

### 2. nsjail Command Builder

```cpp
ErrorOr<Vector<ByteString>> build_nsjail_command(
    String const& target_path,
    Duration timeout)
{
    Vector<ByteString> args;
    TRY(args.try_append("/usr/bin/nsjail"_string));
    TRY(args.try_append("--mode"_string));
    TRY(args.try_append("o"_string));  // once
    TRY(args.try_append("--time_limit"_string));
    TRY(args.try_append(TRY(String::number(timeout.to_seconds()))));
    TRY(args.try_append("--rlimit_as"_string));
    TRY(args.try_append("128"_string));  // 128MB
    TRY(args.try_append("--"_string));
    TRY(args.try_append(target_path));
    return args;
}
```

### 3. Syscall Monitoring (ptrace)

```cpp
ErrorOr<Vector<SyscallEvent>> monitor_via_ptrace(pid_t sandbox_pid)
{
    Vector<SyscallEvent> events;

    // Attach with ptrace
    TRY(Core::System::ptrace(PTRACE_SEIZE, sandbox_pid, nullptr, nullptr));

    while (true) {
        int status;
        pid_t result = waitpid(sandbox_pid, &status, 0);

        if (WIFEXITED(status) || WIFSIGNALED(status))
            break;

        if (WIFSTOPPED(status)) {
            // Read syscall from registers
            struct user_regs_struct regs;
            TRY(Core::System::ptrace(PTRACE_GETREGS, sandbox_pid, nullptr, &regs));

            SyscallEvent event;
            event.syscall_number = regs.orig_rax;
            event.syscall_name = TRY(syscall_number_to_name(regs.orig_rax));
            TRY(events.try_append(move(event)));
        }

        TRY(Core::System::ptrace(PTRACE_SYSCALL, sandbox_pid, nullptr, nullptr));
    }

    return events;
}
```

### 4. Metrics Parser

```cpp
ErrorOr<void> parse_syscall_into_metrics(
    SyscallEvent const& event,
    BehavioralMetrics& metrics)
{
    if (event.syscall_name == "open"sv || event.syscall_name == "openat"sv) {
        metrics.file_operations++;
    }
    else if (event.syscall_name == "fork"sv || event.syscall_name == "clone"sv) {
        metrics.process_operations++;
    }
    else if (event.syscall_name == "socket"sv) {
        metrics.network_operations++;
    }
    else if (event.syscall_name == "mprotect"sv) {
        metrics.memory_operations++;
        // Check for RWX pages (args[2] == PROT_READ|PROT_WRITE|PROT_EXEC)
        if (event.args[2] == 0x7) {
            metrics.code_injection_attempts++;
        }
    }
    return {};
}
```

---

## Syscall-to-Metric Mapping Table

| Syscall | Metric Field | Pattern Detected |
|---------|-------------|------------------|
| `open`, `openat` | `file_operations` | File access |
| `write`, `writev` | `file_operations` | File modification |
| `unlink`, `unlinkat` | `file_operations`, `temp_file_creates` | File deletion |
| `fork`, `clone` | `process_operations` | Process spawning |
| `execve` | `process_operations` | Execution |
| `ptrace` | `self_modification_attempts` | Process injection |
| `socket` | `network_operations` | Network init |
| `connect` | `network_operations`, `outbound_connections` | Outbound connection |
| `sendto`, `send` | `network_operations` | Data exfiltration |
| `mmap` | `memory_operations` | Memory allocation |
| `mprotect` | `memory_operations`, `code_injection_attempts` | Memory permission change |

---

## Scoring Formula (Quick Reference)

```cpp
float threat_score = (
    file_io_score * 0.40 +       // 40% weight
    process_score * 0.30 +       // 30% weight
    network_score * 0.10 +       // 10% weight
    memory_score * 0.15 +        // 15% weight
    platform_score * 0.05        //  5% weight
);

// File I/O scoring
if (file_operations > 100) file_score += 0.75;  // Ransomware
if (executable_drops > 0) file_score += 0.60;   // Dropper

// Process scoring
if (process_operations > 10) process_score += 0.50;
if (self_modification_attempts > 0) process_score += 0.70;  // Injection

// Network scoring
if (outbound_connections > 3) network_score += 0.60;  // C2 beaconing

// Memory scoring
if (code_injection_attempts > 0) memory_score += 0.80;  // RWX pages
```

---

## Testing Strategy

### Unit Tests

```cpp
// Test temp directory creation
TEST_CASE(behavioral_analyzer_temp_directory)
{
    auto temp_dir = MUST(create_temp_sandbox_directory());
    EXPECT(temp_dir.starts_with("/tmp/sentinel-"sv));
    MUST(cleanup_temp_directory(temp_dir));
}

// Test nsjail command generation
TEST_CASE(behavioral_analyzer_nsjail_command)
{
    auto cmd = MUST(build_nsjail_command("/tmp/test"_string, Duration::from_seconds(5)));
    EXPECT(cmd[0] == "/usr/bin/nsjail"_string);
    EXPECT(cmd.contains_slow("--time_limit"_string));
}

// Test scoring algorithm
TEST_CASE(behavioral_analyzer_scoring)
{
    BehavioralMetrics metrics;
    metrics.file_operations = 150;  // Ransomware pattern
    metrics.process_operations = 5;

    auto score = MUST(calculate_threat_score(metrics));
    EXPECT(score > 0.5f);  // Should be suspicious
}
```

### Integration Tests

```cpp
// Test with benign executable
TEST_CASE(behavioral_analyzer_benign_executable)
{
    auto analyzer = MUST(BehavioralAnalyzer::create(default_config));

    // Read /bin/ls as test file
    auto file_data = MUST(read_file("/bin/ls"_string));

    auto metrics = MUST(analyzer->analyze(file_data, "ls"_string, Duration::from_seconds(5)));

    // Should have low threat score
    EXPECT(metrics.threat_score < 0.3f);
    EXPECT(metrics.file_operations < 10);  // /bin/ls doesn't modify many files
}

// Test with malware simulator
TEST_CASE(behavioral_analyzer_malware_simulator)
{
    auto analyzer = MUST(BehavioralAnalyzer::create(default_config));

    // Create malware simulator script
    auto script_data = TRY(ByteBuffer::copy(R"(#!/bin/bash
for i in {1..200}; do
    echo "data" > /tmp/file_$i.txt
done
)"sv.bytes()));

    auto metrics = MUST(analyzer->analyze(script_data, "malware_sim.sh"_string, Duration::from_seconds(5)));

    // Should detect rapid file modification
    EXPECT(metrics.threat_score > 0.6f);
    EXPECT(metrics.file_operations > 100);
}
```

---

## Common Pitfalls

### 1. File Descriptor Leaks

**Problem**: Pipes not closed after fork/exec
```cpp
// BAD
int pipe_fds[2];
pipe(pipe_fds);
fork();
// Child and parent both have open FDs - leak!
```

**Solution**: Close unused ends
```cpp
// GOOD
int pipe_fds[2];
pipe(pipe_fds);
pid_t pid = fork();
if (pid == 0) {
    // Child
    close(pipe_fds[0]);  // Close read end
    // Use pipe_fds[1] for writing
} else {
    // Parent
    close(pipe_fds[1]);  // Close write end
    // Use pipe_fds[0] for reading
}
```

### 2. Zombie Processes

**Problem**: Child processes not reaped
```cpp
// BAD
pid_t pid = fork();
if (pid > 0) {
    // Parent exits without waiting - zombie!
}
```

**Solution**: Always waitpid()
```cpp
// GOOD
pid_t pid = fork();
if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
}
```

### 3. Timeout Not Enforced

**Problem**: alarm() not set before monitoring
```cpp
// BAD
monitor_execution(pid);  // May hang forever
```

**Solution**: Set alarm before monitoring
```cpp
// GOOD
alarm(timeout.to_seconds());
monitor_execution(pid);
alarm(0);  // Cancel alarm after completion
```

---

## Debugging Tips

### Check nsjail Execution

```bash
# Run nsjail manually to verify setup
nsjail --mode o --time_limit 5 -- /bin/ls

# Expected output:
# [I] Mode: STANDALONE_ONCE
# [I] Executing '/bin/ls' ...
# (output of ls)
# [I] Process exited with status 0
```

### Enable Debug Logging

```cpp
// In BehavioralAnalyzer.cpp
#define BEHAVIORAL_ANALYZER_DEBUG 1

dbgln_if(BEHAVIORAL_ANALYZER_DEBUG, "Launching nsjail: {}", target_path);
dbgln_if(BEHAVIORAL_ANALYZER_DEBUG, "Syscall event: {}", event.syscall_name);
```

### Inspect /proc for Syscalls

```bash
# Monitor syscalls in real-time
sudo strace -p <pid> -e trace=all

# View current syscall
cat /proc/<pid>/syscall
```

### Test seccomp_unotify Availability

```cpp
// Check if kernel supports seccomp user notifications
bool seccomp_unotify_available()
{
    int fd = syscall(__NR_seccomp, SECCOMP_GET_NOTIF_SIZES, nullptr);
    return fd >= 0;
}
```

---

## Performance Targets

| Metric | Target | Measured |
|--------|--------|----------|
| Temp dir creation | < 10ms | TBD |
| File write | < 50ms | TBD |
| nsjail startup | < 200ms | TBD |
| Syscall monitoring | < 2s | TBD |
| Score calculation | < 10ms | TBD |
| **Total** | **< 3s** | **TBD** |

---

## Next Steps

1. **Install nsjail**: `sudo apt-get install nsjail`
2. **Read full design**: `BEHAVIORAL_ANALYZER_DESIGN.md`
3. **Start implementation**: Begin with temp directory methods
4. **Write tests**: Create `TestBehavioralAnalyzer.cpp`
5. **Integrate**: Update `Orchestrator::execute_tier2_native()`

---

## Document References

- **Full Design**: `BEHAVIORAL_ANALYZER_DESIGN.md`
- **Behavioral Spec**: `BEHAVIORAL_ANALYSIS_SPEC.md`
- **Sandbox Comparison**: `SANDBOX_TECHNOLOGY_COMPARISON.md`
- **Current Stub**: `Services/Sentinel/Sandbox/BehavioralAnalyzer.{h,cpp}`

---

**Author**: Security Research Team
**Date**: 2025-11-02
**Status**: Quick Reference for Phase 2 Implementation
