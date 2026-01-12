/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "VirusTotalClient.h"
#include "../PolicyGraph.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/File.h>
#include <LibCore/Process.h>
#include <LibCrypto/Hash/SHA2.h>

namespace Sentinel::ThreatIntelligence {

ErrorOr<NonnullOwnPtr<VirusTotalClient>> VirusTotalClient::create(
    String const& api_key,
    u32 requests_per_minute)
{
    return create_with_cache(api_key, nullptr, requests_per_minute);
}

ErrorOr<NonnullOwnPtr<VirusTotalClient>> VirusTotalClient::create_with_cache(
    String const& api_key,
    PolicyGraph* policy_graph,
    u32 requests_per_minute)
{
    if (api_key.is_empty())
        return Error::from_string_literal("VirusTotalClient: API key cannot be empty");

    // Create rate limiter (requests per minute)
    auto rate_limiter = TRY(RateLimiter::create(
        requests_per_minute,
        Duration::from_seconds(60)));  // 1 minute = 60 seconds

    return adopt_nonnull_own_or_enomem(new (nothrow) VirusTotalClient(
        api_key,
        policy_graph,
        move(rate_limiter)));
}

VirusTotalClient::VirusTotalClient(
    String const& api_key,
    PolicyGraph* policy_graph,
    NonnullOwnPtr<RateLimiter> rate_limiter)
    : m_api_key(api_key)
    , m_policy_graph(policy_graph)
    , m_rate_limiter(move(rate_limiter))
{
}

VirusTotalClient::~VirusTotalClient() = default;

ErrorOr<VirusTotalResult> VirusTotalClient::lookup_file_hash(String const& sha256)
{
    m_stats.total_lookups++;

    // Validate SHA256 format (64 hex characters)
    if (sha256.bytes_as_string_view().length() != 64)
        return Error::from_string_literal("Invalid SHA256 hash format (must be 64 hex chars)");

    // Check cache first
    if (m_cache_enabled && m_policy_graph) {
        auto cached = TRY(get_cached_result(sha256));
        if (cached.has_value()) {
            m_stats.cache_hits++;
            return cached.release_value();
        }
        m_stats.cache_misses++;
    }

    // Respect rate limiting
    if (!m_rate_limiter->try_acquire()) {
        m_stats.rate_limited++;
        // Wait for rate limit (blocking)
        TRY(m_rate_limiter->acquire_blocking());
    }

    // Make API request
    auto endpoint = TRY(String::formatted("{}/files/{}", VT_API_BASE, sha256));
    auto json_response = TRY(make_api_request(endpoint));

    // Parse response
    auto result = TRY(parse_file_response(json_response, sha256));

    // Update statistics
    m_stats.api_calls++;
    if (result.is_malicious())
        m_stats.malicious_results++;
    else if (result.is_suspicious())
        m_stats.suspicious_results++;
    else
        m_stats.clean_results++;

    // Cache result
    if (m_cache_enabled && m_policy_graph) {
        auto cache_result_error = cache_result(sha256, result);
        if (cache_result_error.is_error())
            dbgln("VirusTotalClient: Warning: Failed to cache result: {}", cache_result_error.error());
    }

    return result;
}

ErrorOr<VirusTotalResult> VirusTotalClient::lookup_url(String const& url)
{
    m_stats.total_lookups++;

    // URL must be base64-encoded (URL-safe) for VT API
    // For simplicity, we'll use SHA256 of URL as identifier for caching
    auto sha256 = Crypto::Hash::SHA256::create();
    sha256->update(url.bytes());
    auto url_hash = TRY(String::formatted("{:hex-dump}", sha256->digest().bytes()));

    // Check cache
    if (m_cache_enabled && m_policy_graph) {
        auto cached = TRY(get_cached_result(url_hash));
        if (cached.has_value()) {
            m_stats.cache_hits++;
            return cached.release_value();
        }
        m_stats.cache_misses++;
    }

    // Respect rate limiting
    if (!m_rate_limiter->try_acquire()) {
        m_stats.rate_limited++;
        TRY(m_rate_limiter->acquire_blocking());
    }

    // VT requires URL to be base64-encoded (URL-safe, no padding)
    // For now, we'll use a simplified approach via url_id
    auto endpoint = TRY(String::formatted("{}/urls/{}", VT_API_BASE, url_hash));
    auto json_response = TRY(make_api_request(endpoint));

    // Parse response
    auto result = TRY(parse_url_response(json_response, url));

    // Update statistics
    m_stats.api_calls++;
    if (result.is_malicious())
        m_stats.malicious_results++;
    else if (result.is_suspicious())
        m_stats.suspicious_results++;
    else
        m_stats.clean_results++;

    // Cache result
    if (m_cache_enabled && m_policy_graph) {
        auto cache_error = cache_result(url_hash, result);
        if (cache_error.is_error())
            dbgln("VirusTotalClient: Warning: Failed to cache result: {}", cache_error.error());
    }

    return result;
}

ErrorOr<VirusTotalResult> VirusTotalClient::lookup_domain(String const& domain)
{
    m_stats.total_lookups++;

    // Check cache
    if (m_cache_enabled && m_policy_graph) {
        auto cached = TRY(get_cached_result(domain));
        if (cached.has_value()) {
            m_stats.cache_hits++;
            return cached.release_value();
        }
        m_stats.cache_misses++;
    }

    // Respect rate limiting
    if (!m_rate_limiter->try_acquire()) {
        m_stats.rate_limited++;
        TRY(m_rate_limiter->acquire_blocking());
    }

    // Make API request
    auto endpoint = TRY(String::formatted("{}/domains/{}", VT_API_BASE, domain));
    auto json_response = TRY(make_api_request(endpoint));

    // Parse response
    auto result = TRY(parse_domain_response(json_response, domain));

    // Update statistics
    m_stats.api_calls++;
    if (result.is_malicious())
        m_stats.malicious_results++;
    else if (result.is_suspicious())
        m_stats.suspicious_results++;
    else
        m_stats.clean_results++;

    // Cache result
    if (m_cache_enabled && m_policy_graph) {
        auto cache_error = cache_result(domain, result);
        if (cache_error.is_error())
            dbgln("VirusTotalClient: Warning: Failed to cache result: {}", cache_error.error());
    }

    return result;
}

ErrorOr<String> VirusTotalClient::make_api_request(String const& endpoint)
{
    (void)endpoint;  // Mark as unused - TODO: Implement HTTP client
    // TODO: Implement HTTP client for VirusTotal API
    // Use curl to make HTTP GET request with API key header
    // curl -H "x-apikey: YOUR_KEY" https://www.virustotal.com/api/v3/files/{hash}

    // For now, return error indicating HTTP client not implemented
    // In production, this would use LibHTTP or spawn a curl process with Core::Process
    m_stats.api_errors++;
    return Error::from_string_literal("VirusTotal HTTP client not implemented - requires LibHTTP integration");
}

ErrorOr<VirusTotalResult> VirusTotalClient::parse_file_response(
    String const& json_response,
    String const& resource_id)
{
    (void)resource_id;  // TODO: Use in result once HTTP client is implemented
    // Parse JSON response
    auto json = TRY(JsonValue::from_string(json_response));

    if (!json.is_object())
        return Error::from_string_literal("Invalid VirusTotal response: not a JSON object");

    auto root = json.as_object();

    // Check for error response
    if (root.has("error"sv)) {
        auto error_obj = root.get("error"sv)->as_object();
        auto error_code = error_obj.get("code"sv)->as_string();
        dbgln("VirusTotalClient: API error: {}", error_code);
        m_stats.api_errors++;
        return Error::from_string_literal("VirusTotal API returned error");
    }

    // Extract data object
    if (!root.has("data"sv))
        return Error::from_string_literal("Missing 'data' field in VT response");

    auto data = root.get("data"sv)->as_object();

    // Extract attributes
    if (!data.has("attributes"sv))
        return Error::from_string_literal("Missing 'attributes' field in VT response");

    auto attributes = data.get("attributes"sv)->as_object();

    VirusTotalResult result;
    result.resource_id = resource_id;
    result.is_cached = false;

    // Extract scan date
    if (attributes.has("last_analysis_date"sv)) {
        auto timestamp = attributes.get("last_analysis_date"sv)->as_integer<i64>();
        result.scan_date = TRY(String::formatted("{}", timestamp));
    }

    // Extract detection counts
    if (attributes.has("last_analysis_stats"sv)) {
        auto stats = attributes.get("last_analysis_stats"sv)->as_object();

        result.malicious_count = stats.get("malicious"sv)->as_integer<u32>();
        result.suspicious_count = stats.get("suspicious"sv)->as_integer<u32>();
        result.undetected_count = stats.get("undetected"sv)->as_integer<u32>();
        result.harmless_count = stats.get("harmless"sv)->as_integer<u32>();
        result.timeout_count = stats.get("timeout"sv)->as_integer<u32>();

        result.total_engines = result.malicious_count + result.suspicious_count +
            result.undetected_count + result.harmless_count + result.timeout_count;
    }

    // Calculate detection ratio
    if (result.total_engines > 0) {
        result.detection_ratio = static_cast<float>(result.malicious_count) /
            static_cast<float>(result.total_engines);
        result.threat_score = (static_cast<float>(result.malicious_count) +
            static_cast<float>(result.suspicious_count) * 0.5f) /
            static_cast<float>(result.total_engines);
    }

    // Extract vendor verdicts (up to 10 for brevity)
    if (attributes.has("last_analysis_results"sv)) {
        auto results = attributes.get("last_analysis_results"sv)->as_object();
        u32 count = 0;

        results.for_each_member([&](auto const& vendor_name, auto const& verdict_value) {
            if (count >= 10)  // Limit to 10 verdicts
                return;

            auto verdict_obj = verdict_value.as_object();

            VendorVerdict verdict;
            verdict.vendor_name = vendor_name;  // vendor_name is already a String
            verdict.category = verdict_obj.get("category"sv)->as_string();
            if (verdict_obj.has("result"sv) && !verdict_obj.get("result"sv)->is_null())
                verdict.result = verdict_obj.get("result"sv)->as_string();
            else
                verdict.result = "clean"_string;

            result.vendor_verdicts.append(move(verdict));
            count++;
        });
    }

    return result;
}

ErrorOr<VirusTotalResult> VirusTotalClient::parse_url_response(
    String const& json_response,
    String const& resource_id)
{
    // URL responses have same structure as file responses
    return parse_file_response(json_response, resource_id);
}

ErrorOr<VirusTotalResult> VirusTotalClient::parse_domain_response(
    String const& json_response,
    String const& resource_id)
{
    // Domain responses have different structure
    auto json = TRY(JsonValue::from_string(json_response));

    if (!json.is_object())
        return Error::from_string_literal("Invalid VirusTotal response: not a JSON object");

    auto root = json.as_object();

    // Check for error
    if (root.has("error"sv)) {
        m_stats.api_errors++;
        return Error::from_string_literal("VirusTotal API returned error");
    }

    // Extract data
    if (!root.has("data"sv))
        return Error::from_string_literal("Missing 'data' field in VT response");

    auto data = root.get("data"sv)->as_object();

    if (!data.has("attributes"sv))
        return Error::from_string_literal("Missing 'attributes' field in VT response");

    auto attributes = data.get("attributes"sv)->as_object();

    VirusTotalResult result;
    result.resource_id = resource_id;
    result.is_cached = false;

    // Domain reputation uses last_analysis_stats
    if (attributes.has("last_analysis_stats"sv)) {
        auto stats = attributes.get("last_analysis_stats"sv)->as_object();

        result.malicious_count = stats.get("malicious"sv)->as_integer<u32>();
        result.suspicious_count = stats.get("suspicious"sv)->as_integer<u32>();
        result.undetected_count = stats.get("undetected"sv)->as_integer<u32>();
        result.harmless_count = stats.get("harmless"sv)->as_integer<u32>();
        result.timeout_count = stats.get("timeout"sv)->as_integer<u32>();

        result.total_engines = result.malicious_count + result.suspicious_count +
            result.undetected_count + result.harmless_count + result.timeout_count;
    }

    // Calculate scores
    if (result.total_engines > 0) {
        result.detection_ratio = static_cast<float>(result.malicious_count) /
            static_cast<float>(result.total_engines);
        result.threat_score = (static_cast<float>(result.malicious_count) +
            static_cast<float>(result.suspicious_count) * 0.5f) /
            static_cast<float>(result.total_engines);
    }

    return result;
}

ErrorOr<Optional<VirusTotalResult>> VirusTotalClient::get_cached_result(
    String const& resource_id)
{
    if (!m_policy_graph)
        return Optional<VirusTotalResult> {};

    // Query PolicyGraph for cached VT result
    // We'll use the threat_records table with rule_name="virustotal"
    auto cache_key = TRY(String::formatted("vt:{}", resource_id));

    // Try to fetch from threat_records
    // (This is simplified - actual implementation would need a dedicated VT cache table)
    // For now, return empty optional (no cache hit)
    // TODO: Implement proper PolicyGraph cache integration

    return Optional<VirusTotalResult> {};
}

ErrorOr<void> VirusTotalClient::cache_result(
    String const& resource_id,
    VirusTotalResult const& result)
{
    (void)resource_id;  // TODO: Use as cache key when storage is implemented
    if (!m_policy_graph)
        return {};

    // Store result in PolicyGraph cache
    // TTL: 7 days for clean, 30 days for malicious
    u32 ttl_days = result.is_malicious() ? CACHE_TTL_DAYS_MALICIOUS : CACHE_TTL_DAYS_CLEAN;
    (void)ttl_days;  // TODO: Use when cache storage is implemented

    // TODO: Implement PolicyGraph cache storage
    // For now, this is a no-op
    // Actual implementation would store in a dedicated virustotal_cache table

    return {};
}

void VirusTotalClient::reset_statistics()
{
    m_stats.total_lookups = 0;
    m_stats.cache_hits = 0;
    m_stats.cache_misses = 0;
    m_stats.api_calls = 0;
    m_stats.rate_limited = 0;
    m_stats.api_errors = 0;
    m_stats.malicious_results = 0;
    m_stats.suspicious_results = 0;
    m_stats.clean_results = 0;
    m_stats.average_detection_ratio = 0.0f;
}

}
