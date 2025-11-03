/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include <AK/StringView.h>
#include <AK/Time.h>
#include <stdio.h>

using namespace Sentinel;

static void test_create_policy(PolicyGraph& pg)
{
    printf("\n=== Test: Create Policy ===\n");

    PolicyGraph::Policy policy {
        .rule_name = "EICAR_Test_File"_string,
        .url_pattern = {},  // No URL filter
        .file_hash = "abc123"_string,
        .mime_type = {},    // No MIME type filter
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(),
        .created_by = "test_user"_string,
        .expires_at = {},   // No expiration
        .last_hit = {},     // No hits yet
    };

    auto result = pg.create_policy(policy);
    if (result.is_error()) {
        printf("❌ FAILED: Could not create policy: %s\n", result.error().string_literal().characters_without_null_termination());
        return;
    }

    auto policy_id = result.release_value();
    printf("✅ PASSED: Created policy with ID %ld\n", policy_id);
}

static void test_list_policies(PolicyGraph& pg)
{
    printf("\n=== Test: List Policies ===\n");

    auto result = pg.list_policies();
    if (result.is_error()) {
        printf("❌ FAILED: Could not list policies\n");
        return;
    }

    auto policies = result.release_value();
    printf("✅ PASSED: Found %zu policies\n", policies.size());

    for (auto const& policy : policies) {
        printf("  - ID: %ld, Rule: %s, Action: %s, Hits: %ld\n",
               policy.id,
               policy.rule_name.bytes_as_string_view().characters_without_null_termination(),
               policy.action == PolicyGraph::PolicyAction::Allow ? "Allow" :
               policy.action == PolicyGraph::PolicyAction::Block ? "Block" : "Quarantine",
               policy.hit_count);
    }
}

static void test_match_policy_by_hash(PolicyGraph& pg)
{
    printf("\n=== Test: Match Policy by Hash ===\n");

    // Create a policy for specific hash
    PolicyGraph::Policy policy {
        .rule_name = "Test_Rule"_string,
        .url_pattern = {},  // No URL filter
        .file_hash = "hash123456"_string,
        .mime_type = {},    // No MIME type filter
        .action = PolicyGraph::PolicyAction::Quarantine,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(),
        .created_by = "test"_string,
        .expires_at = {},   // No expiration
        .last_hit = {},     // No hits yet
    };

    auto create_result = pg.create_policy(policy);
    if (create_result.is_error()) {
        printf("❌ FAILED: Could not create test policy\n");
        return;
    }

    // Try to match by hash
    PolicyGraph::ThreatMetadata threat {
        .url = "http://example.com/file.exe"_string,
        .filename = "file.exe"_string,
        .file_hash = "hash123456"_string,
        .mime_type = "application/x-msdos-program"_string,
        .file_size = 1024,
        .rule_name = "Test_Rule"_string,
        .severity = "high"_string,
    };

    auto match_result = pg.match_policy(threat);
    if (match_result.is_error()) {
        printf("❌ FAILED: Could not match policy\n");
        return;
    }

    auto matched = match_result.release_value();
    if (matched.has_value()) {
        printf("✅ PASSED: Matched policy ID %ld by hash\n", matched->id);
        printf("  Action: %s, Hit count: %ld\n",
               matched->action == PolicyGraph::PolicyAction::Quarantine ? "Quarantine" : "Other",
               matched->hit_count);
    } else {
        printf("❌ FAILED: No policy matched\n");
    }
}

static void test_match_policy_by_url(PolicyGraph& pg)
{
    printf("\n=== Test: Match Policy by URL Pattern ===\n");

    // Create a policy with URL pattern
    PolicyGraph::Policy policy {
        .rule_name = "Malicious_Domain"_string,
        .url_pattern = "%malicious.com%"_string,  // SQL LIKE pattern
        .file_hash = {},  // No hash filter
        .mime_type = {},  // No MIME type filter
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(),
        .created_by = "test"_string,
        .expires_at = {},  // No expiration
        .last_hit = {},    // No hits yet
    };

    auto create_result = pg.create_policy(policy);
    if (create_result.is_error()) {
        printf("❌ FAILED: Could not create URL pattern policy\n");
        return;
    }

    // Try to match by URL
    PolicyGraph::ThreatMetadata threat {
        .url = "http://evil.malicious.com/payload.exe"_string,
        .filename = "payload.exe"_string,
        .file_hash = "different_hash"_string,
        .mime_type = "application/octet-stream"_string,
        .file_size = 2048,
        .rule_name = "Malicious_Domain"_string,
        .severity = "critical"_string,
    };

    auto match_result = pg.match_policy(threat);
    if (match_result.is_error()) {
        printf("❌ FAILED: Could not match by URL\n");
        return;
    }

    auto matched = match_result.release_value();
    if (matched.has_value()) {
        printf("✅ PASSED: Matched policy ID %ld by URL pattern\n", matched->id);
        printf("  Pattern: %s, Action: Block\n",
               matched->url_pattern.has_value() ? matched->url_pattern->bytes_as_string_view().characters_without_null_termination() : "none");
    } else {
        printf("❌ FAILED: URL pattern did not match\n");
    }
}

static void test_match_policy_by_rule(PolicyGraph& pg)
{
    printf("\n=== Test: Match Policy by Rule Name ===\n");

    // Create a policy for specific rule (no hash or URL)
    PolicyGraph::Policy policy {
        .rule_name = "Windows_PE_Suspicious"_string,
        .url_pattern = {},  // No URL filter
        .file_hash = {},    // No hash filter
        .mime_type = {},    // No MIME type filter
        .action = PolicyGraph::PolicyAction::Quarantine,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(),
        .created_by = "test"_string,
        .expires_at = {},  // No expiration
        .last_hit = {},    // No hits yet
    };

    auto create_result = pg.create_policy(policy);
    if (create_result.is_error()) {
        printf("❌ FAILED: Could not create rule-based policy\n");
        return;
    }

    // Try to match by rule name
    PolicyGraph::ThreatMetadata threat {
        .url = "http://anywhere.com/program.exe"_string,
        .filename = "program.exe"_string,
        .file_hash = "yet_another_hash"_string,
        .mime_type = "application/x-msdownload"_string,
        .file_size = 4096,
        .rule_name = "Windows_PE_Suspicious"_string,
        .severity = "medium"_string,
    };

    auto match_result = pg.match_policy(threat);
    if (match_result.is_error()) {
        printf("❌ FAILED: Could not match by rule name\n");
        return;
    }

    auto matched = match_result.release_value();
    if (matched.has_value()) {
        printf("✅ PASSED: Matched policy ID %ld by rule name\n", matched->id);
        printf("  Rule: %s, Action: Quarantine\n",
               matched->rule_name.bytes_as_string_view().characters_without_null_termination());
    } else {
        printf("❌ FAILED: Rule name did not match\n");
    }
}

static void test_record_threat(PolicyGraph& pg)
{
    printf("\n=== Test: Record Threat History ===\n");

    PolicyGraph::ThreatMetadata threat {
        .url = "http://test.com/threat.exe"_string,
        .filename = "threat.exe"_string,
        .file_hash = "threat_hash_123"_string,
        .mime_type = "application/x-msdos-program"_string,
        .file_size = 8192,
        .rule_name = "Test_Threat"_string,
        .severity = "high"_string,
    };

    auto result = pg.record_threat(threat, "blocked"_string, {}, "{\"test\":\"data\"}"_string);
    if (result.is_error()) {
        printf("❌ FAILED: Could not record threat\n");
        return;
    }

    printf("✅ PASSED: Recorded threat to history\n");
}

static void test_get_threat_history(PolicyGraph& pg)
{
    printf("\n=== Test: Get Threat History ===\n");

    auto result = pg.get_threat_history({});
    if (result.is_error()) {
        printf("❌ FAILED: Could not get threat history\n");
        return;
    }

    auto threats = result.release_value();
    printf("✅ PASSED: Retrieved %zu threat records\n", threats.size());

    for (auto const& threat : threats) {
        printf("  - %s from %s: %s (action: %s)\n",
               threat.filename.bytes_as_string_view().characters_without_null_termination(),
               threat.url.bytes_as_string_view().characters_without_null_termination(),
               threat.rule_name.bytes_as_string_view().characters_without_null_termination(),
               threat.action_taken.bytes_as_string_view().characters_without_null_termination());
    }
}

static void test_policy_statistics(PolicyGraph& pg)
{
    printf("\n=== Test: Policy Statistics ===\n");

    auto policy_count = pg.get_policy_count();
    auto threat_count = pg.get_threat_count();

    if (policy_count.is_error() || threat_count.is_error()) {
        printf("❌ FAILED: Could not get statistics\n");
        return;
    }

    printf("✅ PASSED: Statistics retrieved\n");
    printf("  Total policies: %lu\n", policy_count.release_value());
    printf("  Total threats: %lu\n", threat_count.release_value());
}

static void test_network_behavior_policies(PolicyGraph& pg)
{
    printf("\n=== Test: Network Behavior Policies (Milestone 0.4 Phase 6) ===\n");

    // Test 1: Create a network behavior policy
    auto create_result = pg.create_network_behavior_policy(
        "suspicious-domain.com"_string,
        "block"_string,
        "dga"_string,
        850,  // 0.85 * 1000
        "Detected DGA pattern"_string
    );

    if (create_result.is_error()) {
        printf("❌ FAILED: Could not create network behavior policy: %s\n",
               create_result.error().string_literal().characters_without_null_termination());
        return;
    }

    auto policy_id = create_result.release_value();
    printf("✅ PASSED: Created network behavior policy with ID %ld\n", policy_id);

    // Test 2: Retrieve the policy
    auto get_result = pg.get_network_behavior_policy("suspicious-domain.com"_string, "dga"_string);
    if (get_result.is_error()) {
        printf("❌ FAILED: Could not retrieve network behavior policy\n");
        return;
    }

    auto policy_opt = get_result.release_value();
    if (!policy_opt.has_value()) {
        printf("❌ FAILED: Policy not found\n");
        return;
    }

    auto const& policy = policy_opt.value();
    printf("✅ PASSED: Retrieved policy - Domain: %s, Policy: %s, Threat: %s, Confidence: %d/1000 (%.2f)\n",
           policy.domain.bytes_as_string_view().characters_without_null_termination(),
           policy.policy.bytes_as_string_view().characters_without_null_termination(),
           policy.threat_type.bytes_as_string_view().characters_without_null_termination(),
           policy.confidence,
           policy.confidence / 1000.0f);

    // Test 3: Update the policy
    auto update_result = pg.update_network_behavior_policy(policy_id, "monitor"_string, "Changed to monitoring mode"_string);
    if (update_result.is_error()) {
        printf("❌ FAILED: Could not update network behavior policy\n");
        return;
    }
    printf("✅ PASSED: Updated policy to 'monitor'\n");

    // Test 4: Create another policy for different threat type
    auto create_result2 = pg.create_network_behavior_policy(
        "c2-server.net"_string,
        "block"_string,
        "c2_beaconing"_string,
        920,  // 0.92 * 1000
        "Beaconing pattern detected"_string
    );

    if (create_result2.is_error()) {
        printf("❌ FAILED: Could not create second network behavior policy\n");
        return;
    }
    printf("✅ PASSED: Created second policy (C2 beaconing)\n");

    // Test 5: List all policies
    auto list_result = pg.get_all_network_behavior_policies();
    if (list_result.is_error()) {
        printf("❌ FAILED: Could not list network behavior policies\n");
        return;
    }

    auto policies = list_result.release_value();
    printf("✅ PASSED: Listed %zu network behavior policies:\n", policies.size());
    for (auto const& p : policies) {
        printf("  - ID: %ld, Domain: %s, Policy: %s, Threat: %s, Confidence: %d/1000\n",
               p.id,
               p.domain.bytes_as_string_view().characters_without_null_termination(),
               p.policy.bytes_as_string_view().characters_without_null_termination(),
               p.threat_type.bytes_as_string_view().characters_without_null_termination(),
               p.confidence);
    }

    // Test 6: Delete a policy
    auto delete_result = pg.delete_network_behavior_policy(policy_id);
    if (delete_result.is_error()) {
        printf("❌ FAILED: Could not delete network behavior policy\n");
        return;
    }
    printf("✅ PASSED: Deleted network behavior policy %ld\n", policy_id);

    // Test 7: Verify deletion
    auto verify_result = pg.get_network_behavior_policy("suspicious-domain.com"_string, "dga"_string);
    if (verify_result.is_error()) {
        printf("❌ FAILED: Error verifying deletion\n");
        return;
    }

    if (verify_result.value().has_value()) {
        printf("❌ FAILED: Policy still exists after deletion\n");
        return;
    }
    printf("✅ PASSED: Verified policy deletion\n");
}

// Milestone 0.5 Phase 1d: Sandbox Verdict Cache Tests

static void test_store_and_lookup_verdict(PolicyGraph& pg)
{
    printf("\n=== Test: Store and Lookup Verdict ===\n");

    // Create a test verdict
    PolicyGraph::SandboxVerdict verdict {
        .file_hash = "a1b2c3d4e5f6789012345678901234567890123456789012345678901234abcd"_string,
        .threat_level = 2, // Malicious
        .confidence = 850,
        .composite_score = 780,
        .verdict_explanation = "Detected malware: Zeus trojan variant"_string,
        .yara_score = 900,
        .ml_score = 750,
        .behavioral_score = 690,
        .triggered_rules = { "Zeus_Variant_v1"_string, "Trojan_Generic"_string },
        .detected_behaviors = { "network_beacon"_string, "registry_modification"_string, "process_injection"_string },
        .analyzed_at = UnixDateTime::now(),
        .expires_at = UnixDateTime::now() + PolicyGraph::calculate_verdict_ttl(2) // 90 days for malicious
    };

    // Store the verdict
    auto store_result = pg.store_sandbox_verdict(verdict);
    if (store_result.is_error()) {
        printf("❌ FAILED: Could not store verdict: %s\n", store_result.error().string_literal().characters_without_null_termination());
        return;
    }
    printf("✅ PASSED: Stored verdict for hash %s\n", verdict.file_hash.bytes_as_string_view().characters_without_null_termination());

    // Lookup the verdict
    auto lookup_result = pg.lookup_sandbox_verdict(verdict.file_hash);
    if (lookup_result.is_error()) {
        printf("❌ FAILED: Could not lookup verdict: %s\n", lookup_result.error().string_literal().characters_without_null_termination());
        return;
    }

    auto cached = lookup_result.value();
    if (!cached.has_value()) {
        printf("❌ FAILED: Verdict not found in cache\n");
        return;
    }

    // Verify all fields match
    auto const& v = cached.value();
    bool matches = v.file_hash == verdict.file_hash &&
                   v.threat_level == verdict.threat_level &&
                   v.confidence == verdict.confidence &&
                   v.composite_score == verdict.composite_score &&
                   v.verdict_explanation == verdict.verdict_explanation &&
                   v.yara_score == verdict.yara_score &&
                   v.ml_score == verdict.ml_score &&
                   v.behavioral_score == verdict.behavioral_score &&
                   v.triggered_rules.size() == verdict.triggered_rules.size() &&
                   v.detected_behaviors.size() == verdict.detected_behaviors.size();

    if (!matches) {
        printf("❌ FAILED: Cached verdict does not match stored verdict\n");
        return;
    }

    printf("✅ PASSED: Retrieved verdict matches stored data (threat_level: %d, rules: %zu, behaviors: %zu)\n",
           v.threat_level, v.triggered_rules.size(), v.detected_behaviors.size());
}

static void test_expired_verdict_returns_empty(PolicyGraph& pg)
{
    printf("\n=== Test: Expired Verdict Returns Empty ===\n");

    // Create a verdict that's already expired
    PolicyGraph::SandboxVerdict verdict {
        .file_hash = "expired123456789012345678901234567890123456789012345678901234abcd"_string,
        .threat_level = 1, // Suspicious
        .confidence = 600,
        .composite_score = 550,
        .verdict_explanation = "Expired test verdict"_string,
        .yara_score = 500,
        .ml_score = 600,
        .behavioral_score = 550,
        .triggered_rules = { "Test_Rule"_string },
        .detected_behaviors = { "test_behavior"_string },
        .analyzed_at = UnixDateTime::now() - AK::Duration::from_seconds(86400), // 1 day ago
        .expires_at = UnixDateTime::now() - AK::Duration::from_seconds(3600) // Expired 1 hour ago
    };

    // Store the expired verdict
    auto store_result = pg.store_sandbox_verdict(verdict);
    if (store_result.is_error()) {
        printf("❌ FAILED: Could not store verdict\n");
        return;
    }

    // Try to lookup - should return empty because it's expired
    auto lookup_result = pg.lookup_sandbox_verdict(verdict.file_hash);
    if (lookup_result.is_error()) {
        printf("❌ FAILED: Lookup error: %s\n", lookup_result.error().string_literal().characters_without_null_termination());
        return;
    }

    if (lookup_result.value().has_value()) {
        printf("❌ FAILED: Expired verdict was returned (should be empty)\n");
        return;
    }

    printf("✅ PASSED: Expired verdict correctly returns empty\n");
}

static void test_invalidate_verdict(PolicyGraph& pg)
{
    printf("\n=== Test: Invalidate Verdict ===\n");

    // Create and store a verdict
    PolicyGraph::SandboxVerdict verdict {
        .file_hash = "invalidate12345678901234567890123456789012345678901234567890abcd"_string,
        .threat_level = 0, // Clean
        .confidence = 950,
        .composite_score = 100,
        .verdict_explanation = "Clean file - no threats detected"_string,
        .yara_score = 0,
        .ml_score = 50,
        .behavioral_score = 150,
        .triggered_rules = {},
        .detected_behaviors = {},
        .analyzed_at = UnixDateTime::now(),
        .expires_at = UnixDateTime::now() + PolicyGraph::calculate_verdict_ttl(0) // 30 days
    };

    auto store_result = pg.store_sandbox_verdict(verdict);
    if (store_result.is_error()) {
        printf("❌ FAILED: Could not store verdict\n");
        return;
    }

    // Verify it's in cache
    auto lookup_before = pg.lookup_sandbox_verdict(verdict.file_hash);
    if (lookup_before.is_error() || !lookup_before.value().has_value()) {
        printf("❌ FAILED: Verdict not found before invalidation\n");
        return;
    }

    // Invalidate it
    auto invalidate_result = pg.invalidate_verdict(verdict.file_hash);
    if (invalidate_result.is_error()) {
        printf("❌ FAILED: Could not invalidate verdict\n");
        return;
    }

    // Verify it's gone from cache
    auto lookup_after = pg.lookup_sandbox_verdict(verdict.file_hash);
    if (lookup_after.is_error() || lookup_after.value().has_value()) {
        printf("❌ FAILED: Verdict still exists after invalidation\n");
        return;
    }

    printf("✅ PASSED: Verdict successfully invalidated\n");
}

static void test_ttl_by_threat_level([[maybe_unused]] PolicyGraph& pg)
{
    printf("\n=== Test: TTL Varies by Threat Level ===\n");

    struct ThreatLevelTest {
        i32 level;
        char const* name;
        i64 expected_seconds;
    };

    ThreatLevelTest tests[] = {
        { 0, "Clean", 2592000 },      // 30 days
        { 1, "Suspicious", 604800 },   // 7 days
        { 2, "Malicious", 7776000 },   // 90 days
        { 3, "Critical", 31536000 },   // 365 days
    };

    bool all_passed = true;
    for (auto const& test : tests) {
        auto ttl = PolicyGraph::calculate_verdict_ttl(test.level);
        auto seconds = ttl.to_seconds();

        if (seconds != test.expected_seconds) {
            printf("❌ FAILED: %s (level %d) TTL is %ld seconds, expected %ld\n",
                   test.name, test.level, static_cast<long>(seconds), static_cast<long>(test.expected_seconds));
            all_passed = false;
        } else {
            printf("  ✅ %s (level %d): %ld seconds (%ld days)\n",
                   test.name, test.level, static_cast<long>(seconds), static_cast<long>(seconds / 86400));
        }
    }

    if (all_passed) {
        printf("✅ PASSED: All TTL calculations correct\n");
    }
}

static void test_clear_verdict_cache(PolicyGraph& pg)
{
    printf("\n=== Test: Clear Verdict Cache ===\n");

    // Store multiple verdicts
    int count = 3;
    for (int i = 0; i < count; i++) {
        auto hash = String::formatted("cleartest{}456789012345678901234567890123456789012345678901abcd", i);
        if (hash.is_error()) {
            printf("❌ FAILED: Could not format hash\n");
            return;
        }

        PolicyGraph::SandboxVerdict verdict {
            .file_hash = hash.release_value(),
            .threat_level = i,
            .confidence = 800,
            .composite_score = 700,
            .verdict_explanation = "Test verdict for clear cache"_string,
            .yara_score = 600,
            .ml_score = 700,
            .behavioral_score = 800,
            .triggered_rules = {},
            .detected_behaviors = {},
            .analyzed_at = UnixDateTime::now(),
            .expires_at = UnixDateTime::now() + AK::Duration::from_seconds(86400)
        };

        auto store_result = pg.store_sandbox_verdict(verdict);
        if (store_result.is_error()) {
            printf("❌ FAILED: Could not store verdict %d\n", i);
            return;
        }
    }
    printf("  Stored %d test verdicts\n", count);

    // Clear the cache
    auto clear_result = pg.clear_verdict_cache();
    if (clear_result.is_error()) {
        printf("❌ FAILED: Could not clear cache\n");
        return;
    }

    // Verify all verdicts are gone
    bool all_cleared = true;
    for (int i = 0; i < count; i++) {
        auto hash = String::formatted("cleartest{}456789012345678901234567890123456789012345678901abcd", i);
        if (hash.is_error())
            continue;

        auto lookup = pg.lookup_sandbox_verdict(hash.value());
        if (lookup.is_error() || lookup.value().has_value()) {
            all_cleared = false;
            break;
        }
    }

    if (!all_cleared) {
        printf("❌ FAILED: Some verdicts still exist after clear\n");
        return;
    }

    printf("✅ PASSED: All verdicts cleared from cache\n");
}

int main()
{
    printf("====================================\n");
    printf("  PolicyGraph Integration Tests\n");
    printf("====================================\n");

    // Create PolicyGraph with test database
    auto db_path = "/tmp/sentinel_test"sv;
    auto pg_result = PolicyGraph::create(db_path);

    if (pg_result.is_error()) {
        printf("\n❌ FATAL: Could not create PolicyGraph: %s\n",
               pg_result.error().string_literal().characters_without_null_termination());
        return 1;
    }

    auto& pg = *pg_result.value();
    printf("✅ PolicyGraph initialized at %s\n", db_path.characters_without_null_termination());

    // Run all tests
    test_create_policy(pg);
    test_list_policies(pg);
    test_match_policy_by_hash(pg);
    test_match_policy_by_url(pg);
    test_match_policy_by_rule(pg);
    test_record_threat(pg);
    test_get_threat_history(pg);
    test_policy_statistics(pg);
    test_network_behavior_policies(pg);

    // Milestone 0.5 Phase 1d: Verdict cache tests
    test_store_and_lookup_verdict(pg);
    test_expired_verdict_returns_empty(pg);
    test_invalidate_verdict(pg);
    test_ttl_by_threat_level(pg);
    test_clear_verdict_cache(pg);

    printf("\n====================================\n");
    printf("  All Tests Complete!\n");
    printf("====================================\n");
    printf("\nDatabase location: %s/policy_graph.db\n", db_path.characters_without_null_termination());

    return 0;
}
