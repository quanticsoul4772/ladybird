/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibMain/Main.h>
#include <Services/Sentinel/Sandbox/ThreatReporter.h>

using namespace Sentinel::Sandbox;

// Helper function to create a SandboxResult for testing
static SandboxResult create_test_result(
    SandboxResult::ThreatLevel level,
    float yara_score,
    float ml_score,
    float behavioral_score,
    float composite_score,
    float confidence)
{
    SandboxResult result;
    result.threat_level = level;
    result.yara_score = yara_score;
    result.ml_score = ml_score;
    result.behavioral_score = behavioral_score;
    result.composite_score = composite_score;
    result.confidence = confidence;
    result.execution_time = Duration::from_milliseconds(250);
    return result;
}

// Test 1: Clean file report
static ErrorOr<void> test_clean_file_report()
{
    auto reporter = TRY(ThreatReporter::create());

    auto result = create_test_result(
        SandboxResult::ThreatLevel::Clean,
        0.1f, 0.05f, 0.0f, 0.05f, 0.95f);

    auto report = TRY(reporter->format_verdict(result, "safe_document.pdf"_string));

    // Verify report contains expected elements
    if (!report.contains("\xF0\x9F\x9F\xA2"sv)) { // 🟢
        return Error::from_string_literal("Clean report missing green emoji");
    }
    if (!report.contains("LOW THREAT"sv)) {
        return Error::from_string_literal("Clean report missing 'LOW THREAT'");
    }
    if (!report.contains("safe_document.pdf"sv)) {
        return Error::from_string_literal("Clean report missing filename");
    }
    if (!report.contains("Detection Summary"sv)) {
        return Error::from_string_literal("Clean report missing detection summary");
    }
    if (!report.contains("Technical Details"sv)) {
        return Error::from_string_literal("Clean report missing technical details");
    }

    outln("✓ Test 1: Clean file report - PASSED");
    return {};
}

// Test 2: Suspicious file report
static ErrorOr<void> test_suspicious_file_report()
{
    auto reporter = TRY(ThreatReporter::create());

    auto result = create_test_result(
        SandboxResult::ThreatLevel::Suspicious,
        0.2f, 0.4f, 0.5f, 0.35f, 0.7f);

    result.detected_behaviors.append("Hidden file logging detected"_string);
    result.file_operations = 25;

    auto report = TRY(reporter->format_verdict(result, "suspicious_script.sh"_string));

    // Verify report contains expected elements
    if (!report.contains("\xF0\x9F\x9F\xA1"sv)) { // 🟡
        return Error::from_string_literal("Suspicious report missing yellow emoji");
    }
    if (!report.contains("MEDIUM THREAT"sv)) {
        return Error::from_string_literal("Suspicious report missing 'MEDIUM THREAT'");
    }
    if (!report.contains("suspicious_script.sh"sv)) {
        return Error::from_string_literal("Suspicious report missing filename");
    }
    if (!report.contains("Review carefully before opening"sv)) {
        return Error::from_string_literal("Suspicious report missing warning");
    }
    if (!report.contains("Hidden file logging detected"sv)) {
        return Error::from_string_literal("Suspicious report missing detected behavior");
    }

    outln("✓ Test 2: Suspicious file report - PASSED");
    return {};
}

// Test 3: Malicious file report
static ErrorOr<void> test_malicious_file_report()
{
    auto reporter = TRY(ThreatReporter::create());

    auto result = create_test_result(
        SandboxResult::ThreatLevel::Malicious,
        0.8f, 0.7f, 0.65f, 0.73f, 0.85f);

    result.triggered_rules.append("WannaCry ransomware"_string);
    result.detected_behaviors.append("Ransomware: Rapid file encryption pattern detected"_string);
    result.detected_behaviors.append("Network: Attempted C2 beacon"_string);
    result.file_operations = 150;
    result.network_operations = 5;

    auto report = TRY(reporter->format_verdict(result, "suspicious_download.exe"_string));

    // Verify report contains expected elements
    if (!report.contains("\xF0\x9F\x9F\xA0"sv)) { // 🟠
        return Error::from_string_literal("Malicious report missing orange emoji");
    }
    if (!report.contains("HIGH THREAT"sv)) {
        return Error::from_string_literal("Malicious report missing 'HIGH THREAT'");
    }
    if (!report.contains("QUARANTINED"sv)) {
        return Error::from_string_literal("Malicious report missing quarantine warning");
    }
    if (!report.contains("Delete this file immediately"sv)) {
        return Error::from_string_literal("Malicious report missing deletion instruction");
    }
    if (!report.contains("WannaCry ransomware"sv)) {
        return Error::from_string_literal("Malicious report missing YARA rule");
    }
    if (!report.contains("Ransomware"sv)) {
        return Error::from_string_literal("Malicious report missing behavioral detection");
    }

    outln("✓ Test 3: Malicious file report - PASSED");
    return {};
}

// Test 4: Critical file report
static ErrorOr<void> test_critical_file_report()
{
    auto reporter = TRY(ThreatReporter::create());

    auto result = create_test_result(
        SandboxResult::ThreatLevel::Critical,
        0.95f, 0.9f, 0.85f, 0.91f, 0.95f);

    result.triggered_rules.append("Zeus banking trojan"_string);
    result.triggered_rules.append("Keylogger signature"_string);
    result.detected_behaviors.append("Keylogger: Keyboard input capture"_string);
    result.detected_behaviors.append("Privilege Escalation: Attempted setuid syscall"_string);
    result.detected_behaviors.append("Network: Data exfiltration to suspicious domain"_string);
    result.file_operations = 500;
    result.network_operations = 25;
    result.process_operations = 15;
    result.memory_operations = 100;

    auto report = TRY(reporter->format_verdict(result, "malware.bin"_string));

    // Verify report contains expected elements
    if (!report.contains("\xF0\x9F\x94\xB4"sv)) { // 🔴
        return Error::from_string_literal("Critical report missing red emoji");
    }
    if (!report.contains("CRITICAL THREAT"sv)) {
        return Error::from_string_literal("Critical report missing 'CRITICAL THREAT'");
    }
    if (!report.contains("SEVERE THREAT"sv)) {
        return Error::from_string_literal("Critical report missing severe threat warning");
    }
    if (!report.contains("malware.bin"sv)) {
        return Error::from_string_literal("Critical report missing filename");
    }

    outln("✓ Test 4: Critical file report - PASSED");
    return {};
}

// Test 5: Summary report format
static ErrorOr<void> test_summary_report()
{
    auto reporter = TRY(ThreatReporter::create());

    auto result = create_test_result(
        SandboxResult::ThreatLevel::Malicious,
        0.8f, 0.7f, 0.6f, 0.70f, 0.8f);

    auto summary = TRY(reporter->format_summary(result, "threat.exe"_string));

    // Verify summary is shorter and contains key info
    if (!summary.contains("\xF0\x9F\x9F\xA0"sv)) { // 🟠
        return Error::from_string_literal("Summary missing emoji");
    }
    if (!summary.contains("HIGH THREAT"sv)) {
        return Error::from_string_literal("Summary missing threat level");
    }
    if (!summary.contains("threat.exe"sv)) {
        return Error::from_string_literal("Summary missing filename");
    }
    if (!summary.contains("Score: 0.70/1.0"sv)) {
        return Error::from_string_literal("Summary missing score");
    }

    // Summary should be much shorter than full report
    if (summary.bytes_as_string_view().length() > 500) {
        return Error::from_string_literal("Summary report too long (should be concise)");
    }

    outln("✓ Test 5: Summary report format - PASSED");
    return {};
}

// Test 6: Statistics tracking
static ErrorOr<void> test_statistics_tracking()
{
    auto reporter = TRY(ThreatReporter::create());

    // Generate reports for different threat levels
    auto clean = create_test_result(SandboxResult::ThreatLevel::Clean, 0.1f, 0.05f, 0.0f, 0.05f, 0.95f);
    auto suspicious = create_test_result(SandboxResult::ThreatLevel::Suspicious, 0.2f, 0.4f, 0.3f, 0.35f, 0.7f);
    auto malicious = create_test_result(SandboxResult::ThreatLevel::Malicious, 0.8f, 0.7f, 0.6f, 0.73f, 0.85f);
    auto critical = create_test_result(SandboxResult::ThreatLevel::Critical, 0.95f, 0.9f, 0.85f, 0.91f, 0.95f);

    TRY(reporter->format_verdict(clean, "file1.txt"_string));
    TRY(reporter->format_verdict(suspicious, "file2.sh"_string));
    TRY(reporter->format_verdict(suspicious, "file3.sh"_string));
    TRY(reporter->format_verdict(malicious, "file4.exe"_string));
    TRY(reporter->format_verdict(critical, "file5.bin"_string));

    auto stats = reporter->get_statistics();

    if (stats.total_reports != 5) {
        return Error::from_string_literal("Incorrect total_reports count");
    }
    if (stats.clean_reports != 1) {
        return Error::from_string_literal("Incorrect clean_reports count");
    }
    if (stats.suspicious_reports != 2) {
        return Error::from_string_literal("Incorrect suspicious_reports count");
    }
    if (stats.malicious_reports != 1) {
        return Error::from_string_literal("Incorrect malicious_reports count");
    }
    if (stats.critical_reports != 1) {
        return Error::from_string_literal("Incorrect critical_reports count");
    }

    // Test reset
    reporter->reset_statistics();
    auto reset_stats = reporter->get_statistics();
    if (reset_stats.total_reports != 0) {
        return Error::from_string_literal("Statistics not reset properly");
    }

    outln("✓ Test 6: Statistics tracking - PASSED");
    return {};
}

// Test 7: Unicode emoji rendering
static ErrorOr<void> test_unicode_emoji_rendering()
{
    auto reporter = TRY(ThreatReporter::create());

    // Test all threat levels
    auto clean = create_test_result(SandboxResult::ThreatLevel::Clean, 0.1f, 0.05f, 0.0f, 0.05f, 0.95f);
    auto suspicious = create_test_result(SandboxResult::ThreatLevel::Suspicious, 0.2f, 0.4f, 0.3f, 0.35f, 0.7f);
    auto malicious = create_test_result(SandboxResult::ThreatLevel::Malicious, 0.8f, 0.7f, 0.6f, 0.73f, 0.85f);
    auto critical = create_test_result(SandboxResult::ThreatLevel::Critical, 0.95f, 0.9f, 0.85f, 0.91f, 0.95f);

    auto clean_report = TRY(reporter->format_verdict(clean, "test.txt"_string));
    auto suspicious_report = TRY(reporter->format_verdict(suspicious, "test.sh"_string));
    auto malicious_report = TRY(reporter->format_verdict(malicious, "test.exe"_string));
    auto critical_report = TRY(reporter->format_verdict(critical, "test.bin"_string));

    // Verify correct emojis are present
    if (!clean_report.contains("\xF0\x9F\x9F\xA2"sv)) { // 🟢
        return Error::from_string_literal("Clean report missing green circle emoji");
    }
    if (!suspicious_report.contains("\xF0\x9F\x9F\xA1"sv)) { // 🟡
        return Error::from_string_literal("Suspicious report missing yellow circle emoji");
    }
    if (!malicious_report.contains("\xF0\x9F\x9F\xA0"sv)) { // 🟠
        return Error::from_string_literal("Malicious report missing orange circle emoji");
    }
    if (!critical_report.contains("\xF0\x9F\x94\xB4"sv)) { // 🔴
        return Error::from_string_literal("Critical report missing red circle emoji");
    }

    // Verify action emojis
    if (!clean_report.contains("\xE2\x9C\x85"sv)) { // ✅
        return Error::from_string_literal("Clean report missing checkmark emoji");
    }
    if (!malicious_report.contains("\xE2\x9B\x94"sv)) { // ⛔
        return Error::from_string_literal("Malicious report missing no entry emoji");
    }

    outln("✓ Test 7: Unicode emoji rendering - PASSED");
    return {};
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    outln("=== ThreatReporter Unit Tests ===\n");

    TRY(test_clean_file_report());
    TRY(test_suspicious_file_report());
    TRY(test_malicious_file_report());
    TRY(test_critical_file_report());
    TRY(test_summary_report());
    TRY(test_statistics_tracking());
    TRY(test_unicode_emoji_rendering());

    outln("\n=== All Tests PASSED ===");
    outln("Total: 7 tests");

    return 0;
}
