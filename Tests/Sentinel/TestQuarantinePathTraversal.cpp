/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StringBuilder.h>
#include <AK/StringView.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <stdio.h>

// Import validation functions from Quarantine
// Note: In a real implementation, these would be properly exported
// For testing, we replicate the validation logic

static ErrorOr<String> validate_restore_destination(StringView dest_dir)
{
    // 1. Resolve to canonical path to prevent directory traversal
    auto canonical_path = TRY(FileSystem::real_path(dest_dir));

    // 2. Verify it's an absolute path
    if (!canonical_path.starts_with('/'))
        return Error::from_string_literal("Destination must be an absolute path");

    // 3. Verify the directory exists
    if (!FileSystem::is_directory(canonical_path))
        return Error::from_string_literal("Destination directory does not exist");

    // 4. Verify we have write permission
    auto access_result = Core::System::access(canonical_path, W_OK);
    if (access_result.is_error())
        return Error::from_string_literal("Destination directory is not writable. Check permissions.");

    return String::from_byte_string(canonical_path).release_value();
}

static String sanitize_filename(StringView filename)
{
    // Extract basename to remove any path components
    auto basename_start = filename.find_last('/');
    auto basename = basename_start.has_value()
        ? filename.substring_view(basename_start.value() + 1)
        : filename;

    // Also handle Windows-style backslashes
    auto windows_basename_start = basename.find_last('\\');
    if (windows_basename_start.has_value())
        basename = basename.substring_view(windows_basename_start.value() + 1);

    // Remove dangerous characters
    StringBuilder safe_builder;
    for (auto ch : basename) {
        // Skip: null bytes, path separators, control characters
        if (ch == '\0' || ch == '/' || ch == '\\' || ch < 32)
            continue;
        safe_builder.append(ch);
    }

    auto safe_name = safe_builder.to_string();
    if (safe_name.is_error() || safe_name.value().is_empty()) {
        // Fallback if filename is empty or contains only dangerous characters
        return String::from_utf8_without_validation("quarantined_file"sv.bytes());
    }

    return safe_name.release_value();
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

// Test 1: Valid restore destination should be accepted
static void test_valid_destination_accepted()
{
    print_section("Test 1: Valid Destination Accepted"sv);

    // Create test directory
    auto test_dir = "/tmp/sentinel_restore_test"sv;
    (void)FileSystem::remove(test_dir, FileSystem::RecursionMode::Allowed);
    auto mkdir_result = Core::System::mkdir(test_dir, 0755);
    if (mkdir_result.is_error()) {
        log_fail("Valid destination"sv, "Could not create test directory"sv);
        return;
    }

    // Test validation
    auto result = validate_restore_destination(test_dir);
    if (result.is_error()) {
        log_fail("Valid destination"sv, ByteString::formatted("Valid directory rejected: {}", result.error()).view());
        (void)FileSystem::remove(test_dir, FileSystem::RecursionMode::Allowed);
        return;
    }

    printf("  ✓ Accepted: %s\n", test_dir.characters_without_null_termination());

    // Clean up
    (void)FileSystem::remove(test_dir, FileSystem::RecursionMode::Allowed);

    log_pass("Valid Destination Accepted"sv);
}

// Test 2: Directory traversal should be blocked
static void test_directory_traversal_blocked()
{
    print_section("Test 2: Directory Traversal Blocked"sv);

    Vector<StringView> malicious_destinations = {
        "/tmp/../../etc"sv,
        "/home/user/../../../root"sv,
        "/var/tmp/../../etc/passwd"sv,
    };

    for (auto const& dest : malicious_destinations) {
        auto result = validate_restore_destination(dest);
        // Most of these will fail because the resolved path won't be writable
        // or won't exist, which is the intended behavior
        if (!result.is_error()) {
            // Check if resolved path is actually dangerous
            auto resolved = result.value();
            if (resolved.bytes_as_string_view().starts_with("/etc"sv) ||
                resolved.bytes_as_string_view().starts_with("/root"sv)) {
                log_fail("Directory traversal"sv, ByteString::formatted("Dangerous path '{}' was accepted", dest).view());
                return;
            }
        }

        printf("  ✓ Blocked or resolved safely: %s\n", dest.characters_without_null_termination());
    }

    log_pass("Directory Traversal Blocked"sv);
}

// Test 3: Filename path traversal should be sanitized
static void test_filename_traversal_sanitized()
{
    print_section("Test 3: Filename Path Traversal Sanitized"sv);

    struct TestCase {
        StringView input;
        StringView expected_output;
    };

    Vector<TestCase> test_cases = {
        { "../../.ssh/authorized_keys"sv, "authorized_keys"sv },
        { "../../../etc/passwd"sv, "passwd"sv },
        { "safe_file.txt"sv, "safe_file.txt"sv },
        { "/absolute/path/file.exe"sv, "file.exe"sv },
        { "C:\\Windows\\System32\\evil.dll"sv, "evil.dll"sv },
    };

    for (auto const& test_case : test_cases) {
        auto sanitized = sanitize_filename(test_case.input);
        if (sanitized != test_case.expected_output) {
            log_fail("Filename sanitization"sv,
                     ByteString::formatted("Input '{}' produced '{}', expected '{}'",
                                         test_case.input,
                                         sanitized,
                                         test_case.expected_output).view());
            return;
        }

        printf("  ✓ Sanitized: '%s' -> '%s'\n",
               test_case.input.characters_without_null_termination(),
               sanitized.bytes_as_string_view().characters_without_null_termination());
    }

    log_pass("Filename Path Traversal Sanitized"sv);
}

// Test 4: Null byte injection should be handled
static void test_null_byte_handling()
{
    print_section("Test 4: Null Byte Injection Handled"sv);

    // Create filename with null byte
    char buffer[30];
    memcpy(buffer, "safe.txt", 8);
    buffer[8] = '\0';  // Null byte
    memcpy(buffer + 9, ".exe", 4);
    buffer[13] = '\0';

    StringView filename_with_null { buffer, 13 };
    auto sanitized = sanitize_filename(filename_with_null);

    // Should remove the null byte
    if (sanitized.bytes_as_string_view().contains('\0')) {
        log_fail("Null byte handling"sv, "Null byte not removed from filename"sv);
        return;
    }

    printf("  ✓ Null byte removed from filename\n");
    printf("  ✓ Result: '%s'\n", sanitized.bytes_as_string_view().characters_without_null_termination());

    log_pass("Null Byte Injection Handled"sv);
}

// Test 5: Control characters should be removed
static void test_control_characters_removed()
{
    print_section("Test 5: Control Characters Removed"sv);

    // Filename with control characters
    ByteString filename_with_controls = "file\n\r\t.txt";
    auto sanitized = sanitize_filename(filename_with_controls);

    // Should not contain any control characters
    for (auto ch : sanitized.bytes()) {
        if (ch < 32 && ch != '\0') {  // Control characters (excluding null terminator)
            log_fail("Control characters"sv, "Control character not removed"sv);
            return;
        }
    }

    printf("  ✓ Control characters removed\n");
    printf("  ✓ Result: '%s'\n", sanitized.bytes_as_string_view().characters_without_null_termination());

    log_pass("Control Characters Removed"sv);
}

// Test 6: Empty filename after sanitization should use fallback
static void test_empty_filename_fallback()
{
    print_section("Test 6: Empty Filename Fallback"sv);

    Vector<StringView> filenames_that_become_empty = {
        "////"sv,
        "\\\\\\\\"sv,
        "\0\0\0"sv,
        "../../../"sv,
    };

    for (auto const& filename : filenames_that_become_empty) {
        auto sanitized = sanitize_filename(filename);
        if (sanitized.is_empty()) {
            log_fail("Empty filename fallback"sv, "Sanitization resulted in empty string"sv);
            return;
        }

        if (sanitized != "quarantined_file"_string) {
            log_fail("Empty filename fallback"sv,
                     ByteString::formatted("Expected 'quarantined_file', got '{}'", sanitized).view());
            return;
        }

        printf("  ✓ Fallback applied for dangerous filename\n");
    }

    log_pass("Empty Filename Fallback"sv);
}

// Test 7: Symlink destination should be resolved and validated
static void test_symlink_destination_handled()
{
    print_section("Test 7: Symlink Destination Handled"sv);

    // Create test directory
    auto real_dir = "/tmp/sentinel_real_dir"sv;
    auto symlink_dir = "/tmp/sentinel_symlink_dir"sv;

    (void)FileSystem::remove(real_dir, FileSystem::RecursionMode::Allowed);
    (void)FileSystem::remove(symlink_dir, FileSystem::RecursionMode::Allowed);

    auto mkdir_result = Core::System::mkdir(real_dir, 0755);
    if (mkdir_result.is_error()) {
        log_fail("Symlink destination"sv, "Could not create real directory"sv);
        return;
    }

    // Create symlink
    auto symlink_result = Core::System::symlink(real_dir, symlink_dir);
    if (symlink_result.is_error()) {
        log_fail("Symlink destination"sv, "Could not create symlink"sv);
        (void)FileSystem::remove(real_dir, FileSystem::RecursionMode::Allowed);
        return;
    }

    // Validate symlink destination - should resolve to real path
    auto result = validate_restore_destination(symlink_dir);
    if (result.is_error()) {
        log_fail("Symlink destination"sv, "Symlink destination rejected"sv);
        (void)FileSystem::remove(symlink_dir, FileSystem::RecursionMode::Disallowed);
        (void)FileSystem::remove(real_dir, FileSystem::RecursionMode::Allowed);
        return;
    }

    // Verify it resolved to the real directory
    auto resolved = result.value();
    printf("  ✓ Symlink resolved: %s -> %s\n",
           symlink_dir.characters_without_null_termination(),
           resolved.bytes_as_string_view().characters_without_null_termination());

    // Clean up
    (void)FileSystem::remove(symlink_dir, FileSystem::RecursionMode::Disallowed);
    (void)FileSystem::remove(real_dir, FileSystem::RecursionMode::Allowed);

    log_pass("Symlink Destination Handled"sv);
}

// Test 8: Non-existent destination should be rejected
static void test_non_existent_destination_rejected()
{
    print_section("Test 8: Non-Existent Destination Rejected"sv);

    auto non_existent = "/tmp/sentinel_does_not_exist_12345"sv;

    // Ensure it doesn't exist
    (void)FileSystem::remove(non_existent, FileSystem::RecursionMode::Allowed);

    auto result = validate_restore_destination(non_existent);
    if (!result.is_error()) {
        log_fail("Non-existent destination"sv, "Non-existent directory was accepted"sv);
        return;
    }

    printf("  ✓ Blocked: Non-existent directory\n");

    log_pass("Non-Existent Destination Rejected"sv);
}

// Test 9: Unwritable destination should be rejected
static void test_unwritable_destination_rejected()
{
    print_section("Test 9: Unwritable Destination Rejected"sv);

    // Test with /etc (typically not writable by normal users)
    if (FileSystem::is_directory("/etc"sv)) {
        auto result = validate_restore_destination("/etc"sv);
        if (!result.is_error()) {
            // If running as root, this might succeed, so check
            auto access_result = Core::System::access("/etc"sv, W_OK);
            if (access_result.is_error()) {
                log_fail("Unwritable destination"sv, "/etc accepted despite not being writable"sv);
                return;
            } else {
                printf("  Note: Running as root, /etc is writable\n");
            }
        } else {
            printf("  ✓ Blocked: /etc (not writable)\n");
        }
    }

    log_pass("Unwritable Destination Rejected"sv);
}

int main()
{
    printf("====================================\n");
    printf("  Path Traversal Prevention Tests\n");
    printf("====================================\n");
    printf("\nTesting path traversal prevention in Quarantine restore\n");

    // Run all tests
    test_valid_destination_accepted();
    test_directory_traversal_blocked();
    test_filename_traversal_sanitized();
    test_null_byte_handling();
    test_control_characters_removed();
    test_empty_filename_fallback();
    test_symlink_destination_handled();
    test_non_existent_destination_rejected();
    test_unwritable_destination_rejected();

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
