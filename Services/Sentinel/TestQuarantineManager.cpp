/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/String.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCrypto/Hash/SHA2.h>
#include <LibFileSystem/FileSystem.h>
#include <LibTest/TestCase.h>
#include <Services/Sentinel/Quarantine/FileEncryption.h>
#include <Services/Sentinel/Quarantine/QuarantineManager.h>
#include <Services/Sentinel/Sandbox/Orchestrator.h>

using namespace Sentinel;
using namespace Sentinel::Quarantine;
using namespace Sentinel::Sandbox;

// Helper: Create a temporary test file
static ErrorOr<String> create_test_file(String const& filename, ByteString const& content)
{
    auto test_dir = "/tmp/quarantine_test"_string;

    // Create test directory if it doesn't exist
    if (!FileSystem::exists(test_dir)) {
        TRY(Core::Directory::create(test_dir, Core::Directory::CreateDirectories::Yes));
    }

    auto file_path = MUST(String::formatted("{}/{}", test_dir, filename));
    auto file = TRY(Core::File::open(file_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(content.bytes()));

    return file_path;
}

// Helper: Create test malware analysis result
static SandboxResult create_test_threat_analysis(SandboxResult::ThreatLevel level)
{
    SandboxResult result;
    result.threat_level = level;
    result.confidence = 0.95f;
    result.composite_score = 0.89f;
    result.yara_score = 0.85f;
    result.ml_score = 0.90f;
    result.behavioral_score = 0.92f;

    MUST(result.detected_behaviors.try_append("File creates registry keys"_string));
    MUST(result.detected_behaviors.try_append("Network connection to suspicious IP"_string));
    MUST(result.triggered_rules.try_append("malware_dropper"_string));
    MUST(result.triggered_rules.try_append("network_exfiltration"_string));

    result.verdict_explanation = "Malicious file detected: Dropper with network exfiltration"_string;

    return result;
}

// Helper: Cleanup test directories
static void cleanup_test_environment()
{
    // Clean up test directories
    if (FileSystem::exists("/tmp/quarantine_test"sv)) {
        (void)FileSystem::remove("/tmp/quarantine_test"sv, FileSystem::RecursionMode::Allowed);
    }
    if (FileSystem::exists("/tmp/quarantine_mgr_test"sv)) {
        (void)FileSystem::remove("/tmp/quarantine_mgr_test"sv, FileSystem::RecursionMode::Allowed);
    }
    if (FileSystem::exists("/tmp/quarantine_db_test"sv)) {
        (void)FileSystem::remove("/tmp/quarantine_db_test"sv, FileSystem::RecursionMode::Allowed);
    }
}

TEST_CASE(test_file_encryption_decryption)
{
    // Test basic encryption/decryption workflow

    // Generate encryption key
    auto key = MUST(FileEncryption::generate_encryption_key());
    EXPECT_EQ(key.size(), 32u); // AES-256 requires 32-byte key

    // Create test file
    auto test_content = "This is a test file for encryption"_string;
    auto test_file = MUST(create_test_file("test_encrypt.txt"_string, test_content.to_byte_string()));

    // Encrypt file
    auto encrypted_file = "/tmp/quarantine_test/test_encrypt.txt.encrypted"_string;
    MUST(FileEncryption::encrypt_file(test_file, encrypted_file, key));

    EXPECT(FileSystem::exists(encrypted_file));

    // Read encrypted file and verify it's different from plaintext
    auto encrypted_data = MUST(Core::File::open(encrypted_file, Core::File::OpenMode::Read));
    auto encrypted_bytes = MUST(encrypted_data->read_until_eof());

    // Encrypted data should have IV (16 bytes) + ciphertext
    EXPECT(encrypted_bytes.size() > 16);
    EXPECT(encrypted_bytes.size() != test_content.bytes().size());

    // Decrypt file
    auto decrypted_file = "/tmp/quarantine_test/test_encrypt.txt.decrypted"_string;
    MUST(FileEncryption::decrypt_file(encrypted_file, decrypted_file, key));

    // Verify decrypted content matches original
    auto decrypted_data = MUST(Core::File::open(decrypted_file, Core::File::OpenMode::Read));
    auto decrypted_bytes = MUST(decrypted_data->read_until_eof());

    EXPECT_EQ(decrypted_bytes.size(), test_content.bytes().size());
    EXPECT_EQ(StringView { decrypted_bytes }, test_content);

    cleanup_test_environment();
}

TEST_CASE(test_quarantine_file)
{
    // Test quarantining a malicious file

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Create test malicious file
    auto malware_content = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"_string;
    auto malware_file = MUST(create_test_file("eicar.txt"_string, malware_content.to_byte_string()));

    // Create threat analysis
    auto threat_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Malicious);

    // Quarantine the file
    auto record = MUST(manager->quarantine_file(malware_file, threat_analysis));

    // Verify record
    EXPECT(record.id > 0);
    EXPECT_EQ(record.original_path, malware_file);
    EXPECT(record.quarantine_path.contains("quar"sv));
    EXPECT(record.threat_level == SandboxResult::ThreatLevel::Malicious);
    EXPECT(record.threat_score > 0.0f);

    // Verify quarantined file exists
    EXPECT(FileSystem::exists(record.quarantine_path));

    // Verify original file was deleted
    EXPECT(!FileSystem::exists(malware_file));

    // Verify statistics
    auto stats = manager->get_statistics();
    EXPECT_EQ(stats.total_quarantined, 1u);
    EXPECT_EQ(stats.current_quarantine_count, 1u);

    cleanup_test_environment();
}

TEST_CASE(test_restore_file)
{
    // Test restoring a false positive from quarantine

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Create test file (false positive)
    auto safe_content = "This is actually a safe file"_string;
    auto safe_file = MUST(create_test_file("safe.txt"_string, safe_content.to_byte_string()));

    // Quarantine it (false positive)
    auto threat_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Suspicious);
    auto record = MUST(manager->quarantine_file(safe_file, threat_analysis));

    EXPECT(!FileSystem::exists(safe_file)); // Original deleted

    // Restore the file
    auto restore_path = "/tmp/quarantine_test/safe_restored.txt"_string;
    MUST(manager->restore_file(record.id, restore_path));

    // Verify restored file exists and has correct content
    EXPECT(FileSystem::exists(restore_path));

    auto restored_data = MUST(Core::File::open(restore_path, Core::File::OpenMode::Read));
    auto restored_bytes = MUST(restored_data->read_until_eof());

    EXPECT_EQ(StringView { restored_bytes }, safe_content);

    // Verify quarantined file was deleted
    EXPECT(!FileSystem::exists(record.quarantine_path));

    // Verify statistics
    auto stats = manager->get_statistics();
    EXPECT_EQ(stats.total_quarantined, 1u);
    EXPECT_EQ(stats.total_restored, 1u);
    EXPECT_EQ(stats.current_quarantine_count, 0u);

    cleanup_test_environment();
}

TEST_CASE(test_delete_quarantined_file)
{
    // Test permanently deleting a quarantined file

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Create and quarantine test file
    auto malware_file = MUST(create_test_file("malware.exe"_string, "malicious binary data"_byte_string));
    auto threat_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Critical);
    auto record = MUST(manager->quarantine_file(malware_file, threat_analysis));

    EXPECT(FileSystem::exists(record.quarantine_path));

    // Permanently delete the file
    MUST(manager->delete_file(record.id));

    // Verify quarantined file was deleted
    EXPECT(!FileSystem::exists(record.quarantine_path));

    // Verify statistics
    auto stats = manager->get_statistics();
    EXPECT_EQ(stats.total_quarantined, 1u);
    EXPECT_EQ(stats.total_deleted, 1u);
    EXPECT_EQ(stats.current_quarantine_count, 0u);

    cleanup_test_environment();
}

TEST_CASE(test_cleanup_expired_files)
{
    // Test cleanup of expired quarantined files

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Create and quarantine multiple files
    for (int i = 0; i < 3; i++) {
        auto filename = MUST(String::formatted("malware{}.bin", i));
        auto file = MUST(create_test_file(filename, "malware data"_byte_string));
        auto threat_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Malicious);
        MUST(manager->quarantine_file(file, threat_analysis));
    }

    // Verify 3 files quarantined
    auto all_files = MUST(manager->list_quarantined_files());
    EXPECT_EQ(all_files.size(), 3u);

    // Try to cleanup with 30-day retention (no files should be deleted - they're fresh)
    auto cleaned_count = MUST(manager->cleanup_expired(AK::Duration::from_days(30)));
    EXPECT_EQ(cleaned_count, 0u);

    // Cleanup with 0-second retention (all files should be deleted)
    cleaned_count = MUST(manager->cleanup_expired(AK::Duration::from_seconds(0)));
    EXPECT_EQ(cleaned_count, 3u);

    // Verify all files removed
    all_files = MUST(manager->list_quarantined_files());
    EXPECT_EQ(all_files.size(), 0u);

    cleanup_test_environment();
}

TEST_CASE(test_list_quarantined_files_with_filter)
{
    // Test listing quarantined files with threat level filter

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Quarantine files with different threat levels
    auto suspicious_file = MUST(create_test_file("suspicious.txt"_string, "suspicious"_byte_string));
    auto suspicious_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Suspicious);
    MUST(manager->quarantine_file(suspicious_file, suspicious_analysis));

    auto malicious_file = MUST(create_test_file("malicious.txt"_string, "malicious"_byte_string));
    auto malicious_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Malicious);
    MUST(manager->quarantine_file(malicious_file, malicious_analysis));

    auto critical_file = MUST(create_test_file("critical.txt"_string, "critical"_byte_string));
    auto critical_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Critical);
    MUST(manager->quarantine_file(critical_file, critical_analysis));

    // List all files
    auto all_files = MUST(manager->list_quarantined_files());
    EXPECT_EQ(all_files.size(), 3u);

    // Filter by Suspicious
    auto suspicious_files = MUST(manager->list_quarantined_files(SandboxResult::ThreatLevel::Suspicious));
    EXPECT_EQ(suspicious_files.size(), 1u);
    EXPECT(suspicious_files[0].threat_level == SandboxResult::ThreatLevel::Suspicious);

    // Filter by Malicious
    auto malicious_files = MUST(manager->list_quarantined_files(SandboxResult::ThreatLevel::Malicious));
    EXPECT_EQ(malicious_files.size(), 1u);
    EXPECT(malicious_files[0].threat_level == SandboxResult::ThreatLevel::Malicious);

    // Filter by Critical
    auto critical_files = MUST(manager->list_quarantined_files(SandboxResult::ThreatLevel::Critical));
    EXPECT_EQ(critical_files.size(), 1u);
    EXPECT(critical_files[0].threat_level == SandboxResult::ThreatLevel::Critical);

    cleanup_test_environment();
}

TEST_CASE(test_duplicate_quarantine_prevention)
{
    // Test that duplicate files (same hash) cannot be quarantined twice

    auto quarantine_dir = "/tmp/quarantine_mgr_test"_string;
    auto db_path = "/tmp/quarantine_db_test"_string;

    auto manager = MUST(QuarantineManager::create(quarantine_dir, db_path));

    // Create test file
    auto content = "Duplicate test content"_byte_string;
    auto file1 = MUST(create_test_file("duplicate1.txt"_string, content));

    // Quarantine first file
    auto threat_analysis = create_test_threat_analysis(SandboxResult::ThreatLevel::Malicious);
    auto record1 = MUST(manager->quarantine_file(file1, threat_analysis));

    EXPECT(record1.id > 0);

    // Create identical file with different name
    auto file2 = MUST(create_test_file("duplicate2.txt"_string, content));

    // Attempt to quarantine duplicate (should fail)
    auto result2 = manager->quarantine_file(file2, threat_analysis);
    EXPECT(result2.is_error());

    // Verify only one file in quarantine
    auto all_files = MUST(manager->list_quarantined_files());
    EXPECT_EQ(all_files.size(), 1u);

    cleanup_test_environment();
}
