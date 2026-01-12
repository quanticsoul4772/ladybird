/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NetworkIsolationManager.h"
#include "IPTablesBackend.h"
#include "NFTablesBackend.h"
#include "ProcessMonitor.h"
#include <AK/Debug.h>
#include <LibCore/System.h>
#include <unistd.h>

namespace Sentinel::NetworkIsolation {

// Critical system processes that should never be isolated
static constexpr pid_t CRITICAL_PIDS[] = {
    1,  // init/systemd
};

static constexpr StringView CRITICAL_PROCESS_NAMES[] = {
    "systemd"sv,
    "init"sv,
    "sshd"sv,
    "systemd-resolved"sv,
    "systemd-networkd"sv,
    "NetworkManager"sv,
    "dbus-daemon"sv,
};

NetworkIsolationManager::NetworkIsolationManager(FirewallBackend backend, bool dry_run)
    : m_backend(backend)
    , m_dry_run(dry_run)
{
}

NetworkIsolationManager::~NetworkIsolationManager()
{
    // Best-effort cleanup on destruction
    (void)cleanup_all();
}

ErrorOr<NonnullOwnPtr<NetworkIsolationManager>> NetworkIsolationManager::create(
    FirewallBackend backend, bool dry_run)
{
    auto manager = TRY(adopt_nonnull_own_or_enomem(new (nothrow) NetworkIsolationManager(backend, dry_run)));
    TRY(manager->initialize());
    return manager;
}

ErrorOr<void> NetworkIsolationManager::initialize()
{
    // Detect firewall backend if Auto
    if (m_backend == FirewallBackend::Auto) {
        TRY(detect_firewall_backend());
    }

    // Initialize appropriate backend
    if (m_backend == FirewallBackend::NFTables) {
        m_nftables_backend = TRY(NFTablesBackend::create(m_dry_run));
        dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Using nftables backend");
    } else if (m_backend == FirewallBackend::IPTables) {
        m_iptables_backend = TRY(IPTablesBackend::create(m_dry_run));
        dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Using iptables backend");
    } else {
        return Error::from_string_literal("No supported firewall backend available");
    }

    // Initialize process monitor with exit callback
    m_process_monitor = TRY(ProcessMonitor::create([this](pid_t pid) {
        on_process_exit(pid);
    }));

    dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Initialized successfully");
    return {};
}

ErrorOr<void> NetworkIsolationManager::detect_firewall_backend()
{
    // Try nftables first (modern systems)
    auto nft_available = TRY(NFTablesBackend::is_available());
    if (nft_available) {
        m_backend = FirewallBackend::NFTables;
        dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Detected nftables");
        return {};
    }

    // Fall back to iptables (older systems)
    auto iptables_available = TRY(IPTablesBackend::is_available());
    if (iptables_available) {
        m_backend = FirewallBackend::IPTables;
        dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Detected iptables");
        return {};
    }

    return Error::from_string_literal("No supported firewall backend found (tried nftables and iptables)");
}

ErrorOr<bool> NetworkIsolationManager::is_critical_process(pid_t pid)
{
    // Check critical PIDs
    for (auto critical_pid : CRITICAL_PIDS) {
        if (pid == critical_pid)
            return true;
    }

    // Check process name
    auto comm_path = String::formatted("/proc/{}/comm", pid);
    auto comm_file = TRY(Core::File::open(comm_path, Core::File::OpenMode::Read));
    auto comm_bytes = TRY(comm_file->read_until_eof());
    auto comm = TRY(String::from_utf8(comm_bytes));
    comm = TRY(comm.trim("\n"sv));

    for (auto critical_name : CRITICAL_PROCESS_NAMES) {
        if (comm == critical_name) {
            dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Refusing to isolate critical process: {} (PID {})",
                comm, pid);
            return true;
        }
    }

    return false;
}

ErrorOr<Vector<pid_t>> NetworkIsolationManager::get_process_children(pid_t pid)
{
    Vector<pid_t> children;

    // Read /proc/<pid>/task/<tid>/children
    auto children_path = String::formatted("/proc/{}/task/{}/children", pid, pid);
    auto children_file_or_error = Core::File::open(children_path, Core::File::OpenMode::Read);

    if (children_file_or_error.is_error()) {
        // Process may have exited, return empty list
        return children;
    }

    auto children_file = children_file_or_error.release_value();
    auto children_bytes = TRY(children_file->read_until_eof());
    auto children_str = TRY(String::from_utf8(children_bytes));

    // Parse space-separated PIDs
    auto parts = TRY(children_str.split(' '));
    for (auto const& part : parts) {
        auto trimmed = TRY(part.trim("\n\t "sv));
        if (trimmed.is_empty())
            continue;

        auto child_pid = trimmed.to_number<pid_t>();
        if (child_pid.has_value()) {
            TRY(children.try_append(child_pid.value()));
        }
    }

    return children;
}

ErrorOr<void> NetworkIsolationManager::apply_firewall_rules(pid_t pid)
{
    Vector<String> rules;

    if (m_backend == FirewallBackend::NFTables) {
        rules = TRY(m_nftables_backend->apply_isolation(pid));
    } else if (m_backend == FirewallBackend::IPTables) {
        rules = TRY(m_iptables_backend->apply_isolation(pid));
    } else {
        return Error::from_string_literal("No firewall backend initialized");
    }

    // Store rules for later cleanup
    if (m_isolated_processes.contains(pid)) {
        m_isolated_processes.get(pid)->value.firewall_rules = move(rules);
    }

    m_statistics.total_rules_applied += rules.size();
    return {};
}

ErrorOr<void> NetworkIsolationManager::remove_firewall_rules(pid_t pid)
{
    auto process_iter = m_isolated_processes.find(pid);
    if (process_iter == m_isolated_processes.end()) {
        return Error::from_string_literal("Process not isolated");
    }

    auto const& rules = process_iter->value.firewall_rules;

    if (m_backend == FirewallBackend::NFTables) {
        TRY(m_nftables_backend->remove_isolation(pid, rules));
    } else if (m_backend == FirewallBackend::IPTables) {
        TRY(m_iptables_backend->remove_isolation(pid, rules));
    }

    return {};
}

ErrorOr<void> NetworkIsolationManager::isolate_process(pid_t pid, String const& reason)
{
    // Safety check: Don't isolate critical processes
    if (TRY(is_critical_process(pid))) {
        return Error::from_string_literal("Cannot isolate critical system process");
    }

    // Check if already isolated
    if (m_isolated_processes.contains(pid)) {
        dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Process {} already isolated", pid);
        return {};
    }

    dbgln("NetworkIsolationManager: Isolating process {} (reason: {})", pid, reason);

    // Apply firewall rules
    IsolatedProcess isolated_process {
        .pid = pid,
        .reason = reason,
        .isolated_at = UnixDateTime::now(),
        .firewall_rules = {}
    };

    TRY(m_isolated_processes.try_set(pid, isolated_process));
    TRY(apply_firewall_rules(pid));

    // Start monitoring for process exit
    TRY(m_process_monitor->monitor_process(pid));

    m_statistics.total_isolated_processes++;
    m_statistics.active_isolated_processes++;

    dbgln("NetworkIsolationManager: Successfully isolated process {}", pid);
    return {};
}

ErrorOr<void> NetworkIsolationManager::restore_process(pid_t pid)
{
    if (!m_isolated_processes.contains(pid)) {
        return Error::from_string_literal("Process not isolated");
    }

    dbgln("NetworkIsolationManager: Restoring network access for process {}", pid);

    // Remove firewall rules
    TRY(remove_firewall_rules(pid));

    // Stop monitoring
    m_process_monitor->unmonitor_process(pid);

    // Remove from tracking
    m_isolated_processes.remove(pid);
    m_statistics.active_isolated_processes--;
    m_statistics.total_cleanup_operations++;

    dbgln("NetworkIsolationManager: Successfully restored process {}", pid);
    return {};
}

ErrorOr<void> NetworkIsolationManager::isolate_process_tree(pid_t root_pid)
{
    // Isolate root process first
    TRY(isolate_process(root_pid, "Process tree isolation"_string));

    // Get all children recursively
    Vector<pid_t> to_process;
    TRY(to_process.try_append(root_pid));

    HashTable<pid_t> processed;

    while (!to_process.is_empty()) {
        auto current_pid = to_process.take_last();

        if (processed.contains(current_pid))
            continue;

        TRY(processed.try_set(current_pid));

        auto children = TRY(get_process_children(current_pid));
        for (auto child_pid : children) {
            // Isolate child
            auto result = isolate_process(child_pid, "Child of isolated process tree"_string);
            if (result.is_error()) {
                dbgln("NetworkIsolationManager: Failed to isolate child process {}: {}",
                    child_pid, result.error());
                continue;
            }

            TRY(to_process.try_append(child_pid));
        }
    }

    return {};
}

ErrorOr<Vector<IsolatedProcess>> NetworkIsolationManager::list_isolated_processes() const
{
    Vector<IsolatedProcess> processes;
    TRY(processes.try_ensure_capacity(m_isolated_processes.size()));

    for (auto const& [pid, process] : m_isolated_processes) {
        TRY(processes.try_append(process));
    }

    return processes;
}

ErrorOr<void> NetworkIsolationManager::cleanup_all()
{
    dbgln("NetworkIsolationManager: Cleaning up all isolated processes");

    // Stop monitoring first
    m_process_monitor->stop_monitoring();

    // Collect PIDs to cleanup (avoid modifying map during iteration)
    Vector<pid_t> pids_to_cleanup;
    for (auto const& [pid, _] : m_isolated_processes) {
        TRY(pids_to_cleanup.try_append(pid));
    }

    // Restore each process
    for (auto pid : pids_to_cleanup) {
        auto result = restore_process(pid);
        if (result.is_error()) {
            dbgln("NetworkIsolationManager: Failed to restore process {}: {}", pid, result.error());
        }
    }

    // Cleanup backend rules
    if (m_backend == FirewallBackend::NFTables && m_nftables_backend) {
        auto result = m_nftables_backend->cleanup_all_rules();
        if (result.is_error()) {
            dbgln("NetworkIsolationManager: Failed to cleanup nftables rules: {}", result.error());
        }
    } else if (m_backend == FirewallBackend::IPTables && m_iptables_backend) {
        auto result = m_iptables_backend->cleanup_all_rules();
        if (result.is_error()) {
            dbgln("NetworkIsolationManager: Failed to cleanup iptables rules: {}", result.error());
        }
    }

    dbgln("NetworkIsolationManager: Cleanup complete");
    return {};
}

bool NetworkIsolationManager::is_process_isolated(pid_t pid) const
{
    return m_isolated_processes.contains(pid);
}

void NetworkIsolationManager::on_process_exit(pid_t pid)
{
    dbgln_if(SENTINEL_DEBUG, "NetworkIsolationManager: Process {} exited, cleaning up", pid);

    auto result = restore_process(pid);
    if (result.is_error()) {
        dbgln("NetworkIsolationManager: Failed to cleanup exited process {}: {}", pid, result.error());
    }
}

}
