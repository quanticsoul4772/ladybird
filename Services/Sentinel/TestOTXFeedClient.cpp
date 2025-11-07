/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>
#include <Services/Sentinel/ThreatIntelligence/OTXFeedClient.h>
#include <Services/Sentinel/ThreatIntelligence/YARAGenerator.h>
#include <Services/Sentinel/ThreatIntelligence/UpdateScheduler.h>

using namespace Sentinel;
using namespace Sentinel::ThreatIntelligence;

static ErrorOr<void> test_otx_feed_client_creation()
{
    dbgln("TEST: OTXFeedClient creation");

    // Create temporary database directory
    auto db_dir = "/tmp/test_otx_db"_string;
    (void)FileSystem::remove(db_dir, FileSystem::RecursionMode::Allowed);
    TRY(Core::System::mkdir(db_dir.to_byte_string(), 0755));

    // Test with valid API key
    auto client = TRY(OTXFeedClient::create("test_api_key_12345"_string, db_dir));

    // Test with empty API key (should fail)
    auto empty_result = OTXFeedClient::create(""_string, db_dir);
    VERIFY(empty_result.is_error());

    dbgln("TEST: OTXFeedClient creation - PASSED");
    return {};
}

static ErrorOr<void> test_otx_pulse_fetching()
{
    dbgln("TEST: OTX pulse fetching with mock data");

    auto db_dir = "/tmp/test_otx_pulses"_string;
    (void)FileSystem::remove(db_dir, FileSystem::RecursionMode::Allowed);
    TRY(Core::System::mkdir(db_dir.to_byte_string(), 0755));

    auto client = TRY(OTXFeedClient::create("test_api_key"_string, db_dir));

    // Set mock base URL to trigger mock responses
    client->set_base_url("https://mock.otx.local/api/v1"_string);

    // Fetch pulses (will use mock data)
    TRY(client->fetch_latest_pulses());

    // Verify statistics
    auto const& stats = client->get_statistics();
    dbgln("  Pulses fetched: {}", stats.pulses_fetched);
    dbgln("  IOCs processed: {}", stats.iocs_processed);
    dbgln("  IOCs stored: {}", stats.iocs_stored);

    VERIFY(stats.pulses_fetched > 0);
    VERIFY(stats.iocs_processed > 0);
    VERIFY(stats.iocs_stored > 0);

    dbgln("TEST: OTX pulse fetching - PASSED");
    return {};
}

static ErrorOr<void> test_ioc_storage_and_retrieval()
{
    dbgln("TEST: IOC storage and retrieval");

    auto db_dir = "/tmp/test_otx_ioc_storage"_string;
    (void)FileSystem::remove(db_dir, FileSystem::RecursionMode::Allowed);
    TRY(Core::System::mkdir(db_dir.to_byte_string(), 0755));

    auto client = TRY(OTXFeedClient::create("test_api_key"_string, db_dir));
    client->set_base_url("https://mock.otx.local/api/v1"_string);

    // Fetch and store IOCs
    TRY(client->fetch_latest_pulses());

    // Get IOCs since epoch (should return all)
    auto since = UnixDateTime::from_seconds_since_epoch(0);
    auto iocs = TRY(client->get_new_iocs_since(since));

    dbgln("  Retrieved {} IOCs", iocs.size());
    VERIFY(iocs.size() > 0);

    // Verify IOC properties
    bool found_hash = false;
    bool found_domain = false;

    for (auto const& ioc : iocs) {
        dbgln("  IOC: {} = {}", PolicyGraph::ioc_type_to_string(ioc.type), ioc.indicator);

        if (ioc.type == PolicyGraph::IOC::Type::FileHash)
            found_hash = true;
        if (ioc.type == PolicyGraph::IOC::Type::Domain)
            found_domain = true;

        VERIFY(!ioc.indicator.is_empty());
        VERIFY(ioc.source == "otx"_string);
    }

    VERIFY(found_hash);
    VERIFY(found_domain);

    dbgln("TEST: IOC storage and retrieval - PASSED");
    return {};
}

static ErrorOr<void> test_yara_rule_generation()
{
    dbgln("TEST: YARA rule generation");

    auto db_dir = "/tmp/test_otx_yara"_string;
    auto yara_dir = "/tmp/test_otx_yara_rules"_string;

    (void)FileSystem::remove(db_dir, FileSystem::RecursionMode::Allowed);
    (void)FileSystem::remove(yara_dir, FileSystem::RecursionMode::Allowed);

    TRY(Core::System::mkdir(db_dir.to_byte_string(), 0755));
    TRY(Core::System::mkdir(yara_dir.to_byte_string(), 0755));

    auto client = TRY(OTXFeedClient::create("test_api_key"_string, db_dir));
    client->set_base_url("https://mock.otx.local/api/v1"_string);

    // Fetch IOCs
    TRY(client->fetch_latest_pulses());

    // Generate YARA rules
    TRY(client->generate_yara_rules(yara_dir));

    // Verify statistics
    auto const& stats = client->get_statistics();
    dbgln("  YARA rules generated: {}", stats.yara_rules_generated);
    VERIFY(stats.yara_rules_generated > 0);

    // Verify YARA file was created
    auto yara_file = TRY(String::formatted("{}/otx_hashes.yara", yara_dir));
    VERIFY(FileSystem::exists(yara_file));

    // Read and verify YARA file content
    auto file = TRY(Core::File::open(yara_file, Core::File::OpenMode::Read));
    auto content = TRY(file->read_until_eof());
    auto content_str = TRY(String::from_utf8(content));

    dbgln("  YARA file size: {} bytes", content.size());
    VERIFY(content_str.contains("rule otx_hash_"sv));
    VERIFY(content_str.contains("hash.sha256"sv));
    VERIFY(content_str.contains("meta:"sv));
    VERIFY(content_str.contains("condition:"sv));

    dbgln("TEST: YARA rule generation - PASSED");
    return {};
}

static ErrorOr<void> test_update_scheduler()
{
    dbgln("TEST: Update scheduler");

    // Create scheduler with short interval for testing (1 second)
    auto scheduler = TRY(UpdateScheduler::create(Duration::from_seconds(1)));

    // Track update executions
    bool update_executed = false;

    // Schedule update callback
    TRY(scheduler->schedule_update([&]() -> ErrorOr<void> {
        dbgln("  Update callback executed");
        update_executed = true;
        return {};
    }));

    VERIFY(scheduler->is_running());

    // Trigger immediate update
    TRY(scheduler->trigger_update_now());
    VERIFY(update_executed);

    // Stop scheduler
    scheduler->stop();
    VERIFY(!scheduler->is_running());

    dbgln("TEST: Update scheduler - PASSED");
    return {};
}

static ErrorOr<void> test_yara_generator()
{
    dbgln("TEST: YARA Generator");

    // Create test IOC
    PolicyGraph::IOC ioc;
    ioc.type = PolicyGraph::IOC::Type::FileHash;
    ioc.indicator = "ed01ebfbc9eb5bbea545af4d01bf5f1071661840480439c6e5babe8e080e41aa"_string;
    ioc.description = "Test malware sample"_string;
    ioc.tags = Vector { "malware"_string, "test"_string };
    ioc.created_at = UnixDateTime::now();
    ioc.source = "test"_string;

    // Generate rule
    auto rule = TRY(YARAGenerator::generate_hash_rule(ioc));

    dbgln("  Generated rule length: {} bytes", rule.bytes_as_string_view().length());

    // Verify rule content
    VERIFY(rule.contains("rule otx_hash_"sv));
    VERIFY(rule.contains("meta:"sv));
    VERIFY(rule.contains("description"sv));
    VERIFY(rule.contains("Test malware sample"sv));
    VERIFY(rule.contains("condition:"sv));
    VERIFY(rule.contains("hash.sha256"sv));
    VERIFY(rule.contains(ioc.indicator));

    // Validate syntax
    auto is_valid = TRY(YARAGenerator::validate_rule_syntax(rule));
    VERIFY(is_valid);

    dbgln("TEST: YARA Generator - PASSED");
    return {};
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    dbgln("=== OTX Feed Client Test Suite ===\n");

    TRY(test_otx_feed_client_creation());
    TRY(test_otx_pulse_fetching());
    TRY(test_ioc_storage_and_retrieval());
    TRY(test_yara_rule_generation());
    TRY(test_update_scheduler());
    TRY(test_yara_generator());

    dbgln("\n=== All tests PASSED ===");
    return 0;
}
