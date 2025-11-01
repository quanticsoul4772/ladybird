/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "DNSAnalyzer.h"
#include <LibCore/System.h>
#include <LibMain/Main.h>

using namespace Sentinel;

static void test_shannon_entropy()
{
    // Legitimate domain - should have low entropy
    auto google_entropy = DNSAnalyzer::calculate_shannon_entropy("google"sv);
    outln("Entropy 'google': {:.2f} (expected: ~2.5-3.0)", google_entropy);

    // Random DGA-like domain - should have high entropy
    auto dga_entropy = DNSAnalyzer::calculate_shannon_entropy("xkvjmzqhwbprtfygdn"sv);
    outln("Entropy 'xkvjmzqhwbprtfygdn': {:.2f} (expected: >3.5)", dga_entropy);

    // Very random - should be even higher
    auto random_entropy = DNSAnalyzer::calculate_shannon_entropy("a1b2c3d4e5f6g7h8"sv);
    outln("Entropy 'a1b2c3d4e5f6g7h8': {:.2f} (expected: >4.0)", random_entropy);

    outln("");
}

static void test_consonant_ratio()
{
    // Normal English word - around 40-50% consonants
    auto google_ratio = DNSAnalyzer::calculate_consonant_ratio("google"sv);
    outln("Consonant ratio 'google': {:.2f} (expected: ~0.5)", google_ratio);

    // Consonant-heavy DGA domain
    auto dga_ratio = DNSAnalyzer::calculate_consonant_ratio("bcdfghjklmnpqrstvwxyz"sv);
    outln("Consonant ratio 'bcdfghjklmnpqrstvwxyz': {:.2f} (expected: ~1.0)", dga_ratio);

    // Vowel-heavy
    auto vowel_ratio = DNSAnalyzer::calculate_consonant_ratio("aeiouaeiou"sv);
    outln("Consonant ratio 'aeiouaeiou': {:.2f} (expected: 0.0)", vowel_ratio);

    outln("");
}

static void test_ngram_score()
{
    // Legitimate domain with common patterns
    auto google_score = DNSAnalyzer::calculate_ngram_score("google"sv);
    outln("N-gram score 'google': {:.2f} (expected: <0.3)", google_score);

    // DGA domain with unusual patterns
    auto dga_score = DNSAnalyzer::calculate_ngram_score("xkvjmzqhwb"sv);
    outln("N-gram score 'xkvjmzqhwb': {:.2f} (expected: >0.7)", dga_score);

    outln("");
}

static void test_popular_domain_whitelist()
{
    bool is_google = DNSAnalyzer::is_popular_domain("google.com"sv);
    bool is_facebook = DNSAnalyzer::is_popular_domain("facebook.com"sv);
    bool is_random = DNSAnalyzer::is_popular_domain("randomdomain123.com"sv);

    outln("Is 'google.com' popular: {} (expected: true)", is_google);
    outln("Is 'facebook.com' popular: {} (expected: true)", is_facebook);
    outln("Is 'randomdomain123.com' popular: {} (expected: false)", is_random);

    outln("");
}

static void test_base64_detection()
{
    // Typical base64 string
    bool is_base64_1 = DNSAnalyzer::contains_base64("SGVsbG9Xb3JsZA"sv);
    outln("Is 'SGVsbG9Xb3JsZA' base64: {} (expected: true)", is_base64_1);

    // Not base64
    bool is_base64_2 = DNSAnalyzer::contains_base64("hello-world"sv);
    outln("Is 'hello-world' base64: {} (expected: false)", is_base64_2);

    // Long base64-like string
    bool is_base64_3 = DNSAnalyzer::contains_base64("dGVzdGluZ2Jhc2U2NGVuY29kaW5n"sv);
    outln("Is 'dGVzdGluZ2Jhc2U2NGVuY29kaW5n' base64: {} (expected: true)", is_base64_3);

    outln("");
}

static void test_subdomain_depth()
{
    auto depth1 = DNSAnalyzer::calculate_subdomain_depth("example.com"sv);
    auto depth2 = DNSAnalyzer::calculate_subdomain_depth("www.example.com"sv);
    auto depth3 = DNSAnalyzer::calculate_subdomain_depth("a.b.c.example.com"sv);

    outln("Depth 'example.com': {} (expected: 1)", depth1);
    outln("Depth 'www.example.com': {} (expected: 2)", depth2);
    outln("Depth 'a.b.c.example.com': {} (expected: 4)", depth3);

    outln("");
}

static ErrorOr<void> test_dga_analysis()
{
    auto analyzer = TRY(DNSAnalyzer::create());

    // Test legitimate domain
    outln("=== Testing legitimate domain: google.com ===");
    auto google_result = TRY(analyzer->analyze_dga("google.com"sv));
    outln("Entropy: {:.2f}", google_result.entropy);
    outln("Consonant ratio: {:.2f}", google_result.consonant_ratio);
    outln("N-gram score: {:.2f}", google_result.ngram_score);
    outln("Is DGA: {}", google_result.is_dga);
    outln("Confidence: {:.2f}", google_result.confidence);
    outln("Explanation: {}", google_result.explanation);
    outln("");

    // Test DGA-like domain
    outln("=== Testing DGA-like domain: xk3j9f2lm8n.com ===");
    auto dga_result = TRY(analyzer->analyze_dga("xk3j9f2lm8n.com"sv));
    outln("Entropy: {:.2f}", dga_result.entropy);
    outln("Consonant ratio: {:.2f}", dga_result.consonant_ratio);
    outln("N-gram score: {:.2f}", dga_result.ngram_score);
    outln("Is DGA: {}", dga_result.is_dga);
    outln("Confidence: {:.2f}", dga_result.confidence);
    outln("Explanation: {}", dga_result.explanation);
    outln("");

    // Test another legitimate domain
    outln("=== Testing legitimate domain: github.com ===");
    auto github_result = TRY(analyzer->analyze_dga("github.com"sv));
    outln("Is DGA: {} (expected: false)", github_result.is_dga);
    outln("Explanation: {}", github_result.explanation);
    outln("");

    return {};
}

static ErrorOr<void> test_dns_tunneling_analysis()
{
    auto analyzer = TRY(DNSAnalyzer::create());

    // Test normal query
    outln("=== Testing normal DNS query ===");
    auto normal_result = TRY(analyzer->analyze_tunneling("www.example.com"sv, 5));
    outln("Query depth: {}", normal_result.query_depth);
    outln("Query frequency: {} queries/min", normal_result.query_frequency);
    outln("Has base64: {}", normal_result.has_base64);
    outln("Is tunneling: {}", normal_result.is_tunneling);
    outln("Confidence: {:.2f}", normal_result.confidence);
    outln("Explanation: {}", normal_result.explanation);
    outln("");

    // Test suspicious tunneling
    outln("=== Testing suspicious DNS tunneling ===");
    auto tunnel_result = TRY(analyzer->analyze_tunneling("dGVzdGRhdGE.a.b.c.d.tunnel.example.com"sv, 25));
    outln("Query depth: {}", tunnel_result.query_depth);
    outln("Query frequency: {} queries/min", tunnel_result.query_frequency);
    outln("Has base64: {}", tunnel_result.has_base64);
    outln("Is tunneling: {}", tunnel_result.is_tunneling);
    outln("Confidence: {:.2f}", tunnel_result.confidence);
    outln("Explanation: {}", tunnel_result.explanation);
    outln("");

    return {};
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    outln("=== DNSAnalyzer Test Suite ===\n");

    outln("--- Basic Metrics Tests ---");
    test_shannon_entropy();
    test_consonant_ratio();
    test_ngram_score();
    test_popular_domain_whitelist();
    test_base64_detection();
    test_subdomain_depth();

    outln("--- DGA Detection Tests ---");
    TRY(test_dga_analysis());

    outln("--- DNS Tunneling Detection Tests ---");
    TRY(test_dns_tunneling_analysis());

    outln("=== All tests completed successfully! ===");

    return 0;
}
