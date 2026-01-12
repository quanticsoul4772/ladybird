/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ProcessMonitor.h"
#include <AK/Debug.h>
#include <LibCore/System.h>
#include <signal.h>
#include <unistd.h>

namespace Sentinel::NetworkIsolation {

ProcessMonitor::ProcessMonitor(ExitCallback callback)
    : m_exit_callback(move(callback))
{
    pthread_mutex_init(&m_mutex, nullptr);
}

ProcessMonitor::~ProcessMonitor()
{
    stop_monitoring();
    pthread_mutex_destroy(&m_mutex);
}

ErrorOr<NonnullOwnPtr<ProcessMonitor>> ProcessMonitor::create(ExitCallback callback)
{
    auto monitor = TRY(adopt_nonnull_own_or_enomem(new (nothrow) ProcessMonitor(move(callback))));
    TRY(monitor->start_monitoring_thread());
    return monitor;
}

ErrorOr<void> ProcessMonitor::start_monitoring_thread()
{
    m_monitoring_thread = TRY(Threading::Thread::try_create([this] {
        monitoring_loop();
        return 0;
    }));

    m_monitoring_thread->start();
    dbgln_if(false, "ProcessMonitor: Monitoring thread started");
    return {};
}

void ProcessMonitor::monitoring_loop()
{
    while (!m_should_stop.load()) {
        // Sleep for 1 second between checks
        usleep(1000000);

        // Lock mutex to safely access monitored processes
        pthread_mutex_lock(&m_mutex);

        Vector<pid_t> exited_processes;

        // Check each monitored process
        for (auto pid : m_monitored_processes) {
            auto alive_result = is_process_alive(pid);
            if (alive_result.is_error() || !alive_result.value()) {
                // Process has exited
                if (exited_processes.try_append(pid).is_error()) {
                    dbgln("ProcessMonitor: Failed to track exited process {}", pid);
                }
            }
        }

        pthread_mutex_unlock(&m_mutex);

        // Call exit callback for each exited process
        for (auto pid : exited_processes) {
            dbgln_if(false, "ProcessMonitor: Detected exit of process {}", pid);
            m_exit_callback(pid);

            // Remove from monitored set
            pthread_mutex_lock(&m_mutex);
            m_monitored_processes.remove(pid);
            pthread_mutex_unlock(&m_mutex);
        }
    }

    dbgln_if(false, "ProcessMonitor: Monitoring thread exiting");
}

ErrorOr<bool> ProcessMonitor::is_process_alive(pid_t pid)
{
    // Send signal 0 to check if process exists
    // If kill returns 0, process exists
    // If kill returns -1 with ESRCH, process doesn't exist
    if (kill(pid, 0) == 0) {
        return true;
    }

    if (errno == ESRCH) {
        // Process does not exist
        return false;
    }

    // Other error (EPERM, etc.) - assume process still exists
    return true;
}

ErrorOr<void> ProcessMonitor::monitor_process(pid_t pid)
{
    pthread_mutex_lock(&m_mutex);
    auto result = m_monitored_processes.try_set(pid);
    pthread_mutex_unlock(&m_mutex);

    if (result.is_error()) {
        return Error::from_string_literal("Failed to add process to monitoring set");
    }

    dbgln_if(false, "ProcessMonitor: Now monitoring process {}", pid);
    return {};
}

void ProcessMonitor::unmonitor_process(pid_t pid)
{
    pthread_mutex_lock(&m_mutex);
    m_monitored_processes.remove(pid);
    pthread_mutex_unlock(&m_mutex);

    dbgln_if(false, "ProcessMonitor: Stopped monitoring process {}", pid);
}

void ProcessMonitor::stop_monitoring()
{
    dbgln_if(false, "ProcessMonitor: Stopping monitoring");

    m_should_stop.store(true);

    if (m_monitoring_thread) {
        (void)m_monitoring_thread->join();
        m_monitoring_thread = nullptr;
    }

    pthread_mutex_lock(&m_mutex);
    m_monitored_processes.clear();
    pthread_mutex_unlock(&m_mutex);

    dbgln_if(false, "ProcessMonitor: Monitoring stopped");
}

}
