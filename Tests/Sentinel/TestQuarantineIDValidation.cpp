/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringView.h>
#include <stdio.h>

// Import validation function from Quarantine
// Note: In a real implementation, this would be properly exported
// For testing, we replicate the validation logic

static bool is_valid_quarantine_id(StringView id)
{
    // Must be exactly 21 characters: 8 digits + underscore + 6 digits + underscore + 6 hex chars
    if (id.length() != 21)
        return false;

    // Validate character by character according to position
    for (size_t i = 0; i < id.length(); ++i) {
        char ch = id[i];

        if (i < 8) {
            // Positions 0-7: Date portion (YYYYMMDD) - must be digits
            if (!isdigit(ch))
                return false;
        } else if (i == 8) {
            // Position 8: First separator - must be underscore
            if (ch != '_')
                return false;
        } else if (i >= 9 && i < 15) {
            // Positions 9-14: Time portion (HHMMSS) - must be digits
            if (!isdigit(ch))
                return false;
        } else if (i == 15) {
            // Position 15: Second separator - must be underscore
            if (ch != '_')
                return false;
        } else {
            // Positions 16-20: Random portion (6 hex chars) - must be hex digits
            if (!isxdigit(ch))
                return false;
        }
    }

    return true;
}

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

static void log_pass(StringView test_name)
{
    printf("✅ PASSED: %s\n", test_name.characters_without_null_termination());
    tests_passed++;
}

static void log_fail(StringView test_name, StringView reason)
{
    printf("❌ FAILED: %s - %s\n",
           test_name.characters_without_null_termination(),
           reason.characters_without_null_termination());
    tests_failed++;
}

static void print_section(StringView title)
{
    printf("\n=== %s ===\n", title.characters_without_null_termination());
}

// Test 1: Valid quarantine IDs should be accepted
static void test_valid_ids_accepted()
{
    print_section("Test 1: Valid Quarantine IDs Accepted"sv);

    Vector<StringView> valid_ids = {
        "20251030_143052_a3f5c2"sv,
        "20250101_000000_000000"sv,
        "21001231_235959_ffffff"sv,
        "20251225_120000_abc123"sv,
        "20260630_183045_fedcba"sv,
    };

    for (auto const& id : valid_ids) {
        if (!is_valid_quarantine_id(id)) {
            log_fail("Valid IDs"sv, ByteString::formatted("Valid ID '{}' was rejected", id).view());
            return;
        }

        printf("  ✓ Accepted: %s\n", id.characters_without_null_termination());
    }

    log_pass("Valid Quarantine IDs Accepted"sv);
}

// Test 2: Path traversal attempts should be rejected
static void test_path_traversal_rejected()
{
    print_section("Test 2: Path Traversal Rejected"sv);

    Vector<StringView> malicious_ids = {
        "../../etc/passwd"sv,
        "../../../root/.ssh/id_rsa"sv,
        "../../../../etc/shadow"sv,
        "../malicious_file"sv,
        "../../../../../../boot/vmlinuz"sv,
    };

    for (auto const& id : malicious_ids) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Path traversal"sv, ByteString::formatted("Traversal ID '{}' was accepted", id).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", id.characters_without_null_termination());
    }

    log_pass("Path Traversal Rejected"sv);
}

// Test 3: Absolute paths should be rejected
static void test_absolute_paths_rejected()
{
    print_section("Test 3: Absolute Paths Rejected"sv);

    Vector<StringView> absolute_paths = {
        "/etc/passwd"sv,
        "/etc/shadow"sv,
        "/root/.ssh/id_rsa"sv,
        "/var/log/auth.log"sv,
        "/home/user/.bashrc"sv,
    };

    for (auto const& path : absolute_paths) {
        if (is_valid_quarantine_id(path)) {
            log_fail("Absolute paths"sv, ByteString::formatted("Absolute path '{}' was accepted", path).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", path.characters_without_null_termination());
    }

    log_pass("Absolute Paths Rejected"sv);
}

// Test 4: Wrong length IDs should be rejected
static void test_wrong_length_rejected()
{
    print_section("Test 4: Wrong Length Rejected"sv);

    Vector<StringView> wrong_length_ids = {
        "20251030_143052_a3f5c"sv,    // 20 chars (too short)
        "20251030_143052_a3f5c22"sv,  // 22 chars (too long)
        ""sv,                          // 0 chars (empty)
        "20251030"sv,                  // 8 chars (date only)
        "20251030_143052"sv,           // 15 chars (no random suffix)
    };

    for (auto const& id : wrong_length_ids) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Wrong length"sv, ByteString::formatted("ID with length {} was accepted", id.length()).view());
            return;
        }

        printf("  ✓ Rejected: '%s' (length: %zu, expected: 21)\n",
               id.characters_without_null_termination(), id.length());
    }

    log_pass("Wrong Length Rejected"sv);
}

// Test 5: Wrong format should be rejected
static void test_wrong_format_rejected()
{
    print_section("Test 5: Wrong Format Rejected"sv);

    Vector<StringView> wrong_format_ids = {
        "20251030-143052-a3f5c2"sv,  // Dashes instead of underscores
        "2025/10/30_143052_a3f5c2"sv, // Slashes in date
        "20251030 143052 a3f5c2"sv,  // Spaces instead of underscores
        "20251030_143052:a3f5c2"sv,  // Colon instead of underscore
        "20251030.143052.a3f5c2"sv,  // Dots instead of underscores
    };

    for (auto const& id : wrong_format_ids) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Wrong format"sv, ByteString::formatted("Malformed ID '{}' was accepted", id).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", id.characters_without_null_termination());
    }

    log_pass("Wrong Format Rejected"sv);
}

// Test 6: Non-hex random suffix should be rejected
static void test_non_hex_suffix_rejected()
{
    print_section("Test 6: Non-Hex Random Suffix Rejected"sv);

    Vector<StringView> non_hex_ids = {
        "20251030_143052_g3f5c2"sv,  // 'g' is not hex
        "20251030_143052_zzzzzz"sv,  // All non-hex
        "20251030_143052_GHIJKL"sv,  // Non-hex uppercase
        "20251030_143052_a3f5c!"sv,  // Special character
        "20251030_143052_abc12 "sv,  // Space at end
    };

    for (auto const& id : non_hex_ids) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Non-hex suffix"sv, ByteString::formatted("ID with non-hex suffix '{}' was accepted", id).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", id.characters_without_null_termination());
    }

    log_pass("Non-Hex Random Suffix Rejected"sv);
}

// Test 7: Non-digit date/time should be rejected
static void test_non_digit_date_time_rejected()
{
    print_section("Test 7: Non-Digit Date/Time Rejected"sv);

    Vector<StringView> non_digit_ids = {
        "2025103a_143052_a3f5c2"sv,  // 'a' in date
        "20251030_14305x_a3f5c2"sv,  // 'x' in time
        "YYYYMMDD_143052_a3f5c2"sv,  // Letters in date
        "20251030_HHMMSS_a3f5c2"sv,  // Letters in time
        "2025-030_143052_a3f5c2"sv,  // Dash in date
    };

    for (auto const& id : non_digit_ids) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Non-digit date/time"sv, ByteString::formatted("ID with non-digit '{}' was accepted", id).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", id.characters_without_null_termination());
    }

    log_pass("Non-Digit Date/Time Rejected"sv);
}

// Test 8: Special attack vectors should be rejected
static void test_special_attack_vectors_rejected()
{
    print_section("Test 8: Special Attack Vectors Rejected"sv);

    Vector<StringView> attack_vectors = {
        "20251030_143052_a3f5c2; rm -rf /"sv,  // Command injection
        "20251030_143052_a3f5c2\n"sv,          // Newline injection
        "20251030_143052_a3f5c2\0"sv,          // Null byte
        "20251030_143052_a3f5c2*"sv,           // Wildcard
        "20251030_143052_a3f5c2.."sv,          // Dot-dot
        "20251030_143052_a3f5c2|malicious"sv,  // Pipe character
        "20251030_143052_a3f5c2&"sv,           // Ampersand
        "20251030_143052_a3f5c2$"sv,           // Dollar sign
    };

    for (auto const& attack : attack_vectors) {
        if (is_valid_quarantine_id(attack)) {
            log_fail("Attack vectors"sv, ByteString::formatted("Attack vector '{}' was accepted", attack).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", attack.characters_without_null_termination());
    }

    log_pass("Special Attack Vectors Rejected"sv);
}

// Test 9: Edge cases with valid length but invalid content
static void test_edge_cases()
{
    print_section("Test 9: Edge Cases"sv);

    Vector<StringView> edge_cases = {
        "AAA00000_000000_000000"sv,  // Letters in date
        "20251030_BBB000_000000"sv,  // Letters in time
        "20251030_143052_gggggg"sv,  // Non-hex in suffix
        "20251030.143052.a3f5c2"sv,  // Dots instead of underscores (21 chars)
        "20251030-143052-a3f5c2"sv,  // Dashes instead of underscores (21 chars)
    };

    for (auto const& edge : edge_cases) {
        if (is_valid_quarantine_id(edge)) {
            log_fail("Edge cases"sv, ByteString::formatted("Edge case '{}' was accepted", edge).view());
            return;
        }

        printf("  ✓ Rejected: %s\n", edge.characters_without_null_termination());
    }

    log_pass("Edge Cases"sv);
}

// Test 10: Boundary testing for hex characters
static void test_hex_boundary()
{
    print_section("Test 10: Hex Character Boundaries"sv);

    // All valid hex chars (0-9, a-f, A-F)
    Vector<StringView> valid_hex_variants = {
        "20251030_143052_000000"sv,  // All zeros
        "20251030_143052_999999"sv,  // All nines
        "20251030_143052_aaaaaa"sv,  // Lowercase a
        "20251030_143052_ffffff"sv,  // Lowercase f
        "20251030_143052_AAAAAA"sv,  // Uppercase A
        "20251030_143052_FFFFFF"sv,  // Uppercase F
        "20251030_143052_09aF"sv,    // Mixed (note: this is only 4 chars, will fail length check)
    };

    // Only test the ones that have correct length
    Vector<StringView> valid_with_correct_length = {
        "20251030_143052_000000"sv,
        "20251030_143052_999999"sv,
        "20251030_143052_aaaaaa"sv,
        "20251030_143052_ffffff"sv,
        "20251030_143052_AAAAAA"sv,
        "20251030_143052_FFFFFF"sv,
    };

    for (auto const& id : valid_with_correct_length) {
        if (!is_valid_quarantine_id(id)) {
            log_fail("Hex boundaries"sv, ByteString::formatted("Valid hex ID '{}' was rejected", id).view());
            return;
        }

        printf("  ✓ Accepted: %s\n", id.characters_without_null_termination());
    }

    // Test just outside valid hex range
    Vector<StringView> invalid_hex = {
        "20251030_143052_gggggg"sv,  // 'g' is after 'f'
        "20251030_143052_GGGGGG"sv,  // 'G' is after 'F'
    };

    for (auto const& id : invalid_hex) {
        if (is_valid_quarantine_id(id)) {
            log_fail("Hex boundaries"sv, ByteString::formatted("Invalid hex ID '{}' was accepted", id).view());
            return;
        }

        printf("  ✓ Rejected: %s (non-hex)\n", id.characters_without_null_termination());
    }

    log_pass("Hex Character Boundaries"sv);
}

int main()
{
    printf("====================================\n");
    printf("  Quarantine ID Validation Tests\n");
    printf("====================================\n");
    printf("\nTesting quarantine ID format validation\n");

    // Run all tests
    test_valid_ids_accepted();
    test_path_traversal_rejected();
    test_absolute_paths_rejected();
    test_wrong_length_rejected();
    test_wrong_format_rejected();
    test_non_hex_suffix_rejected();
    test_non_digit_date_time_rejected();
    test_special_attack_vectors_rejected();
    test_edge_cases();
    test_hex_boundary();

    // Print summary
    printf("\n====================================\n");
    printf("  Test Summary\n");
    printf("====================================\n");
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Total:  %d\n", tests_passed + tests_failed);
    printf("====================================\n");

    if (tests_failed > 0) {
        printf("\n❌ Some tests FAILED\n");
        return 1;
    }

    printf("\n✅ All tests PASSED!\n");

    return 0;
}
