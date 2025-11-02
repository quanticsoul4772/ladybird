# nsjail Installation Guide

## Installation Status

**Status**: Requires manual installation (sudo privileges needed)

**System Information**:
- OS: Ubuntu 22.04.5 LTS (Jammy Jellyfish)
- Architecture: x86_64
- Current Status: nsjail NOT installed

## Installation Options

### Option 1: Package Manager (Recommended for Ubuntu 20.04+)

This is the simplest method if you have sudo access:

```bash
sudo apt-get update
sudo apt-get install -y nsjail
```

### Option 2: Build from Source

If the package is not available or you need a specific version:

#### Step 1: Install Build Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
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
```

#### Step 2: Clone and Build nsjail

```bash
# Clone repository
cd /tmp
git clone https://github.com/google/nsjail.git
cd nsjail

# Initialize submodules (required for kafel)
git submodule update --init

# Build nsjail
make -j$(nproc)

# Install to system (requires sudo)
sudo cp nsjail /usr/local/bin/

# OR install to user directory (no sudo required)
mkdir -p ~/bin
cp nsjail ~/bin/
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### Step 3: Verify Installation

```bash
# Check version
nsjail --version

# Display help
nsjail --help
```

## Current Build Status

**Attempted**: Build from source at `/tmp/nsjail`

**Result**: Build failed due to missing dependencies

**Missing Dependencies**:
- `flex` - Fast lexical analyzer generator (required for kafel submodule)
- `bison` - Parser generator (required for kafel submodule)
- `libprotobuf-dev` - Protocol buffers development files
- `libnl-route-3-dev` - Netlink route library development files
- Build tools: `gcc`, `g++`, `make`, `pkg-config`

**Dependencies Already Present**:
- `libprotobuf23` - Protocol buffers runtime library

## Verification Tests

Once nsjail is installed, run these tests to verify it works correctly:

### Test 1: Basic Execution

```bash
nsjail --mode o --time_limit 5 -- /bin/echo "Hello from nsjail"
```

**Expected Output**:
```
[2025-11-02T12:20:00+0000][I][init(236)] Mode: STANDALONE_ONCE
[2025-11-02T12:20:00+0000][I][init(237)] Jail parameters: hostname:'NSJAIL', chroot:'(null)', process:'/bin/echo', bind:[::]:0, max_conns:0, max_conns_per_ip:0, time_limit:5, personality:0, daemonize:false, clone_newnet:true, clone_newuser:true, clone_newns:true, clone_newpid:true, clone_newipc:true, clone_newuts:true, clone_newcgroup:false, keep_caps:false, tmpfs_size:4194304, disable_no_new_privs:false, max_cpus:0
Hello from nsjail
[2025-11-02T12:20:00+0000][I][subprocDone(354)] PID: 12345 exited with status: 0, (PIDs left: 0)
```

### Test 2: Resource Limits

```bash
nsjail --mode o --rlimit_as 128 --rlimit_fsize 1 -- /bin/ls /tmp
```

**Expected**: Command executes with memory and file size limits applied

### Test 3: Network Isolation

```bash
nsjail --mode o --disable_clone_newnet false -- ping -c 1 8.8.8.8
```

**Expected**: Network access should work (or be blocked if `--disable_clone_newnet true`)

### Test 4: Time Limit Enforcement

```bash
nsjail --mode o --time_limit 2 -- /bin/sleep 10
```

**Expected**: Process is killed after 2 seconds

### Test 5: User Namespace Isolation

```bash
nsjail --mode o -- /usr/bin/id
```

**Expected**: Shows isolated user ID (typically uid=1000 mapped to uid=0 inside jail)

## Integration with Sentinel Behavioral Analysis

Once installed, nsjail will be used for:

1. **Sandboxed File Analysis**: Execute suspicious files in isolated environment
2. **Behavioral Monitoring**: Observe file behavior without risk to host system
3. **Resource Control**: Limit CPU, memory, and execution time for analysis
4. **Network Isolation**: Prevent malware from communicating during analysis
5. **File System Isolation**: Monitor file operations in controlled environment

### Sentinel Configuration

nsjail will be integrated via `Services/Sentinel/Sandbox/BehavioralAnalyzer.cpp`:

```cpp
// Example nsjail command for behavioral analysis
nsjail_cmd = {
    "nsjail",
    "--mode", "o",                          // Once mode
    "--time_limit", "30",                   // 30 second max execution
    "--rlimit_as", "256",                   // 256MB memory limit
    "--rlimit_cpu", "10",                   // 10 CPU seconds max
    "--rlimit_fsize", "10",                 // 10MB file size limit
    "--disable_clone_newnet", "true",       // Disable network access
    "--cwd", "/tmp/sandbox",                // Working directory
    "--chroot", "/tmp/sandbox_root",        // Chroot to isolated FS
    "--",
    suspicious_file_path                     // File to execute
};
```

## Version Information

- **nsjail Repository**: https://github.com/google/nsjail
- **Kafel Version**: 6b38446a6ec44f65e34c1e1fd44ac61659a9f724
- **Documentation**: https://github.com/google/nsjail/blob/master/README.md

## Troubleshooting

### Build Errors

**Error**: `flex: No such file or directory`
**Solution**: Install flex: `sudo apt-get install flex`

**Error**: `bison: No such file or directory`
**Solution**: Install bison: `sudo apt-get install bison`

**Error**: `fatal error: google/protobuf/...`
**Solution**: Install protobuf dev files: `sudo apt-get install libprotobuf-dev protobuf-compiler`

**Error**: `fatal error: netlink/...`
**Solution**: Install netlink dev files: `sudo apt-get install libnl-route-3-dev`

### Runtime Errors

**Error**: `FATAL: Cannot create PID namespace`
**Solution**: Check kernel support: `ls /proc/self/ns/`

**Error**: `FATAL: Cannot create user namespace`
**Solution**: Check `/proc/sys/kernel/unprivileged_userns_clone` or run with sudo

**Error**: `Permission denied` when executing
**Solution**: Ensure nsjail binary has execute permissions: `chmod +x /usr/local/bin/nsjail`

## Next Steps

After successful installation:

1. Run all verification tests above
2. Document installed version in this file
3. Proceed to Phase 2, Week 1, Task 2: Create Sentinel configuration file
4. Implement BehavioralAnalyzer.cpp with nsjail integration

## Installation Completion Checklist

- [ ] nsjail binary installed and in PATH
- [ ] `nsjail --version` works
- [ ] `nsjail --help` displays usage
- [ ] Test 1: Basic execution passes
- [ ] Test 2: Resource limits work
- [ ] Test 3: Network isolation verified
- [ ] Test 4: Time limits enforced
- [ ] Test 5: User namespace isolation confirmed
- [ ] Version documented below

---

## Installed Version

**Version**: _Not yet installed_

**Installation Date**: _Pending_

**Installation Method**: _Pending (Package manager or Build from source)_

**Binary Location**: _Pending (/usr/local/bin/nsjail or ~/bin/nsjail)_

**Build Hash (if from source)**: _Pending_
