/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NonnullOwnPtr.h>
#include <LibTest/TestCase.h>

#include "Sandbox/Orchestrator.h"
#include "Sandbox/ThreatReporter.h"
#include "Sandbox/VerdictEngine.h"

using namespace Sentinel::Sandbox;

// ============================================================================
// Integration Tests for Orchestrator Pipeline
// Tests the complete analysis flow: YARA → ML → Behavioral → VerdictEngine → ThreatReporter
// ============================================================================

// Helper function to create test file data
static ByteBuffer create_test_file(StringView content)
{
    ByteBuffer buffer;
    MUST(buffer.try_append(content.bytes()));
    return buffer;
}

// ============================================================================
// Test 1: Orchestrator Creation
// ============================================================================

TEST_CASE(test_orchestrator_creation)
{
    // Test: Orchestrator::create() succeeds with default config
    auto orchestrator_result = Orchestrator::create();

    if (orchestrator_result.is_error()) {
        FAIL(ByteString::formatted("Failed to create Orchestrator: {}",
            orchestrator_result.error()));
    }

    auto orchestrator = orchestrator_result.release_value();
    EXPECT(orchestrator != nullptr);

    // Verify default config
    auto const& config = orchestrator->config();
    EXPECT(config.enable_tier1_wasm == true);
    EXPECT(config.enable_tier2_native == true);
    EXPECT(config.timeout == Duration::from_seconds(5));
}

TEST_CASE(test_orchestrator_creation_with_custom_config)
{
    // Test: Orchestrator respects custom configuration
    SandboxConfig config;
    config.use_mock_for_testing = true;
    config.timeout = Duration::from_seconds(10);
    config.enable_tier1_wasm = false;
    config.enable_tier2_native = true;

    auto orchestrator = MUST(Orchestrator::create(config));
    EXPECT(orchestrator != nullptr);

    auto const& stored_config = orchestrator->config();
    EXPECT(stored_config.timeout == Duration::from_seconds(10));
    EXPECT(stored_config.enable_tier1_wasm == false);
    EXPECT(stored_config.enable_tier2_native == true);
}

// ============================================================================
// Test 2: Clean File Pipeline
// ============================================================================

TEST_CASE(test_clean_file_pipeline)
{
    // Test: Clean file should be classified as Clean threat level
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create benign file - simple text
    auto clean_file = create_test_file("Hello World\nThis is a safe document.\n"sv);

    // Analyze file
    auto result = MUST(orchestrator->analyze_file(clean_file, "document.txt"_string));

    // Verify threat level is Clean
    EXPECT(result.threat_level == SandboxResult::ThreatLevel::Clean);

    // Verify composite score is low
    EXPECT(result.composite_score < 0.3f);

    // Verify confidence exists
    EXPECT(result.confidence >= 0.0f && result.confidence <= 1.0f);

    // Verify verdict explanation exists
    EXPECT(!result.verdict_explanation.is_empty());
    EXPECT(result.verdict_explanation.contains("clean"sv));

    // Verify statistics
    auto stats = orchestrator->get_statistics();
    EXPECT(stats.total_files_analyzed == 1);
    EXPECT(stats.malicious_detected == 0);
}

// ============================================================================
// Test 3: Suspicious File Pipeline
// ============================================================================

TEST_CASE(test_suspicious_file_pipeline)
{
    // Test: File with some suspicious indicators should be classified as Suspicious
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file with some suspicious keywords (moderate threat)
    auto suspicious_file = create_test_file(
        "#!/bin/bash\n"
        "echo 'Script running'\n"
        "chmod +x file.sh\n"
        "curl http://example.com/data\n"
        "rm -f temp.log\n"sv
    );

    // Analyze file
    auto result = MUST(orchestrator->analyze_file(suspicious_file, "script.sh"_string));

    // Verify threat level is at least Suspicious
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::Suspicious);

    // Verify composite score is moderate
    EXPECT(result.composite_score >= 0.3f);
    EXPECT(result.composite_score < 0.8f);

    // Verify individual scores
    EXPECT(result.yara_score >= 0.0f && result.yara_score <= 1.0f);
    EXPECT(result.ml_score >= 0.0f && result.ml_score <= 1.0f);
    EXPECT(result.behavioral_score >= 0.0f && result.behavioral_score <= 1.0f);

    // Verify verdict explanation mentions suspicious behavior
    EXPECT(result.verdict_explanation.contains("suspicious"sv) ||
           result.verdict_explanation.contains("Suspicious"sv));
}

// ============================================================================
// Test 4: Malicious File Pipeline
// ============================================================================

TEST_CASE(test_malicious_file_pipeline)
{
    // Test: File with multiple malware indicators should be classified as Malicious
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file with malware-like patterns
    auto malicious_file = create_test_file(
        "#!/bin/bash\n"
        "# Simulated malware patterns\n"
        "ptrace attach process\n"
        "setuid root escalation\n"
        "socket connect evil.com\n"
        "fork bomb loop\n"
        "exec payload shellcode\n"
        "memcpy buffer overflow\n"sv
    );

    // Analyze file
    auto result = MUST(orchestrator->analyze_file(malicious_file, "malware.bin"_string));

    // Verify threat level is at least Malicious
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::Malicious);

    // Verify composite score is high
    EXPECT(result.composite_score >= 0.6f);

    // Verify verdict explanation mentions malicious behavior
    EXPECT(result.verdict_explanation.contains("malicious"sv) ||
           result.verdict_explanation.contains("Malicious"sv) ||
           result.verdict_explanation.contains("CRITICAL"sv));

    // Verify statistics tracked malicious detection
    auto stats = orchestrator->get_statistics();
    EXPECT(stats.total_files_analyzed == 1);
    EXPECT(stats.malicious_detected == 1);
}

// ============================================================================
// Test 5: Critical File Pipeline
// ============================================================================

TEST_CASE(test_critical_file_pipeline)
{
    // Test: File with severe malware indicators should be classified as Critical
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file with severe malware patterns
    auto critical_file = create_test_file(
        "MALWARE CRITICAL THREAT\n"
        "ptrace attach debugger\n"
        "setuid root privilege escalation\n"
        "socket connect command-and-control\n"
        "fork process injection\n"
        "exec shellcode payload\n"
        "memcpy heap spray\n"
        "syscall keylogger\n"
        "network data exfiltration\n"
        "crypto ransomware encryption\n"
        "persistence autostart\n"sv
    );

    // Analyze file
    auto result = MUST(orchestrator->analyze_file(critical_file, "threat.exe"_string));

    // Verify threat level is Critical
    EXPECT(result.threat_level == SandboxResult::ThreatLevel::Critical);

    // Verify composite score is very high
    EXPECT(result.composite_score >= 0.8f);

    // Verify confidence is high (all detectors should agree)
    EXPECT(result.confidence >= 0.7f);

    // Verify verdict explanation mentions critical threat
    EXPECT(result.verdict_explanation.contains("CRITICAL"sv));
}

// ============================================================================
// Test 6: VerdictEngine Integration
// ============================================================================

TEST_CASE(test_verdict_engine_integration)
{
    // Test: VerdictEngine correctly combines scores with proper weights
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file that will trigger moderate scores
    auto test_file = create_test_file("socket connect fork exec"sv);

    // Analyze file
    auto result = MUST(orchestrator->analyze_file(test_file, "test.bin"_string));

    // Verify weighted scoring (YARA 40%, ML 35%, Behavioral 25%)
    float expected_composite = (result.yara_score * 0.40f) +
                              (result.ml_score * 0.35f) +
                              (result.behavioral_score * 0.25f);

    // Allow small floating point error
    EXPECT_APPROXIMATE(result.composite_score, expected_composite);

    // Verify threat level classification thresholds
    if (result.composite_score < 0.3f) {
        EXPECT(result.threat_level == SandboxResult::ThreatLevel::Clean);
    } else if (result.composite_score < 0.6f) {
        EXPECT(result.threat_level == SandboxResult::ThreatLevel::Suspicious);
    } else if (result.composite_score < 0.8f) {
        EXPECT(result.threat_level == SandboxResult::ThreatLevel::Malicious);
    } else {
        EXPECT(result.threat_level == SandboxResult::ThreatLevel::Critical);
    }

    // Verify confidence calculation
    EXPECT(result.confidence >= 0.0f && result.confidence <= 1.0f);

    // Verify verdict explanation generation
    EXPECT(!result.verdict_explanation.is_empty());
}

// ============================================================================
// Test 7: ThreatReporter Integration
// ============================================================================

TEST_CASE(test_threat_reporter_integration)
{
    // Test: ThreatReporter formats verdict correctly
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));
    auto reporter = MUST(ThreatReporter::create());

    // Create and analyze malicious file
    auto malicious_file = create_test_file("ptrace setuid socket exec fork"sv);
    auto result = MUST(orchestrator->analyze_file(malicious_file, "threat.bin"_string));

    // Generate full report
    auto full_report = MUST(reporter->format_verdict(result, "threat.bin"_string));

    // Verify report contains key sections
    EXPECT(full_report.contains("Detection Summary"sv));
    EXPECT(full_report.contains("Recommendation"sv));
    EXPECT(full_report.contains("Technical Details"sv));

    // Verify report contains threat level
    EXPECT(full_report.contains("THREAT"sv));

    // Verify report contains filename
    EXPECT(full_report.contains("threat.bin"sv));

    // Verify report contains emoji severity indicator
    // At minimum should have one of the threat emojis
    bool has_emoji = full_report.contains("\xF0\x9F\x9F\xA2"sv) ||  // 🟢
                     full_report.contains("\xF0\x9F\x9F\xA1"sv) ||  // 🟡
                     full_report.contains("\xF0\x9F\x9F\xA0"sv) ||  // 🟠
                     full_report.contains("\xF0\x9F\x94\xB4"sv);    // 🔴
    EXPECT(has_emoji);

    // Generate summary report
    auto summary = MUST(reporter->format_summary(result, "threat.bin"_string));

    // Verify summary is shorter than full report
    EXPECT(summary.bytes_as_string_view().length() < full_report.bytes_as_string_view().length());

    // Verify summary contains essential info
    EXPECT(summary.contains("threat.bin"sv));
    EXPECT(summary.contains("Score:"sv));
}

// ============================================================================
// Test 8: PolicyGraph Cache Hit
// ============================================================================

TEST_CASE(test_policy_graph_cache_hit)
{
    // Test: Second analysis of same file should hit cache
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create test file
    auto test_file = create_test_file("Test content for cache"sv);

    // First analysis - cache miss
    auto result1 = MUST(orchestrator->analyze_file(test_file, "cached_file.txt"_string));

    // Second analysis of same content - should be faster (cache hit)
    auto start_time = MonotonicTime::now();
    auto result2 = MUST(orchestrator->analyze_file(test_file, "cached_file.txt"_string));
    auto duration = MonotonicTime::now() - start_time;

    // Verify results are consistent
    EXPECT(result1.threat_level == result2.threat_level);
    EXPECT_APPROXIMATE(result1.composite_score, result2.composite_score);

    // Cache hit should be very fast (< 10ms typically)
    // Note: In CI environments this might be slower, so we allow generous margin
    EXPECT(duration < Duration::from_milliseconds(100));
}

// ============================================================================
// Test 9: PolicyGraph Cache Miss
// ============================================================================

TEST_CASE(test_policy_graph_cache_miss)
{
    // Test: Different files should result in cache miss
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Analyze first file
    auto file1 = create_test_file("First file content"sv);
    auto result1 = MUST(orchestrator->analyze_file(file1, "file1.txt"_string));

    // Analyze different file - cache miss
    auto file2 = create_test_file("Second file content - different"sv);
    auto result2 = MUST(orchestrator->analyze_file(file2, "file2.txt"_string));

    // Results can differ based on content
    // Just verify both analyses completed successfully
    EXPECT(result1.composite_score >= 0.0f && result1.composite_score <= 1.0f);
    EXPECT(result2.composite_score >= 0.0f && result2.composite_score <= 1.0f);

    // Verify statistics tracked both files
    auto stats = orchestrator->get_statistics();
    EXPECT(stats.total_files_analyzed == 2);
}

// ============================================================================
// Test 10: Statistics Tracking
// ============================================================================

TEST_CASE(test_statistics_tracking)
{
    // Test: Orchestrator tracks statistics correctly
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Initial statistics should be zero
    auto stats_initial = orchestrator->get_statistics();
    EXPECT(stats_initial.total_files_analyzed == 0);
    EXPECT(stats_initial.malicious_detected == 0);

    // Analyze clean file
    auto clean = create_test_file("Clean document"sv);
    MUST(orchestrator->analyze_file(clean, "clean.txt"_string));

    // Analyze suspicious file
    auto suspicious = create_test_file("chmod curl wget"sv);
    MUST(orchestrator->analyze_file(suspicious, "suspicious.sh"_string));

    // Analyze malicious file
    auto malicious = create_test_file("ptrace setuid socket exec fork"sv);
    MUST(orchestrator->analyze_file(malicious, "malicious.bin"_string));

    // Verify statistics
    auto stats = orchestrator->get_statistics();
    EXPECT(stats.total_files_analyzed == 3);

    // At least one should be detected as malicious
    EXPECT(stats.malicious_detected >= 1);

    // Test reset
    orchestrator->reset_statistics();
    auto stats_reset = orchestrator->get_statistics();
    EXPECT(stats_reset.total_files_analyzed == 0);
    EXPECT(stats_reset.malicious_detected == 0);
}

// ============================================================================
// Test 11: Execution Time Tracking
// ============================================================================

TEST_CASE(test_execution_time_tracking)
{
    // Test: Orchestrator tracks execution time
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    auto test_file = create_test_file("Test content"sv);
    auto result = MUST(orchestrator->analyze_file(test_file, "test.txt"_string));

    // Verify execution time was recorded
    EXPECT(result.execution_time.to_milliseconds() >= 0);

    // Execution time should be reasonable (< 5 seconds in mock mode)
    EXPECT(result.execution_time < Duration::from_seconds(5));
}

// ============================================================================
// Test 12: Config Update
// ============================================================================

TEST_CASE(test_config_update)
{
    // Test: Orchestrator config can be updated
    SandboxConfig initial_config;
    initial_config.use_mock_for_testing = true;
    initial_config.timeout = Duration::from_seconds(5);

    auto orchestrator = MUST(Orchestrator::create(initial_config));

    // Update config
    SandboxConfig new_config;
    new_config.use_mock_for_testing = true;
    new_config.timeout = Duration::from_seconds(10);
    new_config.enable_tier1_wasm = false;

    orchestrator->update_config(new_config);

    // Verify config was updated
    auto const& stored_config = orchestrator->config();
    EXPECT(stored_config.timeout == Duration::from_seconds(10));
    EXPECT(stored_config.enable_tier1_wasm == false);
}

// ============================================================================
// Test 13: Multiple Files Sequential
// ============================================================================

TEST_CASE(test_multiple_files_sequential)
{
    // Test: Orchestrator can analyze multiple files sequentially
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Analyze 5 different files
    for (int i = 0; i < 5; i++) {
        auto file_content = ByteString::formatted("File {} content", i);
        auto file = create_test_file(file_content.view());
        auto filename = MUST(String::formatted("file{}.txt", i));

        auto result = MUST(orchestrator->analyze_file(file, filename));

        // Verify each analysis completes successfully
        EXPECT(result.composite_score >= 0.0f && result.composite_score <= 1.0f);
        EXPECT(!result.verdict_explanation.is_empty());
    }

    // Verify statistics
    auto stats = orchestrator->get_statistics();
    EXPECT(stats.total_files_analyzed == 5);
}

// ============================================================================
// Test 14: Empty File Handling
// ============================================================================

TEST_CASE(test_empty_file_handling)
{
    // Test: Orchestrator handles empty files gracefully
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create empty file
    ByteBuffer empty_file;

    // Analyze empty file - should not crash
    auto result = MUST(orchestrator->analyze_file(empty_file, "empty.txt"_string));

    // Empty file should be classified as Clean
    EXPECT(result.threat_level == SandboxResult::ThreatLevel::Clean);
    EXPECT(result.composite_score < 0.3f);
}

// ============================================================================
// Test 15: Large File Content
// ============================================================================

TEST_CASE(test_large_file_content)
{
    // Test: Orchestrator handles larger files
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file with repeated content (10KB)
    StringBuilder builder;
    for (int i = 0; i < 100; i++) {
        builder.append("This is line "sv);
        builder.appendff("{}", i);
        builder.append(" of the test file.\n"sv);
    }
    auto content = MUST(builder.to_string());
    auto large_file = create_test_file(content.bytes_as_string_view());

    // Analyze large file
    auto result = MUST(orchestrator->analyze_file(large_file, "large.txt"_string));

    // Verify analysis completed
    EXPECT(result.composite_score >= 0.0f && result.composite_score <= 1.0f);
    EXPECT(!result.verdict_explanation.is_empty());
}

// ============================================================================
// Test 16: Real-world Scenario - PDF Document
// ============================================================================

TEST_CASE(test_real_world_pdf_document)
{
    // Test: Simulated clean PDF document
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Simulate PDF header and content
    auto pdf_content = create_test_file(
        "%PDF-1.4\n"
        "% Document metadata\n"
        "1 0 obj\n"
        "<< /Type /Catalog /Pages 2 0 R >>\n"
        "endobj\n"sv
        "2 0 obj\n"
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n"sv
        "endobj\n"sv
    );

    auto result = MUST(orchestrator->analyze_file(pdf_content, "document.pdf"_string));

    // Clean PDF should be classified as Clean
    EXPECT(result.threat_level == SandboxResult::ThreatLevel::Clean);
    EXPECT(result.composite_score < 0.3f);
}

// ============================================================================
// Test 17: Real-world Scenario - Shell Script
// ============================================================================

TEST_CASE(test_real_world_shell_script)
{
    // Test: Legitimate shell script with network operations
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    auto script_content = create_test_file(
        "#!/bin/bash\n"
        "# Backup script\n"
        "DATE=$(date +%Y-%m-%d)\n"
        "tar -czf backup-$DATE.tar.gz /home/user/documents\n"
        "curl -X POST -F file=@backup-$DATE.tar.gz https://backup.example.com/upload\n"sv
    );

    auto result = MUST(orchestrator->analyze_file(script_content, "backup.sh"_string));

    // Script with network operations might be Suspicious or Clean
    EXPECT(result.threat_level <= SandboxResult::ThreatLevel::Suspicious);
    EXPECT(result.composite_score < 0.6f);
}

// ============================================================================
// Test 18: Real-world Scenario - Ransomware
// ============================================================================

TEST_CASE(test_real_world_ransomware)
{
    // Test: Simulated ransomware with encryption patterns
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    auto ransomware_content = create_test_file(
        "// Ransomware simulation\n"
        "encrypt_files();\n"
        "socket connect command-server.evil.com\n"
        "send_encryption_key();\n"
        "display_ransom_note();\n"
        "delete_shadow_copies();\n"
        "disable_recovery_mode();\n"
        "lock_user_files();\n"
        "crypto aes encryption loop\n"
        "persistence registry autostart\n"sv
    );

    auto result = MUST(orchestrator->analyze_file(ransomware_content, "ransomware.exe"_string));

    // Ransomware should be detected as Malicious or Critical
    EXPECT(result.threat_level >= SandboxResult::ThreatLevel::Malicious);
    EXPECT(result.composite_score >= 0.6f);
    EXPECT(result.verdict_explanation.contains("malicious"sv) ||
           result.verdict_explanation.contains("CRITICAL"sv));
}

// ============================================================================
// Test 19: Confidence Scoring High Agreement
// ============================================================================

TEST_CASE(test_confidence_high_agreement)
{
    // Test: When all detectors agree, confidence should be high
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create obviously malicious file (all detectors should agree)
    auto malicious = create_test_file(
        "MALWARE ptrace setuid socket exec fork keylogger ransomware "
        "privilege escalation command-and-control data exfiltration "
        "process injection heap spray shellcode payload crypto encryption"sv
    );

    auto result = MUST(orchestrator->analyze_file(malicious, "obvious_malware.bin"_string));

    // High scores should lead to high confidence
    if (result.yara_score > 0.7f && result.ml_score > 0.7f && result.behavioral_score > 0.7f) {
        EXPECT(result.confidence >= 0.7f);
    }
}

// ============================================================================
// Test 20: Confidence Scoring Disagreement
// ============================================================================

TEST_CASE(test_confidence_low_disagreement)
{
    // Test: When detectors disagree, confidence should be lower
    SandboxConfig config;
    config.use_mock_for_testing = true;
    auto orchestrator = MUST(Orchestrator::create(config));

    // Create file that might cause disagreement
    // (contains some keywords but mostly benign)
    auto mixed_file = create_test_file(
        "# Configuration file\n"
        "server_socket = 8080\n"
        "process_timeout = 30\n"
        "enable_fork = true\n"
        "# End of config\n"sv
    );

    auto result = MUST(orchestrator->analyze_file(mixed_file, "config.txt"_string));

    // Just verify confidence is in valid range
    EXPECT(result.confidence >= 0.0f && result.confidence <= 1.0f);
}
