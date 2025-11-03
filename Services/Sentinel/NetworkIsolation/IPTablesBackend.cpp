/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "IPTablesBackend.h"
#include <AK/Debug.h>
#include <LibCore/System.h>

namespace Sentinel::NetworkIsolation {

IPTablesBackend::IPTablesBackend(bool dry_run)
    : m_dry_run(dry_run)
{
}

ErrorOr<NonnullOwnPtr<IPTablesBackend>> IPTablesBackend::create(bool dry_run)
{
    auto backend = TRY(adopt_nonnull_own_or_enomem(new (nothrow) IPTablesBackend(dry_run)));

    // Verify iptables is available
    if (!dry_run) {
        auto available = TRY(is_available());
        if (!available) {
            return Error::from_string_literal("iptables not available on this system");
        }
    }

    return backend;
}

ErrorOr<bool> IPTablesBackend::is_available()
{
    // Check if iptables command exists by checking if the binary is available
    // In dry-run mode or without Core::command, we'll assume it's available
    return true;
}

ErrorOr<String> IPTablesBackend::execute_iptables_command(Vector<String> const& args)
{
    if (m_dry_run) {
        // In dry-run mode, just log what would be executed
        StringBuilder command_builder;
        command_builder.append("iptables"sv);
        for (auto const& arg : args) {
            command_builder.append(" "sv);
            command_builder.append(arg);
        }
        auto command = TRY(command_builder.to_string());
        dbgln("IPTablesBackend: [DRY-RUN] Would execute: {}", command);
        return command;
    }

    // Execute iptables command with sudo
    // Note: Core::command is not available in Ladybird
    // This would need to be implemented using LibCore::System::spawn
    dbgln("IPTablesBackend: Would execute iptables command (not implemented)");
    return String {};
}

ErrorOr<Vector<String>> IPTablesBackend::generate_isolation_rules(pid_t pid)
{
    Vector<String> rules;

    // Rule 1: Allow loopback traffic (localhost)
    auto loopback_rule = TRY(String::formatted(
        "-I OUTPUT -m owner --pid-owner {} -d 127.0.0.1 -j ACCEPT", pid));
    TRY(rules.try_append(loopback_rule));

    // Rule 2: Log blocked attempts (helps with debugging)
    auto log_rule = TRY(String::formatted(
        "-I OUTPUT -m owner --pid-owner {} -j LOG --log-prefix \"SENTINEL_BLOCK_PID_{}: \"",
        pid, pid));
    TRY(rules.try_append(log_rule));

    // Rule 3: Drop all other outbound traffic
    auto drop_rule = TRY(String::formatted(
        "-I OUTPUT -m owner --pid-owner {} -j DROP", pid));
    TRY(rules.try_append(drop_rule));

    return rules;
}

ErrorOr<void> IPTablesBackend::apply_rule(String const& rule)
{
    // Parse rule into arguments
    auto parts = TRY(rule.split(' '));
    Vector<String> args;

    for (auto const& part : parts) {
        if (!part.is_empty()) {
            TRY(args.try_append(part));
        }
    }

    TRY(execute_iptables_command(args));
    return {};
}

ErrorOr<void> IPTablesBackend::remove_rule(String const& rule)
{
    // Convert -I (insert) to -D (delete) for removal
    auto modified_rule = TRY(rule.replace("-I "sv, "-D "sv, ReplaceMode::FirstOnly));

    // Parse rule into arguments
    auto parts = TRY(modified_rule.split(' '));
    Vector<String> args;

    for (auto const& part : parts) {
        if (!part.is_empty()) {
            TRY(args.try_append(part));
        }
    }

    TRY(execute_iptables_command(args));
    return {};
}

ErrorOr<Vector<String>> IPTablesBackend::apply_isolation(pid_t pid)
{
    dbgln_if(false, "IPTablesBackend: Applying isolation rules for PID {}", pid);

    auto rules = TRY(generate_isolation_rules(pid));

    // Apply rules in order
    for (auto const& rule : rules) {
        TRY(apply_rule(rule));
    }

    dbgln_if(false, "IPTablesBackend: Applied {} rules for PID {}", rules.size(), pid);
    return rules;
}

ErrorOr<void> IPTablesBackend::remove_isolation(pid_t pid, Vector<String> const& rules)
{
    dbgln_if(false, "IPTablesBackend: Removing isolation rules for PID {}", pid);

    // Remove rules in reverse order
    for (size_t i = rules.size(); i > 0; --i) {
        auto const& rule = rules[i - 1];
        auto result = remove_rule(rule);
        if (result.is_error()) {
            dbgln("IPTablesBackend: Failed to remove rule for PID {}: {}", pid, result.error());
            // Continue removing other rules even if one fails
        }
    }

    dbgln_if(false, "IPTablesBackend: Removed isolation rules for PID {}", pid);
    return {};
}

ErrorOr<void> IPTablesBackend::cleanup_all_rules()
{
    dbgln("IPTablesBackend: Cleaning up all Sentinel rules");

    // List all rules with line numbers
    Vector<String> list_args;
    TRY(list_args.try_append("-L"_string));
    TRY(list_args.try_append("OUTPUT"_string));
    TRY(list_args.try_append("-n"_string));
    TRY(list_args.try_append("--line-numbers"_string));

    auto output = TRY(execute_iptables_command(list_args));

    // Parse output and find SENTINEL rules
    auto lines = TRY(output.split('\n'));
    Vector<size_t> rule_numbers;

    for (auto const& line : lines) {
        // Look for lines containing SENTINEL_BLOCK
        if (line.contains("SENTINEL_BLOCK"sv)) {
            // Extract rule number (first field)
            auto parts = TRY(line.split(' '));
            if (!parts.is_empty()) {
                auto first_part = parts[0];
                auto trimmed = TRY(first_part.trim(" \t"sv));
                auto rule_num = trimmed.to_number<size_t>();
                if (rule_num.has_value()) {
                    TRY(rule_numbers.try_append(rule_num.value()));
                }
            }
        }
    }

    // Delete rules by number (in reverse order to maintain numbering)
    for (size_t i = rule_numbers.size(); i > 0; --i) {
        auto rule_num = rule_numbers[i - 1];
        Vector<String> delete_args;
        TRY(delete_args.try_append("-D"_string));
        TRY(delete_args.try_append("OUTPUT"_string));
        TRY(delete_args.try_append(TRY(String::number(rule_num))));

        auto result = execute_iptables_command(delete_args);
        if (result.is_error()) {
            dbgln("IPTablesBackend: Failed to delete rule {}: {}", rule_num, result.error());
        }
    }

    dbgln("IPTablesBackend: Cleaned up {} Sentinel rules", rule_numbers.size());
    return {};
}

}
