/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/Optional.h>
#include <AK/String.h>

namespace Sentinel {

// Generic O(1) LRU Cache implementation using doubly-linked list + HashMap
// All operations (get, put, evict) are O(1) time complexity
template<typename Key, typename Value>
class LRUCache {
public:
    struct CacheMetrics {
        size_t hits { 0 };
        size_t misses { 0 };
        size_t evictions { 0 };
        size_t invalidations { 0 };
        size_t current_size { 0 };
        size_t max_size { 0 };

        double hit_rate() const
        {
            auto total = hits + misses;
            return total > 0 ? (hits * 100.0) / total : 0.0;
        }
    };

    explicit LRUCache(size_t capacity = 1000)
        : m_head(nullptr)
        , m_tail(nullptr)
        , m_capacity(capacity)
        , m_size(0)
    {
    }

    ~LRUCache()
    {
        clear();
    }

    // Disable copy construction and assignment
    LRUCache(LRUCache const&) = delete;
    LRUCache& operator=(LRUCache const&) = delete;

    // Enable move construction and assignment
    LRUCache(LRUCache&&) noexcept;
    LRUCache& operator=(LRUCache&&) noexcept;

    // Get value from cache, returns empty Optional on miss
    // On hit, moves entry to front (most recently used)
    // Time complexity: O(1)
    Optional<Value> get(Key const& key);

    // Put key-value pair into cache
    // If cache is full, evicts least recently used entry
    // Time complexity: O(1)
    ErrorOr<void> put(Key const& key, Value const& value);

    // Clear all entries from cache
    void clear();

    // Invalidate the cache (alias for clear, for API compatibility)
    void invalidate()
    {
        clear();
        m_cache_invalidations++;
    }

    // Get cache metrics
    CacheMetrics get_metrics() const
    {
        return CacheMetrics {
            .hits = m_cache_hits,
            .misses = m_cache_misses,
            .evictions = m_cache_evictions,
            .invalidations = m_cache_invalidations,
            .current_size = m_size,
            .max_size = m_capacity
        };
    }

    // Reset metrics counters
    void reset_metrics()
    {
        m_cache_hits = 0;
        m_cache_misses = 0;
        m_cache_evictions = 0;
        m_cache_invalidations = 0;
    }

    // Get current cache size
    size_t size() const { return m_size; }

    // Get cache capacity
    size_t capacity() const { return m_capacity; }

    // Check if cache is empty
    bool is_empty() const { return m_size == 0; }

    // Check if cache is full
    bool is_full() const { return m_size >= m_capacity; }

private:
    struct CacheNode {
        Key key;
        Value value;
        CacheNode* prev;
        CacheNode* next;

        CacheNode(Key k, Value v)
            : key(move(k))
            , value(move(v))
            , prev(nullptr)
            , next(nullptr)
        {
        }
    };

    // Move node to front of list (most recently used)
    // Time complexity: O(1)
    void move_to_front(CacheNode* node);

    // Remove node from list
    // Time complexity: O(1)
    void remove_node(CacheNode* node);

    // Add node to front of list
    // Time complexity: O(1)
    void add_to_front(CacheNode* node);

    // Evict least recently used entry (tail)
    // Time complexity: O(1)
    void evict_lru();

    HashMap<Key, CacheNode*> m_cache_map;  // O(1) lookup
    CacheNode* m_head;                      // Most recently used
    CacheNode* m_tail;                      // Least recently used
    size_t m_capacity;
    size_t m_size;

    // Metrics
    mutable size_t m_cache_hits { 0 };
    mutable size_t m_cache_misses { 0 };
    mutable size_t m_cache_evictions { 0 };
    mutable size_t m_cache_invalidations { 0 };
};

// Implementation

template<typename Key, typename Value>
LRUCache<Key, Value>::LRUCache(LRUCache&& other) noexcept
    : m_cache_map(move(other.m_cache_map))
    , m_head(other.m_head)
    , m_tail(other.m_tail)
    , m_capacity(other.m_capacity)
    , m_size(other.m_size)
    , m_cache_hits(other.m_cache_hits)
    , m_cache_misses(other.m_cache_misses)
    , m_cache_evictions(other.m_cache_evictions)
    , m_cache_invalidations(other.m_cache_invalidations)
{
    other.m_head = nullptr;
    other.m_tail = nullptr;
    other.m_size = 0;
}

template<typename Key, typename Value>
LRUCache<Key, Value>& LRUCache<Key, Value>::operator=(LRUCache&& other) noexcept
{
    if (this != &other) {
        clear();
        m_cache_map = move(other.m_cache_map);
        m_head = other.m_head;
        m_tail = other.m_tail;
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        m_cache_hits = other.m_cache_hits;
        m_cache_misses = other.m_cache_misses;
        m_cache_evictions = other.m_cache_evictions;
        m_cache_invalidations = other.m_cache_invalidations;

        other.m_head = nullptr;
        other.m_tail = nullptr;
        other.m_size = 0;
    }
    return *this;
}

template<typename Key, typename Value>
Optional<Value> LRUCache<Key, Value>::get(Key const& key)
{
    auto it = m_cache_map.find(key);
    if (it == m_cache_map.end()) {
        m_cache_misses++;
        return {};
    }

    // Cache hit
    m_cache_hits++;

    // Move to front (most recently used)
    move_to_front(it->value);

    return it->value->value;
}

template<typename Key, typename Value>
ErrorOr<void> LRUCache<Key, Value>::put(Key const& key, Value const& value)
{
    auto it = m_cache_map.find(key);
    if (it != m_cache_map.end()) {
        // Key already exists, update value and move to front
        it->value->value = value;
        move_to_front(it->value);
        return {};
    }

    // New key
    if (is_full()) {
        // Cache is full, evict LRU entry
        evict_lru();
    }

    // Create new node
    auto* node = new (nothrow) CacheNode(key, value);
    if (node == nullptr) {
        return Error::from_errno(ENOMEM);
    }

    // Add to front of list
    add_to_front(node);

    // Add to HashMap
    m_cache_map.set(key, node);
    m_size++;

    return {};
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::clear()
{
    // Delete all nodes
    auto* current = m_head;
    while (current != nullptr) {
        auto* next = current->next;
        delete current;
        current = next;
    }

    m_cache_map.clear();
    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::move_to_front(CacheNode* node)
{
    if (node == m_head) {
        // Already at front
        return;
    }

    // Remove from current position
    remove_node(node);

    // Add to front
    add_to_front(node);
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::remove_node(CacheNode* node)
{
    if (node->prev != nullptr) {
        node->prev->next = node->next;
    } else {
        // Node is head
        m_head = node->next;
    }

    if (node->next != nullptr) {
        node->next->prev = node->prev;
    } else {
        // Node is tail
        m_tail = node->prev;
    }

    node->prev = nullptr;
    node->next = nullptr;
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::add_to_front(CacheNode* node)
{
    node->next = m_head;
    node->prev = nullptr;

    if (m_head != nullptr) {
        m_head->prev = node;
    }

    m_head = node;

    if (m_tail == nullptr) {
        // First node
        m_tail = node;
    }
}

template<typename Key, typename Value>
void LRUCache<Key, Value>::evict_lru()
{
    if (m_tail == nullptr) {
        // Cache is empty
        return;
    }

    // Remove tail (least recently used)
    auto* lru_node = m_tail;

    // Remove from HashMap
    m_cache_map.remove(lru_node->key);

    // Remove from list
    if (m_tail->prev != nullptr) {
        m_tail = m_tail->prev;
        m_tail->next = nullptr;
    } else {
        // Only one node in cache
        m_head = nullptr;
        m_tail = nullptr;
    }

    // Delete node
    delete lru_node;

    m_size--;
    m_cache_evictions++;
}

}
