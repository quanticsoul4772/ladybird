# Markdown Style Guide for Ladybird

Conventions for writing consistent, maintainable markdown documentation.

## File Naming

```
# Good
README.md
BuildInstructions.md
ProcessArchitecture.md
CONTRIBUTING.md

# Bad
readme.md               # Inconsistent capitalization
build-instructions.md   # Use PascalCase, not kebab-case
process_arch.md         # Avoid abbreviations
```

**Rules**:
- Use PascalCase for multi-word files
- Use UPPERCASE for special files (README, CONTRIBUTING, LICENSE)
- Use `.md` extension
- Prefer full words over abbreviations

## Document Structure

### Template

```markdown
# Document Title (H1 - Only One Per Document)

Brief introduction (1-2 paragraphs) explaining what this document covers.

## Section Heading (H2)

Content for the section.

### Subsection Heading (H3)

Subsection content.

#### Sub-subsection (H4)

Rarely needed. If you need H5 or H6, consider restructuring.
```

### Heading Guidelines

- **One H1 per document** - The title
- **H2 for major sections** - Main topics
- **H3 for subsections** - Subtopics
- **H4 sparingly** - Minor divisions
- **Avoid H5/H6** - Restructure instead

```markdown
# Right
# Main Title
## Section
### Subsection

# Wrong
# Main Title
# Another H1 (should be H2)
```

## Formatting

### Emphasis

```markdown
*Italic text* or _italic text_
**Bold text** or __bold text__
***Bold and italic*** or ___bold and italic___
~~Strikethrough text~~
`inline code`
```

**Consistency**: Pick one style and stick to it
- Recommended: `*italic*` and `**bold**`

### Lists

#### Unordered Lists

```markdown
- First item
- Second item
  - Nested item (2 spaces indent)
  - Another nested item
- Third item
```

#### Ordered Lists

```markdown
1. First item
2. Second item
   1. Nested item (3 spaces indent)
   2. Another nested item
3. Third item
```

#### Task Lists

```markdown
- [x] Completed task
- [ ] Incomplete task
- [ ] Another task
```

### Links

#### Internal Links (Relative Paths)

```markdown
# Same directory
[CodingStyle](CodingStyle.md)

# Parent directory
[README](../README.md)

# Specific heading
[Build Instructions](BuildInstructions.md#building-from-source)
```

#### External Links

```markdown
[Ladybird Project](https://ladybird.org)
[WHATWG HTML Spec](https://html.spec.whatwg.org/)
```

#### Reference-Style Links

```markdown
This is [a reference link][ref] and [another][ref2].

[ref]: https://example.com "Optional Title"
[ref2]: https://example.org
```

### Images

```markdown
# Inline
![Alt text](path/to/image.png)

# With title
![Alt text](path/to/image.png "Image title")

# Reference-style
![Alt text][image-ref]

[image-ref]: path/to/image.png "Title"
```

**Best Practices**:
- Always include alt text
- Use relative paths for local images
- Store images in `Images/` or `images/` directory
- Prefer SVG for diagrams (scales better)

## Code Blocks

### Inline Code

```markdown
Use `ErrorOr<T>` for fallible operations.
The `TRY()` macro propagates errors.
```

### Fenced Code Blocks

````markdown
```cpp
ErrorOr<void> example() {
    auto result = TRY(operation());
    return {};
}
```
````

### Language Specifiers

```markdown
```bash
./build.sh
```

```cpp
class Example {};
```

```javascript
console.log("Hello");
```

```json
{
    "key": "value"
}
```

```html
<div>Content</div>
```

```css
.class { color: red; }
```

```python
print("Hello")
```

```
No syntax highlighting
```
```

### Commands vs Output

````markdown
# Commands (use bash)
```bash
cmake --preset Release
cmake --build Build/release
```

# Output (use plaintext)
```
-- Configuring done
-- Generating done
-- Build files written to: Build/release
```
````

## Tables

### Basic Table

```markdown
| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Value 1  | Value 2  | Value 3  |
| Value 4  | Value 5  | Value 6  |
```

### Alignment

```markdown
| Left-aligned | Center-aligned | Right-aligned |
|:-------------|:--------------:|--------------:|
| Left         | Center         | Right         |
| Data         | More data      | Even more     |
```

### Complex Tables

```markdown
| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `validate` | `bool` | `true` | Enable validation |
| `max_size` | `size_t` | `0` | Max size (0 = unlimited) |
```

**Tips**:
- Don't manually align columns (use a formatter)
- Keep tables simple (complex data → use lists)
- Use `<br>` for line breaks in cells if needed

## Blockquotes

```markdown
> Single-line quote

> Multi-line quote
> continues here
> and here

> **Note**: Important information
> that spans multiple lines.
```

## Horizontal Rules

```markdown
---

***

___

<!-- Use sparingly - usually indicates need for new section -->
```

## Line Breaks

```markdown
# Soft break (no <br>)
Line 1
Line 2

# Hard break (with <br>)
Line 1
Line 2

# Paragraph break (blank line)
Paragraph 1

Paragraph 2
```

## Comments

```markdown
<!-- This is a comment -->

<!--
Multi-line
comment
-->

<!-- TODO: Fix this section -->
```

## Special Sections

### Notes and Warnings

```markdown
**Note**: Important information that doesn't fit inline.

**Warning**: Critical warnings about potential issues.

**Important**: Emphasize critical points.

**Tip**: Helpful suggestions.

> **Note for Contributors**:
> Multi-line notes can use blockquotes for emphasis.
```

### Code Examples with Context

```markdown
### Example: Basic Usage

```cpp
auto processor = TRY(Processor::create());
auto result = TRY(processor->process(input));
```

This example shows how to...
```

### Command Examples

```markdown
### Building

```bash
# Configure build
cmake --preset Release

# Build (using all CPU cores)
cmake --build Build/release -j$(nproc)

# Run
./Build/release/bin/Ladybird
```
```

## File Organization

### README.md Structure

```markdown
# Project Name

Brief description

## Quick Start

Minimal example to get running

## Installation

Detailed build instructions

## Usage

How to use the project

## Documentation

Links to detailed docs

## Contributing

How to contribute

## License

License information
```

### Technical Documentation Structure

```markdown
# Feature/Component Name

## Overview

What this is and why it exists

## Architecture

How it's structured

## Usage

How to use it

## API Reference

Detailed API docs (or link to them)

## Examples

Working code examples

## See Also

Related documentation
```

## Line Length

- **Limit to 120 characters** for readability
- Exception: Long URLs can exceed limit
- Use line breaks for readability

```markdown
# Good: Wrapped for readability
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore
et dolore magna aliqua. Ut enim ad minim veniam.

# Bad: One very long line that exceeds 120 characters and is hard to read in editors with limited horizontal space
```

## Cross-References

### Linking Between Docs

```markdown
See [Process Architecture](ProcessArchitecture.md) for details.

For API documentation, refer to [LibWeb API](../API/LibWeb.md).

Build instructions: [BuildInstructions.md](BuildInstructions.md#linux).
```

### Linking to Code

```markdown
Implementation: [`Libraries/LibWeb/HTML/HTMLElement.cpp`](../Libraries/LibWeb/HTML/HTMLElement.cpp)

See the [`HTMLElement` class](../Libraries/LibWeb/HTML/HTMLElement.h).
```

### Linking to Specs

```markdown
Implements: [HTML Standard §4.10.5](https://html.spec.whatwg.org/multipage/section.html)

Based on: [RFC 7230](https://tools.ietf.org/html/rfc7230)
```

## Version History

### Inline Version Info

```markdown
**Added in**: v1.2.0
**Deprecated in**: v2.0.0 (use `new_method()` instead)
**Removed in**: v3.0.0
```

### Changelog Section

```markdown
## Version History

### 1.2.0 (2025-11-01)
- Added feature X
- Fixed bug Y

### 1.1.0 (2025-10-01)
- Initial release
```

## Front Matter

For tools that support it (Jekyll, Hugo, etc.):

```markdown
---
title: Document Title
description: Brief description
date: 2025-11-01
author: Author Name
tags:
  - tag1
  - tag2
---

# Document Title

Content starts here...
```

## Common Mistakes

### ❌ Don't

```markdown
# Multiple H1 headings
# Title
# Another Title (should be ## Another Title)

# Mixing list markers
- Item 1
* Item 2 (inconsistent)

# Missing blank lines
## Heading
Content immediately after (missing blank line)

# Manual table alignment
| A    |    B |
|------|------|
| Long | Data | (don't manually align)

# Absolute paths for internal docs
[Doc](/home/user/project/doc.md) (use relative)

# Missing code language
```
code here
``` (specify language)
```

### ✅ Do

```markdown
# One H1 heading
# Title
## Section (consistent hierarchy)

# Consistent list markers
- Item 1
- Item 2

# Blank lines for readability
## Heading

Content with breathing room.

# Let formatter handle tables
| A | B |
|---|---|
| Long | Data |

# Relative paths
[Doc](../docs/doc.md)

# Specify language
```cpp
code here
```
```

## Tools

### Linters

- **markdownlint**: https://github.com/DavidAnson/markdownlint
- **markdown-link-check**: Check for broken links

### Formatters

- **Prettier**: Auto-format markdown
- **pandoc**: Convert between formats

### Editors

- **VS Code**: With markdownlint extension
- **Typora**: WYSIWYG markdown editor
- **Obsidian**: Knowledge base with markdown

## Checklist

Before committing markdown docs:

- [ ] One H1 heading
- [ ] Proper heading hierarchy (H1 → H2 → H3)
- [ ] All links work (relative paths for internal docs)
- [ ] All images have alt text
- [ ] Code blocks have language specifiers
- [ ] Consistent list markers
- [ ] Proper line breaks and spacing
- [ ] Tables are readable
- [ ] No trailing whitespace
- [ ] Spell-checked

## Resources

- **CommonMark**: https://commonmark.org/ (standard spec)
- **GitHub Flavored Markdown**: https://github.github.com/gfm/
- **Markdown Guide**: https://www.markdownguide.org/
