/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Time.h>

namespace Sentinel {

struct ValidationResult {
    bool is_valid;
    String error_message;

    static ValidationResult success()
    {
        return ValidationResult { .is_valid = true, .error_message = String {} };
    }

    static ValidationResult error(StringView message)
    {
        auto msg = String::from_utf8_without_validation(message.bytes());
        return ValidationResult { .is_valid = false, .error_message = msg };
    }
};

class InputValidator {
public:
    // String validation
    static ValidationResult validate_non_empty(StringView str, StringView field_name);
    static ValidationResult validate_length(StringView str, size_t min, size_t max, StringView field_name);
    static ValidationResult validate_ascii_only(StringView str, StringView field_name);
    static ValidationResult validate_no_control_chars(StringView str, StringView field_name);
    static ValidationResult validate_printable_chars(StringView str, StringView field_name);

    // Number validation
    static ValidationResult validate_positive(i64 value, StringView field_name);
    static ValidationResult validate_non_negative(i64 value, StringView field_name);
    static ValidationResult validate_range(i64 value, i64 min, i64 max, StringView field_name);

    // URL/Path validation
    static ValidationResult validate_url_pattern(StringView pattern);
    static ValidationResult validate_file_path(StringView path);
    static ValidationResult validate_safe_url_chars(StringView url, StringView field_name);

    // Hash validation
    static ValidationResult validate_sha256(StringView hash);
    static ValidationResult validate_hex_string(StringView str, size_t expected_length, StringView field_name);

    // Timestamp validation
    static ValidationResult validate_timestamp(i64 timestamp_ms);
    static ValidationResult validate_expiry(i64 expires_at_ms);
    static ValidationResult validate_timestamp_range(i64 timestamp_ms, i64 min_ms, i64 max_ms, StringView field_name);

    // Action validation
    static ValidationResult validate_action(StringView action);
    static ValidationResult validate_match_type(StringView match_type);

    // MIME type validation
    static ValidationResult validate_mime_type(StringView mime_type);

    // Configuration validation
    static ValidationResult validate_config_value(StringView key, JsonValue const& value);

    // IPC message validation
    static ValidationResult validate_quarantine_id(StringView id);
    static ValidationResult validate_rule_name(StringView name);

    // Helper to convert ValidationResult to ErrorOr
    static ErrorOr<void> to_error(ValidationResult const& result)
    {
        if (result.is_valid)
            return {};
        return Error::from_string_view(result.error_message.bytes_as_string_view());
    }
};

}
