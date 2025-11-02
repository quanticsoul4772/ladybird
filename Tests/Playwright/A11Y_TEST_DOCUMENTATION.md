# Accessibility Testing Suite (A11Y-001 to A11Y-020)

## Overview

Comprehensive Playwright test suite for WCAG 2.1 Level AA web accessibility compliance. Covers all major accessibility categories with 20 focused tests, helper utilities, and HTML fixtures.

**Total Coverage**: 20 Tests
**Priority**: P1 (High)
**WCAG Level**: 2.1 AA
**Framework**: Playwright + TypeScript

---

## Test Categories

### 1. Semantic HTML (5 Tests: A11Y-001 to A11Y-005)

Tests proper use of semantic HTML elements for page structure and meaning.

| Test ID | Title | WCAG Criterion | Description |
|---------|-------|--------|-------------|
| A11Y-001 | Heading Hierarchy | 1.3.1 | Validates proper H1→H2→H3→H4 hierarchy without skips |
| A11Y-002 | Landmark Roles | 1.3.1 | Verifies header, nav, main, footer landmarks present |
| A11Y-003 | List Structure | 1.3.1 | Checks ul, ol, dl with proper li/dt/dd elements |
| A11Y-004 | Table Semantics | 1.3.1 | Validates caption, th headers with scope attributes |
| A11Y-005 | Form Labels | 1.3.1 | Ensures all fields have associated labels or ARIA |

**Fixture**: `/fixtures/accessibility/semantic-html.html`

### 2. ARIA Attributes (5 Tests: A11Y-006 to A11Y-010)

Tests proper ARIA attribute usage for enhanced accessibility.

| Test ID | Title | WCAG Criterion | Description |
|---------|-------|--------|-------------|
| A11Y-006 | aria-label/labelledby | 1.3.1 | Validates accessible names via ARIA labels |
| A11Y-007 | aria-describedby | 1.3.1 | Checks for field descriptions and extra help text |
| A11Y-008 | aria-hidden/inert | 2.1.1 | Verifies decorative content hidden from tree |
| A11Y-009 | ARIA Roles | 1.3.1 | Validates button, tab, dialog, and custom roles |
| A11Y-010 | Live Regions | 4.1.3 | Tests aria-live for dynamic content announcements |

**Fixture**: `/fixtures/accessibility/aria-attributes.html`

### 3. Keyboard Navigation (5 Tests: A11Y-011 to A11Y-015)

Tests full keyboard accessibility without mouse dependency.

| Test ID | Title | WCAG Criterion | Description |
|---------|-------|--------|-------------|
| A11Y-011 | Tab Order | 2.1.1 | Validates logical tab order matching reading order |
| A11Y-012 | Skip Links | 2.4.1 | Checks for skip-to-main-content links |
| A11Y-013 | Keyboard Shortcuts | 2.1.4 | Verifies shortcuts are documented and conflict-free |
| A11Y-014 | Focus Visible | 2.4.7 | Ensures focused elements have visible indicators |
| A11Y-015 | No Traps | 2.1.2 | Confirms keyboard focus can escape any element |

**Fixture**: `/fixtures/accessibility/keyboard-navigation.html`

### 4. Visual Accessibility (3 Tests: A11Y-016 to A11Y-018)

Tests visual design for accessibility (contrast, sizing, indicators).

| Test ID | Title | WCAG Criterion | Description |
|---------|-------|--------|-------------|
| A11Y-016 | Color Contrast | 1.4.3 | Validates 4.5:1 contrast for normal text (AA) |
| A11Y-017 | Text Resizing | 1.4.4 | Tests readability at 200% zoom without overflow |
| A11Y-018 | Focus Indicators | 2.4.7 | Verifies focus outline/highlight visibility |

**Fixture**: `/fixtures/accessibility/visual-accessibility.html`

### 5. Interactive Components (2 Tests: A11Y-019 to A11Y-020)

Tests accessibility of dynamic and interactive components.

| Test ID | Title | WCAG Criterion | Description |
|---------|-------|--------|-------------|
| A11Y-019 | Form Errors | 3.3.1, 3.3.4 | Validates error identification and recovery |
| A11Y-020 | Modal Focus | 2.1.2, 2.4.3 | Tests focus trap and restoration in modals |

**Fixture**: `/fixtures/accessibility/interactive-accessibility.html`

---

## Test Execution

### Running All Accessibility Tests

```bash
# Run all A11Y tests
npx playwright test tests/accessibility/a11y.spec.ts

# Run with UI
npx playwright test tests/accessibility/a11y.spec.ts --ui

# Run specific test
npx playwright test tests/accessibility/a11y.spec.ts -g "A11Y-001"

# Run with headed browser
npx playwright test tests/accessibility/a11y.spec.ts --headed

# Run in debug mode
npx playwright test tests/accessibility/a11y.spec.ts --debug
```

### Running by Category

```bash
# Semantic HTML tests only
npx playwright test tests/accessibility/a11y.spec.ts -g "Semantic HTML"

# ARIA tests only
npx playwright test tests/accessibility/a11y.spec.ts -g "ARIA Attributes"

# Keyboard navigation tests
npx playwright test tests/accessibility/a11y.spec.ts -g "Keyboard Navigation"

# Visual accessibility tests
npx playwright test tests/accessibility/a11y.spec.ts -g "Visual Accessibility"

# Interactive components tests
npx playwright test tests/accessibility/a11y.spec.ts -g "Interactive Components"
```

### Quick Run Command

```bash
# Start test server first (if not already running)
npm run test-server &
npm run test-server-alt &

# Run tests
npm test tests/accessibility/a11y.spec.ts
```

---

## Test Fixtures

All test fixtures are self-contained HTML pages demonstrating proper accessibility practices.

### Fixture Locations

```
fixtures/accessibility/
├── semantic-html.html          (A11Y-001 to A11Y-005)
├── aria-attributes.html         (A11Y-006 to A11Y-010)
├── keyboard-navigation.html     (A11Y-011 to A11Y-015)
├── visual-accessibility.html    (A11Y-016 to A11Y-018)
└── interactive-accessibility.html (A11Y-019 to A11Y-020)
```

### Running Fixtures Locally

```bash
# Fixtures are served by test-server.js on port 8080
# Access via: http://localhost:8080/accessibility/semantic-html.html

# Or directly via file path
file:///home/rbsmith4/ladybird/Tests/Playwright/fixtures/accessibility/semantic-html.html
```

---

## Helper Utilities

Location: `/helpers/accessibility-test-utils.ts`

### Key Utility Functions

#### Semantic HTML Validation

```typescript
// Validate heading hierarchy
const hierarchy = await validateHeadingHierarchy(page);
// Returns: { valid, headings[], errors[] }

// Check landmark roles
const landmarks = await validateLandmarks(page);
// Returns: { found, landmarks[], missingLandmarks[] }

// Validate list structure
const lists = await validateListStructure(page);
// Returns: { valid, lists[], errors[] }

// Check table accessibility
const tables = await validateTableAccessibility(page);
// Returns: { valid, hasCaption, hasHeaderCells, headerScopeValid, errors[] }

// Validate form labels
const labels = await validateFormLabels(page);
// Returns: { valid, unlabeledFields[], orphanLabels[], errors[] }
```

#### ARIA Validation

```typescript
// Validate ARIA labels
const ariaLabels = await validateAriaLabels(page);
// Returns: { valid, elements[], errors[] }

// Check aria-hidden/inert
const hidden = await validateAriaHidden(page);
// Returns: { valid, hidden, issues[] }

// Validate ARIA roles
const roles = await validateAriaRoles(page);
// Returns: { valid, roles[], issues[] }

// Check live regions
const liveRegions = await validateLiveRegions(page);
// Returns: { valid, regions, issues[] }
```

#### Keyboard Navigation

```typescript
// Get keyboard navigation info
const navInfo = await getKeyboardNavigationInfo(page);
// Returns: { valid, focusableElements, tabSequence[], errors[] }

// Validate tab order
const tabOrder = await validateTabOrder(page);
// Returns: { valid, tabIndexValues[], issues[] }

// Check skip links
const skipLinks = await validateSkipLinks(page);
// Returns: { valid, skipLinks }

// Get current focus element
const focused = await getCurrentFocusElement(page);
// Returns: string (selector of focused element)

// Navigate using keyboard
const focusPath = await navigateWithKeyboard(page, 5);
// Returns: string[] (sequence of focused elements)
```

#### Visual Accessibility

```typescript
// Check focus visibility
const focusResult = await checkFocusVisibility(page, selector);
// Returns: { visible, element, hasOutline, hasBackground, hasBorder }

// Calculate color contrast
const contrast = await calculateColorContrast(page, selector);
// Returns: { ratio, wcagAA, wcagAAA, foreground, background }

// Test text resizing
const resize = await testTextResizing(page);
// Returns: { valid, resized, overflow, readableAt200 }
```

#### Interactive Components

```typescript
// Validate modal focus
const modal = await validateModalFocus(page, modalSelector);
// Returns: { valid, focusTrapWorking, initialFocus, restoreFocus, errors[] }

// Validate form errors
const errors = await validateFormErrors(page, formSelector);
// Returns: { valid, errorsIdentified[], issues[] }
```

---

## WCAG 2.1 Criteria Covered

| Criterion | Tests | Description |
|-----------|-------|-------------|
| 1.3.1 Info and Relationships | A11Y-001 to A11Y-009 | Semantic meaning and associations |
| 1.4.3 Contrast (Minimum) | A11Y-016 | Text and background contrast ratios |
| 1.4.4 Resize Text | A11Y-017 | Text must not overflow at 200% zoom |
| 2.1.1 Keyboard | A11Y-011, A11Y-015 | All functionality available via keyboard |
| 2.1.2 No Keyboard Trap | A11Y-015, A11Y-020 | Keyboard focus can escape any element |
| 2.1.4 Character Key Shortcuts | A11Y-013 | Shortcuts don't conflict with browser keys |
| 2.4.1 Bypass Blocks | A11Y-012 | Skip links for repetitive content |
| 2.4.3 Focus Order | A11Y-011, A11Y-020 | Logical tab and focus order |
| 2.4.7 Focus Visible | A11Y-014, A11Y-018 | Focused elements are visually obvious |
| 3.3.1 Error Identification | A11Y-019 | Errors are identified to users |
| 3.3.4 Error Prevention | A11Y-019 | Users can review before submitting |
| 4.1.3 Status Messages | A11Y-010 | Live regions announce dynamic changes |

---

## Expected Results

All tests should pass (100% green) on properly accessible HTML. Example test output:

```
✓ A11Y-001: Proper heading hierarchy (H1 > H2 > H3 > H4)
✓ A11Y-002: Landmark roles (header, nav, main, footer)
✓ A11Y-003: Lists with proper structure (ul, ol, dl)
✓ A11Y-004: Tables with semantic markup (caption, headers, scope)
✓ A11Y-005: Form fields with proper labels and fieldsets
✓ A11Y-006: aria-label and aria-labelledby
✓ A11Y-007: aria-describedby for supplemental descriptions
✓ A11Y-008: aria-hidden and inert content
✓ A11Y-009: ARIA roles (button, tab, dialog)
✓ A11Y-010: Live regions (aria-live)
✓ A11Y-011: Tab order and focus management
✓ A11Y-012: Skip links for main content
✓ A11Y-013: Keyboard shortcuts availability
✓ A11Y-014: Focus visible indicators
✓ A11Y-015: No keyboard traps
✓ A11Y-016: Color contrast (WCAG AA 4.5:1)
✓ A11Y-017: Text resizing without overflow (200%)
✓ A11Y-018: Focus indicators visible and clear
✓ A11Y-019: Form error identification and recovery
✓ A11Y-020: Modal dialog focus management

20 passed
```

---

## Common Issues Found & How Tests Detect Them

### Semantic HTML Issues
- **Missing H1**: Test A11Y-001 will fail if no H1 found
- **Heading skips**: Test A11Y-001 detects H1→H3 (missing H2)
- **No landmarks**: Test A11Y-002 fails without header/nav/main/footer
- **Unlabeled fields**: Test A11Y-005 identifies form inputs without labels
- **Invalid tables**: Test A11Y-004 checks for caption and scope attributes

### ARIA Issues
- **Broken aria-labelledby**: Test A11Y-006 verifies referenced IDs exist
- **Invalid roles**: Test A11Y-009 checks for valid ARIA role names
- **Focusable in aria-hidden**: Test A11Y-008 detects interactive elements hidden

### Keyboard Issues
- **Can't reach elements**: Test A11Y-011 counts focusable elements
- **Positive tabindex**: Test A11Y-011 flags tabindex > 0 (anti-pattern)
- **No skip links**: Test A11Y-012 fails if no skip-to-main link found
- **Focus escapes**: Test A11Y-015 navigates to verify no traps

### Visual Issues
- **Low contrast**: Test A11Y-016 calculates ratio < 4.5:1 for AA failure
- **Text cuts off**: Test A11Y-017 checks for overflow at 200% zoom
- **No focus outline**: Test A11Y-018 verifies outline or background change

### Interactive Issues
- **Errors not marked**: Test A11Y-019 checks for aria-invalid="true"
- **Focus escapes modal**: Test A11Y-020 detects focus trapped outside modal

---

## Integration with Test Server

Tests require the Express.js test servers running:

```bash
# Terminal 1: Start primary server (port 8080)
npm run test-server

# Terminal 2: Start secondary server (port 8081, for cross-origin tests)
PORT=8081 npm run test-server

# Or start both concurrently
npm run servers

# In playwright.config.ts, servers are configured to auto-start:
webServer: [
  { command: 'node test-server.js', port: 8080 },
  { command: 'PORT=8081 node test-server.js', port: 8081 },
]
```

---

## Custom Test Implementation

To add a new accessibility test:

1. **Create fixture** in `fixtures/accessibility/your-test.html`
2. **Add helper utility** in `helpers/accessibility-test-utils.ts` if needed
3. **Add test** to `tests/accessibility/a11y.spec.ts`:

```typescript
test('A11Y-XXX: Your test description', { tag: '@p1' }, async ({ page }) => {
  // Navigate to fixture
  await page.goto('http://localhost:8080/accessibility/your-test.html');

  // Use helper utilities
  const result = await yourValidationFunction(page);

  // Assertions
  expect(result.valid).toBe(true);
  expect(result.errors).toHaveLength(0);
});
```

---

## Performance & CI/CD

- **Execution time**: ~30-45 seconds for all 20 tests
- **Parallel execution**: Tests run in parallel (faster on CI)
- **Retries**: 2 retries on CI for flaky tests
- **Screenshots**: Captured on failure for debugging

### GitHub Actions Integration

```yaml
- name: Run Accessibility Tests
  run: npx playwright test tests/accessibility/a11y.spec.ts

- name: Upload Test Results
  if: always()
  uses: actions/upload-artifact@v3
  with:
    name: a11y-test-results
    path: playwright-report/
```

---

## Troubleshooting

### Tests Timeout
- Ensure test servers are running on ports 8080 and 8081
- Check `playwright.config.ts` timeout settings

### Contrast Calculation Issues
- Tests use WCAG formula: (L1 + 0.05) / (L2 + 0.05)
- Color values must be in sRGB color space
- Some colors may be inherited or affected by CSS filters

### Focus Not Detected
- Ensure browser is in focus (not minimized)
- Check for CSS `outline: none` (common anti-pattern)
- Verify focus-visible polyfill if on older browsers

### Modal Focus Trap Not Working
- Verify modal has role="dialog" and aria-modal="true"
- Check that first/last focusable elements are correctly identified
- Ensure Tab key event listener is properly attached

---

## References

- **WCAG 2.1 Guidelines**: https://www.w3.org/WAI/WCAG21/quickref/
- **Web Content Accessibility Guidelines**: https://www.w3.org/WAI/WCAG21/
- **ARIA Authoring Practices**: https://www.w3.org/WAI/ARIA/apg/
- **Keyboard Accessibility**: https://webaim.org/articles/keyboard/
- **Screen Reader Testing**: https://www.nvaccess.org/

---

## Support & Updates

For issues, questions, or improvements:

1. Review WCAG 2.1 criteria: https://www.w3.org/WAI/WCAG21/quickref/
2. Check axe-core violations: https://github.com/dequelabs/axe-core/blob/develop/doc/rule-descriptions.md
3. Test with actual assistive technologies (NVDA, JAWS, VoiceOver)
4. Use automated tools: axe DevTools, Lighthouse, WAVE

---

## Test Statistics

- **Total Tests**: 20
- **Test Categories**: 5
- **Helper Functions**: 25+
- **HTML Fixtures**: 5
- **WCAG Criteria Covered**: 12+
- **Estimated Run Time**: 30-45 seconds
- **Browser Coverage**: Ladybird + Chromium reference

