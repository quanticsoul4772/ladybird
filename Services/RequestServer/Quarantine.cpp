/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Quarantine.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/Random.h>
#include <AK/StringBuilder.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/RetryPolicy.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <Services/Sentinel/InputValidator.h>
#include <time.h>

namespace RequestServer {

// Retry policy for file I/O operations with exponential backoff
// Max attempts: 3, Initial delay: 200ms, Max delay: 5 seconds, Backoff: 2x
static Core::RetryPolicy s_file_retry_policy(
    3,                                          // max_attempts
    Duration::from_milliseconds(200),           // initial_delay
    Duration::from_seconds(5),                  // max_delay
    2.0,                                        // backoff_multiplier
    0.1                                         // jitter_factor
);

static void initialize_file_retry_policy()
{
    static bool initialized = false;
    if (!initialized) {
        s_file_retry_policy.set_retry_predicate(Core::RetryPolicy::file_io_retry_predicate());
        initialized = true;
    }
}

// Validate quarantine ID using centralized InputValidator
static bool is_valid_quarantine_id(StringView id)
{
    auto result = Sentinel::InputValidator::validate_quarantine_id(id);
    return result.is_valid;
}

ErrorOr<void> Quarantine::initialize()
{
    // Initialize retry policy for file operations
    initialize_file_retry_policy();

    // Get quarantine directory path
    auto quarantine_dir = TRY(get_quarantine_directory());
    auto quarantine_dir_byte_string = quarantine_dir.to_byte_string();

    // Create directory if it doesn't exist - with retry logic for transient failures
    auto create_result = s_file_retry_policy.execute([&]() -> ErrorOr<void> {
        return Core::Directory::create(quarantine_dir_byte_string, Core::Directory::CreateDirectories::Yes);
    });
    if (create_result.is_error()) {
        dbgln("Quarantine: Failed to create directory: {}", create_result.error());
        return Error::from_string_literal("Cannot create quarantine directory. Check disk space and permissions.");
    }

    // Set restrictive permissions on directory (owner only: rwx------) - with retry
    auto chmod_result = s_file_retry_policy.execute([&]() -> ErrorOr<void> {
        return Core::System::chmod(quarantine_dir, 0700);
    });
    if (chmod_result.is_error()) {
        dbgln("Quarantine: Failed to set permissions: {}", chmod_result.error());
        return Error::from_string_literal("Cannot set permissions on quarantine directory. Check file system permissions.");
    }

    // Clean up any orphaned files from previous failed quarantine operations
    auto cleanup_result = cleanup_orphaned_files();
    if (cleanup_result.is_error()) {
        dbgln("Quarantine: Warning - Failed to cleanup orphaned files: {}", cleanup_result.error());
        // Don't fail initialization if cleanup fails - log warning and continue
    }

    dbgln("Quarantine: Initialized directory at {}", quarantine_dir);
    return {};
}

ErrorOr<String> Quarantine::get_quarantine_directory()
{
    // Use ~/.local/share/Ladybird/Quarantine/
    auto user_data_dir = Core::StandardPaths::user_data_directory();
    StringBuilder path_builder;
    path_builder.append(user_data_dir);
    path_builder.append("/Ladybird/Quarantine"sv);
    return path_builder.to_string();
}

ErrorOr<String> Quarantine::generate_quarantine_id()
{
    // Generate ID: YYYYMMDD_HHMMSS_<6_random_hex_chars>
    auto now = UnixDateTime::now();
    time_t timestamp = now.seconds_since_epoch();
    struct tm tm_buf;
    struct tm* tm_info = gmtime_r(&timestamp, &tm_buf);

    if (!tm_info) {
        return Error::from_string_literal("Failed to convert timestamp");
    }

    // Format timestamp
    StringBuilder id_builder;
    id_builder.appendff("{:04}{:02}{:02}_{:02}{:02}{:02}_",
        tm_info->tm_year + 1900,
        tm_info->tm_mon + 1,
        tm_info->tm_mday,
        tm_info->tm_hour,
        tm_info->tm_min,
        tm_info->tm_sec);

    // Add random suffix (6 hex characters)
    u32 random_value = get_random<u32>();
    id_builder.appendff("{:06x}", random_value & 0xFFFFFF);

    return id_builder.to_string();
}

// Mark a quarantined file as orphaned by creating a .orphaned marker file
// This allows later cleanup attempts to identify and remove orphaned files
static ErrorOr<void> mark_as_orphaned(String const& quarantine_dir, String const& quarantine_id)
{
    // Build orphan marker file path
    StringBuilder orphan_path_builder;
    orphan_path_builder.append(quarantine_dir);
    orphan_path_builder.append('/');
    orphan_path_builder.append(quarantine_id);
    orphan_path_builder.append(".orphaned"sv);
    auto orphan_path = TRY(orphan_path_builder.to_string());

    // Write marker file with timestamp
    auto now = UnixDateTime::now();
    time_t timestamp = now.seconds_since_epoch();
    auto timestamp_str = ByteString::formatted("Orphaned at timestamp: {}\n", timestamp);

    auto file = TRY(Core::File::open(orphan_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(timestamp_str.bytes()));

    dbgln("Quarantine: Marked {} as orphaned", quarantine_id);
    return {};
}

// Attempt to remove a file with exponential backoff retry logic
// Returns true if successful, false if all retries failed
static bool remove_file_with_retry(String const& file_path, int max_attempts = 3)
{
    constexpr int initial_delay_ms = 100;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto remove_result = FileSystem::remove(file_path, FileSystem::RecursionMode::Disallowed);
        if (!remove_result.is_error()) {
            if (attempt > 0) {
                dbgln("Quarantine: Successfully removed {} on attempt {}", file_path, attempt + 1);
            }
            return true;
        }

        dbgln("Quarantine: Attempt {} to remove {} failed: {}", attempt + 1, file_path, remove_result.error());

        // Don't sleep after last attempt
        if (attempt < max_attempts - 1) {
            // Exponential backoff: 100ms, 200ms, 400ms
            int delay_ms = initial_delay_ms * (1 << attempt);
            struct timespec sleep_time;
            sleep_time.tv_sec = delay_ms / 1000;
            sleep_time.tv_nsec = (delay_ms % 1000) * 1000000;
            nanosleep(&sleep_time, nullptr);
        }
    }

    dbgln("Quarantine: Failed to remove {} after {} attempts", file_path, max_attempts);
    return false;
}

ErrorOr<void> Quarantine::cleanup_orphaned_files()
{
    auto quarantine_dir = TRY(get_quarantine_directory());
    auto quarantine_dir_byte_string = quarantine_dir.to_byte_string();

    // Check if directory exists
    if (!FileSystem::exists(quarantine_dir_byte_string)) {
        dbgln("Quarantine: Directory does not exist, no orphans to clean");
        return {};
    }

    int orphans_found = 0;
    int orphans_cleaned = 0;
    int orphans_remaining = 0;

    // Scan for .orphaned marker files
    TRY(Core::Directory::for_each_entry(quarantine_dir_byte_string, Core::DirIterator::SkipParentAndBaseDir, [&](auto const& entry, auto const&) -> ErrorOr<IterationDecision> {
        // Only process .orphaned marker files
        if (!entry.name.ends_with(".orphaned"sv))
            return IterationDecision::Continue;

        orphans_found++;

        // Extract quarantine ID (remove .orphaned extension)
        auto quarantine_id_byte = entry.name.substring(0, entry.name.length() - 9);
        auto quarantine_id_result = String::from_byte_string(quarantine_id_byte);

        if (quarantine_id_result.is_error()) {
            dbgln("Quarantine: Failed to convert orphan ID: {}", entry.name);
            return IterationDecision::Continue;
        }

        auto quarantine_id = quarantine_id_result.release_value();
        dbgln("Quarantine: Found orphaned file marker: {}", quarantine_id);

        // Build paths for orphaned file and marker
        StringBuilder bin_path_builder;
        bin_path_builder.append(quarantine_dir);
        bin_path_builder.append('/');
        bin_path_builder.append(quarantine_id);
        bin_path_builder.append(".bin"sv);
        auto bin_path_result = bin_path_builder.to_string();

        StringBuilder marker_path_builder;
        marker_path_builder.append(quarantine_dir);
        marker_path_builder.append('/');
        marker_path_builder.append(quarantine_id);
        marker_path_builder.append(".orphaned"sv);
        auto marker_path_result = marker_path_builder.to_string();

        if (bin_path_result.is_error() || marker_path_result.is_error()) {
            dbgln("Quarantine: Failed to build paths for orphan cleanup");
            return IterationDecision::Continue;
        }

        auto bin_path = bin_path_result.release_value();
        auto marker_path = marker_path_result.release_value();

        // Attempt to remove the orphaned .bin file (if it exists)
        bool bin_removed = true;
        if (FileSystem::exists(bin_path)) {
            bin_removed = remove_file_with_retry(bin_path);
        }

        if (bin_removed) {
            // Successfully removed .bin (or it didn't exist), now remove marker
            bool marker_removed = remove_file_with_retry(marker_path);
            if (marker_removed) {
                orphans_cleaned++;
                dbgln("Quarantine: Successfully cleaned up orphan {}", quarantine_id);
            } else {
                orphans_remaining++;
                dbgln("Quarantine: Warning - Removed orphaned .bin but failed to remove marker: {}", quarantine_id);
            }
        } else {
            orphans_remaining++;
            dbgln("Quarantine: Warning - Failed to remove orphaned .bin file: {}", quarantine_id);
            // Keep the marker so we can try again later
        }

        return IterationDecision::Continue;
    }));

    if (orphans_found > 0) {
        dbgln("Quarantine: Orphan cleanup summary - Found: {}, Cleaned: {}, Remaining: {}",
            orphans_found, orphans_cleaned, orphans_remaining);
    }

    return {};
}

ErrorOr<String> Quarantine::quarantine_file(
    String const& source_path,
    QuarantineMetadata const& metadata)
{
    // Validate metadata fields using InputValidator
    auto filename_result = Sentinel::InputValidator::validate_length(
        ByteString(metadata.filename).view(), 1, 255, "filename"sv);
    if (!filename_result.is_valid)
        return Error::from_string_view(filename_result.error_message);

    auto url_result = Sentinel::InputValidator::validate_length(
        ByteString(metadata.original_url).view(), 0, 2048, "original_url"sv);
    if (!url_result.is_valid)
        return Error::from_string_view(url_result.error_message);

    // Validate SHA256 hash format
    if (!metadata.sha256.is_empty()) {
        auto hash_result = Sentinel::InputValidator::validate_sha256(
            ByteString(metadata.sha256).view());
        if (!hash_result.is_valid)
            return Error::from_string_view(hash_result.error_message);
    }

    // Validate file size is positive
    if (metadata.file_size == 0)
        return Error::from_string_literal("Invalid metadata: file_size cannot be zero");

    // Ensure quarantine directory exists
    auto init_result = initialize();
    if (init_result.is_error()) {
        dbgln("Quarantine: Failed to initialize: {}", init_result.error());
        return Error::from_string_literal("Cannot initialize quarantine directory. Check disk space and permissions.");
    }

    // Generate unique quarantine ID
    auto quarantine_id = TRY(generate_quarantine_id());
    auto quarantine_dir = TRY(get_quarantine_directory());

    // Build destination path for quarantined file
    StringBuilder dest_path_builder;
    dest_path_builder.append(quarantine_dir);
    dest_path_builder.append('/');
    dest_path_builder.append(quarantine_id);
    dest_path_builder.append(".bin"sv);
    auto dest_path = TRY(dest_path_builder.to_string());

    // Check if destination already exists (highly unlikely but possible)
    if (FileSystem::exists(dest_path)) {
        dbgln("Quarantine: Destination file already exists: {}", dest_path);
        return Error::from_string_literal("Quarantine file already exists. Try again.");
    }

    // Move file to quarantine directory (atomic operation)
    dbgln("Quarantine: Moving {} to {}", source_path, dest_path);
    auto move_result = FileSystem::move_file(dest_path, source_path, FileSystem::PreserveMode::Nothing);
    if (move_result.is_error()) {
        dbgln("Quarantine: Failed to move file: {}", move_result.error());
        // Provide user-friendly error messages based on common failures
        auto error_str = move_result.error().string_literal();
        if (ByteString(error_str).contains("No space left"sv)) {
            return Error::from_string_literal("Cannot quarantine file: Disk is full. Free up space and try again.");
        } else if (ByteString(error_str).contains("Permission denied"sv)) {
            return Error::from_string_literal("Cannot quarantine file: Permission denied. Check file system permissions.");
        }
        return Error::from_string_literal("Cannot quarantine file. Check disk space and permissions.");
    }

    // Set restrictive permissions on quarantined file (owner read-only: r--------)
    auto chmod_result = Core::System::chmod(dest_path, 0400);
    if (chmod_result.is_error()) {
        dbgln("Quarantine: Failed to set permissions: {}", chmod_result.error());
        // Don't fail the quarantine operation if chmod fails, just log warning
        dbgln("Quarantine: Warning - could not set restrictive permissions on quarantined file");
    }

    // Update metadata with quarantine ID
    QuarantineMetadata updated_metadata = metadata;
    updated_metadata.quarantine_id = quarantine_id.to_byte_string();

    // Write metadata JSON file
    auto metadata_result = write_metadata(quarantine_id, updated_metadata);
    if (metadata_result.is_error()) {
        dbgln("Quarantine: Failed to write metadata: {}", metadata_result.error());

        // CRITICAL: Clean up quarantined file if metadata write fails
        // Use retry logic with exponential backoff (3 attempts: 100ms, 200ms, 400ms)
        bool cleanup_successful = remove_file_with_retry(dest_path);

        if (!cleanup_successful) {
            // CRITICAL: Cleanup failed after retries - mark file as orphaned
            dbgln("Quarantine: CRITICAL - Failed to clean up quarantined file after metadata error");
            dbgln("Quarantine: Orphaned file will be cleaned up on next initialization");

            // Create orphan marker for future cleanup
            auto mark_result = mark_as_orphaned(quarantine_dir, quarantine_id);
            if (mark_result.is_error()) {
                dbgln("Quarantine: CRITICAL - Failed to mark orphaned file: {}", mark_result.error());
                return Error::from_string_literal("Cannot write quarantine metadata and cleanup failed. Orphaned file exists at quarantine directory. Manual cleanup may be required.");
            }

            return Error::from_string_literal("Cannot write quarantine metadata. Cleanup failed but file marked as orphaned for automatic recovery.");
        }

        return Error::from_string_literal("Cannot write quarantine metadata. The file was not quarantined.");
    }

    dbgln("Quarantine: Successfully quarantined file with ID: {}", quarantine_id);
    return quarantine_id;
}

ErrorOr<void> Quarantine::write_metadata(
    String const& quarantine_id,
    QuarantineMetadata const& metadata)
{
    auto quarantine_dir = TRY(get_quarantine_directory());

    // Build metadata file path
    StringBuilder metadata_path_builder;
    metadata_path_builder.append(quarantine_dir);
    metadata_path_builder.append('/');
    metadata_path_builder.append(quarantine_id);
    metadata_path_builder.append(".json"sv);
    auto metadata_path = TRY(metadata_path_builder.to_string());

    // Build JSON object
    JsonObject json;
    json.set("original_url"sv, JsonValue(metadata.original_url));
    json.set("filename"sv, JsonValue(metadata.filename));
    json.set("detection_time"sv, JsonValue(metadata.detection_time));
    json.set("sha256"sv, JsonValue(metadata.sha256));
    json.set("file_size"sv, JsonValue(static_cast<u64>(metadata.file_size)));
    json.set("quarantine_id"sv, JsonValue(metadata.quarantine_id));

    // Add rule names as JSON array
    JsonArray rules_array;
    for (auto const& rule : metadata.rule_names) {
        TRY(rules_array.append(JsonValue(rule)));
    }
    json.set("rule_names"sv, move(rules_array));

    // Serialize to JSON string
    auto json_string = json.serialized();

    // Write to file
    auto file = TRY(Core::File::open(metadata_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(json_string.bytes()));

    // Set restrictive permissions on metadata file (owner read-only: r--------)
    TRY(Core::System::chmod(metadata_path, 0400));

    dbgln("Quarantine: Wrote metadata to {}", metadata_path);
    return {};
}

ErrorOr<QuarantineMetadata> Quarantine::get_metadata(String const& quarantine_id)
{
    return read_metadata(quarantine_id);
}

ErrorOr<QuarantineMetadata> Quarantine::read_metadata(String const& quarantine_id)
{
    // SECURITY: Validate quarantine ID format to prevent path traversal attacks
    // Must be exactly: YYYYMMDD_HHMMSS_XXXXXX (21 chars)
    if (!is_valid_quarantine_id(quarantine_id)) {
        dbgln("Quarantine: Invalid quarantine ID format: {}", quarantine_id);
        return Error::from_string_literal("Invalid quarantine ID format. Expected format: YYYYMMDD_HHMMSS_XXXXXX");
    }

    auto quarantine_dir = TRY(get_quarantine_directory());

    // Build metadata file path
    StringBuilder metadata_path_builder;
    metadata_path_builder.append(quarantine_dir);
    metadata_path_builder.append('/');
    metadata_path_builder.append(quarantine_id);
    metadata_path_builder.append(".json"sv);
    auto metadata_path = TRY(metadata_path_builder.to_string());

    // Read file contents
    auto file = TRY(Core::File::open(metadata_path, Core::File::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());
    auto json_string = ByteString(contents.bytes());

    // Parse JSON
    auto json_result = JsonValue::from_string(json_string);
    if (json_result.is_error()) {
        return Error::from_string_literal("Failed to parse quarantine metadata JSON");
    }

    auto json = json_result.value();
    if (!json.is_object()) {
        return Error::from_string_literal("Expected JSON object in metadata file");
    }

    auto obj = json.as_object();

    // Extract fields
    QuarantineMetadata metadata;

    auto original_url = obj.get_string("original_url"sv);
    if (!original_url.has_value())
        return Error::from_string_literal("Missing 'original_url' in metadata");
    metadata.original_url = original_url.value().to_byte_string();

    auto filename = obj.get_string("filename"sv);
    if (!filename.has_value())
        return Error::from_string_literal("Missing 'filename' in metadata");
    metadata.filename = filename.value().to_byte_string();

    auto detection_time = obj.get_string("detection_time"sv);
    if (!detection_time.has_value())
        return Error::from_string_literal("Missing 'detection_time' in metadata");
    metadata.detection_time = detection_time.value().to_byte_string();

    auto sha256 = obj.get_string("sha256"sv);
    if (!sha256.has_value())
        return Error::from_string_literal("Missing 'sha256' in metadata");
    metadata.sha256 = sha256.value().to_byte_string();

    auto file_size = obj.get_u64("file_size"sv);
    if (!file_size.has_value())
        return Error::from_string_literal("Missing 'file_size' in metadata");
    metadata.file_size = static_cast<size_t>(file_size.value());

    auto quarantine_id_str = obj.get_string("quarantine_id"sv);
    if (!quarantine_id_str.has_value())
        return Error::from_string_literal("Missing 'quarantine_id' in metadata");
    metadata.quarantine_id = quarantine_id_str.value().to_byte_string();

    // Extract rule names array
    auto rules_array_opt = obj.get_array("rule_names"sv);
    if (!rules_array_opt.has_value())
        return Error::from_string_literal("Missing 'rule_names' in metadata");

    auto const& rules_array = rules_array_opt.value();
    for (size_t i = 0; i < rules_array.size(); i++) {
        auto rule = rules_array.at(i);
        if (rule.is_string()) {
            metadata.rule_names.append(rule.as_string().to_byte_string());
        }
    }

    return metadata;
}

ErrorOr<Vector<QuarantineMetadata>> Quarantine::list_all_entries()
{
    Vector<QuarantineMetadata> entries;

    auto quarantine_dir = TRY(get_quarantine_directory());
    auto quarantine_dir_byte_string = quarantine_dir.to_byte_string();

    // Check if directory exists
    if (!FileSystem::exists(quarantine_dir_byte_string)) {
        dbgln("Quarantine: Directory does not exist: {}", quarantine_dir);
        return entries; // Return empty vector
    }

    // Iterate through directory looking for .json metadata files
    TRY(Core::Directory::for_each_entry(quarantine_dir_byte_string, Core::DirIterator::SkipParentAndBaseDir, [&](auto const& entry, auto const&) -> ErrorOr<IterationDecision> {
        // Only process .json files
        if (!entry.name.ends_with(".json"sv))
            return IterationDecision::Continue;

        // Extract quarantine ID (remove .json extension)
        auto quarantine_id_byte = entry.name.substring(0, entry.name.length() - 5);
        auto quarantine_id_result = String::from_byte_string(quarantine_id_byte);

        if (quarantine_id_result.is_error()) {
            dbgln("Quarantine: Failed to convert quarantine ID: {}", entry.name);
            return IterationDecision::Continue;
        }

        auto quarantine_id = quarantine_id_result.release_value();

        // Read metadata (which will validate the ID format)
        auto metadata_result = read_metadata(quarantine_id);
        if (metadata_result.is_error()) {
            dbgln("Quarantine: Failed to read metadata for {}: {}", quarantine_id, metadata_result.error());
            return IterationDecision::Continue;
        }

        entries.append(metadata_result.release_value());
        return IterationDecision::Continue;
    }));

    dbgln("Quarantine: Found {} quarantined files", entries.size());
    return entries;
}

// SECURITY: Validate and sanitize destination directory for file restoration
// Prevents path traversal attacks via directory parameter
static ErrorOr<String> validate_restore_destination(StringView dest_dir)
{
    // Resolve to canonical path (handles .., symlinks, etc.)
    auto canonical_result = FileSystem::real_path(dest_dir);
    if (canonical_result.is_error()) {
        dbgln("Quarantine: Failed to resolve destination path: {}", canonical_result.error());
        return Error::from_string_literal("Cannot resolve destination directory path. Check that it exists.");
    }

    auto canonical = canonical_result.release_value();

    // Ensure it's an absolute path
    if (!canonical.starts_with("/"sv)) {
        dbgln("Quarantine: Destination is not an absolute path: {}", canonical);
        return Error::from_string_literal("Destination must be an absolute path.");
    }

    // Verify it's actually a directory
    if (!FileSystem::is_directory(canonical)) {
        dbgln("Quarantine: Destination is not a directory: {}", canonical);
        return Error::from_string_literal("Destination is not a directory.");
    }

    // Check write permissions
    auto access_result = Core::System::access(canonical, W_OK);
    if (access_result.is_error()) {
        dbgln("Quarantine: No write permission to destination: {}", canonical);
        return Error::from_string_literal("Destination directory is not writable. Check permissions.");
    }

    dbgln("Quarantine: Validated destination directory: {}", canonical);
    return String::from_byte_string(canonical);
}

// SECURITY: Sanitize filename to prevent path traversal attacks
// Removes dangerous characters and path components from filename
static String sanitize_filename(StringView filename)
{
    // First, extract just the basename (remove any path components)
    // Handle both Unix and Windows path separators
    StringView basename = filename;

    // Find the last occurrence of either forward or back slash
    auto last_slash = basename.find_last('/');
    auto last_backslash = basename.find_last('\\');

    Optional<size_t> last_separator;
    if (last_slash.has_value() && last_backslash.has_value()) {
        last_separator = max(last_slash.value(), last_backslash.value());
    } else if (last_slash.has_value()) {
        last_separator = last_slash;
    } else if (last_backslash.has_value()) {
        last_separator = last_backslash;
    }

    if (last_separator.has_value()) {
        basename = basename.substring_view(last_separator.value() + 1);
    }

    // Now filter out dangerous characters
    StringBuilder safe_filename;
    for (auto byte : basename.bytes()) {
        char ch = static_cast<char>(byte);

        // Remove dangerous characters:
        // - Path separators (/, \)
        // - Null bytes
        // - Control characters (ASCII < 32)
        bool is_safe = ch >= 32 && ch != '/' && ch != '\\';

        if (is_safe) {
            safe_filename.append(ch);
        }
    }

    auto result = safe_filename.to_string();

    // If sanitization resulted in empty string, use a safe default
    if (result.is_error() || result.value().is_empty()) {
        dbgln("Quarantine: Filename sanitization resulted in empty name, using default");
        return String::from_utf8_without_validation("quarantined_file"sv.bytes());
    }

    auto safe_name = result.release_value();
    if (safe_name != filename) {
        dbgln("Quarantine: Sanitized filename '{}' -> '{}'", filename, safe_name);
    }

    return safe_name;
}

ErrorOr<void> Quarantine::restore_file(String const& quarantine_id, String const& destination_dir)
{
    // SECURITY: Validate quarantine ID format to prevent path traversal attacks
    // Must be exactly: YYYYMMDD_HHMMSS_XXXXXX (21 chars)
    if (!is_valid_quarantine_id(quarantine_id)) {
        dbgln("Quarantine: Invalid quarantine ID format: {}", quarantine_id);
        return Error::from_string_literal("Invalid quarantine ID format. Expected format: YYYYMMDD_HHMMSS_XXXXXX");
    }

    // SECURITY: Validate and canonicalize destination directory
    auto validated_dest = TRY(validate_restore_destination(destination_dir));

    auto quarantine_dir = TRY(get_quarantine_directory());

    // Build source paths
    StringBuilder source_file_builder;
    source_file_builder.append(quarantine_dir);
    source_file_builder.append('/');
    source_file_builder.append(quarantine_id);
    source_file_builder.append(".bin"sv);
    auto source_file = TRY(source_file_builder.to_string());

    // Check if quarantined file exists
    if (!FileSystem::exists(source_file)) {
        dbgln("Quarantine: Quarantined file not found: {}", source_file);
        return Error::from_string_literal("Quarantined file not found. It may have been deleted.");
    }

    // Read metadata to get original filename (read_metadata validates ID again)
    auto metadata = TRY(read_metadata(quarantine_id));

    // SECURITY: Sanitize filename to prevent path traversal
    auto safe_filename = sanitize_filename(metadata.filename);

    // Build destination path using validated directory and sanitized filename
    StringBuilder dest_path_builder;
    dest_path_builder.append(validated_dest);
    dest_path_builder.append('/');
    dest_path_builder.append(safe_filename);
    auto dest_path = TRY(dest_path_builder.to_string());

    // Check if destination already exists
    if (FileSystem::exists(dest_path)) {
        // Append number to make unique
        for (int i = 1; i < 1000; i++) {
            StringBuilder unique_dest_builder;
            unique_dest_builder.append(validated_dest);
            unique_dest_builder.append('/');
            unique_dest_builder.append(safe_filename);
            unique_dest_builder.appendff("_({})", i);
            auto unique_dest = TRY(unique_dest_builder.to_string());

            if (!FileSystem::exists(unique_dest)) {
                dest_path = unique_dest;
                break;
            }
        }
    }

    // Move file from quarantine to destination
    dbgln("Quarantine: Restoring {} to {}", source_file, dest_path);
    auto move_result = FileSystem::move_file(dest_path, source_file, FileSystem::PreserveMode::Nothing);
    if (move_result.is_error()) {
        dbgln("Quarantine: Failed to restore file: {}", move_result.error());
        auto error_str = move_result.error().string_literal();
        if (ByteString(error_str).contains("No space left"sv)) {
            return Error::from_string_literal("Cannot restore file: Disk is full. Free up space and try again.");
        } else if (ByteString(error_str).contains("Permission denied"sv)) {
            return Error::from_string_literal("Cannot restore file: Permission denied. Check destination permissions.");
        }
        return Error::from_string_literal("Cannot restore file. Check disk space and permissions.");
    }

    // Restore normal permissions (owner read/write: rw-------)
    auto chmod_result = Core::System::chmod(dest_path, 0600);
    if (chmod_result.is_error()) {
        dbgln("Quarantine: Warning - failed to restore permissions: {}", chmod_result.error());
        // Continue anyway - file is restored even if permissions couldn't be set
    }

    // Delete metadata file
    StringBuilder metadata_path_builder;
    metadata_path_builder.append(quarantine_dir);
    metadata_path_builder.append('/');
    metadata_path_builder.append(quarantine_id);
    metadata_path_builder.append(".json"sv);
    auto metadata_path = TRY(metadata_path_builder.to_string());

    TRY(FileSystem::remove(metadata_path, FileSystem::RecursionMode::Disallowed));

    dbgln("Quarantine: Successfully restored file to {}", dest_path);
    return {};
}

ErrorOr<void> Quarantine::delete_file(String const& quarantine_id)
{
    // SECURITY: Validate quarantine ID format to prevent path traversal attacks
    // Must be exactly: YYYYMMDD_HHMMSS_XXXXXX (21 chars)
    if (!is_valid_quarantine_id(quarantine_id)) {
        dbgln("Quarantine: Invalid quarantine ID format: {}", quarantine_id);
        return Error::from_string_literal("Invalid quarantine ID format. Expected format: YYYYMMDD_HHMMSS_XXXXXX");
    }

    auto quarantine_dir = TRY(get_quarantine_directory());

    // Build file paths
    StringBuilder file_path_builder;
    file_path_builder.append(quarantine_dir);
    file_path_builder.append('/');
    file_path_builder.append(quarantine_id);
    file_path_builder.append(".bin"sv);
    auto file_path = TRY(file_path_builder.to_string());

    StringBuilder metadata_path_builder;
    metadata_path_builder.append(quarantine_dir);
    metadata_path_builder.append('/');
    metadata_path_builder.append(quarantine_id);
    metadata_path_builder.append(".json"sv);
    auto metadata_path = TRY(metadata_path_builder.to_string());

    // Delete quarantined file (if it exists)
    if (FileSystem::exists(file_path)) {
        TRY(FileSystem::remove(file_path, FileSystem::RecursionMode::Disallowed));
        dbgln("Quarantine: Deleted file {}", file_path);
    }

    // Delete metadata file (if it exists)
    if (FileSystem::exists(metadata_path)) {
        TRY(FileSystem::remove(metadata_path, FileSystem::RecursionMode::Disallowed));
        dbgln("Quarantine: Deleted metadata {}", metadata_path);
    }

    dbgln("Quarantine: Successfully deleted quarantine entry {}", quarantine_id);
    return {};
}

}
