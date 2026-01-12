/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "QuarantineManager.h"
#include "FileEncryption.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/LexicalPath.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibCrypto/Hash/SHA2.h>
#include <LibFileSystem/FileSystem.h>
#include <sys/stat.h>

namespace Sentinel::Quarantine {

QuarantineManager::QuarantineManager(String quarantine_dir, NonnullOwnPtr<PolicyGraph> db, ByteBuffer encryption_key)
    : m_quarantine_dir(move(quarantine_dir))
    , m_db(move(db))
    , m_encryption_key(move(encryption_key))
{
}

ErrorOr<NonnullOwnPtr<QuarantineManager>> QuarantineManager::create(
    String const& quarantine_dir,
    String const& db_path)
{
    // Initialize database connection
    auto db = TRY(PolicyGraph::create(db_path.to_byte_string()));

    // Load or generate encryption key
    auto key_path = MUST(String::formatted("{}/encryption.key", quarantine_dir));
    ByteBuffer encryption_key;

    if (FileSystem::exists(key_path)) {
        // Load existing key
        auto key_file = TRY(Core::File::open(key_path, Core::File::OpenMode::Read));
        encryption_key = TRY(key_file->read_until_eof());

        if (encryption_key.size() != 32) {
            return Error::from_string_literal("Invalid encryption key size (expected 32 bytes)");
        }
    } else {
        // Generate new key
        encryption_key = TRY(FileEncryption::generate_encryption_key());

        // Create quarantine directory if it doesn't exist
        if (!FileSystem::exists(quarantine_dir)) {
            TRY(Core::Directory::create(quarantine_dir.to_byte_string(), Core::Directory::CreateDirectories::Yes));
        }

        // Set restrictive permissions (owner only: rwx------)
        if (chmod(quarantine_dir.to_byte_string().characters(), 0700) != 0) {
            return Error::from_errno(errno);
        }

        // Save encryption key securely
        auto key_file = TRY(Core::File::open(key_path, Core::File::OpenMode::Write));
        TRY(key_file->write_until_depleted(encryption_key));

        // Set key file permissions (owner only: rw-------)
        if (chmod(key_path.to_byte_string().characters(), 0600) != 0) {
            return Error::from_errno(errno);
        }
    }

    auto manager = adopt_own(*new QuarantineManager(quarantine_dir, move(db), move(encryption_key)));

    // Initialize quarantine directory
    TRY(manager->initialize_quarantine_directory());

    return manager;
}

ErrorOr<void> QuarantineManager::initialize_quarantine_directory()
{
    // Ensure quarantine directory exists
    if (!FileSystem::exists(m_quarantine_dir)) {
        TRY(Core::Directory::create(m_quarantine_dir.to_byte_string(), Core::Directory::CreateDirectories::Yes));

        // Set restrictive permissions
        if (chmod(m_quarantine_dir.to_byte_string().characters(), 0700) != 0) {
            return Error::from_errno(errno);
        }
    }

    // Initialize statistics by counting existing records
    auto all_records = TRY(list_quarantined_files());
    m_stats.current_quarantine_count = all_records.size();
    m_stats.total_quarantine_size_bytes = 0;

    for (auto const& record : all_records) {
        m_stats.total_quarantine_size_bytes += record.file_size;
    }

    return {};
}

ErrorOr<String> QuarantineManager::calculate_file_hash(String const& file_path)
{
    // Read file contents
    auto file = TRY(Core::File::open(file_path, Core::File::OpenMode::Read));
    auto file_data = TRY(file->read_until_eof());

    // Calculate SHA256 hash
    auto sha256 = Crypto::Hash::SHA256::create();
    sha256->update(file_data);
    auto hash_bytes = sha256->digest();

    // Convert to hex string
    StringBuilder hash_string;
    for (auto byte : hash_bytes.bytes()) {
        hash_string.appendff("{:02x}", byte);
    }

    return TRY(hash_string.to_string());
}

String QuarantineManager::generate_quarantine_filename(String const& original_filename, String const& sha256_hash)
{
    // Format: <timestamp>_<first8chars_of_hash>_<original_filename>.quar
    auto timestamp = UnixDateTime::now().seconds_since_epoch();
    auto hash_prefix = sha256_hash.bytes_as_string_view().substring_view(0, 8);

    // Sanitize original filename (remove path separators)
    auto sanitized_name = LexicalPath(original_filename.to_byte_string()).basename();

    return MUST(String::formatted("{}_{}_{}. quar"sv, timestamp, hash_prefix, sanitized_name));
}

ErrorOr<QuarantineRecord> QuarantineManager::quarantine_file(
    String const& file_path,
    Sandbox::SandboxResult const& threat_analysis)
{
    // Verify file exists
    if (!FileSystem::exists(file_path)) {
        return Error::from_string_literal("File not found");
    }

    // Calculate file hash
    auto sha256_hash = TRY(calculate_file_hash(file_path));

    // Check if already quarantined (avoid duplicates)
    if (TRY(is_file_quarantined(sha256_hash))) {
        dbgln("QuarantineManager: File already quarantined (hash: {})", sha256_hash);
        return Error::from_string_literal("File already quarantined");
    }

    // Get file size
    auto stat_result = TRY(Core::System::stat(file_path));
    auto file_size = stat_result.st_size;

    // Generate quarantine filename
    auto quarantine_filename = generate_quarantine_filename(file_path, sha256_hash);
    auto quarantine_path = MUST(String::formatted("{}/{}", m_quarantine_dir, quarantine_filename));

    // Encrypt file to quarantine location
    TRY(FileEncryption::encrypt_file(file_path, quarantine_path, m_encryption_key));

    // Delete original file (it's now safely encrypted in quarantine)
    TRY(FileSystem::remove(file_path, FileSystem::RecursionMode::Disallowed));

    // Build quarantine reason from threat analysis
    auto quarantine_reason = MUST(String::formatted(
        "Threat Level: {} | Confidence: {:.1f}% | Behaviors: {} | Rules: {}",
        static_cast<int>(threat_analysis.threat_level),
        threat_analysis.confidence * 100.0f,
        threat_analysis.detected_behaviors.size(),
        threat_analysis.triggered_rules.size()));

    // Create quarantine record
    QuarantineRecord record {
        .id = -1,
        .original_path = file_path,
        .quarantine_path = quarantine_path,
        .quarantine_reason = quarantine_reason,
        .threat_score = threat_analysis.composite_score,
        .threat_level = threat_analysis.threat_level,
        .quarantined_at = UnixDateTime::now(),
        .file_size = static_cast<u64>(file_size),
        .sha256_hash = sha256_hash
    };

    // Store in database
    record.id = TRY(insert_quarantine_record(record));

    // Update statistics
    m_stats.total_quarantined++;
    m_stats.current_quarantine_count++;
    m_stats.total_quarantine_size_bytes += file_size;

    dbgln("QuarantineManager: Quarantined file: {} -> {} (ID: {})", file_path, quarantine_path, record.id);

    return record;
}

ErrorOr<void> QuarantineManager::restore_file(i64 quarantine_id, String const& target_path)
{
    // Get quarantine record
    auto record = TRY(get_quarantine_record(quarantine_id));

    // Verify quarantined file exists
    if (!FileSystem::exists(record.quarantine_path)) {
        return Error::from_string_literal("Quarantined file not found on disk");
    }

    // Decrypt file to target location
    TRY(FileEncryption::decrypt_file(record.quarantine_path, target_path, m_encryption_key));

    // Delete encrypted quarantine file
    TRY(FileSystem::remove(record.quarantine_path, FileSystem::RecursionMode::Disallowed));

    // Remove from database
    TRY(delete_quarantine_record(quarantine_id));

    // Update statistics
    m_stats.total_restored++;
    m_stats.current_quarantine_count--;
    m_stats.total_quarantine_size_bytes -= record.file_size;

    dbgln("QuarantineManager: Restored file: {} -> {} (ID: {})", record.quarantine_path, target_path, quarantine_id);

    return {};
}

ErrorOr<void> QuarantineManager::delete_file(i64 quarantine_id)
{
    // Get quarantine record
    auto record = TRY(get_quarantine_record(quarantine_id));

    // Delete encrypted file from disk
    if (FileSystem::exists(record.quarantine_path)) {
        TRY(FileSystem::remove(record.quarantine_path, FileSystem::RecursionMode::Disallowed));
    }

    // Remove from database
    TRY(delete_quarantine_record(quarantine_id));

    // Update statistics
    m_stats.total_deleted++;
    m_stats.current_quarantine_count--;
    m_stats.total_quarantine_size_bytes -= record.file_size;

    dbgln("QuarantineManager: Permanently deleted file (ID: {})", quarantine_id);

    return {};
}

ErrorOr<Vector<QuarantineRecord>> QuarantineManager::list_quarantined_files(
    Optional<Sandbox::SandboxResult::ThreatLevel> threat_level_filter)
{
    String where_clause;

    if (threat_level_filter.has_value()) {
        where_clause = MUST(String::formatted(
            "WHERE threat_level = {}",
            static_cast<int>(threat_level_filter.value())));
    } else {
        where_clause = ""_string;
    }

    return query_quarantine_records(where_clause);
}

ErrorOr<QuarantineRecord> QuarantineManager::get_quarantine_record(i64 quarantine_id)
{
    auto where_clause = MUST(String::formatted("WHERE id = {}", quarantine_id));
    auto records = TRY(query_quarantine_records(where_clause));

    if (records.is_empty()) {
        return Error::from_string_literal("Quarantine record not found");
    }

    return records[0];
}

ErrorOr<u64> QuarantineManager::cleanup_expired(AK::Duration retention_period)
{
    // Get all quarantined files
    auto all_records = TRY(list_quarantined_files());

    u64 cleaned_count = 0;

    for (auto const& record : all_records) {
        if (record.is_expired(retention_period)) {
            // Delete expired file
            auto result = delete_file(record.id);
            if (result.is_error()) {
                dbgln("QuarantineManager: Failed to delete expired file (ID: {}): {}", record.id, result.error());
                continue;
            }

            cleaned_count++;
        }
    }

    m_stats.total_expired_cleaned += cleaned_count;

    dbgln("QuarantineManager: Cleaned up {} expired files (retention: {} days)",
        cleaned_count, retention_period.to_seconds() / (24 * 60 * 60));

    return cleaned_count;
}

QuarantineStatistics QuarantineManager::get_statistics() const
{
    return m_stats;
}

ErrorOr<bool> QuarantineManager::is_file_quarantined(String const& sha256_hash)
{
    auto where_clause = MUST(String::formatted("WHERE sha256_hash = '{}'", sha256_hash));
    auto records = TRY(query_quarantine_records(where_clause));

    return !records.is_empty();
}

// Database operations

ErrorOr<i64> QuarantineManager::insert_quarantine_record(QuarantineRecord const& record)
{
    // Access database via PolicyGraph's public API
    // We'll use PolicyGraph to store quarantine records in the database
    // For now, we'll use a direct database access pattern

    // Note: This requires PolicyGraph::m_database to be accessible
    // We'll add a method to PolicyGraph to expose database for quarantine operations
    return TRY(m_db->insert_quarantine_record(record));
}

ErrorOr<void> QuarantineManager::delete_quarantine_record(i64 quarantine_id)
{
    return TRY(m_db->delete_quarantine_record(quarantine_id));
}

ErrorOr<Vector<QuarantineRecord>> QuarantineManager::query_quarantine_records(String const& where_clause)
{
    return TRY(m_db->query_quarantine_records(where_clause));
}

}
