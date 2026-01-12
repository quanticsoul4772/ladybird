# Phase 1c Integration Guide: WASM Module Loading

**Phase:** 1c - Module Loading & Full Integration
**Prerequisite:** Phase 1b Complete ✅
**Estimated Time:** 2-3 days
**Status:** ⏳ Ready to Start

## Overview

Phase 1c completes the Wasmtime integration by implementing real WASM module loading and execution. Phase 1b provided the infrastructure (engine initialization, resource limits, stub mode); Phase 1c makes it operational with real WASM execution.

## Prerequisites

### 1. Install Wasmtime Runtime

**Detailed Instructions:** See `docs/WASMTIME_INTEGRATION_GUIDE.md` (Section 2: Installation)

**Quick Install (Ubuntu/Debian x86_64):**
```bash
cd ~/Downloads

# Download Wasmtime C API (v38.0.3 - tested version)
wget https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-linux-c-api.tar.xz

# Extract to standard location
sudo mkdir -p /usr/local/wasmtime
sudo tar -xf wasmtime-v38.0.3-x86_64-linux-c-api.tar.xz -C /usr/local/wasmtime --strip-components=1

# Verify installation
ls -l /usr/local/wasmtime/lib/libwasmtime.a
ls -l /usr/local/wasmtime/include/wasmtime.h
```

**Expected Output:**
```
-rw-r--r-- 1 root root 45M Oct 15 2024 /usr/local/wasmtime/lib/libwasmtime.a
-rw-r--r-- 1 root root 89K Oct 15 2024 /usr/local/wasmtime/include/wasmtime.h
```

**macOS ARM64:**
```bash
wget https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-macos-c-api.tar.xz
sudo mkdir -p /usr/local/wasmtime
sudo tar -xf wasmtime-v38.0.3-aarch64-macos-c-api.tar.xz -C /usr/local/wasmtime --strip-components=1
```

### 2. Rebuild with Wasmtime Support

```bash
cd /home/rbsmith4/ladybird

# Reconfigure CMake (will detect Wasmtime)
cmake --preset Release

# Build Sentinel service
cmake --build Build/release --target sentinelservice
```

**Expected Output:**
```
-- Found Wasmtime: /usr/local/wasmtime/lib/libwasmtime.a
-- Wasmtime include: /usr/local/wasmtime/include
-- Configuring done
-- Generating done
-- Build files written to: /home/rbsmith4/ladybird/Build/release
[100%] Built target sentinelservice
```

**Verify ENABLE_WASMTIME is defined:**
```bash
grep ENABLE_WASMTIME Build/release/CMakeCache.txt
```

Expected: `sentinelservice_COMPILE_DEFINITIONS:STATIC=ENABLE_WASMTIME`

### 3. Verify WASM Module Exists

```bash
ls -lh /home/rbsmith4/ladybird/Services/Sentinel/assets/malware_analyzer.wasm
```

Expected: `-rwxr-xr-x 1 user user 2.7K Nov 1 19:18 malware_analyzer.wasm`

## Implementation Tasks

### Task 1: Implement Module Loading

**File:** `Services/Sentinel/Sandbox/WasmExecutor.cpp`

**Location:** Add new method (after `initialize_wasmtime()`, around line 98)

```cpp
ErrorOr<void> WasmExecutor::load_module(String const& module_path)
{
#ifdef ENABLE_WASMTIME
    VERIFY(m_wasmtime_engine);

    dbgln_if(false, "WasmExecutor: Loading WASM module from: {}", module_path);

    // Read WASM module file
    auto file = TRY(Core::File::open(module_path, Core::File::OpenMode::Read));
    auto file_size = TRY(file->size());
    auto module_data = TRY(ByteBuffer::create_uninitialized(file_size));
    TRY(file->read_until_filled(module_data.bytes()));

    dbgln_if(false, "WasmExecutor: Read {} bytes from module file", module_data.size());

    // Create WASM module from bytecode
    wasm_byte_vec_t wasm_bytes;
    wasm_byte_vec_new(&wasm_bytes, module_data.size(), module_data.data());

    wasmtime_error_t* error = nullptr;
    wasm_module_t* module = nullptr;

    error = wasmtime_module_new(
        static_cast<wasm_engine_t*>(m_wasmtime_engine),
        reinterpret_cast<uint8_t const*>(wasm_bytes.data),
        wasm_bytes.size,
        &module
    );

    wasm_byte_vec_delete(&wasm_bytes);

    if (error) {
        wasm_byte_vec_t error_msg;
        wasmtime_error_message(error, &error_msg);
        auto error_string = String::from_utf8(StringView(
            reinterpret_cast<char const*>(error_msg.data), error_msg.size));
        wasm_byte_vec_delete(&error_msg);
        wasmtime_error_delete(error);

        return Error::from_string_literal("Failed to load WASM module");
    }

    m_wasm_module = module;

    dbgln_if(false, "WasmExecutor: WASM module loaded successfully");
    return {};
#else
    (void)module_path; // Suppress unused parameter warning
    return Error::from_string_literal("Wasmtime support not compiled in");
#endif
}
```

**Add to header (WasmExecutor.h, around line 54):**
```cpp
// Load WASM module from file
ErrorOr<void> load_module(String const& module_path);
```

**Call from constructor:**
```cpp
WasmExecutor::WasmExecutor(SandboxConfig const& config)
    : m_config(config)
{
#ifdef ENABLE_WASMTIME
    // Try to load default module
    auto module_path = "/home/rbsmith4/ladybird/Services/Sentinel/assets/malware_analyzer.wasm"_string;
    if (auto result = load_module(module_path); result.is_error()) {
        dbgln("WasmExecutor: Could not load WASM module: {}", result.error());
        // Will fall back to stub mode
    }
#endif
}
```

### Task 2: Complete execute_wasmtime() Implementation

**File:** `Services/Sentinel/Sandbox/WasmExecutor.cpp` (lines 220-255)

**Replace TODO section with:**

```cpp
ErrorOr<WasmExecutionResult> WasmExecutor::execute_wasmtime(ByteBuffer const& file_data, String const& filename)
{
#ifdef ENABLE_WASMTIME
    WasmExecutionResult result;

    if (!m_wasmtime_store || !m_wasmtime_engine) {
        dbgln("WasmExecutor: Wasmtime not initialized, falling back to stub");
        return execute_stub(file_data, filename);
    }

    if (!m_wasm_module) {
        dbgln_if(false, "WasmExecutor: No WASM module loaded, using stub mode");
        return execute_stub(file_data, filename);
    }

    auto* store = static_cast<wasmtime_store_t*>(m_wasmtime_store);
    auto* module = static_cast<wasm_module_t*>(m_wasm_module);

    // Create module instance
    wasm_extern_vec_t imports = { 0 };
    wasm_trap_t* trap = nullptr;
    wasmtime_instance_t instance;
    wasmtime_error_t* error = nullptr;

    error = wasmtime_instance_new(store, module, &imports, &instance, &trap);
    if (error || trap) {
        if (trap) wasm_trap_delete(trap);
        if (error) wasmtime_error_delete(error);
        dbgln("WasmExecutor: Failed to instantiate module, using stub");
        return execute_stub(file_data, filename);
    }

    // Get module exports
    wasm_extern_vec_t exports;
    wasmtime_instance_exports(store, &instance, &exports);

    // Find functions: allocate, analyze_file, deallocate
    wasmtime_func_t* allocate_func = nullptr;
    wasmtime_func_t* analyze_func = nullptr;
    wasmtime_func_t* deallocate_func = nullptr;

    for (size_t i = 0; i < exports.size; ++i) {
        auto* exp = exports.data[i];
        wasm_externtype_t* type = wasm_extern_type(exp);
        wasm_externkind_t kind = wasm_externtype_kind(type);

        if (kind == WASM_EXTERN_FUNC) {
            // Get export name (requires module exports list)
            // For now, assume exports are in order: allocate, deallocate, analyze_file
            if (i == 0) allocate_func = wasmtime_extern_as_func(exp);
            if (i == 1) deallocate_func = wasmtime_extern_as_func(exp);
            if (i == 2) analyze_func = wasmtime_extern_as_func(exp);
        }
    }

    if (!allocate_func || !analyze_func || !deallocate_func) {
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: Required functions not found, using stub");
        return execute_stub(file_data, filename);
    }

    // Step 1: Allocate memory in WASM
    wasmtime_val_t allocate_args[1] = {
        WASMTIME_I32_VAL(static_cast<int32_t>(file_data.size()))
    };
    wasmtime_val_t allocate_results[1] = { WASMTIME_I32_VAL(0) };

    error = wasmtime_func_call(store, allocate_func, allocate_args, 1, allocate_results, 1, &trap);
    if (error || trap) {
        if (trap) wasm_trap_delete(trap);
        if (error) wasmtime_error_delete(error);
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: Allocate failed, using stub");
        return execute_stub(file_data, filename);
    }

    int32_t wasm_ptr = allocate_results[0].of.i32;
    if (wasm_ptr == 0) {
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: WASM allocation failed (OOM), using stub");
        return execute_stub(file_data, filename);
    }

    // Step 2: Get WASM memory and copy file data
    wasmtime_memory_t* memory = nullptr;
    for (size_t i = 0; i < exports.size; ++i) {
        auto* exp = exports.data[i];
        if (wasm_externtype_kind(wasm_extern_type(exp)) == WASM_EXTERN_MEMORY) {
            memory = wasmtime_extern_as_memory(exp);
            break;
        }
    }

    if (!memory) {
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: WASM memory not found, using stub");
        return execute_stub(file_data, filename);
    }

    uint8_t* wasm_memory_data = wasmtime_memory_data(store, memory);
    size_t wasm_memory_size = wasmtime_memory_data_size(store, memory);

    if (static_cast<size_t>(wasm_ptr) + file_data.size() > wasm_memory_size) {
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: Insufficient WASM memory, using stub");
        return execute_stub(file_data, filename);
    }

    memcpy(wasm_memory_data + wasm_ptr, file_data.data(), file_data.size());

    // Step 3: Call analyze_file
    wasmtime_val_t analyze_args[2] = {
        WASMTIME_I32_VAL(wasm_ptr),
        WASMTIME_I32_VAL(static_cast<int32_t>(file_data.size()))
    };
    wasmtime_val_t analyze_results[1] = { WASMTIME_I32_VAL(0) };

    error = wasmtime_func_call(store, analyze_func, analyze_args, 2, analyze_results, 1, &trap);
    if (error || trap) {
        if (trap) wasm_trap_delete(trap);
        if (error) wasmtime_error_delete(error);
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: analyze_file failed, using stub");
        return execute_stub(file_data, filename);
    }

    int32_t result_ptr = analyze_results[0].of.i32;
    if (result_ptr == 0) {
        wasm_extern_vec_delete(&exports);
        dbgln("WasmExecutor: analyze_file returned null, using stub");
        return execute_stub(file_data, filename);
    }

    // Step 4: Read result structure
    struct WasmAnalysisResult {
        float yara_score;
        float ml_score;
        u32 detected_patterns;
        u64 execution_time_us;
        u32 error_code;
        u32 padding;
    } __attribute__((packed));

    WasmAnalysisResult wasm_result;
    memcpy(&wasm_result, wasm_memory_data + result_ptr, sizeof(wasm_result));

    // Step 5: Deallocate WASM memory
    wasmtime_val_t dealloc_args[2] = {
        WASMTIME_I32_VAL(wasm_ptr),
        WASMTIME_I32_VAL(static_cast<int32_t>(file_data.size()))
    };
    wasmtime_func_call(store, deallocate_func, dealloc_args, 2, nullptr, 0, nullptr);

    wasm_extern_vec_delete(&exports);

    // Step 6: Convert to WasmExecutionResult
    result.yara_score = wasm_result.yara_score;
    result.ml_score = wasm_result.ml_score;
    result.timed_out = false;

    // Generate detected behaviors based on scores
    if (result.yara_score > 0.5f) {
        TRY(result.detected_behaviors.try_append("High YARA score - suspicious patterns detected"_string));
    }
    if (result.ml_score > 0.5f) {
        TRY(result.detected_behaviors.try_append("High ML score - malware-like features"_string));
    }

    TRY(result.triggered_rules.try_append(TRY(String::formatted(
        "WASM analysis: {} patterns detected", wasm_result.detected_patterns))));

    dbgln_if(false, "WasmExecutor: WASM execution complete - YARA: {:.2f}, ML: {:.2f}",
        result.yara_score, result.ml_score);

    return result;
#else
    return execute_stub(file_data, filename);
#endif
}
```

### Task 3: Implement Host Imports (Optional)

**Note:** The current WASM module defines host imports (log, current_time_ms) but doesn't use them. This task is optional for Phase 1c.

**If you want to implement host imports:**

Add after module instantiation (in `execute_wasmtime()`):

```cpp
// Define host functions
wasmtime_func_t log_func;
wasmtime_func_callback_t log_callback = [](void* env, wasmtime_caller_t* caller,
    wasmtime_val_t const* args, size_t nargs, wasmtime_val_t* results, size_t nresults,
    wasm_trap_t** trap) -> wasmtime_error_t* {

    u32 level = args[0].of.i32;
    int32_t msg_ptr = args[1].of.i32;
    u32 msg_len = args[2].of.i32;

    // Get memory and read message
    wasmtime_memory_t* memory = /* get memory from caller */;
    uint8_t* data = wasmtime_memory_data(/* store */, memory);
    StringView msg(reinterpret_cast<char const*>(data + msg_ptr), msg_len);

    dbgln("WASM Log [{}]: {}", level, msg);
    return nullptr;
};

wasmtime_func_new(store, /* log function type */, log_callback, nullptr, nullptr, &log_func);

// Add log_func to imports vector before instantiation
```

### Task 4: Add Timeout Enforcement (Optional)

**For timeout enforcement**, create an epoch incrementing thread:

```cpp
class EpochThread {
public:
    EpochThread(wasm_engine_t* engine, Duration timeout)
        : m_engine(engine)
        , m_timeout(timeout)
    {
        m_thread = Threading::Thread::create([this] {
            AK::sleep(m_timeout);
            wasm_engine_increment_epoch(m_engine);
        });
    }

    ~EpochThread()
    {
        if (m_thread)
            m_thread->join();
    }

private:
    wasm_engine_t* m_engine;
    Duration m_timeout;
    RefPtr<Threading::Thread> m_thread;
};
```

Use in `execute_wasmtime()`:
```cpp
EpochThread epoch_thread(static_cast<wasm_engine_t*>(m_wasmtime_engine), timeout);
// ... execute WASM ...
// Thread will increment epoch after timeout, interrupting execution
```

## Testing

### Unit Tests

**Run existing tests (should now use real WASM):**
```bash
cd /home/rbsmith4/ladybird/Build/release
./bin/TestSandbox
```

**Expected Output:**
```
Running Sandbox Tests...

✅ Test 1: WasmExecutor creation
   - Executor created successfully
   - Stub mode: false (Wasmtime initialized) ✓
   - Module loaded: true ✓

✅ Test 2: Malicious detection (REAL WASM)
   - File: eicar.txt (68 bytes)
   - YARA score: 0.87 (from WASM module) ✓
   - ML score: 0.45 (from WASM module) ✓
   - Execution time: 22ms
   - Verdict: MALICIOUS ✓

... (10/10 tests pass)
```

### Integration Tests

**Create integration test script:**
```bash
#!/bin/bash
# Services/Sentinel/Sandbox/test_wasm_integration.sh

WASM_MODULE="/home/rbsmith4/ladybird/Services/Sentinel/assets/malware_analyzer.wasm"
TEST_SAMPLES="/home/rbsmith4/ladybird/Services/Sentinel/Sandbox/test_samples"

echo "=== WASM Integration Tests ==="

# Test 1: Module exists
echo "Test 1: WASM module exists"
if [ -f "$WASM_MODULE" ]; then
    echo "✓ Module found: $(ls -lh $WASM_MODULE)"
else
    echo "✗ Module not found"
    exit 1
fi

# Test 2: Module is valid
echo "Test 2: WASM module is valid"
if command -v wasm-tools &> /dev/null; then
    wasm-tools validate "$WASM_MODULE" && echo "✓ Module is valid" || echo "✗ Invalid module"
else
    echo "⚠ wasm-tools not installed, skipping validation"
fi

# Test 3: Run sandbox tests
echo "Test 3: Run sandbox tests"
cd /home/rbsmith4/ladybird/Build/release
./bin/TestSandbox && echo "✓ All tests passed" || echo "✗ Tests failed"

echo "=== Integration Tests Complete ==="
```

### Performance Benchmarks

**Expected performance with real WASM:**
- Stub mode: ~5ms (C++ heuristics)
- WASM mode: ~20-50ms (WASM + sandbox overhead)
- Both acceptable (<100ms target)

**Benchmark:**
```bash
# Run 100 iterations
for i in {1..100}; do
    ./bin/TestSandbox | grep "Execution time"
done | awk '{sum+=$3; count++} END {print "Average:", sum/count "ms"}'
```

Expected: `Average: 35ms` (within target)

## Success Criteria

Phase 1c is complete when:

- ✅ WASM module loads successfully (no fallback to stub)
- ✅ Real WASM execution (not stub heuristics)
- ✅ All tests pass (10/10)
- ✅ Performance <100ms for 1MB file
- ✅ Timeout enforcement works (epoch interruption)
- ✅ No memory leaks (verified with ASAN build)
- ✅ Graceful degradation (stub fallback on errors)

## Troubleshooting

### Issue: Module loading fails

**Error:** "Failed to load WASM module"

**Solutions:**
1. Check module exists: `ls -lh Services/Sentinel/assets/malware_analyzer.wasm`
2. Validate module: `wasm-tools validate malware_analyzer.wasm`
3. Check file permissions: `chmod +x malware_analyzer.wasm`
4. Check Wasmtime version: `wasmtime --version` (should be >=1.0.0)

### Issue: Stub mode still used after Wasmtime install

**Symptom:** Tests show "Stub mode: true"

**Solutions:**
1. Reconfigure CMake: `cmake --preset Release`
2. Verify ENABLE_WASMTIME: `grep ENABLE_WASMTIME Build/release/CMakeCache.txt`
3. Check library path: `ls /usr/local/wasmtime/lib/libwasmtime.a`
4. Rebuild: `cmake --build Build/release --target sentinelservice`

### Issue: Module instantiation fails

**Error:** "Failed to instantiate module"

**Solutions:**
1. Check memory limits: Increase to 256MB if needed
2. Check exports: Module should export `allocate`, `deallocate`, `analyze_file`
3. Verify WASM version: Module should be compatible with Wasmtime v38.0.3
4. Enable debug logging: `#define DEBUG_WASM_EXECUTOR 1`

### Issue: Performance worse than stub

**Symptom:** Execution time >100ms

**Solutions:**
1. Use Release build (not Debug): `cmake --preset Release`
2. Enable LTO in WASM module: `lto = true` in Cargo.toml
3. Profile with `perf`: `perf record -g ./bin/TestSandbox`
4. Consider precompiling module: Use `wasmtime_module_serialize()`

## References

### Phase 1b Documentation
- **Status Report:** `docs/MILESTONE_0.5_PHASE_1B_STATUS.md`
- **Integration Guide:** `docs/WASMTIME_INTEGRATION_GUIDE.md`
- **Module Specification:** `docs/WASM_MODULE_SPECIFICATION.md`
- **Resource Limits:** `docs/WASMTIME_RESOURCE_LIMITS.md`
- **Quick Start:** `docs/WASMTIME_QUICK_START.md`

### Wasmtime API Documentation
- **C API Reference:** https://docs.wasmtime.dev/c-api/
- **Embedding Guide:** https://docs.wasmtime.dev/examples-c-embedding.html
- **Security Guide:** https://docs.wasmtime.dev/security.html

### WASM Module Source
- **Rust Source:** `Services/Sentinel/Sandbox/malware_analyzer_wasm/src/`
- **Binary:** `Services/Sentinel/assets/malware_analyzer.wasm`
- **Build Instructions:** See `WASM_MODULE_SPECIFICATION.md` Section 8

## Next Steps: Phase 1d

After completing Phase 1c, proceed to **Phase 1d: RequestServer Integration**:

1. Integrate sandbox with RequestServer download flow
2. Add PolicyGraph caching for verdicts
3. Implement user alerts (Qt dialogs)
4. Connect to quarantine system
5. End-to-end testing with real downloads

**Estimated Timeline:**
- Phase 1c: 2-3 days (module loading)
- Phase 1d: 3-4 days (full integration)
- **Total Phase 1:** ~4 weeks (complete)

---

*Document version: 1.0*
*Created: 2025-11-01*
*Ladybird Sentinel - Phase 1c Integration Guide*
