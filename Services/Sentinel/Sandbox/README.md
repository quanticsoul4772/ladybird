# Sentinel Sandbox Components

This directory contains components for behavioral analysis of suspicious files using sandboxing technology.

## Quick Start

### 1. Install nsjail

Run the installation script with sudo privileges:

```bash
cd /home/rbsmith4/ladybird/Services/Sentinel/Sandbox
./install_nsjail.sh
```

This script will:
- Try to install nsjail via package manager (fastest)
- If unavailable, build from source automatically
- Install all required dependencies
- Verify the installation

### 2. Verify Installation

Run the verification test suite:

```bash
./verify_nsjail.sh
```

This will run 12+ comprehensive tests covering:
- Basic execution
- Resource limits (memory, CPU, file size)
- Time limit enforcement
- Namespace isolation (user, PID, UTS, mount)
- Multiple consecutive executions
- Working directory and environment isolation

### 3. Update Documentation

After successful installation, update `NSJAIL_INSTALLATION.md` with:
- Installed version
- Installation date
- Installation method used
- Binary location

## Files in This Directory

### Installation & Verification

- **`install_nsjail.sh`** - Automated installation script
  - Handles both package manager and source builds
  - Installs all dependencies
  - Verifies installation
  - Cleans up after build

- **`verify_nsjail.sh`** - Comprehensive test suite
  - 12+ verification tests
  - Colored output for easy reading
  - Success/failure reporting
  - Exit codes: 0 = success, 1 = failure

- **`NSJAIL_INSTALLATION.md`** - Complete installation guide
  - System requirements
  - Manual installation steps
  - Troubleshooting guide
  - Integration documentation

### Behavioral Analysis Components (Coming Soon)

- **`BehavioralAnalyzer.h`** - Header for behavioral analysis engine
- **`BehavioralAnalyzer.cpp`** - Implementation of sandboxed file analysis
- **`BehavioralSignatures.h`** - Malware behavior signatures
- **`SandboxConfig.h`** - nsjail configuration templates

## Usage Examples

### Basic File Execution in Sandbox

```bash
# Execute a suspicious file with 30-second timeout and 256MB memory limit
nsjail --mode o \
       --time_limit 30 \
       --rlimit_as 256 \
       --rlimit_cpu 10 \
       --disable_clone_newnet true \
       -- /path/to/suspicious_file
```

### Sandboxed Script Analysis

```bash
# Execute a shell script in isolated environment
nsjail --mode o \
       --time_limit 60 \
       --rlimit_as 512 \
       --cwd /tmp/sandbox \
       -- /bin/bash /path/to/script.sh
```

### Network-Isolated Execution

```bash
# Run a binary with network completely disabled
nsjail --mode o \
       --time_limit 30 \
       --disable_clone_newnet true \
       -- /path/to/binary
```

## Integration with Sentinel

The Behavioral Analyzer will use nsjail to:

1. **Execute Suspicious Files** - Run files in isolated environment
2. **Monitor Behavior** - Track system calls, file access, network attempts
3. **Detect Malicious Patterns** - Match behavior against signatures
4. **Resource Control** - Enforce strict CPU, memory, and time limits
5. **Prevent Escape** - Multiple layers of isolation (user, PID, mount, network)

### Example Integration (Pseudocode)

```cpp
// In BehavioralAnalyzer.cpp
ErrorOr<BehavioralAnalysisResult> BehavioralAnalyzer::analyze_file(String file_path) {
    // Prepare nsjail environment
    auto sandbox_config = prepare_sandbox_environment();

    // Execute in nsjail with monitoring
    auto execution_result = TRY(execute_in_nsjail(file_path, sandbox_config));

    // Analyze behavior
    auto behavior_score = TRY(analyze_behavior(execution_result));

    // Match against signatures
    auto matched_signatures = match_behavioral_signatures(behavior_score);

    return BehavioralAnalysisResult {
        .is_malicious = behavior_score.total_score > MALICIOUS_THRESHOLD,
        .confidence = behavior_score.confidence,
        .matched_signatures = matched_signatures,
        .execution_trace = execution_result.trace
    };
}
```

## Security Considerations

### nsjail Isolation Layers

1. **Linux Namespaces**
   - UTS: Isolated hostname
   - Mount: Separate filesystem view
   - PID: Isolated process tree
   - IPC: Isolated IPC mechanisms
   - Network: Isolated networking (can be disabled)
   - User: UID/GID mapping
   - Cgroup: Resource control

2. **Resource Limits** (rlimits)
   - `RLIMIT_AS`: Virtual memory limit
   - `RLIMIT_CPU`: CPU time limit
   - `RLIMIT_FSIZE`: Maximum file size
   - `RLIMIT_NOFILE`: Maximum open files
   - `RLIMIT_NPROC`: Maximum processes

3. **Seccomp-BPF** (Optional)
   - Syscall filtering
   - Block dangerous operations
   - Configured via kafel language

### Best Practices

1. **Always Use Time Limits** - Prevent infinite loops
2. **Limit Memory** - Prevent memory exhaustion attacks
3. **Disable Network** - Prevent C&C communication during analysis
4. **Limit File Operations** - Prevent disk filling
5. **Monitor Syscalls** - Detect malicious behavior patterns
6. **Log Everything** - Maintain audit trail

## Troubleshooting

### nsjail Not Found

```bash
# Check if in PATH
which nsjail

# If not found, verify installation
ls -la /usr/local/bin/nsjail

# Add to PATH if needed
export PATH="/usr/local/bin:$PATH"
```

### Permission Denied Errors

```bash
# Check if binary is executable
chmod +x /usr/local/bin/nsjail

# Check kernel support for user namespaces
ls /proc/self/ns/
cat /proc/sys/kernel/unprivileged_userns_clone  # Should be 1
```

### Build Failures

See `NSJAIL_INSTALLATION.md` troubleshooting section for:
- Missing dependencies
- Build errors
- Kernel compatibility issues

## Next Steps

After completing nsjail installation and verification:

1. ✓ Install nsjail (`install_nsjail.sh`)
2. ✓ Verify installation (`verify_nsjail.sh`)
3. ✓ Update documentation with version info
4. → Create Sentinel configuration file (`SentinelConfig.cpp`)
5. → Implement BehavioralAnalyzer class
6. → Add behavioral signatures database
7. → Integrate with PolicyGraph
8. → Test with sample malware (in sandbox!)

## References

- **nsjail GitHub**: https://github.com/google/nsjail
- **nsjail Documentation**: https://github.com/google/nsjail/blob/master/README.md
- **Linux Namespaces**: https://man7.org/linux/man-pages/man7/namespaces.7.html
- **Seccomp-BPF**: https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html
- **Kafel Language**: https://github.com/google/kafel

## License

nsjail is licensed under Apache License 2.0.
See: https://github.com/google/nsjail/blob/master/LICENSE
