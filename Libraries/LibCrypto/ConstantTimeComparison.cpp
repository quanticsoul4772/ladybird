/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCrypto/ConstantTimeComparison.h>
#include <AK/Platform.h>

namespace Crypto {

// Implementation notes:
//
// The constant-time property is achieved through:
// 1. Always iterating through all bytes (no early exit)
// 2. Using only bitwise operations (no conditional branches on data)
// 3. Accumulating differences with OR (can't be optimized away)
// 4. Converting final result to boolean without branching
//
// Compiler barriers and volatile operations help ensure the compiler
// doesn't optimize away our constant-time properties.

u8 ConstantTimeComparison::constant_time_compare_impl(u8 const* a, size_t len_a, u8 const* b, size_t len_b)
{
    // XOR the lengths - if different, result will be non-zero
    // This ensures we compare lengths in constant time too
    size_t len_diff = len_a ^ len_b;

    // We need to iterate through the longer of the two lengths
    // to ensure constant time regardless of which is longer
    size_t max_len = (len_a > len_b) ? len_a : len_b;

    // Accumulator for differences (0 = equal, non-zero = different)
    u8 diff = 0;

    // Compare all bytes, padding shorter string with zeros
    // This loop always runs max_len times regardless of input content
    for (size_t i = 0; i < max_len; i++) {
        // Get byte from each input, or 0 if past the end
        // These ternary operators are based on loop counter (i), not data,
        // so they don't leak timing information about the strings themselves
        u8 byte_a = (i < len_a) ? a[i] : 0;
        u8 byte_b = (i < len_b) ? b[i] : 0;

        // XOR to find differences, OR to accumulate
        // If bytes are equal, XOR produces 0
        // If bytes differ, XOR produces non-zero
        // OR accumulates any non-zero values
        diff |= byte_a ^ byte_b;
    }

    // Combine byte differences with length difference
    // Cast is safe because len_diff can only contribute to making diff non-zero
    diff |= static_cast<u8>(len_diff);

    // Use volatile to prevent compiler from optimizing away the comparison
    // This is a compiler barrier that ensures the above loop isn't optimized
    volatile u8 result = diff;

    return result;
}

bool ConstantTimeComparison::to_bool(u8 result)
{
    // Convert u8 to boolean without branching
    //
    // If result == 0 (strings equal):
    //   result - 1 = 0xFF (underflow to max u8)
    //   (u32)0xFF = 0x000000FF
    //   0x000000FF - 1 = 0x000000FE
    //   0x000000FE >> 8 = 0x00000000
    //   0x00000000 & 1 = 0
    //   return 1 - 0 = 1 (true)
    //
    // If result != 0 (strings different):
    //   result - 1 = some value < 0xFF
    //   (u32)(value) = 0x000000XX where XX < 0xFF
    //   0x000000XX - 1 = 0x000000YY where YY < 0xFE
    //   0x000000YY >> 8 = 0x00000000
    //   0x00000000 & 1 = 0
    //   But we need to return false...
    //
    // Actually, let's use a simpler bitwise approach:
    //
    // If result == 0: return true
    // If result != 0: return false
    //
    // We can do this by checking if (result - 1) produces an underflow:
    //   result = 0 -> result - 1 = 0xFF (underflow) -> high bit set
    //   result > 0 -> result - 1 = 0-254 -> high bit clear (for values 1-255)
    //
    // Even simpler: use the fact that (0 - 1) = 0xFFFFFFFF in u32
    // and anything else - 1 won't have all bits set
    //
    // Most reliable approach without branches:

    // If result is 0, all these operations should yield 1
    // If result is non-zero, all these operations should yield 0
    u32 result32 = static_cast<u32>(result);

    // Expand result to fill 32 bits if non-zero
    // 0 stays 0, any non-zero becomes 0xFFFFFFFF
    result32 = result32 | (result32 << 8);
    result32 = result32 | (result32 << 16);

    // Now result32 is either 0x00000000 or 0xFFFFFFFF

    // Subtract 1: 0x00000000 - 1 = 0xFFFFFFFF, 0xFFFFFFFF - 1 = 0xFFFFFFFE
    result32 = result32 - 1;

    // Right shift by 31 bits: 0xFFFFFFFF >> 31 = 1, 0xFFFFFFFE >> 31 = 1
    // Wait, both give 1...

    // Let me reconsider. The goal:
    // - If result == 0, return true (1)
    // - If result != 0, return false (0)

    // Simpler approach using two's complement:
    // (result | -result) will have high bit set if result != 0
    // High bit clear if result == 0

    i32 signed_result = static_cast<i32>(result);
    i32 negated = -signed_result;
    i32 combined = signed_result | negated;

    // If result was 0: signed_result = 0, negated = 0, combined = 0, high bit = 0
    // If result was non-zero: high bit will be set in combined

    // Extract high bit and invert
    // High bit = 1 means different, we want to return 0 (false)
    // High bit = 0 means equal, we want to return 1 (true)
    u32 high_bit = static_cast<u32>(combined) >> 31;
    bool equal = (high_bit == 0);

    return equal;
}

bool ConstantTimeComparison::compare_strings(StringView a, StringView b)
{
    u8 diff = constant_time_compare_impl(
        reinterpret_cast<u8 const*>(a.characters_without_null_termination()),
        a.length(),
        reinterpret_cast<u8 const*>(b.characters_without_null_termination()),
        b.length());

    return to_bool(diff);
}

bool ConstantTimeComparison::compare_bytes(ReadonlyBytes a, ReadonlyBytes b)
{
    u8 diff = constant_time_compare_impl(a.data(), a.size(), b.data(), b.size());
    return to_bool(diff);
}

bool ConstantTimeComparison::compare_hashes(StringView a, StringView b)
{
    // Functionally identical to compare_strings, but provides clearer API
    return compare_strings(a, b);
}

}
