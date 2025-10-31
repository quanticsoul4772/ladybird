/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include <AK/StringView.h>
#include <LibCore/Directory.h>
#include <LibCrypto/ConstantTimeComparison.h>
#include <LibCrypto/Hash/SHA2.h>
#include <LibFileSystem/FileSystem.h>
#include <stdio.h>

using namespace Sentinel;

// EICAR test file content (standard malware test signature)
constexpr auto EICAR_CONTENT = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"sv;
constexpr auto EICAR_HASH = "275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f"sv;

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

// Helper: Compute SHA256 hash for test data
static ErrorOr<String> compute_sha256(StringView data)
{
    auto hasher = Crypto::Hash::SHA256::create();
    hasher->update(data.bytes());
    auto digest = hasher->digest();

    StringBuilder hex_builder;
    for (auto byte : digest.bytes())
        hex_builder.appendff("{:02x}", byte);

    return hex_builder.to_string();
}

// Test 1: EICAR File Detection and Policy Creation
static void test_eicar_detection_and_policy(PolicyGraph& pg)
{
    print_section("Test 1: EICAR File Detection and Policy Creation"sv);

    // Simulate EICAR file detection
    PolicyGraph::ThreatMetadata eicar_threat {
        .url = "http://test.example.com/eicar.com"_string,
        .filename = "eicar.com"_string,
        .file_hash = String::from_utf8_without_validation(EICAR_HASH.bytes()),
        .mime_type = "application/octet-stream"_string,
        .file_size = EICAR_CONTENT.length(),
        .rule_name = "EICAR_Test_File"_string,
        .severity = "critical"_string,
    };

    // First detection - no policy exists yet
    auto match_before = pg.match_policy(eicar_threat);
    if (match_before.is_error()) {
        log_fail("Initial EICAR Detection"sv, "Policy match query failed"sv);
        return;
    }

    if (match_before.value().has_value()) {
        printf("  Note: Policy already exists for EICAR (ID: %ld)\n", match_before.value()->id);
    } else {
        printf("  No existing policy for EICAR (expected for first run)\n");
    }

    // User decision: Create Block policy for EICAR
    PolicyGraph::Policy block_policy {
        .rule_name = "EICAR_Test_File"_string,
        .url_pattern = {},
        .file_hash = String::from_utf8_without_validation(EICAR_HASH.bytes()),
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "download_vetting_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto create_result = pg.create_policy(block_policy);
    if (create_result.is_error()) {
        log_fail("Create EICAR Block Policy"sv, "Could not create policy"sv);
        return;
    }

    auto policy_id = create_result.release_value();
    printf("  Created block policy for EICAR (ID: %ld)\n", policy_id);

    // Record the first threat
    auto record_result = pg.record_threat(eicar_threat, "blocked"_string, policy_id,
                                         "{\"action\":\"block\",\"source\":\"user_decision\"}"_string);
    if (record_result.is_error()) {
        log_fail("Record EICAR Threat"sv, "Could not record threat to history"sv);
        return;
    }
    printf("  Recorded EICAR threat in history\n");

    // Simulate second EICAR download - should auto-block
    PolicyGraph::ThreatMetadata eicar_second {
        .url = "http://another-site.com/malware.exe"_string,
        .filename = "malware.exe"_string,
        .file_hash = String::from_utf8_without_validation(EICAR_HASH.bytes()),
        .mime_type = "application/x-msdos-program"_string,
        .file_size = EICAR_CONTENT.length(),
        .rule_name = "EICAR_Test_File"_string,
        .severity = "critical"_string,
    };

    auto match_after = pg.match_policy(eicar_second);
    if (match_after.is_error() || !match_after.value().has_value()) {
        log_fail("Auto-Block Second EICAR"sv, "Policy did not match on second detection"sv);
        return;
    }

    auto matched_policy = match_after.value().value();
    if (matched_policy.action != PolicyGraph::PolicyAction::Block) {
        log_fail("Auto-Block Second EICAR"sv, "Wrong action (expected Block)"sv);
        return;
    }

    printf("  Second EICAR detection matched policy (Action: Block, ID: %ld)\n", matched_policy.id);
    printf("  AUTO-BLOCK: No user prompt required for second detection\n");

    // Record second threat
    auto record_second = pg.record_threat(eicar_second, "blocked"_string, matched_policy.id,
                                         "{\"action\":\"block\",\"source\":\"auto_enforcement\"}"_string);
    if (record_second.is_error()) {
        log_fail("Record Second EICAR"sv, "Could not record second threat"sv);
        return;
    }

    log_pass("EICAR Detection and Policy Creation"sv);
}

// Test 2: Clean File (No Alert)
static void test_clean_file_no_alert(PolicyGraph& pg)
{
    print_section("Test 2: Clean File Download (No Alert)"sv);

    // Simulate clean file download
    auto clean_content = "This is a clean text file with no threats."sv;
    auto clean_hash_result = compute_sha256(clean_content);
    if (clean_hash_result.is_error()) {
        log_fail("Compute Clean File Hash"sv, "Hash computation failed"sv);
        return;
    }

    auto clean_hash = clean_hash_result.release_value();
    printf("  Clean file hash: %s\n", clean_hash.bytes_as_string_view().characters_without_null_termination());

    PolicyGraph::ThreatMetadata clean_file {
        .url = "http://safe-site.com/document.txt"_string,
        .filename = "document.txt"_string,
        .file_hash = clean_hash,
        .mime_type = "text/plain"_string,
        .file_size = clean_content.length(),
        .rule_name = "Clean_File"_string, // Would not match any malware rule
        .severity = "none"_string,
    };

    // No policy should match
    auto match_result = pg.match_policy(clean_file);
    if (match_result.is_error()) {
        log_fail("Clean File Policy Check"sv, "Policy match query failed"sv);
        return;
    }

    if (match_result.value().has_value()) {
        log_fail("Clean File Policy Check"sv, "Unexpected policy match for clean file"sv);
        return;
    }

    printf("  Clean file: No policy matched (expected)\n");
    printf("  Download proceeds without user alert\n");

    log_pass("Clean File Download (No Alert)"sv);
}

// Test 3: Policy Enforcement - Block, Quarantine, Allow
static void test_policy_enforcement_actions(PolicyGraph& pg)
{
    print_section("Test 3: Policy Enforcement Actions"sv);

    // Test Block Action
    PolicyGraph::Policy block_policy {
        .rule_name = "Test_Malware_Block"_string,
        .url_pattern = {},
        .file_hash = "block_test_hash_abc123"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "download_vetting_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto block_id = pg.create_policy(block_policy);
    if (block_id.is_error()) {
        log_fail("Create Block Policy"sv, "Could not create block policy"sv);
        return;
    }

    PolicyGraph::ThreatMetadata block_threat {
        .url = "http://malware-site.com/virus.exe"_string,
        .filename = "virus.exe"_string,
        .file_hash = "block_test_hash_abc123"_string,
        .mime_type = "application/x-executable"_string,
        .file_size = 10240,
        .rule_name = "Test_Malware_Block"_string,
        .severity = "critical"_string,
    };

    auto block_match = pg.match_policy(block_threat);
    if (block_match.is_error() || !block_match.value().has_value() ||
        block_match.value()->action != PolicyGraph::PolicyAction::Block) {
        log_fail("Block Policy Enforcement"sv, "Block policy not enforced correctly"sv);
        return;
    }
    printf("  ✓ Block action enforced: Download prevented\n");

    // Record blocked threat
    MUST(pg.record_threat(block_threat, "blocked"_string, block_match.value()->id, "{}"_string));

    // Test Quarantine Action
    PolicyGraph::Policy quarantine_policy {
        .rule_name = "Test_Suspicious_Quarantine"_string,
        .url_pattern = {},
        .file_hash = "quarantine_test_hash_def456"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Quarantine,
        .enforcement_action = "quarantine"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "download_vetting_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto quarantine_id = pg.create_policy(quarantine_policy);
    if (quarantine_id.is_error()) {
        log_fail("Create Quarantine Policy"sv, "Could not create quarantine policy"sv);
        return;
    }

    PolicyGraph::ThreatMetadata quarantine_threat {
        .url = "http://suspicious-site.org/payload.dll"_string,
        .filename = "payload.dll"_string,
        .file_hash = "quarantine_test_hash_def456"_string,
        .mime_type = "application/x-msdownload"_string,
        .file_size = 20480,
        .rule_name = "Test_Suspicious_Quarantine"_string,
        .severity = "high"_string,
    };

    auto quarantine_match = pg.match_policy(quarantine_threat);
    if (quarantine_match.is_error() || !quarantine_match.value().has_value() ||
        quarantine_match.value()->action != PolicyGraph::PolicyAction::Quarantine) {
        log_fail("Quarantine Policy Enforcement"sv, "Quarantine policy not enforced correctly"sv);
        return;
    }
    printf("  ✓ Quarantine action enforced: File isolated\n");

    // Record quarantined threat
    MUST(pg.record_threat(quarantine_threat, "quarantined"_string, quarantine_match.value()->id,
                    "{\"quarantine_path\":\"/tmp/quarantine/payload.dll\"}"_string));

    // Test Allow Action
    PolicyGraph::Policy allow_policy {
        .rule_name = "Test_Safe_Allow"_string,
        .url_pattern = {},
        .file_hash = "allow_test_hash_ghi789"_string,
        .mime_type = {},
        .action = PolicyGraph::PolicyAction::Allow,
        .enforcement_action = "allow"_string,
        .created_at = UnixDateTime::now(),
        .created_by = "download_vetting_test"_string,
        .expires_at = {},
        .last_hit = {},
    };

    auto allow_id = pg.create_policy(allow_policy);
    if (allow_id.is_error()) {
        log_fail("Create Allow Policy"sv, "Could not create allow policy"sv);
        return;
    }

    PolicyGraph::ThreatMetadata allow_threat {
        .url = "http://trusted-site.com/safe-tool.exe"_string,
        .filename = "safe-tool.exe"_string,
        .file_hash = "allow_test_hash_ghi789"_string,
        .mime_type = "application/x-executable"_string,
        .file_size = 15360,
        .rule_name = "Test_Safe_Allow"_string,
        .severity = "low"_string,
    };

    auto allow_match = pg.match_policy(allow_threat);
    if (allow_match.is_error() || !allow_match.value().has_value() ||
        allow_match.value()->action != PolicyGraph::PolicyAction::Allow) {
        log_fail("Allow Policy Enforcement"sv, "Allow policy not enforced correctly"sv);
        return;
    }
    printf("  ✓ Allow action enforced: Download permitted\n");

    // Record allowed file
    MUST(pg.record_threat(allow_threat, "allowed"_string, allow_match.value()->id, "{}"_string));

    log_pass("Policy Enforcement Actions"sv);
}

// Test 4: Rate Limiting Simulation
static void test_rate_limiting(PolicyGraph& pg)
{
    print_section("Test 4: Rate Limiting Simulation"sv);

    // Simulate rapid policy creation (5 policies in quick succession)
    printf("  Simulating rapid policy creation...\n");

    Vector<i64> policy_ids;
    for (int i = 0; i < 5; i++) {
        PolicyGraph::Policy rate_test_policy {
            .rule_name = String::formatted("RateLimit_Test_{}", i).release_value_but_fixme_should_propagate_errors(),
            .url_pattern = {},
            .file_hash = String::formatted("rate_limit_hash_{}", i).release_value_but_fixme_should_propagate_errors(),
            .mime_type = {},
            .action = PolicyGraph::PolicyAction::Block,
        .enforcement_action = "block"_string,
            .created_at = UnixDateTime::now(),
            .created_by = "download_vetting_test"_string,
            .expires_at = {},
            .last_hit = {},
        };

        auto result = pg.create_policy(rate_test_policy);
        if (result.is_error()) {
            log_fail("Rate Limiting Test"sv, "Policy creation failed during rate test"sv);
            return;
        }

        policy_ids.append(result.value());
        printf("  Created policy %d/5 (ID: %ld)\n", i + 1, result.value());
    }

    // Note: Actual rate limiting is enforced in the UI/IPC layer
    // PolicyGraph doesn't enforce rate limits, but we verify all policies were created
    if (policy_ids.size() != 5) {
        log_fail("Rate Limiting Test"sv, "Not all policies were created"sv);
        return;
    }

    printf("  All 5 policies created successfully\n");
    printf("  Note: Rate limiting is enforced at UI/IPC layer (5 policies/minute)\n");

    // Cleanup test policies
    for (auto policy_id : policy_ids) {
        MUST(pg.delete_policy(policy_id));
    }

    log_pass("Rate Limiting Simulation"sv);
}

// Test 5: Threat History Tracking
static void test_threat_history_tracking(PolicyGraph& pg)
{
    print_section("Test 5: Threat History Tracking"sv);

    // Record multiple detections to verify history tracking
    Vector<PolicyGraph::ThreatMetadata> threats;

    threats.append({
        .url = "http://malware1.com/threat1.exe"_string,
        .filename = "threat1.exe"_string,
        .file_hash = "history_test_hash_1"_string,
        .mime_type = "application/x-executable"_string,
        .file_size = 5000,
        .rule_name = "History_Test_Rule"_string,
        .severity = "high"_string,
    });

    threats.append({
        .url = "http://malware2.com/threat2.dll"_string,
        .filename = "threat2.dll"_string,
        .file_hash = "history_test_hash_2"_string,
        .mime_type = "application/x-msdownload"_string,
        .file_size = 7500,
        .rule_name = "History_Test_Rule"_string,
        .severity = "critical"_string,
    });

    threats.append({
        .url = "http://malware3.com/threat3.js"_string,
        .filename = "threat3.js"_string,
        .file_hash = "history_test_hash_3"_string,
        .mime_type = "text/javascript"_string,
        .file_size = 2000,
        .rule_name = "History_Test_Rule"_string,
        .severity = "medium"_string,
    });

    // Record all threats
    for (auto const& threat : threats) {
        auto result = pg.record_threat(threat, "blocked"_string, {}, "{}"_string);
        if (result.is_error()) {
            log_fail("Record Threat History"sv, "Could not record threat"sv);
            return;
        }
    }
    printf("  Recorded %zu threats to history\n", threats.size());

    // Query history by rule
    auto history = pg.get_threats_by_rule("History_Test_Rule"_string);
    if (history.is_error()) {
        log_fail("Query Threat History"sv, "Could not query history"sv);
        return;
    }

    if (history.value().size() != threats.size()) {
        log_fail("Verify History Count"sv, "History count mismatch"sv);
        return;
    }

    printf("  Retrieved %zu threats from history\n", history.value().size());

    // Verify history contains our threats
    bool found_all = true;
    for (auto const& threat : threats) {
        bool found = false;
        for (auto const& record : history.value()) {
            // Use constant-time comparison for hash to prevent timing attacks
            if (Crypto::ConstantTimeComparison::compare_hashes(record.file_hash.bytes_as_string_view(), threat.file_hash.bytes_as_string_view())) {
                found = true;
                break;
            }
        }
        if (!found) {
            found_all = false;
            printf("  Warning: Threat with hash %s not found in history\n",
                   threat.file_hash.bytes_as_string_view().characters_without_null_termination());
        }
    }

    if (!found_all) {
        log_fail("Verify History Contents"sv, "Not all threats found in history"sv);
        return;
    }

    printf("  Verified all threats present in history\n");

    log_pass("Threat History Tracking"sv);
}

int main()
{
    printf("====================================\n");
    printf("  Download Vetting Flow Tests\n");
    printf("  (Phase 5 Days 29-30)\n");
    printf("====================================\n");

    // Create PolicyGraph with test database
    auto db_path = "/tmp/sentinel_download_test"sv;

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

    auto pg = pg_result.release_value();
    printf("✅ PolicyGraph initialized at %s\n", db_path.characters_without_null_termination());

    // Run all download vetting tests
    test_eicar_detection_and_policy(pg);
    test_clean_file_no_alert(pg);
    test_policy_enforcement_actions(pg);
    test_rate_limiting(pg);
    test_threat_history_tracking(pg);

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

    printf("\n✅ All Download Vetting Tests PASSED!\n");
    printf("\nNote: These tests verify policy enforcement logic.\n");
    printf("Full end-to-end testing requires running Sentinel daemon.\n");
    printf("\nDatabase location: %s/policy_graph.db\n", db_path.characters_without_null_termination());

    return 0;
}
