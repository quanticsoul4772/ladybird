# Network Isolation - Security Considerations

## Overview

The Network Isolation system provides process-level network blocking to prevent malware from:
- **Command & Control (C2)**: Blocking communication with attacker infrastructure
- **Data Exfiltration**: Preventing stolen data transmission
- **Lateral Movement**: Stopping network propagation
- **Second-Stage Downloads**: Blocking additional payload retrieval

## Privilege Requirements

### Root/Sudo Access

Network isolation requires **root privileges** to modify firewall rules:

- **iptables**: Requires root to modify kernel netfilter rules
- **nftables**: Requires root to modify nftables rulesets

**Current Implementation**: Uses `sudo` to execute firewall commands
- Requires passwordless sudo for Sentinel service user
- Alternative: Use Linux capabilities (see below)

### Linux Capabilities (Recommended Alternative)

Instead of full root access, use **CAP_NET_ADMIN** capability:

```bash
# Grant CAP_NET_ADMIN to Sentinel binary
sudo setcap cap_net_admin+ep /path/to/Sentinel

# Verify capability
getcap /path/to/Sentinel
# Output: /path/to/Sentinel = cap_net_admin+ep
```

**Benefits**:
- Least-privilege security model
- No full root access required
- Limits attack surface if Sentinel is compromised

**Implementation**:
- Modify firewall backends to not use `sudo` when CAP_NET_ADMIN is available
- Check capabilities at runtime: `capget(CAP_NET_ADMIN)`

## Security Risks

### 1. Sentinel Compromise

**Risk**: If Sentinel is compromised, attacker could:
- Disable network isolation for malware processes
- Add firewall rules to block legitimate traffic
- Create denial-of-service by blocking critical services

**Mitigations**:
- Run Sentinel as separate user with minimal privileges
- Use Linux capabilities instead of full root
- Implement audit logging for all firewall modifications
- Rate-limit firewall rule changes
- Require elevated privileges for cleanup operations

### 2. Privilege Escalation

**Risk**: Vulnerabilities in Sentinel could lead to privilege escalation

**Mitigations**:
- Minimize attack surface (small codebase, minimal dependencies)
- Regular security audits
- Sandboxing Sentinel itself (AppArmor/SELinux profiles)
- Input validation on all user-controlled data (PIDs, reasons)

### 3. Critical Process Blocking

**Risk**: Accidentally blocking critical system processes could cause system instability

**Mitigations**:
- **Critical process list**: Never isolate PID 1, systemd, sshd, etc.
- **Process name checking**: Verify process name before isolation
- **Dry-run mode**: Test firewall rules before applying
- **Automatic cleanup**: Remove rules on Sentinel shutdown
- **Rule timeout**: Auto-remove rules after 1 hour (prevents rule leakage)

### 4. Race Conditions

**Risk**: Process could establish network connection before isolation is applied

**Mitigations**:
- Apply firewall rules **before** executing suspicious file
- Use process tree isolation to catch forked children
- Monitor for network connections during isolation setup

### 5. Bypass Techniques

**Risk**: Malware could bypass isolation through:
- **IPC to non-isolated process**: Use another process as proxy
- **Shared memory**: Exfiltrate data via shared memory to isolated process
- **File-based communication**: Write to shared files read by non-isolated process

**Mitigations**:
- **Process tree isolation**: Isolate entire process tree, not just parent
- **IPC monitoring**: Detect suspicious IPC between isolated/non-isolated processes
- **File system monitoring**: Monitor file writes by isolated processes
- **User awareness**: Educate users that isolation is not perfect

## Implementation Safety Features

### 1. Critical Process Protection

```cpp
static constexpr pid_t CRITICAL_PIDS[] = { 1 };
static constexpr StringView CRITICAL_PROCESS_NAMES[] = {
    "systemd", "init", "sshd", "systemd-resolved", ...
};
```

**Never** allow isolation of these processes.

### 2. Automatic Cleanup

- **Process exit monitoring**: ProcessMonitor detects when isolated process exits
- **Rule removal**: Automatically remove firewall rules when process terminates
- **Destructor cleanup**: NetworkIsolationManager destructor calls cleanup_all()
- **Signal handling**: Sentinel should handle SIGTERM/SIGINT and cleanup before exit

### 3. Dry-Run Mode

All backends support dry-run mode for testing:
```cpp
auto manager = NetworkIsolationManager::create(
    FirewallBackend::Auto,
    true  // dry_run = true
);
```

Logs what would be executed without actually modifying firewall.

### 4. Rule Timeout (TODO)

**Not yet implemented**: Automatically remove rules after 1 hour to prevent rule leakage.

Implementation approach:
```cpp
struct IsolatedProcess {
    // ... existing fields ...
    UnixDateTime expires_at;  // Auto-cleanup after this time
};
```

ProcessMonitor should check for expired rules and remove them.

## Deployment Recommendations

### Production Deployment

1. **Separate Service User**:
   ```bash
   sudo useradd -r -s /bin/false sentinel
   sudo usermod -aG <appropriate groups> sentinel
   ```

2. **Grant Capabilities**:
   ```bash
   sudo setcap cap_net_admin+ep /usr/bin/Sentinel
   ```

3. **AppArmor/SELinux Profile**:
   - Restrict Sentinel to only modify firewall rules
   - Deny access to sensitive files
   - Limit IPC to only required services

4. **Audit Logging**:
   - Log all isolation/restoration operations
   - Forward logs to centralized SIEM
   - Alert on suspicious patterns (e.g., mass isolation)

5. **Rate Limiting**:
   - Limit number of isolations per minute
   - Prevent DoS via excessive firewall modifications

### Testing Recommendations

1. **Test in VM/Container**: Never test on production system
2. **Use Dry-Run**: Always test with dry_run=true first
3. **Monitor System**: Watch for performance impact during isolation
4. **Test Recovery**: Verify cleanup works correctly on crashes

## Alternatives Considered

### 1. Network Namespaces

**Approach**: Use Linux network namespaces to isolate processes
- Create isolated network namespace per suspicious process
- No network interfaces in namespace (completely isolated)

**Pros**:
- More secure isolation (kernel-enforced)
- No firewall rule management complexity
- Harder to bypass

**Cons**:
- More complex to implement
- Requires CAP_SYS_ADMIN (more privileged than CAP_NET_ADMIN)
- Harder to debug

**Status**: Consider for future enhancement

### 2. Seccomp-BPF

**Approach**: Use seccomp to block syscalls related to networking
- Block socket(), connect(), sendto(), etc.

**Pros**:
- Process-level enforcement
- No elevated privileges needed (can be applied by process itself)

**Cons**:
- Process must cooperate (apply seccomp filter itself)
- Malware won't cooperate
- Cannot be applied externally to existing processes

**Status**: Not suitable for our use case

### 3. eBPF

**Approach**: Use eBPF programs to filter network traffic per-process

**Pros**:
- Kernel-level enforcement
- Fine-grained control
- Better performance than iptables

**Cons**:
- Requires kernel 4.18+ with eBPF support
- Complex to implement
- Requires CAP_BPF and CAP_NET_ADMIN

**Status**: Consider for future enhancement

## Audit & Compliance

### Logging Requirements

All isolation operations should be logged:
- **Isolation**: `Isolated PID <pid> (<process_name>) - Reason: <reason>`
- **Restoration**: `Restored PID <pid> (<process_name>)`
- **Failed Attempts**: `Failed to isolate PID <pid>: <error>`
- **Critical Process Block**: `ALERT: Attempted to isolate critical process <name> (PID <pid>)`

### Audit Questions

Security auditors should verify:
1. Who can trigger isolation? (Only Sentinel orchestrator)
2. What privileges does Sentinel have? (CAP_NET_ADMIN, not full root)
3. How are critical processes protected? (Hardcoded safelist)
4. What happens if Sentinel crashes? (Automatic cleanup via destructor)
5. Can isolation be bypassed? (Document known bypass techniques)

## Future Enhancements

1. **Rule Timeout**: Auto-remove rules after configurable duration
2. **Network Namespace Isolation**: More secure alternative to firewall rules
3. **eBPF Backend**: Higher performance, finer-grained control
4. **IPC Monitoring**: Detect bypass attempts via IPC
5. **User Notification**: Alert user when process is isolated
6. **Per-Process Whitelisting**: Allow specific destinations (e.g., localhost only)
7. **Statistics Dashboard**: Track isolation effectiveness, bypass attempts

## References

- [Linux Capabilities](https://man7.org/linux/man-pages/man7/capabilities.7.html)
- [nftables](https://netfilter.org/projects/nftables/)
- [iptables](https://netfilter.org/projects/iptables/)
- [Network Namespaces](https://man7.org/linux/man-pages/man7/network_namespaces.7.html)
- [Seccomp-BPF](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
