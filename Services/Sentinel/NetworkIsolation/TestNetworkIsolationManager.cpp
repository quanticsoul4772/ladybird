/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <Services/Sentinel/NetworkIsolation/NetworkIsolationManager.h>
#include <Services/Sentinel/NetworkIsolation/IPTablesBackend.h>
#include <Services/Sentinel/NetworkIsolation/NFTablesBackend.h>
#include <Services/Sentinel/NetworkIsolation/ProcessMonitor.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace Sentinel::NetworkIsolation;

TEST_CASE(firewall_backend_detection)
{
    // Test automatic firewall backend detection
    auto manager_result = NetworkIsolationManager::create(
        NetworkIsolationManager::FirewallBackend::Auto, true);

    if (manager_result.is_error()) {
        // It's okay if no firewall backend is available in test environment
        EXPECT(manager_result.error().string_literal().contains("No supported firewall"sv));
        return;
    }

    auto manager = manager_result.release_value();
    auto backend = manager->get_backend();

    // Should have detected either iptables or nftables
    EXPECT(backend == NetworkIsolationManager::FirewallBackend::IPTables ||
           backend == NetworkIsolationManager::FirewallBackend::NFTables);
}

TEST_CASE(iptables_rule_generation)
{
    // Test iptables backend in dry-run mode
    auto backend_result = NetworkIsolation::IPTablesBackend::create(true);

    if (backend_result.is_error()) {
        // Skip if iptables not available
        return;
    }

    auto backend = backend_result.release_value();
    auto rules = backend->apply_isolation(12345);

    EXPECT(!rules.is_error());

    auto rule_list = rules.release_value();
    EXPECT(rule_list.size() == 3);

    // Check that rules contain expected patterns
    bool has_loopback = false;
    bool has_log = false;
    bool has_drop = false;

    for (auto const& rule : rule_list) {
        if (rule.contains("127.0.0.1"))
            has_loopback = true;
        if (rule.contains("LOG"))
            has_log = true;
        if (rule.contains("DROP"))
            has_drop = true;
    }

    EXPECT(has_loopback);
    EXPECT(has_log);
    EXPECT(has_drop);
}

TEST_CASE(nftables_rule_generation)
{
    // Test nftables backend in dry-run mode
    auto backend_result = NetworkIsolation::NFTablesBackend::create(true);

    if (backend_result.is_error()) {
        // Skip if nftables not available
        return;
    }

    auto backend = backend_result.release_value();

    // Use our own PID for testing
    pid_t test_pid = getpid();
    auto rules = backend->apply_isolation(test_pid);

    EXPECT(!rules.is_error());

    auto rule_list = rules.release_value();
    EXPECT(rule_list.size() == 3);

    // Check that rules contain expected patterns
    bool has_loopback = false;
    bool has_log = false;
    bool has_drop = false;

    for (auto const& rule : rule_list) {
        if (rule.contains("127.0.0"))
            has_loopback = true;
        if (rule.contains("log"))
            has_log = true;
        if (rule.contains("drop"))
            has_drop = true;
    }

    EXPECT(has_loopback);
    EXPECT(has_log);
    EXPECT(has_drop);
}

TEST_CASE(process_monitoring)
{
    // Create a short-lived child process
    pid_t child_pid = fork();

    if (child_pid == 0) {
        // Child process: sleep briefly then exit
        usleep(500000); // 0.5 seconds
        exit(0);
    }

    // Parent process: monitor the child
    bool callback_invoked = false;
    pid_t exited_pid = 0;

    auto monitor = MUST(NetworkIsolation::ProcessMonitor::create([&](pid_t pid) {
        callback_invoked = true;
        exited_pid = pid;
    }));

    MUST(monitor->monitor_process(child_pid));

    // Wait for child to exit and callback to be invoked
    // Process monitor checks every 1 second, so wait up to 2 seconds
    for (int i = 0; i < 20; i++) {
        usleep(100000); // 0.1 seconds
        if (callback_invoked)
            break;
    }

    // Wait for child process to avoid zombie
    int status;
    waitpid(child_pid, &status, 0);

    EXPECT(callback_invoked);
    EXPECT(exited_pid == child_pid);
}

TEST_CASE(critical_process_blocking_prevention)
{
    // Test that critical processes cannot be isolated
    auto manager = MUST(NetworkIsolationManager::create(
        NetworkIsolationManager::FirewallBackend::Auto, true));

    // Try to isolate PID 1 (init/systemd)
    auto result = manager->isolate_process(1, "Test"_string);

    EXPECT(result.is_error());
    EXPECT(result.error().string_literal().contains("critical"sv));
}

TEST_CASE(process_isolation_and_restoration)
{
    // Test basic isolation and restoration in dry-run mode
    auto manager = MUST(NetworkIsolationManager::create(
        NetworkIsolationManager::FirewallBackend::Auto, true));

    // Use a non-critical PID (our own PID is safe to test with in dry-run)
    pid_t test_pid = getpid();

    // Isolate process
    auto isolate_result = manager->isolate_process(test_pid, "Test isolation"_string);
    EXPECT(!isolate_result.is_error());

    // Check that process is tracked
    EXPECT(manager->is_process_isolated(test_pid));

    // List isolated processes
    auto list = MUST(manager->list_isolated_processes());
    EXPECT(list.size() == 1);
    EXPECT(list[0].pid == test_pid);
    EXPECT(list[0].reason == "Test isolation"_string);

    // Restore process
    auto restore_result = manager->restore_process(test_pid);
    EXPECT(!restore_result.is_error());

    // Check that process is no longer tracked
    EXPECT(!manager->is_process_isolated(test_pid));

    // Check statistics
    auto stats = manager->get_statistics();
    EXPECT(stats.total_isolated_processes == 1);
    EXPECT(stats.active_isolated_processes == 0);
    EXPECT(stats.total_cleanup_operations == 1);
}

TEST_CASE(duplicate_isolation_handling)
{
    // Test that isolating the same process twice is handled gracefully
    auto manager = MUST(NetworkIsolationManager::create(
        NetworkIsolationManager::FirewallBackend::Auto, true));

    pid_t test_pid = getpid();

    // Isolate process first time
    MUST(manager->isolate_process(test_pid, "First isolation"_string));

    // Isolate process second time (should be idempotent)
    auto second_result = manager->isolate_process(test_pid, "Second isolation"_string);
    EXPECT(!second_result.is_error());

    // Should still only have one isolated process
    auto list = MUST(manager->list_isolated_processes());
    EXPECT(list.size() == 1);

    // Cleanup
    MUST(manager->restore_process(test_pid));
}

TEST_CASE(cleanup_all_processes)
{
    // Test cleanup_all functionality
    auto manager = MUST(NetworkIsolationManager::create(
        NetworkIsolationManager::FirewallBackend::Auto, true));

    pid_t test_pid = getpid();

    // Isolate multiple "processes" (using same PID with different reasons for testing)
    MUST(manager->isolate_process(test_pid, "Test 1"_string));

    // Cleanup all
    auto cleanup_result = manager->cleanup_all();
    EXPECT(!cleanup_result.is_error());

    // Check that all processes are cleaned up
    EXPECT(!manager->is_process_isolated(test_pid));

    auto stats = manager->get_statistics();
    EXPECT(stats.active_isolated_processes == 0);
}
