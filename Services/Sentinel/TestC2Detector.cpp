/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "C2Detector.h"
#include <LibCore/System.h>
#include <LibMain/Main.h>

static void test_beaconing_regular_intervals()
{
    dbgln("\n=== Test: Beaconing - Regular Intervals ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // Simulate regular beaconing (every 60 seconds)
    Vector<float> intervals = { 60.0f, 59.8f, 60.2f, 60.1f, 59.9f };

    auto result = detector->analyze_beaconing(intervals);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Intervals: {:.1f}, {:.1f}, {:.1f}, {:.1f}, {:.1f}",
        intervals[0], intervals[1], intervals[2], intervals[3], intervals[4]);
    dbgln("Coefficient of Variation: {:.4f}", analysis.interval_regularity);
    dbgln("Request Count: {}", analysis.request_count);
    dbgln("Is Beaconing: {}", analysis.is_beaconing ? "YES" : "NO");
    dbgln("Confidence: {:.2f}", analysis.confidence);
    dbgln("Explanation: {}", analysis.explanation);

    if (analysis.is_beaconing && analysis.interval_regularity < 0.01f) {
        dbgln("✓ PASS: Highly regular intervals detected as beaconing");
    } else {
        dbgln("✗ FAIL: Expected beaconing detection");
    }
}

static void test_beaconing_irregular_intervals()
{
    dbgln("\n=== Test: Beaconing - Irregular Intervals ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // Simulate normal browsing (irregular intervals)
    Vector<float> intervals = { 5.0f, 45.0f, 2.0f, 120.0f, 15.0f, 90.0f };

    auto result = detector->analyze_beaconing(intervals);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Coefficient of Variation: {:.4f}", analysis.interval_regularity);
    dbgln("Is Beaconing: {}", analysis.is_beaconing ? "YES" : "NO");
    dbgln("Confidence: {:.2f}", analysis.confidence);
    dbgln("Explanation: {}", analysis.explanation);

    if (!analysis.is_beaconing) {
        dbgln("✓ PASS: Irregular intervals not flagged as beaconing");
    } else {
        dbgln("✗ FAIL: Expected no beaconing detection");
    }
}

static void test_beaconing_insufficient_data()
{
    dbgln("\n=== Test: Beaconing - Insufficient Data ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // Only 3 requests (need at least 5)
    Vector<float> intervals = { 60.0f, 60.0f, 60.0f };

    auto result = detector->analyze_beaconing(intervals);
    if (result.is_error()) {
        dbgln("Error (expected): {}", result.error());
        dbgln("✓ PASS: Correctly rejected insufficient data");
    } else {
        dbgln("✗ FAIL: Should have returned error for insufficient data");
    }
}

static void test_exfiltration_high_upload_ratio()
{
    dbgln("\n=== Test: Exfiltration - High Upload Ratio ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // 50 MB sent, 5 MB received (91% upload ratio)
    u64 bytes_sent = 50 * 1024 * 1024;
    u64 bytes_received = 5 * 1024 * 1024;
    auto domain = "evil-exfil-server.com"sv;

    auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Bytes Sent: {} MB", bytes_sent / (1024 * 1024));
    dbgln("Bytes Received: {} MB", bytes_received / (1024 * 1024));
    dbgln("Upload Ratio: {:.2f}%", analysis.upload_ratio * 100.0f);
    dbgln("Is Exfiltration: {}", analysis.is_exfiltration ? "YES" : "NO");
    dbgln("Confidence: {:.2f}", analysis.confidence);
    dbgln("Explanation: {}", analysis.explanation);

    if (analysis.is_exfiltration && analysis.upload_ratio > 0.9f) {
        dbgln("✓ PASS: High upload ratio detected as exfiltration");
    } else {
        dbgln("✗ FAIL: Expected exfiltration detection");
    }
}

static void test_exfiltration_normal_ratio()
{
    dbgln("\n=== Test: Exfiltration - Normal Upload Ratio ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // 1 MB sent, 10 MB received (9% upload ratio - normal browsing)
    u64 bytes_sent = 1 * 1024 * 1024;
    u64 bytes_received = 10 * 1024 * 1024;
    auto domain = "example.com"sv;

    auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Upload Ratio: {:.2f}%", analysis.upload_ratio * 100.0f);
    dbgln("Is Exfiltration: {}", analysis.is_exfiltration ? "YES" : "NO");
    dbgln("Explanation: {}", analysis.explanation);

    if (!analysis.is_exfiltration) {
        dbgln("✓ PASS: Normal upload ratio not flagged");
    } else {
        dbgln("✗ FAIL: Expected no exfiltration detection");
    }
}

static void test_exfiltration_known_upload_service()
{
    dbgln("\n=== Test: Exfiltration - Known Upload Service (Whitelist) ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // High upload ratio but to Google Drive (whitelisted)
    u64 bytes_sent = 50 * 1024 * 1024;
    u64 bytes_received = 1 * 1024 * 1024;
    auto domain = "drive.google.com"sv;

    auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Domain: {}", domain);
    dbgln("Upload Ratio: {:.2f}%", analysis.upload_ratio * 100.0f);
    dbgln("Is Exfiltration: {}", analysis.is_exfiltration ? "YES" : "NO");
    dbgln("Explanation: {}", analysis.explanation);

    if (!analysis.is_exfiltration) {
        dbgln("✓ PASS: Known upload service whitelisted");
    } else {
        dbgln("✗ FAIL: Should not flag known upload service");
    }
}

static void test_exfiltration_small_volume()
{
    dbgln("\n=== Test: Exfiltration - Small Volume (Below Threshold) ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // High upload ratio but small volume (only 1 MB)
    u64 bytes_sent = 1 * 1024 * 1024;
    u64 bytes_received = 100 * 1024;  // 100 KB
    auto domain = "test.com"sv;

    auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
    if (result.is_error()) {
        dbgln("ERROR: {}", result.error());
        return;
    }

    auto analysis = result.value();
    dbgln("Bytes Sent: {} MB", bytes_sent / (1024 * 1024));
    dbgln("Upload Ratio: {:.2f}%", analysis.upload_ratio * 100.0f);
    dbgln("Is Exfiltration: {}", analysis.is_exfiltration ? "YES" : "NO");
    dbgln("Explanation: {}", analysis.explanation);

    if (!analysis.is_exfiltration) {
        dbgln("✓ PASS: Small volume not flagged despite high ratio");
    } else {
        dbgln("✗ FAIL: Should not flag small volumes");
    }
}

static void test_statistical_accuracy()
{
    dbgln("\n=== Test: Statistical Accuracy ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // Test precise C2 beaconing (30 second intervals)
    Vector<float> perfect_intervals = { 30.0f, 30.0f, 30.0f, 30.0f, 30.0f };
    auto result1 = detector->analyze_beaconing(perfect_intervals);
    if (!result1.is_error()) {
        auto analysis = result1.value();
        dbgln("Perfect intervals (all 30.0s):");
        dbgln("  CV: {:.6f} (should be ~0.0)", analysis.interval_regularity);
        dbgln("  Mean: 30.0s");
        dbgln("  Is Beaconing: {}", analysis.is_beaconing ? "YES" : "NO");

        if (analysis.interval_regularity < 0.001f && analysis.is_beaconing) {
            dbgln("  ✓ PASS: Perfect regularity detected");
        } else {
            dbgln("  ✗ FAIL: Expected near-zero CV");
        }
    }

    // Test moderate variation
    Vector<float> moderate_intervals = { 30.0f, 32.0f, 28.0f, 31.0f, 29.0f };
    auto result2 = detector->analyze_beaconing(moderate_intervals);
    if (!result2.is_error()) {
        auto analysis = result2.value();
        dbgln("\nModerate variation (30±2s):");
        dbgln("  CV: {:.4f}", analysis.interval_regularity);
        dbgln("  Is Beaconing: {}", analysis.is_beaconing ? "YES" : "NO");

        if (analysis.interval_regularity < 0.2f) {
            dbgln("  ✓ PASS: Moderate regularity detected");
        }
    }
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    dbgln("========================================");
    dbgln("  C2Detector Unit Tests");
    dbgln("  Milestone 0.4 Phase 6");
    dbgln("========================================");

    test_beaconing_regular_intervals();
    test_beaconing_irregular_intervals();
    test_beaconing_insufficient_data();
    test_exfiltration_high_upload_ratio();
    test_exfiltration_normal_ratio();
    test_exfiltration_known_upload_service();
    test_exfiltration_small_volume();
    test_statistical_accuracy();

    dbgln("\n========================================");
    dbgln("  Tests Complete");
    dbgln("========================================");

    return 0;
}
