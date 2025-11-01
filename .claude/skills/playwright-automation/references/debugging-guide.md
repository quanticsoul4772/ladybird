# Debugging Guide for Playwright Tests

Comprehensive guide to troubleshooting failed Playwright tests.

## Quick Diagnosis

### Symptom: Test fails intermittently

**Likely Causes:**
1. Race conditions (timing issues)
2. Network delays
3. Animation/transition timing
4. Shared state between tests

**Solutions:**
```javascript
// Use explicit waits instead of time-based waits
// BAD
await mcp__playwright__browser_wait_for({ time: 2 });

// GOOD
await mcp__playwright__browser_wait_for({
  text: "Expected content"
});

// Wait for network idle
const WaitHelpers = require('../helpers/wait-helpers');
await WaitHelpers.waitForNetworkIdle();
```

### Symptom: Element not found

**Likely Causes:**
1. Element hasn't loaded yet
2. Wrong selector
3. Element in iframe
4. Element hidden/removed

**Debug Steps:**
```javascript
// 1. Capture snapshot to see what's actually on page
const snapshot = await mcp__playwright__browser_snapshot({});
console.log(snapshot);

// 2. Take screenshot to visualize
await mcp__playwright__browser_take_screenshot({
  filename: "debug-element-not-found.png"
});

// 3. Wait for element explicitly
await mcp__playwright__browser_wait_for({
  text: "Expected element text"
});
```

### Symptom: Click doesn't work

**Likely Causes:**
1. Element not clickable (covered by another element)
2. Element not in viewport
3. Element disabled
4. JavaScript event not attached yet

**Debug Steps:**
```javascript
// 1. Scroll element into view first
await mcp__playwright__browser_hover({
  element: "Target element",
  ref: "button#target"
});

// 2. Wait before clicking
await mcp__playwright__browser_wait_for({ time: 0.5 });

// 3. Try double-click if single click fails
await mcp__playwright__browser_click({
  element: "Element",
  ref: "button",
  doubleClick: true
});
```

### Symptom: Form submission doesn't work

**Likely Causes:**
1. Validation errors
2. Required fields not filled
3. JavaScript not loaded
4. CSRF token issues

**Debug Steps:**
```javascript
// 1. Check console for errors
const errors = await mcp__playwright__browser_console_messages({
  onlyErrors: true
});
console.log("Console errors:", errors);

// 2. Check network for failed requests
const requests = await mcp__playwright__browser_network_requests({});
const failed = requests.filter(r => r.status >= 400);
console.log("Failed requests:", failed);

// 3. Take screenshot before submission
await mcp__playwright__browser_take_screenshot({
  filename: "before-submit.png"
});

// 4. Submit and wait
await mcp__playwright__browser_click({
  element: "Submit",
  ref: "button[type='submit']"
});
await mcp__playwright__browser_wait_for({ time: 2 });

// 5. Take screenshot after submission
await mcp__playwright__browser_take_screenshot({
  filename: "after-submit.png"
});
```

## Debugging Techniques

### 1. Progressive Screenshots

Capture screenshot after each step to see where failure occurs:

```javascript
async function debuggableTest() {
  let step = 0;

  try {
    console.log("Step 1: Navigate");
    await mcp__playwright__browser_navigate({ url: "..." });
    await screenshot(++step);

    console.log("Step 2: Fill form");
    await fillForm();
    await screenshot(++step);

    console.log("Step 3: Submit");
    await submit();
    await screenshot(++step);

    console.log("Step 4: Verify");
    await verify();
    await screenshot(++step);

  } catch (error) {
    await screenshot(`FAILURE-step-${step}`);
    throw error;
  }
}

async function screenshot(name) {
  await mcp__playwright__browser_take_screenshot({
    filename: `debug-step-${name}.png`
  });
}
```

### 2. Console Log Monitoring

Track console messages to detect JavaScript errors:

```javascript
async function monitorConsole() {
  // Before action
  const beforeMessages = await mcp__playwright__browser_console_messages({});
  const beforeErrors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  // Perform action
  await performAction();

  // After action
  const afterMessages = await mcp__playwright__browser_console_messages({});
  const afterErrors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  // Compare
  const newErrors = afterErrors.slice(beforeErrors.length);

  if (newErrors.length > 0) {
    console.error("New console errors detected:");
    newErrors.forEach(err => console.error("  ", err));
  }

  return newErrors;
}
```

### 3. Network Request Debugging

Monitor network activity to find failed requests:

```javascript
async function debugNetworkRequests() {
  // Perform action that triggers requests
  await mcp__playwright__browser_click({
    element: "Load Data",
    ref: "button#load"
  });

  await mcp__playwright__browser_wait_for({ time: 2 });

  // Analyze requests
  const requests = await mcp__playwright__browser_network_requests({});

  console.log(`Total requests: ${requests.length}`);

  // Failed requests
  const failed = requests.filter(r => r.status >= 400);
  if (failed.length > 0) {
    console.error(`Failed requests (${failed.length}):`);
    failed.forEach(req => {
      console.error(`  ${req.method} ${req.url} - ${req.status}`);
    });
  }

  // Slow requests
  const slow = requests.filter(r => r.duration > 1000);
  if (slow.length > 0) {
    console.warn(`Slow requests (${slow.length}):`);
    slow.forEach(req => {
      console.warn(`  ${req.url} - ${req.duration}ms`);
    });
  }

  // Group by domain
  const byDomain = {};
  requests.forEach(req => {
    const url = new URL(req.url);
    byDomain[url.hostname] = (byDomain[url.hostname] || 0) + 1;
  });

  console.log("Requests by domain:");
  Object.entries(byDomain).forEach(([domain, count]) => {
    console.log(`  ${domain}: ${count}`);
  });
}
```

### 4. Snapshot Diff

Compare snapshots before and after action:

```javascript
async function snapshotDiff() {
  const before = await mcp__playwright__browser_snapshot({});

  await performAction();

  const after = await mcp__playwright__browser_snapshot({});

  // Simple diff (in production, use proper diff library)
  if (before === after) {
    console.warn("Warning: No changes detected in snapshot");
  }

  // Check for specific changes
  const addedText = findNewText(before, after);
  const removedText = findRemovedText(before, after);

  console.log("Added text:", addedText);
  console.log("Removed text:", removedText);
}

function findNewText(before, after) {
  // Simplified - in production use proper diff algorithm
  const beforeLines = before.split('\n');
  const afterLines = after.split('\n');

  return afterLines.filter(line => !beforeLines.includes(line));
}

function findRemovedText(before, after) {
  const beforeLines = before.split('\n');
  const afterLines = after.split('\n');

  return beforeLines.filter(line => !afterLines.includes(line));
}
```

### 5. Slow Motion Execution

Add delays to watch test execution:

```javascript
async function slowMotionTest() {
  const DELAY = 1000; // 1 second between actions

  async function slowAction(fn, description) {
    console.log(`  ${description}...`);
    await fn();
    await mcp__playwright__browser_wait_for({ time: DELAY / 1000 });
    await mcp__playwright__browser_take_screenshot({
      filename: `slow-${description.replace(/\s+/g, '-')}.png`
    });
  }

  await slowAction(
    () => mcp__playwright__browser_navigate({ url: "..." }),
    "Navigate to page"
  );

  await slowAction(
    () => mcp__playwright__browser_click({ element: "Button", ref: "button" }),
    "Click button"
  );

  await slowAction(
    () => mcp__playwright__browser_type({
      element: "Input",
      ref: "input",
      text: "test"
    }),
    "Type text"
  );
}
```

## Common Issues and Solutions

### Issue: Timeout waiting for text

**Problem:** Text never appears on page

**Debug:**
```javascript
// 1. Check if text exists but with different case/whitespace
const snapshot = await mcp__playwright__browser_snapshot({});
console.log("Searching for:", expectedText);
console.log("Snapshot includes:", snapshot.includes(expectedText));
console.log("Snapshot (first 500 chars):", snapshot.substring(0, 500));

// 2. Use regex for flexible matching
const pattern = /loading|loaded|complete/i;
console.log("Regex match:", pattern.test(snapshot));

// 3. Wait longer or check for alternative text
await mcp__playwright__browser_wait_for({
  text: "Alternative text"
});
```

### Issue: Element selector not working

**Problem:** Element exists but selector doesn't match

**Debug:**
```javascript
// 1. Capture full snapshot
const snapshot = await mcp__playwright__browser_snapshot({});

// 2. Search for element in snapshot
console.log("Selector:", targetSelector);
console.log("Found in snapshot:", snapshot.includes(targetSelector));

// 3. Try alternative selectors
const alternatives = [
  "button#submit",
  "button[type='submit']",
  "input[type='submit']",
  "button.submit-button"
];

for (const selector of alternatives) {
  if (snapshot.includes(selector)) {
    console.log("Alternative selector works:", selector);
  }
}
```

### Issue: Form validation prevents submission

**Problem:** Form has validation errors

**Debug:**
```javascript
// 1. Check for validation messages
const snapshot = await mcp__playwright__browser_snapshot({});
const hasError = snapshot.includes("error") ||
                 snapshot.includes("required") ||
                 snapshot.includes("invalid");

if (hasError) {
  console.error("Validation errors detected");
  await mcp__playwright__browser_take_screenshot({
    filename: "validation-errors.png"
  });
}

// 2. Check field values
await mcp__playwright__browser_evaluate({
  function: `() => {
    const form = document.querySelector('form');
    const formData = new FormData(form);
    const data = {};
    formData.forEach((value, key) => { data[key] = value; });
    console.log('Form data:', data);
    return data;
  }`
});
```

### Issue: Test works locally but fails in CI

**Problem:** Environment differences

**Debug:**
```javascript
// 1. Log environment info
const env = await mcp__playwright__browser_evaluate({
  function: `() => ({
    userAgent: navigator.userAgent,
    viewport: {
      width: window.innerWidth,
      height: window.innerHeight
    },
    devicePixelRatio: window.devicePixelRatio,
    platform: navigator.platform
  })`
});

console.log("Browser environment:", env);

// 2. Increase timeouts for CI
const isCI = process.env.CI === 'true';
const timeout = isCI ? 30000 : 10000;

// 3. Add stability waits
await mcp__playwright__browser_wait_for({ time: 2 }); // Wait for page to stabilize
```

## Debugging Helpers

### Comprehensive Debug Capture

```javascript
async function captureDebugInfo(testName) {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const prefix = `debug-${testName}-${timestamp}`;

  // 1. Screenshot
  await mcp__playwright__browser_take_screenshot({
    fullPage: true,
    filename: `${prefix}-screenshot.png`
  });

  // 2. Snapshot
  const snapshot = await mcp__playwright__browser_snapshot({});
  require('fs').writeFileSync(
    `${prefix}-snapshot.txt`,
    snapshot
  );

  // 3. Console messages
  const consoleAll = await mcp__playwright__browser_console_messages({});
  const consoleErrors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  // 4. Network requests
  const requests = await mcp__playwright__browser_network_requests({});

  // 5. Generate report
  const report = {
    testName,
    timestamp,
    consoleMessages: consoleAll.length,
    consoleErrors: consoleErrors.length,
    networkRequests: requests.length,
    failedRequests: requests.filter(r => r.status >= 400).length,
    requests: requests.map(r => ({
      url: r.url,
      method: r.method,
      status: r.status
    })),
    errors: consoleErrors
  };

  require('fs').writeFileSync(
    `${prefix}-report.json`,
    JSON.stringify(report, null, 2)
  );

  console.log(`Debug info saved to ${prefix}-*`);

  return report;
}
```

### Test Execution Logger

```javascript
class TestLogger {
  constructor(testName) {
    this.testName = testName;
    this.steps = [];
    this.startTime = Date.now();
  }

  async step(description, fn) {
    const stepStart = Date.now();
    console.log(`  [${this.testName}] ${description}...`);

    try {
      const result = await fn();

      this.steps.push({
        description,
        status: 'PASS',
        duration: Date.now() - stepStart
      });

      return result;

    } catch (error) {
      this.steps.push({
        description,
        status: 'FAIL',
        duration: Date.now() - stepStart,
        error: error.message
      });

      await mcp__playwright__browser_take_screenshot({
        filename: `${this.testName}-FAILURE-${this.steps.length}.png`
      });

      throw error;
    }
  }

  report() {
    const totalDuration = Date.now() - this.startTime;
    const passed = this.steps.filter(s => s.status === 'PASS').length;
    const failed = this.steps.filter(s => s.status === 'FAIL').length;

    console.log(`\n=== ${this.testName} Report ===`);
    console.log(`Total: ${this.steps.length} steps`);
    console.log(`Passed: ${passed}`);
    console.log(`Failed: ${failed}`);
    console.log(`Duration: ${totalDuration}ms`);
    console.log('\nSteps:');

    this.steps.forEach((step, i) => {
      const icon = step.status === 'PASS' ? '✓' : '✗';
      console.log(`  ${i + 1}. ${icon} ${step.description} (${step.duration}ms)`);
      if (step.error) {
        console.log(`     Error: ${step.error}`);
      }
    });
  }
}

// Usage
const logger = new TestLogger("My Test");

await logger.step("Navigate to page", async () => {
  await mcp__playwright__browser_navigate({ url: "..." });
});

await logger.step("Fill form", async () => {
  await fillForm();
});

logger.report();
```

## Best Practices for Debuggable Tests

### 1. Use Descriptive Names
```javascript
// BAD
await mcp__playwright__browser_click({ element: "btn", ref: "#b1" });

// GOOD
await mcp__playwright__browser_click({
  element: "Submit registration button",
  ref: "button#submit-registration"
});
```

### 2. Add Context to Assertions
```javascript
// BAD
assert(condition);

// GOOD
assert(
  condition,
  `Expected user to be logged in. Snapshot includes: ${snapshot.substring(0, 200)}`
);
```

### 3. Log Key Decision Points
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});

if (snapshot.includes("Login")) {
  console.log("User not logged in, performing login");
  await login();
} else {
  console.log("User already logged in, skipping login");
}
```

### 4. Capture Evidence on Failure
```javascript
try {
  await performTest();
} catch (error) {
  await captureDebugInfo(testName);
  throw error; // Re-throw to fail the test
}
```

### 5. Use Test Isolation
```javascript
// Each test should be independent
beforeEach(async () => {
  await setup(); // Fresh state
});

afterEach(async () => {
  await cleanup(); // Clean up
});
```
