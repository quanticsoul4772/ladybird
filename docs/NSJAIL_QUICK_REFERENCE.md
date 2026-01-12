# NsJail Quick Reference for Sentinel Integration

**Quick Start**: Essential commands and configurations for Phase 2 implementation

---

## Installation (One-Liner)

```bash
# Ubuntu/Debian
sudo apt install -y autoconf bison flex gcc g++ git libprotobuf-dev libnl-route-3-dev libtool make pkg-config protobuf-compiler && \
cd /tmp && git clone https://github.com/google/nsjail.git && cd nsjail && make -j$(nproc) && sudo cp nsjail /usr/local/bin/

# Arch Linux
sudo pacman -S nsjail

# Verify
nsjail --version
```

---

## Essential Configuration Files

### Location
```
/etc/sentinel/nsjail-malware-sandbox.cfg    # Main config
/etc/sentinel/malware-sandbox.kafel         # Seccomp policy
```

### Minimal Config (for testing)
```protobuf
name: "Quick Test"
mode: ONCE
time_limit: 10

clone_newnet: true
clone_newuser: true
clone_newns: true
clone_newpid: true

uidmap { inside_id: "0" outside_id: "1000" count: 1 }
gidmap { inside_id: "0" outside_id: "1000" count: 1 }

mount { src: "/bin" dst: "/bin" is_bind: true rw: false }
mount { src: "/lib" dst: "/lib" is_bind: true rw: false }
mount { src: "/usr" dst: "/usr" is_bind: true rw: false }
mount { dst: "/tmp" fstype: "tmpfs" rw: true }

rlimit_as: 128
rlimit_cpu: 10

exec_bin { path: "/bin/sh" arg: "-c" }
```

---

## Common Commands

### Basic Execution
```bash
# Run with config
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /path/to/file:/sandbox/file \
    -- /sandbox/file

# Quick test (no config)
nsjail -Mo --chroot /tmp --user 99999 -- /bin/echo "test"
```

### With Monitoring
```bash
# With strace (slow but detailed)
strace -f -o analysis.log \
    nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /suspicious/file:/sandbox/target \
    -- /sandbox/target

# With timeout
timeout 30s nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    -- /sandbox/target
```

### Network Isolation
```bash
# Fully isolated (no network)
nsjail --clone_newnet --iface_no_lo ...

# With monitoring (MACVLAN)
nsjail --macvlan_iface eth0 --macvlan_vs_ip 10.99.0.2 ...
```

---

## Kafel Seccomp Policy Template

```kafel
POLICY basic_malware_sandbox {
    # Allow file operations
    ALLOW { read, write, open, openat, close, stat, fstat, mmap, munmap, brk }

    # Log suspicious operations
    LOG { socket, connect, execve, fork, clone, ptrace }

    # Block dangerous operations
    ERRNO(EPERM) { init_module, delete_module, reboot, mount, setuid, setgid }
}
DEFAULT KILL
```

**Save to**: `/etc/sentinel/malware-sandbox.kafel`

**Use in config**:
```protobuf
seccomp_policy_file: "/etc/sentinel/malware-sandbox.kafel"
seccomp_log: true
```

---

## C++ Integration Skeleton

```cpp
// Services/Sentinel/BehavioralAnalyzer.h
#pragma once
#include <AK/Error.h>
#include <AK/String.h>

namespace Sentinel {

struct BehavioralReport {
    u8 threat_score { 0 };
    bool attempted_network_access { false };
    bool attempted_process_creation { false };
};

class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create(
        StringView nsjail_path,
        StringView config_path);

    ErrorOr<BehavioralReport> analyze_file(StringView file_path, u32 timeout_seconds = 30);

private:
    BehavioralAnalyzer(String nsjail_path, String config_path);

    String m_nsjail_path;
    String m_config_path;
};

}
```

```cpp
// Services/Sentinel/BehavioralAnalyzer.cpp
#include "BehavioralAnalyzer.h"
#include <LibCore/Process.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> BehavioralAnalyzer::create(
    StringView nsjail_path, StringView config_path)
{
    return adopt_nonnull_own_or_enomem(new (nothrow) BehavioralAnalyzer(
        TRY(String::from_utf8(nsjail_path)),
        TRY(String::from_utf8(config_path))
    ));
}

ErrorOr<BehavioralReport> BehavioralAnalyzer::analyze_file(
    StringView file_path, u32 timeout_seconds)
{
    // 1. Prepare sandbox
    auto sandbox_dir = TRY(String::formatted("/var/sentinel/sandbox/{}", getpid()));
    TRY(Core::System::mkdir(sandbox_dir, 0700));

    // 2. Build command
    Vector<StringView> args {
        m_nsjail_path.bytes_as_string_view(),
        "--config"sv, m_config_path.bytes_as_string_view(),
        "--bindmount"sv, TRY(String::formatted("{}:/sandbox", sandbox_dir)).bytes_as_string_view(),
        "--"sv,
        "/sandbox/file_to_analyze"sv
    };

    // 3. Spawn nsjail
    auto process = TRY(Core::Process::spawn(args[0], args, {}, {}));

    // 4. Wait for completion
    int status;
    waitpid(process.pid(), &status, 0);

    // 5. Analyze results
    BehavioralReport report;
    // TODO: Parse strace/seccomp logs

    return report;
}

}
```

---

## Testing Commands

```bash
# 1. Test nsjail installation
nsjail --help

# 2. Test namespace support
nsjail -Mo --chroot /tmp -- /bin/echo "Namespaces work!"

# 3. Test config file
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg -- /bin/ls

# 4. Test with benign file
echo '#!/bin/sh\necho "Hello from sandbox"' > /tmp/test.sh
chmod +x /tmp/test.sh
nsjail --config /etc/sentinel/nsjail-malware-sandbox.cfg \
    --bindmount /tmp/test.sh:/sandbox/test \
    -- /sandbox/test

# 5. Test resource limits (should timeout)
nsjail --time_limit 5 -- /bin/sleep 10
```

---

## Troubleshooting

### Error: "clone(CLONE_NEWUSER) failed"
**Cause**: User namespaces disabled
**Fix**:
```bash
sudo sysctl -w kernel.unprivileged_userns_clone=1
echo "kernel.unprivileged_userns_clone=1" | sudo tee -a /etc/sysctl.conf
```

### Error: "pivot_root failed"
**Cause**: Filesystem doesn't support pivot_root
**Fix**: Add `no_pivotroot: true` to config

### Error: "CLONE_NEWCGROUP not supported"
**Cause**: Old kernel (< 4.6)
**Fix**: Upgrade kernel or remove `clone_newcgroup: true`

### High CPU usage with strace
**Expected**: strace adds 100-200x overhead
**Fix**: Use strace selectively, or wait for eBPF implementation

---

## Performance Benchmarks

| Configuration | Overhead | Use Case |
|---------------|----------|----------|
| nsjail only | ~1-2% | Production |
| nsjail + seccomp LOG | ~3-5% | Production |
| nsjail + strace | ~100-200x | Deep analysis only |

---

## Key Resources

- **Full Research**: `/home/rbsmith4/ladybird/docs/NSJAIL_RESEARCH_SUMMARY.md`
- **Official Docs**: https://github.com/google/nsjail
- **Kafel Language**: https://google.github.io/kafel/
- **Example Configs**: https://github.com/google/nsjail/tree/master/configs

---

## Next Steps for Phase 2

1. ✅ Research complete (this document)
2. ⬜ Install nsjail on dev system
3. ⬜ Create `/etc/sentinel/` configs
4. ⬜ Implement `BehavioralAnalyzer.{h,cpp}`
5. ⬜ Write unit tests (`TestBehavioralAnalyzer.cpp`)
6. ⬜ Integrate with MalwareDetector (Tier 2 escalation)
7. ⬜ Test with sample files
8. ⬜ Document in `SENTINEL_ARCHITECTURE.md`

---

**Status**: Ready for Implementation
**Estimated Time**: 2-3 days for basic integration
