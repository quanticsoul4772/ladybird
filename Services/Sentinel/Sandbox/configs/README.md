# Malware Sandbox Configuration

This directory contains production configuration files for the Ladybird Sentinel malware analysis sandbox, utilizing **nsjail** and **seccomp-BPF** for strong isolation and syscall filtering.

## Overview

The malware sandbox provides defense-in-depth isolation through multiple layers:

1. **Namespace isolation** (PID, mount, network, user, UTS, IPC, cgroup)
2. **Resource limits** (CPU, memory, file descriptors)
3. **Syscall filtering** (seccomp-BPF with Kafel DSL)
4. **Filesystem isolation** (minimal read-only mounts, tmpfs working directory)
5. **User remapping** (root in sandbox → nobody outside)

## Files

### `malware-sandbox.cfg`

**nsjail configuration** in Protocol Buffer text format.

Key isolation features:
- **Execution mode**: ONCE (single execution, exit after completion)
- **Time limit**: 5 seconds (wall clock time)
- **Resource limits**:
  - Memory: 256 MB (rlimit_as)
  - CPU: 5 seconds (rlimit_cpu)
  - File descriptors: 64 (rlimit_nofile)
  - File writes: 10 MB max (rlimit_fsize)
- **Namespace isolation**:
  - PID: Isolated process tree (PID 1 inside sandbox)
  - Mount: Custom filesystem view with minimal read-only mounts
  - Network: No network access (isolated network namespace)
  - User: UID/GID remapping (root → nobody/65534)
  - UTS: Isolated hostname ("malware-sandbox")
  - IPC: Isolated SysV IPC and POSIX message queues
  - Cgroup: Isolated cgroup view
- **Filesystem**:
  - `/tmp`: Read-write tmpfs (64 MB limit)
  - `/lib`, `/lib64`, `/usr/lib`: Read-only shared libraries
  - `/bin`, `/usr/bin`: Read-only utilities
  - `/proc`: Process information (required)
  - `/dev`: Minimal device nodes (read-only)

### `malware-sandbox.kafel`

**Seccomp-BPF policy** in Kafel DSL format.

Three-tier syscall filtering:

1. **ALLOW** (Tier 1): Safe syscalls required for basic execution
   - File I/O: read, write, open, close, stat, lseek
   - Memory: mmap, munmap, mprotect, brk
   - Process info: getpid, getuid, exit
   - Signals: rt_sigreturn, rt_sigprocmask
   - Time: clock_gettime, nanosleep
   - I/O multiplex: select, poll, epoll

2. **LOG** (Tier 2): Suspicious syscalls indicating malicious behavior
   - Network: socket, connect, bind, sendto, recvfrom
   - Process creation: execve, fork, clone
   - Debugging: ptrace, process_vm_readv
   - File modification: unlink, rename, chmod, truncate
   - Kernel modules: init_module, delete_module

3. **ERRNO(EPERM)** (Tier 3): Blocked syscalls returning permission denied
   - Privilege escalation: setuid, setgid, capset
   - Filesystem modification: mount, umount, chroot
   - Namespace manipulation: unshare, setns
   - Priority manipulation: setpriority, nice

4. **KILL** (Tier 4): Dangerous syscalls causing immediate termination
   - System operations: reboot, kexec_load
   - Kernel modules: init_module, delete_module
   - Low-level I/O: ioperm, iopl
   - Default: All unknown syscalls → KILL

### Inline Fallback Policy

The file `../BehavioralAnalyzer.h` contains `INLINE_SECCOMP_POLICY`, a condensed version of the Kafel policy embedded as a C++ string constant. This provides a fallback when the external `.kafel` file is unavailable.

## Usage

### With nsjail Command Line

```bash
# Basic usage
nsjail --config malware-sandbox.cfg -- /path/to/suspicious/binary

# With arguments
nsjail --config malware-sandbox.cfg -- /bin/sh -c "ls /tmp"

# Verbose logging
nsjail --config malware-sandbox.cfg --verbose -- /path/to/suspicious/binary
```

### With BehavioralAnalyzer (C++ API)

```cpp
#include "Services/Sentinel/Sandbox/BehavioralAnalyzer.h"

// Create analyzer with default config
auto analyzer = TRY(BehavioralAnalyzer::create(config));

// Analyze suspicious file
auto metrics = TRY(analyzer->analyze(file_data, "suspicious.bin", Duration::from_seconds(5)));

// Check behavioral score
if (metrics.threat_score > 0.7) {
    dbgln("High threat detected: {}", metrics.threat_score);
    for (auto const& behavior : metrics.suspicious_behaviors)
        dbgln("  - {}", behavior);
}
```

### Monitoring Seccomp Violations

Seccomp violations are logged to the kernel audit log:

```bash
# View recent seccomp violations
ausearch -m SECCOMP -ts recent

# Or via journalctl
journalctl -k | grep SECCOMP

# Example output:
# audit: type=1326 audit(1234567890.123:456): auid=1000 uid=65534 gid=65534
#   ses=1 pid=12345 comm="suspicious.bin" exe="/tmp/suspicious.bin"
#   sig=31 arch=c000003e syscall=59 compat=0 ip=0x7f1234567890 code=0x80000000
```

Syscall numbers reference (x86_64):
- `59` = execve
- `41` = socket
- `42` = connect
- `57` = fork
- `56` = clone

## Customization

### Adjusting Resource Limits

Edit `malware-sandbox.cfg`:

```protobuf
# Increase memory limit to 512 MB
rlimit_as: 536870912

# Increase execution time to 10 seconds
time_limit: 10
rlimit_cpu: 10

# Increase file descriptor limit
rlimit_nofile: 128
```

### Modifying Syscall Policy

Edit `malware-sandbox.kafel`:

```kafel
# Example: Allow network operations for specific analysis
POLICY malware_sandbox {
  ALLOW {
    # ... existing syscalls ...
    socket, connect, sendto, recvfrom  # Move from LOG to ALLOW
  },

  # ... rest of policy ...
}
```

### Creating Specialized Policies

For analyzing specific malware families:

```bash
# Copy base policy
cp malware-sandbox.kafel ransomware-sandbox.kafel

# Edit to allow file operations for ransomware analysis
# Move file modification syscalls from LOG to ALLOW
# This allows observing file encryption behavior
```

### Using External Kafel Policy File

Modify `malware-sandbox.cfg`:

```protobuf
# Replace inline seccomp_string with external file reference
seccomp_string: "@/path/to/malware-sandbox.kafel"
```

Or compile Kafel to BPF:

```bash
# Compile Kafel DSL to raw BPF bytecode
kafel malware-sandbox.kafel > malware-sandbox.bpf

# Reference compiled BPF in config
seccomp_bpf: "/path/to/malware-sandbox.bpf"
```

## Security Considerations

### What the Sandbox DOES Protect Against

- **Process isolation**: Malware cannot see or interact with host processes
- **Filesystem isolation**: Malware cannot access host filesystem (only tmpfs and read-only mounts)
- **Network isolation**: Malware cannot make network connections (isolated network namespace)
- **Privilege escalation**: Syscalls like setuid, capset are blocked
- **Kernel exploitation**: Dangerous syscalls (ioperm, iopl, module loading) are blocked
- **Resource exhaustion**: CPU, memory, file descriptor limits prevent DoS

### What the Sandbox DOES NOT Protect Against

- **Kernel vulnerabilities**: Exploits in the Linux kernel itself can escape sandbox
- **Hardware exploits**: Spectre, Meltdown, Rowhammer, etc.
- **Side-channel attacks**: Timing attacks, cache attacks
- **Multi-stage attacks**: Malware that requires network or specific files may not fully execute
- **VM escapes**: If running in a VM, malware could potentially escape VM isolation

### Best Practices

1. **Run nsjail on dedicated analysis VM**: Add VM isolation layer
2. **Monitor kernel audit logs**: Detect escape attempts via seccomp violations
3. **Update regularly**: Keep kernel, nsjail, and seccomp policies current
4. **Limit analysis time**: Shorter time limits reduce attack surface
5. **Use minimal mounts**: Only mount libraries/utilities required for execution
6. **Review logs**: Regularly audit LOG tier syscalls for new attack patterns
7. **Network isolation**: Run on air-gapped network or VLAN with no internet access
8. **Snapshot and restore**: Use VM snapshots to reset analysis environment

## Troubleshooting

### "nsjail: command not found"

Install nsjail:

```bash
# Ubuntu/Debian
sudo apt install nsjail

# Or build from source (see NSJAIL_INSTALLATION_GUIDE.md)
git clone https://github.com/google/nsjail.git
cd nsjail && make && sudo make install
```

### "Failed to parse config file"

Protocol Buffer syntax error. Common issues:

```protobuf
# Wrong: Missing quotes around strings
hostname: malware-sandbox

# Right: String values must be quoted
hostname: "malware-sandbox"

# Wrong: Numeric values with quotes
time_limit: "5"

# Right: Numeric values without quotes
time_limit: 5
```

### "Kafel compilation failed"

Kafel DSL syntax error. Check:

1. Syscall names are valid (see `man 2 syscalls`)
2. Braces are balanced `{ }`
3. Commas separate syscall names
4. Policy name matches usage

### "Binary exits immediately with code 0"

Possible causes:

1. **Missing shared libraries**: Add library mounts to config
2. **Missing `/proc`**: Required by most binaries (already mounted in config)
3. **Seccomp blocking required syscall**: Check audit logs, move syscall to ALLOW

```bash
# Debug: List required libraries
ldd /path/to/binary

# Add missing library paths to config
mount {
  src: "/usr/lib/x86_64-linux-gnu"
  dst: "/usr/lib/x86_64-linux-gnu"
  is_bind: true
  rw: false
}
```

### "SECCOMP violations flooding logs"

Legitimate syscalls being blocked:

1. Check audit logs for syscall number
2. Look up syscall name: `ausyscall <number>`
3. Add to ALLOW or LOG tier if safe
4. Recompile Kafel policy if using external file

## References

- **nsjail**: https://github.com/google/nsjail
- **nsjail documentation**: https://nsjail.dev/
- **Kafel**: https://github.com/google/kafel
- **seccomp-bpf**: https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html
- **Linux syscalls**: https://man7.org/linux/man-pages/man2/syscalls.2.html
- **Linux namespaces**: https://man7.org/linux/man-pages/man7/namespaces.7.html

## Development History

- **Created**: 2025-11-02 (Milestone 0.5 Phase 1b)
- **Purpose**: Behavioral analysis sandbox for malware detection
- **Maintainer**: Ladybird Sentinel Team
- **Status**: Production-ready (pending nsjail installation)

## License

BSD-2-Clause (same as Ladybird project)

---

**Security Warning**: This sandbox is designed for malware analysis in controlled environments. Do not rely on it as the sole protection mechanism. Always run analysis on dedicated, isolated systems with VM-level isolation as an additional layer.
