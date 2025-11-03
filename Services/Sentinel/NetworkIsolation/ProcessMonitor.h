/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/HashTable.h>
#include <LibThreading/Thread.h>
#include <pthread.h>

namespace Sentinel::NetworkIsolation {

// Monitors processes for termination and triggers cleanup callbacks
class ProcessMonitor {
public:
    using ExitCallback = Function<void(pid_t)>;

    static ErrorOr<NonnullOwnPtr<ProcessMonitor>> create(ExitCallback callback);

    ~ProcessMonitor();

    // Add a process to monitor
    ErrorOr<void> monitor_process(pid_t pid);

    // Stop monitoring a process
    void unmonitor_process(pid_t pid);

    // Stop monitoring all processes
    void stop_monitoring();

private:
    explicit ProcessMonitor(ExitCallback callback);

    ErrorOr<void> start_monitoring_thread();
    void monitoring_loop();
    ErrorOr<bool> is_process_alive(pid_t pid);

    ExitCallback m_exit_callback;
    HashTable<pid_t> m_monitored_processes;
    RefPtr<Threading::Thread> m_monitoring_thread;
    Atomic<bool> m_should_stop { false };
    pthread_mutex_t m_mutex;
};

}
