/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/String.h>
#include <LibCrypto/ConstantTimeComparison.h>
#include <LibTest/TestCase.h>
#include <chrono>

// Helper to measure execution time in nanoseconds
static u64 measure_time_ns(auto&& func)
{
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

TEST_CASE(equal_strings_same_length)
{
    StringView a = "hello_world_test_string_42"sv;
    StringView b = "hello_world_test_string_42"sv;

    EXPECT(Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(different_strings_same_length)
{
    StringView a = "hello_world_test_string_42"sv;
    StringView b = "hello_world_test_string_43"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(different_lengths)
{
    StringView a = "hello_world"sv;
    StringView b = "hello"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(empty_strings)
{
    StringView a = ""sv;
    StringView b = ""sv;

    EXPECT(Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(one_empty_one_not)
{
    StringView a = ""sv;
    StringView b = "hello"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(very_long_strings_equal)
{
    // Create 2KB strings
    StringBuilder builder_a;
    StringBuilder builder_b;
    for (int i = 0; i < 2048; i++) {
        builder_a.append('x');
        builder_b.append('x');
    }
    auto string_a = MUST(builder_a.to_string());
    auto string_b = MUST(builder_b.to_string());

    EXPECT(Crypto::ConstantTimeComparison::compare_strings(string_a, string_b));
}

TEST_CASE(very_long_strings_different_at_end)
{
    // Create 2KB strings that differ only at the very end
    StringBuilder builder_a;
    StringBuilder builder_b;
    for (int i = 0; i < 2047; i++) {
        builder_a.append('x');
        builder_b.append('x');
    }
    builder_a.append('a');
    builder_b.append('b');
    auto string_a = MUST(builder_a.to_string());
    auto string_b = MUST(builder_b.to_string());

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(string_a, string_b));
}

TEST_CASE(strings_differing_at_start)
{
    StringView a = "a_test_string_with_lots_of_content_following"sv;
    StringView b = "b_test_string_with_lots_of_content_following"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(strings_differing_at_middle)
{
    StringView a = "test_string_a_with_content"sv;
    StringView b = "test_string_b_with_content"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(strings_differing_at_end)
{
    StringView a = "test_string_with_content_a"sv;
    StringView b = "test_string_with_content_b"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, b));
}

TEST_CASE(all_zeros_vs_all_ones)
{
    // Create strings with only 0s and only 1s
    StringBuilder builder_zeros;
    StringBuilder builder_ones;
    for (int i = 0; i < 64; i++) {
        builder_zeros.append('0');
        builder_ones.append('1');
    }
    auto zeros = MUST(builder_zeros.to_string());
    auto ones = MUST(builder_ones.to_string());

    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(zeros, ones));
}

TEST_CASE(hash_comparison_64_hex_chars)
{
    // Typical SHA256 hash (64 hex characters)
    StringView hash_a = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7a"sv;
    StringView hash_b = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7a"sv;

    EXPECT(Crypto::ConstantTimeComparison::compare_hashes(hash_a, hash_b));
}

TEST_CASE(hash_comparison_different_hashes)
{
    StringView hash_a = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7a"sv;
    StringView hash_b = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7b"sv;

    EXPECT(!Crypto::ConstantTimeComparison::compare_hashes(hash_a, hash_b));
}

TEST_CASE(timing_consistency_verification)
{
    // This test verifies that comparison time doesn't depend on where strings differ
    StringView correct = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7a"sv;
    StringView wrong_at_start = "0000000000000000000000000000000000000000000000000000000000000000"sv;
    StringView wrong_at_end = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f70"sv;

    // Run each comparison many times and measure average time
    constexpr int iterations = 1000;

    u64 total_time_start = 0;
    u64 total_time_end = 0;

    for (int i = 0; i < iterations; i++) {
        total_time_start += measure_time_ns([&]() {
            [[maybe_unused]] volatile bool result = Crypto::ConstantTimeComparison::compare_hashes(correct, wrong_at_start);
        });

        total_time_end += measure_time_ns([&]() {
            [[maybe_unused]] volatile bool result = Crypto::ConstantTimeComparison::compare_hashes(correct, wrong_at_end);
        });
    }

    u64 avg_time_start = total_time_start / iterations;
    u64 avg_time_end = total_time_end / iterations;

    // Calculate percentage difference
    u64 max_time = (avg_time_start > avg_time_end) ? avg_time_start : avg_time_end;
    u64 min_time = (avg_time_start < avg_time_end) ? avg_time_start : avg_time_end;
    double percent_diff = ((double)(max_time - min_time) / (double)min_time) * 100.0;

    // Times should be within 20% of each other
    // (We allow some variance due to CPU scheduling, cache effects, etc.)
    EXPECT(percent_diff < 20.0);

    // Also verify that comparison completes in reasonable time (< 1 microsecond)
    EXPECT(avg_time_start < 1000);
    EXPECT(avg_time_end < 1000);
}

TEST_CASE(byte_array_comparison_equal)
{
    u8 data_a[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    u8 data_b[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    ReadonlyBytes bytes_a { data_a, sizeof(data_a) };
    ReadonlyBytes bytes_b { data_b, sizeof(data_b) };

    EXPECT(Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_b));
}

TEST_CASE(byte_array_comparison_different)
{
    u8 data_a[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    u8 data_b[] = { 0x01, 0x02, 0x03, 0x04, 0x06 };

    ReadonlyBytes bytes_a { data_a, sizeof(data_a) };
    ReadonlyBytes bytes_b { data_b, sizeof(data_b) };

    EXPECT(!Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_b));
}

TEST_CASE(byte_array_different_lengths)
{
    u8 data_a[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    u8 data_b[] = { 0x01, 0x02, 0x03 };

    ReadonlyBytes bytes_a { data_a, sizeof(data_a) };
    ReadonlyBytes bytes_b { data_b, sizeof(data_b) };

    EXPECT(!Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_b));
}

TEST_CASE(real_world_sha256_hashes)
{
    // Real SHA256 hashes from actual files
    StringView hash1 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv; // Empty file
    StringView hash2 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"sv; // "abc"
    StringView hash3 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"sv; // Empty file (duplicate)

    EXPECT(Crypto::ConstantTimeComparison::compare_hashes(hash1, hash3));
    EXPECT(!Crypto::ConstantTimeComparison::compare_hashes(hash1, hash2));
}

TEST_CASE(performance_benchmark)
{
    // Benchmark to ensure constant-time comparison is acceptably fast
    StringView hash = "3a4f9c2b8d1e6f0a9c3b7d2e8f1a4c6b9d2e7f3a8c1b4d7e0f9a2c5b8d1e4f7a"sv;

    constexpr int iterations = 10000;
    auto total_time = measure_time_ns([&]() {
        for (int i = 0; i < iterations; i++) {
            [[maybe_unused]] volatile bool result = Crypto::ConstantTimeComparison::compare_hashes(hash, hash);
        }
    });

    u64 avg_time_ns = total_time / iterations;

    // Should complete in less than 500ns per comparison for 64-char hash
    EXPECT(avg_time_ns < 500);

    // Print performance info (visible with --verbose)
    warnln("Average comparison time: {} ns", avg_time_ns);
}

TEST_CASE(null_bytes_in_strings)
{
    // Test strings containing null bytes (from StringView perspective, not C strings)
    u8 data_a[] = { 'h', 'e', 'l', 'l', 'o', '\0', 'w', 'o', 'r', 'l', 'd' };
    u8 data_b[] = { 'h', 'e', 'l', 'l', 'o', '\0', 'w', 'o', 'r', 'l', 'd' };
    u8 data_c[] = { 'h', 'e', 'l', 'l', 'o', '\0', 'x', 'o', 'r', 'l', 'd' };

    ReadonlyBytes bytes_a { data_a, sizeof(data_a) };
    ReadonlyBytes bytes_b { data_b, sizeof(data_b) };
    ReadonlyBytes bytes_c { data_c, sizeof(data_c) };

    EXPECT(Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_b));
    EXPECT(!Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_c));
}

TEST_CASE(single_bit_difference)
{
    // Strings that differ by only a single bit
    u8 data_a[] = { 0b00000000 };
    u8 data_b[] = { 0b00000001 };

    ReadonlyBytes bytes_a { data_a, sizeof(data_a) };
    ReadonlyBytes bytes_b { data_b, sizeof(data_b) };

    EXPECT(!Crypto::ConstantTimeComparison::compare_bytes(bytes_a, bytes_b));
}

TEST_CASE(unicode_strings)
{
    // Test with UTF-8 encoded strings
    StringView a = "Hello, 世界! 🌍"sv;
    StringView b = "Hello, 世界! 🌍"sv;
    StringView c = "Hello, 世界! 🌎"sv;

    EXPECT(Crypto::ConstantTimeComparison::compare_strings(a, b));
    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, c));
}

TEST_CASE(special_characters)
{
    StringView a = "!@#$%^&*()_+-=[]{}|;':\",./<>?`~";
    StringView b = "!@#$%^&*()_+-=[]{}|;':\",./<>?`~";
    StringView c = "!@#$%^&*()_+-=[]{}|;':\",./<>?`~!";

    EXPECT(Crypto::ConstantTimeComparison::compare_strings(a, b));
    EXPECT(!Crypto::ConstantTimeComparison::compare_strings(a, c));
}
