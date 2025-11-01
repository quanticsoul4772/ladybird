# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Multi-threaded document processing support with thread pool
- New `ProcessingOptions::max_threads` configuration option
- Batch processing API for handling multiple documents efficiently
- Performance metrics collection and reporting

### Changed
- Improved validation error messages with line and column information
- Updated TransformationRule interface to support async operations
- Optimized memory usage for large documents (30% reduction)

### Deprecated
- `is_valid_document()` function in favor of `DocumentProcessor::validate()`
- Legacy `ProcessingOptions::legacy_mode` flag (will be removed in 2.0)

### Fixed
- Memory leak in transformation rule chain when processing fails
- Race condition in statistics collection (introduced in 1.3.0)
- Incorrect error codes for validation failures

## [1.3.0] - 2025-10-15

### Added
- Strict validation mode with enhanced constraint checking
- Custom transformation rule API for user-defined transformations
- Processing statistics and metrics (count, timing, errors)
- `format_processing_result()` utility function

### Changed
- Refactored validation pipeline for better performance
- Improved error messages with context and suggestions
- Updated documentation with comprehensive examples

### Fixed
- Crash when processing empty documents
- Incorrect handling of UTF-8 BOM markers
- Memory corruption in transformation rule cleanup

## [1.2.1] - 2025-09-30

### Fixed
- Critical security issue in document validation (CVE-2025-XXXX)
- Buffer overflow when processing malformed headers
- Integer overflow in size calculations

### Security
- Added bounds checking for all size operations
- Implemented safe integer arithmetic using SafeMath
- Enhanced input validation for untrusted documents

## [1.2.0] - 2025-09-20

### Added
- Support for streaming document processing
- Incremental validation for large documents
- Progress callbacks during long operations
- New `ProcessingOptions::max_size` field

### Changed
- Default timeout increased from 30s to 60s
- Error handling now provides more context
- Memory allocation strategy optimized for large documents

### Deprecated
- `process_sync()` method (use `process()` instead)

### Removed
- Support for legacy format v1 (deprecated in 1.0.0)

### Fixed
- Deadlock in concurrent processing scenarios
- Resource leak when operations are cancelled

## [1.1.0] - 2025-08-10

### Added
- New transformation rules: uppercase, lowercase, trim
- Support for custom validation schemas
- Document metadata extraction API
- Colorized error output in verbose mode

### Changed
- Validation is now performed in two passes for better accuracy
- Improved performance of regex-based transformations (3x faster)
- Error messages now include suggestions for common mistakes

### Fixed
- Incorrect line number reporting in validation errors
- Memory leak in error path of transformation pipeline
- Off-by-one error in size limit checking

## [1.0.1] - 2025-07-25

### Fixed
- Crash on macOS when processing documents with special characters
- Build failure on Windows with MSVC
- Documentation links broken after repository restructure

## [1.0.0] - 2025-07-15

### Added
- Initial stable release
- Core document processing pipeline
- Validation engine with structural and semantic checks
- Transformation rule system
- Comprehensive error handling with ErrorOr
- Full test suite with >95% coverage
- Complete API documentation

### Changed
- API finalized and frozen for 1.x series
- Performance improvements across the board
- Consistent naming conventions throughout codebase

### Breaking Changes from 0.x
- `DocumentProcessor::create()` now returns `ErrorOr` (was nullable)
- `ProcessingOptions` is now a struct (was class with builder)
- Removed deprecated `process_unsafe()` method
- Renamed `validate_strict()` to `set_strict_validation()`

### Migration Guide
See [docs/migration-1.0.md](docs/migration-1.0.md) for upgrading from 0.x.

## [0.9.0] - 2025-06-01 (Beta)

### Added
- Beta release with feature freeze
- Complete API coverage
- Performance benchmarks

### Changed
- Finalized API signatures (no more breaking changes before 1.0)
- Improved documentation coverage

### Fixed
- All known critical bugs resolved

## [0.8.0] - 2025-05-15 (Alpha)

### Added
- Alpha release for early testing
- Core functionality complete
- Basic documentation

### Known Issues
- Performance not yet optimized
- Some edge cases not handled
- Documentation incomplete

## [0.1.0] - 2025-04-01 (Initial Development)

### Added
- Initial prototype
- Basic document processing
- Proof of concept validation

---

## Release Notes Guidelines

### Version Numbering

We follow [Semantic Versioning](https://semver.org/):

- **MAJOR** version: Incompatible API changes
- **MINOR** version: Backwards-compatible new features
- **PATCH** version: Backwards-compatible bug fixes

### Change Categories

- **Added**: New features
- **Changed**: Changes in existing functionality
- **Deprecated**: Soon-to-be removed features
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Security vulnerability fixes

### Writing Change Entries

**Good entries** (specific and actionable):
```markdown
- Fixed memory leak in transformation rule chain when processing fails (#123)
- Added support for streaming processing with new StreamProcessor class
- Deprecated is_valid_document() in favor of DocumentProcessor::validate()
```

**Bad entries** (vague or incomplete):
```markdown
- Fixed bugs
- Improved performance
- Updated documentation
```

### Breaking Changes

Always highlight breaking changes:

```markdown
### Breaking Changes

- **API**: Removed deprecated process_unsafe() method
  - Migration: Use process() with appropriate error handling
  - Impact: Code using process_unsafe() will fail to compile

- **Behavior**: Strict validation is now enabled by default
  - Migration: Disable with set_strict_validation(false)
  - Impact: Previously valid documents may now fail validation
```

### Security Fixes

Format security entries clearly:

```markdown
### Security

- **CVE-2025-XXXX**: Fixed buffer overflow in document parsing
  - Severity: High (CVSS 8.1)
  - Impact: Remote code execution with malformed input
  - Credit: Security Researcher Name (@username)
  - Mitigation: Upgrade to 1.2.1 immediately
```

---

## Comparison Links

[Unreleased]: https://github.com/username/project/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/username/project/compare/v1.2.1...v1.3.0
[1.2.1]: https://github.com/username/project/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/username/project/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/username/project/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/username/project/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/username/project/compare/v0.9.0...v1.0.0
[0.9.0]: https://github.com/username/project/compare/v0.8.0...v0.9.0
[0.8.0]: https://github.com/username/project/compare/v0.1.0...v0.8.0
[0.1.0]: https://github.com/username/project/releases/tag/v0.1.0
