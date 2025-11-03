/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "../PolicyGraph.h"
#include "../Sandbox/Orchestrator.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel::Quarantine {

// Quarantine record metadata stored in database
struct QuarantineRecord {
    i64 id { -1 };
    String original_path;
    String quarantine_path;
    String quarantine_reason;
    float threat_score { 0.0f };
    Sandbox::SandboxResult::ThreatLevel threat_level { Sandbox::SandboxResult::ThreatLevel::Clean };
    UnixDateTime quarantined_at;
    u64 file_size { 0 };
    String sha256_hash;

    // Helper to check if record is expired based on retention period
    bool is_expired(AK::Duration retention_period) const
    {
        auto expiry_time = quarantined_at + retention_period;
        return UnixDateTime::now() > expiry_time;
    }
};

// Statistics for quarantine manager
struct QuarantineStatistics {
    u64 total_quarantined { 0 };
    u64 total_restored { 0 };
    u64 total_deleted { 0 };
    u64 total_expired_cleaned { 0 };
    u64 current_quarantine_count { 0 };
    u64 total_quarantine_size_bytes { 0 };
};

// Quarantine Manager - manages isolation and recovery of malicious files
// Milestone 0.5 Phase 1e: Automated Quarantine with User Rollback
class QuarantineManager {
public:
    // Create quarantine manager with specified quarantine directory
    // Directory will be created if it doesn't exist with restricted permissions (chmod 700)
    static ErrorOr<NonnullOwnPtr<QuarantineManager>> create(
        String const& quarantine_dir,
        String const& db_path);

    // Quarantine a file based on sandbox analysis results
    // 1. Calculates SHA256 hash
    // 2. Moves file to quarantine directory
    // 3. Encrypts file with AES-256
    // 4. Stores metadata in database
    // Returns: QuarantineRecord with quarantine details
    ErrorOr<QuarantineRecord> quarantine_file(
        String const& file_path,
        Sandbox::SandboxResult const& threat_analysis);

    // Restore a quarantined file (for false positives)
    // 1. Decrypts file
    // 2. Moves file to target path
    // 3. Removes quarantine record from database
    ErrorOr<void> restore_file(i64 quarantine_id, String const& target_path);

    // Permanently delete a quarantined file
    // 1. Deletes encrypted file from disk
    // 2. Removes quarantine record from database
    ErrorOr<void> delete_file(i64 quarantine_id);

    // List all quarantined files (optionally filtered)
    ErrorOr<Vector<QuarantineRecord>> list_quarantined_files(
        Optional<Sandbox::SandboxResult::ThreatLevel> threat_level_filter = {});

    // Get a specific quarantine record by ID
    ErrorOr<QuarantineRecord> get_quarantine_record(i64 quarantine_id);

    // Cleanup expired quarantined files (default: 30 days retention)
    // Returns: Number of files cleaned up
    ErrorOr<u64> cleanup_expired(AK::Duration retention_period = AK::Duration::from_seconds(30 * 24 * 60 * 60));

    // Get quarantine statistics
    QuarantineStatistics get_statistics() const;

    // Check if a file hash is already quarantined (avoid duplicates)
    ErrorOr<bool> is_file_quarantined(String const& sha256_hash);

    // Configuration
    String const& quarantine_directory() const { return m_quarantine_dir; }

private:
    QuarantineManager(String quarantine_dir, NonnullOwnPtr<PolicyGraph> db, ByteBuffer encryption_key);

    // Initialize quarantine directory with secure permissions
    ErrorOr<void> initialize_quarantine_directory();

    // Load or generate encryption key
    ErrorOr<ByteBuffer> load_or_generate_encryption_key();

    // Save encryption key securely
    ErrorOr<void> save_encryption_key(ByteBuffer const& key);

    // Calculate SHA256 hash of file
    ErrorOr<String> calculate_file_hash(String const& file_path);

    // Generate unique quarantine filename
    String generate_quarantine_filename(String const& original_filename, String const& sha256_hash);

    // Database operations
    ErrorOr<i64> insert_quarantine_record(QuarantineRecord const& record);
    ErrorOr<void> delete_quarantine_record(i64 quarantine_id);
    ErrorOr<Vector<QuarantineRecord>> query_quarantine_records(String const& where_clause);

    String m_quarantine_dir;                    // e.g., ~/.local/share/ladybird/quarantine/
    NonnullOwnPtr<PolicyGraph> m_db;            // Database for quarantine records
    ByteBuffer m_encryption_key;                // AES-256 key (32 bytes)

    // Statistics tracking
    mutable QuarantineStatistics m_stats;
};

}
