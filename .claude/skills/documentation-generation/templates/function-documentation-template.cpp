/*
 * Copyright (c) YEAR, Author Name <email@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/StringView.h>

namespace YourNamespace {

// ============================================================================
// SIMPLE FUNCTIONS (minimal documentation needed)
// ============================================================================

/**
 * @brief Convert string to uppercase
 * @param input String to convert
 * @return Uppercase version of input
 */
String to_uppercase(StringView input);

/**
 * @brief Check if string is empty
 * @param str String to check
 * @return true if string has zero length
 */
bool is_empty(StringView str);

// ============================================================================
// STANDARD FUNCTIONS (moderate documentation)
// ============================================================================

/**
 * @brief Parse integer from string with validation
 *
 * Converts a string representation of an integer to its numeric value.
 * Supports decimal notation only. Leading/trailing whitespace is ignored.
 *
 * @param str String to parse (e.g., "123", " -456 ")
 * @return Parsed integer value or error
 *
 * @throws EINVAL if string contains non-numeric characters
 * @throws ERANGE if value exceeds integer range
 *
 * Example:
 * @code
 * auto value = TRY(parse_integer("42"));  // value == 42
 * @endcode
 */
ErrorOr<int> parse_integer(StringView str);

/**
 * @brief Calculate hash of data using specified algorithm
 *
 * @param data Data to hash
 * @param algorithm Hash algorithm to use (SHA256, SHA512, etc.)
 * @return Hash digest as byte array
 *
 * @throws EINVAL if algorithm is not supported
 * @throws ENOMEM if allocation fails
 *
 * @note For security-critical hashing, use HMAC variants
 * @see hmac_sha256() for authenticated hashing
 */
ErrorOr<ByteBuffer> calculate_hash(ReadonlyBytes data, HashAlgorithm algorithm);

// ============================================================================
// COMPLEX FUNCTIONS (extensive documentation)
// ============================================================================

/**
 * @brief Process document through multi-stage pipeline
 *
 * Performs comprehensive document processing including:
 * 1. Validation - Structural and semantic checks
 * 2. Normalization - Convert to canonical form
 * 3. Transformation - Apply configured rules
 * 4. Serialization - Generate output format
 *
 * The processing pipeline maintains transactional semantics: if any stage
 * fails, no partial results are returned and the original document is
 * unchanged.
 *
 * @param document Input document to process (must be valid)
 * @param rules Transformation rules to apply (applied in order)
 * @param options Processing options controlling behavior
 * @param progress Optional callback for progress reporting (nullable)
 * @return Processed document or error
 *
 * @throws EINVAL if document is invalid or empty
 * @throws ENOENT if required transformation rule is missing
 * @throws ENOMEM if allocation fails during processing
 * @throws ECANCELED if processing is cancelled via progress callback
 *
 * Performance Characteristics:
 * - Time: O(n * m) where n=document_size, m=rule_count
 * - Space: O(n) temporary allocations
 *
 * Thread Safety: This function is thread-safe and reentrant
 *
 * Example usage:
 * @code
 * Vector<TransformationRule> rules;
 * rules.append(uppercase_rule);
 * rules.append(trim_rule);
 *
 * ProcessingOptions options {
 *     .validate = true,
 *     .max_size = 1024 * 1024
 * };
 *
 * auto result = TRY(process_document(
 *     document,
 *     rules,
 *     options,
 *     [](size_t current, size_t total) {
 *         dbgln("Progress: {}%", current * 100 / total);
 *         return true; // Continue processing
 *     }
 * ));
 * @endcode
 *
 * @warning Large documents (>10MB) may cause significant memory usage
 * @note Progress callback is called approximately every 1000 operations
 *
 * @see TransformationRule for creating custom rules
 * @see ProcessingOptions for configuration details
 */
ErrorOr<ProcessedDocument> process_document(
    Document const& document,
    Vector<TransformationRule> const& rules,
    ProcessingOptions const& options,
    Function<bool(size_t current, size_t total)> progress = nullptr);

// ============================================================================
// SPEC ALGORITHM IMPLEMENTATION
// ============================================================================

/**
 * @brief Implement spec algorithm name
 *
 * Implements the algorithm defined in:
 * https://spec.example.org/multipage/section.html#algorithm-name
 *
 * Spec text (quote relevant parts):
 * > "The user agent must perform the following steps..."
 *
 * Algorithm steps (match spec exactly):
 * 1. If condition is true, return early
 * 2. Let variable be the result of operation
 * 3. For each item in collection:
 *    3.1. Process item
 *    3.2. Append to results
 * 4. Return results
 *
 * @param input Input value as defined in spec
 * @return Result as specified in algorithm
 *
 * Spec conformance:
 * - Passes WPT test: test-name.html
 * - Known issues: [describe any spec ambiguities or deviations]
 */
ErrorOr<SpecResult> spec_algorithm_name(SpecInput const& input);

// ============================================================================
// DEPRECATED FUNCTIONS
// ============================================================================

/**
 * @brief Old API for backwards compatibility
 *
 * @param param Old parameter format
 * @return Result in legacy format
 *
 * @deprecated Use new_function_name() instead (since v1.5)
 * @warning This function will be removed in v2.0
 *
 * Migration guide:
 * @code
 * // Old code:
 * auto result = old_function(param);
 *
 * // New code:
 * auto result = new_function_name(NewParam { param });
 * @endcode
 */
[[deprecated("Use new_function_name() instead")]]
Result old_function(OldParam param);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Format bytes as human-readable size (e.g., "1.5 MB")
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 MB", "512 KB")
 */
String format_byte_size(size_t bytes);

/**
 * @brief Clamp value to range [min, max]
 * @param value Value to clamp
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Clamped value within [min, max]
 */
template<typename T>
T clamp(T value, T min, T max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

} // namespace YourNamespace
