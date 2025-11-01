# Core Browser Functionality Test Suite

Comprehensive Playwright test suite for Ladybird browser core functionality.

## Overview

**Total Tests**: 45
**Test Files**: 4
**Coverage**: Navigation, Tabs, History, Bookmarks
**Status**: ✅ Complete and ready for execution

## Test Files

### 1. navigation.spec.ts (15 tests - P0)
Tests fundamental page navigation functionality:
- HTTP/HTTPS navigation
- Forward/back buttons
- Page reloading
- Link navigation (target=_blank, target=_self)
- Anchor links with smooth scroll
- JavaScript navigation
- Redirects (301, 302, meta refresh)
- Invalid URL handling
- Data URL navigation

**Test IDs**: NAV-001 to NAV-015

### 2. tabs.spec.ts (12 tests - P0)
Tests browser tab management:
- Opening/closing tabs (Ctrl+T, Ctrl+W)
- Tab switching (Ctrl+Tab, click)
- Tab restoration (Ctrl+Shift+T)
- Tab manipulation (duplicate, pin, reorder)
- Multi-tab performance (10+ tabs)
- Dynamic title updates

**Test IDs**: TAB-001 to TAB-012

### 3. history.spec.ts (8 tests - P0)
Tests browsing history functionality:
- Forward/back navigation
- History population
- History search/filter
- Clear history
- Date grouping
- Private browsing mode
- Individual entry deletion

**Test IDs**: HIST-001 to HIST-008

### 4. bookmarks.spec.ts (10 tests - P1)
Tests bookmark management:
- Add/remove bookmarks (Ctrl+D)
- Edit bookmark properties
- Folder organization
- Bookmark bar toggle
- Import/export (HTML format)
- Search functionality
- Duplicate detection

**Test IDs**: BKM-001 to BKM-010

## Running Tests

### Quick Start
```bash
# Run all core browser tests
npx playwright test tests/core-browser/

# Run specific test file
npx playwright test tests/core-browser/tabs.spec.ts

# Run with UI
npx playwright test tests/core-browser/ --ui

# Run in debug mode
npx playwright test tests/core-browser/tabs.spec.ts --debug
```

### Filter by Priority
```bash
# Run only P0 (critical) tests
npx playwright test tests/core-browser/ --grep "@p0"

# Run only P1 (important) tests
npx playwright test tests/core-browser/ --grep "@p1"
```

### Run Individual Tests
```bash
# Run specific test by ID
npx playwright test tests/core-browser/tabs.spec.ts -g "TAB-001"

# Run multiple specific tests
npx playwright test tests/core-browser/tabs.spec.ts -g "TAB-001|TAB-002|TAB-003"
```

### Generate Reports
```bash
# Run tests and generate HTML report
npx playwright test tests/core-browser/
npx playwright show-report
```

## Test Structure

Each test follows this pattern:

```typescript
test('TEST-ID: Description', { tag: '@priority' }, async ({ page, context }) => {
  // 1. Setup / Navigation
  await page.goto('http://example.com');

  // 2. Perform actions
  await page.click('button');

  // 3. Assert expectations
  await expect(page).toHaveTitle(/Expected Title/);

  // 4. Cleanup (if needed)
  await newPage.close();
});
```

## Best Practices

### ✅ DO:
- Use descriptive test names matching TEST_MATRIX.md
- Add priority tags (@p0, @p1)
- Wait for page load states
- Clean up opened tabs/contexts
- Capture evidence on failure

### ❌ DON'T:
- Use arbitrary timeouts (prefer waitForLoadState)
- Leave tabs/contexts open without cleanup
- Make tests dependent on each other
- Skip assertions

## Evidence Capture

Tests automatically capture:
- **Screenshots**: On failure
- **Videos**: For failed tests
- **Traces**: On first retry
- **Console logs**: Via page.evaluate()

Evidence location: `Tests/Playwright/test-results/`

## Verification

Run the verification script:
```bash
./verify-tests.sh
```

This checks:
- ✅ All test files exist
- ✅ Test count matches expectations (45 total)
- ✅ Proper test structure (describe blocks, tags)
- ✅ File integrity

## Test Coverage Matrix

| Feature | Tests | Priority | Status |
|---------|-------|----------|--------|
| Navigation | 15 | P0 | ✅ Complete |
| Tab Management | 12 | P0 | ✅ Complete |
| History | 8 | P0 | ✅ Complete |
| Bookmarks | 10 | P1 | ✅ Complete |
| **TOTAL** | **45** | **P0/P1** | **✅** |

## Known Limitations

Some tests document expected behavior but note features requiring browser UI automation:

1. **Bookmark Manager**: Tests document bookmark operations that require browser chrome access
2. **History UI**: Date grouping, search require history UI automation
3. **Tab Reordering**: Drag-and-drop requires browser UI interaction
4. **Pin/Unpin Tabs**: Requires access to tab chrome

These tests serve as documentation and will be enhanced when browser APIs are available.

## Integration with TEST_MATRIX.md

All tests align with `/home/rbsmith4/ladybird/Tests/Playwright/TEST_MATRIX.md`:

- **Section 1.1**: Navigation (NAV-001 to NAV-015) ✅
- **Section 1.2**: Tab Management (TAB-001 to TAB-012) ✅
- **Section 1.3**: History Management (HIST-001 to HIST-008) ✅
- **Section 1.4**: Bookmarks (BOOK-001 to BOOK-010) ✅

## Next Steps

1. **Run tests against Ladybird**:
   - Update playwright.config.ts with Ladybird binary path
   - Execute test suite
   - Review results

2. **CI/CD Integration**:
   - Add tests to GitHub Actions
   - Configure artifact upload
   - Set up test reporting

3. **Expand coverage**:
   - Settings & Preferences (10 tests)
   - HTML/CSS Rendering (30 tests)
   - JavaScript Execution (15 tests)
   - Security Features (40 tests)

## Support

- **Documentation**: See `IMPLEMENTATION_REPORT.md` for detailed implementation notes
- **Test Matrix**: See `TEST_MATRIX.md` for complete test specifications
- **Playwright Docs**: https://playwright.dev/

## Contributing

When adding new tests:
1. Follow existing naming conventions (TEST-ID: Description)
2. Add appropriate priority tags
3. Include comprehensive documentation
4. Update this README with test counts
5. Run verification script

---

**Last Updated**: 2025-11-01
**Maintainer**: Ladybird Browser Test Team
