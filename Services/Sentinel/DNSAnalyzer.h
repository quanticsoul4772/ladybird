/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

// DGA (Domain Generation Algorithm) detection and DNS tunneling analysis
// Milestone 0.4 Phase 6: Network Behavioral Analysis
class DNSAnalyzer {
public:
    // DGA detection result
    struct DGAAnalysis {
        float entropy { 0.0f };                // Shannon entropy (0.0-8.0, typical range 0-5)
        float consonant_ratio { 0.0f };        // Ratio of consonants to alpha chars (0.0-1.0)
        float ngram_score { 0.0f };            // N-gram frequency score (0.0-1.0, higher = more unusual)
        bool is_dga { false };                 // True if DGA detected
        float confidence { 0.0f };             // Detection confidence (0.0-1.0)
        String explanation;                    // Human-readable explanation
    };

    // DNS tunneling detection result
    struct DNSTunnelingAnalysis {
        u32 query_depth { 0 };                 // Subdomain depth (e.g., "a.b.c.com" = 3)
        u32 query_frequency { 0 };             // Queries per minute
        bool has_base64 { false };             // Contains base64-encoded data
        bool is_tunneling { false };           // True if tunneling detected
        float confidence { 0.0f };             // Detection confidence (0.0-1.0)
        String explanation;                    // Human-readable explanation
    };

    static ErrorOr<NonnullOwnPtr<DNSAnalyzer>> create();
    ~DNSAnalyzer() = default;

    // Analyze a domain for DGA characteristics
    ErrorOr<DGAAnalysis> analyze_dga(StringView domain);

    // Analyze for DNS tunneling (requires query frequency)
    ErrorOr<DNSTunnelingAnalysis> analyze_tunneling(StringView domain, u32 query_count_per_minute);

    // Individual detection methods (public for testing)
    static float calculate_shannon_entropy(StringView domain);
    static float calculate_consonant_ratio(StringView domain);
    static float calculate_ngram_score(StringView domain);
    static bool is_popular_domain(StringView domain);
    static bool contains_base64(StringView subdomain);
    static u32 calculate_subdomain_depth(StringView domain);

private:
    DNSAnalyzer() = default;

    // Common English bigrams and trigrams for n-gram analysis
    static HashMap<String, float> const& common_bigrams();
    static HashMap<String, float> const& common_trigrams();

    // Popular domains whitelist (top 100 domains to reduce false positives)
    static Vector<StringView> const& popular_domain_list();

    // Detection thresholds
    static constexpr float HIGH_ENTROPY_THRESHOLD = 3.5f;          // Shannon entropy threshold
    static constexpr float VERY_HIGH_ENTROPY_THRESHOLD = 4.0f;     // Very suspicious entropy
    static constexpr float HIGH_CONSONANT_RATIO_THRESHOLD = 0.65f; // Suspicious consonant ratio
    static constexpr float NORMAL_CONSONANT_RATIO_MIN = 0.40f;     // Normal range min
    static constexpr float NORMAL_CONSONANT_RATIO_MAX = 0.55f;     // Normal range max
    static constexpr u32 TUNNELING_QUERY_THRESHOLD = 10;           // Queries per minute
    static constexpr u32 TUNNELING_DEPTH_THRESHOLD = 3;            // Subdomain depth
    static constexpr size_t BASE64_MIN_LENGTH = 8;                 // Min length for base64 detection
};

}
