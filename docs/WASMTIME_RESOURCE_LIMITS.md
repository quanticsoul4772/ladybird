# Wasmtime Resource Limits Configuration

## Overview

This document specifies comprehensive resource limits for the Wasmtime sandbox used in Ladybird's Sentinel malware detection system. These limits prevent denial-of-service attacks, resource exhaustion, and sandbox escape attempts while maintaining acceptable performance for legitimate YARA rule execution.

**Design Goals:**
- Target execution time: <100ms for 10MB files
- Prevent infinite loops, memory bombs, and stack overflow attacks
- Minimal performance overhead (<30%)
- Fail-safe: timeout/OOM results in quarantine, not crash
- Measurable and monitorable resource consumption

## 1. Limit Categories

### 1.1 Linear Memory Limits

Controls the maximum size of WebAssembly linear memory (heap allocation).

**Purpose:** Prevents memory exhaustion attacks where malicious YARA rules allocate unbounded memory.

**Wasmtime API:**
```cpp
// Maximum memory size in bytes
config.max_wasm_stack(size_t bytes);
// Note: Linear memory is separate from stack
```

**Limit Rationale:**
- YARA scanning requires buffering file data (up to 10MB)
- Additional memory for pattern matching state machines
- Overhead for Wasmtime runtime structures
- Must stay within sandbox process memory quota

**Attack Prevention:**
```wasm
;; Malicious WASM attempting memory bomb
(memory.grow 65536)  ;; Attempt to grow to 4GB
;; Blocked by linear memory limit
```

### 1.2 Fuel System (Instruction Budget)

Wasmtime's deterministic execution metering. Each WASM instruction consumes fuel units.

**Purpose:** Prevents infinite loops and excessive CPU consumption independent of wall-clock time.

**Wasmtime API:**
```cpp
// Enable fuel consumption
config.consume_fuel(true);

// During execution
store.add_fuel(u64 fuel);  // Set initial budget
u64 consumed = store.fuel_consumed();  // Check consumption
```

**Fuel Consumption Pattern:**
- Simple instructions (add, load): 1 fuel unit
- Complex instructions (div, call): 2-10 fuel units
- Memory operations: 1-5 fuel units
- Branches/loops: 1 fuel unit per iteration

**Limit Rationale:**
- Pattern matching complexity: O(n*m) where n=file size, m=pattern count
- 10MB file × 100 patterns ≈ 1B operations worst-case
- Balanced preset allows 500M instructions (sufficient for 99% cases)

**Attack Prevention:**
```wasm
;; Malicious infinite loop
(loop $infinite
  (br $infinite))  ;; Consumes 1 fuel per iteration
;; Blocked when fuel budget exhausted
```

### 1.3 Wall-Clock Timeout

Hard timeout using platform-specific mechanisms (SIGALRM on Unix, timer threads on Windows).

**Purpose:** Defense-in-depth against fuel metering bypass or time-based attacks.

**Implementation Strategy:**
```cpp
// Option 1: Signal-based (Unix only, most accurate)
#ifdef AK_OS_LINUX
    alarm(timeout_seconds);
    // SIGALRM handler terminates execution
#endif

// Option 2: Timer thread (cross-platform)
std::thread timer([&]() {
    std::this_thread::sleep_for(timeout_ms);
    store.interrupt_handle().interrupt();  // Force halt
});

// Option 3: Fuel-based approximation
// Check fuel consumption periodically during execution
```

**Limit Rationale:**
- Fuel metering adds ~10-30% overhead; timeout catches edge cases
- Must account for fuel metering overhead in timeout value
- Conservative: 50ms, Balanced: 100ms, Performance: 200ms
- Includes time for WASM compilation (<10ms for YARA modules)

**Attack Prevention:**
- Fuel metering bypass via JIT exploitation → Caught by wall-clock timeout
- Sleep-based timing attacks → Caught by timeout
- Slowloris-style gradual resource consumption → Caught by combined fuel+timeout

### 1.4 Stack Size Limits

Controls call stack depth for WebAssembly function calls.

**Purpose:** Prevents stack overflow attacks and excessive recursion.

**Wasmtime API:**
```cpp
config.max_wasm_stack(size_t bytes);
// Typical value: 512KB - 2MB
```

**Limit Rationale:**
- YARA rule matching typically uses shallow call stacks (<50 frames)
- Recursive pattern matching may use moderate depth (<200 frames)
- Each frame ~2-4KB (parameters, locals, return address)
- 512KB = ~128-256 frames (conservative)
- 1MB = ~256-512 frames (balanced)

**Attack Prevention:**
```wasm
;; Malicious deep recursion
(func $recursive (param i32)
  (call $recursive (i32.const 0)))  ;; Unbounded recursion
;; Blocked by stack limit
```

### 1.5 Table Size Limits

Controls maximum size of WebAssembly tables (function references, externrefs).

**Purpose:** Prevents table exhaustion attacks and excessive indirect call overhead.

**Wasmtime API:**
```cpp
// Set in module instantiation limits
// Default Wasmtime limit: 10,000 elements
```

**Limit Rationale:**
- YARA compiled modules use tables for pattern dispatch
- Typical module: <100 table entries
- Conservative limit: 1,000 entries
- Balanced limit: 5,000 entries

**Attack Prevention:**
```wasm
;; Malicious table growth
(table.grow $t (i32.const 1000000) (ref.null func))
;; Blocked by table element limit
```

### 1.6 Instance/Module Limits

Additional Wasmtime configuration limits.

**Wasmtime API:**
```cpp
config.max_instances(u32 count);        // Max instances per store
config.max_memories(u32 count);         // Max memories per instance
config.max_tables(u32 count);           // Max tables per instance
config.wasm_threads(bool enabled);      // Disable threading
config.wasm_simd(bool enabled);         // SIMD support
```

**Recommended Settings:**
```cpp
config.max_instances(1);      // Single YARA module per scan
config.max_memories(1);       // Single linear memory
config.max_tables(10);        // Multiple pattern dispatch tables
config.wasm_threads(false);   // Disable threading (attack surface)
config.wasm_simd(true);       // Enable SIMD (performance)
```

## 2. Configuration Presets

### 2.1 Conservative (High Security)

**Use Case:** Maximum security for untrusted/unknown YARA rules, production default for sensitive environments.

```cpp
struct ConservativePreset {
    // Memory
    size_t linear_memory_max = 64 * 1024 * 1024;  // 64MB
    size_t stack_size = 512 * 1024;                // 512KB

    // CPU
    u64 fuel_budget = 100'000'000;                 // 100M instructions
    std::chrono::milliseconds timeout{50};         // 50ms wall-clock

    // Tables
    u32 max_table_elements = 1'000;

    // Instances
    u32 max_instances = 1;
    u32 max_memories = 1;
    u32 max_tables = 5;

    // Features
    bool enable_threads = false;
    bool enable_simd = true;
    bool enable_bulk_memory = true;
};
```

**Expected Performance:**
- Files ≤5MB: <30ms
- Files 5-10MB: <50ms
- May timeout on extremely complex patterns (>500 rules with regex)

**Security Properties:**
- Prevents memory exhaustion (64MB cap)
- Prevents CPU hogging (100M instruction cap ≈ 50-100ms real time)
- Prevents stack overflow (512KB = ~128 frames)
- Minimal attack surface (no threading)

### 2.2 Balanced (Default)

**Use Case:** Default configuration balancing security and performance for typical YARA rulesets.

```cpp
struct BalancedPreset {
    // Memory
    size_t linear_memory_max = 128 * 1024 * 1024; // 128MB
    size_t stack_size = 1 * 1024 * 1024;           // 1MB

    // CPU
    u64 fuel_budget = 500'000'000;                 // 500M instructions
    std::chrono::milliseconds timeout{100};        // 100ms wall-clock

    // Tables
    u32 max_table_elements = 5'000;

    // Instances
    u32 max_instances = 1;
    u32 max_memories = 1;
    u32 max_tables = 10;

    // Features
    bool enable_threads = false;
    bool enable_simd = true;
    bool enable_bulk_memory = true;
};
```

**Expected Performance:**
- Files ≤10MB: <100ms (99th percentile)
- Median: <50ms
- Handles complex rulesets (1000+ rules)

**Security Properties:**
- Adequate memory protection (128MB cap)
- Robust CPU protection (500M instruction cap ≈ 100-200ms real time)
- Comfortable stack depth (1MB = ~256 frames)
- Disabled threading attack surface

### 2.3 Performance (Fast)

**Use Case:** Trusted YARA rules, performance-critical environments, low-latency requirements.

```cpp
struct PerformancePreset {
    // Memory
    size_t linear_memory_max = 256 * 1024 * 1024; // 256MB
    size_t stack_size = 2 * 1024 * 1024;           // 2MB

    // CPU
    u64 fuel_budget = 1'000'000'000;               // 1B instructions
    std::chrono::milliseconds timeout{200};        // 200ms wall-clock

    // Tables
    u32 max_table_elements = 10'000;

    // Instances
    u32 max_instances = 1;
    u32 max_memories = 1;
    u32 max_tables = 20;

    // Features
    bool enable_threads = false;  // Still disabled for security
    bool enable_simd = true;
    bool enable_bulk_memory = true;
};
```

**Expected Performance:**
- Files ≤10MB: <50ms (99th percentile)
- Median: <20ms
- Handles very complex rulesets (2000+ rules with heavy regex)

**Security Properties:**
- Generous memory headroom (256MB cap)
- Extended CPU budget (1B instruction cap ≈ 200-400ms real time)
- Deep stack support (2MB = ~512 frames)
- Threading still disabled

**Warning:** Only use for trusted rulesets. Higher limits reduce security margin.

### 2.4 Minimal (Testing/Development)

**Use Case:** Unit testing, rapid iteration, debugging.

```cpp
struct MinimalPreset {
    // Memory
    size_t linear_memory_max = 32 * 1024 * 1024;  // 32MB
    size_t stack_size = 256 * 1024;                // 256KB

    // CPU
    u64 fuel_budget = 50'000'000;                  // 50M instructions
    std::chrono::milliseconds timeout{25};         // 25ms wall-clock

    // Tables
    u32 max_table_elements = 500;

    // Instances
    u32 max_instances = 1;
    u32 max_memories = 1;
    u32 max_tables = 5;

    // Features
    bool enable_threads = false;
    bool enable_simd = false;  // Disable for determinism
    bool enable_bulk_memory = true;
};
```

**Use Case:** Validates that tests complete within tight bounds, exposes resource issues early.

## 3. Limit Enforcement Implementation

### 3.1 Configuration API Usage

```cpp
// WasmExecutor.cpp implementation

ErrorOr<NonnullOwnPtr<WasmExecutor>> WasmExecutor::create(SandboxConfig const& config)
{
    auto executor = TRY(adopt_nonnull_own_or_enomem(new (nothrow) WasmExecutor()));

    // Create Wasmtime engine and store
    auto engine = wasmtime::Engine(get_wasmtime_config(config));
    auto store = wasmtime::Store(engine);

    // Configure fuel (instruction budget)
    if (config.fuel_budget > 0) {
        store.add_fuel(config.fuel_budget);
    }

    // Set epoch interruption for timeout
    engine.increment_epoch();  // Start epoch timer
    store.set_epoch_deadline(1);  // Interrupt after 1 epoch

    executor->m_engine = move(engine);
    executor->m_store = move(store);

    return executor;
}

wasmtime::Config WasmExecutor::get_wasmtime_config(SandboxConfig const& config)
{
    wasmtime::Config wasm_config;

    // Enable fuel consumption for CPU limits
    wasm_config.consume_fuel(config.fuel_budget > 0);

    // Memory limits
    wasm_config.max_wasm_stack(config.stack_size);
    // Note: Linear memory max is set via module limits, not Config

    // Disable dangerous features
    wasm_config.wasm_threads(config.enable_threads);
    wasm_config.wasm_reference_types(true);   // Safe, needed for tables
    wasm_config.wasm_simd(config.enable_simd);
    wasm_config.wasm_bulk_memory(config.enable_bulk_memory);
    wasm_config.wasm_multi_value(true);        // Safe
    wasm_config.wasm_multi_memory(false);      // Disable multiple memories

    // Disable WASI (file system access)
    // YARA modules should not need WASI

    // Enable epoch-based interruption for timeout
    wasm_config.epoch_interruption(true);

    // Compilation settings
    wasm_config.strategy(wasmtime::Strategy::Cranelift);  // Secure JIT
    wasm_config.cranelift_opt_level(wasmtime::OptLevel::Speed);

    return wasm_config;
}
```

### 3.2 Fuel Consumption Pattern

```cpp
ErrorOr<YaraResult> WasmExecutor::scan(ReadonlyBytes file_data)
{
    // Check fuel before execution
    u64 initial_fuel = m_store.fuel_consumed().value_or(0);

    // Start timeout timer (epoch-based)
    auto timeout_thread = std::thread([this, timeout_ms = m_config.timeout.count()]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
        m_engine.increment_epoch();  // Trigger interrupt
    });

    // Execute YARA scan
    auto result = m_instance.call("yara_scan", file_data.data(), file_data.size());

    // Clean up timeout thread
    timeout_thread.join();

    // Check fuel consumption
    u64 final_fuel = m_store.fuel_consumed().value_or(0);
    u64 consumed = final_fuel - initial_fuel;

    dbgln("WASM fuel consumed: {} / {} ({:.1f}%)",
          consumed, m_config.fuel_budget,
          100.0 * consumed / m_config.fuel_budget);

    if (!result) {
        // Check error type
        if (result.error().is_fuel_exhausted()) {
            return Error::from_string_literal("YARA scan exceeded fuel budget (CPU limit)");
        }
        if (result.error().is_epoch_interrupted()) {
            return Error::from_string_literal("YARA scan timed out (wall-clock limit)");
        }
        if (result.error().is_memory_exhausted()) {
            return Error::from_string_literal("YARA scan exceeded memory limit");
        }
        return Error::from_string_literal("YARA scan failed");
    }

    return parse_yara_result(result.value());
}
```

### 3.3 Linear Memory Limit Enforcement

```cpp
// Enforce linear memory limit during module instantiation

ErrorOr<void> WasmExecutor::load_module(ReadonlyBytes wasm_bytes)
{
    auto module = wasmtime::Module::compile(m_engine, wasm_bytes);
    if (!module) {
        return Error::from_string_literal("Failed to compile WASM module");
    }

    // Validate memory limits before instantiation
    for (auto const& import : module.imports()) {
        if (import.type().is_memory()) {
            auto memory_type = import.type().memory();
            u64 min_bytes = memory_type.minimum() * 65536;  // Pages to bytes

            if (min_bytes > m_config.linear_memory_max) {
                return Error::from_string_literal(
                    "WASM module requires more memory than allowed");
            }
        }
    }

    // Create memory with enforced maximum
    wasmtime::MemoryType memory_type(
        m_config.linear_memory_max / 65536,  // max pages
        0                                     // min pages (module decides)
    );

    auto memory = wasmtime::Memory::new_(m_store, memory_type);

    // Instantiate with memory limit
    auto instance = wasmtime::Instance::new_(m_store, module, {memory});
    if (!instance) {
        return Error::from_string_literal("Failed to instantiate WASM module");
    }

    m_instance = instance.value();
    return {};
}
```

### 3.4 Timeout Implementation Strategies

#### Strategy 1: Epoch-Based Interruption (Recommended)

```cpp
// Pros: Cross-platform, precise, integrated with Wasmtime
// Cons: Requires Wasmtime 0.38+

// Setup (in create())
config.epoch_interruption(true);
store.set_epoch_deadline(1);

// Trigger timeout
std::thread timeout_thread([this, ms = config.timeout.count()]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    m_engine.increment_epoch();  // Interrupt store
});

// Cleanup
timeout_thread.join();
```

#### Strategy 2: Signal-Based (Unix Only)

```cpp
// Pros: Most accurate, minimal overhead
// Cons: Unix-only, signal handler complexity

#ifdef AK_OS_UNIX
static wasmtime::Store* g_current_store = nullptr;

static void timeout_handler(int signum) {
    if (g_current_store) {
        g_current_store->interrupt_handle().interrupt();
    }
}

// Setup
signal(SIGALRM, timeout_handler);
g_current_store = &m_store;
alarm(config.timeout.count() / 1000);  // seconds

// Cleanup
alarm(0);
g_current_store = nullptr;
```

#### Strategy 3: Fuel-Based Approximation

```cpp
// Pros: No threading, no signals, portable
// Cons: Indirect timing, depends on calibration

// Calibrate: instructions per millisecond
u64 fuel_per_ms = calibrate_fuel_rate();  // e.g., 1M instructions/ms

// Set fuel budget based on timeout
u64 fuel_for_timeout = config.timeout.count() * fuel_per_ms;
u64 fuel_budget = min(config.fuel_budget, fuel_for_timeout);

store.add_fuel(fuel_budget);
```

**Recommended:** Use epoch-based interruption (Strategy 1) as primary with fuel as secondary defense.

### 3.5 Out-of-Memory Handling

```cpp
ErrorOr<YaraResult> WasmExecutor::scan(ReadonlyBytes file_data)
{
    try {
        auto result = m_instance.call("yara_scan", file_data.data(), file_data.size());
        // ... handle result
    } catch (wasmtime::Error const& e) {
        if (e.is_memory_exhausted()) {
            dbgln("YARA scan OOM: exceeded {}MB limit",
                  m_config.linear_memory_max / 1024 / 1024);

            // Log telemetry
            m_telemetry.oom_count++;
            m_telemetry.last_oom_file_size = file_data.size();

            // Fail-safe: quarantine file
            return Error::from_string_literal(
                "YARA scan exceeded memory limit - file quarantined for safety");
        }
        throw;
    }
}
```

### 3.6 Stack Overflow Handling

```cpp
// Wasmtime automatically checks stack on each function call
// No additional code needed, but handle the error:

if (result.error().is_stack_overflow()) {
    dbgln("YARA scan stack overflow: exceeded {}KB limit",
          m_config.stack_size / 1024);

    // This indicates extremely deep recursion in YARA rules
    // Likely malicious or badly written rules
    m_telemetry.stack_overflow_count++;

    return Error::from_string_literal(
        "YARA scan stack overflow - excessively recursive rules");
}
```

## 4. Attack Scenarios and Mitigations

### 4.1 Infinite Loop Attack

**Attack Vector:**
```wasm
;; Malicious YARA rule with infinite loop
(func $scan (param $data i32) (param $size i32) (result i32)
  (loop $forever
    (br $forever))  ;; Infinite loop
  (i32.const 0))
```

**Detection Mechanisms:**
1. **Fuel Exhaustion (Primary):** Loop consumes 1 fuel per iteration. After fuel budget exhausted, execution halts.
2. **Wall-Clock Timeout (Secondary):** Even if fuel metering is bypassed, epoch interrupt triggers after timeout.

**Limit Configuration:**
```
Conservative: 100M fuel (~100M iterations) + 50ms timeout
Balanced:     500M fuel (~500M iterations) + 100ms timeout
Performance:  1B fuel (~1B iterations) + 200ms timeout
```

**Telemetry:**
- Increment `infinite_loop_detected` counter
- Log fuel consumption rate (fuel/ms)
- Alert if fuel consumption is linear (indicates tight loop)

### 4.2 Memory Bomb Attack

**Attack Vector:**
```wasm
;; Attempt to allocate unbounded memory
(func $scan (param $data i32) (param $size i32) (result i32)
  (memory.grow (i32.const 65535))  ;; Try to allocate 4GB
  (i32.const 0))
```

**Detection Mechanisms:**
1. **Linear Memory Maximum (Primary):** `memory.grow` fails when approaching limit.
2. **Module Validation (Secondary):** Reject modules declaring excessive initial memory.

**Limit Configuration:**
```
Conservative: 64MB max linear memory
Balanced:     128MB max linear memory
Performance:  256MB max linear memory
```

**Behavior:**
- `memory.grow` returns -1 (failure) when limit reached
- YARA module must handle allocation failure gracefully
- If module doesn't check return value → execution continues with old size
- If module crashes on allocation failure → caught as WASM trap

**Telemetry:**
- Track memory high-water mark
- Alert if memory usage >90% of limit
- Log allocation patterns (growth frequency, size)

### 4.3 Stack Overflow Attack

**Attack Vector:**
```wasm
;; Unbounded recursion
(func $recursive (param $depth i32) (result i32)
  (call $recursive (i32.add (local.get $depth) (i32.const 1))))

(func $scan (param $data i32) (param $size i32) (result i32)
  (call $recursive (i32.const 0)))
```

**Detection Mechanisms:**
1. **Stack Size Limit (Primary):** Wasmtime checks stack on each function entry. Traps on overflow.
2. **Fuel Budget (Secondary):** Deep recursion consumes fuel via call instructions.

**Limit Configuration:**
```
Conservative: 512KB stack (~128 frames)
Balanced:     1MB stack (~256 frames)
Performance:  2MB stack (~512 frames)
```

**Behavior:**
- Stack overflow raises WASM trap
- Execution halts immediately
- Error propagates to caller

**Telemetry:**
- Increment `stack_overflow_count`
- Log maximum call depth achieved
- Alert on unusual recursion patterns

### 4.4 Table Exhaustion Attack

**Attack Vector:**
```wasm
;; Attempt to grow table to consume memory
(table $dispatch 10 funcref)
(func $scan (param $data i32) (param $size i32) (result i32)
  (table.grow $dispatch (ref.null func) (i32.const 1000000))
  (i32.const 0))
```

**Detection Mechanisms:**
1. **Table Element Limit (Primary):** `table.grow` fails when exceeding element limit.
2. **Module Validation (Secondary):** Reject modules declaring excessive initial table size.

**Limit Configuration:**
```
Conservative: 1,000 table elements
Balanced:     5,000 table elements
Performance:  10,000 table elements
```

**Behavior:**
- `table.grow` returns -1 (failure) when limit reached
- Minimal memory impact (each entry ~8 bytes)

**Telemetry:**
- Track table sizes across all tables
- Alert if table growth is excessive

### 4.5 Slowloris-Style Gradual Consumption

**Attack Vector:**
```wasm
;; Gradually consume resources just under thresholds
(func $scan (param $data i32) (param $size i32) (result i32)
  (local $i i32)
  ;; Allocate memory slowly
  (loop $slow
    (memory.grow (i32.const 1))  ;; 64KB per iteration
    (local.set $i (i32.add (local.get $i) (i32.const 1)))
    (br_if $slow (i32.lt_u (local.get $i) (i32.const 1000))))
  ;; Consume CPU slowly
  (loop $burn
    (local.set $i (i32.sub (local.get $i) (i32.const 1)))
    (br_if $burn (i32.gt_u (local.get $i) (i32.const 0))))
  (i32.const 0))
```

**Detection Mechanisms:**
1. **Combined Fuel + Timeout (Primary):** Even gradual consumption eventually exceeds both limits.
2. **Memory Limit (Secondary):** Gradual allocation still hits memory ceiling.

**Limit Configuration:**
- All presets enforce both fuel and timeout
- Timeout provides absolute upper bound regardless of consumption rate

**Behavior:**
- Gradual consumption is acceptable if within limits
- Attack fails when any limit is exceeded

**Telemetry:**
- Track resource consumption rate over time
- Alert on sustained high resource usage without completion

### 4.6 Fork Bomb (Not Applicable)

**Attack Vector:**
```wasm
;; Attempt to spawn processes/threads
;; (not possible in WASM without WASI or threads)
```

**Mitigation:**
- WASM threads proposal disabled (`config.wasm_threads(false)`)
- WASI disabled (no process spawn capability)
- No host functions provided for spawning

**Telemetry:** Not applicable (attack surface eliminated).

### 4.7 Timing Side-Channel

**Attack Vector:**
```wasm
;; Attempt to leak information via execution time
(func $scan (param $data i32) (param $size i32) (result i32)
  ;; Execute different code paths based on secrets
  ;; Attacker measures time differences
  (i32.const 0))
```

**Mitigation:**
- Not a primary concern for YARA scanning (no secrets in WASM)
- Fuel metering adds deterministic overhead (reduces timing precision)
- Timeout prevents prolonged timing measurements

**Note:** This is lower priority for malware scanning use case.

### 4.8 JIT Spray / Code Injection

**Attack Vector:**
- Exploit Wasmtime JIT compiler to inject malicious code
- Overflow JIT code cache

**Mitigation:**
1. **Wasmtime Security:** Rely on Wasmtime's JIT security (Cranelift backend)
2. **W^X Protection:** JIT code pages are non-writable at runtime
3. **Regular Updates:** Keep Wasmtime updated for security patches
4. **Defense-in-depth:** Even if JIT is compromised:
   - Fuel limit prevents code execution runaway
   - Timeout provides failsafe
   - Sandboxed process isolation (future: seccomp-bpf)

**Telemetry:**
- Log Wasmtime version on startup
- Monitor for unexpected crashes (potential JIT bugs)

## 5. Performance Impact Analysis

### 5.1 Fuel Metering Overhead

**Measurement Methodology:**
```cpp
// Benchmark with and without fuel metering
auto start = std::chrono::high_resolution_clock::now();

// Without fuel
config.consume_fuel(false);
auto result_no_fuel = scan_file(test_file);

// With fuel
config.consume_fuel(true);
store.add_fuel(500'000'000);
auto result_with_fuel = scan_file(test_file);

auto end = std::chrono::high_resolution_clock::now();
auto overhead_pct = 100.0 * (time_with_fuel - time_no_fuel) / time_no_fuel;
```

**Expected Overhead:**
- **Simple YARA rules:** 5-10% overhead
  - Few branches, mostly linear scanning
  - Fuel checking cost amortized
- **Complex regex patterns:** 20-30% overhead
  - Many branches and function calls
  - Fuel checks on every branch
- **Worst case:** 30-40% overhead
  - Deep recursion, tight loops
  - Frequent fuel checks dominate runtime

**Mitigation:**
- Use faster preset for trusted rules (higher fuel budget)
- Disable fuel for trusted rulesets (if acceptable risk)
- SIMD-enabled patterns reduce overhead (vectorized operations)

**Telemetry:**
```cpp
struct FuelMetrics {
    u64 total_fuel_consumed;
    u64 fuel_per_byte;           // fuel / file_size
    f64 instructions_per_ms;     // fuel / wall_time_ms
    f64 overhead_estimate_pct;   // (actual_time - no_fuel_baseline) / no_fuel_baseline * 100
};
```

### 5.2 Memory Allocation Cost

**Impact:**
- Linear memory allocation during module instantiation: <5ms
- Memory page zeroing: ~1ms per 64KB page (Linux)
- 128MB allocation: ~2000 pages = ~2000ms (but lazy allocation amortizes this)

**Optimization:**
- Linux lazy allocation: pages allocated on first write (virtually free)
- Windows: can use MEM_RESERVE + MEM_COMMIT for similar behavior
- Pre-allocate memory pool if scanning multiple files

**Telemetry:**
```cpp
struct MemoryMetrics {
    size_t peak_usage;               // High water mark
    size_t allocated_size;           // Committed pages
    size_t reserved_size;            // Virtual reservation
    u32 growth_operations;           // Number of memory.grow calls
    f64 allocation_time_ms;          // Time to allocate
};
```

### 5.3 Timeout Checking Frequency

**Epoch-Based Timeout:**
- Epoch check on backward branches (loops, recursion)
- Check cost: <10 CPU cycles (atomically read counter)
- Frequency: 1 check per loop iteration
- Overhead: <1% for most workloads

**Timer Thread Overhead:**
- Thread creation: ~1ms
- Thread join: ~1ms
- Total: ~2ms per scan (negligible)

**Signal-Based Timeout (Unix):**
- `alarm()` syscall: <1µs
- Signal delivery: <10µs
- Overhead: negligible

**Recommendation:** Epoch-based for cross-platform, signal-based for lowest overhead on Unix.

### 5.4 Overall Performance Budget

**Target: <100ms total execution time**

**Breakdown (Balanced preset, 10MB file, 500 YARA rules):**
```
Component                   Time (ms)  % of Total
--------------------------------------------------
WASM compilation            5-10       5-10%
Memory allocation           1-2        1-2%
File buffer copy            1-2        1-2%
Pattern matching (core)     60-80      60-80%
Fuel metering overhead      10-15      10-15%
Timeout/epoch checking      <1         <1%
Result marshaling           1-2        1-2%
--------------------------------------------------
Total                       78-112     100%
```

**Performance Targets:**
- **P50 (median):** <50ms
- **P95:** <100ms
- **P99:** <150ms (may exceed for complex rulesets)

**Worst-Case Scenarios:**
- Very complex regex (catastrophic backtracking): timeout at 100ms
- Large file (100MB) with conservative preset: timeout at 50ms (expected)
- Malicious infinite loop: fuel exhaustion + timeout at 100ms

### 5.5 Optimization Opportunities

**SIMD Enablement:**
```cpp
config.wasm_simd(true);
// Pattern matching can use WASM SIMD instructions
// Expected speedup: 2-4x for vectorizable patterns
```

**AOT Compilation (Future):**
```cpp
// Pre-compile trusted YARA modules to native code
auto module = wasmtime::Module::deserialize(engine, aot_bytes);
// Eliminates 5-10ms JIT compilation time per scan
```

**Memory Pool Reuse:**
```cpp
// Reuse Store across multiple scans
// Avoid repeated memory allocation
// Save ~2ms per scan
```

**Parallelization (Future):**
```cpp
// Scan multiple files in parallel with separate Store instances
// Linear scaling up to CPU core count
```

## 6. Monitoring and Telemetry

### 6.1 Resource Consumption Metrics

```cpp
struct SandboxTelemetry {
    // Execution counts
    u64 scans_completed = 0;
    u64 scans_failed = 0;

    // Limit violations
    u64 fuel_exhausted_count = 0;
    u64 timeout_count = 0;
    u64 oom_count = 0;
    u64 stack_overflow_count = 0;

    // Resource usage statistics
    struct {
        u64 min, max, sum, count;
        f64 mean() const { return count > 0 ? f64(sum) / count : 0.0; }
    } fuel_consumed, memory_peak, execution_time_ms;

    // Performance metrics
    f64 avg_fuel_per_byte = 0.0;
    f64 avg_ms_per_mb = 0.0;

    // Error tracking
    u64 compilation_errors = 0;
    u64 instantiation_errors = 0;
    u64 runtime_errors = 0;
};
```

### 6.2 Logging and Alerts

```cpp
// Log on limit violation
if (result.error().is_fuel_exhausted()) {
    dbgln("SECURITY: Fuel exhausted - potential infinite loop attack");
    dbgln("  File size: {} bytes", file_size);
    dbgln("  Fuel consumed: {}", fuel_consumed);
    dbgln("  Fuel budget: {}", config.fuel_budget);
    dbgln("  Fuel per byte: {:.1f}", fuel_per_byte);

    m_telemetry.fuel_exhausted_count++;

    // Alert if excessive
    if (fuel_per_byte > 10000) {  // Threshold for "abnormal"
        send_security_alert(SecurityAlertType::SuspiciousResourceUsage);
    }
}

// Log performance outliers
if (execution_time_ms > config.timeout.count() * 0.9) {
    dbgln("WARNING: YARA scan near timeout threshold");
    dbgln("  Time: {}ms / {}ms", execution_time_ms, config.timeout.count());
    dbgln("  Consider increasing timeout or optimizing rules");
}

// Periodic telemetry dump
if (m_telemetry.scans_completed % 1000 == 0) {
    dbgln("Sandbox telemetry (last 1000 scans):");
    dbgln("  Success rate: {:.1f}%",
          100.0 * m_telemetry.scans_completed /
          (m_telemetry.scans_completed + m_telemetry.scans_failed));
    dbgln("  Avg fuel: {:.0f}", m_telemetry.fuel_consumed.mean());
    dbgln("  Avg time: {:.1f}ms", m_telemetry.execution_time_ms.mean());
    dbgln("  Fuel exhausted: {}", m_telemetry.fuel_exhausted_count);
    dbgln("  Timeouts: {}", m_telemetry.timeout_count);
    dbgln("  OOM: {}", m_telemetry.oom_count);
}
```

### 6.3 Profiling Integration

```cpp
// Detailed profiling for performance analysis
#ifdef ENABLE_WASM_PROFILING
struct ProfileData {
    std::chrono::microseconds compilation_time;
    std::chrono::microseconds instantiation_time;
    std::chrono::microseconds execution_time;

    u64 fuel_consumed;
    size_t memory_peak;

    std::vector<std::pair<String, u64>> function_fuel_usage;  // Per-function
};

ProfileData profile_scan(ReadonlyBytes file_data) {
    ProfileData profile;

    auto t0 = std::chrono::high_resolution_clock::now();
    auto module = compile_module();
    auto t1 = std::chrono::high_resolution_clock::now();
    profile.compilation_time = duration_cast<microseconds>(t1 - t0);

    auto instance = instantiate_module(module);
    auto t2 = std::chrono::high_resolution_clock::now();
    profile.instantiation_time = duration_cast<microseconds>(t2 - t1);

    u64 fuel_before = m_store.fuel_consumed().value_or(0);
    auto result = execute_scan(instance, file_data);
    auto t3 = std::chrono::high_resolution_clock::now();
    u64 fuel_after = m_store.fuel_consumed().value_or(0);

    profile.execution_time = duration_cast<microseconds>(t3 - t2);
    profile.fuel_consumed = fuel_after - fuel_before;
    profile.memory_peak = get_memory_peak();

    return profile;
}
#endif
```

### 6.4 Adaptive Limit Tuning (Future)

```cpp
// Automatically adjust limits based on observed behavior
class AdaptiveLimitTuner {
public:
    void record_scan(ScanMetrics const& metrics) {
        m_history.push_back(metrics);
        if (m_history.size() > 1000) {
            m_history.erase(m_history.begin());
        }

        // Recalculate recommended limits every 100 scans
        if (m_history.size() % 100 == 0) {
            update_recommendations();
        }
    }

    SandboxConfig recommend_config() const {
        // Recommend limits based on P95 observed usage + 20% margin
        SandboxConfig config;
        config.fuel_budget = m_p95_fuel * 1.2;
        config.timeout = std::chrono::milliseconds(u64(m_p95_time_ms * 1.2));
        config.linear_memory_max = m_p95_memory * 1.2;
        return config;
    }

private:
    void update_recommendations() {
        // Calculate percentiles
        m_p95_fuel = percentile(m_history, 0.95, [](auto& m) { return m.fuel_consumed; });
        m_p95_time_ms = percentile(m_history, 0.95, [](auto& m) { return m.time_ms; });
        m_p95_memory = percentile(m_history, 0.95, [](auto& m) { return m.memory_peak; });
    }

    std::vector<ScanMetrics> m_history;
    u64 m_p95_fuel = 0;
    f64 m_p95_time_ms = 0;
    size_t m_p95_memory = 0;
};
```

## 7. Implementation Checklist

### Phase 1: Basic Limits
- [ ] Implement `SandboxConfig` struct with all limit fields
- [ ] Configure Wasmtime with fuel, stack, memory limits
- [ ] Basic timeout using epoch interruption
- [ ] Error handling for fuel exhaustion, timeout, OOM
- [ ] Unit tests for each limit type

### Phase 2: Telemetry
- [ ] Implement `SandboxTelemetry` struct
- [ ] Record fuel consumption per scan
- [ ] Record memory high-water mark
- [ ] Record execution time
- [ ] Log limit violations

### Phase 3: Presets
- [ ] Define Conservative, Balanced, Performance presets
- [ ] Configuration file support (TOML/JSON)
- [ ] Command-line flags to select preset
- [ ] Documentation for each preset use case

### Phase 4: Advanced Features
- [ ] Signal-based timeout (Unix)
- [ ] Profiling mode with detailed breakdowns
- [ ] Adaptive limit tuning
- [ ] Security alert integration

### Phase 5: Testing
- [ ] Malicious WASM test suite (infinite loop, memory bomb, etc.)
- [ ] Performance benchmarks for each preset
- [ ] Stress testing with real-world YARA rulesets
- [ ] Regression tests for limit enforcement

## 8. Configuration File Format

```toml
# sentinel_sandbox.toml

[sandbox]
preset = "balanced"  # conservative, balanced, performance, or custom

[sandbox.limits]
# Custom limits (overrides preset)
linear_memory_max_mb = 128
stack_size_kb = 1024
fuel_budget = 500000000
timeout_ms = 100
max_table_elements = 5000

[sandbox.features]
enable_threads = false
enable_simd = true
enable_bulk_memory = true

[sandbox.telemetry]
enabled = true
log_level = "info"  # debug, info, warn, error
log_limit_violations = true
log_performance_outliers = true
alert_on_abnormal_usage = true

[sandbox.performance]
# Performance tuning
jit_optimization_level = "speed"  # speed, size, none
aot_compilation = false  # Pre-compile modules
memory_pool_size = 10  # Reuse stores for this many scans
```

## 9. Future Enhancements

### 9.1 AOT Compilation
Pre-compile trusted YARA modules to eliminate JIT overhead:
```cpp
// Compile module to native code
auto aot_bytes = module.serialize();
write_file("yara_rules.aot", aot_bytes);

// Later: load pre-compiled module
auto module = wasmtime::Module::deserialize(engine, aot_bytes);
// 5-10ms savings per scan
```

### 9.2 Sandboxed Process Isolation
Run Wasmtime in separate process with seccomp-bpf:
```cpp
// Fork child process
pid_t child = fork();
if (child == 0) {
    // Child: install seccomp filter
    install_seccomp_filter();

    // Execute YARA scan
    auto result = wasm_executor.scan(file_data);
    exit(0);
}

// Parent: wait with timeout
// Provides additional security layer beyond WASM sandbox
```

### 9.3 Multi-Tier Scanning
Fast path for trusted files, strict limits for unknown files:
```cpp
SandboxConfig choose_config(FileReputation reputation) {
    switch (reputation) {
    case FileReputation::Trusted:
        return PerformancePreset{};  // Fast scan
    case FileReputation::Unknown:
        return BalancedPreset{};     // Standard scan
    case FileReputation::Suspicious:
        return ConservativePreset{}; // Strict limits
    }
}
```

### 9.4 Parallel Scanning
Scan multiple files concurrently:
```cpp
// Each file gets its own Store (thread-safe)
std::vector<std::future<YaraResult>> futures;
for (auto const& file : files) {
    futures.push_back(std::async([&]() {
        auto executor = WasmExecutor::create(config);
        return executor->scan(file.data());
    }));
}

// Aggregate results
for (auto& future : futures) {
    auto result = future.get();
    // Process result
}
```

## 10. References

- **Wasmtime Documentation:** https://docs.wasmtime.dev/
- **Wasmtime Config API:** https://docs.rs/wasmtime/latest/wasmtime/struct.Config.html
- **Wasmtime Store API:** https://docs.rs/wasmtime/latest/wasmtime/struct.Store.html
- **WebAssembly Spec:** https://webassembly.github.io/spec/
- **YARA Documentation:** https://yara.readthedocs.io/
- **CVE Database (WASM/JIT):** https://cve.mitre.org/
- **Cranelift JIT Security:** https://github.com/bytecodealliance/wasmtime/blob/main/cranelift/docs/security.md

## 11. Summary

This document provides a comprehensive framework for resource limits in the Wasmtime sandbox:

**Key Takeaways:**
1. **Multi-layered defense:** Fuel + timeout + memory + stack limits
2. **Three presets:** Conservative (secure), Balanced (default), Performance (fast)
3. **Attack mitigation:** Prevents infinite loops, memory bombs, stack overflow, etc.
4. **Performance cost:** 10-30% overhead from fuel metering, <100ms total execution
5. **Telemetry:** Track resource usage, detect anomalies, tune limits
6. **Fail-safe:** Limit violations result in quarantine, not crash

**Recommended Starting Configuration:** Balanced preset (128MB, 500M fuel, 100ms timeout)

**Next Steps:**
1. Implement basic limits in `WasmExecutor.cpp`
2. Add telemetry logging
3. Create malicious WASM test suite
4. Benchmark with real YARA rulesets
5. Tune limits based on observed performance
