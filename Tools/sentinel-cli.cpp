/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/Format.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/Socket.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>
#include <Services/RequestServer/Quarantine.h>
#include <Services/Sentinel/PolicyGraph.h>
#include <sys/stat.h>

using namespace RequestServer;
using namespace Sentinel;

// Forward declaration
ErrorOr<int> ladybird_main(Main::Arguments arguments);

static ErrorOr<String> get_database_path()
{
    auto data_dir = Core::StandardPaths::user_data_directory();
    StringBuilder path_builder;
    path_builder.append(data_dir);
    path_builder.append("/Ladybird/PolicyGraph"sv);
    return path_builder.to_string();
}

static ErrorOr<void> command_status()
{
    outln("=== Sentinel Status ===\n");

    // Check if Sentinel daemon is running
    auto socket_result = Core::LocalSocket::connect("/tmp/sentinel.sock"sv);
    if (socket_result.is_error()) {
        outln("Sentinel Daemon:      \033[31mNOT RUNNING\033[0m");
        outln("  (Cannot connect to /tmp/sentinel.sock)");
    } else {
        outln("Sentinel Daemon:      \033[32mRUNNING\033[0m");
    }
    outln();

    // Check database
    auto db_dir = TRY(get_database_path());
    StringBuilder db_path_builder;
    db_path_builder.append(db_dir);
    db_path_builder.append("/policies.db"sv);
    auto db_path = TRY(db_path_builder.to_string());

    if (FileSystem::exists(db_path)) {
        outln("Policy Database:      \033[32mEXISTS\033[0m");
        outln("  Path: {}", db_path);

        // Get file size
        struct stat st;
        if (stat(db_path.bytes_as_string_view().characters_without_null_termination(), &st) == 0) {
            outln("  Size: {} bytes", st.st_size);
        }

        // Open database and get counts
        auto pg_result = PolicyGraph::create(db_dir.to_byte_string());
        if (!pg_result.is_error()) {
            auto& pg = *pg_result.value();
            auto policy_count = pg.get_policy_count();
            auto threat_count = pg.get_threat_count();

            if (!policy_count.is_error()) {
                outln("  Policies: {}", policy_count.value());
            }
            if (!threat_count.is_error()) {
                outln("  Threats: {}", threat_count.value());
            }
        }
    } else {
        outln("Policy Database:      \033[33mNOT FOUND\033[0m");
        outln("  Expected at: {}", db_path);
    }
    outln();

    // Check quarantine
    auto quarantine_dir = TRY(Quarantine::get_quarantine_directory());
    if (FileSystem::exists(quarantine_dir)) {
        outln("Quarantine Directory: \033[32mEXISTS\033[0m");
        outln("  Path: {}", quarantine_dir);

        auto entries_result = Quarantine::list_all_entries();
        if (!entries_result.is_error()) {
            outln("  Files: {}", entries_result.value().size());
        }
    } else {
        outln("Quarantine Directory: \033[33mNOT FOUND\033[0m");
        outln("  Expected at: {}", quarantine_dir);
    }
    outln();

    // Check YARA rules
    auto config_dir = Core::StandardPaths::config_directory();
    StringBuilder rules_path_builder;
    rules_path_builder.append(config_dir);
    rules_path_builder.append("/ladybird/sentinel/rules"sv);
    auto rules_path = TRY(rules_path_builder.to_string());

    if (FileSystem::exists(rules_path)) {
        outln("YARA Rules:           \033[32mFOUND\033[0m");
        outln("  Path: {}", rules_path);
    } else {
        outln("YARA Rules:           \033[33mNOT FOUND\033[0m");
        outln("  Expected at: {}", rules_path);
    }

    return {};
}

static ErrorOr<void> command_list_policies()
{
    auto db_dir = TRY(get_database_path());
    auto pg = TRY(PolicyGraph::create(db_dir.to_byte_string()));

    auto policies = TRY(pg->list_policies());

    if (policies.is_empty()) {
        outln("No policies found.");
        return {};
    }

    outln("=== Policies ({}) ===\n", policies.size());

    for (auto const& policy : policies) {
        outln("Policy ID: {}", policy.id);
        outln("  Rule: {}", policy.rule_name);

        if (policy.url_pattern.has_value())
            outln("  URL Pattern: {}", policy.url_pattern.value());

        if (policy.file_hash.has_value())
            outln("  File Hash: {}", policy.file_hash.value());

        if (policy.mime_type.has_value())
            outln("  MIME Type: {}", policy.mime_type.value());

        outln("  Action: {}", [&] {
            switch (policy.action) {
            case PolicyGraph::PolicyAction::Allow:
                return "Allow";
            case PolicyGraph::PolicyAction::Block:
                return "Block";
            case PolicyGraph::PolicyAction::Quarantine:
                return "Quarantine";
            case PolicyGraph::PolicyAction::BlockAutofill:
                return "Block Autofill";
            case PolicyGraph::PolicyAction::WarnUser:
                return "Warn User";
            }
            return "Unknown";
        }());

        outln("  Created: {} by {}", policy.created_at.seconds_since_epoch(), policy.created_by);
        outln("  Hit Count: {}", policy.hit_count);

        if (policy.last_hit.has_value())
            outln("  Last Hit: {}", policy.last_hit.value().seconds_since_epoch());

        outln();
    }

    return {};
}

static ErrorOr<void> command_show_policy(i64 policy_id)
{
    auto db_dir = TRY(get_database_path());
    auto pg = TRY(PolicyGraph::create(db_dir.to_byte_string()));

    auto policy = TRY(pg->get_policy(policy_id));

    outln("=== Policy {} ===\n", policy.id);
    outln("Rule Name:     {}", policy.rule_name);

    if (policy.url_pattern.has_value())
        outln("URL Pattern:   {}", policy.url_pattern.value());
    else
        outln("URL Pattern:   (any)");

    if (policy.file_hash.has_value())
        outln("File Hash:     {}", policy.file_hash.value());
    else
        outln("File Hash:     (any)");

    if (policy.mime_type.has_value())
        outln("MIME Type:     {}", policy.mime_type.value());
    else
        outln("MIME Type:     (any)");

    outln("Action:        {}", [&] {
        switch (policy.action) {
        case PolicyGraph::PolicyAction::Allow:
            return "Allow";
        case PolicyGraph::PolicyAction::Block:
            return "Block";
        case PolicyGraph::PolicyAction::Quarantine:
            return "Quarantine";
        case PolicyGraph::PolicyAction::BlockAutofill:
            return "Block Autofill";
        case PolicyGraph::PolicyAction::WarnUser:
            return "Warn User";
        }
        return "Unknown";
    }());

    outln("Created At:    {}", policy.created_at.seconds_since_epoch());
    outln("Created By:    {}", policy.created_by);

    if (policy.expires_at.has_value())
        outln("Expires At:    {}", policy.expires_at.value().seconds_since_epoch());
    else
        outln("Expires At:    Never");

    outln("Hit Count:     {}", policy.hit_count);

    if (policy.last_hit.has_value())
        outln("Last Hit:      {}", policy.last_hit.value().seconds_since_epoch());
    else
        outln("Last Hit:      Never");

    return {};
}

static ErrorOr<void> command_list_quarantine()
{
    auto entries = TRY(Quarantine::list_all_entries());

    if (entries.is_empty()) {
        outln("No quarantined files found.");
        return {};
    }

    outln("=== Quarantined Files ({}) ===\n", entries.size());

    for (auto const& entry : entries) {
        outln("ID: {}", entry.quarantine_id);
        outln("  Filename:      {}", entry.filename);
        outln("  URL:           {}", entry.original_url);
        outln("  SHA256:        {}", entry.sha256);
        outln("  Size:          {} bytes", entry.file_size);
        outln("  Detected:      {}", entry.detection_time);
        outln("  Rules:         {}", String::join(", "sv, entry.rule_names));
        outln();
    }

    return {};
}

static ErrorOr<void> command_restore(String const& quarantine_id, String const& destination_dir)
{
    outln("Restoring quarantine ID {} to {}...", quarantine_id, destination_dir);

    // Check if destination directory exists
    if (!FileSystem::exists(destination_dir)) {
        return Error::from_string_literal("Destination directory does not exist");
    }

    TRY(Quarantine::restore_file(quarantine_id, destination_dir));

    outln("\033[32mSuccessfully restored file.\033[0m");
    return {};
}

static ErrorOr<void> command_vacuum()
{
    auto db_dir = TRY(get_database_path());
    auto pg = TRY(PolicyGraph::create(db_dir.to_byte_string()));

    outln("Running database vacuum...");

    TRY(pg->vacuum_database());

    outln("\033[32mVacuum completed successfully.\033[0m");
    return {};
}

static ErrorOr<void> command_verify()
{
    auto db_dir = TRY(get_database_path());
    StringBuilder db_path_builder;
    db_path_builder.append(db_dir);
    db_path_builder.append("/policies.db"sv);
    auto db_path = TRY(db_path_builder.to_string());

    outln("Verifying database integrity...");

    // Try to open database
    auto pg_result = PolicyGraph::create(db_dir.to_byte_string());
    if (pg_result.is_error()) {
        outln("\033[31mERROR: Cannot open database: {}\033[0m", pg_result.error());
        return pg_result.release_error();
    }

    auto pg = pg_result.release_value();

    // Try to count policies
    auto policy_count = TRY(pg->get_policy_count());
    outln("  Policies: {}", policy_count);

    // Try to count threats
    auto threat_count = TRY(pg->get_threat_count());
    outln("  Threats: {}", threat_count);

    outln("\033[32mDatabase integrity verified.\033[0m");
    return {};
}

static ErrorOr<void> command_backup()
{
    auto db_dir = TRY(get_database_path());
    StringBuilder db_path_builder;
    db_path_builder.append(db_dir);
    db_path_builder.append("/policies.db"sv);
    auto db_path = TRY(db_path_builder.to_string());

    // Create backup path with timestamp
    auto now = UnixDateTime::now();
    StringBuilder backup_path_builder;
    backup_path_builder.append(db_dir);
    backup_path_builder.appendff("/policies.db.backup.{}", now.seconds_since_epoch());
    auto backup_path = TRY(backup_path_builder.to_string());

    outln("Backing up database...");
    outln("  Source: {}", db_path);
    outln("  Destination: {}", backup_path);

    // Copy file
    TRY(FileSystem::copy_file_or_directory(backup_path, db_path, FileSystem::RecursionMode::Disallowed, FileSystem::LinkMode::Disallowed, FileSystem::AddDuplicateFileMarker::No, FileSystem::PreserveMode::Nothing));

    outln("\033[32mBackup created successfully.\033[0m");
    return {};
}

static void print_usage()
{
    outln("Usage: sentinel-cli <command> [arguments]\n");
    outln("Commands:");
    outln("  status                      Show Sentinel system status");
    outln("  list-policies               List all policies");
    outln("  show-policy <id>            Show details of a specific policy");
    outln("  list-quarantine             List all quarantined files");
    outln("  restore <id> <path>         Restore quarantined file to path");
    outln("  vacuum                      Vacuum database (reclaim space)");
    outln("  verify                      Verify database integrity");
    outln("  backup                      Create database backup");
    outln();
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (arguments.argc < 2) {
        print_usage();
        return 1;
    }

    StringView command = arguments.strings[1];

    if (command == "status"sv) {
        TRY(command_status());
    } else if (command == "list-policies"sv) {
        TRY(command_list_policies());
    } else if (command == "show-policy"sv) {
        if (arguments.argc < 3) {
            warnln("Error: Missing policy ID");
            return 1;
        }
        auto policy_id = TRY(String::from_utf8(arguments.strings[2])).to_number<i64>().value();
        TRY(command_show_policy(policy_id));
    } else if (command == "list-quarantine"sv) {
        TRY(command_list_quarantine());
    } else if (command == "restore"sv) {
        if (arguments.argc < 4) {
            warnln("Error: Missing quarantine ID or destination path");
            return 1;
        }
        auto quarantine_id = TRY(String::from_utf8(arguments.strings[2]));
        auto dest_path = TRY(String::from_utf8(arguments.strings[3]));
        TRY(command_restore(quarantine_id, dest_path));
    } else if (command == "vacuum"sv) {
        TRY(command_vacuum());
    } else if (command == "verify"sv) {
        TRY(command_verify());
    } else if (command == "backup"sv) {
        TRY(command_backup());
    } else {
        warnln("Error: Unknown command '{}'", command);
        print_usage();
        return 1;
    }

    return 0;
}
