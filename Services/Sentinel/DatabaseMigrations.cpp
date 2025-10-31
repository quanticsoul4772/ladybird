/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Services/Sentinel/DatabaseMigrations.h>

namespace Sentinel {

ErrorOr<int> DatabaseMigrations::get_schema_version(Database::Database& db)
{
    // Check if schema_version table exists
    auto check_table_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'"_string;
    auto check_statement = TRY(db.prepare_statement(check_table_sql));
    bool table_exists = false;
    db.execute_statement(check_statement, [&](auto) { table_exists = true; });

    if (!table_exists) {
        // No schema_version table means version 0 (uninitialized or old schema)
        return 0;
    }

    // Read version from table
    auto version_sql = "SELECT version FROM schema_version LIMIT 1"_string;
    auto version_statement = TRY(db.prepare_statement(version_sql));
    i64 version = 0;
    db.execute_statement(version_statement, [&](auto stmt_id) {
        version = db.result_column<i64>(stmt_id, 0);
    });

    if (version == 0) {
        return 0;
    }

    return static_cast<int>(version);
}

ErrorOr<void> DatabaseMigrations::set_schema_version(Database::Database& db, int version)
{
    // Create or update schema_version table
    auto create_table_sql = "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY)"_string;
    auto create_statement = TRY(db.prepare_statement(create_table_sql));
    db.execute_statement(create_statement, {});

    // Clear any existing version
    auto clear_sql = "DELETE FROM schema_version"_string;
    auto clear_statement = TRY(db.prepare_statement(clear_sql));
    db.execute_statement(clear_statement, {});

    // Insert new version
    auto insert_sql = "INSERT INTO schema_version (version) VALUES (?)"_string;
    auto insert_statement = TRY(db.prepare_statement(insert_sql));
    db.execute_statement(insert_statement, {}, version);

    dbgln("DatabaseMigrations: Set schema version to {}", version);
    return {};
}

ErrorOr<bool> DatabaseMigrations::needs_migration(Database::Database& db)
{
    auto current_version = TRY(get_schema_version(db));
    return current_version < CURRENT_SCHEMA_VERSION;
}

ErrorOr<void> DatabaseMigrations::initialize_schema(Database::Database& db)
{
    dbgln("DatabaseMigrations: Initializing fresh schema (version {})", CURRENT_SCHEMA_VERSION);

    // This schema matches the PolicyGraph initialization
    // We don't need to recreate tables here if PolicyGraph already created them
    // Just set the version number

    TRY(set_schema_version(db, CURRENT_SCHEMA_VERSION));

    dbgln("DatabaseMigrations: Schema initialization complete");
    return {};
}

ErrorOr<void> DatabaseMigrations::migrate_v0_to_v1(Database::Database&)
{
    dbgln("DatabaseMigrations: Migrating from v0 to v1");

    // v0 to v1 migration (if needed in the future)
    // For now, this is a no-op since v1 is the initial versioned schema

    // If there were changes, we would add them here:
    // Example:
    // auto sql = "ALTER TABLE policies ADD COLUMN new_field TEXT"_string;
    // auto statement = TRY(db.prepare_statement(sql));
    // db.execute_statement(statement, {});

    return {};
}

ErrorOr<void> DatabaseMigrations::migrate_v1_to_v2(Database::Database& db)
{
    dbgln("DatabaseMigrations: Migrating from v1 to v2 (adding performance indexes)");

    // TASK: Add database indexes for query optimization
    // ISSUE-019 from SENTINEL_DAY29_KNOWN_ISSUES.md
    //
    // Performance improvements:
    // - Composite indexes for common multi-column queries
    // - Indexes for cleanup and expiration queries
    // - Indexes for duplicate detection
    // - Indexes for threat analysis queries
    //
    // Expected improvements:
    // - check_url() lookups: 100ms -> <1ms (100x)
    // - Duplicate detection: 50ms -> <1ms (50x)
    // - Cleanup queries: 500ms -> <10ms (50x)

    // Index 1: Composite index on (file_hash, expires_at) for match_by_hash query
    // This query is in the hot path for every download
    // Query: SELECT * FROM policies WHERE file_hash = ? AND (expires_at = -1 OR expires_at > ?)
    auto idx_hash_expires = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_hash_expires ON policies(file_hash, expires_at);"_string));
    db.execute_statement(idx_hash_expires, {});
    dbgln("DatabaseMigrations: Created index idx_policies_hash_expires");

    // Index 2: Index on expires_at for cleanup query
    // Query: DELETE FROM policies WHERE expires_at IS NOT NULL AND expires_at <= ?
    auto idx_expires = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_expires_at ON policies(expires_at);"_string));
    db.execute_statement(idx_expires, {});
    dbgln("DatabaseMigrations: Created index idx_policies_expires_at");

    // Index 3: Composite index for duplicate detection
    // Query: check if policy with (rule_name, url_pattern, file_hash) already exists
    // This prevents duplicate policy creation
    auto idx_duplicate = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_duplicate_check ON policies(rule_name, url_pattern, file_hash);"_string));
    db.execute_statement(idx_duplicate, {});
    dbgln("DatabaseMigrations: Created index idx_policies_duplicate_check");

    // Index 4: Index on last_hit for cleanup/maintenance queries
    // Used to find and clean up stale policies
    auto idx_last_hit = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_last_hit ON policies(last_hit);"_string));
    db.execute_statement(idx_last_hit, {});
    dbgln("DatabaseMigrations: Created index idx_policies_last_hit");

    // Index 5: Index on action for filtering and reporting
    // Used to filter policies by action type (block, allow, quarantine, etc.)
    auto idx_action = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_action ON policies(action);"_string));
    db.execute_statement(idx_action, {});
    dbgln("DatabaseMigrations: Created index idx_policies_action");

    // Index 6: Composite index on (rule_name, expires_at) for match_by_rule_name query
    // Query: SELECT * FROM policies WHERE rule_name = ? AND ... AND (expires_at = -1 OR expires_at > ?)
    auto idx_rule_expires = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_rule_expires ON policies(rule_name, expires_at);"_string));
    db.execute_statement(idx_rule_expires, {});
    dbgln("DatabaseMigrations: Created index idx_policies_rule_expires");

    // Threat history indexes

    // Index 7: Composite index on (detected_at, action_taken) for threat analysis
    // Used to analyze threat response patterns over time
    auto idx_threat_detected_action = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_detected_action ON threat_history(detected_at, action_taken);"_string));
    db.execute_statement(idx_threat_detected_action, {});
    dbgln("DatabaseMigrations: Created index idx_threat_history_detected_action");

    // Index 8: Index on action_taken for filtering threats by response
    auto idx_threat_action = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_action_taken ON threat_history(action_taken);"_string));
    db.execute_statement(idx_threat_action, {});
    dbgln("DatabaseMigrations: Created index idx_threat_history_action_taken");

    // Index 9: Index on severity for filtering critical threats
    auto idx_threat_severity = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_severity ON threat_history(severity);"_string));
    db.execute_statement(idx_threat_severity, {});
    dbgln("DatabaseMigrations: Created index idx_threat_history_severity");

    // Index 10: Composite index on (rule_name, detected_at) for rule-specific threat timeline
    // Query: SELECT * FROM threat_history WHERE rule_name = ? ORDER BY detected_at DESC
    auto idx_threat_rule_detected = TRY(db.prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_rule_detected ON threat_history(rule_name, detected_at);"_string));
    db.execute_statement(idx_threat_rule_detected, {});
    dbgln("DatabaseMigrations: Created index idx_threat_history_rule_detected");

    dbgln("DatabaseMigrations: v1 to v2 migration complete - added 10 performance indexes");
    return {};
}

ErrorOr<void> DatabaseMigrations::migrate(Database::Database& db)
{
    auto current_version = TRY(get_schema_version(db));

    if (current_version == CURRENT_SCHEMA_VERSION) {
        dbgln("DatabaseMigrations: Database is already at current version ({})", CURRENT_SCHEMA_VERSION);
        return {};
    }

    if (current_version > CURRENT_SCHEMA_VERSION) {
        return Error::from_string_literal("Database schema version is newer than supported by this version of Sentinel");
    }

    dbgln("DatabaseMigrations: Starting migration from v{} to v{}", current_version, CURRENT_SCHEMA_VERSION);

    // Perform migrations in sequence
    if (current_version < 1) {
        TRY(migrate_v0_to_v1(db));
        TRY(set_schema_version(db, 1));
    }

    if (current_version < 2) {
        TRY(migrate_v1_to_v2(db));
        TRY(set_schema_version(db, 2));
    }

    // Future migrations would go here:
    // if (current_version < 3) {
    //     TRY(migrate_v2_to_v3(db));
    //     TRY(set_schema_version(db, 3));
    // }

    dbgln("DatabaseMigrations: Migration complete");
    return {};
}

ErrorOr<void> DatabaseMigrations::verify_schema(Database::Database& db)
{
    auto version = TRY(get_schema_version(db));

    if (version != CURRENT_SCHEMA_VERSION) {
        return Error::from_string_literal("Database schema version mismatch");
    }

    // Verify that critical tables exist
    Vector<String> required_tables = {
        "policies"_string,
        "threats"_string,
        "schema_version"_string
    };

    for (auto const& table : required_tables) {
        auto check_sql = MUST(String::formatted("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'", table));
        auto check_statement = TRY(db.prepare_statement(check_sql));
        bool table_exists = false;
        db.execute_statement(check_statement, [&](auto) { table_exists = true; });

        if (!table_exists) {
            return Error::from_string_literal("Required table missing from database schema");
        }
    }

    dbgln("DatabaseMigrations: Schema verification passed");
    return {};
}

}
