# Accessibility Test Suite - Quick Start Guide

## Overview

20 comprehensive Playwright tests for WCAG 2.1 Level AA accessibility compliance.

**Tests**: A11Y-001 to A11Y-020
**Categories**: Semantic HTML, ARIA, Keyboard, Visual, Interactive
**Time to Run**: ~30-45 seconds
**Success Rate Target**: 100% (all 20 tests pass)

---

## 60-Second Setup

### 1. Start Test Servers

```bash
# Terminal 1
npm run test-server

# Terminal 2 (in another terminal)
PORT=8081 npm run test-server

# Or start both at once
npm run servers
```

### 2. Run All A11Y Tests

```bash
npx playwright test tests/accessibility/a11y.spec.ts
```

### 3. View Results

```bash
npm run report
```

---

## Test Categories at a Glance

### 1️⃣ Semantic HTML (A11Y-001 to A11Y-005)
```bash
npx playwright test tests/accessibility/a11y.spec.ts -g "Semantic HTML"
```
- ✅ H1→H2→H3→H4 hierarchy
- ✅ Landmark roles (header, nav, main, footer)
- ✅ Lists (ul, ol, dl structure)
- ✅ Tables with caption & scope
- ✅ Form labels & fieldsets

### 2️⃣ ARIA Attributes (A11Y-006 to A11Y-010)
```bash
npx playwright test tests/accessibility/a11y.spec.ts -g "ARIA Attributes"
```
- ✅ aria-label / aria-labelledby
- ✅ aria-describedby
- ✅ aria-hidden / inert
- ✅ Valid ARIA roles
- ✅ Live regions (aria-live)

### 3️⃣ Keyboard Navigation (A11Y-011 to A11Y-015)
```bash
npx playwright test tests/accessibility/a11y.spec.ts -g "Keyboard Navigation"
```
- ✅ Tab order (logical, no traps)
- ✅ Skip links
- ✅ Keyboard shortcuts
- ✅ Focus visible
- ✅ No keyboard traps

### 4️⃣ Visual (A11Y-016 to A11Y-018)
```bash
npx playwright test tests/accessibility/a11y.spec.ts -g "Visual Accessibility"
```
- ✅ Color contrast 4.5:1 (WCAG AA)
- ✅ Text resizing at 200% zoom
- ✅ Focus indicators visible

### 5️⃣ Interactive (A11Y-019 to A11Y-020)
```bash
npx playwright test tests/accessibility/a11y.spec.ts -g "Interactive"
```
- ✅ Form error identification
- ✅ Modal focus management

---

## Running Specific Tests

```bash
# Run single test
npx playwright test -g "A11Y-001"

# Run with UI
npx playwright test tests/accessibility/a11y.spec.ts --ui

# Run with headed browser
npx playwright test tests/accessibility/a11y.spec.ts --headed

# Run with debug
npx playwright test tests/accessibility/a11y.spec.ts --debug

# Run with specific reporter
npx playwright test tests/accessibility/a11y.spec.ts --reporter=list
```

---

## Test Fixtures

All fixtures include working examples of accessible components:

| Fixture | Tests | URL |
|---------|-------|-----|
| `semantic-html.html` | A11Y-001 to 005 | http://localhost:8080/accessibility/semantic-html.html |
| `aria-attributes.html` | A11Y-006 to 010 | http://localhost:8080/accessibility/aria-attributes.html |
| `keyboard-navigation.html` | A11Y-011 to 015 | http://localhost:8080/accessibility/keyboard-navigation.html |
| `visual-accessibility.html` | A11Y-016 to 018 | http://localhost:8080/accessibility/visual-accessibility.html |
| `interactive-accessibility.html` | A11Y-019 to 020 | http://localhost:8080/accessibility/interactive-accessibility.html |

**Navigate to fixtures** in browser while server running:
```
http://localhost:8080/accessibility/semantic-html.html
```

---

## Expected Output

```
$ npx playwright test tests/accessibility/a11y.spec.ts

Semantic HTML Structure
  ✓ A11Y-001: Proper heading hierarchy (H1 > H2 > H3 > H4) (1.2s)
  ✓ A11Y-002: Landmark roles (header, nav, main, footer) (980ms)
  ✓ A11Y-003: Lists with proper structure (ul, ol, dl) (890ms)
  ✓ A11Y-004: Tables with semantic markup (caption, headers, scope) (950ms)
  ✓ A11Y-005: Form fields with proper labels and fieldsets (1.1s)

ARIA Attributes and Roles
  ✓ A11Y-006: aria-label and aria-labelledby (1.0s)
  ✓ A11Y-007: aria-describedby for supplemental descriptions (980ms)
  ✓ A11Y-008: aria-hidden and inert content (900ms)
  ✓ A11Y-009: ARIA roles (button, tab, dialog) (1.1s)
  ✓ A11Y-010: Live regions (aria-live) (1.2s)

Keyboard Navigation
  ✓ A11Y-011: Tab order and focus management (1.0s)
  ✓ A11Y-012: Skip links for main content (980ms)
  ✓ A11Y-013: Keyboard shortcuts availability (900ms)
  ✓ A11Y-014: Focus visible indicators (1.1s)
  ✓ A11Y-015: No keyboard traps (1.2s)

Visual Accessibility
  ✓ A11Y-016: Color contrast (WCAG AA 4.5:1) (980ms)
  ✓ A11Y-017: Text resizing without overflow (200%) (1.1s)
  ✓ A11Y-018: Focus indicators visible and clear (900ms)

Interactive Components
  ✓ A11Y-019: Form error identification and recovery (1.2s)
  ✓ A11Y-020: Modal dialog focus management (1.0s)

20 passed (38.2s)
```

---

## Common Test Commands

```bash
# All accessibility tests
npx playwright test tests/accessibility/a11y.spec.ts

# Specific test file
npx playwright test tests/accessibility/a11y.spec.ts -g "A11Y-001"

# By category
npx playwright test tests/accessibility/a11y.spec.ts -g "Semantic HTML"
npx playwright test tests/accessibility/a11y.spec.ts -g "ARIA Attributes"
npx playwright test tests/accessibility/a11y.spec.ts -g "Keyboard Navigation"
npx playwright test tests/accessibility/a11y.spec.ts -g "Visual Accessibility"
npx playwright test tests/accessibility/a11y.spec.ts -g "Interactive Components"

# With browser visible
npx playwright test tests/accessibility/a11y.spec.ts --headed

# Interactive UI
npx playwright test tests/accessibility/a11y.spec.ts --ui

# Debug mode
npx playwright test tests/accessibility/a11y.spec.ts --debug

# Verbose output
npx playwright test tests/accessibility/a11y.spec.ts --reporter=verbose

# View test report
npx playwright show-report
```

---

## Test Structure

```
tests/accessibility/
└── a11y.spec.ts                 # All 20 tests

fixtures/accessibility/
├── semantic-html.html            # Tests A11Y-001 to 005
├── aria-attributes.html          # Tests A11Y-006 to 010
├── keyboard-navigation.html      # Tests A11Y-011 to 015
├── visual-accessibility.html     # Tests A11Y-016 to 018
└── interactive-accessibility.html # Tests A11Y-019 to 020

helpers/
└── accessibility-test-utils.ts   # 25+ helper functions
```

---

## Helper Utilities Available

All helper functions are in `helpers/accessibility-test-utils.ts`:

```typescript
// Semantic HTML
validateHeadingHierarchy()      // H1→H2→H3 hierarchy check
validateLandmarks()              // header, nav, main, footer
validateListStructure()          // ul, ol, dl validation
validateTableAccessibility()     // caption, headers, scope
validateFormLabels()             // associated labels

// ARIA
validateAriaLabels()             // aria-label, aria-labelledby
validateAriaHidden()             // aria-hidden, inert
validateAriaRoles()              // valid role names
validateLiveRegions()            // aria-live regions

// Keyboard
getKeyboardNavigationInfo()      // focusable elements
validateTabOrder()               // tabindex values
validateSkipLinks()              // skip links present
navigateWithKeyboard()           // Tab navigation

// Visual
checkFocusVisibility()           // focus outline/background
calculateColorContrast()         // ratio calculation
testTextResizing()               // 200% zoom test

// Interactive
validateModalFocus()             // focus trap
validateFormErrors()             // error identification
```

---

## Troubleshooting

### Tests fail with "page crashed"
**Solution**: Ensure Ladybird is properly built
```bash
./Meta/ladybird.py build
./Meta/ladybird.py test LibWeb  # Test basic functionality first
```

### "Cannot connect to localhost:8080"
**Solution**: Start test servers first
```bash
npm run test-server
npm run test-server-alt  # In another terminal
```

### Timeout errors
**Solution**: Increase timeout in `playwright.config.ts`
```typescript
timeout: 120000,  // 2 minutes
```

### Focus test fails
**Solution**: Ensure Ladybird window is focused (not minimized)

### Contrast calculation errors
**Solution**: Some colors are calculated from CSS. Verify with developer tools

---

## WCAG 2.1 Coverage

✅ **12 WCAG Criteria Covered** at Level AA:

- 1.3.1 Info and Relationships (semantic HTML, ARIA)
- 1.4.3 Contrast (minimum 4.5:1)
- 1.4.4 Resize Text (200% zoom)
- 2.1.1 Keyboard (all features)
- 2.1.2 No Keyboard Trap
- 2.1.4 Character Key Shortcuts
- 2.4.1 Bypass Blocks (skip links)
- 2.4.3 Focus Order
- 2.4.7 Focus Visible
- 3.3.1 Error Identification
- 3.3.4 Error Prevention
- 4.1.3 Status Messages

---

## Next Steps

1. **Run all tests**: `npx playwright test tests/accessibility/a11y.spec.ts`
2. **Review failures**: Look at test output and screenshots
3. **Check fixtures**: Open fixtures in browser to see examples
4. **Read WCAG criteria**: https://www.w3.org/WAI/WCAG21/quickref/
5. **Fix issues**: Update your code to match WCAG 2.1 AA

---

## Resources

- **Full Documentation**: See `A11Y_TEST_DOCUMENTATION.md`
- **WCAG 2.1 Quick Ref**: https://www.w3.org/WAI/WCAG21/quickref/
- **ARIA Practices**: https://www.w3.org/WAI/ARIA/apg/
- **WebAIM Keyboard**: https://webaim.org/articles/keyboard/
- **Playwright Docs**: https://playwright.dev/

---

## Key Files

| File | Purpose |
|------|---------|
| `tests/accessibility/a11y.spec.ts` | All 20 tests |
| `helpers/accessibility-test-utils.ts` | Helper functions |
| `fixtures/accessibility/*.html` | Test fixtures |
| `A11Y_TEST_DOCUMENTATION.md` | Complete reference |
| `A11Y_QUICK_START.md` | This file |

