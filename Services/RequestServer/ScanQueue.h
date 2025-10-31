/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibThreading/Mutex.h>
#include <Services/RequestServer/SecurityTap.h>

namespace RequestServer {

// Scan request queued for asynchronous processing
struct ScanRequest {
    String request_id;
    ByteBuffer content;
    Function<void(ErrorOr<SecurityTap::ScanResult>)> callback;
    UnixDateTime enqueued_time;
    size_t priority; // Lower number = higher priority (small files first)
};

// Thread-safe queue for scan requests with priority ordering
class ScanQueue {
public:
    ScanQueue() = default;
    ~ScanQueue() = default;

    // Enqueue a scan request (thread-safe)
    // Returns error if queue is full
    ErrorOr<void> enqueue(ScanRequest request);

    // Dequeue highest priority scan request (thread-safe)
    // Blocks if queue is empty and not shutting down
    // Returns empty Optional if shutting down
    Optional<ScanRequest> dequeue();

    // Get current queue depth (thread-safe)
    size_t size() const;

    // Signal shutdown - causes dequeue() to return empty Optional
    void shutdown();

    // Check if queue is shutting down
    bool is_shutting_down() const;

    // Maximum queue size before rejecting new requests
    static constexpr size_t MAX_QUEUE_SIZE = 100;

private:
    mutable Threading::Mutex m_mutex;
    Vector<ScanRequest> m_queue;
    bool m_shutting_down { false };
};

}
