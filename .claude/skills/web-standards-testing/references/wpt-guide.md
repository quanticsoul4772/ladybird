# Web Platform Tests (WPT) Guide

## What is WPT?

Web Platform Tests (WPT) is a **cross-browser test suite** for web platform features. It's used by all major browser vendors (Chrome, Firefox, Safari, Edge) and increasingly by Ladybird to ensure **spec compliance** and **interoperability**.

## WPT Repository Structure

```
Tests/LibWeb/WPT/wpt/
├── html/                  # HTML Living Standard tests
│   ├── dom/               # DOM APIs
│   ├── semantics/         # HTML elements
│   └── webappapis/        # Web Application APIs
├── css/                   # CSS specifications
│   ├── css-color/         # CSS Color Module
│   ├── css-flexbox/       # Flexbox
│   └── css-grid/          # Grid Layout
├── dom/                   # DOM Standard tests
│   ├── nodes/             # Node APIs
│   └── events/            # Event handling
├── fetch/                 # Fetch API
├── websockets/            # WebSocket API
├── workers/               # Web Workers
├── resources/             # Test infrastructure
│   ├── testharness.js     # Test framework
│   └── testharnessreport.js
└── tools/                 # WPT tooling
    └── wptrunner/         # Test runner
```

## WPT Test Format

### Standard WPT Test Structure

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>Feature name - specific aspect being tested</title>
<link rel="help" href="https://spec.url/#section">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
test(() => {
    // Test code
    assert_equals(actual, expected, "description");
}, "Test name");
</script>
```

### Required Metadata

1. **`<meta charset="utf-8">`** - Character encoding
2. **`<title>`** - What's being tested (appears in results)
3. **`<link rel="help">`** - Spec section URL(s)
4. **Script includes** - Test harness files

### Test Types

**Synchronous Test**:
```javascript
test(() => {
    assert_equals(1 + 1, 2, "Math works");
}, "Basic arithmetic test");
```

**Async Test**:
```javascript
async_test(t => {
    setTimeout(() => {
        assert_true(true, "Timeout completed");
        t.done();
    }, 100);
}, "Async test with callback");
```

**Promise Test**:
```javascript
promise_test(async () => {
    const response = await fetch("/api");
    assert_equals(response.status, 200);
}, "Fetch returns 200");
```

## WPT Assertions

### Common Assertions

```javascript
// Boolean assertions
assert_true(value, "message");
assert_false(value, "message");

// Equality
assert_equals(actual, expected, "message");
assert_not_equals(actual, expected, "message");

// Array equality
assert_array_equals(actual, expected, "message");

// Object properties
assert_own_property(object, "property", "message");

// Type checking
assert_class_string(object, "ClassName", "message");

// Null/undefined
assert_not_exists(object, "property", "message");

// Exceptions
assert_throws_dom("InvalidStateError", () => {
    // Code that should throw
}, "description");

assert_throws_js(TypeError, () => {
    // Code that should throw TypeError
});

// Unreachable code
assert_unreached("Should not reach here");
```

## Running WPT in Ladybird

### Basic Commands

```bash
# Update WPT repository
./Meta/WPT.sh update

# Run all tests
./Meta/WPT.sh run --log results.log

# Run specific directory
./Meta/WPT.sh run --log dom-results.log dom

# Run single test
./Meta/WPT.sh run --log test.log html/dom/aria-attribute-reflection.html
```

### Advanced Options

```bash
# Parallel execution (auto-detect cores)
./Meta/WPT.sh run --parallel-instances 0 --log results.log css

# Show browser window (debugging)
./Meta/WPT.sh run --show-window --log results.log dom/nodes/Node-cloneNode.html

# Debug specific process
./Meta/WPT.sh run --debug-process WebContent --log results.log dom/historical.html

# Multiple log formats
./Meta/WPT.sh run \
    --log-raw results.log \
    --log-wptreport results.json \
    --log-wptscreenshot screenshots.db \
    css
```

## Comparing Results

```bash
# Establish baseline
./Meta/WPT.sh run --log baseline.log css

# Make changes
git checkout feature-branch

# Compare
./Meta/WPT.sh compare --log results.log baseline.log css
```

This will:
1. Generate expectations from baseline
2. Run tests again
3. Report new passes (improvements) and new failures (regressions)

## Importing WPT Tests

When Ladybird passes a WPT test, import it:

```bash
# Import from wpt.live
./Meta/WPT.sh import html/dom/aria-attribute-reflection.html

# This will:
# 1. Download test from https://wpt.live/
# 2. Copy to Tests/LibWeb/<type>/input/wpt-import/
# 3. Run test and capture output
# 4. Create expectation file
```

Force re-import:
```bash
./Meta/WPT.sh import --force html/dom/aria-attribute-reflection.html
```

## WPT vs LibWeb Tests

| Aspect | WPT | LibWeb Tests |
|--------|-----|--------------|
| **Purpose** | Cross-browser compliance | Ladybird-specific testing |
| **Format** | testharness.js | include.js with println() |
| **Upstream** | Shared with all browsers | Ladybird only |
| **When to use** | Standard web features | Ladybird internals, regressions |
| **Location** | `wpt/` or `wpt-import/` | `Text/`, `Layout/`, etc. |

## Best Practices

### 1. One Test Per File (Usually)

```javascript
// GOOD: Focused test
test(() => {
    const div = document.createElement("div");
    div.setAttribute("id", "test");
    assert_equals(div.id, "test");
}, "Element.id reflects attribute");
```

```javascript
// AVOID: Multiple unrelated tests in one file
test(() => { /* test id */ }, "id test");
test(() => { /* test class */ }, "class test");
test(() => { /* test style */ }, "style test");
// Better to split these into separate files
```

### 2. Use Descriptive Test Names

```javascript
// GOOD
test(() => {
    // ...
}, "cloneNode(true) creates deep clone with all descendants");

// BAD
test(() => {
    // ...
}, "test 1");
```

### 3. Always Include Spec References

```html
<link rel="help" href="https://dom.spec.whatwg.org/#dom-node-clonenode">
```

Multiple refs:
```html
<link rel="help" href="https://dom.spec.whatwg.org/#dom-node-clonenode">
<link rel="help" href="https://dom.spec.whatwg.org/#concept-node-clone">
```

### 4. Test Edge Cases

```javascript
test(() => {
    const div = document.createElement("div");

    // Test normal case
    div.setAttribute("id", "normal");
    assert_equals(div.id, "normal");

    // Test empty string
    div.setAttribute("id", "");
    assert_equals(div.id, "");

    // Test removal
    div.removeAttribute("id");
    assert_equals(div.id, "");
}, "Element.id handles edge cases");
```

## Common WPT Patterns

### Testing IDL Attributes

```javascript
test(() => {
    const input = document.createElement("input");

    // Test getter
    assert_equals(input.value, "", "Initial value is empty");

    // Test setter
    input.value = "test";
    assert_equals(input.value, "test", "Value can be set");

    // Test reflection to attribute
    assert_equals(input.getAttribute("value"), "test", "Reflects to attribute");
}, "HTMLInputElement.value IDL attribute");
```

### Testing Events

```javascript
async_test(t => {
    const div = document.createElement("div");
    let eventFired = false;

    div.addEventListener("click", t.step_func(() => {
        eventFired = true;
        assert_true(eventFired, "Event listener called");
        t.done();
    }));

    div.click();
}, "click() fires click event");
```

### Testing Exceptions

```javascript
test(() => {
    const div = document.createElement("div");

    assert_throws_dom("HierarchyRequestError", () => {
        div.appendChild(div); // Can't append element to itself
    }, "appendChild throws when appending to self");
}, "Node.appendChild validation");
```

## Debugging WPT Tests

### View Test in Browser

```bash
# Run WPT server locally
cd Tests/LibWeb/WPT/wpt
./wpt serve

# Visit in browser:
# http://web-platform.test:8000/dom/nodes/Node-cloneNode.html
```

### Debug with Ladybird

```bash
# Run with visible window
./Meta/WPT.sh run --show-window --log test.log \
    dom/nodes/Node-cloneNode.html

# Attach debugger
./Meta/WPT.sh run --debug-process WebContent --log test.log \
    dom/nodes/Node-cloneNode.html
```

### Common Failure Patterns

**Timeout** - Async test never completes:
```javascript
// WRONG: forgot t.done()
async_test(t => {
    setTimeout(() => {
        assert_true(true);
        // Missing: t.done();
    }, 100);
});

// RIGHT:
async_test(t => {
    setTimeout(t.step_func_done(() => {
        assert_true(true);
    }), 100);
});
```

**Assertion fails unexpectedly**:
- Check spec for exact expected behavior
- Compare with other browsers (Chrome/Firefox)
- Verify feature is actually implemented

**Test crashes**:
- Run with sanitizers to find memory issues
- Check for unimplemented features
- Look for null pointer dereferences

## Resources

- **WPT Documentation**: https://web-platform-tests.org/
- **WPT Dashboard**: https://wpt.fyi/
- **Testharness.js API**: https://web-platform-tests.org/writing-tests/testharness-api.html
- **Writing WPT Tests**: https://web-platform-tests.org/writing-tests/
- **Ladybird WPT Integration**: `Documentation/Testing.md`
