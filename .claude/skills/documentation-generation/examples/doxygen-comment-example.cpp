/*
 * Copyright (c) 2025, Example Author <author@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Vector.h>

namespace Example {

/**
 * @class DocumentProcessor
 * @brief Processes and validates documents according to configurable rules
 *
 * The DocumentProcessor provides a high-level interface for processing various
 * document types. It validates structure, applies transformations, and ensures
 * compliance with configured policies.
 *
 * Thread Safety: This class is not thread-safe. Create separate instances for
 * concurrent processing.
 *
 * @see DocumentValidator
 * @see TransformationRule
 */
class DocumentProcessor {
public:
    /**
     * @brief Create a new DocumentProcessor instance
     *
     * Initializes the processor with default configuration. Call configure()
     * to customize behavior before processing documents.
     *
     * @return DocumentProcessor instance or error if initialization fails
     *
     * @note This method never returns nullptr - check ErrorOr for errors
     */
    static ErrorOr<NonnullOwnPtr<DocumentProcessor>> create();

    /**
     * @brief Process a document with validation and transformation
     *
     * Performs the following steps:
     * 1. Validates document structure
     * 2. Applies configured transformation rules
     * 3. Generates processed output
     *
     * The original document is not modified. Processing errors are reported
     * via the ErrorOr return type.
     *
     * @param document_content Raw document content to process
     * @param options Processing options (validation level, transform rules)
     * @return Processed document content or error
     *
     * @throws EINVAL if document_content is empty
     * @throws ENOENT if required transformation rule is missing
     * @throws ENOMEM if allocation fails during processing
     *
     * Example usage:
     * @code
     * auto processor = TRY(DocumentProcessor::create());
     * ProcessingOptions options { .validate = true };
     * auto result = TRY(processor->process(content, options));
     * dbgln("Processed: {}", result);
     * @endcode
     *
     * @see ProcessingOptions
     * @see validate()
     */
    ErrorOr<String> process(StringView document_content, ProcessingOptions const& options);

    /**
     * @brief Validate document without processing
     *
     * Performs structural validation only, without applying transformations.
     * Useful for checking document validity before committing to expensive
     * processing operations.
     *
     * @param document_content Document to validate
     * @return void on success, error describing validation failure
     *
     * @warning This is a lightweight check. Full validation occurs during process()
     */
    ErrorOr<void> validate(StringView document_content) const;

    /**
     * @brief Add a custom transformation rule
     *
     * Rules are applied in the order they are added. If a rule with the same
     * name already exists, it will be replaced.
     *
     * @param rule Transformation rule to add (moved into processor)
     *
     * @note Rule execution order matters for dependent transformations
     * @see TransformationRule
     */
    void add_transformation_rule(NonnullOwnPtr<TransformationRule> rule);

    /**
     * @brief Get the number of processed documents
     * @return Count of documents successfully processed by this instance
     */
    size_t processed_count() const { return m_processed_count; }

    /**
     * @brief Check if strict validation is enabled
     * @return true if strict validation is enabled
     */
    bool strict_validation() const { return m_strict_validation; }

    /**
     * @brief Enable or disable strict validation mode
     *
     * Strict validation enforces additional constraints:
     * - No empty sections
     * - All references must be resolvable
     * - Mandatory metadata fields must be present
     *
     * @param enabled Whether to enable strict validation
     *
     * @note Strict mode significantly increases processing time
     */
    void set_strict_validation(bool enabled) { m_strict_validation = enabled; }

private:
    DocumentProcessor() = default;

    // Internal validation helper
    ErrorOr<void> validate_structure(StringView content) const;

    // Apply all transformation rules in order
    ErrorOr<String> apply_transformations(String const& content) const;

    Vector<NonnullOwnPtr<TransformationRule>> m_rules;
    size_t m_processed_count { 0 };
    bool m_strict_validation { false };
};

/**
 * @struct ProcessingOptions
 * @brief Configuration options for document processing
 *
 * Provides fine-grained control over document processing behavior.
 * All fields have sensible defaults.
 */
struct ProcessingOptions {
    /** Enable structural validation before processing */
    bool validate { true };

    /** Maximum document size in bytes (0 = unlimited) */
    size_t max_size { 0 };

    /** Skip transformation rules (validation only) */
    bool skip_transformations { false };

    /** Preserve original formatting where possible */
    bool preserve_formatting { false };
};

/**
 * @brief Convert processing result to human-readable format
 *
 * Utility function for debugging and logging. Not intended for production
 * serialization - use a proper serialization library instead.
 *
 * @param result Processing result to format
 * @return Human-readable string representation
 *
 * @note Output format is not stable across versions
 */
String format_processing_result(ProcessingResult const& result);

/**
 * @brief Check if a document is valid without creating a processor
 *
 * Convenience function for quick validation. For repeated validations,
 * create a DocumentProcessor instance for better performance.
 *
 * @param content Document content to validate
 * @return true if document is structurally valid
 *
 * @deprecated Use DocumentProcessor::validate() for detailed error messages
 */
[[deprecated("Use DocumentProcessor::validate() instead")]]
bool is_valid_document(StringView content);

} // namespace Example
