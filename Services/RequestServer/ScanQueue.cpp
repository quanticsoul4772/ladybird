/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ScanQueue.h"
#include <AK/QuickSort.h>

namespace RequestServer {

ScanQueue::ScanQueue()
    : m_condition_variable(make<Threading::ConditionVariable>(m_mutex))
{
}

ScanQueue::~ScanQueue()
{
    // ConditionVariable cleanup is automatic via RAII
}

ErrorOr<void> ScanQueue::enqueue(ScanRequest request)
{
    Threading::MutexLocker locker(m_mutex);

    // Check if queue is full
    if (m_queue.size() >= MAX_QUEUE_SIZE) {
        return Error::from_string_literal("Scan queue is full - rejecting request");
    }

    // Check if shutting down
    if (m_shutting_down) {
        return Error::from_string_literal("Scan queue is shutting down");
    }

    // Add to queue
    m_queue.append(move(request));

    // Sort by priority (lower number = higher priority)
    // This ensures small files are scanned first
    quick_sort(m_queue, [](ScanRequest const& a, ScanRequest const& b) {
        return a.priority < b.priority;
    });

    // Signal waiting worker threads that work is available
    m_condition_variable->signal();

    return {};
}

Optional<ScanRequest> ScanQueue::dequeue()
{
    Threading::MutexLocker locker(m_mutex);

    // Wait until queue has items or we're shutting down
    // This blocks efficiently using a condition variable instead of spinning
    m_condition_variable->wait_while([this] {
        return m_queue.is_empty() && !m_shutting_down;
    });

    // If shutting down and queue is empty, return nothing
    if (m_shutting_down && m_queue.is_empty()) {
        return {};
    }

    // Take the highest priority request (first in sorted queue)
    auto request = move(m_queue.first());
    m_queue.remove(0);

    return request;
}

size_t ScanQueue::size() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_queue.size();
}

void ScanQueue::shutdown()
{
    Threading::MutexLocker locker(m_mutex);
    m_shutting_down = true;
    // Wake all waiting threads so they can exit
    m_condition_variable->broadcast();
}

bool ScanQueue::is_shutting_down() const
{
    Threading::MutexLocker locker(m_mutex);
    return m_shutting_down;
}

}
