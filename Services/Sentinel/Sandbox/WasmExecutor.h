/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Orchestrator.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel::Sandbox {

// WASM execution result from Tier 1 analysis
struct WasmExecutionResult {
    float yara_score { 0.0f };                    // 0.0-1.0
    float ml_score { 0.0f };                      // 0.0-1.0
    Vector<String> detected_behaviors;            // Behavioral observations
    Vector<String> triggered_rules;               // YARA/ML rules
    bool timed_out { false };
    Duration execution_time;
};

// WasmExecutor - Tier 1 fast pre-analysis using Wasmtime sandbox
// Milestone 0.5 Phase 1: Real-time Sandboxing
//
// This component executes suspicious files in a WASM sandbox for fast
// pre-analysis without full OS-level sandboxing overhead. Target: 50-100ms.
//
// Implementation Strategy:
// - Phase 1a: Stub implementation (simulates WASM execution)
// - Phase 1b: Wasmtime integration (real WASM sandbox)
//
// The stub allows development and testing of the sandbox infrastructure
// without requiring Wasmtime installation, similar to the ML stub approach.
class WasmExecutor {
public:
    static ErrorOr<NonnullOwnPtr<WasmExecutor>> create(SandboxConfig const& config);

    ~WasmExecutor();

    // Load WASM module from file
    ErrorOr<void> load_module(String const& module_path);

    // Execute file in WASM sandbox with timeout
    // Returns YARA + ML scores from fast heuristic analysis
    ErrorOr<WasmExecutionResult> execute(
        ByteBuffer const& file_data,
        String const& filename,
        Duration timeout);

    // Precompile WASM module for faster execution (optional optimization)
    ErrorOr<void> precompile_module(ByteBuffer const& wasm_module);

    // Statistics
    struct Statistics {
        u64 total_executions { 0 };
        u64 timeouts { 0 };
        u64 errors { 0 };
        Duration average_execution_time;
        Duration max_execution_time;
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

private:
    explicit WasmExecutor(SandboxConfig const& config);

    ErrorOr<void> initialize_wasmtime();
    ErrorOr<void> setup_wasm_limits();

    // Execution helpers
    ErrorOr<WasmExecutionResult> execute_stub(ByteBuffer const& file_data, String const& filename);
    ErrorOr<WasmExecutionResult> execute_wasmtime(ByteBuffer const& file_data, String const& filename);

    // Heuristic analysis (used by stub, fast path for Wasmtime)
    ErrorOr<float> calculate_yara_heuristic(ByteBuffer const& file_data);
    ErrorOr<float> calculate_ml_heuristic(ByteBuffer const& file_data);
    ErrorOr<Vector<String>> detect_suspicious_patterns(ByteBuffer const& file_data);

    SandboxConfig m_config;
    Statistics m_stats;

    // Wasmtime runtime (opaque pointer, null if using stub)
#ifdef ENABLE_WASMTIME
    void* m_wasmtime_engine { nullptr };      // wasm_engine_t*
    void* m_wasmtime_store { nullptr };       // wasmtime_store_t*
    void* m_wasm_module { nullptr };          // wasm_module_t*
#else
    [[maybe_unused]] void* m_wasmtime_engine { nullptr };
    [[maybe_unused]] void* m_wasmtime_store { nullptr };
    [[maybe_unused]] void* m_wasm_module { nullptr };
#endif

    bool m_use_stub { true };                 // True if Wasmtime not available

    // Timeout enforcement helper
    ErrorOr<void> enforce_timeout_async(Duration timeout);
};

}
