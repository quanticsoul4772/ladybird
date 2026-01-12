/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PhishingURLAnalyzer.h"
#include <AK/Math.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StringBuilder.h>
#include <LibURL/URL.h>
#include <unicode/uspoof.h>
#include <unicode/utypes.h>

namespace Sentinel {

// Top 100 popular domains that phishers often impersonate
Vector<ByteString> const& PhishingURLAnalyzer::popular_domains()
{
    static Vector<ByteString> const domains = {
        // Financial
        "paypal.com"sv, "chase.com"sv, "bankofamerica.com"sv, "wellsfargo.com"sv,
        "citibank.com"sv, "capitalone.com"sv, "americanexpress.com"sv,
        // Tech giants
        "google.com"sv, "apple.com"sv, "microsoft.com"sv, "amazon.com"sv,
        "facebook.com"sv, "instagram.com"sv, "twitter.com"sv, "linkedin.com"sv,
        "netflix.com"sv, "spotify.com"sv, "dropbox.com"sv,
        // Email providers
        "gmail.com"sv, "outlook.com"sv, "yahoo.com"sv, "protonmail.com"sv,
        // E-commerce
        "ebay.com"sv, "etsy.com"sv, "shopify.com"sv, "walmart.com"sv,
        // Crypto
        "coinbase.com"sv, "binance.com"sv, "kraken.com"sv, "blockchain.com"sv,
        // Government
        "irs.gov"sv, "usps.com"sv, "fedex.com"sv, "ups.com"sv,
        // Cloud/Dev
        "github.com"sv, "gitlab.com"sv, "docker.com"sv, "cloudflare.com"sv,
    };
    return domains;
}

// Suspicious TLDs commonly used for phishing (free or no verification required)
Vector<ByteString> const& PhishingURLAnalyzer::suspicious_tlds()
{
    static Vector<ByteString> const tlds = {
        // Free TLDs (Freenom)
        "tk"sv, "ml"sv, "ga"sv, "cf"sv, "gq"sv,
        // Other suspicious
        "top"sv, "xyz"sv, "club"sv, "work"sv, "click"sv, "link"sv,
        "download"sv, "stream"sv, "online"sv, "site"sv, "website"sv,
    };
    return tlds;
}

ErrorOr<NonnullOwnPtr<PhishingURLAnalyzer>> PhishingURLAnalyzer::create()
{
    return adopt_own(*new PhishingURLAnalyzer());
}

// Extract domain from URL (e.g., "https://example.com/path" -> "example.com")
ErrorOr<String> PhishingURLAnalyzer::extract_domain(StringView url_string)
{
    // Parse URL
    auto url_opt = URL::create_with_url_or_path(ByteString(url_string));
    if (!url_opt.has_value())
        return Error::from_string_literal("Invalid URL");

    auto url = url_opt.release_value();
    auto host = url.serialized_host();

    return TRY(String::from_utf8(StringView(host)));
}

// Unicode homograph detection using ICU spoofchecker
ErrorOr<bool> PhishingURLAnalyzer::detect_homograph(StringView domain)
{
    UErrorCode status = U_ZERO_ERROR;

    // Create spoofchecker
    USpoofChecker* checker = uspoof_open(&status);
    if (U_FAILURE(status)) {
        return Error::from_string_literal("Failed to create ICU spoofchecker");
    }

    // Configure checker for confusables
    uspoof_setChecks(checker, USPOOF_CONFUSABLE, &status);
    if (U_FAILURE(status)) {
        uspoof_close(checker);
        return Error::from_string_literal("Failed to configure spoofchecker");
    }

    // Check if domain contains suspicious confusable characters
    int32_t result = uspoof_checkUTF8(checker,
        reinterpret_cast<char const*>(domain.characters_without_null_termination()),
        domain.length(), nullptr, &status);

    uspoof_close(checker);

    if (U_FAILURE(status))
        return Error::from_string_literal("Spoofcheck failed");

    // Result > 0 means suspicious confusables detected
    return result > 0;
}

// Normalize domain to skeleton (confusables -> ASCII equivalents)
ErrorOr<String> PhishingURLAnalyzer::normalize_to_skeleton(StringView domain)
{
    UErrorCode status = U_ZERO_ERROR;

    USpoofChecker* checker = uspoof_open(&status);
    if (U_FAILURE(status))
        return Error::from_string_literal("Failed to create ICU spoofchecker");

    char skeleton[256];
    int32_t skeleton_len = uspoof_getSkeletonUTF8(checker, 0,
        reinterpret_cast<char const*>(domain.characters_without_null_termination()),
        domain.length(), skeleton, sizeof(skeleton), &status);

    uspoof_close(checker);

    if (U_FAILURE(status) || skeleton_len < 0)
        return Error::from_string_literal("Failed to generate skeleton");

    return String::from_utf8(StringView(skeleton, skeleton_len));
}

// Levenshtein distance (edit distance) between two strings
int PhishingURLAnalyzer::levenshtein_distance(StringView a, StringView b)
{
    size_t const m = a.length();
    size_t const n = b.length();

    // Create distance matrix
    Vector<Vector<int>> distance;
    distance.resize(m + 1);
    for (size_t i = 0; i <= m; i++) {
        distance[i].resize(n + 1);
        distance[i][0] = i;
    }
    for (size_t j = 0; j <= n; j++)
        distance[0][j] = j;

    // Calculate distances
    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int deletion = distance[i - 1][j] + 1;
            int insertion = distance[i][j - 1] + 1;
            int substitution = distance[i - 1][j - 1] + cost;
            distance[i][j] = AK::min(deletion, AK::min(insertion, substitution));
        }
    }

    return distance[m][n];
}

// Find closest popular domain by Levenshtein distance
ErrorOr<ByteString> PhishingURLAnalyzer::find_closest_popular_domain(StringView domain, int& out_distance)
{
    auto const& domains = popular_domains();

    int min_distance = INT_MAX;
    ByteString closest_domain;

    for (auto const& popular_domain : domains) {
        // Extract domain name from popular domain (e.g., "paypal.com" -> "paypal")
        auto popular_domain_sv = StringView(popular_domain);
        auto dot_pos = popular_domain_sv.find_last('.');
        StringView popular_domain_name = dot_pos.has_value()
            ? popular_domain_sv.substring_view(0, dot_pos.value())
            : popular_domain_sv;

        // Compare domain names without TLDs
        auto distance = levenshtein_distance(domain, popular_domain_name);

        if (distance < min_distance) {
            min_distance = distance;
            closest_domain = popular_domain;
        }
    }

    out_distance = min_distance;
    return closest_domain;
}

// Check if TLD is suspicious
bool PhishingURLAnalyzer::is_suspicious_tld(StringView tld)
{
    auto const& suspicious = suspicious_tlds();

    for (auto const& suspicious_tld : suspicious) {
        if (tld.equals_ignoring_ascii_case(suspicious_tld))
            return true;
    }

    return false;
}

// Calculate Shannon entropy of domain name
float PhishingURLAnalyzer::calculate_domain_entropy(StringView domain)
{
    if (domain.is_empty())
        return 0.0f;

    // Count character frequency
    u32 frequency[256] = { 0 };
    for (auto ch : domain.bytes())
        frequency[ch]++;

    // Calculate entropy
    float entropy = 0.0f;
    float domain_size = static_cast<float>(domain.length());

    for (size_t i = 0; i < 256; i++) {
        if (frequency[i] == 0)
            continue;

        float probability = static_cast<float>(frequency[i]) / domain_size;
        entropy -= probability * AK::log2(probability);
    }

    return entropy;
}

// Main analysis method
ErrorOr<PhishingURLAnalyzer::AnalysisResult> PhishingURLAnalyzer::analyze_url(StringView url_string)
{
    AnalysisResult result;

    // Extract domain from URL
    auto domain_result = extract_domain(url_string);
    if (domain_result.is_error()) {
        result.explanation = "Invalid URL format"_string;
        return result;
    }

    auto domain = domain_result.release_value();

    // Remove "www." prefix if present
    auto domain_sv = domain.bytes_as_string_view();
    if (domain_sv.starts_with("www."sv))
        domain_sv = domain_sv.substring_view(4);

    // Extract TLD
    auto last_dot = domain_sv.find_last('.');
    StringView tld = last_dot.has_value() ? domain_sv.substring_view(last_dot.value() + 1) : ""sv;
    StringView domain_name = last_dot.has_value() ? domain_sv.substring_view(0, last_dot.value()) : domain_sv;

    Vector<String> reasons;

    // 1. Homograph detection (30% weight)
    auto homograph_result = detect_homograph(domain_sv);
    if (!homograph_result.is_error() && homograph_result.value()) {
        result.is_homograph_attack = true;
        result.phishing_score += 0.3f;
        reasons.append("Contains Unicode lookalike characters (homograph attack)"_string);
    }

    // 2. Typosquatting detection (25% weight)
    int edit_distance = 0;
    auto closest_result = find_closest_popular_domain(domain_name, edit_distance);
    if (!closest_result.is_error()) {
        result.closest_legitimate_domain = MUST(String::from_utf8(StringView(closest_result.value())));

        // Consider it typosquatting if edit distance is 1-3
        if (edit_distance > 0 && edit_distance <= 3) {
            result.is_typosquatting = true;
            result.phishing_score += 0.25f;
            reasons.append(MUST(String::formatted("Similar to legitimate domain '{}' (edit distance: {})",
                closest_result.value(), edit_distance)));
        }
    }

    // 3. Suspicious TLD detection (20% weight)
    if (is_suspicious_tld(tld)) {
        result.has_suspicious_tld = true;
        result.phishing_score += 0.2f;
        reasons.append(MUST(String::formatted("Suspicious TLD: .{}", tld)));
    }

    // 4. Domain entropy analysis (15% weight)
    result.domain_entropy = calculate_domain_entropy(domain_name);
    // High entropy (>3.5) suggests random/generated domain name
    if (result.domain_entropy > 3.5f) {
        result.phishing_score += 0.15f;
        reasons.append(MUST(String::formatted("High domain entropy ({:.2f}) suggests random generation",
            result.domain_entropy)));
    }

    // 5. Very short domains (<4 chars) can be suspicious (10% weight)
    if (domain_name.length() < 4 && domain_name.length() > 0) {
        result.phishing_score += 0.1f;
        reasons.append("Very short domain name"_string);
    }

    // Cap score at 1.0
    result.phishing_score = min(result.phishing_score, 1.0f);

    // Set confidence based on number of indicators
    result.confidence = min(static_cast<float>(reasons.size()) / 3.0f, 1.0f);

    // Generate explanation
    if (reasons.is_empty()) {
        result.explanation = "No phishing indicators detected"_string;
    } else {
        StringBuilder builder;
        for (size_t i = 0; i < reasons.size(); i++) {
            builder.append(reasons[i]);
            if (i < reasons.size() - 1)
                builder.append("; "_string);
        }
        result.explanation = MUST(builder.to_string());
    }

    return result;
}

}
