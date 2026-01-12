/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/StringView.h>
#include <AK/Types.h>

namespace Crypto {

// Constant-time comparison utilities to prevent timing attacks.
//
// These functions are designed to take the same amount of time regardless
// of where input strings differ, preventing attackers from using timing
// side-channels to extract sensitive information like hashes, tokens, or secrets.
//
// WARNING: Standard string comparison operators (==, !=, strcmp) are NOT
// constant-time and MUST NOT be used for security-sensitive comparisons.
//
// Example vulnerable code:
//     if (provided_hash == stored_hash) { ... }  // VULNERABLE!
//
// Example secure code:
//     if (ConstantTimeComparison::compare_hashes(provided_hash, stored_hash)) { ... }
//
class ConstantTimeComparison {
public:
    // Compare two strings in constant time.
    //
    // This function:
    // - Takes the same time regardless of where strings differ
    // - Takes the same time for different-length strings
    // - Uses bitwise operations to avoid branching
    // - Returns true if strings are equal, false otherwise
    //
    // Use this for: tokens, session IDs, API keys, any secret strings
    //
    // Performance: ~50-100ns for 64-character strings (2x slower than strcmp)
    // Security: Resistant to timing attacks, cache attacks on comparison itself
    //
    [[nodiscard]] static bool compare_strings(StringView a, StringView b);

    // Compare two byte arrays in constant time.
    //
    // Similar to compare_strings but operates on raw byte arrays.
    // Use this for: binary secrets, keys, nonces
    //
    [[nodiscard]] static bool compare_bytes(ReadonlyBytes a, ReadonlyBytes b);

    // Compare two hex-encoded hashes in constant time.
    //
    // Optimized for hex string comparisons (SHA256, etc).
    // Use this for: file hashes, password hashes, checksums
    //
    // This is functionally equivalent to compare_strings but provides
    // a clearer semantic API for hash comparisons.
    //
    [[nodiscard]] static bool compare_hashes(StringView a, StringView b);

private:
    // Convert comparison result to boolean without branching.
    //
    // Takes a byte that is 0 if inputs were equal, non-zero if different.
    // Returns true if byte is 0, false otherwise, using only bitwise operations.
    //
    // This ensures the compiler doesn't optimize our constant-time logic
    // into a branch instruction.
    //
    [[nodiscard]] static bool to_bool(u8 result);

    // Perform the core constant-time byte-by-byte comparison.
    //
    // This is the heart of the constant-time algorithm:
    // - Iterates through all bytes regardless of early mismatches
    // - Uses XOR to detect differences
    // - Uses OR to accumulate differences
    // - No conditional branches based on data
    //
    [[nodiscard]] static u8 constant_time_compare_impl(u8 const* a, size_t len_a, u8 const* b, size_t len_b);
};

}
