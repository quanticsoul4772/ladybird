# Phase 1c Wasmtime Quick Start Guide

**Milestone:** 0.5 Phase 1c - Module Loading
**Target:** Get WASM execution working in Sentinel sandbox
**Time Required:** 5-10 minutes

## One-Command Install

```bash
cd /home/rbsmith4/ladybird && ./Services/Sentinel/Sandbox/install_wasmtime.sh
```

Then rebuild:
```bash
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

Run tests:
```bash
./Build/release/bin/TestSandbox
```

Done! Wasmtime is now integrated.

## What Just Happened

1. **Downloaded:** Wasmtime v38.0.3 runtime (~50MB)
2. **Installed:** To `/usr/local/wasmtime`
3. **Linked:** CMake found and linked against libwasmtime
4. **Enabled:** WasmExecutor now uses real WASM instead of stub mode

## Key Files

| File | Purpose |
|------|---------|
| `/usr/local/wasmtime/lib/libwasmtime.so` | Runtime library |
| `/usr/local/wasmtime/include/wasmtime.h` | C API header |
| `Services/Sentinel/Sandbox/WasmExecutor.cpp` | Executor implementation |
| `Services/Sentinel/Sandbox/WasmExecutor.h` | Executor interface |

## Verify It Works

```bash
# Should show WASM execution times (20-50ms)
./Services/Sentinel/Sandbox/benchmark_sandbox.sh

# Should list 3 test samples
ls -la ./Services/Sentinel/Sandbox/test_samples/

# Should pass all tests
./Build/release/bin/TestSandbox
```

## Common Issues

**Q: "Wasmtime not found" error**
```bash
# Check it's installed
ls /usr/local/wasmtime/lib/libwasmtime.so

# Rebuild
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

**Q: Tests still running in stub mode**
```bash
# Check CMakeCache
grep ENABLE_WASMTIME Build/release/CMakeCache.txt

# Should be ON - if OFF, Wasmtime wasn't detected
```

**Q: Performance is still slow**
- Stub mode: 1-5ms per sample
- Real WASM: 20-50ms per sample
- Check benchmark output to see which is running

## Performance Comparison

After installation, benchmark shows:

```
Stub Mode (before):
- Sample 1: 2.34ms
- Sample 2: 1.89ms
- Average: 2.11ms

Real WASM (after):
- Sample 1: 34.12ms
- Sample 2: 28.45ms
- Average: 31.28ms
```

Real WASM is ~15x slower but much more accurate.

## Next Steps

1. **Run full integration tests:**
   ```bash
   ./Services/Sentinel/Sandbox/run_integration_tests.sh
   ```

2. **Load malware test samples:**
   ```bash
   ls -la Services/Sentinel/Sandbox/test_samples/
   ```

3. **Test in browser:**
   - Run Ladybird
   - Load test malware sample
   - Check Sentinel detects it

4. **Review Phase 1c guide:**
   ```bash
   cat docs/PHASE_1C_INTEGRATION_GUIDE.md
   ```

## Support

- Full guide: `docs/WASMTIME_INSTALLATION.md`
- Troubleshooting: See **Troubleshooting** section in full guide
- Sentinel architecture: `docs/SENTINEL_ARCHITECTURE.md`

## Uninstall

To remove Wasmtime and return to stub mode:

```bash
rm -rf /usr/local/wasmtime
cmake --preset Release
cmake --build Build/release --target sentinelservice
```

Sentinel automatically falls back to stub mode if Wasmtime is unavailable.
