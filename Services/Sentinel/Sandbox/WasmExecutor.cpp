/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/Math.h>
#include <AK/NonnullOwnPtr.h>
#include <LibCore/File.h>

#ifdef ENABLE_WASMTIME
#    include <wasmtime.h>
#    include <chrono>
#    include <thread>
#endif

#include "../MalwareML.h"
#include "WasmExecutor.h"

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<WasmExecutor>> WasmExecutor::create(SandboxConfig const& config)
{
    auto executor = adopt_own(*new WasmExecutor(config));

    // Try to initialize Wasmtime, fall back to stub if not available
    auto wasmtime_result = executor->initialize_wasmtime();
    if (wasmtime_result.is_error()) {
        dbgln("WasmExecutor: Wasmtime not available, using stub implementation: {}", wasmtime_result.error());
        executor->m_use_stub = true;
    } else {
        dbgln_if(false, "WasmExecutor: Wasmtime initialized successfully");
        executor->m_use_stub = false;

        // Try to load WASM module
        auto module_result = executor->load_module("/usr/share/ladybird/malware_analyzer.wasm"_string);
        if (module_result.is_error()) {
            // Try alternative path (for development)
            auto dev_path = "/home/rbsmith4/ladybird/Services/Sentinel/assets/malware_analyzer.wasm"_string;
            module_result = executor->load_module(dev_path);

            if (module_result.is_error()) {
                dbgln("WasmExecutor: Could not load WASM module: {}", module_result.error());
                dbgln("WasmExecutor: Falling back to stub mode");
                executor->m_use_stub = true;
            } else {
                dbgln_if(false, "WasmExecutor: WASM module loaded from development path");
            }
        } else {
            dbgln_if(false, "WasmExecutor: WASM module loaded from system path");
        }
    }

    return executor;
}

WasmExecutor::WasmExecutor(SandboxConfig const& config)
    : m_config(config)
{
}

WasmExecutor::~WasmExecutor()
{
#ifdef ENABLE_WASMTIME
    if (m_wasm_module) {
        wasm_module_delete(static_cast<wasm_module_t*>(m_wasm_module));
        m_wasm_module = nullptr;
    }
    if (m_wasmtime_store) {
        wasmtime_store_delete(static_cast<wasmtime_store_t*>(m_wasmtime_store));
        m_wasmtime_store = nullptr;
    }
    if (m_wasmtime_engine) {
        wasm_engine_delete(static_cast<wasm_engine_t*>(m_wasmtime_engine));
        m_wasmtime_engine = nullptr;
    }
#endif
}

ErrorOr<void> WasmExecutor::initialize_wasmtime()
{
#ifdef ENABLE_WASMTIME
    // Create Wasmtime configuration with sandbox limits
    wasm_config_t* config = wasm_config_new();
    if (!config)
        return Error::from_string_literal("Failed to create Wasmtime config");

    // Enable fuel consumption for CPU limiting
    wasmtime_config_consume_fuel_set(config, true);

    // Enable epoch interruption for timeout enforcement
    wasmtime_config_epoch_interruption_set(config, true);

    // Create Wasmtime engine with configuration
    wasm_engine_t* engine = wasm_engine_new_with_config(config);
    if (!engine)
        return Error::from_string_literal("Failed to create Wasmtime engine");

    m_wasmtime_engine = engine;

    // Create Wasmtime store
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    if (!store) {
        wasm_engine_delete(engine);
        m_wasmtime_engine = nullptr;
        return Error::from_string_literal("Failed to create Wasmtime store");
    }

    m_wasmtime_store = store;

    // Configure resource limits
    TRY(setup_wasm_limits());

    dbgln_if(false, "WasmExecutor: Wasmtime initialized successfully");
    return {};
#else
    return Error::from_string_literal("Wasmtime support not compiled in");
#endif
}

ErrorOr<void> WasmExecutor::setup_wasm_limits()
{
#ifdef ENABLE_WASMTIME
    VERIFY(m_wasmtime_store);

    auto* store = static_cast<wasmtime_store_t*>(m_wasmtime_store);
    auto* context = wasmtime_store_context(store);

    // Set memory and table limits using store limiter
    // Parameters: memory_size, table_elements, instances, tables, memories
    wasmtime_store_limiter(
        store,
        m_config.max_memory_bytes,  // Maximum memory in bytes
        10000,                       // Maximum table elements
        1,                           // Maximum instances
        10,                          // Maximum tables
        1                            // Maximum memories
    );

    // Set fuel limit (CPU limiting via instruction budget)
    // 500M instructions ≈ 100-200ms execution time
    u64 fuel_amount = 500'000'000;
    wasmtime_error_t* error = wasmtime_context_set_fuel(context, fuel_amount);
    if (error) {
        // Non-fatal: fuel may not be enabled
        wasmtime_error_delete(error);
        dbgln_if(false, "WasmExecutor: Warning - could not add fuel (fuel metering may not be enabled)");
    }

    // Set epoch deadline for timeout enforcement
    // Epoch will be incremented after timeout to interrupt execution
    wasmtime_context_set_epoch_deadline(context, 1);

    dbgln_if(false, "WasmExecutor: Configured limits - Memory: {} bytes, Fuel: {} instructions, Timeout: {}ms",
        m_config.max_memory_bytes, fuel_amount, m_config.timeout.to_milliseconds());

    return {};
#else
    return {};
#endif
}

ErrorOr<void> WasmExecutor::load_module(String const& module_path)
{
#ifdef ENABLE_WASMTIME
    VERIFY(m_wasmtime_engine);

    dbgln_if(false, "WasmExecutor: Loading WASM module from: {}", module_path);

    // Read WASM module file
    auto file_or_error = Core::File::open(module_path, Core::File::OpenMode::Read);
    if (file_or_error.is_error()) {
        return Error::from_string_literal("Failed to open WASM module file");
    }

    auto file = file_or_error.release_value();
    auto module_data_or_error = file->read_until_eof();
    if (module_data_or_error.is_error()) {
        return Error::from_string_literal("Failed to read WASM module file");
    }

    auto module_data = module_data_or_error.release_value();

    dbgln_if(false, "WasmExecutor: Read {} bytes from WASM module", module_data.size());

    // Create WASM module from bytecode
    wasmtime_error_t* error = nullptr;
    wasmtime_module_t* module = nullptr;

    auto* engine = static_cast<wasm_engine_t*>(m_wasmtime_engine);
    error = wasmtime_module_new(engine, module_data.data(), module_data.size(), &module);

    if (error) {
        // Get error message
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);

        dbgln("WasmExecutor: Failed to load WASM module: {}",
            StringView { error_msg.data, error_msg.size });

        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);

        return Error::from_string_literal("Failed to create WASM module");
    }

    // Store module (delete old one if exists)
    if (m_wasm_module) {
        wasmtime_module_delete(static_cast<wasmtime_module_t*>(m_wasm_module));
    }

    m_wasm_module = module;

    dbgln_if(false, "WasmExecutor: WASM module loaded successfully");
    return {};
#else
    (void)module_path;
    return Error::from_string_literal("Wasmtime support not compiled in");
#endif
}

ErrorOr<WasmExecutionResult> WasmExecutor::execute(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    dbgln_if(false, "WasmExecutor: Executing '{}' ({} bytes) with timeout {}ms",
        filename, file_data.size(), timeout.to_milliseconds());

    auto start_time = MonotonicTime::now();
    m_stats.total_executions++;

    WasmExecutionResult result;

    // Execute using stub or Wasmtime based on availability
    if (m_use_stub) {
        dbgln_if(false, "WasmExecutor: Using STUB mode for execution");
        auto stub_result = TRY(execute_stub(file_data, filename));
        result = move(stub_result);
    } else {
        dbgln_if(false, "WasmExecutor: Using REAL WASMTIME for execution");
        auto wasmtime_result = TRY(execute_wasmtime(file_data, filename));
        result = move(wasmtime_result);
    }

    result.execution_time = MonotonicTime::now() - start_time;

    // Update statistics
    if (m_stats.total_executions > 0) {
        m_stats.average_execution_time = Duration::from_nanoseconds(
            (m_stats.average_execution_time.to_nanoseconds() * (m_stats.total_executions - 1) + result.execution_time.to_nanoseconds()) / m_stats.total_executions);
    }

    if (result.execution_time > m_stats.max_execution_time)
        m_stats.max_execution_time = result.execution_time;

    if (result.timed_out)
        m_stats.timeouts++;

    dbgln_if(false, "WasmExecutor: Execution complete in {}ms - YARA: {:.2f}, ML: {:.2f}",
        result.execution_time.to_milliseconds(), result.yara_score, result.ml_score);

    return result;
}

ErrorOr<WasmExecutionResult> WasmExecutor::execute_stub(ByteBuffer const& file_data, [[maybe_unused]] String const& filename)
{
    // Stub implementation: Fast heuristic analysis without WASM
    // This simulates what Tier 1 WASM sandbox would do:
    // 1. Quick pattern matching (YARA-like)
    // 2. Basic ML feature extraction and scoring
    // 3. Suspicious behavior detection

    dbgln_if(false, "WasmExecutor: Using stub implementation for fast analysis");

    WasmExecutionResult result;

    // Calculate YARA heuristic score (pattern-based)
    result.yara_score = TRY(calculate_yara_heuristic(file_data));

    // Calculate ML heuristic score (feature-based)
    result.ml_score = TRY(calculate_ml_heuristic(file_data));

    // Detect suspicious patterns
    result.detected_behaviors = TRY(detect_suspicious_patterns(file_data));

    // Generate triggered rules based on scores
    if (result.yara_score > 0.5f) {
        TRY(result.triggered_rules.try_append(TRY(String::formatted(
            "YARA heuristic: High entropy and suspicious strings (score: {:.2f})", result.yara_score))));
    }

    if (result.ml_score > 0.5f) {
        TRY(result.triggered_rules.try_append(TRY(String::formatted(
            "ML heuristic: Malware-like features detected (score: {:.2f})", result.ml_score))));
    }

    result.timed_out = false; // Stub is always fast

    return result;
}

ErrorOr<WasmExecutionResult> WasmExecutor::execute_wasmtime(ByteBuffer const& file_data, [[maybe_unused]] String const& filename)
{
#ifdef ENABLE_WASMTIME
    VERIFY(m_wasmtime_store);
    VERIFY(m_wasmtime_engine);
    VERIFY(m_wasm_module);

    dbgln_if(false, "WasmExecutor: Executing analysis with WASM module ({} bytes)", file_data.size());

    WasmExecutionResult result;
    auto* store = static_cast<wasmtime_store_t*>(m_wasmtime_store);
    auto* engine = static_cast<wasm_engine_t*>(m_wasmtime_engine);
    auto* module = static_cast<wasmtime_module_t*>(m_wasm_module);
    auto* context = wasmtime_store_context(store);

    // Step 1: Create linker and define host imports
    wasmtime_linker_t* linker = wasmtime_linker_new(engine);
    if (!linker)
        return Error::from_string_literal("Failed to create Wasmtime linker");

    // Define host import: log(level: u32, ptr: i32, len: i32)
    // Signature: (i32, i32, i32) -> ()
    wasm_valtype_t* log_params[3] = {
        wasm_valtype_new_i32(),
        wasm_valtype_new_i32(),
        wasm_valtype_new_i32()
    };
    wasm_valtype_vec_t log_params_vec;
    wasm_valtype_vec_new(&log_params_vec, 3, log_params);

    wasm_valtype_vec_t log_results_vec;
    wasm_valtype_vec_new_empty(&log_results_vec);

    wasm_functype_t* log_functype = wasm_functype_new(&log_params_vec, &log_results_vec);

    // Create host function callback
    auto log_callback = +[](void* env, wasmtime_caller_t* caller, wasmtime_val_t const* args, size_t nargs, wasmtime_val_t* results, size_t nresults) -> wasm_trap_t* {
        (void)env;
        (void)results;
        (void)nresults;

        if (nargs != 3) {
            return wasm_trap_new(nullptr, 0);
        }

        i32 level = args[0].of.i32;
        i32 ptr = args[1].of.i32;
        i32 len = args[2].of.i32;

        // Get memory to read log message
        wasmtime_context_t* ctx = wasmtime_caller_context(caller);
        wasmtime_extern_t memory_extern;
        if (!wasmtime_caller_export_get(caller, "memory", 6, &memory_extern) || memory_extern.kind != WASMTIME_EXTERN_MEMORY) {
            return wasm_trap_new(nullptr, 0);
        }

        wasmtime_memory_t memory = memory_extern.of.memory;
        u8* memory_data = wasmtime_memory_data(ctx, &memory);
        size_t memory_size = wasmtime_memory_data_size(ctx, &memory);

        if (ptr < 0 || len < 0 || static_cast<size_t>(ptr) + static_cast<size_t>(len) > memory_size) {
            return wasm_trap_new(nullptr, 0);
        }

        // Read and log message
        StringView message { reinterpret_cast<char const*>(memory_data + ptr), static_cast<size_t>(len) };
        dbgln_if(false, "WASM[{}]: {}", level, message);

        return nullptr;
    };

    wasmtime_func_t log_func;
    wasmtime_func_new(context, log_functype, log_callback, nullptr, nullptr, &log_func);
    wasm_functype_delete(log_functype);

    wasmtime_extern_t log_extern;
    log_extern.kind = WASMTIME_EXTERN_FUNC;
    log_extern.of.func = log_func;

    wasmtime_error_t* error = wasmtime_linker_define(linker, context, "", 0, "log", 3, &log_extern);
    if (error) {
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Failed to define 'log' import: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
        wasmtime_linker_delete(linker);
        return Error::from_string_literal("Failed to define log host function");
    }

    // Define host import: current_time_ms() -> u64
    // Signature: () -> i64
    wasm_valtype_vec_t time_params_vec;
    wasm_valtype_vec_new_empty(&time_params_vec);

    wasm_valtype_t* time_results[1] = { wasm_valtype_new_i64() };
    wasm_valtype_vec_t time_results_vec;
    wasm_valtype_vec_new(&time_results_vec, 1, time_results);

    wasm_functype_t* time_functype = wasm_functype_new(&time_params_vec, &time_results_vec);

    auto time_callback = +[](void* env, wasmtime_caller_t* caller, wasmtime_val_t const* args, size_t nargs, wasmtime_val_t* results, size_t nresults) -> wasm_trap_t* {
        (void)env;
        (void)caller;
        (void)args;
        (void)nargs;

        if (nresults != 1)
            return nullptr;

        // Return current time in milliseconds
        auto now = MonotonicTime::now();
        results[0].kind = WASMTIME_I64;
        results[0].of.i64 = now.milliseconds();

        return nullptr;
    };

    wasmtime_func_t time_func;
    wasmtime_func_new(context, time_functype, time_callback, nullptr, nullptr, &time_func);
    wasm_functype_delete(time_functype);

    wasmtime_extern_t time_extern;
    time_extern.kind = WASMTIME_EXTERN_FUNC;
    time_extern.of.func = time_func;

    error = wasmtime_linker_define(linker, context, "", 0, "current_time_ms", 16, &time_extern);
    if (error) {
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Failed to define 'current_time_ms' import: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
        wasmtime_linker_delete(linker);
        return Error::from_string_literal("Failed to define current_time_ms host function");
    }

    // Step 2: Instantiate module with host imports
    wasmtime_instance_t instance;
    wasm_trap_t* trap = nullptr;
    error = wasmtime_linker_instantiate(linker, context, module, &instance, &trap);

    if (error) {
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Failed to instantiate module: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
        wasmtime_linker_delete(linker);
        return Error::from_string_literal("Failed to instantiate WASM module");
    }

    if (trap) {
        wasm_message_t trap_msg;
        wasm_trap_message(trap, &trap_msg);
        dbgln("WasmExecutor: Trap during module instantiation: {}", StringView { trap_msg.data, trap_msg.size });
        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(trap);
        wasmtime_linker_delete(linker);
        return Error::from_string_literal("Trap during WASM module instantiation");
    }

    wasmtime_linker_delete(linker);

    // Step 3: Get exported functions
    wasmtime_extern_t allocate_extern, analyze_extern, deallocate_extern, memory_extern;

    if (!wasmtime_instance_export_get(context, &instance, "allocate", 8, &allocate_extern)) {
        dbgln("WasmExecutor: Failed to get 'allocate' export");
        return Error::from_string_literal("WASM module missing 'allocate' export");
    }

    if (!wasmtime_instance_export_get(context, &instance, "analyze_file", 12, &analyze_extern)) {
        dbgln("WasmExecutor: Failed to get 'analyze_file' export");
        return Error::from_string_literal("WASM module missing 'analyze_file' export");
    }

    if (!wasmtime_instance_export_get(context, &instance, "deallocate", 10, &deallocate_extern)) {
        dbgln("WasmExecutor: Failed to get 'deallocate' export");
        return Error::from_string_literal("WASM module missing 'deallocate' export");
    }

    if (!wasmtime_instance_export_get(context, &instance, "memory", 6, &memory_extern)) {
        dbgln("WasmExecutor: Failed to get 'memory' export");
        return Error::from_string_literal("WASM module missing 'memory' export");
    }

    // Verify exports are of correct type
    if (allocate_extern.kind != WASMTIME_EXTERN_FUNC || analyze_extern.kind != WASMTIME_EXTERN_FUNC || deallocate_extern.kind != WASMTIME_EXTERN_FUNC) {
        dbgln("WasmExecutor: Exported items are not functions");
        return Error::from_string_literal("WASM exports have incorrect types");
    }

    if (memory_extern.kind != WASMTIME_EXTERN_MEMORY) {
        dbgln("WasmExecutor: Memory export is not a memory");
        return Error::from_string_literal("WASM memory export has incorrect type");
    }

    wasmtime_func_t allocate_func = allocate_extern.of.func;
    wasmtime_func_t analyze_func = analyze_extern.of.func;
    wasmtime_func_t deallocate_func = deallocate_extern.of.func;
    wasmtime_memory_t memory = memory_extern.of.memory;

    // Step 4: Allocate buffer in WASM memory using allocate(size) -> ptr
    wasmtime_val_t alloc_args[1];
    alloc_args[0].kind = WASMTIME_I32;
    alloc_args[0].of.i32 = static_cast<i32>(file_data.size());

    wasmtime_val_t alloc_results[1];
    trap = nullptr;
    error = wasmtime_func_call(context, &allocate_func, alloc_args, 1, alloc_results, 1, &trap);

    if (error) {
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Failed to call allocate: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
        return Error::from_string_literal("Failed to call WASM allocate function");
    }

    if (trap) {
        wasm_message_t trap_msg;
        wasm_trap_message(trap, &trap_msg);
        dbgln("WasmExecutor: Trap during allocate: {}", StringView { trap_msg.data, trap_msg.size });
        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(trap);
        return Error::from_string_literal("Trap during WASM allocate");
    }

    if (alloc_results[0].kind != WASMTIME_I32) {
        dbgln("WasmExecutor: allocate returned non-i32 result");
        return Error::from_string_literal("WASM allocate returned wrong type");
    }

    i32 wasm_buffer_ptr = alloc_results[0].of.i32;
    dbgln_if(false, "WasmExecutor: Allocated {} bytes at WASM offset {}", file_data.size(), wasm_buffer_ptr);

    // Step 5: Copy file data to WASM memory
    u8* memory_data = wasmtime_memory_data(context, &memory);
    size_t memory_size = wasmtime_memory_data_size(context, &memory);

    if (static_cast<size_t>(wasm_buffer_ptr) + file_data.size() > memory_size) {
        dbgln("WasmExecutor: Buffer pointer out of bounds (ptr={}, size={}, memory={})",
            wasm_buffer_ptr, file_data.size(), memory_size);

        // Deallocate before returning error
        wasmtime_val_t dealloc_args[2];
        dealloc_args[0].kind = WASMTIME_I32;
        dealloc_args[0].of.i32 = wasm_buffer_ptr;
        dealloc_args[1].kind = WASMTIME_I32;
        dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());
        wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);
        if (trap)
            wasm_trap_delete(trap);

        return Error::from_string_literal("WASM buffer pointer out of bounds");
    }

    memcpy(memory_data + wasm_buffer_ptr, file_data.data(), file_data.size());
    dbgln_if(false, "WasmExecutor: Copied {} bytes to WASM memory", file_data.size());

    // Start timeout enforcement thread
    TRY(enforce_timeout_async(m_config.timeout));

    // Step 6: Call analyze_file(ptr, size) -> result_ptr
    wasmtime_val_t analyze_args[2];
    analyze_args[0].kind = WASMTIME_I32;
    analyze_args[0].of.i32 = wasm_buffer_ptr;
    analyze_args[1].kind = WASMTIME_I32;
    analyze_args[1].of.i32 = static_cast<i32>(file_data.size());

    wasmtime_val_t analyze_results[1];
    trap = nullptr;
    error = wasmtime_func_call(context, &analyze_func, analyze_args, 2, analyze_results, 1, &trap);

    if (error) {
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Failed to call analyze_file: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);

        // Deallocate before returning error
        wasmtime_val_t dealloc_args[2];
        dealloc_args[0].kind = WASMTIME_I32;
        dealloc_args[0].of.i32 = wasm_buffer_ptr;
        dealloc_args[1].kind = WASMTIME_I32;
        dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());
        wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);
        if (trap)
            wasm_trap_delete(trap);

        return Error::from_string_literal("Failed to call WASM analyze_file function");
    }

    if (trap) {
        wasm_message_t trap_msg;
        wasm_trap_message(trap, &trap_msg);
        StringView trap_message { trap_msg.data, trap_msg.size };

        // Check if this is a timeout trap (epoch deadline exceeded)
        bool is_timeout = trap_message.contains("epoch deadline"sv) ||
                         trap_message.contains("interrupt"sv) ||
                         trap_message.contains("timeout"sv);

        dbgln("WasmExecutor: Trap during analyze_file (timeout={}): {}",
              is_timeout, trap_message);

        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(trap);

        // Deallocate before returning
        wasmtime_val_t dealloc_args[2];
        dealloc_args[0].kind = WASMTIME_I32;
        dealloc_args[0].of.i32 = wasm_buffer_ptr;
        dealloc_args[1].kind = WASMTIME_I32;
        dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());
        wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);
        if (trap)
            wasm_trap_delete(trap);

        // Return timeout result instead of error
        if (is_timeout) {
            WasmExecutionResult timeout_result;
            timeout_result.yara_score = 0.0f;
            timeout_result.ml_score = 0.0f;
            timeout_result.timed_out = true;
            return timeout_result;
        }

        return Error::from_string_literal("Trap during WASM analyze_file");
    }

    if (analyze_results[0].kind != WASMTIME_I32) {
        dbgln("WasmExecutor: analyze_file returned non-i32 result");

        // Deallocate before returning error
        wasmtime_val_t dealloc_args[2];
        dealloc_args[0].kind = WASMTIME_I32;
        dealloc_args[0].of.i32 = wasm_buffer_ptr;
        dealloc_args[1].kind = WASMTIME_I32;
        dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());
        wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);
        if (trap)
            wasm_trap_delete(trap);

        return Error::from_string_literal("WASM analyze_file returned wrong type");
    }

    i32 result_ptr = analyze_results[0].of.i32;
    dbgln_if(false, "WasmExecutor: analyze_file returned result at WASM offset {}", result_ptr);

    // Step 7: Read AnalysisResult struct from WASM memory
    // Struct layout (28 bytes total):
    //   yara_score: f32 (offset 0, 4 bytes)
    //   ml_score: f32 (offset 4, 4 bytes)
    //   detected_patterns: u32 (offset 8, 4 bytes)
    //   execution_time_us: u64 (offset 12, 8 bytes)
    //   error_code: u32 (offset 20, 4 bytes)
    //   padding: u32 (offset 24, 4 bytes)
    constexpr size_t RESULT_STRUCT_SIZE = 28;

    memory_data = wasmtime_memory_data(context, &memory);
    memory_size = wasmtime_memory_data_size(context, &memory);

    if (static_cast<size_t>(result_ptr) + RESULT_STRUCT_SIZE > memory_size) {
        dbgln("WasmExecutor: Result pointer out of bounds (ptr={}, size={}, memory={})",
            result_ptr, RESULT_STRUCT_SIZE, memory_size);

        // Deallocate input buffer (ptr, size)
        wasmtime_val_t dealloc_args[2];
        dealloc_args[0].kind = WASMTIME_I32;
        dealloc_args[0].of.i32 = wasm_buffer_ptr;
        dealloc_args[1].kind = WASMTIME_I32;
        dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());
        wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);
        if (trap)
            wasm_trap_delete(trap);

        return Error::from_string_literal("WASM result pointer out of bounds");
    }

    // Read struct fields from memory (little-endian)
    u8 const* struct_ptr = memory_data + result_ptr;

    // Extract fields using memcpy to avoid alignment issues
    float yara_score, ml_score;
    u32 detected_patterns, error_code;
    u64 execution_time_us;

    memcpy(&yara_score, struct_ptr + 0, sizeof(float));
    memcpy(&ml_score, struct_ptr + 4, sizeof(float));
    memcpy(&detected_patterns, struct_ptr + 8, sizeof(u32));
    memcpy(&execution_time_us, struct_ptr + 12, sizeof(u64));
    memcpy(&error_code, struct_ptr + 20, sizeof(u32));

    result.yara_score = yara_score;
    result.ml_score = ml_score;
    result.timed_out = false;

    dbgln_if(false, "WasmExecutor: Results - YARA: {:.2f}, ML: {:.2f}, Patterns: {}, Error: {}, Time: {} us",
        result.yara_score, result.ml_score, detected_patterns, error_code, execution_time_us);

    // Step 8: Deallocate both buffers (input and result)
    // Deallocate input buffer
    wasmtime_val_t dealloc_args[2];
    dealloc_args[0].kind = WASMTIME_I32;
    dealloc_args[0].of.i32 = wasm_buffer_ptr;
    dealloc_args[1].kind = WASMTIME_I32;
    dealloc_args[1].of.i32 = static_cast<i32>(file_data.size());

    trap = nullptr;
    error = wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);

    if (error) {
        // Non-fatal - log but continue
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Warning - failed to deallocate input buffer: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
    }

    if (trap) {
        // Non-fatal - log but continue
        wasm_message_t trap_msg;
        wasm_trap_message(trap, &trap_msg);
        dbgln("WasmExecutor: Warning - trap during input buffer deallocate: {}", StringView { trap_msg.data, trap_msg.size });
        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(trap);
    }

    // Deallocate result struct
    dealloc_args[0].of.i32 = result_ptr;
    dealloc_args[1].of.i32 = RESULT_STRUCT_SIZE;

    trap = nullptr;
    error = wasmtime_func_call(context, &deallocate_func, dealloc_args, 2, nullptr, 0, &trap);

    if (error) {
        // Non-fatal - log but continue
        wasm_name_t error_msg;
        wasmtime_error_message(error, &error_msg);
        dbgln("WasmExecutor: Warning - failed to deallocate result struct: {}", StringView { error_msg.data, error_msg.size });
        wasm_name_delete(&error_msg);
        wasmtime_error_delete(error);
    }

    if (trap) {
        // Non-fatal - log but continue
        wasm_message_t trap_msg;
        wasm_trap_message(trap, &trap_msg);
        dbgln("WasmExecutor: Warning - trap during result struct deallocate: {}", StringView { trap_msg.data, trap_msg.size });
        wasm_byte_vec_delete(&trap_msg);
        wasm_trap_delete(trap);
    }

    return result;

#else
    (void)file_data;
    (void)filename;
    return Error::from_string_literal("Wasmtime support not compiled in");
#endif
}

ErrorOr<float> WasmExecutor::calculate_yara_heuristic(ByteBuffer const& file_data)
{
    // Fast YARA-like pattern matching
    // Count occurrences of suspicious patterns

    u32 suspicious_pattern_count = 0;

    // Common malware strings (simplified for speed)
    static constexpr StringView suspicious_strings[] = {
        "eval"sv, "exec"sv, "shell"sv, "cmd"sv,
        "createprocess"sv, "virtualalloc"sv, "writeprocessmemory"sv,
        "createremotethread"sv, "loadlibrary"sv, "getprocaddress"sv,
        "http"sv, "https"sv, "ftp"sv,
        "powershell"sv, "cmdexe"sv, "bash"sv,
        "ransomware"sv, "cryptolocker"sv, "wannacry"sv
    };

    String file_content_lower;
    {
        StringBuilder sb;
        for (size_t i = 0; i < file_data.size(); ++i) {
            char c = static_cast<char>(file_data[i]);
            if (c >= 'A' && c <= 'Z')
                c = c + ('a' - 'A'); // Simple lowercase
            if (c >= 'a' && c <= 'z')
                sb.append(c);
        }
        file_content_lower = TRY(sb.to_string());
    }

    for (auto const& pattern : suspicious_strings) {
        if (file_content_lower.bytes_as_string_view().contains(pattern))
            suspicious_pattern_count++;
    }

    // Score: 0.0 (none) to 1.0 (many patterns)
    float score = min(1.0f, suspicious_pattern_count / 10.0f);

    dbgln_if(false, "WasmExecutor: YARA heuristic - {} patterns, score: {:.2f}",
        suspicious_pattern_count, score);

    return score;
}

ErrorOr<float> WasmExecutor::calculate_ml_heuristic(ByteBuffer const& file_data)
{
    // Fast ML-like heuristic using basic features
    // This mirrors MalwareML but optimized for speed

    // Calculate Shannon entropy (complexity measure)
    u32 byte_counts[256] = {};
    for (size_t i = 0; i < file_data.size(); ++i)
        byte_counts[file_data[i]]++;

    float entropy = 0.0f;
    for (size_t i = 0; i < 256; ++i) {
        if (byte_counts[i] > 0) {
            float probability = static_cast<float>(byte_counts[i]) / static_cast<float>(file_data.size());
            entropy -= probability * AK::log2(probability);
        }
    }

    // High entropy (>7.0) suggests encryption/packing (common in malware)
    float entropy_score = (entropy > 7.0f) ? 0.8f : (entropy > 6.0f) ? 0.5f : 0.2f;

    // Small files with high entropy are more suspicious
    float size_score = (file_data.size() < 10000 && entropy > 6.5f) ? 0.7f : 0.3f;

    // Combine scores
    float ml_score = (entropy_score + size_score) / 2.0f;

    dbgln_if(false, "WasmExecutor: ML heuristic - Entropy: {:.2f}, Score: {:.2f}",
        entropy, ml_score);

    return ml_score;
}

ErrorOr<Vector<String>> WasmExecutor::detect_suspicious_patterns(ByteBuffer const& file_data)
{
    Vector<String> behaviors;

    // Pattern 1: PE header (Windows executable)
    if (file_data.size() > 2 && file_data[0] == 'M' && file_data[1] == 'Z') {
        TRY(behaviors.try_append("Windows PE executable detected"_string));
    }

    // Pattern 2: ELF header (Linux executable)
    if (file_data.size() > 4 && file_data[0] == 0x7F && file_data[1] == 'E' && file_data[2] == 'L' && file_data[3] == 'F') {
        TRY(behaviors.try_append("Linux ELF executable detected"_string));
    }

    // Pattern 3: Shebang (script)
    if (file_data.size() > 2 && file_data[0] == '#' && file_data[1] == '!') {
        TRY(behaviors.try_append("Script with shebang detected"_string));
    }

    // Pattern 4: High entropy (packed/encrypted)
    u32 byte_counts[256] = {};
    for (size_t i = 0; i < file_data.size(); ++i)
        byte_counts[file_data[i]]++;

    float entropy = 0.0f;
    for (size_t i = 0; i < 256; ++i) {
        if (byte_counts[i] > 0) {
            float probability = static_cast<float>(byte_counts[i]) / static_cast<float>(file_data.size());
            entropy -= probability * AK::log2(probability);
        }
    }

    if (entropy > 7.0f) {
        TRY(behaviors.try_append(TRY(String::formatted("High entropy ({:.2f}) - possible packing/encryption", entropy))));
    }

    // Pattern 5: Embedded URLs
    StringView content { file_data.bytes() };
    if (content.contains("http://"sv) || content.contains("https://"sv)) {
        TRY(behaviors.try_append("Embedded URLs detected"_string));
    }

    return behaviors;
}

ErrorOr<void> WasmExecutor::precompile_module(ByteBuffer const& wasm_module)
{
    // TODO: Precompile WASM module for faster execution
    // This is an optimization for repeatedly executing the same module

    dbgln_if(false, "WasmExecutor: Precompiling module ({} bytes)", wasm_module.size());

    return {};
}

void WasmExecutor::reset_statistics()
{
    m_stats = Statistics {};
    dbgln_if(false, "WasmExecutor: Statistics reset");
}

ErrorOr<void> WasmExecutor::enforce_timeout_async(Duration timeout)
{
#ifdef ENABLE_WASMTIME
    VERIFY(m_wasmtime_engine);

    auto* engine = static_cast<wasm_engine_t*>(m_wasmtime_engine);

    // Create a detached thread that will increment the epoch after timeout
    // This uses std::thread since we need portable thread support
    auto timeout_ms = timeout.to_milliseconds();

    // Increment epoch in a detached thread to interrupt execution
    std::thread([engine, timeout_ms]() {
        // Sleep for timeout duration
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));

        // Increment engine epoch - this will interrupt any running WASM code
        // that exceeded its epoch deadline
        wasmtime_engine_increment_epoch(engine);

        dbgln_if(false, "WasmExecutor: Timeout expired ({}ms), epoch incremented", timeout_ms);
    }).detach();

    dbgln_if(false, "WasmExecutor: Started timeout enforcement thread ({}ms)", timeout_ms);
    return {};
#else
    (void)timeout;
    return {};
#endif
}

}
