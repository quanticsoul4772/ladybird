# Rendering Tests

Comprehensive HTML/CSS rendering validation tests for Ladybird browser.

## Overview

This test suite validates HTML and CSS rendering compliance across:
- **HTML Elements** (15 tests): Semantic elements, forms, tables, media, etc.
- **CSS Layout** (20 tests): Flexbox, Grid, positioning, box model, etc.
- **CSS Visual** (15 tests): Colors, backgrounds, shadows, animations, filters, etc.
- **Responsive Design** (10 tests): Media queries, viewport, responsive images, etc.

**Total: 60 rendering tests**

## Test Files

### `html-elements.spec.ts`
Tests HTML element rendering and behavior:
- HTML-001: Semantic elements (header, nav, main, article, footer)
- HTML-002: Headings hierarchy (h1-h6)
- HTML-003: Text formatting (strong, em, mark, code, etc.)
- HTML-004: Lists (ul, ol, dl)
- HTML-005: Basic tables
- HTML-006: Complex tables (colspan, rowspan)
- HTML-007: Form input types
- HTML-008: Links and anchors
- HTML-009: Images (src, alt, loading)
- HTML-010: Embedded content (iframe)
- HTML-011: Audio element
- HTML-012: Video element
- HTML-013: Details/summary disclosure
- HTML-014: Dialog element
- HTML-015: Custom data attributes

### `css-layout.spec.ts`
Tests CSS layout capabilities:
- CSS-001: Flexbox basic layout
- CSS-002: Flexbox wrap and alignment
- CSS-003: CSS Grid basic layout
- CSS-004: CSS Grid areas and gaps
- CSS-005: Float-based layout
- CSS-006: Absolute positioning
- CSS-007: Fixed positioning
- CSS-008: Sticky positioning
- CSS-009: Z-index stacking
- CSS-010: Display modes (block, inline, inline-block)
- CSS-011: Box model (margin, border, padding)
- CSS-012: Width/height constraints
- CSS-013: Overflow handling
- CSS-014: Multi-column layout
- CSS-015: Responsive breakpoints
- CSS-016: Viewport units (vw, vh)
- CSS-017: Calc() expressions
- CSS-018: CSS variables (custom properties)
- CSS-019: Container queries
- CSS-020: Aspect ratio

### `css-visual.spec.ts`
Tests CSS visual styling:
- VIS-001: Colors (hex, rgb, hsl, named)
- VIS-002: Backgrounds (color, image, gradient)
- VIS-003: Borders (width, style, color, radius)
- VIS-004: Shadows (box-shadow, text-shadow)
- VIS-005: Opacity and transparency
- VIS-006: Transforms (translate, rotate, scale)
- VIS-007: Transitions
- VIS-008: Animations (keyframes)
- VIS-009: Filters (blur, grayscale, etc.)
- VIS-010: Blend modes
- VIS-011: Fonts (family, size, weight, style)
- VIS-012: Text properties (align, decoration, transform)
- VIS-013: Line height and letter spacing
- VIS-014: Pseudo-elements (::before, ::after)
- VIS-015: Pseudo-classes (:hover, :focus, :active)

### `responsive.spec.ts`
Tests responsive design features:
- RESP-001: Viewport meta tag
- RESP-002: Media queries (width)
- RESP-003: Media queries (orientation)
- RESP-004: Media queries (prefers-color-scheme)
- RESP-005: Responsive images (srcset)
- RESP-006: Picture element
- RESP-007: Fluid layouts
- RESP-008: Mobile navigation patterns
- RESP-009: Touch target sizes
- RESP-010: Responsive typography

## Test Helpers

### `helpers/visual-test-helper.ts`
Utility classes for visual testing:
- `VisualTestHelper`: Screenshot comparison, style verification, dimension validation
- `VIEWPORTS`: Common viewport sizes (mobile, tablet, desktop)
- `TestDataURL`: Generate test data URLs (SVG, HTML)

## Running Tests

### Run all rendering tests
```bash
cd Tests/Playwright
npm test tests/rendering/
```

### Run specific test file
```bash
npm test tests/rendering/html-elements.spec.ts
npm test tests/rendering/css-layout.spec.ts
npm test tests/rendering/css-visual.spec.ts
npm test tests/rendering/responsive.spec.ts
```

### Run tests with specific tag
```bash
npm test -- --grep @p0
```

### Run tests in headed mode (see browser)
```bash
npm test tests/rendering/ -- --headed
```

### Run tests with debug output
```bash
DEBUG=pw:api npm test tests/rendering/
```

## Test Strategy

### 1. DOM Validation
Tests verify correct DOM structure:
- Elements render with correct tag names
- Attributes are set correctly
- Parent-child relationships are preserved

### 2. Style Validation
Tests verify computed styles:
- CSS properties are applied correctly
- Cascade and specificity work as expected
- Browser default styles are reasonable

### 3. Visual Validation
Tests verify visual appearance:
- Elements are visible in viewport
- Dimensions match expectations
- Positioning is correct

### 4. Responsive Validation
Tests verify responsive behavior:
- Media queries apply at correct breakpoints
- Viewport changes trigger layout updates
- Touch targets meet accessibility guidelines

## Test Data

Tests use inline data URLs to avoid external dependencies:
- **Images**: SVG data URLs with configurable dimensions/colors
- **HTML**: Inline HTML via `data:text/html` URLs
- **Media**: Minimal valid audio/video data URLs

## Writing New Tests

### Example Test Structure
```typescript
test('TEST-ID: Test description', { tag: '@p0' }, async ({ page }) => {
  // 1. Create test HTML
  const html = `
    <!DOCTYPE html>
    <html>
      <head><style>/* CSS here */</style></head>
      <body><!-- HTML here --></body>
    </html>
  `;

  // 2. Load test page
  await page.goto(`data:text/html,${encodeURIComponent(html)}`);

  // 3. Verify element is visible
  await expect(page.locator('#element')).toBeVisible();

  // 4. Verify computed styles
  const style = await page.locator('#element').evaluate(el => {
    return window.getComputedStyle(el).propertyName;
  });
  expect(style).toBe('expected-value');

  // 5. Verify behavior (if applicable)
  await page.click('#element');
  // ... assertions
});
```

### Using Visual Test Helper
```typescript
import { VisualTestHelper, VIEWPORTS } from './helpers/visual-test-helper';

test('Responsive test', async ({ page }) => {
  const helper = new VisualTestHelper(page);

  // Verify styles
  await helper.verifyStyles('#element', {
    display: 'flex',
    justifyContent: 'center'
  });

  // Verify dimensions
  await helper.verifyDimensions('#element', {
    width: 200,
    height: 100
  });

  // Test across viewports
  await helper.verifyResponsiveBehavior(async () => {
    await expect(page.locator('#element')).toBeVisible();
  }, [VIEWPORTS.mobile, VIEWPORTS.tablet, VIEWPORTS.desktop]);
});
```

## Known Limitations

1. **Browser Differences**: Some CSS features may have different computed values across browsers
2. **Font Rendering**: Font metrics may vary by system
3. **Color Precision**: RGB values may have slight rounding differences
4. **Animation Timing**: Transitions/animations tested by verifying properties, not visual motion
5. **Pseudo-classes**: :hover/:active states verified by checking CSS rules, actual interaction may vary

## Future Enhancements

- [ ] Visual regression testing with pixelmatch
- [ ] Accessibility tree validation
- [ ] Performance metrics (paint timing, layout shift)
- [ ] Cross-browser comparison reports
- [ ] Test page fixtures for complex layouts
- [ ] Animation recording and playback verification
- [ ] WebGL rendering tests
- [ ] SVG rendering tests
- [ ] Canvas 2D API rendering tests

## Related Documentation

- **TEST_MATRIX.md**: Complete test matrix for all Ladybird tests
- **IMPLEMENTATION_STATUS.md**: Current implementation status
- **playwright.config.ts**: Playwright configuration
- **../core-browser/**: Core browser functionality tests
- **../security/**: Security feature tests (Sentinel)

## Maintenance

### Updating Tests
When updating rendering tests:
1. Ensure test IDs match TEST_MATRIX.md
2. Keep inline HTML/CSS minimal but complete
3. Add comments explaining complex CSS
4. Verify tests pass on multiple viewport sizes
5. Update this README if adding new test categories

### Debugging Failures
1. Run test with `--headed` to see browser
2. Use `page.screenshot()` to capture failure state
3. Check `test-results/` directory for artifacts
4. Verify computed styles match expectations
5. Check browser console for errors

## Contact

For questions about rendering tests:
- See **CLAUDE.md** for Ladybird development guidance
- Check **Documentation/Testing.md** for general test guidelines
- Review **Documentation/LibWebPatterns.md** for LibWeb specifics
