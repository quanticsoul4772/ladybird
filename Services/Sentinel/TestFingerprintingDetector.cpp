/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FingerprintingDetector.h"
#include <AK/NonnullOwnPtr.h>
#include <LibMain/Main.h>
#include <stdio.h>

using namespace Sentinel;

static int tests_passed = 0;
static int tests_failed = 0;

static void log_pass(char const* test_name)
{
    printf("✅ PASSED: %s\n", test_name);
    tests_passed++;
}

static void log_fail(char const* test_name, char const* reason)
{
    printf("❌ FAILED: %s - %s\n", test_name, reason);
    tests_failed++;
}

static void print_section(char const* title)
{
    printf("\n=== %s ===\n", title);
}

// Test 1: No fingerprinting activity
static void test_no_activity()
{
    print_section("Test 1: No Fingerprinting Activity");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("No activity", "Failed to create detector");
        return;
    }

    auto score = detector.value()->calculate_score();
    printf("  Score: %.2f, Confidence: %.2f\n", score.aggressiveness_score, score.confidence);
    printf("  Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.aggressiveness_score == 0.0f && score.total_api_calls == 0)
        log_pass("No fingerprinting activity detected");
    else
        log_fail("No activity", "Expected score 0.0");
}

// Test 2: Canvas fingerprinting
static void test_canvas_fingerprinting()
{
    print_section("Test 2: Canvas Fingerprinting");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Canvas fingerprinting", "Failed to create detector");
        return;
    }

    // Simulate canvas fingerprinting: render text, read pixels
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "toDataURL"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "getImageData"sv, false);

    auto score = detector.value()->calculate_score();
    printf("  Canvas fingerprinting detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Confidence: %.2f\n", score.confidence);
    printf("    Uses Canvas: %s\n", score.uses_canvas ? "yes" : "no");
    printf("    Canvas calls: %zu\n", score.canvas_calls);
    printf("    Techniques used: %zu\n", score.techniques_used);
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.uses_canvas && score.aggressiveness_score > 0.5f)
        log_pass("Canvas fingerprinting detected");
    else
        log_fail("Canvas fingerprinting", "Did not detect canvas fingerprinting");
}

// Test 3: WebGL fingerprinting
static void test_webgl_fingerprinting()
{
    print_section("Test 3: WebGL Fingerprinting");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("WebGL fingerprinting", "Failed to create detector");
        return;
    }

    // Simulate WebGL fingerprinting: query renderer info
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::WebGL, "getParameter"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::WebGL, "getSupportedExtensions"sv, false);

    auto score = detector.value()->calculate_score();
    printf("  WebGL fingerprinting detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Uses WebGL: %s\n", score.uses_webgl ? "yes" : "no");
    printf("    WebGL calls: %zu\n", score.webgl_calls);
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.uses_webgl && score.aggressiveness_score > 0.4f)
        log_pass("WebGL fingerprinting detected");
    else
        log_fail("WebGL fingerprinting", "Did not detect WebGL fingerprinting");
}

// Test 4: Audio fingerprinting
static void test_audio_fingerprinting()
{
    print_section("Test 4: Audio Fingerprinting");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Audio fingerprinting", "Failed to create detector");
        return;
    }

    // Simulate audio fingerprinting: create oscillator, analyze
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::AudioContext, "createOscillator"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::AudioContext, "createAnalyser"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::AudioContext, "getChannelData"sv, false);

    auto score = detector.value()->calculate_score();
    printf("  Audio fingerprinting detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Uses Audio: %s\n", score.uses_audio ? "yes" : "no");
    printf("    Audio calls: %zu\n", score.audio_calls);
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.uses_audio && score.aggressiveness_score > 0.6f)
        log_pass("Audio fingerprinting detected");
    else
        log_fail("Audio fingerprinting", "Did not detect audio fingerprinting");
}

// Test 5: Excessive navigator enumeration
static void test_navigator_enumeration()
{
    print_section("Test 5: Navigator Property Enumeration");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Navigator enumeration", "Failed to create detector");
        return;
    }

    // Simulate excessive navigator property access (15 calls)
    for (int i = 0; i < 15; i++) {
        detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::NavigatorEnumeration, "navigator.platform"sv, false);
    }

    auto score = detector.value()->calculate_score();
    printf("  Navigator enumeration detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Uses Navigator: %s\n", score.uses_navigator ? "yes" : "no");
    printf("    Navigator calls: %zu\n", score.navigator_calls);
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.uses_navigator && score.navigator_calls >= 10)
        log_pass("Navigator enumeration detected");
    else
        log_fail("Navigator enumeration", "Did not detect excessive navigator access");
}

// Test 6: Font enumeration
static void test_font_enumeration()
{
    print_section("Test 6: Font Enumeration");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Font enumeration", "Failed to create detector");
        return;
    }

    // Simulate font enumeration (25 font checks)
    for (int i = 0; i < 25; i++) {
        detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::FontEnumeration, "measureText"sv, false);
    }

    auto score = detector.value()->calculate_score();
    printf("  Font enumeration detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Uses Fonts: %s\n", score.uses_fonts ? "yes" : "no");
    printf("    Font calls: %zu\n", score.font_calls);
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.uses_fonts && score.font_calls >= 20)
        log_pass("Font enumeration detected");
    else
        log_fail("Font enumeration", "Did not detect font enumeration");
}

// Test 7: Combined fingerprinting (multiple techniques)
static void test_combined_fingerprinting()
{
    print_section("Test 7: Combined Fingerprinting (Multiple Techniques)");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Combined fingerprinting", "Failed to create detector");
        return;
    }

    // Simulate aggressive fingerprinting using multiple techniques
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "toDataURL"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::WebGL, "getParameter"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::AudioContext, "createOscillator"sv, false);
    for (int i = 0; i < 12; i++) {
        detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::NavigatorEnumeration, "navigator.userAgent"sv, false);
    }

    auto score = detector.value()->calculate_score();
    printf("  Combined fingerprinting detected:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Confidence: %.2f\n", score.confidence);
    printf("    Techniques used: %zu\n", score.techniques_used);
    printf("    Total API calls: %zu\n", score.total_api_calls);
    printf("    Rapid fire: %s\n", score.rapid_fire_detected ? "yes" : "no");
    printf("    No user interaction: %s\n", score.no_user_interaction ? "yes" : "no");
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    // Combined techniques should result in high score with multipliers
    if (score.techniques_used >= 3 && score.aggressiveness_score > 0.7f)
        log_pass("Combined fingerprinting detected with high score");
    else
        log_fail("Combined fingerprinting", "Did not detect combined fingerprinting properly");
}

// Test 8: Rapid fire detection
static void test_rapid_fire()
{
    print_section("Test 8: Rapid Fire Detection");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Rapid fire", "Failed to create detector");
        return;
    }

    // Simulate rapid fire: 6+ calls in quick succession
    for (int i = 0; i < 6; i++) {
        detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "toDataURL"sv, false);
    }

    auto score = detector.value()->calculate_score();
    printf("  Rapid fire detection:\n");
    printf("    Score: %.2f\n", score.aggressiveness_score);
    printf("    Total calls: %zu\n", score.total_api_calls);
    printf("    Time window: %.2fs\n", score.time_window_seconds);
    printf("    Rapid fire detected: %s\n", score.rapid_fire_detected ? "yes" : "no");
    printf("    Explanation: %s\n", score.explanation.bytes_as_string_view().characters_without_null_termination());

    if (score.rapid_fire_detected)
        log_pass("Rapid fire detection");
    else
        log_fail("Rapid fire", "Did not detect rapid fire API calls");
}

// Test 9: is_aggressive_fingerprinting() helper
static void test_aggressive_helper()
{
    print_section("Test 9: is_aggressive_fingerprinting() Helper");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Aggressive helper", "Failed to create detector");
        return;
    }

    // Add aggressive fingerprinting (multiple techniques + rapid fire)
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "toDataURL"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::WebGL, "getParameter"sv, false);
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::AudioContext, "createOscillator"sv, false);

    bool is_aggressive = detector.value()->is_aggressive_fingerprinting();
    printf("  is_aggressive_fingerprinting(): %s\n", is_aggressive ? "true" : "false");

    if (is_aggressive)
        log_pass("Aggressive fingerprinting helper works");
    else
        log_fail("Aggressive helper", "Helper did not identify aggressive fingerprinting");
}

// Test 10: Reset functionality
static void test_reset()
{
    print_section("Test 10: Reset Functionality");

    auto detector = FingerprintingDetector::create();
    if (detector.is_error()) {
        log_fail("Reset", "Failed to create detector");
        return;
    }

    // Add some API calls
    detector.value()->record_api_call(FingerprintingDetector::FingerprintingTechnique::Canvas, "toDataURL"sv, false);
    auto score_before = detector.value()->calculate_score();

    // Reset
    detector.value()->reset();
    auto score_after = detector.value()->calculate_score();

    printf("  Score before reset: %.2f\n", score_before.aggressiveness_score);
    printf("  Score after reset: %.2f\n", score_after.aggressiveness_score);

    if (score_after.aggressiveness_score == 0.0f && score_after.total_api_calls == 0)
        log_pass("Reset functionality works");
    else
        log_fail("Reset", "Reset did not clear detector state");
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    printf("====================================\n");
    printf("  FingerprintingDetector Tests\n");
    printf("====================================\n");
    printf("  Milestone 0.4 Phase 4\n");
    printf("  Browser Fingerprinting Detection\n");
    printf("====================================\n");

    test_no_activity();
    test_canvas_fingerprinting();
    test_webgl_fingerprinting();
    test_audio_fingerprinting();
    test_navigator_enumeration();
    test_font_enumeration();
    test_combined_fingerprinting();
    test_rapid_fire();
    test_aggressive_helper();
    test_reset();

    printf("\n====================================\n");
    printf("  Test Summary\n");
    printf("====================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("====================================\n\n");

    if (tests_failed > 0) {
        printf("❌ Some tests FAILED\n");
        return 1;
    }

    printf("✅ All tests PASSED\n");
    return 0;
}
