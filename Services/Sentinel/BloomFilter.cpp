/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BloomFilter.h"
#include <AK/Math.h>
#include <AK/Random.h>
#include <LibCrypto/Hash/SHA2.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<BloomFilter>> BloomFilter::create(size_t size_bits, size_t num_hashes)
{
    if (size_bits == 0 || num_hashes == 0)
        return Error::from_string_literal("BloomFilter: Invalid parameters");

    // Calculate number of bytes needed (rounded up)
    size_t size_bytes = (size_bits + 7) / 8;

    auto filter = adopt_own(*new BloomFilter(size_bits, num_hashes));

    // Allocate bit array
    filter->m_bits = TRY(ByteBuffer::create_zeroed(size_bytes));

    dbgln("BloomFilter: Created with {} bits ({} MB), {} hash functions",
        size_bits, size_bytes / 1024 / 1024, num_hashes);

    return filter;
}

BloomFilter::BloomFilter(size_t size_bits, size_t num_hashes)
    : m_size_bits(size_bits)
    , m_num_hashes(num_hashes)
{
}

void BloomFilter::add(String const& item)
{
    add(item.bytes());
}

void BloomFilter::add(ReadonlyBytes data)
{
    auto hashes = hash_item(data);

    for (auto hash : hashes) {
        size_t position = hash % m_size_bits;
        set_bit(position);
    }
}

bool BloomFilter::contains(String const& item) const
{
    return contains(item.bytes());
}

bool BloomFilter::contains(ReadonlyBytes data) const
{
    auto hashes = hash_item(data);

    for (auto hash : hashes) {
        size_t position = hash % m_size_bits;
        if (!get_bit(position))
            return false;
    }

    return true;
}

void BloomFilter::clear()
{
    m_bits.zero_fill();
}

size_t BloomFilter::bits_set() const
{
    size_t count = 0;

    for (size_t byte_idx = 0; byte_idx < m_bits.size(); ++byte_idx) {
        u8 byte = m_bits[byte_idx];
        // Count set bits using Brian Kernighan's algorithm
        while (byte) {
            byte &= byte - 1;
            count++;
        }
    }

    return count;
}

double BloomFilter::estimated_false_positive_rate() const
{
    // Formula: (1 - e^(-k*n/m))^k
    // Where k = num_hashes, n = estimated items, m = size_bits

    size_t n = estimated_item_count();
    if (n == 0)
        return 0.0;

    double k = static_cast<double>(m_num_hashes);
    double m = static_cast<double>(m_size_bits);
    double n_double = static_cast<double>(n);

    double exponent = -k * n_double / m;
    double base = 1.0 - AK::exp(exponent);
    return AK::pow(base, k);
}

size_t BloomFilter::estimated_item_count() const
{
    // Formula: n = -(m/k) * ln(1 - X/m)
    // Where X = bits set, m = total bits, k = num hashes

    size_t x = bits_set();
    if (x == 0)
        return 0;

    double m = static_cast<double>(m_size_bits);
    double k = static_cast<double>(m_num_hashes);
    double x_double = static_cast<double>(x);

    double ratio = x_double / m;
    if (ratio >= 1.0)
        return m_size_bits; // Filter is saturated

    double n = -(m / k) * AK::log(1.0 - ratio);
    return static_cast<size_t>(n);
}

Vector<u64> BloomFilter::hash_item(ReadonlyBytes data) const
{
    Vector<u64> hashes;
    hashes.ensure_capacity(m_num_hashes);

    // Use SHA256 to generate two base hashes
    auto sha256_1 = Crypto::Hash::SHA256::create();
    sha256_1->update(data);
    auto digest1 = sha256_1->digest();
    auto hash1_bytes = digest1.bytes();

    // Add salt for second hash
    auto sha256_2 = Crypto::Hash::SHA256::create();
    sha256_2->update("salt"sv.bytes());
    sha256_2->update(data);
    auto digest2 = sha256_2->digest();
    auto hash2_bytes = digest2.bytes();

    // Convert first 8 bytes of each hash to u64
    u64 hash1 = 0;
    u64 hash2 = 0;

    for (size_t i = 0; i < 8; ++i) {
        hash1 = (hash1 << 8) | hash1_bytes[i];
        hash2 = (hash2 << 8) | hash2_bytes[i];
    }

    // Generate k hashes using double hashing technique
    // hash_i = hash1 + i * hash2
    for (size_t i = 0; i < m_num_hashes; ++i) {
        u64 hash = hash1 + i * hash2;
        hashes.append(hash);
    }

    return hashes;
}

bool BloomFilter::get_bit(size_t position) const
{
    size_t byte_idx = position / 8;
    size_t bit_idx = position % 8;

    if (byte_idx >= m_bits.size())
        return false;

    return (m_bits[byte_idx] & (1 << bit_idx)) != 0;
}

void BloomFilter::set_bit(size_t position)
{
    size_t byte_idx = position / 8;
    size_t bit_idx = position % 8;

    if (byte_idx >= m_bits.size())
        return;

    m_bits[byte_idx] |= (1 << bit_idx);
}

ErrorOr<ByteBuffer> BloomFilter::serialize() const
{
    // Format: [size_bits:8][num_hashes:4][bits:variable]
    size_t header_size = sizeof(u64) + sizeof(u32);
    size_t total_size = header_size + m_bits.size();

    auto buffer = TRY(ByteBuffer::create_uninitialized(total_size));

    // Write header
    *reinterpret_cast<u64*>(buffer.data()) = m_size_bits;
    *reinterpret_cast<u32*>(buffer.data() + sizeof(u64)) = m_num_hashes;

    // Write bits
    memcpy(buffer.data() + header_size, m_bits.data(), m_bits.size());

    return buffer;
}

ErrorOr<NonnullOwnPtr<BloomFilter>> BloomFilter::deserialize(ReadonlyBytes data)
{
    size_t header_size = sizeof(u64) + sizeof(u32);
    if (data.size() < header_size)
        return Error::from_string_literal("BloomFilter: Invalid serialized data");

    // Read header
    u64 size_bits = *reinterpret_cast<u64 const*>(data.data());
    u32 num_hashes = *reinterpret_cast<u32 const*>(data.data() + sizeof(u64));

    // Create filter
    auto filter = TRY(create(size_bits, num_hashes));

    // Read bits
    size_t expected_bytes = (size_bits + 7) / 8;
    if (data.size() != header_size + expected_bytes)
        return Error::from_string_literal("BloomFilter: Size mismatch in serialized data");

    memcpy(filter->m_bits.data(), data.data() + header_size, expected_bytes);

    return filter;
}

ErrorOr<void> BloomFilter::merge(BloomFilter const& other)
{
    if (m_size_bits != other.m_size_bits || m_num_hashes != other.m_num_hashes)
        return Error::from_string_literal("BloomFilter: Cannot merge filters with different parameters");

    // Bitwise OR of the two filters
    for (size_t i = 0; i < m_bits.size(); ++i) {
        m_bits[i] |= other.m_bits[i];
    }

    return {};
}

}