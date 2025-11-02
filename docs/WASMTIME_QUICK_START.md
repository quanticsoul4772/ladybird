# Wasmtime Quick Start Guide

**Quick reference for installing and testing Wasmtime integration in Sentinel**

## Installation (Ubuntu/Linux x86_64)

```bash
# Download latest Wasmtime C API
cd ~/Downloads
wget https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-linux-c-api.tar.xz

# Extract to standard location
sudo mkdir -p /usr/local/wasmtime
sudo tar -xf wasmtime-v38.0.3-x86_64-linux-c-api.tar.xz -C /usr/local/wasmtime --strip-components=1

# Verify installation
ls /usr/local/wasmtime/include/wasmtime.h  # Should exist
ls /usr/local/wasmtime/lib/libwasmtime.a   # Should exist
```

## Installation (macOS ARM64)

```bash
cd ~/Downloads
wget https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-macos-c-api.tar.xz
sudo mkdir -p /usr/local/wasmtime
sudo tar -xf wasmtime-v38.0.3-aarch64-macos-c-api.tar.xz -C /usr/local/wasmtime --strip-components=1
```

## Build Sentinel with Wasmtime

```bash
cd /home/rbsmith4/ladybird

# Reconfigure CMake (detects Wasmtime)
cmake --preset Release

# Build Sentinel
cmake --build Build/release --target sentinelservice

# Expected output:
# -- Found Wasmtime: /usr/local/wasmtime/lib/libwasmtime.a
# -- Wasmtime include: /usr/local/wasmtime/include
```

## Test Wasmtime Integration

```bash
# Run sandbox tests
cd Build/release
./bin/TestSandbox

# Expected output:
# ✓ WASM executor available
# ✓ Sandbox analysis completed
```

## Verify Detection

```bash
# Check compile definitions
grep ENABLE_WASMTIME Build/release/CMakeCache.txt

# Should show:
# sentinelservice_COMPILE_DEFINITIONS:STATIC=ENABLE_WASMTIME
```

## Troubleshooting

**Library not found?**
```bash
# Check installation
ls -lh /usr/local/wasmtime/lib/libwasmtime.a
ls -lh /usr/local/wasmtime/include/wasmtime.h

# If missing, re-run installation steps
```

**Stub mode used instead?**
```bash
# CMake will show:
# -- Wasmtime not found. WASM sandbox will use stub mode.

# Solution: Install Wasmtime and reconfigure CMake
```

## Full Documentation

See [WASMTIME_INTEGRATION_GUIDE.md](./WASMTIME_INTEGRATION_GUIDE.md) for:
- Platform-specific instructions
- API usage examples
- Version requirements
- Security considerations
- Performance benchmarks

## Version Info

- **Minimum**: Wasmtime v1.0.0
- **Tested**: Wasmtime v38.0.3
- **Latest**: Check https://github.com/bytecodealliance/wasmtime/releases
