/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <Services/Sentinel/LRUCache.h>

using namespace Sentinel;

TEST_CASE(basic_get_put)
{
    LRUCache<String, int> cache(3);

    // Put three values
    MUST(cache.put("key1"_string, 100));
    MUST(cache.put("key2"_string, 200));
    MUST(cache.put("key3"_string, 300));

    EXPECT_EQ(cache.size(), 3u);

    // Get values
    auto val1 = cache.get("key1"_string);
    EXPECT(val1.has_value());
    EXPECT_EQ(val1.value(), 100);

    auto val2 = cache.get("key2"_string);
    EXPECT(val2.has_value());
    EXPECT_EQ(val2.value(), 200);

    auto val3 = cache.get("key3"_string);
    EXPECT(val3.has_value());
    EXPECT_EQ(val3.value(), 300);
}

TEST_CASE(cache_miss)
{
    LRUCache<String, int> cache(5);

    MUST(cache.put("key1"_string, 100));

    // Try to get non-existent key
    auto val = cache.get("key2"_string);
    EXPECT(!val.has_value());

    // Verify metrics
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.hits, 0u);
    EXPECT_EQ(metrics.misses, 1u);
}

TEST_CASE(cache_hit)
{
    LRUCache<String, int> cache(5);

    MUST(cache.put("key1"_string, 100));

    // Get existing key
    auto val = cache.get("key1"_string);
    EXPECT(val.has_value());
    EXPECT_EQ(val.value(), 100);

    // Verify metrics
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.hits, 1u);
    EXPECT_EQ(metrics.misses, 0u);
}

TEST_CASE(lru_eviction)
{
    LRUCache<String, int> cache(3);

    // Fill cache to capacity
    MUST(cache.put("key1"_string, 100));
    MUST(cache.put("key2"_string, 200));
    MUST(cache.put("key3"_string, 300));

    EXPECT_EQ(cache.size(), 3u);
    EXPECT(cache.is_full());

    // Add fourth item - should evict key1 (least recently used)
    MUST(cache.put("key4"_string, 400));

    EXPECT_EQ(cache.size(), 3u);

    // key1 should be evicted
    auto val1 = cache.get("key1"_string);
    EXPECT(!val1.has_value());

    // key2, key3, key4 should exist
    auto val2 = cache.get("key2"_string);
    EXPECT(val2.has_value());
    EXPECT_EQ(val2.value(), 200);

    auto val3 = cache.get("key3"_string);
    EXPECT(val3.has_value());
    EXPECT_EQ(val3.value(), 300);

    auto val4 = cache.get("key4"_string);
    EXPECT(val4.has_value());
    EXPECT_EQ(val4.value(), 400);

    // Verify eviction count
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 1u);
}

TEST_CASE(move_to_front_on_access)
{
    LRUCache<String, int> cache(3);

    // Fill cache
    MUST(cache.put("key1"_string, 100));
    MUST(cache.put("key2"_string, 200));
    MUST(cache.put("key3"_string, 300));

    // Access key1 - moves it to front
    auto val1 = cache.get("key1"_string);
    EXPECT(val1.has_value());

    // Add key4 - should evict key2 (now LRU)
    MUST(cache.put("key4"_string, 400));

    // key1 should still exist (was moved to front)
    val1 = cache.get("key1"_string);
    EXPECT(val1.has_value());
    EXPECT_EQ(val1.value(), 100);

    // key2 should be evicted
    auto val2 = cache.get("key2"_string);
    EXPECT(!val2.has_value());

    // key3 and key4 should exist
    auto val3 = cache.get("key3"_string);
    EXPECT(val3.has_value());

    auto val4 = cache.get("key4"_string);
    EXPECT(val4.has_value());
}

TEST_CASE(update_existing_key)
{
    LRUCache<String, int> cache(3);

    // Put initial value
    MUST(cache.put("key1"_string, 100));

    // Update with new value
    MUST(cache.put("key1"_string, 999));

    // Should have new value
    auto val = cache.get("key1"_string);
    EXPECT(val.has_value());
    EXPECT_EQ(val.value(), 999);

    // Size should still be 1
    EXPECT_EQ(cache.size(), 1u);

    // No evictions should occur
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 0u);
}

TEST_CASE(cache_full_scenario)
{
    LRUCache<String, int> cache(2);

    EXPECT(!cache.is_full());
    EXPECT(cache.is_empty());

    MUST(cache.put("key1"_string, 100));
    EXPECT(!cache.is_full());
    EXPECT(!cache.is_empty());

    MUST(cache.put("key2"_string, 200));
    EXPECT(cache.is_full());
    EXPECT(!cache.is_empty());

    // Add third item - triggers eviction
    MUST(cache.put("key3"_string, 300));
    EXPECT(cache.is_full());
    EXPECT_EQ(cache.size(), 2u);

    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 1u);
}

TEST_CASE(invalidate_all)
{
    LRUCache<String, int> cache(5);

    // Add multiple items
    MUST(cache.put("key1"_string, 100));
    MUST(cache.put("key2"_string, 200));
    MUST(cache.put("key3"_string, 300));

    EXPECT_EQ(cache.size(), 3u);

    // Invalidate cache
    cache.invalidate();

    EXPECT_EQ(cache.size(), 0u);
    EXPECT(cache.is_empty());

    // All items should be gone
    auto val1 = cache.get("key1"_string);
    EXPECT(!val1.has_value());

    auto val2 = cache.get("key2"_string);
    EXPECT(!val2.has_value());

    auto val3 = cache.get("key3"_string);
    EXPECT(!val3.has_value());

    // Verify invalidation count
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.invalidations, 1u);
}

TEST_CASE(large_cache_performance)
{
    // Test with 1000 entries
    LRUCache<String, int> cache(1000);

    // Fill cache completely
    for (int i = 0; i < 1000; i++) {
        auto key = MUST(String::formatted("key{}", i));
        MUST(cache.put(key, i));
    }

    EXPECT_EQ(cache.size(), 1000u);
    EXPECT(cache.is_full());

    // Access all items - should be O(1) for each
    for (int i = 0; i < 1000; i++) {
        auto key = MUST(String::formatted("key{}", i));
        auto val = cache.get(key);
        EXPECT(val.has_value());
        EXPECT_EQ(val.value(), i);
    }

    // All accesses should be cache hits
    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.hits, 1000u);
    EXPECT_EQ(metrics.misses, 0u);

    // Add one more - should evict first item (key0)
    MUST(cache.put("key1000"_string, 1000));

    auto val = cache.get("key0"_string);
    EXPECT(!val.has_value());

    metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 1u);
}

TEST_CASE(memory_usage_validation)
{
    // Test that cache properly manages memory
    LRUCache<String, int> cache(100);

    // Add 200 items - should evict 100
    for (int i = 0; i < 200; i++) {
        auto key = MUST(String::formatted("key{}", i));
        MUST(cache.put(key, i));
    }

    // Cache should have exactly 100 items
    EXPECT_EQ(cache.size(), 100u);

    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 100u);

    // First 100 items should be evicted
    for (int i = 0; i < 100; i++) {
        auto key = MUST(String::formatted("key{}", i));
        auto val = cache.get(key);
        EXPECT(!val.has_value());
    }

    // Last 100 items should exist
    for (int i = 100; i < 200; i++) {
        auto key = MUST(String::formatted("key{}", i));
        auto val = cache.get(key);
        EXPECT(val.has_value());
        EXPECT_EQ(val.value(), i);
    }
}

TEST_CASE(statistics_accuracy)
{
    LRUCache<String, int> cache(5);

    // Add 3 items
    MUST(cache.put("key1"_string, 100));
    MUST(cache.put("key2"_string, 200));
    MUST(cache.put("key3"_string, 300));

    // 2 hits
    (void)cache.get("key1"_string);
    (void)cache.get("key2"_string);

    // 3 misses
    (void)cache.get("key4"_string);
    (void)cache.get("key5"_string);
    (void)cache.get("key6"_string);

    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.hits, 2u);
    EXPECT_EQ(metrics.misses, 3u);
    EXPECT_EQ(metrics.current_size, 3u);
    EXPECT_EQ(metrics.max_size, 5u);

    // Hit rate should be 2/5 = 40%
    EXPECT_EQ(metrics.hit_rate(), 40.0);

    // Reset metrics
    cache.reset_metrics();
    metrics = cache.get_metrics();
    EXPECT_EQ(metrics.hits, 0u);
    EXPECT_EQ(metrics.misses, 0u);
    EXPECT_EQ(metrics.current_size, 3u); // Size unchanged
}

TEST_CASE(optional_value_support)
{
    // Test cache with Optional<int> values (like PolicyGraphCache)
    LRUCache<String, Optional<int>> cache(5);

    // Put Some and None values
    MUST(cache.put("key1"_string, Optional<int>(100)));
    MUST(cache.put("key2"_string, Optional<int>()));  // None

    // Get Some value
    auto val1 = cache.get("key1"_string);
    EXPECT(val1.has_value());
    EXPECT(val1.value().has_value());
    EXPECT_EQ(val1.value().value(), 100);

    // Get None value
    auto val2 = cache.get("key2"_string);
    EXPECT(val2.has_value());
    EXPECT(!val2.value().has_value());
}

TEST_CASE(complex_eviction_pattern)
{
    LRUCache<String, int> cache(4);

    // Fill cache
    MUST(cache.put("A"_string, 1));
    MUST(cache.put("B"_string, 2));
    MUST(cache.put("C"_string, 3));
    MUST(cache.put("D"_string, 4));

    // Access pattern: B, A, C (D is now LRU)
    (void)cache.get("B"_string);
    (void)cache.get("A"_string);
    (void)cache.get("C"_string);

    // Add E - should evict D
    MUST(cache.put("E"_string, 5));

    auto valD = cache.get("D"_string);
    EXPECT(!valD.has_value());

    // B, A, C should still exist
    EXPECT(cache.get("B"_string).has_value());
    EXPECT(cache.get("A"_string).has_value());
    EXPECT(cache.get("C"_string).has_value());
    EXPECT(cache.get("E"_string).has_value());
}

TEST_CASE(single_entry_cache)
{
    LRUCache<String, int> cache(1);

    MUST(cache.put("key1"_string, 100));
    EXPECT_EQ(cache.size(), 1u);
    EXPECT(cache.is_full());

    auto val1 = cache.get("key1"_string);
    EXPECT(val1.has_value());
    EXPECT_EQ(val1.value(), 100);

    // Add second item - should evict first
    MUST(cache.put("key2"_string, 200));
    EXPECT_EQ(cache.size(), 1u);

    auto val2 = cache.get("key2"_string);
    EXPECT(val2.has_value());
    EXPECT_EQ(val2.value(), 200);

    val1 = cache.get("key1"_string);
    EXPECT(!val1.has_value());

    auto metrics = cache.get_metrics();
    EXPECT_EQ(metrics.evictions, 1u);
}
