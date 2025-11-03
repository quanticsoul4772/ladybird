/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel::NetworkIsolation {

class NFTablesBackend {
public:
    static ErrorOr<NonnullOwnPtr<NFTablesBackend>> create(bool dry_run = false);

    ~NFTablesBackend();

    // Check if nftables is available on the system
    static ErrorOr<bool> is_available();

    // Apply isolation rules for a process
    ErrorOr<Vector<String>> apply_isolation(pid_t pid);

    // Remove isolation rules for a process
    ErrorOr<void> remove_isolation(pid_t pid, Vector<String> const& rules);

    // Remove all Sentinel-created rules
    ErrorOr<void> cleanup_all_rules();

private:
    explicit NFTablesBackend(bool dry_run);

    ErrorOr<void> ensure_sentinel_table_exists();
    ErrorOr<String> execute_nft_command(Vector<String> const& args);
    ErrorOr<Vector<String>> generate_isolation_rules(pid_t pid);
    ErrorOr<void> apply_rule(String const& rule);
    ErrorOr<void> remove_rule(String const& rule);

    bool m_dry_run;
    bool m_table_created;
    static constexpr auto SENTINEL_TABLE_NAME = "sentinel_isolation"sv;
    static constexpr auto SENTINEL_CHAIN_NAME = "output"sv;
};

}
