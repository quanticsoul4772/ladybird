/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "YARAScanWorkerPool.h"
#include <LibCore/EventLoop.h>
#include <unistd.h>

namespace RequestServer {

ErrorOr<NonnullOwnPtr<YARAScanWorkerPool>> YARAScanWorkerPool::create(
    SecurityTap* security_tap,
    size_t num_threads)
{
    if (!security_tap) {
        return Error::from_string_literal("SecurityTap is required for worker pool");
    }

    if (num_threads == 0 || num_threads > 16) {
        return Error::from_string_literal("Invalid thread count (must be 1-16)");
    }

    auto pool = adopt_own(*new YARAScanWorkerPool(security_tap, num_threads));
    return pool;
}

YARAScanWorkerPool::YARAScanWorkerPool(SecurityTap* security_tap, size_t num_threads)
    : m_security_tap(security_tap)
    , m_num_threads(num_threads)
{
}

YARAScanWorkerPool::~YARAScanWorkerPool()
{
    if (m_running) {
        auto result = stop();
        if (result.is_error()) {
            dbgln("YARAScanWorkerPool: Error during shutdown: {}", result.error());
        }
    }
}

ErrorOr<void> YARAScanWorkerPool::start()
{
    if (m_running) {
        return Error::from_string_literal("Worker pool already running");
    }

    dbgln("YARAScanWorkerPool: Starting {} worker threads", m_num_threads);

    m_running = true;
    m_threads.ensure_capacity(m_num_threads);

    // Start worker threads
    for (size_t i = 0; i < m_num_threads; i++) {
        pthread_t thread;
        int result = pthread_create(&thread, nullptr, worker_thread_entry, this);
        if (result != 0) {
            // Failed to create thread - stop all threads and return error
            stop().release_value_but_fixme_should_propagate_errors();
            return Error::from_errno(result);
        }
        m_threads.append(thread);
        dbgln("YARAScanWorkerPool: Started worker thread {}", i + 1);
    }

    dbgln("YARAScanWorkerPool: All {} worker threads started successfully", m_num_threads);
    return {};
}

ErrorOr<void> YARAScanWorkerPool::stop()
{
    if (!m_running) {
        return {};
    }

    dbgln("YARAScanWorkerPool: Stopping worker threads...");

    // Signal shutdown to queue
    m_queue.shutdown();
    m_running = false;

    // Wait for all threads to finish
    for (auto& thread : m_threads) {
        void* retval;
        int result = pthread_join(thread, &retval);
        if (result != 0) {
            dbgln("YARAScanWorkerPool: Warning - pthread_join failed: {}", strerror(result));
        }
    }

    m_threads.clear();
    dbgln("YARAScanWorkerPool: All worker threads stopped");

    return {};
}

ErrorOr<void> YARAScanWorkerPool::enqueue_scan(
    SecurityTap::DownloadMetadata const& metadata,
    ByteBuffer content,
    SecurityTap::ScanCallback callback)
{
    if (!m_running) {
        return Error::from_string_literal("Worker pool not running");
    }

    // Calculate priority based on content size (smaller files = higher priority)
    // Priority 0-9: 0-1MB (immediate)
    // Priority 10-99: 1-10MB (high)
    // Priority 100-999: 10-100MB (normal)
    size_t priority = content.size() / (1024 * 1024); // MB
    if (priority > 999) {
        priority = 999; // Cap at 999
    }

    // Create scan request
    ScanRequest request {
        .request_id = MUST(String::formatted("scan_{}", metadata.sha256)),
        .content = move(content),
        .callback = move(callback),
        .enqueued_time = UnixDateTime::now(),
        .priority = priority,
    };

    // Enqueue the request
    TRY(m_queue.enqueue(move(request)));

    // Update telemetry
    {
        Threading::MutexLocker locker(m_telemetry_mutex);
        m_telemetry.current_queue_depth = m_queue.size();
        if (m_telemetry.current_queue_depth > m_telemetry.max_queue_depth_seen) {
            m_telemetry.max_queue_depth_seen = m_telemetry.current_queue_depth;
        }
    }

    return {};
}

void* YARAScanWorkerPool::worker_thread_entry(void* arg)
{
    auto* pool = static_cast<YARAScanWorkerPool*>(arg);
    pool->worker_thread();
    return nullptr;
}

void YARAScanWorkerPool::worker_thread()
{
    dbgln("YARAScanWorkerPool: Worker thread started (tid={})", gettid());

    // Update active worker count
    {
        Threading::MutexLocker locker(m_telemetry_mutex);
        m_telemetry.active_workers++;
    }

    while (m_running) {
        // Dequeue next scan request (blocks if queue is empty)
        auto request_opt = m_queue.dequeue();

        // If empty, we're shutting down
        if (!request_opt.has_value()) {
            break;
        }

        auto request = request_opt.release_value();

        // Execute the scan
        execute_scan(request);

        // Update telemetry
        {
            Threading::MutexLocker locker(m_telemetry_mutex);
            m_telemetry.current_queue_depth = m_queue.size();
        }
    }

    // Update active worker count
    {
        Threading::MutexLocker locker(m_telemetry_mutex);
        m_telemetry.active_workers--;
    }

    dbgln("YARAScanWorkerPool: Worker thread exiting (tid={})", gettid());
}

void YARAScanWorkerPool::execute_scan(ScanRequest& request)
{
    auto start_time = UnixDateTime::now();

    // Check if request has been waiting too long (timeout in queue)
    auto wait_time = start_time - request.enqueued_time;
    if (wait_time > MAX_SCAN_TIMEOUT) {
        dbgln("YARAScanWorkerPool: Request {} timed out in queue (waited {}s)",
            request.request_id, wait_time.to_seconds());

        // Update telemetry
        {
            Threading::MutexLocker locker(m_telemetry_mutex);
            m_telemetry.total_scans_timeout++;
        }

        // Schedule callback with timeout error
        schedule_callback(
            move(request.callback),
            Error::from_string_literal("Scan timed out in queue"));
        return;
    }

    // Perform the actual YARA scan using SecurityTap
    // This is the blocking operation that runs in the worker thread
    SecurityTap::DownloadMetadata metadata {
        .url = "async-scan"_string.to_byte_string(),
        .filename = request.request_id.to_byte_string(),
        .mime_type = "application/octet-stream"_string.to_byte_string(),
        .sha256 = request.request_id.to_byte_string(),
        .size_bytes = request.content.size(),
    };

    auto scan_result = m_security_tap->inspect_download(metadata, request.content.bytes());

    auto end_time = UnixDateTime::now();
    auto scan_duration = end_time - start_time;

    // Update telemetry
    {
        Threading::MutexLocker locker(m_telemetry_mutex);
        if (scan_result.is_error()) {
            m_telemetry.total_scans_failed++;
        } else {
            m_telemetry.total_scans_completed++;
        }

        m_telemetry.total_scan_time = m_telemetry.total_scan_time + scan_duration;

        if (scan_duration < m_telemetry.min_scan_time) {
            m_telemetry.min_scan_time = scan_duration;
        }
        if (scan_duration > m_telemetry.max_scan_time) {
            m_telemetry.max_scan_time = scan_duration;
        }
    }

    dbgln("YARAScanWorkerPool: Scan completed for {} in {}ms (queue_wait={}ms)",
        request.request_id,
        scan_duration.to_milliseconds(),
        wait_time.to_milliseconds());

    // Schedule callback execution on main event loop
    schedule_callback(move(request.callback), move(scan_result));
}

void YARAScanWorkerPool::schedule_callback(
    SecurityTap::ScanCallback callback,
    ErrorOr<SecurityTap::ScanResult> result)
{
    // Schedule callback execution on the main event loop thread
    // This ensures callbacks are executed on the IPC thread, not worker threads
    Core::EventLoop::current().deferred_invoke([callback = move(callback), result = move(result)]() mutable {
        callback(move(result));
    });
}

WorkerPoolTelemetry YARAScanWorkerPool::get_telemetry() const
{
    Threading::MutexLocker locker(m_telemetry_mutex);
    return m_telemetry;
}

}
