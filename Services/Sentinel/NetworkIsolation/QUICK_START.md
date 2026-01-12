# Network Isolation - Quick Start Guide

## Overview

Network isolation blocks suspicious processes from accessing the network, preventing:
- Command & Control (C2) communication
- Data exfiltration
- Malware propagation
- Second-stage payload downloads

## Installation

### 1. Install Firewall Tools

```bash
# Ubuntu/Debian
sudo apt-get install iptables nftables

# Fedora/RHEL
sudo dnf install iptables nftables

# Arch
sudo pacman -S iptables nftables
```

### 2. Grant Privileges

**Option A: Linux Capabilities (Recommended)**

```bash
# Build Sentinel first
cmake --build Build/release

# Grant CAP_NET_ADMIN capability
sudo setcap cap_net_admin+ep Build/release/bin/Sentinel

# Verify
getcap Build/release/bin/Sentinel
```

**Option B: Passwordless Sudo (Less Secure)**

```bash
# Edit sudoers
sudo visudo -f /etc/sudoers.d/sentinel

# Add these lines (replace <username> with your user):
<username> ALL=(ALL) NOPASSWD: /usr/bin/iptables
<username> ALL=(ALL) NOPASSWD: /usr/bin/nft
```

### 3. Build and Test

```bash
# Build
cmake --preset Release
cmake --build Build/release

# Run tests
./Build/release/bin/TestNetworkIsolationManager
```

## Basic Usage

### C++ API

```cpp
#include <Services/Sentinel/NetworkIsolation/NetworkIsolationManager.h>

using namespace Sentinel::NetworkIsolation;

// Create manager (auto-detects iptables/nftables)
auto manager = TRY(NetworkIsolationManager::create());

// Isolate a process
pid_t suspicious_pid = 12345;
TRY(manager->isolate_process(suspicious_pid, "Suspicious download"_string));

// Process is now blocked from network access
// (except localhost)

// Restore network access
TRY(manager->restore_process(suspicious_pid));

// Or wait for automatic cleanup when process exits
```

### Command Line (via sentinel-cli)

```bash
# Start Sentinel service
sudo Sentinel

# In another terminal, isolate a process
echo "network-isolate <PID> <reason>" | sentinel-cli

# Example
echo "network-isolate 12345 Suspicious behavior" | sentinel-cli

# List isolated processes
echo "network-list" | sentinel-cli

# Restore process
echo "network-restore 12345" | sentinel-cli
```

## Configuration

### Enable in Orchestrator

```cpp
#include <Services/Sentinel/Sandbox/Orchestrator.h>

// Configure sandbox with network isolation
SandboxConfig config;
config.enable_network_isolation = true;

auto orchestrator = TRY(Orchestrator::create(config));
```

### Environment Variables

```bash
# Enable network isolation
export SENTINEL_ENABLE_NETWORK_ISOLATION=1

# Use specific firewall backend
export SENTINEL_FIREWALL_BACKEND=nftables  # or iptables

# Enable dry-run mode (testing)
export SENTINEL_DRY_RUN=1
```

## Testing

### Dry-Run Mode

Test without actually modifying firewall:

```cpp
// Create in dry-run mode
auto manager = TRY(NetworkIsolationManager::create(
    NetworkIsolationManager::FirewallBackend::Auto,
    true  // dry_run = true
));

// All operations log but don't execute
TRY(manager->isolate_process(12345, "Test"_string));
// Output: [DRY-RUN] Would execute: sudo iptables -I OUTPUT ...
```

### Manual Testing

```bash
# Terminal 1: Start a long-running process
sleep 300 &
PID=$!
echo "Process PID: $PID"

# Terminal 2: Isolate it
./sentinel-cli <<EOF
network-isolate $PID Test isolation
EOF

# Terminal 3: Verify isolation
# Check firewall rules
sudo iptables -L OUTPUT -n -v | grep $PID
# Or for nftables:
sudo nft list table inet sentinel_isolation

# Try network access (should fail)
# Note: This needs to run as the same user as $PID
curl http://example.com  # Should timeout

# Terminal 2: Restore access
./sentinel-cli <<EOF
network-restore $PID
EOF

# Terminal 3: Verify restoration
sudo iptables -L OUTPUT -n -v | grep $PID  # Should be empty
```

## Verification

### Check Backend Detection

```bash
# Run test
./Build/release/bin/TestNetworkIsolationManager 2>&1 | grep -i backend
# Output should show: "Detected nftables" or "Detected iptables"
```

### Verify Rules Applied

**For iptables:**
```bash
# List all rules
sudo iptables -L OUTPUT -n -v

# Look for SENTINEL_BLOCK rules
sudo iptables -L OUTPUT -n -v | grep SENTINEL

# Example output:
# 0 0 ACCEPT all -- * * 0.0.0.0/0 127.0.0.1 owner PID match 12345
# 0 0 LOG all -- * * 0.0.0.0/0 0.0.0.0/0 owner PID match 12345 LOG flags 0 level 4 prefix "SENTINEL_BLOCK_PID_12345: "
# 0 0 DROP all -- * * 0.0.0.0/0 0.0.0.0/0 owner PID match 12345
```

**For nftables:**
```bash
# List Sentinel table
sudo nft list table inet sentinel_isolation

# Example output:
# table inet sentinel_isolation {
#   chain output {
#     type filter hook output priority 0; policy accept;
#     meta skuid 1000 ip daddr 127.0.0.0/8 accept comment "Sentinel: Allow loopback for UID 1000 (PID 12345)"
#     meta skuid 1000 log prefix "SENTINEL_BLOCK_UID_1000_PID_12345: " comment "Sentinel: Log blocked traffic for PID 12345"
#     meta skuid 1000 drop comment "Sentinel: Block network for PID 12345"
#   }
# }
```

## Troubleshooting

### "No supported firewall backend found"

**Problem**: Neither iptables nor nftables is installed

**Solution**:
```bash
# Install both
sudo apt-get install iptables nftables

# Verify
which iptables  # Should output /usr/bin/iptables
which nft       # Should output /usr/bin/nft
```

### "Failed to execute iptables command"

**Problem**: Missing sudo privileges

**Solution**:
```bash
# Option 1: Grant CAP_NET_ADMIN (recommended)
sudo setcap cap_net_admin+ep Build/release/bin/Sentinel

# Option 2: Add passwordless sudo
sudo visudo -f /etc/sudoers.d/sentinel
# Add: <username> ALL=(ALL) NOPASSWD: /usr/bin/iptables, /usr/bin/nft
```

### "Cannot isolate critical system process"

**Problem**: Attempting to isolate PID 1, systemd, sshd, etc.

**Solution**: This is a safety feature. Don't isolate critical processes.

### Rules not removed after process exits

**Problem**: ProcessMonitor not detecting exit

**Debug**:
```bash
# Check if process is really dead
ps aux | grep <PID>

# Wait for ProcessMonitor polling (up to 1 second)
sleep 2

# Manually cleanup if needed
./sentinel-cli <<EOF
network-restore <PID>
EOF
```

### High CPU usage

**Problem**: ProcessMonitor polling overhead

**Solution**: This is normal. Monitor polls every 1 second with ~0.1ms overhead per process.

## Best Practices

### 1. Use Dry-Run for Testing

Always test in dry-run mode first:
```cpp
auto manager = TRY(NetworkIsolationManager::create(
    NetworkIsolationManager::FirewallBackend::Auto,
    true  // dry_run
));
```

### 2. Isolate Process Trees

Isolate entire process tree to catch forked children:
```cpp
TRY(manager->isolate_process_tree(root_pid));
```

### 3. Monitor Statistics

Track isolation effectiveness:
```cpp
auto stats = manager->get_statistics();
dbgln("Active isolations: {}", stats.active_isolated_processes);
dbgln("Total rules: {}", stats.total_rules_applied);
```

### 4. Cleanup on Shutdown

Ensure cleanup in destructor or signal handler:
```cpp
// Automatic via destructor
~MyClass() {
    if (m_network_isolation) {
        (void)m_network_isolation->cleanup_all();
    }
}

// Or in signal handler
void signal_handler(int sig) {
    if (network_isolation) {
        (void)network_isolation->cleanup_all();
    }
    exit(0);
}
```

### 5. Log All Operations

Enable debug logging for audit trail:
```bash
export SENTINEL_DEBUG=1
./Build/release/bin/Sentinel 2>&1 | tee sentinel.log
```

## Performance Tips

### 1. Use nftables When Possible

nftables is 2-3x faster than iptables for large rulesets.

### 2. Batch Operations

Isolate multiple processes at once to amortize overhead:
```cpp
for (auto pid : suspicious_pids) {
    (void)manager->isolate_process(pid, "Batch isolation"_string);
}
```

### 3. Monitor ProcessMonitor Overhead

For 1000+ isolated processes, consider increasing polling interval:
```cpp
// TODO: Add configurable polling interval
// Default: 1 second
```

## Security Considerations

### Critical Processes Protected

These processes are **never** isolated:
- PID 1 (init/systemd)
- systemd
- sshd
- systemd-resolved
- systemd-networkd
- NetworkManager
- dbus-daemon

### Bypass Techniques

Malware can bypass via:
- **IPC to non-isolated process**: Use another process as proxy
- **Shared memory**: Exfiltrate via shared memory
- **File-based communication**: Write files read by non-isolated process

**Mitigation**: Isolate entire process tree, monitor IPC/files

### Privilege Requirements

Network isolation requires:
- **CAP_NET_ADMIN** capability (recommended), OR
- **sudo** access for iptables/nft commands

Never run Sentinel as full root user. Use capabilities.

## Example Use Cases

### 1. Isolate Suspicious Download

```cpp
// User downloads unknown file
auto file_path = "/tmp/suspicious_file.exe"_string;

// Analyze with sandbox
auto result = TRY(orchestrator->analyze_file(file_data, file_path));

if (result.is_suspicious() && m_network_isolation) {
    // Execute file in isolated environment
    pid_t pid = execute_file(file_path);

    // Block network access
    TRY(m_network_isolation->isolate_process(
        pid,
        TRY(String::formatted("Suspicious file: {}", file_path))
    ));

    dbgln("Isolated suspicious process {} from network", pid);
}
```

### 2. Quarantine Malicious Process

```cpp
// YARA/ML detected malware
if (result.is_malicious()) {
    // Isolate immediately
    TRY(m_network_isolation->isolate_process_tree(pid));

    // Alert user
    alert_user("Malware detected and network isolated"_string);

    // Optionally kill process
    kill(pid, SIGKILL);
}
```

### 3. Temporary Isolation for Analysis

```cpp
// Isolate process for behavioral analysis
TRY(m_network_isolation->isolate_process(pid, "Behavioral analysis"_string));

// Run analysis (5 seconds)
sleep(5);

// Restore if clean
if (analysis_result.is_clean()) {
    TRY(m_network_isolation->restore_process(pid));
}
```

## API Reference

### NetworkIsolationManager

```cpp
class NetworkIsolationManager {
public:
    // Create manager (auto-detect backend)
    static ErrorOr<NonnullOwnPtr<NetworkIsolationManager>> create(
        FirewallBackend backend = FirewallBackend::Auto,
        bool dry_run = false);

    // Isolate single process
    ErrorOr<void> isolate_process(pid_t pid, String const& reason);

    // Restore network access
    ErrorOr<void> restore_process(pid_t pid);

    // Isolate process tree (parent + children)
    ErrorOr<void> isolate_process_tree(pid_t root_pid);

    // Query isolated processes
    ErrorOr<Vector<IsolatedProcess>> list_isolated_processes() const;
    bool is_process_isolated(pid_t pid) const;

    // Cleanup all isolations
    ErrorOr<void> cleanup_all();

    // Statistics
    Statistics get_statistics() const;

    // Backend info
    FirewallBackend get_backend() const;
};
```

### IsolatedProcess

```cpp
struct IsolatedProcess {
    pid_t pid;
    String reason;
    UnixDateTime isolated_at;
    Vector<String> firewall_rules;
};
```

### Statistics

```cpp
struct Statistics {
    size_t total_isolated_processes;
    size_t total_rules_applied;
    size_t total_cleanup_operations;
    size_t active_isolated_processes;
};
```

## Further Reading

- **SECURITY_CONSIDERATIONS.md**: Comprehensive security analysis
- **IMPLEMENTATION_REPORT.md**: Technical implementation details
- **NetworkIsolationManager.h**: Full API documentation

## Support

For issues or questions:
1. Check logs: `tail -f sentinel.log`
2. Run tests: `./Build/release/bin/TestNetworkIsolationManager`
3. Enable dry-run mode for debugging
4. Review security considerations document
