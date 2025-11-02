/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "../MalwareML.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

using AK::Duration;

namespace Sentinel {
    class PolicyGraph;  // Milestone 0.5 Phase 1d
}

namespace Sentinel::Sandbox {

// Forward declarations
class WasmExecutor;
class BehavioralAnalyzer;
class VerdictEngine;
class Reporter;

// Sandbox execution result with comprehensive analysis
struct SandboxResult {
    enum class ThreatLevel {
        Clean = 0,
        Suspicious = 1,
        Malicious = 2,
        Critical = 3
    };

    ThreatLevel threat_level { ThreatLevel::Clean };
    float confidence { 0.0f };                    // 0.0-1.0
    Vector<String> detected_behaviors;            // Human-readable behaviors
    Vector<String> triggered_rules;               // YARA/ML/Behavioral rules
    Duration execution_time;                      // Total sandbox time
    String verdict_explanation;                   // User-facing summary

    // Detailed scoring breakdown
    float yara_score { 0.0f };                    // 0.0-1.0 (40% weight)
    float ml_score { 0.0f };                      // 0.0-1.0 (35% weight)
    float behavioral_score { 0.0f };              // 0.0-1.0 (25% weight)
    float composite_score { 0.0f };               // Final weighted score

    // Metrics from behavioral analysis
    u32 file_operations { 0 };
    u32 process_operations { 0 };
    u32 network_operations { 0 };
    u32 registry_operations { 0 };                // Windows only
    u32 memory_operations { 0 };

    bool is_malicious() const { return threat_level >= ThreatLevel::Malicious; }
    bool is_suspicious() const { return threat_level >= ThreatLevel::Suspicious; }
};

// Sandbox configuration options
struct SandboxConfig {
    Duration timeout { Duration::from_seconds(5) };   // Max execution time
    bool enable_tier1_wasm { true };                  // Fast WASM pre-analysis
    bool enable_tier2_native { true };                // Deep OS-level analysis
    bool allow_network { false };                     // Network access in sandbox
    bool allow_filesystem { false };                  // File I/O in sandbox
    u64 max_memory_bytes { 128 * 1024 * 1024 };      // 128 MB memory limit
};

// Orchestrator - coordinates sandbox execution and analysis
// Milestone 0.5 Phase 1: Real-time Sandboxing for Suspicious Downloads
class Orchestrator {
public:
    // Create orchestrator with default configuration
    static ErrorOr<NonnullOwnPtr<Orchestrator>> create();
    static ErrorOr<NonnullOwnPtr<Orchestrator>> create(SandboxConfig const& config);

    ~Orchestrator();

    // Main entry point: sandbox a file and return threat assessment
    // 1. Extract features (YARA + ML)
    // 2. If suspicious: Run Tier 1 WASM sandbox
    // 3. If still suspicious: Run Tier 2 native sandbox
    // 4. Generate verdict with confidence score
    ErrorOr<SandboxResult> analyze_file(ByteBuffer const& file_data, String const& filename);

    // Analyze with pre-computed ML features (optimization)
    ErrorOr<SandboxResult> analyze_file_with_features(
        ByteBuffer const& file_data,
        String const& filename,
        MalwareMLDetector::Features const& features);

    // Statistics for monitoring
    struct Statistics {
        u64 total_files_analyzed { 0 };
        u64 tier1_executions { 0 };                   // WASM sandbox runs
        u64 tier2_executions { 0 };                   // Native sandbox runs
        u64 malicious_detected { 0 };
        u64 timeouts { 0 };
        Duration average_tier1_time;
        Duration average_tier2_time;
        Duration average_total_time;
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

    // Configuration management
    SandboxConfig const& config() const { return m_config; }
    void update_config(SandboxConfig const& config);

private:
    explicit Orchestrator(SandboxConfig const& config);

    ErrorOr<void> initialize_components();

    // Execution pipeline stages
    ErrorOr<SandboxResult> execute_tier1_wasm(ByteBuffer const& file_data, String const& filename);
    ErrorOr<SandboxResult> execute_tier2_native(ByteBuffer const& file_data, String const& filename);
    ErrorOr<void> generate_verdict(SandboxResult& result);

    // Component lifecycle
    ErrorOr<void> setup_sandbox_environment();
    void cleanup_sandbox_environment();

    SandboxConfig m_config;
    Statistics m_stats;

    // Sandbox components (owned)
    OwnPtr<WasmExecutor> m_wasm_executor;
    OwnPtr<BehavioralAnalyzer> m_behavioral_analyzer;
    OwnPtr<VerdictEngine> m_verdict_engine;
    OwnPtr<PolicyGraph> m_policy_graph;  // Milestone 0.5 Phase 1d: Verdict cache
    // OwnPtr<Reporter> m_reporter; // TODO: Implement Reporter component
};

}
