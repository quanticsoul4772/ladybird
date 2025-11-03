/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel::NetworkIsolation {

class IPTablesBackend;
class NFTablesBackend;
class ProcessMonitor;

struct IsolatedProcess {
    pid_t pid;
    String reason;
    UnixDateTime isolated_at;
    Vector<String> firewall_rules;
};

struct Statistics {
    size_t total_isolated_processes { 0 };
    size_t total_rules_applied { 0 };
    size_t total_cleanup_operations { 0 };
    size_t active_isolated_processes { 0 };
};

class NetworkIsolationManager {
public:
    enum class FirewallBackend {
        IPTables,
        NFTables,
        Auto
    };

    static ErrorOr<NonnullOwnPtr<NetworkIsolationManager>> create(
        FirewallBackend backend = FirewallBackend::Auto,
        bool dry_run = false);

    ~NetworkIsolationManager();

    // Isolate a single process from network access
    ErrorOr<void> isolate_process(pid_t pid, String const& reason);

    // Restore network access for a process
    ErrorOr<void> restore_process(pid_t pid);

    // Isolate entire process tree (parent + all children)
    ErrorOr<void> isolate_process_tree(pid_t root_pid);

    // List all currently isolated processes
    ErrorOr<Vector<IsolatedProcess>> list_isolated_processes() const;

    // Remove all firewall rules and cleanup
    ErrorOr<void> cleanup_all();

    // Get isolation statistics
    Statistics get_statistics() const { return m_statistics; }

    // Check if process is isolated
    bool is_process_isolated(pid_t pid) const;

    // Get firewall backend in use
    FirewallBackend get_backend() const { return m_backend; }

private:
    NetworkIsolationManager(FirewallBackend backend, bool dry_run);

    ErrorOr<void> initialize();
    ErrorOr<void> detect_firewall_backend();
    ErrorOr<bool> is_critical_process(pid_t pid);
    ErrorOr<Vector<pid_t>> get_process_children(pid_t pid);
    ErrorOr<void> apply_firewall_rules(pid_t pid);
    ErrorOr<void> remove_firewall_rules(pid_t pid);
    void on_process_exit(pid_t pid);

    FirewallBackend m_backend;
    bool m_dry_run;
    HashMap<pid_t, IsolatedProcess> m_isolated_processes;
    OwnPtr<IPTablesBackend> m_iptables_backend;
    OwnPtr<NFTablesBackend> m_nftables_backend;
    OwnPtr<ProcessMonitor> m_process_monitor;
    Statistics m_statistics;
};

}
