/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibDatabase/Database.h>

namespace Sentinel {

// Database schema version tracking and migrations
class DatabaseMigrations {
public:
    static constexpr int CURRENT_SCHEMA_VERSION = 3;

    // Check if database needs migration
    static ErrorOr<bool> needs_migration(Database::Database& db);

    // Get current schema version from database
    static ErrorOr<int> get_schema_version(Database::Database& db);

    // Perform migration from old version to current
    static ErrorOr<void> migrate(Database::Database& db);

    // Initialize fresh database with current schema
    static ErrorOr<void> initialize_schema(Database::Database& db);

    // Verify schema is compatible
    static ErrorOr<void> verify_schema(Database::Database& db);

private:
    // Individual migration steps
    static ErrorOr<void> migrate_v0_to_v1(Database::Database& db);
    static ErrorOr<void> migrate_v1_to_v2(Database::Database& db);
    static ErrorOr<void> migrate_v2_to_v3(Database::Database& db);

    // Set schema version in database
    static ErrorOr<void> set_schema_version(Database::Database& db, int version);
};

}
