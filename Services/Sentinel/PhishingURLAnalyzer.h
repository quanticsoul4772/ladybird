/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

// Advanced phishing URL detection using multiple heuristics
// Milestone 0.4 Phase 5: Advanced Phishing URL Analysis
class PhishingURLAnalyzer {
public:
    struct AnalysisResult {
        float phishing_score { 0.0f };      // 0.0-1.0 (0 = benign, 1 = phishing)
        float confidence { 0.0f };          // Model confidence (0.0-1.0)
        bool is_homograph_attack { false }; // Unicode lookalike domain
        bool is_typosquatting { false };    // Similar to popular domain
        bool has_suspicious_tld { false };  // Free/suspicious TLD
        float domain_entropy { 0.0f };      // Randomness of domain name
        String explanation;                 // Human-readable explanation
        String closest_legitimate_domain;   // If typosquatting detected
    };

    static ErrorOr<NonnullOwnPtr<PhishingURLAnalyzer>> create();
    ~PhishingURLAnalyzer() = default;

    // Analyze a URL for phishing indicators
    ErrorOr<AnalysisResult> analyze_url(StringView url);

    // Individual detection methods
    static ErrorOr<bool> detect_homograph(StringView domain);
    static ErrorOr<String> normalize_to_skeleton(StringView domain);
    static int levenshtein_distance(StringView a, StringView b);
    static ErrorOr<ByteString> find_closest_popular_domain(StringView domain, int& out_distance);
    static bool is_suspicious_tld(StringView tld);
    static float calculate_domain_entropy(StringView domain);

private:
    PhishingURLAnalyzer() = default;

    // Popular domains to check against for typosquatting
    static Vector<ByteString> const& popular_domains();

    // Suspicious/free TLDs commonly used for phishing
    static Vector<ByteString> const& suspicious_tlds();

    // Extract domain from full URL
    static ErrorOr<String> extract_domain(StringView url);
};

}
