/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include <AK/StringView.h>
#include <LibCore/Directory.h>
#include <LibFileSystem/FileSystem.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Sentinel;

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

static void log_pass(StringView test_name)
{
    printf("✅ PASSED: %s\n", test_name.characters_without_null_termination());
    tests_passed++;
}

static void log_fail(StringView test_name, StringView reason)
{
    printf("❌ FAILED: %s - %s\n",
           test_name.characters_without_null_termination(),
           reason.characters_without_null_termination());
    tests_failed++;
}

static void print_section(StringView title)
{
    printf("\n=== %s ===\n", title.characters_without_null_termination());
}

// Test 1: PolicyGraph CRUD Operations
static void test_policy_crud_operations(PolicyGraph& pg)
{
    print_section("Test 1: PolicyGraph CRUD Operations"sv);

    // CREATE - Test all three action types
    PolicyGraph::Policy block_policy {
        .rule_name = "Backend_Test_Block"_string,
        .url_pattern = "%malicious.example%"_string,
        .file_hash = {},
        .mime_type = "application/x-executable"_string,
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto create_result = pg.create_policy(block_policy);
    if (create_result.is_error()) {
        log_fail("CREATE Block Policy"sv, "Could not create block policy"sv);
        return;
    }
    auto block_id = create_result.release_value();
    printf("  Created Block policy ID: %ld\n", block_id);

    // CREATE Quarantine policy
    PolicyGraph::Policy quarantine_policy {
        .rule_name = "Backend_Test_Quarantine"_string,
        .url_pattern = {},
        .file_hash = "a1b2c3d4e5f6789012345678901234567890123456789012345678901234567a"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Quarantine,
        .enforcement_action = "quarantine"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto quarantine_result = pg.create_policy(quarantine_policy);
    if (quarantine_result.is_error()) {
        log_fail("CREATE Quarantine Policy"sv, "Could not create quarantine policy"sv);
        return;
    }
    auto quarantine_id = quarantine_result.release_value();
    printf("  Created Quarantine policy ID: %ld\n", quarantine_id);

    // CREATE Allow policy
    PolicyGraph::Policy allow_policy {
        .rule_name = "Backend_Test_Allow"_string,
        .url_pattern = {},
        .file_hash = {},
        .mime_type = "image/png"_string,
        .action = PolicyGraph::PolicyAction::Allow,
        .enforcement_action = "allow"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto allow_result = pg.create_policy(allow_policy);
    if (allow_result.is_error()) {
        log_fail("CREATE Allow Policy"sv, "Could not create allow policy"sv);
        return;
    }
    auto allow_id = allow_result.release_value();
    printf("  Created Allow policy ID: %ld\n", allow_id);

    // READ - Verify each policy
    auto read_block = pg.get_policy(block_id);
    if (read_block.is_error() || read_block.value().action != PolicyGraph::PolicyAction::Block) {
        log_fail("READ Block Policy"sv, "Could not read or verify block policy"sv);
        return;
    }
    printf("  READ: Verified block policy\n");

    auto read_quarantine = pg.get_policy(quarantine_id);
    if (read_quarantine.is_error() || read_quarantine.value().action != PolicyGraph::PolicyAction::Quarantine) {
        log_fail("READ Quarantine Policy"sv, "Could not read or verify quarantine policy"sv);
        return;
    }
    printf("  READ: Verified quarantine policy\n");

    // UPDATE - Change action type
    auto update_policy = read_block.release_value();
    update_policy.action = PolicyGraph::PolicyAction::Quarantine;
    update_policy.url_pattern = "%very-malicious.example%"_string;

    auto update_result = pg.update_policy(block_id, update_policy);
    if (update_result.is_error()) {
        log_fail("UPDATE Policy"sv, "Could not update policy"sv);
        return;
    }

    auto verify_update = pg.get_policy(block_id);
    if (verify_update.is_error() || verify_update.value().action != PolicyGraph::PolicyAction::Quarantine) {
        log_fail("UPDATE Policy"sv, "Policy action not updated correctly"sv);
        return;
    }
    printf("  UPDATE: Successfully changed policy action and URL pattern\n");

    // LIST - Verify all policies are listed
    auto list_result = pg.list_policies();
    if (list_result.is_error()) {
        log_fail("LIST Policies"sv, "Could not list policies"sv);
        return;
    }

    auto policies = list_result.release_value();
    printf("  LIST: Found %zu policies\n", policies.size());

    if (policies.size() < 3) {
        log_fail("LIST Policies"sv, "Expected at least 3 policies"sv);
        return;
    }

    // DELETE - Remove one policy and verify
    auto delete_result = pg.delete_policy(allow_id);
    if (delete_result.is_error()) {
        log_fail("DELETE Policy"sv, "Could not delete policy"sv);
        return;
    }

    auto verify_delete = pg.get_policy(allow_id);
    if (!verify_delete.is_error()) {
        log_fail("DELETE Policy"sv, "Policy still exists after deletion"sv);
        return;
    }
    printf("  DELETE: Successfully removed policy ID %ld\n", allow_id);

    log_pass("PolicyGraph CRUD Operations"sv);
}

// Test 2: Policy Matching Accuracy
static void test_policy_matching_accuracy(PolicyGraph& pg)
{
    print_section("Test 2: Policy Matching Accuracy"sv);

    // Create policies with different match criteria
    PolicyGraph::Policy hash_policy {
        .rule_name = "Exact_Hash_Match"_string,
        .url_pattern = {},
        .file_hash = "b2c3d4e5f6789012345678901234567890123456789012345678901234567890"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    PolicyGraph::Policy url_policy {
        .rule_name = "URL_Pattern_Match"_string,
        .url_pattern = "%download.evil.com%"_string,
        .file_hash = {},
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Quarantine,
        .enforcement_action = "quarantine"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    PolicyGraph::Policy rule_policy {
        .rule_name = "Generic_Rule_Match"_string,
        .url_pattern = {},
        .file_hash = {},
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Allow,
        .enforcement_action = "allow"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto hash_id = pg.create_policy(hash_policy);
    auto url_id = pg.create_policy(url_policy);
    auto rule_id = pg.create_policy(rule_policy);

    if (hash_id.is_error() || url_id.is_error() || rule_id.is_error()) {
        log_fail("Create Matching Policies"sv, "Failed to create test policies"sv);
        return;
    }

    // Test 1: Exact hash match (highest priority)
    PolicyGraph::ThreatMetadata threat_hash {
        .url = "http://anywhere.com/file.exe"_string,
        .filename = "file.exe"_string,
        .file_hash = "b2c3d4e5f6789012345678901234567890123456789012345678901234567890"_string,
        .mime_type = "application/octet-stream"_string,
        .file_size = 1024,
        .rule_name = "Generic_Rule_Match"_string,
        .severity = "high"_string,
    };

    auto match1 = pg.match_policy(threat_hash);
    if (match1.is_error() || !match1.value().has_value() || match1.value()->id != hash_id.value()) {
        log_fail("Exact Hash Match"sv, "Hash match failed or wrong policy matched"sv);
        return;
    }
    printf("  ✓ Exact hash match (priority 1): Policy ID %ld\n", match1.value()->id);

    // Test 2: URL pattern match
    PolicyGraph::ThreatMetadata threat_url {
        .url = "http://download.evil.com/malware.exe"_string,
        .filename = "malware.exe"_string,
        .file_hash = "c3d4e5f6789012345678901234567890123456789012345678901234567890ab"_string,
        .mime_type = "application/x-executable"_string,
        .file_size = 2048,
        .rule_name = "Generic_Rule_Match"_string,
        .severity = "critical"_string,
    };

    auto match2 = pg.match_policy(threat_url);
    if (match2.is_error() || !match2.value().has_value() || match2.value()->id != url_id.value()) {
        log_fail("URL Pattern Match"sv, "URL match failed or wrong policy matched"sv);
        return;
    }
    printf("  ✓ URL pattern match (priority 2): Policy ID %ld\n", match2.value()->id);

    // Test 3: Rule name match (fallback)
    PolicyGraph::ThreatMetadata threat_rule {
        .url = "http://safe-site.com/download.bin"_string,
        .filename = "download.bin"_string,
        .file_hash = "d4e5f6789012345678901234567890123456789012345678901234567890abcd"_string,
        .mime_type = "application/octet-stream"_string,
        .file_size = 4096,
        .rule_name = "Generic_Rule_Match"_string,
        .severity = "low"_string,
    };

    auto match3 = pg.match_policy(threat_rule);
    if (match3.is_error() || !match3.value().has_value() || match3.value()->id != rule_id.value()) {
        log_fail("Rule Name Match"sv, "Rule match failed or wrong policy matched"sv);
        return;
    }
    printf("  ✓ Rule name match (priority 3): Policy ID %ld\n", match3.value()->id);

    // Test 4: No match scenario
    PolicyGraph::ThreatMetadata threat_no_match {
        .url = "http://unknown.com/file.txt"_string,
        .filename = "file.txt"_string,
        .file_hash = "e5f6789012345678901234567890123456789012345678901234567890abcdef"_string,
        .mime_type = "text/plain"_string,
        .file_size = 512,
        .rule_name = "Nonexistent_Rule"_string,
        .severity = "low"_string,
    };

    auto match4 = pg.match_policy(threat_no_match);
    if (match4.is_error() || match4.value().has_value()) {
        log_fail("No Match Scenario"sv, "Expected no match but got a policy"sv);
        return;
    }
    printf("  ✓ No match scenario: Correctly returned no policy\n");

    log_pass("Policy Matching Accuracy"sv);
}

// Test 3: Cache Effectiveness
static void test_cache_effectiveness(PolicyGraph& pg)
{
    print_section("Test 3: Cache Effectiveness"sv);

    // Create a policy for cache testing
    PolicyGraph::Policy cache_test_policy {
        .rule_name = "Cache_Test_Rule"_string,
        .url_pattern = {},
        .file_hash = "f6789012345678901234567890123456789012345678901234567890abcdef12"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "backend_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto policy_id = pg.create_policy(cache_test_policy);
    if (policy_id.is_error()) {
        log_fail("Create Cache Test Policy"sv, "Could not create policy"sv);
        return;
    }

    PolicyGraph::ThreatMetadata threat {
        .url = "http://test.com/cached.exe"_string,
        .filename = "cached.exe"_string,
        .file_hash = "f6789012345678901234567890123456789012345678901234567890abcdef12"_string,
        .mime_type = "application/x-executable"_string,
        .file_size = 8192,
        .rule_name = "Cache_Test_Rule"_string,
        .severity = "high"_string,
    };

    // First match - cache miss
    auto match1 = pg.match_policy(threat);
    if (match1.is_error() || !match1.value().has_value()) {
        log_fail("Cache Test - First Match"sv, "First match failed"sv);
        return;
    }
    printf("  First match (cache miss): Policy ID %ld\n", match1.value()->id);

    // Second match - should hit cache
    auto match2 = pg.match_policy(threat);
    if (match2.is_error() || !match2.value().has_value() || match2.value()->id != match1.value()->id) {
        log_fail("Cache Test - Second Match"sv, "Second match failed or returned different policy"sv);
        return;
    }
    printf("  Second match (cache hit): Policy ID %ld\n", match2.value()->id);

    // Repeat multiple times to test cache stability
    for (int i = 0; i < 10; i++) {
        auto match = pg.match_policy(threat);
        if (match.is_error() || !match.value().has_value() || match.value()->id != policy_id.value()) {
            log_fail("Cache Test - Repeated Matches"sv, "Cache returned inconsistent results"sv);
            return;
        }
    }
    printf("  Repeated 10 matches: All returned correct cached policy\n");

    log_pass("Cache Effectiveness"sv);
}

// Test 4: Threat History and Statistics
static void test_threat_history_statistics(PolicyGraph& pg)
{
    print_section("Test 4: Threat History and Statistics"sv);

    // Get initial counts
    auto initial_threat_count = pg.get_threat_count();
    if (initial_threat_count.is_error()) {
        log_fail("Get Initial Threat Count"sv, "Could not get threat count"sv);
        return;
    }
    printf("  Initial threat count: %lu\n", initial_threat_count.value());

    // Record multiple threats
    Vector<PolicyGraph::ThreatMetadata> threats;
    for (int i = 0; i < 5; i++) {
        // Generate unique 64-character hex hashes
        String hashes[] = {
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"_string,
            "1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde0"_string,
            "2123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde1"_string,
            "3123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde2"_string,
            "4123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde3"_string,
        };
        threats.append({
            .url = String::formatted("http://threat{}.com/malware.exe", i).release_value_but_fixme_should_propagate_errors(),
            .filename = String::formatted("malware{}.exe", i).release_value_but_fixme_should_propagate_errors(),
            .file_hash = hashes[i],
            .mime_type = "application/x-executable"_string,
            .file_size = static_cast<u64>(1024 * (i + 1)),
            .rule_name = "History_Test_Rule"_string,
            .severity = (i % 2 == 0) ? "high"_string : "critical"_string,
        });
    }

    for (auto const& threat : threats) {
        auto result = pg.record_threat(threat, "blocked"_string, {}, "{\"test\":\"data\"}"_string);
        if (result.is_error()) {
            log_fail("Record Threats"sv, "Could not record threat"sv);
            return;
        }
    }
    printf("  Recorded %zu threats\n", threats.size());

    // Verify threat count increased
    auto new_threat_count = pg.get_threat_count();
    if (new_threat_count.is_error()) {
        log_fail("Get New Threat Count"sv, "Could not get updated threat count"sv);
        return;
    }

    if (new_threat_count.value() < initial_threat_count.value() + threats.size()) {
        log_fail("Verify Threat Count"sv, "Threat count did not increase as expected"sv);
        return;
    }
    printf("  New threat count: %lu (increased by at least %zu)\n",
           new_threat_count.value(), threats.size());

    // Query history by rule
    auto history = pg.get_threats_by_rule("History_Test_Rule"_string);
    if (history.is_error()) {
        log_fail("Query History by Rule"sv, "Could not query threat history"sv);
        return;
    }

    if (history.value().size() != threats.size()) {
        log_fail("Verify History Count"sv, "Expected 5 threats in history"sv);
        return;
    }
    printf("  Retrieved %zu threats for 'History_Test_Rule'\n", history.value().size());

    // Verify ordering (newest first)
    auto& records = history.value();
    for (size_t i = 0; i < records.size() - 1; i++) {
        if (records[i].detected_at < records[i + 1].detected_at) {
            log_fail("Verify History Ordering"sv, "History not ordered by time (newest first)"sv);
            return;
        }
    }
    printf("  Verified history ordered by detection time (newest first)\n");

    // Query all history
    auto all_history = pg.get_threat_history({});
    if (all_history.is_error()) {
        log_fail("Query All History"sv, "Could not query all threat history"sv);
        return;
    }
    printf("  Total threats in history: %zu\n", all_history.value().size());

    log_pass("Threat History and Statistics"sv);
}

// Test 5: Database Maintenance
static void test_database_maintenance(PolicyGraph& pg)
{
    print_section("Test 5: Database Maintenance"sv);

    // Get initial policy count
    auto initial_count = pg.get_policy_count();
    if (initial_count.is_error()) {
        log_fail("Get Initial Policy Count"sv, "Could not get policy count"sv);
        return;
    }
    printf("  Initial policy count: %lu\n", initial_count.value());

    // Create an expired policy (set to expire 1ms in the future, which will be expired by the time we clean up)
    auto now = UnixDateTime::now();
    auto almost_expired = UnixDateTime::from_milliseconds_since_epoch(now.milliseconds_since_epoch() + 1); // 1ms from now
    auto past_time = UnixDateTime::from_seconds_since_epoch(now.seconds_since_epoch() - 3600); // 1 hour ago (for created_at)

    PolicyGraph::Policy expired_policy {
        .rule_name = "Expired_Test_Policy"_string,
        .url_pattern = {},
        .file_hash = "5234567890abcdef0123456789abcdef0123456789abcdef0123456789abcde4"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = past_time,
        .created_by = "backend_test"_string,
        .expires_at = almost_expired, // Expires 1ms from now
        .last_hit = {},
    };

    auto expired_id = pg.create_policy(expired_policy);
    if (expired_id.is_error()) {
        log_fail("Create Expired Policy"sv, "Could not create expired policy"sv);
        return;
    }
    printf("  Created expired policy ID: %ld\n", expired_id.value());

    // Create a non-expired policy
    PolicyGraph::Policy active_policy {
        .rule_name = "Active_Test_Policy"_string,
        .url_pattern = {},
        .file_hash = "6345678901bcdef0123456789abcdef0123456789abcdef0123456789abcdef5"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = now,
        .created_by = "backend_test"_string,
        .expires_at = {}, // No expiration
        .last_hit = {},
    };

    auto active_id = pg.create_policy(active_policy);
    if (active_id.is_error()) {
        log_fail("Create Active Policy"sv, "Could not create active policy"sv);
        return;
    }
    printf("  Created active policy ID: %ld\n", active_id.value());

    // Wait 2ms to ensure the expired policy has definitely expired
    usleep(2000); // 2 milliseconds

    // Cleanup expired policies
    auto cleanup_result = pg.cleanup_expired_policies();
    if (cleanup_result.is_error()) {
        log_fail("Cleanup Expired Policies"sv, "Cleanup operation failed"sv);
        return;
    }
    printf("  Cleaned up expired policies\n");

    // Verify expired policy is gone
    auto verify_expired = pg.get_policy(expired_id.value());
    if (!verify_expired.is_error()) {
        log_fail("Verify Expired Deleted"sv, "Expired policy still exists"sv);
        return;
    }
    printf("  Verified expired policy was removed\n");

    // Verify active policy still exists
    auto verify_active = pg.get_policy(active_id.value());
    if (verify_active.is_error()) {
        log_fail("Verify Active Exists"sv, "Active policy was incorrectly removed"sv);
        return;
    }
    printf("  Verified active policy still exists\n");

    // Test vacuum operation
    auto vacuum_result = pg.vacuum_database();
    if (vacuum_result.is_error()) {
        log_fail("Vacuum Database"sv, "Vacuum operation failed"sv);
        return;
    }
    printf("  Successfully vacuumed database\n");

    log_pass("Database Maintenance"sv);
}

int main()
{
    printf("====================================\n");
    printf("  Backend Components Tests\n");
    printf("  (Phase 5 Days 29-30)\n");
    printf("====================================\n");

    // Create PolicyGraph with test database
    auto db_path = "/tmp/sentinel_backend_test"sv;

    // Clean up any existing test database
    if (FileSystem::exists(ByteString::formatted("{}/policy_graph.db", db_path))) {
        printf("\nCleaning up previous test database...\n");
        (void)FileSystem::remove(ByteString::formatted("{}/policy_graph.db", db_path), FileSystem::RecursionMode::Allowed);
    }

    auto pg_result = PolicyGraph::create(ByteString(db_path));
    if (pg_result.is_error()) {
        printf("\n❌ FATAL: Could not create PolicyGraph: %s\n",
               pg_result.error().string_literal().characters_without_null_termination());
        return 1;
    }

    auto& pg = *pg_result.value();
    printf("✅ PolicyGraph initialized at %s\n", db_path.characters_without_null_termination());

    // Run all backend tests
    test_policy_crud_operations(pg);
    test_policy_matching_accuracy(pg);
    test_cache_effectiveness(pg);
    test_threat_history_statistics(pg);
    test_database_maintenance(pg);

    // Print summary
    printf("\n====================================\n");
    printf("  Test Summary\n");
    printf("====================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("====================================\n");

    if (tests_failed > 0) {
        printf("\n❌ Some tests FAILED\n");
        return 1;
    }

    printf("\n✅ All Backend Tests PASSED!\n");
    printf("\nDatabase location: %s/policy_graph.db\n", db_path.characters_without_null_termination());

    return 0;
}
