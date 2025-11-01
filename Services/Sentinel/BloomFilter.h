/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/BitStream.h>
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Vector.h>
#include <LibCrypto/Hash/HashFunction.h>
#include <LibCrypto/Hash/SHA2.h>

namespace Sentinel {

class BloomFilter {
public:
    // Default configuration for 100M items with ~0.1% false positive rate
    // Formula: m = -n*ln(p) / (ln(2)^2) where n=100M, p=0.001
    // This gives ~1.44GB, but we'll use a smaller size for practicality
    static constexpr size_t DEFAULT_SIZE_BITS = 1'200'000'000; // ~150MB
    static constexpr size_t DEFAULT_NUM_HASHES = 10; // Optimal for p=0.001

    static ErrorOr<NonnullOwnPtr<BloomFilter>> create(
        size_t size_bits = DEFAULT_SIZE_BITS,
        size_t num_hashes = DEFAULT_NUM_HASHES);

    // Add an item to the filter
    void add(String const& item);
    void add(ReadonlyBytes data);

    // Check if an item might be in the filter
    // Returns true if the item *might* be present (or definitely is)
    // Returns false if the item is definitely not present
    bool contains(String const& item) const;
    bool contains(ReadonlyBytes data) const;

    // Clear all bits in the filter
    void clear();

    // Get filter statistics
    size_t size_bits() const { return m_size_bits; }
    size_t num_hashes() const { return m_num_hashes; }
    size_t bits_set() const;
    double estimated_false_positive_rate() const;

    // Estimate number of items added (uses approximation formula)
    size_t estimated_item_count() const;

    // Serialize/deserialize for persistence
    ErrorOr<ByteBuffer> serialize() const;
    static ErrorOr<NonnullOwnPtr<BloomFilter>> deserialize(ReadonlyBytes data);

    // Merge another bloom filter into this one (must have same parameters)
    ErrorOr<void> merge(BloomFilter const& other);

private:
    BloomFilter(size_t size_bits, size_t num_hashes);

    // Generate hash values for an item using double hashing
    // Uses SHA256 to generate two base hashes, then combines them
    Vector<u64> hash_item(ReadonlyBytes data) const;

    // Get/set bit at position
    bool get_bit(size_t position) const;
    void set_bit(size_t position);

    size_t m_size_bits { 0 };
    size_t m_num_hashes { 0 };
    ByteBuffer m_bits;
};

}