/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <AK/Time.h>
#include "ThreatIntelligence/RateLimiter.h"
#include "ThreatIntelligence/VirusTotalClient.h"

using namespace Sentinel::ThreatIntelligence;

TEST_CASE(rate_limiter_basic_functionality)
{
    // Create rate limiter: 4 requests per second
    auto rate_limiter = MUST(RateLimiter::create(4, Duration::from_seconds(1)));

    // First 4 requests should succeed
    for (u32 i = 0; i < 4; i++) {
        EXPECT(rate_limiter->try_acquire());
    }

    // 5th request should fail (rate limit exceeded)
    EXPECT(!rate_limiter->try_acquire());

    // Statistics should reflect this
    auto stats = rate_limiter->get_statistics();
    EXPECT_EQ(stats.total_requests, 5u);
    EXPECT_EQ(stats.allowed_requests, 4u);
    EXPECT_EQ(stats.denied_requests, 1u);
}

TEST_CASE(rate_limiter_token_refill)
{
    // Create rate limiter: 2 requests per second
    auto rate_limiter = MUST(RateLimiter::create(2, Duration::from_seconds(1)));

    // Consume both tokens
    EXPECT(rate_limiter->try_acquire());
    EXPECT(rate_limiter->try_acquire());
    EXPECT(!rate_limiter->try_acquire());

    // Wait for token refill (1 second)
    timespec sleep_time { .tv_sec = 1, .tv_nsec = 100'000'000 };  // 1.1 seconds
    MUST(Core::System::nanosleep(&sleep_time, nullptr));

    // Should be able to acquire tokens again
    EXPECT(rate_limiter->try_acquire());
    EXPECT(rate_limiter->try_acquire());
}

TEST_CASE(virustotal_client_creation_requires_api_key)
{
    // Empty API key should fail
    auto result = VirusTotalClient::create(""_string);
    EXPECT(result.is_error());
}

TEST_CASE(virustotal_client_creation_with_valid_key)
{
    // Valid API key should succeed
    auto client = MUST(VirusTotalClient::create("test_api_key_12345"_string));
    EXPECT(client.ptr() != nullptr);

    // Check default configuration
    EXPECT_EQ(client->timeout().to_seconds(), 30);
    EXPECT(client->is_cache_enabled());
}

TEST_CASE(virustotal_result_detection_thresholds)
{
    VirusTotalResult result;

    // Clean file (0% detection)
    result.malicious_count = 0;
    result.total_engines = 70;
    result.detection_ratio = 0.0f;
    result.threat_score = 0.0f;
    EXPECT(result.is_clean());
    EXPECT(!result.is_suspicious());
    EXPECT(!result.is_malicious());

    // Suspicious file (5% detection)
    result.malicious_count = 3;
    result.suspicious_count = 2;
    result.total_engines = 70;
    result.detection_ratio = 3.0f / 70.0f;  // ~4.3%
    result.threat_score = (3.0f + 2.0f * 0.5f) / 70.0f;  // ~5.7%
    EXPECT(!result.is_malicious());
    EXPECT(result.is_suspicious());

    // Malicious file (10%+ detection)
    result.malicious_count = 10;
    result.suspicious_count = 5;
    result.total_engines = 70;
    result.detection_ratio = 10.0f / 70.0f;  // ~14.3%
    result.threat_score = (10.0f + 5.0f * 0.5f) / 70.0f;  // ~17.9%
    EXPECT(result.is_malicious());
    EXPECT(!result.is_clean());
}

TEST_CASE(virustotal_client_statistics)
{
    auto client = MUST(VirusTotalClient::create("test_api_key"_string));

    // Initial statistics should be zero
    auto stats = client->get_statistics();
    EXPECT_EQ(stats.total_lookups, 0u);
    EXPECT_EQ(stats.cache_hits, 0u);
    EXPECT_EQ(stats.api_calls, 0u);

    // Reset statistics should work
    client->reset_statistics();
    stats = client->get_statistics();
    EXPECT_EQ(stats.total_lookups, 0u);
}

TEST_CASE(virustotal_client_rate_limiting)
{
    // Create client with low rate limit (1 request per minute for testing)
    auto client = MUST(VirusTotalClient::create("test_key"_string, 1));

    // Note: Actual API calls would be tested with mock responses
    // This test just verifies rate limiter integration exists
    auto stats = client->get_statistics();
    EXPECT_EQ(stats.rate_limited, 0u);
}

TEST_CASE(virustotal_hash_validation)
{
    auto client = MUST(VirusTotalClient::create("test_key"_string));

    // Invalid hash (too short) should fail
    auto result = client->lookup_file_hash("invalid"_string);
    EXPECT(result.is_error());

    // Invalid hash (too long) should fail
    auto long_hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0"_string;
    result = client->lookup_file_hash(long_hash);
    EXPECT(result.is_error());

    // Note: Valid hash test would require mock API response
    // Actual API call test: lookup_file_hash("a" * 64) should succeed (if API key valid)
}

TEST_CASE(vendor_verdict_structure)
{
    VendorVerdict verdict;
    verdict.vendor_name = "Kaspersky"_string;
    verdict.category = "malicious"_string;
    verdict.result = "Trojan.Win32.Generic"_string;

    EXPECT_EQ(verdict.vendor_name, "Kaspersky"_string);
    EXPECT_EQ(verdict.category, "malicious"_string);
    EXPECT_EQ(verdict.result, "Trojan.Win32.Generic"_string);
}
