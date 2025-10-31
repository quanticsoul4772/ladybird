/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ScanQueue.h"
#include "SecurityTap.h"
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Time.h>
#include <LibCore/EventLoop.h>
#include <LibThreading/Mutex.h>
#include <pthread.h>

namespace RequestServer {

using AK::Duration;

// Telemetry data for monitoring worker pool performance
struct WorkerPoolTelemetry {
    size_t total_scans_completed { 0 };
    size_t total_scans_failed { 0 };
    size_t total_scans_timeout { 0 };
    Duration total_scan_time { Duration::zero() };
    Duration min_scan_time { Duration::from_seconds(999999) };
    Duration max_scan_time { Duration::zero() };
    size_t current_queue_depth { 0 };
    size_t max_queue_depth_seen { 0 };
    size_t active_workers { 0 };
};

// Thread pool for asynchronous YARA scanning
class YARAScanWorkerPool {
public:
    static ErrorOr<NonnullOwnPtr<YARAScanWorkerPool>> create(
        SecurityTap* security_tap,
        size_t num_threads = 4);

    ~YARAScanWorkerPool();

    // Enqueue a scan request for async processing
    ErrorOr<void> enqueue_scan(
        SecurityTap::DownloadMetadata const& metadata,
        ByteBuffer content,
        SecurityTap::ScanCallback callback);

    // Start the worker threads
    ErrorOr<void> start();

    // Stop the worker threads (graceful shutdown)
    ErrorOr<void> stop();

    // Get current telemetry data
    WorkerPoolTelemetry get_telemetry() const;

    // Maximum time allowed per scan before timeout
    static constexpr Duration MAX_SCAN_TIMEOUT = Duration::from_seconds(60);

private:
    YARAScanWorkerPool(SecurityTap* security_tap, size_t num_threads);

    // Worker thread entry point
    static void* worker_thread_entry(void* arg);
    void worker_thread();

    // Execute a single scan request
    void execute_scan(ScanRequest& request);

    // Schedule callback execution on main event loop
    void schedule_callback(
        SecurityTap::ScanCallback callback,
        ErrorOr<ScanResult> result);

    SecurityTap* m_security_tap { nullptr };
    size_t m_num_threads { 4 };
    Vector<pthread_t> m_threads;
    ScanQueue m_queue;
    bool m_running { false };

    // Telemetry (protected by mutex)
    mutable Threading::Mutex m_telemetry_mutex;
    WorkerPoolTelemetry m_telemetry;
};

}
