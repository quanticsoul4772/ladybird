/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "DNSAnalyzer.h"
#include <AK/CharacterTypes.h>
#include <AK/Math.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

// Top 100 popular domains to whitelist (reduce false positives)
Vector<StringView> const& DNSAnalyzer::popular_domain_list()
{
    static Vector<StringView> const domains = {
        // Search engines
        "google.com"sv, "bing.com"sv, "yahoo.com"sv, "duckduckgo.com"sv, "baidu.com"sv,
        // Social media
        "facebook.com"sv, "twitter.com"sv, "instagram.com"sv, "linkedin.com"sv, "reddit.com"sv,
        "tiktok.com"sv, "pinterest.com"sv, "snapchat.com"sv, "tumblr.com"sv, "whatsapp.com"sv,
        // E-commerce
        "amazon.com"sv, "ebay.com"sv, "alibaba.com"sv, "walmart.com"sv, "etsy.com"sv,
        "shopify.com"sv, "target.com"sv, "bestbuy.com"sv,
        // Tech companies
        "apple.com"sv, "microsoft.com"sv, "github.com"sv, "gitlab.com"sv, "stackoverflow.com"sv,
        "adobe.com"sv, "nvidia.com"sv, "intel.com"sv, "amd.com"sv, "oracle.com"sv,
        "salesforce.com"sv, "atlassian.com"sv, "zoom.us"sv, "slack.com"sv, "dropbox.com"sv,
        // Media & Entertainment
        "youtube.com"sv, "netflix.com"sv, "spotify.com"sv, "twitch.tv"sv, "hulu.com"sv,
        "vimeo.com"sv, "soundcloud.com"sv, "medium.com"sv, "wordpress.com"sv,
        // News
        "cnn.com"sv, "bbc.com"sv, "nytimes.com"sv, "theguardian.com"sv, "reuters.com"sv,
        "bloomberg.com"sv, "forbes.com"sv, "wsj.com"sv,
        // Cloud & CDN
        "cloudflare.com"sv, "amazonaws.com"sv, "azure.com"sv, "googlecloud.com"sv,
        "digitalocean.com"sv, "heroku.com"sv, "fastly.com"sv, "akamai.com"sv,
        // Email & Productivity
        "gmail.com"sv, "outlook.com"sv, "protonmail.com"sv, "mail.com"sv,
        "office365.com"sv, "gsuite.com"sv,
        // Financial
        "paypal.com"sv, "chase.com"sv, "wellsfargo.com"sv, "bankofamerica.com"sv,
        "coinbase.com"sv, "binance.com"sv, "kraken.com"sv, "stripe.com"sv,
        // Travel
        "booking.com"sv, "airbnb.com"sv, "expedia.com"sv, "tripadvisor.com"sv,
        // Government & Education
        "irs.gov"sv, "whitehouse.gov"sv, "nasa.gov"sv, "wikipedia.org"sv,
        "arxiv.org"sv, "mit.edu"sv, "stanford.edu"sv, "harvard.edu"sv,
        // Other popular
        "craigslist.org"sv, "indeed.com"sv, "weather.com"sv, "imdb.com"sv,
        "yelp.com"sv, "zillow.com"sv, "espn.com"sv, "webmd.com"sv,
    };
    return domains;
}

// Common English bigrams (two-character sequences) with normalized frequencies
// Higher frequency = more common in legitimate domains
HashMap<String, float> const& DNSAnalyzer::common_bigrams()
{
    static HashMap<String, float> bigrams = {
        { "th"_string, 1.0f }, { "he"_string, 0.98f }, { "in"_string, 0.96f },
        { "er"_string, 0.94f }, { "an"_string, 0.92f }, { "re"_string, 0.90f },
        { "on"_string, 0.88f }, { "at"_string, 0.86f }, { "en"_string, 0.84f },
        { "nd"_string, 0.82f }, { "ti"_string, 0.80f }, { "es"_string, 0.78f },
        { "or"_string, 0.76f }, { "te"_string, 0.74f }, { "of"_string, 0.72f },
        { "ed"_string, 0.70f }, { "is"_string, 0.68f }, { "it"_string, 0.66f },
        { "al"_string, 0.64f }, { "ar"_string, 0.62f }, { "st"_string, 0.60f },
        { "to"_string, 0.58f }, { "nt"_string, 0.56f }, { "ng"_string, 0.54f },
        { "se"_string, 0.52f }, { "ha"_string, 0.50f }, { "as"_string, 0.48f },
        { "ou"_string, 0.46f }, { "io"_string, 0.44f }, { "le"_string, 0.42f },
    };
    return bigrams;
}

// Common English trigrams (three-character sequences)
HashMap<String, float> const& DNSAnalyzer::common_trigrams()
{
    static HashMap<String, float> trigrams = {
        { "the"_string, 1.0f }, { "and"_string, 0.95f }, { "ing"_string, 0.90f },
        { "ion"_string, 0.85f }, { "tio"_string, 0.80f }, { "ent"_string, 0.75f },
        { "ati"_string, 0.70f }, { "for"_string, 0.65f }, { "her"_string, 0.60f },
        { "ter"_string, 0.55f }, { "hat"_string, 0.50f }, { "tha"_string, 0.48f },
        { "ere"_string, 0.46f }, { "ate"_string, 0.44f }, { "his"_string, 0.42f },
        { "con"_string, 0.40f }, { "res"_string, 0.38f }, { "ver"_string, 0.36f },
        { "all"_string, 0.34f }, { "ons"_string, 0.32f }, { "nce"_string, 0.30f },
        { "men"_string, 0.28f }, { "ith"_string, 0.26f }, { "ted"_string, 0.24f },
        { "ers"_string, 0.22f }, { "pro"_string, 0.20f }, { "thi"_string, 0.18f },
        { "wit"_string, 0.16f }, { "are"_string, 0.14f }, { "ess"_string, 0.12f },
    };
    return trigrams;
}

ErrorOr<NonnullOwnPtr<DNSAnalyzer>> DNSAnalyzer::create()
{
    return adopt_own(*new DNSAnalyzer());
}

// Calculate Shannon entropy of a domain name
// H(X) = -Σ p(x) * log2(p(x))
// Returns: 0.0-8.0 (typical legitimate domains: 2.5-3.5, DGA domains: 3.5-5.0)
float DNSAnalyzer::calculate_shannon_entropy(StringView domain)
{
    if (domain.is_empty())
        return 0.0f;

    // Count character frequency
    u32 frequency[256] = { 0 };
    for (auto ch : domain.bytes())
        frequency[static_cast<u8>(ch)]++;

    // Calculate Shannon entropy
    float entropy = 0.0f;
    float domain_length = static_cast<float>(domain.length());

    for (size_t i = 0; i < 256; i++) {
        if (frequency[i] == 0)
            continue;

        float probability = static_cast<float>(frequency[i]) / domain_length;
        entropy -= probability * AK::log2(probability);
    }

    return entropy;
}

// Calculate ratio of consonants to total alphabetic characters
// Legitimate domains typically have 40-55% consonants, DGA domains often >65%
float DNSAnalyzer::calculate_consonant_ratio(StringView domain)
{
    if (domain.is_empty())
        return 0.0f;

    u32 consonant_count = 0;
    u32 alpha_count = 0;

    for (auto ch : domain.bytes()) {
        if (is_ascii_alpha(ch)) {
            alpha_count++;
            char lower_ch = to_ascii_lowercase(ch);
            // Vowels: a, e, i, o, u
            if (lower_ch != 'a' && lower_ch != 'e' && lower_ch != 'i'
                && lower_ch != 'o' && lower_ch != 'u') {
                consonant_count++;
            }
        }
    }

    if (alpha_count == 0)
        return 0.0f;

    return static_cast<float>(consonant_count) / static_cast<float>(alpha_count);
}

// Calculate n-gram frequency score (0.0 = common patterns, 1.0 = unusual patterns)
// Compares bigrams and trigrams against common English patterns
float DNSAnalyzer::calculate_ngram_score(StringView domain)
{
    if (domain.length() < 2)
        return 0.0f;

    // Extract alphabetic characters only (ignore dots, numbers)
    StringBuilder alpha_only;
    for (auto ch : domain.bytes()) {
        if (is_ascii_alpha(ch))
            alpha_only.append(to_ascii_lowercase(ch));
    }

    auto alpha_domain = MUST(alpha_only.to_string());
    if (alpha_domain.bytes_as_string_view().length() < 2)
        return 0.0f;

    auto const& bigrams = common_bigrams();
    auto const& trigrams = common_trigrams();

    float bigram_score = 0.0f;
    u32 bigram_count = 0;

    float trigram_score = 0.0f;
    u32 trigram_count = 0;

    // Analyze bigrams
    for (size_t i = 0; i < alpha_domain.bytes_as_string_view().length() - 1; i++) {
        auto bigram = MUST(String::from_utf8(alpha_domain.bytes_as_string_view().substring_view(i, 2)));

        if (bigrams.contains(bigram)) {
            bigram_score += bigrams.get(bigram).value();
        }
        bigram_count++;
    }

    // Analyze trigrams
    for (size_t i = 0; i < alpha_domain.bytes_as_string_view().length() - 2; i++) {
        auto trigram = MUST(String::from_utf8(alpha_domain.bytes_as_string_view().substring_view(i, 3)));

        if (trigrams.contains(trigram)) {
            trigram_score += trigrams.get(trigram).value();
        }
        trigram_count++;
    }

    // Calculate average n-gram frequency
    float avg_frequency = 0.0f;
    if (bigram_count > 0 && trigram_count > 0) {
        avg_frequency = ((bigram_score / static_cast<float>(bigram_count)) * 0.4f)
            + ((trigram_score / static_cast<float>(trigram_count)) * 0.6f);
    } else if (bigram_count > 0) {
        avg_frequency = bigram_score / static_cast<float>(bigram_count);
    }

    // Invert score: low frequency = high score (more unusual = more suspicious)
    // avg_frequency ranges from 0.0 (no common patterns) to ~0.6 (many common patterns)
    // We want to return 0.0-1.0 where 1.0 is most unusual
    return 1.0f - AK::min(avg_frequency * 2.0f, 1.0f);
}

// Check if domain is in popular domains whitelist
bool DNSAnalyzer::is_popular_domain(StringView domain)
{
    auto const& popular = popular_domain_list();

    for (auto const& popular_domain : popular) {
        if (domain.equals_ignoring_ascii_case(popular_domain))
            return true;

        // Also check if domain ends with popular domain (e.g., "www.google.com" matches "google.com")
        if (domain.ends_with(popular_domain, CaseSensitivity::CaseInsensitive))
            return true;
    }

    return false;
}

// Detect base64-encoded subdomains (common in DNS tunneling)
// Checks for base64 character distribution and typical padding
bool DNSAnalyzer::contains_base64(StringView subdomain)
{
    if (subdomain.length() < BASE64_MIN_LENGTH)
        return false;

    // Count special characters that indicate NOT base64
    u32 special_chars = 0;
    u32 base64_only_chars = 0; // Characters that are valid in base64 but less common in normal domains
    u32 total_chars = subdomain.length();

    for (auto ch : subdomain.bytes()) {
        // Characters that indicate normal domain (not base64)
        if (ch == '-' || ch == '_') {
            special_chars++;
        }
        // Base64 padding or special chars
        if (ch == '=' || ch == '+' || ch == '/') {
            base64_only_chars++;
        }
    }

    // If it has hyphens but no base64-specific chars, it's probably a normal domain
    if (special_chars > 0 && base64_only_chars == 0)
        return false;

    // Count uppercase letters (common in base64, less common in domains)
    u32 uppercase_count = 0;
    for (auto ch : subdomain.bytes()) {
        if (is_ascii_upper_alpha(ch))
            uppercase_count++;
    }

    // Base64 typically has mixed case, regular domains are usually lowercase
    float uppercase_ratio = static_cast<float>(uppercase_count) / static_cast<float>(total_chars);

    // Check for typical base64 length (multiples of 4, or close to it with padding)
    bool length_compatible = (total_chars % 4 == 0) || (total_chars % 4 == 2) || (total_chars % 4 == 3);

    // More strict detection: must have mixed case AND proper length
    return (uppercase_ratio > 0.2f) && length_compatible && (total_chars >= BASE64_MIN_LENGTH);
}

// Calculate subdomain depth (e.g., "a.b.c.example.com" = 3)
u32 DNSAnalyzer::calculate_subdomain_depth(StringView domain)
{
    u32 depth = 0;

    for (auto ch : domain.bytes()) {
        if (ch == '.')
            depth++;
    }

    // Depth is number of dots (e.g., "example.com" has 1 dot but depth 0 for our purposes)
    // We consider the rightmost two components as the base domain
    // So "a.b.example.com" has 3 dots total, minus 1 for ".com", minus 1 for "example." = depth 1
    return depth > 0 ? depth : 0;
}

// Main DGA analysis method
ErrorOr<DNSAnalyzer::DGAAnalysis> DNSAnalyzer::analyze_dga(StringView full_domain)
{
    DGAAnalysis result;

    if (full_domain.is_empty()) {
        result.explanation = "Empty domain"_string;
        return result;
    }

    // Quick check: whitelist popular domains
    if (is_popular_domain(full_domain)) {
        result.explanation = "Whitelisted popular domain"_string;
        return result;
    }

    // Extract domain name without TLD (e.g., "example.com" -> "example")
    auto last_dot = full_domain.find_last('.');
    StringView domain_name = last_dot.has_value()
        ? full_domain.substring_view(0, last_dot.value())
        : full_domain;

    // If there's a subdomain, extract just the second-level domain
    // (e.g., "www.example.com" -> "example")
    auto second_last_dot = domain_name.find_last('.');
    if (second_last_dot.has_value()) {
        domain_name = domain_name.substring_view(second_last_dot.value() + 1);
    }

    // Calculate individual metrics
    result.entropy = calculate_shannon_entropy(domain_name);
    result.consonant_ratio = calculate_consonant_ratio(domain_name);
    result.ngram_score = calculate_ngram_score(domain_name);

    Vector<String> indicators;

    // Scoring system: accumulate evidence
    float dga_score = 0.0f;

    // High entropy (weight: 35%)
    if (result.entropy > VERY_HIGH_ENTROPY_THRESHOLD) {
        dga_score += 0.35f;
        indicators.append(MUST(String::formatted("Very high entropy ({:.2f})", result.entropy)));
    } else if (result.entropy > HIGH_ENTROPY_THRESHOLD) {
        dga_score += 0.20f;
        indicators.append(MUST(String::formatted("High entropy ({:.2f})", result.entropy)));
    }

    // Abnormal consonant ratio (weight: 25%)
    if (result.consonant_ratio > HIGH_CONSONANT_RATIO_THRESHOLD) {
        dga_score += 0.25f;
        indicators.append(MUST(String::formatted("Excessive consonants ({:.0f}%)",
            result.consonant_ratio * 100.0f)));
    } else if (result.consonant_ratio < NORMAL_CONSONANT_RATIO_MIN) {
        dga_score += 0.15f;
        indicators.append(MUST(String::formatted("Too many vowels ({:.0f}% consonants)",
            result.consonant_ratio * 100.0f)));
    }

    // Unusual n-gram patterns (weight: 30%)
    if (result.ngram_score > 0.7f) {
        dga_score += 0.30f;
        indicators.append("Unusual character patterns"_string);
    } else if (result.ngram_score > 0.5f) {
        dga_score += 0.15f;
        indicators.append("Uncommon character patterns"_string);
    }

    // Domain length analysis (weight: 10%)
    if (domain_name.length() > 20) {
        dga_score += 0.10f;
        indicators.append(MUST(String::formatted("Unusually long domain ({} chars)", domain_name.length())));
    }

    // Cap score at 1.0
    dga_score = AK::min(dga_score, 1.0f);

    // Determine if DGA (threshold: 0.6)
    result.is_dga = dga_score >= 0.6f;

    // Confidence based on strength of indicators
    result.confidence = AK::min(dga_score * 1.2f, 1.0f);

    // Generate explanation
    if (indicators.is_empty()) {
        result.explanation = "No DGA indicators detected - appears legitimate"_string;
    } else {
        StringBuilder builder;
        if (result.is_dga) {
            builder.append("DGA domain detected: "_string);
        } else {
            builder.append("Suspicious characteristics: "_string);
        }

        for (size_t i = 0; i < indicators.size(); i++) {
            builder.append(indicators[i]);
            if (i < indicators.size() - 1)
                builder.append(", "_string);
        }

        result.explanation = MUST(builder.to_string());
    }

    return result;
}

// DNS tunneling analysis
ErrorOr<DNSAnalyzer::DNSTunnelingAnalysis> DNSAnalyzer::analyze_tunneling(
    StringView domain, u32 query_count_per_minute)
{
    DNSTunnelingAnalysis result;

    if (domain.is_empty()) {
        result.explanation = "Empty domain"_string;
        return result;
    }

    // Calculate metrics
    result.query_depth = calculate_subdomain_depth(domain);
    result.query_frequency = query_count_per_minute;

    // Check for base64-encoded subdomains
    // Extract first subdomain component
    auto first_dot = domain.find('.');
    if (first_dot.has_value() && first_dot.value() > 0) {
        auto first_subdomain = domain.substring_view(0, first_dot.value());
        result.has_base64 = contains_base64(first_subdomain);
    }

    Vector<String> indicators;
    float tunneling_score = 0.0f;

    // High query rate (weight: 35%)
    if (query_count_per_minute > TUNNELING_QUERY_THRESHOLD * 2) {
        tunneling_score += 0.35f;
        indicators.append(MUST(String::formatted("Very high query rate ({} queries/min)",
            query_count_per_minute)));
    } else if (query_count_per_minute > TUNNELING_QUERY_THRESHOLD) {
        tunneling_score += 0.20f;
        indicators.append(MUST(String::formatted("High query rate ({} queries/min)",
            query_count_per_minute)));
    }

    // Excessive subdomain depth (weight: 25%)
    if (result.query_depth > TUNNELING_DEPTH_THRESHOLD + 2) {
        tunneling_score += 0.25f;
        indicators.append(MUST(String::formatted("Very deep subdomains (depth: {})",
            result.query_depth)));
    } else if (result.query_depth > TUNNELING_DEPTH_THRESHOLD) {
        tunneling_score += 0.15f;
        indicators.append(MUST(String::formatted("Deep subdomains (depth: {})",
            result.query_depth)));
    }

    // Base64-encoded data (weight: 40% - very suspicious)
    if (result.has_base64) {
        tunneling_score += 0.40f;
        indicators.append("Base64-encoded subdomain detected"_string);
    }

    // Cap score at 1.0
    tunneling_score = AK::min(tunneling_score, 1.0f);

    // Determine if tunneling (threshold: 0.5)
    result.is_tunneling = tunneling_score >= 0.5f;

    // Confidence based on strength of indicators
    result.confidence = AK::min(tunneling_score * 1.3f, 1.0f);

    // Generate explanation
    if (indicators.is_empty()) {
        result.explanation = "No DNS tunneling indicators detected"_string;
    } else {
        StringBuilder builder;
        if (result.is_tunneling) {
            builder.append("DNS tunneling detected: "_string);
        } else {
            builder.append("Suspicious DNS patterns: "_string);
        }

        for (size_t i = 0; i < indicators.size(); i++) {
            builder.append(indicators[i]);
            if (i < indicators.size() - 1)
                builder.append(", "_string);
        }

        result.explanation = MUST(builder.to_string());
    }

    return result;
}

}
