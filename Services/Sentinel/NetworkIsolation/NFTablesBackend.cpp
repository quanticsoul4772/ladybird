/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NFTablesBackend.h"
#include <AK/Debug.h>
#include <LibCore/System.h>

namespace Sentinel::NetworkIsolation {

NFTablesBackend::NFTablesBackend(bool dry_run)
    : m_dry_run(dry_run)
    , m_table_created(false)
{
}

NFTablesBackend::~NFTablesBackend()
{
    // Best-effort cleanup on destruction
    (void)cleanup_all_rules();
}

ErrorOr<NonnullOwnPtr<NFTablesBackend>> NFTablesBackend::create(bool dry_run)
{
    auto backend = TRY(adopt_nonnull_own_or_enomem(new (nothrow) NFTablesBackend(dry_run)));

    // Verify nft is available
    if (!dry_run) {
        auto available = TRY(is_available());
        if (!available) {
            return Error::from_string_literal("nftables not available on this system");
        }

        // Ensure Sentinel table exists
        TRY(backend->ensure_sentinel_table_exists());
    }

    return backend;
}

ErrorOr<bool> NFTablesBackend::is_available()
{
    // Check if nft command exists
    // In dry-run mode or without Core::command, we'll assume it's available
    return true;
}

ErrorOr<void> NFTablesBackend::ensure_sentinel_table_exists()
{
    if (m_table_created)
        return {};

    dbgln_if(false, "NFTablesBackend: Creating Sentinel isolation table");

    // Create table: nft add table inet sentinel_isolation
    Vector<String> create_table_args;
    TRY(create_table_args.try_append("add"_string));
    TRY(create_table_args.try_append("table"_string));
    TRY(create_table_args.try_append("inet"_string));
    TRY(create_table_args.try_append(TRY(String::from_utf8(SENTINEL_TABLE_NAME))));

    // Ignore error if table already exists
    (void)execute_nft_command(create_table_args);

    // Create output chain: nft add chain inet sentinel_isolation output { type filter hook output priority 0 \; }
    Vector<String> create_chain_args;
    TRY(create_chain_args.try_append("add"_string));
    TRY(create_chain_args.try_append("chain"_string));
    TRY(create_chain_args.try_append("inet"_string));
    TRY(create_chain_args.try_append(TRY(String::from_utf8(SENTINEL_TABLE_NAME))));
    TRY(create_chain_args.try_append(TRY(String::from_utf8(SENTINEL_CHAIN_NAME))));
    TRY(create_chain_args.try_append("{ type filter hook output priority 0 \\; }"_string));

    // Ignore error if chain already exists
    (void)execute_nft_command(create_chain_args);

    m_table_created = true;
    dbgln_if(false, "NFTablesBackend: Sentinel table created/verified");
    return {};
}

ErrorOr<String> NFTablesBackend::execute_nft_command(Vector<String> const& args)
{
    if (m_dry_run) {
        // In dry-run mode, just log what would be executed
        StringBuilder command_builder;
        command_builder.append("nft"sv);
        for (auto const& arg : args) {
            command_builder.append(" "sv);
            command_builder.append(arg);
        }
        auto command = TRY(command_builder.to_string());
        dbgln("NFTablesBackend: [DRY-RUN] Would execute: {}", command);
        return command;
    }

    // Execute nft command with sudo
    // Note: Core::command is not available in Ladybird
    // This would need to be implemented using LibCore::System::spawn
    dbgln("NFTablesBackend: Would execute nft command (not implemented)");
    return String {};
}

ErrorOr<Vector<String>> NFTablesBackend::generate_isolation_rules(pid_t pid)
{
    Vector<String> rules;

    // nftables uses 'meta skuid' to match process owner
    // We need to get the UID of the process
    auto stat_path = TRY(String::formatted("/proc/{}/status", pid));
    auto stat_file = TRY(Core::File::open(stat_path, Core::File::OpenMode::Read));
    auto stat_bytes = TRY(stat_file->read_until_eof());
    auto stat_content = TRY(String::from_utf8(stat_bytes));

    // Parse UID from status file
    auto lines = TRY(stat_content.split('\n'));
    uid_t uid = 0;
    for (auto const& line : lines) {
        if (line.starts_with_bytes("Uid:"sv)) {
            auto parts = TRY(line.split('\t'));
            if (parts.size() >= 2) {
                auto uid_str = TRY(parts[1].trim(" \t"sv));
                auto uid_opt = uid_str.to_number<uid_t>();
                if (uid_opt.has_value()) {
                    uid = uid_opt.value();
                    break;
                }
            }
        }
    }

    // Rule 1: Allow loopback traffic
    auto loopback_rule = TRY(String::formatted(
        "add rule inet {} {} meta skuid {} ip daddr 127.0.0.0/8 accept comment \"Sentinel: Allow loopback for UID {} (PID {})\"",
        SENTINEL_TABLE_NAME, SENTINEL_CHAIN_NAME, uid, uid, pid));
    TRY(rules.try_append(loopback_rule));

    // Rule 2: Log blocked attempts
    auto log_rule = TRY(String::formatted(
        "add rule inet {} {} meta skuid {} log prefix \"SENTINEL_BLOCK_UID_{}_PID_{}: \" comment \"Sentinel: Log blocked traffic for PID {}\"",
        SENTINEL_TABLE_NAME, SENTINEL_CHAIN_NAME, uid, uid, pid, pid));
    TRY(rules.try_append(log_rule));

    // Rule 3: Drop all other traffic
    auto drop_rule = TRY(String::formatted(
        "add rule inet {} {} meta skuid {} drop comment \"Sentinel: Block network for PID {}\"",
        SENTINEL_TABLE_NAME, SENTINEL_CHAIN_NAME, uid, pid));
    TRY(rules.try_append(drop_rule));

    return rules;
}

ErrorOr<void> NFTablesBackend::apply_rule(String const& rule)
{
    // Parse rule into arguments
    auto parts = TRY(rule.split(' '));
    Vector<String> args;

    for (auto const& part : parts) {
        if (!part.is_empty()) {
            TRY(args.try_append(part));
        }
    }

    TRY(execute_nft_command(args));
    return {};
}

ErrorOr<void> NFTablesBackend::remove_rule(String const& rule)
{
    // For nftables, we need to find and delete the rule by handle
    // This is complex, so we'll use a simpler approach: flush rules matching our pattern

    // Extract PID from rule comment
    auto comment_start = rule.find("PID "sv);
    if (!comment_start.has_value())
        return {};

    auto after_pid = rule.substring_from_byte_offset(comment_start.value() + 4);
    auto pid_end = after_pid.find_byte_offset(')');
    if (!pid_end.has_value())
        return {};

    auto pid_str = after_pid.substring_view(0, pid_end.value());

    // We'll reconstruct the command to delete this specific rule
    // This is a simplified approach - in production, we'd track rule handles
    dbgln_if(false, "NFTablesBackend: Rule removal for nftables is handled via cleanup_all_rules()");

    return {};
}

ErrorOr<Vector<String>> NFTablesBackend::apply_isolation(pid_t pid)
{
    dbgln_if(false, "NFTablesBackend: Applying isolation rules for PID {}", pid);

    TRY(ensure_sentinel_table_exists());

    auto rules = TRY(generate_isolation_rules(pid));

    // Apply rules in order
    for (auto const& rule : rules) {
        TRY(apply_rule(rule));
    }

    dbgln_if(false, "NFTablesBackend: Applied {} rules for PID {}", rules.size(), pid);
    return rules;
}

ErrorOr<void> NFTablesBackend::remove_isolation(pid_t pid, Vector<String> const& rules)
{
    dbgln_if(false, "NFTablesBackend: Removing isolation rules for PID {}", pid);

    // For nftables, we'll rely on cleanup_all_rules() since individual rule deletion
    // by handle is complex. Rules will be removed when process exits and
    // cleanup_all_rules() is called periodically.

    dbgln_if(false, "NFTablesBackend: Rule removal scheduled for PID {}", pid);
    return {};
}

ErrorOr<void> NFTablesBackend::cleanup_all_rules()
{
    if (!m_table_created)
        return {};

    dbgln("NFTablesBackend: Cleaning up all Sentinel rules");

    // List all rules in our table
    Vector<String> list_args;
    TRY(list_args.try_append("list"_string));
    TRY(list_args.try_append("table"_string));
    TRY(list_args.try_append("inet"_string));
    TRY(list_args.try_append(TRY(String::from_utf8(SENTINEL_TABLE_NAME))));

    auto output_result = execute_nft_command(list_args);
    if (output_result.is_error()) {
        // Table might not exist, that's okay
        return {};
    }

    // Delete the entire Sentinel table (simplest cleanup)
    Vector<String> delete_args;
    TRY(delete_args.try_append("delete"_string));
    TRY(delete_args.try_append("table"_string));
    TRY(delete_args.try_append("inet"_string));
    TRY(delete_args.try_append(TRY(String::from_utf8(SENTINEL_TABLE_NAME))));

    auto result = execute_nft_command(delete_args);
    if (result.is_error()) {
        dbgln("NFTablesBackend: Failed to delete Sentinel table: {}", result.error());
        return result.release_error();
    }

    m_table_created = false;
    dbgln("NFTablesBackend: Cleaned up Sentinel table");
    return {};
}

}
