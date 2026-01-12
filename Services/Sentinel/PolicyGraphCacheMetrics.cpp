/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Cache metrics implementation - to be merged into PolicyGraph.cpp

// Replace get_cached:
Optional<Optional<int>> PolicyGraphCache::get_cached(String const& key)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_cache_misses++;
        return {};
    }

    // Cache hit
    m_cache_hits++;

    // Update LRU order
    update_lru(key);

    return it->value;
}

// Replace cache_policy:
void PolicyGraphCache::cache_policy(String const& key, Optional<int> policy_id)
{
    // If cache is full, evict least recently used entry
    if (m_cache.size() >= m_max_size && !m_cache.contains(key)) {
        if (!m_lru_order.is_empty()) {
            auto lru_key = m_lru_order.take_first();
            m_cache.remove(lru_key);
            m_cache_evictions++;
        }
    }

    m_cache.set(key, policy_id);
    update_lru(key);
}

// Replace invalidate and add new methods:
void PolicyGraphCache::invalidate()
{
    m_cache.clear();
    m_lru_order.clear();
    m_cache_invalidations++;
}

PolicyGraphCache::CacheMetrics PolicyGraphCache::get_metrics() const
{
    return CacheMetrics {
        .hits = m_cache_hits,
        .misses = m_cache_misses,
        .evictions = m_cache_evictions,
        .invalidations = m_cache_invalidations,
        .current_size = m_cache.size(),
        .max_size = m_max_size
    };
}

void PolicyGraphCache::reset_metrics()
{
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_cache_evictions = 0;
    m_cache_invalidations = 0;
}
