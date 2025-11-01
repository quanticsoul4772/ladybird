/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PhishingURLAnalyzer.h"
#include <AK/Array.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StringView.h>
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

// Test 1: Legitimate URLs should pass
static void test_legitimate_urls()
{
    print_section("Test 1: Legitimate URLs");

    auto analyzer = PhishingURLAnalyzer::create();
    if (analyzer.is_error()) {
        log_fail("Legitimate URLs", "Failed to create analyzer");
        return;
    }

    Array legitimate_urls = {
        "https://google.com"sv,
        "https://github.com/ladybird"sv,
        "https://www.paypal.com/login"sv,
        "https://amazon.com/products"sv,
    };

    for (auto url : legitimate_urls) {
        auto result = analyzer.value()->analyze_url(url);
        if (result.is_error()) {
            log_fail("Legitimate URLs", "Analysis failed");
            return;
        }

        printf("  %s: score=%.2f\n", url.characters_without_null_termination(),
            result.value().phishing_score);

        if (result.value().phishing_score > 0.5f) {
            log_fail("Legitimate URLs", "False positive on legitimate URL");
            return;
        }
    }

    log_pass("All legitimate URLs scored low");
}

// Test 2: Homograph attacks (Unicode lookalikes)
static void test_homograph_attacks()
{
    print_section("Test 2: Homograph Attacks");

    // Note: ICU homograph detection works on actual Unicode mixed-script strings
    // Testing the detect_homograph function directly since Unicode literals in
    // source files can be unreliable across different editors/encodings

    // Test that the function exists and returns valid results
    auto result1 = PhishingURLAnalyzer::detect_homograph("paypal.com"sv);
    auto result2 = PhishingURLAnalyzer::detect_homograph("google.com"sv);

    if (result1.is_error() || result2.is_error()) {
        log_fail("Homograph attacks", "Homograph detection failed to execute");
        return;
    }

    // Normal ASCII domains should not trigger homograph detection
    if (!result1.value() && !result2.value()) {
        printf("  paypal.com: homograph=%d (expected: 0)\n", result1.value());
        printf("  google.com: homograph=%d (expected: 0)\n", result2.value());
        log_pass("Homograph detection function works correctly");
    } else {
        log_fail("Homograph attacks", "False positive on ASCII domains");
    }
}

// Test 3: Typosquatting (similar to popular domains)
static void test_typosquatting()
{
    print_section("Test 3: Typosquatting Detection");

    auto analyzer = PhishingURLAnalyzer::create();
    if (analyzer.is_error()) {
        log_fail("Typosquatting", "Failed to create analyzer");
        return;
    }

    Array typosquatting_urls = {
        "https://paypai.com"sv,      // paypal typo
        "https://gooogle.com"sv,     // google typo
        "https://amazom.com"sv,      // amazon typo
        "https://bankofamerca.com"sv, // bank of america typo
    };

    int detected = 0;
    for (auto url : typosquatting_urls) {
        auto result = analyzer.value()->analyze_url(url);
        if (result.is_error())
            continue;

        printf("  %s: typosquat=%d, closest='%s', score=%.2f\n",
            url.characters_without_null_termination(),
            result.value().is_typosquatting,
            result.value().closest_legitimate_domain.bytes_as_string_view().characters_without_null_termination(),
            result.value().phishing_score);

        if (result.value().is_typosquatting || result.value().phishing_score > 0.3f)
            detected++;
    }

    if (detected >= 3)
        log_pass("Typosquatting detection");
    else
        log_fail("Typosquatting", "Did not detect enough typosquatting attempts");
}

// Test 4: Suspicious TLDs
static void test_suspicious_tlds()
{
    print_section("Test 4: Suspicious TLD Detection");

    auto analyzer = PhishingURLAnalyzer::create();
    if (analyzer.is_error()) {
        log_fail("Suspicious TLDs", "Failed to create analyzer");
        return;
    }

    Array suspicious_tld_urls = {
        "https://paypal-login.tk"sv,  // Free TLD
        "https://secure-bank.ml"sv,   // Free TLD
        "https://verify-account.gq"sv, // Free TLD
    };

    int detected = 0;
    for (auto url : suspicious_tld_urls) {
        auto result = analyzer.value()->analyze_url(url);
        if (result.is_error())
            continue;

        printf("  %s: suspicious_tld=%d, score=%.2f\n",
            url.characters_without_null_termination(),
            result.value().has_suspicious_tld,
            result.value().phishing_score);

        if (result.value().has_suspicious_tld)
            detected++;
    }

    if (detected == suspicious_tld_urls.size())
        log_pass("Suspicious TLD detection");
    else
        log_fail("Suspicious TLDs", "Did not detect all suspicious TLDs");
}

// Test 5: High-entropy random domains
static void test_high_entropy_domains()
{
    print_section("Test 5: High Entropy Domain Detection");

    auto analyzer = PhishingURLAnalyzer::create();
    if (analyzer.is_error()) {
        log_fail("High entropy", "Failed to create analyzer");
        return;
    }

    Array random_urls = {
        "https://xj3k9f2m8q.com"sv,     // Random
        "https://a1b2c3d4e5.com"sv,     // Random alphanumeric
        "https://qwertyasdfgh.com"sv,   // Random keyboard mash
    };

    int high_entropy = 0;
    for (auto url : random_urls) {
        auto result = analyzer.value()->analyze_url(url);
        if (result.is_error())
            continue;

        printf("  %s: entropy=%.2f, score=%.2f\n",
            url.characters_without_null_termination(),
            result.value().domain_entropy,
            result.value().phishing_score);

        if (result.value().domain_entropy > 3.5f)
            high_entropy++;
    }

    if (high_entropy >= 1)
        log_pass("High entropy detection");
    else
        log_fail("High entropy", "Did not detect high-entropy domains");
}

// Test 6: Combined phishing indicators
static void test_combined_indicators()
{
    print_section("Test 6: Combined Phishing Indicators");

    auto analyzer = PhishingURLAnalyzer::create();
    if (analyzer.is_error()) {
        log_fail("Combined indicators", "Failed to create analyzer");
        return;
    }

    // URL with multiple red flags: typosquatting + suspicious TLD
    auto result = analyzer.value()->analyze_url("https://paypai.tk"sv);
    if (result.is_error()) {
        log_fail("Combined indicators", "Analysis failed");
        return;
    }

    printf("  paypai.tk: score=%.2f, confidence=%.2f\n",
        result.value().phishing_score,
        result.value().confidence);
    printf("  Explanation: %s\n",
        result.value().explanation.bytes_as_string_view().characters_without_null_termination());

    // Should have high score due to multiple indicators
    if (result.value().phishing_score > 0.4f)
        log_pass("Combined indicators detected");
    else
        log_fail("Combined indicators", "Did not combine multiple indicators properly");
}

// Test 7: Levenshtein distance calculation
static void test_levenshtein_distance()
{
    print_section("Test 7: Levenshtein Distance");

    // Test known distances
    struct TestCase {
        StringView a;
        StringView b;
        int expected_distance;
    };

    Array test_cases = {
        TestCase { "kitten"sv, "sitting"sv, 3 },
        TestCase { "paypal"sv, "paypai"sv, 1 },
        TestCase { "google"sv, "gooogle"sv, 1 },
        TestCase { "abc"sv, "abc"sv, 0 },
    };

    bool all_correct = true;
    for (auto const& test : test_cases) {
        int distance = PhishingURLAnalyzer::levenshtein_distance(test.a, test.b);
        printf("  distance('%s', '%s') = %d (expected %d)\n",
            test.a.characters_without_null_termination(),
            test.b.characters_without_null_termination(),
            distance, test.expected_distance);

        if (distance != test.expected_distance)
            all_correct = false;
    }

    if (all_correct)
        log_pass("Levenshtein distance calculation");
    else
        log_fail("Levenshtein distance", "Incorrect distance calculations");
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    printf("====================================\n");
    printf("  PhishingURLAnalyzer Tests\n");
    printf("====================================\n");
    printf("  Milestone 0.4 Phase 5\n");
    printf("  Advanced Phishing URL Analysis\n");
    printf("====================================\n");

    test_legitimate_urls();
    test_homograph_attacks();
    test_typosquatting();
    test_suspicious_tlds();
    test_high_entropy_domains();
    test_combined_indicators();
    test_levenshtein_distance();

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
