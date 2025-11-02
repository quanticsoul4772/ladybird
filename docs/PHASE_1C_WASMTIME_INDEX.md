# Phase 1c Wasmtime Installation Index

**Milestone:** 0.5 Phase 1c - Module Loading
**Created:** 2025-11-01
**Status:** Documentation Complete

## Overview

This collection provides complete installation and integration documentation for Wasmtime v38.0.3 in the Ladybird Browser's Sentinel security system.

Wasmtime enables real WebAssembly execution for Sentinel's malware detection sandbox, replacing stub mode with actual WASM module analysis. This is optional - Sentinel works perfectly without it.

## Documents

### 1. Quick Start Guide
**File:** `docs/PHASE_1C_WASMTIME_QUICK_START.md`
**Audience:** Developers who just want to install and go
**Time:** 5-10 minutes

Best for:
- First-time installation
- One-command setup
- Quick verification
- Common issues

**Start here if:** You want to get Wasmtime working immediately.

### 2. Complete Installation Guide
**File:** `docs/WASMTIME_INSTALLATION.md`
**Audience:** All users (developers, testers, operators)
**Time:** 15-30 minutes for full read

Contains:
- Automated installation script guide
- Platform-specific manual installation (Linux, macOS)
- Step-by-step verification
- Comprehensive troubleshooting
- Performance expectations
- Build from source instructions
- Uninstall guide

**Start here if:** You need detailed instructions or are troubleshooting.

### 3. CMake Integration Guide
**File:** `Services/Sentinel/Sandbox/WASMTIME_CMAKE_INTEGRATION.md`
**Audience:** Build system developers, CI/CD engineers
**Time:** 20-30 minutes

Covers:
- CMake detection mechanism
- Custom installation paths
- Build variables and flags
- Compilation configuration
- Platform-specific notes
- Debugging detection issues
- Custom FindWasmtime.cmake patterns

**Start here if:** You're modifying the build system or CI/CD.

### 4. Installation Script
**File:** `Services/Sentinel/Sandbox/install_wasmtime.sh`
**Type:** Bash script (executable)
**Status:** Ready to use

Features:
- Automatic platform detection
- Error handling and fallbacks
- User-local install fallback
- Post-install verification
- Next steps guidance

**Run:** `./Services/Sentinel/Sandbox/install_wasmtime.sh`

## Quick Reference

### For End Users

**Install Wasmtime:**
```bash
cd /home/rbsmith4/ladybird
./Services/Sentinel/Sandbox/install_wasmtime.sh
```

**Verify installation:**
```bash
ls /usr/local/wasmtime/lib/libwasmtime.so
./Build/release/bin/TestSandbox
```

**Uninstall:**
```bash
rm -rf /usr/local/wasmtime
```

### For Build System Developers

**Check detection:**
```bash
grep -i wasmtime Build/release/CMakeCache.txt
```

**Custom path:**
```bash
cmake --preset Release -DCMAKE_PREFIX_PATH="/path/to/wasmtime"
```

**Debugging:**
```bash
cmake --trace-expand Build/release 2>&1 | grep -i wasmtime
```

## File Structure

```
/home/rbsmith4/ladybird/
├── Services/Sentinel/Sandbox/
│   ├── install_wasmtime.sh                  (Automated installer)
│   ├── WASMTIME_CMAKE_INTEGRATION.md        (Build system guide)
│   ├── WasmExecutor.h                       (Implementation)
│   ├── WasmExecutor.cpp
│   └── test_samples/                        (Test data)
└── docs/
    ├── PHASE_1C_WASMTIME_QUICK_START.md    (Quick guide)
    ├── WASMTIME_INSTALLATION.md             (Full guide)
    ├── PHASE_1C_WASMTIME_INDEX.md           (This file)
    └── [Other Sentinel docs...]
```

## Installation Paths

Wasmtime is installed to one of these locations:

| Path | Context | Default |
|------|---------|---------|
| `/usr/local/wasmtime` | System-wide (Linux/macOS) | Yes |
| `~/wasmtime` | User home directory | Fallback |
| Custom path | Via CMAKE_PREFIX_PATH | Advanced |

CMake automatically searches all these locations.

## Verification Checklist

After installation:

- [ ] Run installation script: `./install_wasmtime.sh`
- [ ] Verify files exist: `ls /usr/local/wasmtime/lib/libwasmtime.*`
- [ ] Rebuild Ladybird: `cmake --preset Release && cmake --build Build/release --target sentinelservice`
- [ ] Check CMake detected it: `grep Wasmtime Build/release/CMakeCache.txt`
- [ ] Run tests: `./Build/release/bin/TestSandbox`
- [ ] Check performance: `./Services/Sentinel/Sandbox/benchmark_sandbox.sh`

All tests should pass with timing ~20-50ms per sample (real WASM mode).

## Performance Impact

| Metric | Stub Mode | Real WASM |
|--------|-----------|-----------|
| Startup | <1ms | <1ms |
| Per-sample | 1-5ms | 20-50ms |
| Memory | ~1MB | ~10MB |
| Accuracy | Limited | Full |

Real WASM is ~15x slower but significantly more accurate for malware detection.

## Troubleshooting Quick Links

| Problem | Solution |
|---------|----------|
| "Wasmtime not found" | See WASMTIME_INSTALLATION.md > Troubleshooting > Issue 1 |
| CMake doesn't detect | See WASMTIME_CMAKE_INTEGRATION.md > Troubleshooting |
| Linker errors | See WASMTIME_INSTALLATION.md > Troubleshooting > Issue 3 |
| Tests in stub mode | See PHASE_1C_WASMTIME_QUICK_START.md > Common Issues |
| Download fails | See WASMTIME_INSTALLATION.md > Troubleshooting > Issue 4 |

## Related Documentation

For additional context, see:
- `docs/SENTINEL_ARCHITECTURE.md` - Sentinel system overview
- `Services/Sentinel/Sandbox/WasmExecutor.h` - Implementation interface
- `Documentation/BuildInstructionsLadybird.md` - Build system overview
- `docs/PHASE_1C_INTEGRATION_GUIDE.md` - Full Phase 1c integration testing

## Common Tasks

### Task: Install Wasmtime
1. Read: `PHASE_1C_WASMTIME_QUICK_START.md`
2. Run: `./Services/Sentinel/Sandbox/install_wasmtime.sh`
3. Verify: `./Build/release/bin/TestSandbox`

### Task: Troubleshoot CMake detection
1. Check file: `Services/Sentinel/Sandbox/WASMTIME_CMAKE_INTEGRATION.md`
2. Verify paths: `ls /usr/local/wasmtime/lib/libwasmtime.*`
3. Rebuild: `cmake --preset Release`

### Task: Install to custom location
1. Check: `WASMTIME_CMAKE_INTEGRATION.md` > Custom Installation Path
2. Use: `export CMAKE_PREFIX_PATH="/custom/path:$CMAKE_PREFIX_PATH"`
3. Configure: `cmake --preset Release`

### Task: Debug build failures
1. Read: `WASMTIME_CMAKE_INTEGRATION.md` > Debugging CMake Detection
2. Run: `cmake --trace-expand Build/release 2>&1 | grep wasmtime`
3. Check: `grep -i wasmtime Build/release/CMakeOutput.log`

## Support & Resources

**Documentation:**
- Complete installation guide: `docs/WASMTIME_INSTALLATION.md`
- CMake integration: `Services/Sentinel/Sandbox/WASMTIME_CMAKE_INTEGRATION.md`
- Quick start: `docs/PHASE_1C_WASMTIME_QUICK_START.md`

**Official Resources:**
- Wasmtime GitHub: https://github.com/bytecodealliance/wasmtime
- Wasmtime C API: https://github.com/bytecodealliance/wasmtime/tree/main/crates/c-api
- Wasmtime Releases: https://github.com/bytecodealliance/wasmtime/releases

**Implementation:**
- `Services/Sentinel/Sandbox/WasmExecutor.h` - Interface
- `Services/Sentinel/Sandbox/WasmExecutor.cpp` - Implementation
- Test samples: `Services/Sentinel/Sandbox/test_samples/`

## Version Information

- **Wasmtime Version:** 38.0.3
- **Documentation Created:** 2025-11-01
- **Ladybird Branch:** master
- **Sentinel Version:** 0.5 Phase 1c

## Status Tracking

| Component | Status | Notes |
|-----------|--------|-------|
| Installation script | ✅ Complete | Ready for use |
| Installation guide | ✅ Complete | Comprehensive coverage |
| CMake integration | ✅ Complete | Full documentation |
| Quick start guide | ✅ Complete | 5-minute setup |
| WasmExecutor | ✅ Implemented | Real WASM execution |
| Tests | ✅ Passing | Full suite functional |
| Benchmarks | ✅ Configured | Performance tracking |

## Next Steps

After installing Wasmtime:

1. **Run integration tests:** `./Services/Sentinel/Sandbox/run_integration_tests.sh`
2. **Test in browser:** Load Ladybird and test with malware samples
3. **Review performance:** Compare stub mode vs. real WASM timing
4. **Deploy:** Configure WASM module path in production
5. **Monitor:** Track detection accuracy and performance metrics

## Document Change Log

| Date | Change | File |
|------|--------|------|
| 2025-11-01 | Initial creation | All files |
| 2025-11-01 | Automated installer | install_wasmtime.sh |
| 2025-11-01 | Installation guide | WASMTIME_INSTALLATION.md |
| 2025-11-01 | Quick start | PHASE_1C_WASMTIME_QUICK_START.md |
| 2025-11-01 | CMake guide | WASMTIME_CMAKE_INTEGRATION.md |
| 2025-11-01 | Index | PHASE_1C_WASMTIME_INDEX.md |

---

**Last Updated:** 2025-11-01
**For Questions:** See relevant documentation or Sentinel architecture docs
