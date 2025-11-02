# Edge Case and Boundary Tests

Comprehensive test suite for edge cases, boundary conditions, and unusual scenarios across Ladybird browser functionality.

## Test Overview

**File**: `boundaries.spec.ts`
**Tests**: 18 (EDGE-001 to EDGE-018)
**Priority**: P1 (Critical) and P2 (High)
**Lines of Code**: ~1,360

## Test Categories

### Input Boundaries (EDGE-001 to EDGE-004) - Priority: P1

Tests handling of edge cases in user input:

- **EDGE-001**: Empty string handling in forms
  - Verifies browser handles empty strings in text fields, textareas, select elements
  - Tests form submission with all empty values
  - No crashes or undefined behavior

- **EDGE-002**: Maximum length input boundaries
  - Tests very long input values (5,000 to 1,000,000 characters)
  - Verifies maxlength attribute enforcement
  - Tests unlimited fields with long strings
  - Ensures no buffer overflows or crashes

- **EDGE-003**: Special characters and encoding
  - Tests HTML entities, unicode, emoji, SQL injection attempts
  - Verifies XSS prevention (script tags should not execute)
  - Tests proper escaping and encoding preservation
  - Validates newlines, backslashes, quotes, null bytes

- **EDGE-004**: Null and undefined values
  - Tests setting null/undefined on form field values
  - Verifies automatic conversion to strings
  - Ensures no crashes or exceptions

### DOM Edge Cases (EDGE-005 to EDGE-007) - Priority: P2

Tests handling of unusual DOM structures:

- **EDGE-005**: Deeply nested DOM elements
  - Creates 500+ levels of nested divs
  - Verifies querySelector still works
  - Tests no stack overflow occurs
  - Validates proper parent traversal

- **EDGE-006**: Disconnected DOM nodes operations
  - Tests operations on nodes not in the document
  - Verifies appendChild, cloneNode, querySelector on disconnected nodes
  - Tests re-attachment to document

- **EDGE-007**: Circular reference cleanup
  - Creates circular object references
  - Creates circular DOM node references
  - Verifies garbage collection can handle circles
  - Tests proper cleanup

### Navigation Edge Cases (EDGE-008 to EDGE-010) - Priority: P1/P2

Tests browser navigation edge cases:

- **EDGE-008**: Rapid navigation (race conditions) - P1
  - Fires 5 navigation requests rapidly
  - Tests for race conditions and crashes
  - Verifies browser handles navigation cancellations
  - Ensures final page loads correctly

- **EDGE-009**: Invalid URL handling - P1
  - Tests 16 types of invalid URLs
  - Includes malformed protocols, missing parts, invalid characters
  - Verifies graceful error handling
  - Tests javascript: URL blocking

- **EDGE-010**: Data URL size limits - P2
  - Tests data URLs from 1 KB to 1 MB
  - Verifies browser handles or rejects gracefully
  - No crashes on very large data URLs

### Form Edge Cases (EDGE-011 to EDGE-013) - Priority: P1

Tests form handling edge cases:

- **EDGE-011**: Duplicate form submissions - P1
  - Tests rapid-fire clicking of submit button
  - Verifies multiple submissions are tracked
  - Tests timing detection for bot behavior

- **EDGE-012**: Malformed form data - P1
  - Tests invalid email, out-of-range numbers, invalid URLs
  - Verifies HTML5 validation works correctly
  - Tests date field with invalid dates

- **EDGE-013**: Form with all required fields missing - P1
  - Tests validation when all required fields are empty
  - Verifies form submission is prevented
  - Tests checkValidity() returns false

### Sentinel Edge Cases (EDGE-014 to EDGE-016) - Priority: P2

Tests Sentinel security system edge cases:

- **EDGE-014**: PolicyGraph with empty database - P2
  - Tests behavior when no policies exist
  - Verifies graceful degradation
  - Tests default prompt behavior

- **EDGE-015**: FormMonitor with malformed data - P2
  - Tests forms with unusual structures
  - Fields with no name attribute
  - Multiple password fields with same name
  - Very long field names (1000 chars)
  - Special characters in field names

- **EDGE-016**: Fingerprinting with disabled APIs - P2
  - Tests when Canvas, WebGL, Audio APIs are unavailable
  - Verifies graceful degradation
  - No crashes when APIs are missing

### Resource Edge Cases (EDGE-017 to EDGE-018) - Priority: P1/P2

Tests resource loading edge cases:

- **EDGE-017**: 404 resources and error handling - P1
  - Tests missing CSS, JavaScript, images
  - Verifies page remains functional
  - Tests error event listeners

- **EDGE-018**: CORS failures and cross-origin errors - P2
  - Tests fetch() and XMLHttpRequest CORS failures
  - Verifies proper error handling
  - Tests browser remains functional after CORS errors

## Running Tests

```bash
# Run all edge case tests
npx playwright test tests/edge-cases/boundaries.spec.ts

# Run specific priority
npx playwright test tests/edge-cases/boundaries.spec.ts --grep "@p1"

# Run specific test
npx playwright test tests/edge-cases/boundaries.spec.ts --grep "EDGE-001"

# Run with specific browser
npx playwright test tests/edge-cases/boundaries.spec.ts --project=ladybird
```

## Test Results

All 18 tests pass on both Ladybird and Chromium reference browsers (36 total test runs).

## Key Testing Patterns

1. **Graceful Degradation**: Tests verify browser doesn't crash even with invalid input
2. **Try/Catch Blocks**: Expected failures are caught and verified
3. **Console Logging**: Each test logs its progress for debugging
4. **Boundary Testing**: Tests extremes (empty, very large, invalid)
5. **XSS Prevention**: Tests that malicious code doesn't execute
6. **Validation Testing**: Verifies HTML5 form validation works

## Important Notes

- Tests use data: URLs for most scenarios (no server required)
- Some tests intentionally trigger errors to verify error handling
- Race condition tests may show expected navigation cancellations
- Special character tests verify both storage and XSS prevention
- All tests verify "no crash" as primary success criterion

## Future Enhancements

Potential additional edge cases to test:

- WebWorker edge cases
- WebSocket connection failures
- LocalStorage quota exceeded
- IndexedDB edge cases
- Service Worker installation failures
- Media element error handling
- Drag-and-drop edge cases
- Clipboard API edge cases

## Related Tests

- `tests/forms/form-handling.spec.ts` - Form handling tests
- `tests/security/form-monitor.spec.ts` - Sentinel FormMonitor tests
- `tests/security/fingerprinting.spec.ts` - Fingerprinting detection tests
- `tests/accessibility/a11y.spec.ts` - Accessibility tests
