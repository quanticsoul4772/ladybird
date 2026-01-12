# LibFoo API Reference

## Overview

LibFoo provides a high-performance document processing and transformation library for Ladybird. It handles validation, parsing, and conversion of various document formats with a focus on safety and spec compliance.

**Version**: 1.0.0
**Namespace**: `Foo`
**Include**: `<LibFoo/Foo.h>`

## Core Concepts

### Processing Pipeline

Documents flow through a three-stage pipeline:

1. **Validation**: Structural and semantic validation
2. **Transformation**: Rule-based document transformation
3. **Serialization**: Output in desired format

### Error Handling

All fallible operations return `ErrorOr<T>`. Use the `TRY()` macro for error propagation:

```cpp
auto result = TRY(processor->process(content));
```

Common errors:
- `EINVAL` - Invalid input data
- `ENOMEM` - Memory allocation failure
- `ENOENT` - Required resource not found

## Classes

### DocumentProcessor

Main class for document processing operations.

**Include**: `<LibFoo/DocumentProcessor.h>`
**Namespace**: `Foo`
**Header**: Public API

#### Construction

##### `create()`

```cpp
static ErrorOr<NonnullOwnPtr<DocumentProcessor>> create();
```

Creates a new DocumentProcessor instance with default configuration.

**Returns**:
- `ErrorOr<NonnullOwnPtr<DocumentProcessor>>` - Processor instance or error

**Errors**:
- `ENOMEM` - Out of memory

**Example**:
```cpp
#include <LibFoo/DocumentProcessor.h>

ErrorOr<void> example()
{
    auto processor = TRY(Foo::DocumentProcessor::create());
    // Use processor...
    return {};
}
```

---

#### Processing Methods

##### `process()`

```cpp
ErrorOr<String> process(StringView document_content,
                        ProcessingOptions const& options);
```

Processes a document through the complete pipeline: validation, transformation, and serialization.

**Parameters**:
- `document_content` - Raw document content to process (must not be empty)
- `options` - Processing options controlling behavior

**Returns**:
- `ErrorOr<String>` - Processed document or error

**Errors**:
- `EINVAL` - document_content is empty or invalid
- `ENOENT` - Required transformation rule missing
- `ENOMEM` - Allocation failure during processing

**Thread Safety**: Not thread-safe. Use separate instances for concurrent processing.

**Example**:
```cpp
Foo::ProcessingOptions options {
    .validate = true,
    .skip_transformations = false,
    .preserve_formatting = true
};

auto result = TRY(processor->process(content, options));
dbgln("Processed {} bytes", result.length());
```

---

##### `validate()`

```cpp
ErrorOr<void> validate(StringView document_content) const;
```

Validates document structure without performing transformations. Useful for pre-flight checks before expensive processing.

**Parameters**:
- `document_content` - Document to validate

**Returns**:
- `ErrorOr<void>` - Success or validation error

**Errors**:
- `EINVAL` - Validation failed (check error message for details)

**Note**: This is a lightweight check. Full validation occurs during `process()`.

**Example**:
```cpp
// Quick validation before processing
if (auto result = processor->validate(content); result.is_error()) {
    dbgln("Validation failed: {}", result.error());
    return result.error();
}

// Proceed with processing
auto processed = TRY(processor->process(content, options));
```

---

#### Configuration Methods

##### `add_transformation_rule()`

```cpp
void add_transformation_rule(NonnullOwnPtr<TransformationRule> rule);
```

Adds a custom transformation rule to the processing pipeline. Rules are applied in the order they are added.

**Parameters**:
- `rule` - Transformation rule (ownership transferred to processor)

**Notes**:
- Rules are executed in registration order
- If a rule with the same name exists, it will be replaced
- Rule execution order matters for dependent transformations

**Example**:
```cpp
// Create custom rule
auto rule = TRY(CustomTransformationRule::create("uppercase"));
processor->add_transformation_rule(move(rule));

// Process will now apply the rule
auto result = TRY(processor->process(content, options));
```

---

##### `set_strict_validation()`

```cpp
void set_strict_validation(bool enabled);
```

Enables or disables strict validation mode.

**Parameters**:
- `enabled` - Whether to enable strict validation

**Strict Mode Constraints**:
- No empty sections allowed
- All references must be resolvable
- Mandatory metadata fields must be present
- Schema validation enforced

**Performance Impact**: Strict mode significantly increases processing time (2-3x slower).

**Example**:
```cpp
processor->set_strict_validation(true);
auto result = processor->process(content, options); // Full validation
```

---

#### Query Methods

##### `processed_count()`

```cpp
size_t processed_count() const;
```

Returns the number of documents successfully processed by this instance.

**Returns**:
- `size_t` - Count of processed documents

**Example**:
```cpp
dbgln("Processor has handled {} documents", processor->processed_count());
```

---

##### `strict_validation()`

```cpp
bool strict_validation() const;
```

Checks whether strict validation mode is enabled.

**Returns**:
- `bool` - true if strict validation is enabled

---

### TransformationRule

Abstract base class for document transformation rules.

**Include**: `<LibFoo/TransformationRule.h>`
**Namespace**: `Foo`

#### Methods

##### `name()`

```cpp
virtual String name() const = 0;
```

Returns the unique name of this transformation rule.

**Returns**:
- `String` - Rule name

---

##### `apply()`

```cpp
virtual ErrorOr<String> apply(String const& input) const = 0;
```

Applies the transformation to input content.

**Parameters**:
- `input` - Content to transform

**Returns**:
- `ErrorOr<String>` - Transformed content or error

---

#### Creating Custom Rules

Subclass `TransformationRule` to create custom transformations:

```cpp
class UppercaseRule : public Foo::TransformationRule {
public:
    static ErrorOr<NonnullOwnPtr<UppercaseRule>> create()
    {
        return adopt_nonnull_own_or_enomem(new (nothrow) UppercaseRule());
    }

    virtual String name() const override
    {
        return "uppercase"_string;
    }

    virtual ErrorOr<String> apply(String const& input) const override
    {
        return TRY(input.to_uppercase());
    }

private:
    UppercaseRule() = default;
};
```

---

## Data Structures

### ProcessingOptions

Configuration structure for document processing.

**Include**: `<LibFoo/ProcessingOptions.h>`
**Type**: `struct` (POD)

#### Fields

```cpp
struct ProcessingOptions {
    bool validate { true };
    size_t max_size { 0 };
    bool skip_transformations { false };
    bool preserve_formatting { false };
};
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `validate` | `bool` | `true` | Enable structural validation before processing |
| `max_size` | `size_t` | `0` | Maximum document size in bytes (0 = unlimited) |
| `skip_transformations` | `bool` | `false` | Skip transformation rules (validation only) |
| `preserve_formatting` | `bool` | `false` | Preserve original formatting where possible |

**Example**:
```cpp
Foo::ProcessingOptions options {
    .validate = true,
    .max_size = 1024 * 1024, // 1MB limit
    .skip_transformations = false,
    .preserve_formatting = true
};
```

---

### ProcessingResult

Represents the result of document processing.

**Include**: `<LibFoo/ProcessingResult.h>`
**Type**: `struct`

#### Fields

```cpp
struct ProcessingResult {
    String content;
    size_t rules_applied { 0 };
    Vector<String> warnings;
    Duration processing_time;
};
```

| Field | Type | Description |
|-------|------|-------------|
| `content` | `String` | Processed document content |
| `rules_applied` | `size_t` | Number of transformation rules applied |
| `warnings` | `Vector<String>` | Non-fatal warnings during processing |
| `processing_time` | `Duration` | Time spent processing |

---

## Utility Functions

### `format_processing_result()`

```cpp
String format_processing_result(ProcessingResult const& result);
```

Converts processing result to human-readable format for debugging.

**Parameters**:
- `result` - Processing result to format

**Returns**:
- `String` - Human-readable representation

**Note**: Output format is not stable across versions. Do not parse for production use.

**Example**:
```cpp
auto result = TRY(processor->process(content, options));
dbgln("{}", format_processing_result(result));
// Output: "Processed 1024 bytes, 3 rules applied, 0 warnings in 15ms"
```

---

### `is_valid_document()` (Deprecated)

```cpp
[[deprecated("Use DocumentProcessor::validate() instead")]]
bool is_valid_document(StringView content);
```

Quick validation without detailed error messages.

**Parameters**:
- `content` - Document to validate

**Returns**:
- `bool` - true if valid

**Deprecated**: Use `DocumentProcessor::validate()` for detailed error messages.

---

## Complete Example

```cpp
#include <LibFoo/DocumentProcessor.h>
#include <LibFoo/TransformationRule.h>

ErrorOr<int> process_documents(Vector<String> const& documents)
{
    // Create processor
    auto processor = TRY(Foo::DocumentProcessor::create());

    // Configure strict validation
    processor->set_strict_validation(true);

    // Add custom transformation rule
    auto uppercase_rule = TRY(UppercaseRule::create());
    processor->add_transformation_rule(move(uppercase_rule));

    // Configure options
    Foo::ProcessingOptions options {
        .validate = true,
        .max_size = 10 * 1024 * 1024, // 10MB
        .preserve_formatting = false
    };

    // Process each document
    for (auto const& doc : documents) {
        // Validate first
        if (auto result = processor->validate(doc); result.is_error()) {
            warnln("Skipping invalid document: {}", result.error());
            continue;
        }

        // Process
        auto processed = TRY(processor->process(doc, options));
        outln("Processed: {} bytes", processed.length());
    }

    outln("Total documents processed: {}", processor->processed_count());
    return 0;
}
```

---

## Error Reference

| Error Code | Constant | Common Causes | Solutions |
|------------|----------|---------------|-----------|
| `EINVAL` | Invalid argument | Empty input, malformed document | Check input validation |
| `ENOMEM` | Out of memory | Large document, many rules | Reduce document size, limit rules |
| `ENOENT` | Not found | Missing transformation rule | Register required rules |
| `EOVERFLOW` | Overflow | Document exceeds max_size | Increase max_size or reduce input |

---

## Performance Considerations

### Memory Usage

- **DocumentProcessor**: ~1KB base overhead
- **TransformationRule**: ~100 bytes per rule
- **Processing**: Temporary allocations proportional to document size

### Processing Speed

Typical performance (Intel i7, single-threaded):

| Document Size | Validation Only | With Transformations | Strict Mode |
|---------------|-----------------|---------------------|-------------|
| 1 KB          | ~0.1ms          | ~0.5ms              | ~1ms        |
| 100 KB        | ~2ms            | ~10ms               | ~25ms       |
| 1 MB          | ~20ms           | ~100ms              | ~250ms      |

**Optimization Tips**:
- Reuse `DocumentProcessor` instances (avoid repeated `create()` calls)
- Disable strict validation for trusted input
- Use `validate()` to fail fast on invalid documents
- Minimize number of transformation rules

---

## Thread Safety

**Not thread-safe**: `DocumentProcessor` is not safe for concurrent access. Use one of:

1. **Separate instances per thread**:
```cpp
// Each thread gets its own processor
auto processor = TRY(Foo::DocumentProcessor::create());
```

2. **Mutex protection** (not recommended for performance):
```cpp
static Mutex s_processor_mutex;
Locker locker(s_processor_mutex);
auto result = TRY(processor->process(content, options));
```

---

## Version History

### 1.0.0 (2025-11-01)
- Initial stable release
- Core processing pipeline
- Transformation rule system
- Validation support

---

## See Also

- [LibFoo Tutorial](tutorials/libfoo-tutorial.md) - Step-by-step introduction
- [Transformation Rules Guide](guides/transformation-rules.md) - Writing custom rules
- [Migration Guide](guides/migration-v1.md) - Upgrading from v0.x
- [LibBar API Reference](LibBar.md) - Related library for serialization
