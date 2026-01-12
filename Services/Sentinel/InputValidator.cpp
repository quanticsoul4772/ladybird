/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "InputValidator.h"
#include <AK/CharacterTypes.h>
#include <AK/StringBuilder.h>
#include <AK/Time.h>

namespace Sentinel {

// String validation functions

ValidationResult InputValidator::validate_non_empty(StringView str, StringView field_name)
{
    if (str.is_empty()) {
        StringBuilder msg;
        msg.append("Field '"sv);
        msg.append(field_name);
        msg.append("' cannot be empty"sv);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field cannot be empty"sv);
        return ValidationResult::error(msg_str.value());
    }
    return ValidationResult::success();
}

ValidationResult InputValidator::validate_length(StringView str, size_t min, size_t max, StringView field_name)
{
    size_t length = str.length();

    if (length < min) {
        StringBuilder msg;
        msg.appendff("Field '{}' is too short (min {} bytes, got {} bytes)", field_name, min, length);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field is too short"sv);
        return ValidationResult::error(msg_str.value());
    }

    if (length > max) {
        StringBuilder msg;
        msg.appendff("Field '{}' is too long (max {} bytes, got {} bytes)", field_name, max, length);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field is too long"sv);
        return ValidationResult::error(msg_str.value());
    }

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_ascii_only(StringView str, StringView field_name)
{
    for (auto byte : str.bytes()) {
        if (byte > 127) {
            StringBuilder msg;
            msg.append("Field '"sv);
            msg.append(field_name);
            msg.append("' must contain only ASCII characters"sv);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("Field must contain only ASCII characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }
    return ValidationResult::success();
}

ValidationResult InputValidator::validate_no_control_chars(StringView str, StringView field_name)
{
    for (auto byte : str.bytes()) {
        char ch = static_cast<char>(byte);
        // Control characters are ASCII 0-31 (except tab, newline, carriage return which might be valid)
        // and ASCII 127 (DEL)
        if ((ch >= 0 && ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') || ch == 127) {
            StringBuilder msg;
            msg.append("Field '"sv);
            msg.append(field_name);
            msg.append("' contains control characters"sv);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("Field contains control characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }
    return ValidationResult::success();
}

ValidationResult InputValidator::validate_printable_chars(StringView str, StringView field_name)
{
    for (auto byte : str.bytes()) {
        char ch = static_cast<char>(byte);
        // Printable characters are ASCII 32-126
        if (ch < 32 || ch > 126) {
            StringBuilder msg;
            msg.append("Field '"sv);
            msg.append(field_name);
            msg.append("' must contain only printable characters"sv);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("Field must contain only printable characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }
    return ValidationResult::success();
}

// Number validation functions

ValidationResult InputValidator::validate_positive(i64 value, StringView field_name)
{
    if (value <= 0) {
        StringBuilder msg;
        msg.appendff("Field '{}' must be positive (got {})", field_name, value);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field must be positive"sv);
        return ValidationResult::error(msg_str.value());
    }
    return ValidationResult::success();
}

ValidationResult InputValidator::validate_non_negative(i64 value, StringView field_name)
{
    if (value < 0) {
        StringBuilder msg;
        msg.appendff("Field '{}' must be non-negative (got {})", field_name, value);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field must be non-negative"sv);
        return ValidationResult::error(msg_str.value());
    }
    return ValidationResult::success();
}

ValidationResult InputValidator::validate_range(i64 value, i64 min, i64 max, StringView field_name)
{
    if (value < min || value > max) {
        StringBuilder msg;
        msg.appendff("Field '{}' out of range (min {}, max {}, got {})", field_name, min, max, value);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field out of range"sv);
        return ValidationResult::error(msg_str.value());
    }
    return ValidationResult::success();
}

// URL/Path validation functions

ValidationResult InputValidator::validate_url_pattern(StringView pattern)
{
    // Validate length
    auto length_result = validate_length(pattern, 0, 2048, "url_pattern"sv);
    if (!length_result.is_valid)
        return length_result;

    // Empty pattern is valid (matches nothing)
    if (pattern.is_empty())
        return ValidationResult::success();

    // Allow only safe URL pattern characters: alphanumeric, /, -, _, ., *, %
    // Also allow : for protocol (http://, https://)
    for (auto ch : pattern) {
        bool is_safe = is_ascii_alphanumeric(ch)
            || ch == '/' || ch == '-' || ch == '_'
            || ch == '.' || ch == '*' || ch == '%'
            || ch == ':';

        if (!is_safe) {
            StringBuilder msg;
            msg.appendff("URL pattern contains unsafe character: '{}'", ch);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("URL pattern contains unsafe characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }

    // Limit wildcards to prevent DoS
    size_t wildcard_count = 0;
    for (auto ch : pattern) {
        if (ch == '*' || ch == '%')
            wildcard_count++;
    }

    if (wildcard_count > 10)
        return ValidationResult::error("URL pattern has too many wildcards (max 10)"sv);

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_file_path(StringView path)
{
    // Validate length
    auto length_result = validate_length(path, 1, 4096, "file_path"sv);
    if (!length_result.is_valid)
        return length_result;

    // Check for null bytes (path truncation attack)
    for (auto byte : path.bytes()) {
        if (byte == 0)
            return ValidationResult::error("File path contains null bytes"sv);
    }

    // Check for control characters
    auto control_result = validate_no_control_chars(path, "file_path"sv);
    if (!control_result.is_valid)
        return control_result;

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_safe_url_chars(StringView url, StringView field_name)
{
    // Validate length
    auto length_result = validate_length(url, 0, 2048, field_name);
    if (!length_result.is_valid)
        return length_result;

    // Allow URL-safe characters: alphanumeric, :/?#[]@!$&'()*+,;=.-_~%
    for (auto ch : url) {
        bool is_safe = is_ascii_alphanumeric(ch)
            || ch == ':' || ch == '/' || ch == '?' || ch == '#'
            || ch == '[' || ch == ']' || ch == '@' || ch == '!'
            || ch == '$' || ch == '&' || ch == '\'' || ch == '('
            || ch == ')' || ch == '*' || ch == '+' || ch == ','
            || ch == ';' || ch == '=' || ch == '.' || ch == '-'
            || ch == '_' || ch == '~' || ch == '%';

        if (!is_safe) {
            StringBuilder msg;
            msg.append("Field '"sv);
            msg.append(field_name);
            msg.append("' contains invalid URL characters"sv);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("Field contains invalid URL characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }

    return ValidationResult::success();
}

// Hash validation functions

ValidationResult InputValidator::validate_sha256(StringView hash)
{
    return validate_hex_string(hash, 64, "sha256"sv);
}

ValidationResult InputValidator::validate_hex_string(StringView str, size_t expected_length, StringView field_name)
{
    // Empty hash is valid (means no hash available)
    if (str.is_empty())
        return ValidationResult::success();

    // Check length
    if (str.length() != expected_length) {
        StringBuilder msg;
        msg.appendff("Field '{}' has invalid length (expected {} hex chars, got {})",
            field_name, expected_length, str.length());
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Field has invalid length"sv);
        return ValidationResult::error(msg_str.value());
    }

    // Verify all characters are hex digits
    for (auto ch : str) {
        if (!is_ascii_hex_digit(ch)) {
            StringBuilder msg;
            msg.append("Field '"sv);
            msg.append(field_name);
            msg.append("' must contain only hex characters (0-9, a-f, A-F)"sv);
            auto msg_str = msg.to_string();
            if (msg_str.is_error())
                return ValidationResult::error("Field must contain only hex characters"sv);
            return ValidationResult::error(msg_str.value());
        }
    }

    return ValidationResult::success();
}

// Timestamp validation functions

ValidationResult InputValidator::validate_timestamp(i64 timestamp_ms)
{
    // Timestamp must be non-negative
    if (timestamp_ms < 0)
        return ValidationResult::error("Timestamp cannot be negative"sv);

    // Get current time
    auto now = UnixDateTime::now();
    auto now_ms = now.milliseconds_since_epoch();

    // Allow timestamps up to 1 year in the future (to handle clock skew)
    constexpr i64 ONE_YEAR_MS = 365LL * 24 * 60 * 60 * 1000;
    i64 max_future = now_ms + ONE_YEAR_MS;

    if (timestamp_ms > max_future) {
        StringBuilder msg;
        msg.appendff("Timestamp is too far in the future (max 1 year from now)");
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Timestamp is too far in the future"sv);
        return ValidationResult::error(msg_str.value());
    }

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_expiry(i64 expires_at_ms)
{
    // -1 means never expires
    if (expires_at_ms == -1)
        return ValidationResult::success();

    // Get current time
    auto now = UnixDateTime::now();
    auto now_ms = now.milliseconds_since_epoch();

    // Expiry must be in the future
    if (expires_at_ms <= now_ms)
        return ValidationResult::error("Expiry time must be in the future"sv);

    // Allow up to 10 years in the future
    constexpr i64 TEN_YEARS_MS = 10LL * 365 * 24 * 60 * 60 * 1000;
    i64 max_expiry = now_ms + TEN_YEARS_MS;

    if (expires_at_ms > max_expiry)
        return ValidationResult::error("Expiry time is too far in the future (max 10 years)"sv);

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_timestamp_range(i64 timestamp_ms, i64 min_ms, i64 max_ms, StringView field_name)
{
    if (timestamp_ms < min_ms || timestamp_ms > max_ms) {
        StringBuilder msg;
        msg.appendff("Field '{}' timestamp out of range", field_name);
        auto msg_str = msg.to_string();
        if (msg_str.is_error())
            return ValidationResult::error("Timestamp out of range"sv);
        return ValidationResult::error(msg_str.value());
    }
    return ValidationResult::success();
}

// Action validation functions

ValidationResult InputValidator::validate_action(StringView action)
{
    // Valid actions: allow, block, quarantine, block_autofill, warn_user
    if (action == "allow"sv || action == "block"sv || action == "quarantine"sv
        || action == "block_autofill"sv || action == "warn_user"sv) {
        return ValidationResult::success();
    }

    StringBuilder msg;
    msg.append("Invalid action: '"sv);
    msg.append(action);
    msg.append("' (must be: allow, block, quarantine, block_autofill, or warn_user)"sv);
    auto msg_str = msg.to_string();
    if (msg_str.is_error())
        return ValidationResult::error("Invalid action"sv);
    return ValidationResult::error(msg_str.value());
}

ValidationResult InputValidator::validate_match_type(StringView match_type)
{
    // Valid match types: download, form_mismatch, insecure_cred, third_party_form
    if (match_type == "download"sv || match_type == "form_mismatch"sv
        || match_type == "insecure_cred"sv || match_type == "third_party_form"sv) {
        return ValidationResult::success();
    }

    StringBuilder msg;
    msg.append("Invalid match type: '"sv);
    msg.append(match_type);
    msg.append("' (must be: download, form_mismatch, insecure_cred, or third_party_form)"sv);
    auto msg_str = msg.to_string();
    if (msg_str.is_error())
        return ValidationResult::error("Invalid match type"sv);
    return ValidationResult::error(msg_str.value());
}

// MIME type validation

ValidationResult InputValidator::validate_mime_type(StringView mime_type)
{
    // Empty MIME type is valid
    if (mime_type.is_empty())
        return ValidationResult::success();

    // Validate length
    auto length_result = validate_length(mime_type, 0, 255, "mime_type"sv);
    if (!length_result.is_valid)
        return length_result;

    // MIME type format: type/subtype
    // Allow alphanumeric, /, -, +, .
    for (auto ch : mime_type) {
        bool is_valid = is_ascii_alphanumeric(ch)
            || ch == '/' || ch == '-' || ch == '+' || ch == '.';

        if (!is_valid)
            return ValidationResult::error("MIME type contains invalid characters"sv);
    }

    // Should contain exactly one /
    size_t slash_count = 0;
    for (auto ch : mime_type) {
        if (ch == '/')
            slash_count++;
    }

    if (slash_count != 1)
        return ValidationResult::error("MIME type must be in format: type/subtype"sv);

    return ValidationResult::success();
}

// Configuration validation

ValidationResult InputValidator::validate_config_value(StringView key, JsonValue const& value)
{
    // Validate policy_cache_size
    if (key == "policy_cache_size"sv) {
        if (!value.is_number())
            return ValidationResult::error("policy_cache_size must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1, 100000, "policy_cache_size"sv);
    }

    // Validate threat_retention_days
    if (key == "threat_retention_days"sv) {
        if (!value.is_number())
            return ValidationResult::error("threat_retention_days must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1, 3650, "threat_retention_days"sv); // 1 day to 10 years
    }

    // Validate worker_threads (if present in config)
    if (key == "worker_threads"sv) {
        if (!value.is_number())
            return ValidationResult::error("worker_threads must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1, 64, "worker_threads"sv);
    }

    // Validate max_scan_size
    if (key == "max_scan_size"sv || key == "max_file_size"sv) {
        if (!value.is_number())
            return ValidationResult::error("Scan size must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1024, 1024LL * 1024 * 1024 * 10, key); // 1KB to 10GB
    }

    // Validate timeout values
    if (key.ends_with("_timeout"sv) || key.ends_with("_timeout_ms"sv)) {
        if (!value.is_number())
            return ValidationResult::error("Timeout must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 100, 300000, key); // 100ms to 5 minutes
    }

    // Validate rate limit values
    if (key == "policies_per_minute"sv) {
        if (!value.is_number())
            return ValidationResult::error("policies_per_minute must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1, 1000, "policies_per_minute"sv);
    }

    if (key == "rate_window_seconds"sv) {
        if (!value.is_number())
            return ValidationResult::error("rate_window_seconds must be a number"sv);

        auto num = value.as_integer<i64>();
        return validate_range(num, 1, 3600, "rate_window_seconds"sv); // 1 second to 1 hour
    }

    // Validate boolean flags
    if (key == "enabled"sv || key.starts_with("enable_"sv)) {
        if (!value.is_bool())
            return ValidationResult::error("Boolean flag must be true or false"sv);
    }

    // Validate path strings
    if (key.ends_with("_path"sv) || key.ends_with("_dir"sv) || key.ends_with("_directory"sv)) {
        if (!value.is_string())
            return ValidationResult::error("Path must be a string"sv);

        auto path = value.as_string();
        return validate_file_path(path);
    }

    // Unknown keys are allowed (for extensibility)
    return ValidationResult::success();
}

// IPC message validation

ValidationResult InputValidator::validate_quarantine_id(StringView id)
{
    // Expected format: YYYYMMDD_HHMMSS_XXXXXX (exactly 21 characters)
    if (id.length() != 21)
        return ValidationResult::error("Quarantine ID must be 21 characters (format: YYYYMMDD_HHMMSS_XXXXXX)"sv);

    // Validate format character by character
    for (size_t i = 0; i < id.length(); ++i) {
        char ch = id[i];

        if (i < 8) {
            // Date portion: must be digits
            if (!is_ascii_digit(ch))
                return ValidationResult::error("Quarantine ID date portion must be digits"sv);
        } else if (i == 8 || i == 15) {
            // Separators: must be underscores
            if (ch != '_')
                return ValidationResult::error("Quarantine ID must have underscores at positions 8 and 15"sv);
        } else if (i >= 9 && i < 15) {
            // Time portion: must be digits
            if (!is_ascii_digit(ch))
                return ValidationResult::error("Quarantine ID time portion must be digits"sv);
        } else {
            // Random portion: must be hex digits
            if (!is_ascii_hex_digit(ch))
                return ValidationResult::error("Quarantine ID random portion must be hex digits"sv);
        }
    }

    return ValidationResult::success();
}

ValidationResult InputValidator::validate_rule_name(StringView name)
{
    // Validate non-empty
    auto non_empty_result = validate_non_empty(name, "rule_name"sv);
    if (!non_empty_result.is_valid)
        return non_empty_result;

    // Validate length
    auto length_result = validate_length(name, 1, 256, "rule_name"sv);
    if (!length_result.is_valid)
        return length_result;

    // Validate no control characters
    auto control_result = validate_no_control_chars(name, "rule_name"sv);
    if (!control_result.is_valid)
        return control_result;

    return ValidationResult::success();
}

}
