# Doxygen Reference

Quick reference for Doxygen documentation commands and configuration.

## Common Doxygen Tags

### Basic Documentation

```cpp
/**
 * @brief One-line brief description
 *
 * Detailed multi-line description.
 * Can span multiple paragraphs.
 */
```

### Function Documentation

```cpp
/**
 * @brief Brief description
 * @param param_name Description of parameter
 * @param[in] input_param Input-only parameter
 * @param[out] output_param Output-only parameter
 * @param[in,out] inout_param Input and output parameter
 * @return Description of return value
 * @retval value Description of specific return value
 * @throws ExceptionType When this exception is thrown
 */
```

### Class Documentation

```cpp
/**
 * @class ClassName
 * @brief Brief description
 *
 * @details Detailed description
 * @see RelatedClass
 * @note Additional notes
 * @warning Important warnings
 */
```

### Sections and Structure

```cpp
/**
 * @section section_name Section Title
 * Content of the section.
 *
 * @subsection subsection_name Subsection Title
 * Content of the subsection.
 */
```

### Code Examples

```cpp
/**
 * Example usage:
 * @code
 * auto instance = TRY(ClassName::create());
 * auto result = TRY(instance->method());
 * @endcode
 */
```

### Lists

```cpp
/**
 * Features:
 * - Feature 1
 * - Feature 2
 * - Feature 3
 *
 * Or numbered:
 * -# Step 1
 * -# Step 2
 * -# Step 3
 */
```

### Cross-References

```cpp
/**
 * @see other_function() Related function
 * @see ClassName Related class
 * @see https://example.com External reference
 * @ref section_name Reference to section
 */
```

### Special Tags

```cpp
/**
 * @todo Describe what needs to be done
 * @bug Describe known bug
 * @deprecated Use new_function() instead
 * @since Version 1.2.0
 * @version 1.0.0
 * @author Author Name
 * @date 2025-11-01
 */
```

## Doxygen Configuration

### Minimal Doxyfile

```
PROJECT_NAME           = "Project Name"
OUTPUT_DIRECTORY       = docs/
INPUT                  = src/ include/
RECURSIVE              = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
EXTRACT_ALL            = YES
```

### Recommended Settings

```
# Project info
PROJECT_NAME           = "Ladybird Browser"
PROJECT_BRIEF          = "Independent web browser"
PROJECT_VERSION        = 1.0.0

# Input
INPUT                  = Libraries/ Services/
FILE_PATTERNS          = *.h *.cpp
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */Tests/* */Build/*

# Output
OUTPUT_DIRECTORY       = docs/api
GENERATE_HTML          = YES
HTML_OUTPUT            = html
HTML_TIMESTAMP         = YES

# Extraction
EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES

# Warnings
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_DOC_ERROR      = YES

# Source browsing
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO

# Diagrams (requires Graphviz)
HAVE_DOT               = YES
CLASS_DIAGRAMS         = YES
CLASS_GRAPH            = YES
COLLABORATION_GRAPH    = YES
CALL_GRAPH             = NO
CALLER_GRAPH           = NO

# Search
SEARCHENGINE           = YES
```

### Output Formats

```
# HTML output
GENERATE_HTML          = YES
HTML_OUTPUT            = html
HTML_FILE_EXTENSION    = .html

# LaTeX output (for PDF)
GENERATE_LATEX         = YES
LATEX_OUTPUT           = latex

# XML output (for post-processing)
GENERATE_XML           = YES
XML_OUTPUT             = xml

# Man pages
GENERATE_MAN           = YES
MAN_OUTPUT             = man
```

### Diagram Configuration

```
# Graphviz/dot diagrams
HAVE_DOT               = YES
DOT_NUM_THREADS        = 4
DOT_FONTNAME           = Helvetica
DOT_FONTSIZE           = 10

# Graph types
CLASS_DIAGRAMS         = YES
CLASS_GRAPH            = YES
COLLABORATION_GRAPH    = YES
GROUP_GRAPHS           = YES
INCLUDE_GRAPH          = YES
INCLUDED_BY_GRAPH      = YES
CALL_GRAPH             = NO    # Can be very large
CALLER_GRAPH           = NO    # Can be very large
GRAPHICAL_HIERARCHY    = YES
DIRECTORY_GRAPH        = YES
```

### Preprocessing

```
# Preprocessor
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
EXPAND_ONLY_PREDEF     = NO

# Predefined macros
PREDEFINED             = __attribute__(x)= \
                         ALWAYS_INLINE= \
                         EXPORT=
```

## Command-Line Usage

### Generate Documentation

```bash
# Using Doxyfile in current directory
doxygen

# Using specific Doxyfile
doxygen path/to/Doxyfile

# Generate default Doxyfile
doxygen -g

# Generate with custom name
doxygen -g MyDoxyfile
```

### Useful Options

```bash
# Print version
doxygen --version

# Update existing Doxyfile
doxygen -u Doxyfile

# Check configuration
doxygen -x Doxyfile
```

## Special Comment Styles

### C++ Style

```cpp
/// Brief description
/// Detailed description continues
/// on multiple lines.
```

### JavaDoc Style

```cpp
/**
 * JavaDoc-style comment
 * @param x Parameter
 */
```

### Qt Style

```cpp
/*!
 * Qt-style comment
 * \param x Parameter
 */
```

### After-Member Documentation

```cpp
int member;  ///< Brief description after member
int other;   //!< Alternative style
```

## Grouping and Modules

### Define Groups

```cpp
/**
 * @defgroup core_group Core Components
 * @brief Core functionality
 * @{
 */

// Members of the group...

/** @} */ // End of core_group
```

### Add to Groups

```cpp
/**
 * @ingroup core_group
 * @brief Function in core group
 */
void function();
```

### Modules

```cpp
/**
 * @addtogroup module_name Module Name
 * @{
 */

// Module content

/** @} */
```

## File Documentation

```cpp
/**
 * @file FileName.h
 * @brief Brief description of file
 * @author Author Name
 * @date 2025-11-01
 *
 * Detailed description of what this file contains.
 */
```

## Namespace Documentation

```cpp
/**
 * @namespace MyNamespace
 * @brief Brief description
 *
 * Detailed description of namespace purpose.
 */
namespace MyNamespace {
```

## Enum Documentation

```cpp
/**
 * @enum EnumName
 * @brief Brief description
 */
enum class EnumName {
    Value1,  ///< Description of Value1
    Value2,  ///< Description of Value2
    Value3   ///< Description of Value3
};
```

## Markdown in Doxygen

Doxygen supports Markdown formatting:

```cpp
/**
 * # Heading 1
 * ## Heading 2
 *
 * **Bold text** and *italic text*
 *
 * - List item 1
 * - List item 2
 *
 * 1. Numbered item
 * 2. Another item
 *
 * `inline code`
 *
 * ```cpp
 * // Code block
 * auto x = 42;
 * ```
 *
 * [Link text](https://example.com)
 *
 * | Column 1 | Column 2 |
 * |----------|----------|
 * | Cell 1   | Cell 2   |
 */
```

## Formula Support

### Inline Formulas

```cpp
/**
 * The formula @f$ x = \frac{-b \pm \sqrt{b^2-4ac}}{2a} @f$ solves...
 */
```

### Block Formulas

```cpp
/**
 * The Pythagorean theorem:
 * @f[
 *   a^2 + b^2 = c^2
 * @f]
 */
```

## Best Practices

1. **Brief descriptions**: One sentence, no period needed
2. **Detailed descriptions**: Multiple paragraphs are fine
3. **Parameter descriptions**: Be specific about constraints
4. **Return values**: Describe both success and error cases
5. **Examples**: Show realistic usage
6. **Cross-references**: Link to related functions/classes
7. **Warnings**: Highlight important constraints
8. **Thread safety**: Document concurrent access behavior

## Common Pitfalls

### Don't Over-Document

```cpp
// Bad: Obvious documentation
/**
 * @brief Get the count
 * @return The count
 */
int count() const { return m_count; }

// Good: No documentation needed
int count() const { return m_count; }
```

### Do Document Complex Behavior

```cpp
// Good: Complex behavior needs explanation
/**
 * @brief Calculate optimal cache size
 *
 * Uses heuristics based on available memory and expected
 * workload. The algorithm favors smaller caches on systems
 * with < 4GB RAM.
 *
 * @return Recommended cache size in bytes
 */
size_t calculate_cache_size() const;
```

## Resources

- **Official Website**: https://www.doxygen.nl/
- **Manual**: https://www.doxygen.nl/manual/
- **Download**: https://www.doxygen.nl/download.html
- **Markdown Support**: https://www.doxygen.nl/manual/markdown.html
