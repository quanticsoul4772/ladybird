# Wasmtime Installation Guide

**For:** Milestone 0.5 Phase 1c - Module Loading
**Version:** Wasmtime v38.0.3
**Status:** Optional (Sentinel works in stub mode without Wasmtime)
**Last Updated:** 2025-11-01

## Overview

Wasmtime is a lightweight, embeddable WebAssembly runtime used by Sentinel's malware detection sandbox to execute WASM modules. This guide covers installation options and troubleshooting.

**Important:** Sentinel functions completely without Wasmtime - it will automatically fall back to stub mode. Installing Wasmtime enables real WASM execution for more accurate malware analysis.

## Quick Install (Automated)

The easiest way to install is using the provided installation script:

```bash
cd /home/rbsmith4/ladybird
./Services/Sentinel/Sandbox/install_wasmtime.sh
```

This script will:
1. Detect your platform (Linux/macOS, x86_64/aarch64)
2. Download Wasmtime v38.0.3 from GitHub releases
3. Install to `/usr/local/wasmtime` (or `~/wasmtime` if no sudo)
4. Verify the installation
5. Print next steps

**Expected output:**
```
=== Wasmtime Installation Script ===
This script will install Wasmtime v38.0.3 for Sentinel sandbox

Detected: linux on x86_64
Download URL: https://github.com/bytecodealliance/wasmtime/...
[...]
=== Installation Complete ===
Installed to: /usr/local/wasmtime
```

## Manual Installation

If you prefer to install manually, choose your platform:

### Linux x86_64

```bash
# Download
curl -LO https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-linux.tar.xz

# Extract
tar xf wasmtime-v38.0.3-x86_64-linux.tar.xz

# Install (requires sudo)
sudo mv wasmtime-v38.0.3-x86_64-linux /usr/local/wasmtime

# Verify
ls -lh /usr/local/wasmtime/lib/libwasmtime.so
ls -lh /usr/local/wasmtime/include/wasmtime.h
```

### Linux aarch64 (ARM64)

```bash
curl -LO https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-linux.tar.xz
tar xf wasmtime-v38.0.3-aarch64-linux.tar.xz
sudo mv wasmtime-v38.0.3-aarch64-linux /usr/local/wasmtime
```

### macOS (Apple Silicon)

```bash
curl -LO https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-macos.tar.xz
tar xf wasmtime-v38.0.3-aarch64-macos.tar.xz
sudo mv wasmtime-v38.0.3-aarch64-macos /usr/local/wasmtime
```

### macOS (Intel)

```bash
curl -LO https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-macos.tar.xz
tar xf wasmtime-v38.0.3-x86_64-macos.tar.xz
sudo mv wasmtime-v38.0.3-x86_64-macos /usr/local/wasmtime
```

## Rebuild Ladybird

After installing Wasmtime, rebuild Ladybird to detect and link against it:

```bash
cd /home/rbsmith4/ladybird
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

**Expected CMake output:**
```
-- Found Wasmtime: /usr/local/wasmtime
-- Wasmtime version: 38.0.3
-- Wasmtime library: /usr/local/wasmtime/lib/libwasmtime.so
-- Wasmtime include: /usr/local/wasmtime/include
```

If you don't see these lines, Wasmtime was not detected. See **Troubleshooting** below.

## Verify Installation

Check that Wasmtime is properly installed and detected:

```bash
# Check Wasmtime files
ls -lh /usr/local/wasmtime/lib/
ls -lh /usr/local/wasmtime/include/

# Check CMake detected it
grep -i wasmtime Build/release/CMakeCache.txt

# Run unit tests
./Build/release/bin/TestSandbox
```

Expected test output:
```
TestSandbox: Running sandbox tests...
[OK] WasmExecutor initialization
[OK] Module loading from file
[OK] Function execution
[OK] Error handling
All tests passed!
```

## System Requirements

### Minimum
- **OS:** Linux (x86_64/aarch64) or macOS (Intel/Apple Silicon)
- **Disk Space:** ~100MB for extracted files
- **Download:** ~50MB

### Build Requirements
- **Compiler:** GCC 11+ or Clang 14+
- **CMake:** 3.20+
- **xz-utils:** For extracting tar.xz files

Check your system:
```bash
uname -m                    # Architecture
lsb_release -d              # Linux distribution
cmake --version
gcc --version
```

## Troubleshooting

### Issue: "Wasmtime not found" after installation

Wasmtime files aren't where CMake expects them.

**Solution 1:** Verify files exist:
```bash
ls -lh /usr/local/wasmtime/lib/libwasmtime.*
ls -lh /usr/local/wasmtime/include/wasmtime.h
```

If not found, check for installation in user directory:
```bash
ls -lh ~/wasmtime/lib/
ls -lh ~/wasmtime/include/
```

**Solution 2:** Clear CMake cache and reconfigure:
```bash
cd /home/rbsmith4/ladybird
rm -rf Build/release/CMakeCache.txt
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

### Issue: Permission denied during installation

**Solution:** Install to user directory instead:
```bash
# When installing manually
mv wasmtime-v38.0.3-x86_64-linux ~/wasmtime
chmod -R u+rwx ~/wasmtime

# Or re-run the script (it will detect this)
./Services/Sentinel/Sandbox/install_wasmtime.sh
```

### Issue: Wasmtime library is found but CMake build still fails

Library found but linker can't locate it.

**Solution:** Check for library format issues:
```bash
# Verify library is properly formatted
file /usr/local/wasmtime/lib/libwasmtime.so

# Check library symbols
nm /usr/local/wasmtime/lib/libwasmtime.so | head -20
```

If this shows errors, Wasmtime may be corrupted. Reinstall:
```bash
rm -rf /usr/local/wasmtime
./Services/Sentinel/Sandbox/install_wasmtime.sh
```

### Issue: Download fails (network error, etc.)

**Solution:** Download manually from GitHub:
1. Visit: https://github.com/bytecodealliance/wasmtime/releases/tag/v38.0.3
2. Download the appropriate `.tar.xz` file for your platform
3. Extract: `tar xf wasmtime-v38.0.3-*.tar.xz`
4. Install: `sudo mv wasmtime-v38.0.3-* /usr/local/wasmtime`

### Issue: curl/wget not available

Install the download tool:
```bash
# Ubuntu/Debian
sudo apt-get install curl

# macOS
brew install curl
```

Then re-run the installation script.

### Issue: TestSandbox doesn't show expected output

Tests are running in stub mode (Wasmtime not enabled).

**Check the build log:**
```bash
# Search for Wasmtime detection
grep "Wasmtime" Build/release/CMakeCache.txt

# Check CMake configuration
grep "ENABLE_WASMTIME" Build/release/CMakeCache.txt
```

If `ENABLE_WASMTIME` is OFF, Wasmtime wasn't detected. Follow Solution 2 in the "Wasmtime not found" issue above.

## Performance Expectations

| Mode | Startup | Per-Request | Memory |
|------|---------|------------|--------|
| Stub | <1ms | 1-5ms | ~1MB |
| Real WASM | <1ms | 20-50ms | ~10MB |

Real WASM mode provides much more accurate analysis but is slower. Stub mode is suitable for development and testing.

## Uninstall

To remove Wasmtime:

```bash
sudo rm -rf /usr/local/wasmtime
# or
rm -rf ~/wasmtime
```

Then rebuild Ladybird without Wasmtime:
```bash
cd /home/rbsmith4/ladybird
rm -rf Build/release/CMakeCache.txt
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

Sentinel will automatically fall back to stub mode.

## Build from Source (Advanced)

If prebuilt binaries don't work for your platform, build Wasmtime from source:

```bash
# Clone repository
git clone https://github.com/bytecodealliance/wasmtime.git
cd wasmtime
git checkout v38.0.3

# Build
cargo build --release

# Install
sudo cp target/release/lib* /usr/local/wasmtime/lib/
sudo cp crates/c-api/include/* /usr/local/wasmtime/include/
```

Requires Rust toolchain (see https://rustup.rs/).

## Getting Help

If you encounter issues:

1. Check this guide's **Troubleshooting** section
2. Review CMake configuration:
   ```bash
   grep -A5 "Wasmtime" Build/release/CMakeOutput.log
   ```
3. Check system compatibility:
   ```bash
   file /usr/local/wasmtime/lib/libwasmtime.so
   ldd /usr/local/wasmtime/lib/libwasmtime.so
   ```
4. See Ladybird's build guide:
   - **Linux:** `Documentation/BuildInstructionsLinux.md`
   - **macOS:** `Documentation/BuildInstructionsMacOS.md`

## What's Next

After installing Wasmtime:

1. **Rebuild:** `cmake --preset Release && cmake --build Build/release --target sentinelservice`
2. **Test:** `./Build/release/bin/TestSandbox`
3. **Integration:** See `docs/PHASE_1C_INTEGRATION_GUIDE.md` for full testing guide
4. **Deploy:** Configure WASM module path in `Services/WebContent/PageClient.h`:
   ```cpp
   #define MALWARE_ANALYZER_WASM_PATH "/usr/share/ladybird/malware_analyzer.wasm"
   ```

## References

- **Wasmtime GitHub:** https://github.com/bytecodealliance/wasmtime
- **Wasmtime C API:** https://github.com/bytecodealliance/wasmtime/tree/main/crates/c-api
- **Ladybird Build Guide:** `Documentation/BuildInstructionsLadybird.md`
- **Sentinel Architecture:** `docs/SENTINEL_ARCHITECTURE.md`
