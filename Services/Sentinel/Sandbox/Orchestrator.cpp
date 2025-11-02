/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/NonnullOwnPtr.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "BehavioralAnalyzer.h"
#include "Orchestrator.h"
#include "VerdictEngine.h"
#include "WasmExecutor.h"
#include "../PolicyGraph.h"

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<Orchestrator>> Orchestrator::create()
{
    return create(SandboxConfig {});
}

ErrorOr<NonnullOwnPtr<Orchestrator>> Orchestrator::create(SandboxConfig const& config)
{
    auto orchestrator = adopt_own(*new Orchestrator(config));
    TRY(orchestrator->initialize_components());
    return orchestrator;
}

Orchestrator::Orchestrator(SandboxConfig const& config)
    : m_config(config)
{
}

Orchestrator::~Orchestrator()
{
    cleanup_sandbox_environment();
}

ErrorOr<void> Orchestrator::initialize_components()
{
    dbgln_if(false, "Orchestrator: Initializing sandbox components");

    // Initialize WASM executor (Tier 1 fast analysis)
    if (m_config.enable_tier1_wasm) {
        auto wasm_executor = TRY(WasmExecutor::create(m_config));
        m_wasm_executor = move(wasm_executor);
        dbgln_if(false, "Orchestrator: WASM executor initialized");
    }

    // Initialize behavioral analyzer (Tier 2 deep analysis)
    if (m_config.enable_tier2_native) {
        auto analyzer = TRY(BehavioralAnalyzer::create(m_config));
        m_behavioral_analyzer = move(analyzer);
        dbgln_if(false, "Orchestrator: Behavioral analyzer initialized");
    }

    // Initialize verdict engine (scoring and decision)
    auto verdict_engine = TRY(VerdictEngine::create());
    m_verdict_engine = move(verdict_engine);
    dbgln_if(false, "Orchestrator: Verdict engine initialized");

    // Initialize policy graph (Milestone 0.5 Phase 1d: verdict cache)
    // Create in ~/.cache/ladybird/sentinel directory (same as RequestServer)
    auto cache_dir = Core::StandardPaths::cache_directory();
    auto ladybird_cache_dir = ByteString::formatted("{}/ladybird", cache_dir);
    auto sentinel_cache_dir = ByteString::formatted("{}/sentinel", ladybird_cache_dir);

    // Ensure parent directories exist
    if (!FileSystem::exists(ladybird_cache_dir)) {
        auto mkdir_result = Core::System::mkdir(ladybird_cache_dir, 0700);
        if (mkdir_result.is_error()) {
            dbgln("Orchestrator: Warning - failed to create ladybird cache directory: {}", mkdir_result.error());
            dbgln("Orchestrator: Verdict caching will be disabled");
        }
    }

    if (FileSystem::exists(ladybird_cache_dir)) {
        auto policy_graph_result = PolicyGraph::create(sentinel_cache_dir);
        if (policy_graph_result.is_error()) {
            dbgln("Orchestrator: Warning - failed to initialize PolicyGraph: {}", policy_graph_result.error());
            dbgln("Orchestrator: Verdict caching will be disabled");
            // Continue without PolicyGraph - it's optional
        } else {
            m_policy_graph = policy_graph_result.release_value();
            dbgln_if(false, "Orchestrator: PolicyGraph initialized (verdict caching enabled)");
        }
    }

    TRY(setup_sandbox_environment());

    return {};
}

ErrorOr<void> Orchestrator::setup_sandbox_environment()
{
    // TODO: Create temporary directories for sandbox isolation
    // TODO: Set up seccomp filters for syscall monitoring
    // TODO: Initialize resource limits (memory, CPU)

    dbgln_if(false, "Orchestrator: Sandbox environment setup complete");
    return {};
}

void Orchestrator::cleanup_sandbox_environment()
{
    // TODO: Clean up temporary sandbox directories
    // TODO: Release seccomp resources

    dbgln_if(false, "Orchestrator: Sandbox environment cleaned up");
}

ErrorOr<SandboxResult> Orchestrator::analyze_file(ByteBuffer const& file_data, String const& filename)
{
    dbgln_if(false, "Orchestrator: Analyzing file '{}' ({} bytes)", filename, file_data.size());

    auto start_time = MonotonicTime::now();
    m_stats.total_files_analyzed++;

    // Milestone 0.5 Phase 1d: Check verdict cache before expensive sandbox analysis
    String file_hash;
    if (m_policy_graph) {
        // Compute SHA256 hash of file data using OpenSSL (same method as SecurityTap)
        unsigned char hash[SHA256_DIGEST_LENGTH];
        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (!context)
            return Error::from_string_literal("Failed to create EVP context for SHA256");

        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(context);
            return Error::from_string_literal("Failed to initialize SHA256");
        }

        if (EVP_DigestUpdate(context, file_data.data(), file_data.size()) != 1) {
            EVP_MD_CTX_free(context);
            return Error::from_string_literal("Failed to update SHA256");
        }

        unsigned int len = 0;
        if (EVP_DigestFinal_ex(context, hash, &len) != 1) {
            EVP_MD_CTX_free(context);
            return Error::from_string_literal("Failed to finalize SHA256");
        }

        EVP_MD_CTX_free(context);

        // Convert to hex string
        StringBuilder hex_builder;
        for (unsigned int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            hex_builder.appendff("{:02x}", hash[i]);

        file_hash = TRY(hex_builder.to_string());

        // Lookup cached verdict
        auto cached_verdict = m_policy_graph->lookup_sandbox_verdict(file_hash);
        if (!cached_verdict.is_error() && cached_verdict.value().has_value()) {
            auto const& verdict = cached_verdict.value().value();
            dbgln("Orchestrator: Cache HIT for '{}' - returning cached verdict", filename);

            // Convert PolicyGraph::SandboxVerdict back to SandboxResult
            SandboxResult result;
            result.threat_level = static_cast<SandboxResult::ThreatLevel>(verdict.threat_level);
            result.confidence = static_cast<float>(verdict.confidence) / 1000.0f;
            result.composite_score = static_cast<float>(verdict.composite_score) / 1000.0f;
            result.verdict_explanation = verdict.verdict_explanation;
            result.yara_score = static_cast<float>(verdict.yara_score) / 1000.0f;
            result.ml_score = static_cast<float>(verdict.ml_score) / 1000.0f;
            result.behavioral_score = static_cast<float>(verdict.behavioral_score) / 1000.0f;
            result.execution_time = MonotonicTime::now() - start_time;

            return result;
        }
        dbgln("Orchestrator: Cache MISS for '{}' - performing full analysis", filename);
    }

    SandboxResult result;
    result.verdict_explanation = TRY(String::formatted("Analyzing '{}'...", filename));

    // Stage 1: Tier 1 WASM sandbox (fast pre-analysis)
    if (m_config.enable_tier1_wasm && m_wasm_executor) {
        auto tier1_result = execute_tier1_wasm(file_data, filename);
        if (tier1_result.is_error()) {
            dbgln("Orchestrator: Tier 1 WASM execution failed: {}", tier1_result.error());
            // Continue to Tier 2 if Tier 1 fails
        } else {
            result = tier1_result.release_value();
            m_stats.tier1_executions++;

            // If Tier 1 verdict is conclusive (very clean or very malicious), skip Tier 2
            if (result.confidence > 0.9f) {
                dbgln_if(false, "Orchestrator: Tier 1 verdict conclusive (confidence: {}), skipping Tier 2", result.confidence);
                TRY(generate_verdict(result));
                result.execution_time = MonotonicTime::now() - start_time;
                return result;
            }
        }
    }

    // Stage 2: Tier 2 native sandbox (deep behavioral analysis)
    if (m_config.enable_tier2_native && m_behavioral_analyzer) {
        // Only run Tier 2 if file is suspicious
        if (result.composite_score > 0.3f || !m_config.enable_tier1_wasm) {
            auto tier2_result = execute_tier2_native(file_data, filename);
            if (tier2_result.is_error()) {
                dbgln("Orchestrator: Tier 2 native execution failed: {}", tier2_result.error());
                // Use Tier 1 result if available
                if (result.composite_score == 0.0f) {
                    return Error::from_string_literal("Both sandbox tiers failed");
                }
            } else {
                // Merge Tier 2 results into existing result
                auto tier2 = tier2_result.release_value();
                result.behavioral_score = tier2.behavioral_score;
                result.file_operations = tier2.file_operations;
                result.process_operations = tier2.process_operations;
                result.network_operations = tier2.network_operations;
                result.registry_operations = tier2.registry_operations;
                result.memory_operations = tier2.memory_operations;

                // Append behavioral detections
                for (auto const& behavior : tier2.detected_behaviors)
                    TRY(result.detected_behaviors.try_append(behavior));

                m_stats.tier2_executions++;
            }
        }
    }

    // Stage 3: Generate final verdict
    TRY(generate_verdict(result));

    result.execution_time = MonotonicTime::now() - start_time;

    // Update statistics
    if (result.is_malicious())
        m_stats.malicious_detected++;

    dbgln_if(false, "Orchestrator: Analysis complete - Threat level: {}, Confidence: {:.2f}, Time: {}ms",
        static_cast<int>(result.threat_level), result.confidence, result.execution_time.to_milliseconds());

    // Milestone 0.5 Phase 1d: Store verdict in cache
    if (m_policy_graph && !file_hash.is_empty()) {
        PolicyGraph::SandboxVerdict verdict;
        verdict.file_hash = file_hash;
        verdict.threat_level = static_cast<i32>(result.threat_level);
        verdict.confidence = static_cast<i32>(result.confidence * 1000.0f);
        verdict.composite_score = static_cast<i32>(result.composite_score * 1000.0f);
        verdict.verdict_explanation = result.verdict_explanation;
        verdict.yara_score = static_cast<i32>(result.yara_score * 1000.0f);
        verdict.ml_score = static_cast<i32>(result.ml_score * 1000.0f);
        verdict.behavioral_score = static_cast<i32>(result.behavioral_score * 1000.0f);
        verdict.analyzed_at = UnixDateTime::now();
        // Cache expires after 30 days
        verdict.expires_at = UnixDateTime::from_seconds_since_epoch(UnixDateTime::now().seconds_since_epoch() + (30 * 24 * 60 * 60));

        auto store_result = m_policy_graph->store_sandbox_verdict(verdict);
        if (store_result.is_error()) {
            dbgln("Orchestrator: Warning - failed to cache verdict: {}", store_result.error());
            // Continue anyway - caching is optional
        } else {
            dbgln_if(false, "Orchestrator: Verdict cached for future lookups");
        }
    }

    return result;
}

ErrorOr<SandboxResult> Orchestrator::analyze_file_with_features(
    ByteBuffer const& file_data,
    String const& filename,
    [[maybe_unused]] MalwareMLDetector::Features const& features)
{
    // TODO: Use pre-computed ML features to optimize analysis
    // For now, delegate to standard analysis
    return analyze_file(file_data, filename);
}

ErrorOr<SandboxResult> Orchestrator::execute_tier1_wasm(ByteBuffer const& file_data, String const& filename)
{
    dbgln_if(false, "Orchestrator: Executing Tier 1 WASM sandbox");

    VERIFY(m_wasm_executor);
    auto start_time = MonotonicTime::now();

    SandboxResult result;

    // Execute in WASM sandbox with timeout
    auto execution_result = m_wasm_executor->execute(file_data, filename, m_config.timeout);
    if (execution_result.is_error()) {
        if (execution_result.error().string_literal().contains("timeout"sv))
            m_stats.timeouts++;
        return execution_result.release_error();
    }

    auto wasm_result = execution_result.release_value();

    // Map WASM execution results to SandboxResult
    result.yara_score = wasm_result.yara_score;
    result.ml_score = wasm_result.ml_score;
    result.detected_behaviors = move(wasm_result.detected_behaviors);
    result.triggered_rules = move(wasm_result.triggered_rules);

    auto tier1_time = MonotonicTime::now() - start_time;
    if (m_stats.tier1_executions > 0) {
        m_stats.average_tier1_time = Duration::from_nanoseconds(
            (m_stats.average_tier1_time.to_nanoseconds() * (m_stats.tier1_executions - 1) + tier1_time.to_nanoseconds()) / m_stats.tier1_executions);
    }

    dbgln_if(false, "Orchestrator: Tier 1 complete in {}ms - YARA: {:.2f}, ML: {:.2f}",
        tier1_time.to_milliseconds(), result.yara_score, result.ml_score);

    return result;
}

ErrorOr<SandboxResult> Orchestrator::execute_tier2_native(ByteBuffer const& file_data, String const& filename)
{
    dbgln_if(false, "Orchestrator: Executing Tier 2 native sandbox");

    VERIFY(m_behavioral_analyzer);
    auto start_time = MonotonicTime::now();

    SandboxResult result;

    // Execute in native sandbox with syscall monitoring
    auto analysis_result = m_behavioral_analyzer->analyze(file_data, filename, m_config.timeout);
    if (analysis_result.is_error()) {
        if (analysis_result.error().string_literal().contains("timeout"sv))
            m_stats.timeouts++;
        return analysis_result.release_error();
    }

    auto behavioral_metrics = analysis_result.release_value();

    // Map behavioral metrics to SandboxResult
    result.behavioral_score = behavioral_metrics.threat_score;
    result.file_operations = behavioral_metrics.file_operations;
    result.process_operations = behavioral_metrics.process_operations;
    result.network_operations = behavioral_metrics.network_operations;
    result.registry_operations = behavioral_metrics.registry_operations;
    result.memory_operations = behavioral_metrics.memory_operations;
    result.detected_behaviors = move(behavioral_metrics.suspicious_behaviors);

    auto tier2_time = MonotonicTime::now() - start_time;
    if (m_stats.tier2_executions > 0) {
        m_stats.average_tier2_time = Duration::from_nanoseconds(
            (m_stats.average_tier2_time.to_nanoseconds() * (m_stats.tier2_executions - 1) + tier2_time.to_nanoseconds()) / m_stats.tier2_executions);
    }

    dbgln_if(false, "Orchestrator: Tier 2 complete in {}ms - Behavioral score: {:.2f}",
        tier2_time.to_milliseconds(), result.behavioral_score);

    return result;
}

ErrorOr<void> Orchestrator::generate_verdict(SandboxResult& result)
{
    VERIFY(m_verdict_engine);

    // Generate final verdict using multi-layer scoring
    auto verdict = TRY(m_verdict_engine->calculate_verdict(
        result.yara_score,
        result.ml_score,
        result.behavioral_score));

    result.composite_score = verdict.composite_score;
    result.confidence = verdict.confidence;
    result.threat_level = verdict.threat_level;
    result.verdict_explanation = move(verdict.explanation);

    dbgln_if(false, "Orchestrator: Verdict - Composite: {:.2f}, Confidence: {:.2f}, Level: {}",
        result.composite_score, result.confidence, static_cast<int>(result.threat_level));

    return {};
}

void Orchestrator::reset_statistics()
{
    m_stats = Statistics {};
    dbgln_if(false, "Orchestrator: Statistics reset");
}

void Orchestrator::update_config(SandboxConfig const& config)
{
    m_config = config;
    dbgln_if(false, "Orchestrator: Configuration updated");

    // Reinitialize components if needed
    if (auto result = initialize_components(); result.is_error()) {
        dbgln("Orchestrator: Warning - Failed to reinitialize components after config update: {}", result.error());
    }
}

}
