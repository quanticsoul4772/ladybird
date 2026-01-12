---
name: playwright-automation
description: Comprehensive browser automation testing for Ladybird using Playwright MCP tools, including visual regression, accessibility testing, and end-to-end test patterns
use-when: Writing browser automation tests, creating E2E test suites, visual regression testing, accessibility validation, debugging browser behavior, or automating user workflows
allowed-tools:
  - mcp__playwright__browser_navigate
  - mcp__playwright__browser_snapshot
  - mcp__playwright__browser_click
  - mcp__playwright__browser_type
  - mcp__playwright__browser_take_screenshot
  - mcp__playwright__browser_fill_form
  - mcp__playwright__browser_wait_for
  - mcp__playwright__browser_evaluate
  - mcp__playwright__browser_tabs
  - mcp__playwright__browser_console_messages
  - mcp__playwright__browser_network_requests
  - Bash
  - Read
  - Write
  - Grep
---

# Playwright Automation for Ladybird Browser

## Overview

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Write Test  │ → │  Run Against │ → │  Capture     │ → │  Assert &    │
│  Scenario    │    │  Ladybird    │    │  Evidence    │    │  Report      │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
        ↓                   ↓                   ↓                   ↓
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Page Objects │    │  Navigation  │    │ Screenshots  │    │  Regression  │
│  & Fixtures  │    │  & Actions   │    │  & A11y Tree │    │  Detection   │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

## 1. Playwright MCP Tools Reference

### Navigation & Page State

**browser_navigate** - Navigate to a URL
```javascript
// Usage
mcp__playwright__browser_navigate({ url: "https://example.com" })
mcp__playwright__browser_navigate({ url: "file:///path/to/test.html" })
```

**browser_navigate_back** - Go back to previous page
```javascript
mcp__playwright__browser_navigate_back({})
```

**browser_snapshot** - Capture accessibility tree (better than screenshot for automation)
```javascript
// Returns structured accessibility tree with interactive elements
// This is the PRIMARY way to understand page state
const snapshot = mcp__playwright__browser_snapshot({})
```

**browser_take_screenshot** - Capture visual screenshot
```javascript
// Full page screenshot
mcp__playwright__browser_take_screenshot({
  fullPage: true,
  filename: "test-result.png"
})

// Element screenshot
mcp__playwright__browser_take_screenshot({
  element: "Submit button",
  ref: "element_ref_from_snapshot",
  filename: "button-state.png"
})
```

### User Interactions

**browser_click** - Click an element
```javascript
mcp__playwright__browser_click({
  element: "Login button",  // Human-readable description
  ref: "button[role='button']",  // Element ref from snapshot
  button: "left",  // Optional: left (default), right, middle
  doubleClick: false  // Optional: double-click
})
```

**browser_type** - Type text into an input
```javascript
mcp__playwright__browser_type({
  element: "Username input",
  ref: "input[name='username']",
  text: "test_user",
  slowly: false,  // Optional: type character-by-character
  submit: false   // Optional: press Enter after typing
})
```

**browser_fill_form** - Fill multiple form fields at once
```javascript
mcp__playwright__browser_fill_form({
  fields: [
    {
      name: "Username",
      type: "textbox",
      ref: "input[name='username']",
      value: "test_user"
    },
    {
      name: "Remember me",
      type: "checkbox",
      ref: "input[name='remember']",
      value: "true"
    },
    {
      name: "Country",
      type: "combobox",
      ref: "select[name='country']",
      value: "United States"
    }
  ]
})
```

**browser_select_option** - Select dropdown option
```javascript
mcp__playwright__browser_select_option({
  element: "Country dropdown",
  ref: "select[name='country']",
  values: ["us"]  // Can be multiple for multi-select
})
```

**browser_hover** - Hover over element
```javascript
mcp__playwright__browser_hover({
  element: "Tooltip trigger",
  ref: "div.tooltip-trigger"
})
```

**browser_drag** - Drag and drop
```javascript
mcp__playwright__browser_drag({
  startElement: "Draggable item",
  startRef: "div.draggable",
  endElement: "Drop zone",
  endRef: "div.dropzone"
})
```

**browser_press_key** - Press keyboard key
```javascript
mcp__playwright__browser_press_key({ key: "Enter" })
mcp__playwright__browser_press_key({ key: "Escape" })
mcp__playwright__browser_press_key({ key: "ArrowDown" })
```

### Waiting & Timing

**browser_wait_for** - Wait for conditions
```javascript
// Wait for text to appear
mcp__playwright__browser_wait_for({ text: "Loading complete" })

// Wait for text to disappear
mcp__playwright__browser_wait_for({ textGone: "Loading..." })

// Wait for time
mcp__playwright__browser_wait_for({ time: 2 })  // seconds
```

### Tab Management

**browser_tabs** - Manage browser tabs
```javascript
// List all tabs
mcp__playwright__browser_tabs({ action: "list" })

// Create new tab
mcp__playwright__browser_tabs({ action: "new" })

// Switch to tab by index
mcp__playwright__browser_tabs({ action: "select", index: 0 })

// Close current tab or specific tab
mcp__playwright__browser_tabs({ action: "close" })
mcp__playwright__browser_tabs({ action: "close", index: 1 })
```

### Debugging & Diagnostics

**browser_console_messages** - Get console logs
```javascript
// All console messages
mcp__playwright__browser_console_messages({})

// Only errors
mcp__playwright__browser_console_messages({ onlyErrors: true })
```

**browser_network_requests** - Get network activity
```javascript
// All network requests since page load
const requests = mcp__playwright__browser_network_requests({})
```

**browser_evaluate** - Execute JavaScript
```javascript
// On page
mcp__playwright__browser_evaluate({
  function: "() => document.title"
})

// On element
mcp__playwright__browser_evaluate({
  element: "Canvas element",
  ref: "canvas#fingerprint",
  function: "(element) => element.toDataURL()"
})
```

**browser_handle_dialog** - Handle alert/confirm/prompt
```javascript
// Accept dialog
mcp__playwright__browser_handle_dialog({ accept: true })

// Dismiss dialog
mcp__playwright__browser_handle_dialog({ accept: false })

// Prompt with text
mcp__playwright__browser_handle_dialog({
  accept: true,
  promptText: "test input"
})
```

## 2. Test Structure & Organization

### Recommended Test Directory Structure

```
Tests/Playwright/
├── e2e/                          # End-to-end user flows
│   ├── login-flow.test.js
│   ├── checkout-flow.test.js
│   └── search-flow.test.js
├── visual/                       # Visual regression tests
│   ├── homepage.visual.js
│   ├── mobile-responsive.visual.js
│   └── dark-mode.visual.js
├── accessibility/                # A11y tests
│   ├── keyboard-navigation.a11y.js
│   ├── screen-reader.a11y.js
│   └── aria-compliance.a11y.js
├── security/                     # Fork-specific security tests
│   ├── fingerprinting-detection.test.js
│   ├── phishing-detection.test.js
│   ├── credential-protection.test.js
│   └── malware-scanning.test.js
├── performance/                  # Performance tests
│   ├── load-time.perf.js
│   └── rendering-benchmark.perf.js
├── fixtures/                     # Reusable test fixtures
│   ├── auth-fixture.js
│   ├── mock-data-fixture.js
│   └── page-objects.js
└── helpers/                      # Test utilities
    ├── assertions.js
    ├── wait-strategies.js
    └── visual-diff.js
```

### Test Naming Conventions

```javascript
// Pattern: [feature].[type].js
// Examples:
// - login-flow.test.js (E2E test)
// - homepage.visual.js (Visual regression)
// - navigation.a11y.js (Accessibility)
// - api-endpoint.perf.js (Performance)
```

## 3. Common Test Patterns

### Pattern 1: Basic Navigation & Assertion

```javascript
/**
 * Test: Basic page navigation and content verification
 */
async function testBasicNavigation() {
  // 1. Navigate to page
  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  // 2. Wait for page load
  await mcp__playwright__browser_wait_for({
    text: "Example Domain"
  });

  // 3. Capture accessibility snapshot
  const snapshot = await mcp__playwright__browser_snapshot({});

  // 4. Assert page state
  assert(snapshot.includes("Example Domain"), "Page title not found");
  assert(snapshot.includes("More information..."), "Expected link missing");

  // 5. Take screenshot for evidence
  await mcp__playwright__browser_take_screenshot({
    fullPage: true,
    filename: "basic-navigation-success.png"
  });

  return { status: "PASS", evidence: "basic-navigation-success.png" };
}
```

### Pattern 2: Form Interaction & Submission

```javascript
/**
 * Test: Fill and submit a form
 */
async function testFormSubmission() {
  // 1. Navigate to form page
  await mcp__playwright__browser_navigate({
    url: "file:///home/user/test-form.html"
  });

  // 2. Fill form fields
  await mcp__playwright__browser_fill_form({
    fields: [
      {
        name: "Username",
        type: "textbox",
        ref: "input#username",
        value: "test_user"
      },
      {
        name: "Password",
        type: "textbox",
        ref: "input#password",
        value: "secure_password"
      },
      {
        name: "Remember me",
        type: "checkbox",
        ref: "input#remember",
        value: "true"
      }
    ]
  });

  // 3. Take screenshot before submission
  await mcp__playwright__browser_take_screenshot({
    filename: "form-filled.png"
  });

  // 4. Submit form
  await mcp__playwright__browser_click({
    element: "Submit button",
    ref: "button[type='submit']"
  });

  // 5. Wait for result
  await mcp__playwright__browser_wait_for({
    text: "Login successful"
  });

  // 6. Verify success
  const snapshot = await mcp__playwright__browser_snapshot({});
  assert(snapshot.includes("Welcome"), "Login success message not found");

  return { status: "PASS" };
}
```

### Pattern 3: Visual Regression Testing

```javascript
/**
 * Test: Visual regression with baseline comparison
 */
async function testVisualRegression() {
  // 1. Navigate to page
  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  // 2. Wait for page stability
  await mcp__playwright__browser_wait_for({ time: 1 });

  // 3. Take current screenshot
  await mcp__playwright__browser_take_screenshot({
    fullPage: true,
    filename: "current-screenshot.png"
  });

  // 4. Compare with baseline (external utility)
  const diffResult = await compareWithBaseline(
    "current-screenshot.png",
    "baseline-screenshots/homepage.png"
  );

  // 5. If differences found, save diff image
  if (diffResult.pixelDifference > 0.1) {  // 0.1% threshold
    await saveDiffImage(diffResult.diffBuffer, "visual-diff.png");
    return {
      status: "FAIL",
      reason: `Visual regression detected: ${diffResult.pixelDifference}%`,
      evidence: "visual-diff.png"
    };
  }

  return { status: "PASS", pixelDifference: diffResult.pixelDifference };
}
```

### Pattern 4: Accessibility Testing

```javascript
/**
 * Test: Accessibility snapshot validation
 */
async function testAccessibility() {
  // 1. Navigate to page
  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  // 2. Capture accessibility tree
  const snapshot = await mcp__playwright__browser_snapshot({});

  // 3. Parse and validate
  const violations = [];

  // Check for images without alt text
  if (snapshot.match(/img\s+(?![^>]*alt=)/gi)) {
    violations.push("Images without alt text found");
  }

  // Check for buttons without labels
  if (snapshot.match(/button\s+(?![^>]*aria-label=)/gi)) {
    violations.push("Buttons without aria-label found");
  }

  // Check for proper heading hierarchy
  const headings = snapshot.match(/<h[1-6]/gi) || [];
  if (headings[0] !== "<h1") {
    violations.push("Page doesn't start with h1");
  }

  // 4. Test keyboard navigation
  const keyboardTest = await testKeyboardNavigation();
  if (!keyboardTest.success) {
    violations.push("Keyboard navigation failed");
  }

  if (violations.length > 0) {
    return {
      status: "FAIL",
      violations: violations
    };
  }

  return { status: "PASS" };
}

async function testKeyboardNavigation() {
  // Tab through interactive elements
  await mcp__playwright__browser_press_key({ key: "Tab" });
  await mcp__playwright__browser_press_key({ key: "Tab" });
  await mcp__playwright__browser_press_key({ key: "Enter" });

  const snapshot = await mcp__playwright__browser_snapshot({});
  return { success: snapshot.includes("expected-result") };
}
```

### Pattern 5: Multi-Tab Testing

```javascript
/**
 * Test: Multi-tab workflows
 */
async function testMultiTab() {
  // 1. Open first tab
  await mcp__playwright__browser_navigate({
    url: "https://example.com/page1"
  });

  const page1Snapshot = await mcp__playwright__browser_snapshot({});

  // 2. Open new tab
  await mcp__playwright__browser_tabs({ action: "new" });

  // 3. Navigate in new tab
  await mcp__playwright__browser_navigate({
    url: "https://example.com/page2"
  });

  const page2Snapshot = await mcp__playwright__browser_snapshot({});

  // 4. List all tabs
  const tabs = await mcp__playwright__browser_tabs({ action: "list" });
  assert(tabs.length === 2, "Expected 2 tabs");

  // 5. Switch back to first tab
  await mcp__playwright__browser_tabs({ action: "select", index: 0 });

  // 6. Verify we're on page 1
  const currentSnapshot = await mcp__playwright__browser_snapshot({});
  assert(currentSnapshot.includes("page1-content"), "Not on page 1");

  // 7. Close second tab
  await mcp__playwright__browser_tabs({ action: "close", index: 1 });

  return { status: "PASS" };
}
```

### Pattern 6: Error Condition Testing

```javascript
/**
 * Test: Error handling and edge cases
 */
async function testErrorHandling() {
  const results = [];

  // Test 1: 404 page
  await mcp__playwright__browser_navigate({
    url: "https://example.com/nonexistent"
  });

  await mcp__playwright__browser_wait_for({ text: "404" });
  const snapshot404 = await mcp__playwright__browser_snapshot({});
  results.push({
    test: "404 page",
    pass: snapshot404.includes("Not Found")
  });

  // Test 2: Network timeout
  await mcp__playwright__browser_navigate({
    url: "https://httpstat.us/200?sleep=60000"  // Simulate timeout
  });

  // Should show error or timeout message
  await mcp__playwright__browser_wait_for({ time: 5 });
  const timeoutSnapshot = await mcp__playwright__browser_snapshot({});
  results.push({
    test: "Network timeout",
    pass: timeoutSnapshot.includes("timeout") || timeoutSnapshot.includes("error")
  });

  // Test 3: JavaScript error
  await mcp__playwright__browser_navigate({
    url: "file:///home/user/js-error-test.html"
  });

  const consoleMessages = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });
  results.push({
    test: "JavaScript error captured",
    pass: consoleMessages.length > 0
  });

  // Evaluate results
  const allPassed = results.every(r => r.pass);
  return {
    status: allPassed ? "PASS" : "FAIL",
    results: results
  };
}
```

### Pattern 7: Network Interception & Validation

```javascript
/**
 * Test: Network request monitoring
 */
async function testNetworkRequests() {
  // 1. Navigate to page
  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  // 2. Wait for page load
  await mcp__playwright__browser_wait_for({ time: 2 });

  // 3. Get all network requests
  const requests = await mcp__playwright__browser_network_requests({});

  // 4. Analyze requests
  const analysis = {
    totalRequests: requests.length,
    httpRequests: requests.filter(r => r.url.startsWith("http:")).length,
    httpsRequests: requests.filter(r => r.url.startsWith("https:")).length,
    thirdParty: requests.filter(r => !r.url.includes("example.com")).length,
    images: requests.filter(r => r.url.match(/\.(png|jpg|gif|webp)$/)).length,
    scripts: requests.filter(r => r.url.match(/\.js$/)).length,
    stylesheets: requests.filter(r => r.url.match(/\.css$/)).length
  };

  // 5. Assert expectations
  assert(analysis.httpsRequests > 0, "No HTTPS requests found");
  assert(analysis.httpRequests === 0, "Insecure HTTP requests detected");

  return { status: "PASS", analysis: analysis };
}
```

## 4. Ladybird Fork-Specific Test Patterns

### Pattern 8: Fingerprinting Detection Test

```javascript
/**
 * Test: Canvas fingerprinting detection in Ladybird fork
 */
async function testFingerprintingDetection() {
  // 1. Navigate to fingerprinting test page
  await mcp__playwright__browser_navigate({
    url: "file:///home/rbsmith4/ladybird/test_canvas_fingerprinting.html"
  });

  // 2. Wait for fingerprinting script to execute
  await mcp__playwright__browser_wait_for({ time: 2 });

  // 3. Check console for detection warnings
  const consoleMessages = await mcp__playwright__browser_console_messages({});

  const detectionTriggered = consoleMessages.some(msg =>
    msg.includes("Fingerprinting detected") ||
    msg.includes("Canvas API monitoring")
  );

  // 4. Verify detection UI appeared
  const snapshot = await mcp__playwright__browser_snapshot({});
  const alertShown = snapshot.includes("Privacy Alert") ||
                     snapshot.includes("Fingerprinting detected");

  // 5. Take screenshot of detection
  await mcp__playwright__browser_take_screenshot({
    filename: "fingerprinting-detection.png"
  });

  if (!detectionTriggered && !alertShown) {
    return {
      status: "FAIL",
      reason: "Fingerprinting detection did not trigger"
    };
  }

  return {
    status: "PASS",
    detectionTriggered: detectionTriggered,
    alertShown: alertShown
  };
}
```

### Pattern 9: Credential Protection Test

```javascript
/**
 * Test: Cross-origin credential submission protection
 */
async function testCredentialProtection() {
  // 1. Navigate to login form
  await mcp__playwright__browser_navigate({
    url: "https://example.com/login"
  });

  // 2. Fill credentials
  await mcp__playwright__browser_fill_form({
    fields: [
      {
        name: "Username",
        type: "textbox",
        ref: "input[name='username']",
        value: "test_user"
      },
      {
        name: "Password",
        type: "textbox",
        ref: "input[name='password']",
        value: "test_password"
      }
    ]
  });

  // 3. Modify form action to cross-origin (simulate phishing)
  await mcp__playwright__browser_evaluate({
    function: `() => {
      const form = document.querySelector('form');
      form.action = 'https://evil.com/steal';
    }`
  });

  // 4. Submit form
  await mcp__playwright__browser_click({
    element: "Submit button",
    ref: "button[type='submit']"
  });

  // 5. Check if protection triggered
  await mcp__playwright__browser_wait_for({ time: 1 });
  const snapshot = await mcp__playwright__browser_snapshot({});

  const protectionTriggered = snapshot.includes("Security Warning") ||
                              snapshot.includes("Credential Protection") ||
                              snapshot.includes("untrusted destination");

  if (!protectionTriggered) {
    return {
      status: "FAIL",
      reason: "Credential protection did not trigger for cross-origin submission"
    };
  }

  return { status: "PASS" };
}
```

### Pattern 10: Phishing Detection Test

```javascript
/**
 * Test: URL phishing detection
 */
async function testPhishingDetection() {
  const testURLs = [
    { url: "https://paypal-secure-login.com", shouldBlock: true },
    { url: "https://www.paypa1.com", shouldBlock: true },  // Homograph
    { url: "https://paypal.com", shouldBlock: false },
    { url: "https://google-login.xyz", shouldBlock: true }
  ];

  const results = [];

  for (const testCase of testURLs) {
    // 1. Navigate to URL
    await mcp__playwright__browser_navigate({ url: testCase.url });

    // 2. Wait for detection
    await mcp__playwright__browser_wait_for({ time: 2 });

    // 3. Check for warning
    const snapshot = await mcp__playwright__browser_snapshot({});
    const warningShown = snapshot.includes("Phishing") ||
                         snapshot.includes("Suspicious URL") ||
                         snapshot.includes("Security Warning");

    // 4. Verify expected behavior
    const passed = (testCase.shouldBlock && warningShown) ||
                   (!testCase.shouldBlock && !warningShown);

    results.push({
      url: testCase.url,
      expectedBlock: testCase.shouldBlock,
      actualBlock: warningShown,
      passed: passed
    });
  }

  const allPassed = results.every(r => r.passed);
  return {
    status: allPassed ? "PASS" : "FAIL",
    results: results
  };
}
```

## 5. Test Fixtures & Page Objects

### Fixture Pattern: Authentication

```javascript
/**
 * Reusable authentication fixture
 */
class AuthFixture {
  async login(username, password) {
    await mcp__playwright__browser_navigate({
      url: "https://example.com/login"
    });

    await mcp__playwright__browser_fill_form({
      fields: [
        {
          name: "Username",
          type: "textbox",
          ref: "input[name='username']",
          value: username
        },
        {
          name: "Password",
          type: "textbox",
          ref: "input[name='password']",
          value: password
        }
      ]
    });

    await mcp__playwright__browser_click({
      element: "Login button",
      ref: "button[type='submit']"
    });

    await mcp__playwright__browser_wait_for({
      text: "Welcome"
    });

    return { authenticated: true };
  }

  async logout() {
    await mcp__playwright__browser_click({
      element: "Logout link",
      ref: "a#logout"
    });

    await mcp__playwright__browser_wait_for({
      text: "Logged out"
    });

    return { authenticated: false };
  }
}
```

### Page Object Pattern

```javascript
/**
 * Page Object for Login page
 */
class LoginPage {
  constructor() {
    this.url = "https://example.com/login";
    this.selectors = {
      username: "input[name='username']",
      password: "input[name='password']",
      submitButton: "button[type='submit']",
      errorMessage: "div.error"
    };
  }

  async navigate() {
    await mcp__playwright__browser_navigate({ url: this.url });
    await mcp__playwright__browser_wait_for({ text: "Login" });
  }

  async fillUsername(username) {
    await mcp__playwright__browser_type({
      element: "Username input",
      ref: this.selectors.username,
      text: username
    });
  }

  async fillPassword(password) {
    await mcp__playwright__browser_type({
      element: "Password input",
      ref: this.selectors.password,
      text: password
    });
  }

  async submit() {
    await mcp__playwright__browser_click({
      element: "Submit button",
      ref: this.selectors.submitButton
    });
  }

  async login(username, password) {
    await this.navigate();
    await this.fillUsername(username);
    await this.fillPassword(password);
    await this.submit();
  }

  async getErrorMessage() {
    const snapshot = await mcp__playwright__browser_snapshot({});
    const match = snapshot.match(/div\.error[^>]*>([^<]+)</);
    return match ? match[1] : null;
  }
}
```

## 6. Debugging Failed Tests

### Strategy 1: Progressive Screenshots

```javascript
/**
 * Take screenshots at each step for debugging
 */
async function debuggableTest() {
  let step = 0;

  try {
    // Step 1
    await mcp__playwright__browser_navigate({ url: "https://example.com" });
    await mcp__playwright__browser_take_screenshot({
      filename: `debug-step-${++step}.png`
    });

    // Step 2
    await mcp__playwright__browser_click({
      element: "Login button",
      ref: "button#login"
    });
    await mcp__playwright__browser_take_screenshot({
      filename: `debug-step-${++step}.png`
    });

    // Step 3
    await mcp__playwright__browser_wait_for({ text: "Welcome" });
    await mcp__playwright__browser_take_screenshot({
      filename: `debug-step-${++step}.png`
    });

  } catch (error) {
    // Capture failure state
    await mcp__playwright__browser_take_screenshot({
      filename: `debug-FAILURE-step-${step}.png`
    });

    const snapshot = await mcp__playwright__browser_snapshot({});
    const consoleMessages = await mcp__playwright__browser_console_messages({
      onlyErrors: true
    });

    return {
      status: "FAIL",
      failedAtStep: step,
      error: error.message,
      snapshot: snapshot,
      consoleErrors: consoleMessages
    };
  }
}
```

### Strategy 2: Console & Network Logs

```javascript
/**
 * Capture comprehensive debugging information
 */
async function captureDebugInfo() {
  // Get console messages
  const allMessages = await mcp__playwright__browser_console_messages({});
  const errors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  // Get network requests
  const requests = await mcp__playwright__browser_network_requests({});

  // Get page snapshot
  const snapshot = await mcp__playwright__browser_snapshot({});

  // Get screenshot
  await mcp__playwright__browser_take_screenshot({
    fullPage: true,
    filename: "debug-state.png"
  });

  return {
    consoleMessages: allMessages.length,
    consoleErrors: errors.length,
    networkRequests: requests.length,
    failedRequests: requests.filter(r => r.status >= 400).length,
    snapshot: snapshot
  };
}
```

### Strategy 3: Slow Motion Execution

```javascript
/**
 * Add delays between actions for debugging
 */
async function slowMotionTest() {
  const DELAY = 1000; // 1 second delay between actions

  await mcp__playwright__browser_navigate({ url: "https://example.com" });
  await mcp__playwright__browser_wait_for({ time: DELAY / 1000 });

  await mcp__playwright__browser_click({
    element: "Button",
    ref: "button#action"
  });
  await mcp__playwright__browser_wait_for({ time: DELAY / 1000 });

  await mcp__playwright__browser_type({
    element: "Input",
    ref: "input#text",
    text: "test",
    slowly: true  // Type character-by-character
  });
  await mcp__playwright__browser_wait_for({ time: DELAY / 1000 });
}
```

## 7. CI/CD Integration

### Running Tests in GitHub Actions

```yaml
# .github/workflows/playwright-tests.yml
name: Playwright E2E Tests

on:
  push:
    branches: [master, main]
  pull_request:
    branches: [master, main]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Build Ladybird
        run: |
          cmake --preset Release
          cmake --build Build/release

      - name: Start Ladybird
        run: |
          ./Build/release/bin/Ladybird &
          sleep 5  # Wait for browser to start

      - name: Run Playwright tests
        run: |
          node Tests/Playwright/run-all-tests.js

      - name: Upload screenshots on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: test-failure-screenshots
          path: Tests/Playwright/screenshots/
          retention-days: 30

      - name: Upload test reports
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-reports
          path: Tests/Playwright/reports/
```

### Test Runner Script

```javascript
/**
 * Tests/Playwright/run-all-tests.js
 * Master test runner
 */
const tests = [
  require('./e2e/login-flow.test.js'),
  require('./e2e/navigation.test.js'),
  require('./visual/homepage.visual.js'),
  require('./accessibility/keyboard-nav.a11y.js'),
  require('./security/fingerprinting-detection.test.js')
];

async function runAllTests() {
  const results = [];

  for (const test of tests) {
    console.log(`Running: ${test.name}`);

    const startTime = Date.now();
    const result = await test.run();
    const duration = Date.now() - startTime;

    results.push({
      name: test.name,
      status: result.status,
      duration: duration,
      details: result
    });

    console.log(`  ${result.status} (${duration}ms)`);
  }

  // Generate report
  const passed = results.filter(r => r.status === "PASS").length;
  const failed = results.filter(r => r.status === "FAIL").length;

  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${passed}`);
  console.log(`Failed: ${failed}`);
  console.log(`Total: ${results.length}`);

  // Exit with error code if any failed
  process.exit(failed > 0 ? 1 : 0);
}

runAllTests();
```

## 8. Best Practices

### 1. Use Accessibility Snapshots Over Screenshots
```javascript
// GOOD: Fast, structured, easier to assert
const snapshot = await mcp__playwright__browser_snapshot({});
assert(snapshot.includes("Expected text"));

// OKAY: Slower, for visual regression only
const screenshot = await mcp__playwright__browser_take_screenshot({});
```

### 2. Wait for Conditions, Not Arbitrary Times
```javascript
// GOOD: Wait for specific condition
await mcp__playwright__browser_wait_for({ text: "Loading complete" });

// BAD: Arbitrary timeout, flaky
await mcp__playwright__browser_wait_for({ time: 5 });
```

### 3. Use Page Objects for Complex Pages
```javascript
// GOOD: Encapsulated, reusable
const loginPage = new LoginPage();
await loginPage.login("user", "pass");

// BAD: Repeated selectors, hard to maintain
await mcp__playwright__browser_type({ ref: "input[name='username']", ... });
await mcp__playwright__browser_type({ ref: "input[name='password']", ... });
```

### 4. Clean Up After Tests
```javascript
async function testWithCleanup() {
  try {
    // Test logic
    await runTest();
  } finally {
    // Always clean up
    await mcp__playwright__browser_tabs({ action: "close" });
  }
}
```

### 5. Make Tests Independent
```javascript
// GOOD: Each test starts fresh
async function testA() {
  await mcp__playwright__browser_navigate({ url: "..." });
  // Test A logic
}

async function testB() {
  await mcp__playwright__browser_navigate({ url: "..." });
  // Test B logic (doesn't depend on Test A)
}
```

### 6. Use Meaningful Assertions
```javascript
// GOOD: Clear failure message
assert(
  snapshot.includes("Login successful"),
  "Login success message not found after form submission"
);

// BAD: Unclear what failed
assert(result);
```

### 7. Capture Evidence for Failures
```javascript
if (testFailed) {
  await mcp__playwright__browser_take_screenshot({
    filename: `failure-${testName}-${Date.now()}.png`
  });

  const consoleErrors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  return {
    status: "FAIL",
    screenshot: "failure-...",
    consoleErrors: consoleErrors
  };
}
```

### 8. Test Multi-Process Behavior
```javascript
// Ladybird uses separate processes for WebContent, RequestServer, etc.
// Test process isolation and IPC
async function testProcessIsolation() {
  // Open multiple tabs (separate WebContent processes)
  await mcp__playwright__browser_tabs({ action: "new" });
  await mcp__playwright__browser_navigate({ url: "page1.html" });

  await mcp__playwright__browser_tabs({ action: "new" });
  await mcp__playwright__browser_navigate({ url: "page2.html" });

  // Crash one tab (should not affect others)
  await mcp__playwright__browser_evaluate({
    function: "() => { while(true) {} }"  // Infinite loop
  });

  // Other tabs should still work
  await mcp__playwright__browser_tabs({ action: "select", index: 0 });
  const snapshot = await mcp__playwright__browser_snapshot({});
  assert(snapshot.includes("page1"), "Other tabs affected by crash");
}
```

## 9. Performance Testing Patterns

### Load Time Measurement

```javascript
async function testPageLoadPerformance() {
  const startTime = Date.now();

  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  await mcp__playwright__browser_wait_for({
    text: "Page loaded"
  });

  const loadTime = Date.now() - startTime;

  // Assert performance threshold
  const MAX_LOAD_TIME = 3000; // 3 seconds
  if (loadTime > MAX_LOAD_TIME) {
    return {
      status: "FAIL",
      reason: `Page load too slow: ${loadTime}ms (max: ${MAX_LOAD_TIME}ms)`
    };
  }

  return { status: "PASS", loadTime: loadTime };
}
```

### Network Efficiency

```javascript
async function testNetworkEfficiency() {
  await mcp__playwright__browser_navigate({
    url: "https://example.com"
  });

  await mcp__playwright__browser_wait_for({ time: 2 });

  const requests = await mcp__playwright__browser_network_requests({});

  // Analyze efficiency
  const totalRequests = requests.length;
  const cachedRequests = requests.filter(r => r.cached).length;
  const compressedRequests = requests.filter(r => r.compressed).length;

  const efficiency = {
    totalRequests: totalRequests,
    cacheHitRate: cachedRequests / totalRequests,
    compressionRate: compressedRequests / totalRequests
  };

  // Assert thresholds
  assert(efficiency.cacheHitRate > 0.5, "Cache hit rate too low");
  assert(efficiency.compressionRate > 0.8, "Compression rate too low");

  return { status: "PASS", efficiency: efficiency };
}
```

## Checklist

- [ ] Test suite covers critical user flows
- [ ] Visual regression baselines established
- [ ] Accessibility tests for keyboard navigation
- [ ] Accessibility tests for ARIA compliance
- [ ] Security tests for fork features (fingerprinting, phishing, credentials)
- [ ] Error condition tests (404, timeout, network failure)
- [ ] Multi-tab tests for process isolation
- [ ] Performance thresholds defined and tested
- [ ] Test fixtures and page objects created for reusability
- [ ] CI/CD integration configured
- [ ] Screenshot capture on test failures
- [ ] Console and network logs captured for debugging
- [ ] Test independence verified (no shared state)
- [ ] Test documentation and naming conventions followed

## Related Skills

### Complementary Skills
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Combine Playwright E2E tests with WPT and LibWeb tests. Use Playwright for user-facing flows, WPT for spec compliance.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Complement fuzzing with E2E tests. Fuzzers find crashes, Playwright validates user-facing behavior and security features.

### Implementation Dependencies
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Understand Ladybird's architecture to write effective browser tests. Test WebContent, RequestServer, and Sentinel integration.
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Test multi-process behavior, IPC boundaries, and process isolation using Playwright multi-tab tests.

### Testing Integration
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Integrate Playwright tests into CI pipeline. Run tests on PR, upload screenshots on failure, gate merges on test pass.
- **[documentation-generation](../documentation-generation/SKILL.md)**: Auto-generate test documentation from test results. Create visual regression reports with screenshots.

### Security Testing
- **[ipc-security](../ipc-security/SKILL.md)**: Use Playwright to test IPC security from user perspective. Verify rate limiting, validation, and sandboxing.
- **[tor-integration](../tor-integration/SKILL.md)**: Test Tor integration with Playwright. Verify circuit isolation, leak prevention, and privacy features.

## Additional Resources

- Playwright MCP Documentation: Available through MCP tool descriptions
- Ladybird Testing Guide: `/home/rbsmith4/ladybird/Documentation/Testing.md`
- LibWeb Test Patterns: `/home/rbsmith4/ladybird/Documentation/LibWebPatterns.md`
- Fork Security Features: `/home/rbsmith4/ladybird/docs/FEATURES.md`
