---
name: web-standards-testing
description: Comprehensive guide for Web Platform Tests (WPT) integration and LibWeb testing patterns, including standards compliance for WHATWG, W3C, and TC39 specifications
use-when: Writing web standards tests, running WPT, fixing spec compliance issues, creating LibWeb tests, or ensuring cross-browser compatibility
allowed-tools:
  - Bash
  - Read
  - Write
  - Grep
  - Glob
scripts:
  - run_wpt_subset.sh
  - compare_wpt_results.sh
  - create_libweb_test.sh
  - rebaseline_tests.sh
---

# Web Standards Testing for Ladybird

## Overview

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Implement   │ → │  Write Test  │ → │  Run Tests   │
│  Web Spec    │    │  (WPT/LibWeb)│    │  (Verify)    │
└──────────────┘    └──────────────┘    └──────────────┘
                            ↓                   ↓
                    ┌──────────────┐    ┌──────────────┐
                    │  Rebaseline  │ ← │  Triage      │
                    │  Expected    │    │  Failures    │
                    └──────────────┘    └──────────────┘
```

## 1. Web Standards Overview

### Specification Organizations

**WHATWG (Web Hypertext Application Technology Working Group)**
- **HTML Living Standard**: https://html.spec.whatwg.org/
- **DOM Living Standard**: https://dom.spec.whatwg.org/
- **Fetch Living Standard**: https://fetch.spec.whatwg.org/
- **URL Living Standard**: https://url.spec.whatwg.org/
- Living standards that continuously evolve

**W3C (World Wide Web Consortium)**
- **CSS Specifications**: https://www.w3.org/Style/CSS/specs.en.html
- **WebGL**: https://www.khronos.org/registry/webgl/specs/
- **Web Audio**: https://www.w3.org/TR/webaudio/
- Versioned specifications (e.g., CSS3, WebGL 2.0)

**TC39 (ECMAScript Technical Committee)**
- **ECMAScript Specification**: https://tc39.es/ecma262/
- JavaScript language specification
- Stage-based proposal process

### Spec Compliance Philosophy

**Critical Rule: Match spec naming exactly**

```cpp
// RIGHT - matches https://html.spec.whatwg.org/
bool HTMLInputElement::suffering_from_being_missing();

// WRONG - arbitrarily differs from spec naming
bool HTMLInputElement::has_missing_constraint();
```

When implementing algorithms, preserve spec terminology even if unconventional:
- Spec says "suffering from being missing" → use `suffering_from_being_missing()`
- Spec says "run the animation frame callbacks" → use `run_the_animation_frame_callbacks()`
- Spec says "fire an event" → use `fire_an_event()`

## 2. Web Platform Tests (WPT)

### WPT Architecture

```
Tests/LibWeb/WPT/
├── wpt/                    # Upstream WPT repository (git submodule)
│   ├── html/               # HTML tests
│   ├── css/                # CSS tests
│   ├── dom/                # DOM tests
│   ├── fetch/              # Fetch API tests
│   └── tools/              # WPT infrastructure
└── wpt-import/             # Imported WPT tests (copied locally)
```

### Running WPT

```bash
# Update WPT repository
./Meta/WPT.sh update

# Run all WPT tests
./Meta/WPT.sh run --log results.log

# Run specific test directories
./Meta/WPT.sh run --log css-results.log css
./Meta/WPT.sh run --log dom-results.log dom

# Run specific test file
./Meta/WPT.sh run --log single.log html/dom/aria-attribute-reflection.html

# Run with headless browser (default)
./Meta/WPT.sh run --log results.log css

# Run with visible browser (for debugging)
./Meta/WPT.sh run --show-window --log results.log css

# Run with debugger attached
./Meta/WPT.sh run --debug-process WebContent --log results.log dom/historical.html
```

### WPT Parallel Execution

```bash
# Auto-detect optimal parallelism
./Meta/WPT.sh run --parallel-instances 0 --log results.log css

# Explicit parallelism (4 instances)
./Meta/WPT.sh run --parallel-instances 4 --log results.log css

# Disable parallelism (sequential)
./Meta/WPT.sh run --parallel-instances 1 --log results.log css
```

### Comparing WPT Results

```bash
# Establish baseline before changes
./Meta/WPT.sh run --log baseline.log css

# Make your changes
git checkout my-css-feature-branch

# Compare against baseline
./Meta/WPT.sh compare --log results.log baseline.log css

# This will:
# 1. Update expectations based on baseline.log
# 2. Run tests again
# 3. Report regressions (new failures) and improvements (new passes)
```

### WPT Bisecting

Find the first commit that broke WPT tests:

```bash
# Find when css/CSS2 tests broke
./Meta/WPT.sh bisect BAD_COMMIT GOOD_COMMIT css/CSS2

# With specific test paths
./Meta/WPT.sh bisect BAD_COMMIT GOOD_COMMIT css/CSS2/colors/t123.html
```

### Importing WPT Tests

```bash
# Import single test from wpt.live
./Meta/WPT.sh import html/dom/aria-attribute-reflection.html

# Import multiple tests
./Meta/WPT.sh import css/css-color/color-rgb.html css/css-color/color-hsl.html

# Force re-import (overwrite existing)
./Meta/WPT.sh import --force html/dom/aria-attribute-reflection.html

# Import from custom WPT instance
./Meta/WPT.sh import --wpt-base-url=http://localhost:8000 dom/nodes/Node-cloneNode.html
```

This will:
1. Download test files from https://wpt.live/
2. Copy to `Tests/LibWeb/<type>/input/wpt-import/`
3. Run test and capture output
4. Create expectation file in `Tests/LibWeb/<type>/expected/wpt-import/`

### WPT Listing Tests

```bash
# List all tests in directory
./Meta/WPT.sh list-tests css/css-color

# List tests matching pattern
./Meta/WPT.sh list-tests css dom/nodes
```

### WPT Log Formats

```bash
# Raw format (default, human-readable)
./Meta/WPT.sh run --log results.log css

# WPTReport JSON format (machine-readable)
./Meta/WPT.sh run --log-wptreport results.json css

# WPTScreenshot database (for visual regression)
./Meta/WPT.sh run --log-wptscreenshot screenshots.db css

# Multiple formats simultaneously
./Meta/WPT.sh run \
    --log-raw results.log \
    --log-wptreport results.json \
    --log-wptscreenshot screenshots.db \
    css
```

### WPT Environment Variables

```bash
# Show progress indicators (default: true)
SHOW_PROGRESS=true ./Meta/WPT.sh run --log results.log css

# Show logs in tmux split panes (requires tmux)
TRY_SHOW_LOGFILES_IN_TMUX=true ./Meta/WPT.sh run --log results.log css

# Disable log output
SHOW_LOGFILES=false ./Meta/WPT.sh run --log results.log css

# Override WPT parallelism
WPT_PROCESSES=8 ./Meta/WPT.sh run --log results.log css

# Custom Ladybird binary
LADYBIRD_BINARY=/custom/path/Ladybird ./Meta/WPT.sh run --log results.log css
```

## 3. LibWeb Test Types

Ladybird has four test types for LibWeb:

| Type       | Purpose                          | Output Format    | Use Case                          |
|------------|----------------------------------|------------------|-----------------------------------|
| Text       | JavaScript API testing           | println() output | Testing Web APIs, JS behavior     |
| Layout     | Layout tree structure            | Layout tree dump | Testing CSS layout algorithms     |
| Ref        | Visual comparison (HTML)         | Screenshot diff  | Visual rendering, use HTML ref    |
| Screenshot | Visual comparison (image)        | Screenshot diff  | Visual rendering, use image ref   |

### 3.1 Text Tests

**Purpose**: Test JavaScript APIs without visual representation

**Structure**:
```html
<!DOCTYPE html>
<script src="include.js"></script>
<script>
    test(() => {
        println("Testing Web API");

        // Test implementation
        const result = document.querySelector("div");
        println(`Result: ${result}`);
    });
</script>
```

**Async Text Tests**:
```html
<!DOCTYPE html>
<script src="include.js"></script>
<script>
    asyncTest(async (done) => {
        println("Testing async Web API");

        await fetch("/api/data");
        println("Fetch completed");

        done();
    });
</script>
```

**Creating Text Tests**:
```bash
# Synchronous test
./Tests/LibWeb/add_libweb_test.py my-dom-test Text

# Async test
./Tests/LibWeb/add_libweb_test.py my-fetch-test Text --async

# Edit the generated file
vim Tests/LibWeb/Text/input/my-dom-test.html

# Rebaseline to capture expected output
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-dom-test.html
```

**Expected Output**:
```
# Tests/LibWeb/Text/expected/my-dom-test.txt
Testing Web API
Result: [object HTMLDivElement]
```

**Text Test Patterns**:
```javascript
// Testing DOM APIs
test(() => {
    const div = document.createElement("div");
    div.textContent = "Hello";
    println(`Text: ${div.textContent}`);
});

// Testing events
test(() => {
    let fired = false;
    document.addEventListener("click", () => { fired = true; });
    document.dispatchEvent(new Event("click"));
    println(`Event fired: ${fired}`);
});

// Testing attributes
test(() => {
    const input = document.querySelector("input");
    input.setAttribute("value", "test");
    println(`Value: ${input.getAttribute("value")}`);
});

// Testing computed styles
test(() => {
    const div = document.querySelector("div");
    const style = getComputedStyle(div);
    println(`Color: ${style.color}`);
});
```

### 3.2 Layout Tests

**Purpose**: Test CSS layout algorithms and box tree structure

**Structure**:
```html
<!DOCTYPE html>
<style>
    .container {
        display: flex;
        width: 400px;
    }
    .item {
        flex: 1;
    }
</style>
<div class="container">
    <div class="item">Item 1</div>
    <div class="item">Item 2</div>
</div>
```

**Creating Layout Tests**:
```bash
# Create layout test
./Tests/LibWeb/add_libweb_test.py flexbox-layout Layout

# Edit HTML
vim Tests/LibWeb/Layout/input/flexbox-layout.html

# Rebaseline to capture layout tree
./Meta/ladybird.py run test-web --rebaseline -f Layout/input/flexbox-layout.html
```

**Expected Output** (layout tree dump):
```
# Tests/LibWeb/Layout/expected/flexbox-layout.txt
Viewport <html> at (0,0) content-size 800x600 [BFC] children: not-inline
  BlockContainer <body> at (8,8) content-size 784x584 children: not-inline
    BlockContainer <div.container> at (8,8) content-size 400x20 flex-container(row) children: not-inline
      BlockContainer <div.item> at (8,8) content-size 200x20 flex-item children: inline
        TextNode <#text>
      BlockContainer <div.item> at (208,8) content-size 200x20 flex-item children: inline
        TextNode <#text>
```

**Quirks Mode Layout Tests**:
```html
<!--Quirks mode-->
<style>
    /* Test quirks mode layout */
</style>
<body>
    <div>Content</div>
</body>
```

**Layout Test Use Cases**:
- Flexbox/Grid layout algorithms
- Absolute positioning
- Float behavior
- Block formatting contexts
- Table layout
- CSS transforms (layout impact)
- Scrollable overflow

### 3.3 Ref Tests

**Purpose**: Visual comparison against reference HTML page

**Structure** (test page):
```html
<!DOCTYPE html>
<link rel="match" href="../expected/my-test-ref.html" />
<style>
    .box {
        width: 100px;
        height: 100px;
        background: linear-gradient(red, blue);
    }
</style>
<div class="box"></div>
```

**Structure** (reference page):
```html
<!DOCTYPE html>
<style>
    .box {
        width: 100px;
        height: 100px;
        background: red; /* Simplified equivalent */
    }
</style>
<div class="box"></div>
```

**Creating Ref Tests**:
```bash
# Create ref test
./Tests/LibWeb/add_libweb_test.py gradient-rendering Ref

# Edit test HTML
vim Tests/LibWeb/Ref/input/gradient-rendering.html

# Edit reference HTML
vim Tests/LibWeb/Ref/expected/gradient-rendering-ref.html

# Run test
./Meta/ladybird.py run test-web -f Ref/input/gradient-rendering.html
```

**Mismatch Ref Tests** (verify rendering is different):
```html
<!DOCTYPE html>
<link rel="mismatch" href="../expected/wrong-rendering-ref.html" />
<style>
    /* This should NOT match the reference */
</style>
```

**Ref Test Best Practices**:
1. Reference should render identically but use simpler techniques
2. Avoid complex features in reference (defeats purpose)
3. Multiple tests can share the same reference
4. Use `rel="match"` for positive tests
5. Use `rel="mismatch"` for negative tests

**Ref Test Use Cases**:
- CSS gradients
- Box shadows
- Border radius
- Transforms
- Filters
- Opacity
- Z-index stacking
- Text rendering

### 3.4 Screenshot Tests

**Purpose**: Visual comparison against reference screenshot image

**Structure**:
```html
<!DOCTYPE html>
<link rel="match" href="../expected/canvas-rendering-ref.html" />
<style>
    canvas {
        border: 1px solid black;
    }
</style>
<canvas id="c" width="200" height="200"></canvas>
<script>
    const ctx = c.getContext("2d");
    ctx.fillStyle = "red";
    ctx.fillRect(50, 50, 100, 100);
</script>
```

**Reference page** (screenshot-ref.html):
```html
<!DOCTYPE html>
<style>
    * {
        margin: 0;
    }
    body {
        background-color: white;
    }
</style>
<img src="../images/canvas-rendering-ref.png">
```

**Creating Screenshot Tests**:
```bash
# Create screenshot test
./Tests/LibWeb/add_libweb_test.py canvas-api Screenshot

# Edit test HTML
vim Tests/LibWeb/Screenshot/input/canvas-api.html

# Generate reference screenshot using headless mode
./Meta/ladybird.py run ladybird --headless --layout-test-mode \
    Tests/LibWeb/Screenshot/input/canvas-api.html

# Move screenshot to images directory
mv ~/Downloads/screenshot-2025-*.png \
    Tests/LibWeb/Screenshot/images/canvas-api-ref.png

# Run test
./Meta/ladybird.py run test-web -f Screenshot/input/canvas-api.html
```

**Screenshot Test Use Cases**:
- Canvas 2D rendering
- WebGL output
- SVG rendering (complex)
- CSS effects difficult to replicate in HTML
- Font rendering edge cases

**Screenshot Test Limitations**:
- Platform-dependent (fonts, anti-aliasing)
- Fragile to minor rendering changes
- Prefer Ref tests when possible

## 4. Running LibWeb Tests

### Basic Test Execution

```bash
# Run all LibWeb tests
./Meta/ladybird.py test LibWeb

# Run via CMake/CTest
cmake --preset Release
ctest --preset Release -R LibWeb

# Run test-web directly
./Meta/ladybird.py run test-web

# Run specific test type
./Meta/ladybird.py run test-web -f Text/input/
./Meta/ladybird.py run test-web -f Layout/input/
./Meta/ladybird.py run test-web -f Ref/input/
./Meta/ladybird.py run test-web -f Screenshot/input/
```

### Running Specific Tests

```bash
# Single test file
./Meta/ladybird.py run test-web -f Text/input/basic.html

# Pattern matching (regex)
./Meta/ladybird.py run test-web -f "Text/input/.*DOM.*"

# Multiple specific tests
./Meta/ladybird.py run test-web \
    -f Text/input/basic.html \
    -f Layout/input/flexbox.html
```

### Rebaselining Tests

```bash
# Rebaseline single test
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-test.html

# Rebaseline all tests in directory
./Meta/ladybird.py run test-web --rebaseline -f Text/input/

# Rebaseline with pattern
./Meta/ladybird.py run test-web --rebaseline -f "Text/input/DOM-.*"
```

**When to Rebaseline**:
- After fixing a bug (intentional output change)
- After implementing new feature
- After updating expected behavior
- Never rebaseline to hide failures!

### Test Output and Debugging

```bash
# Show verbose output
CTEST_OUTPUT_ON_FAILURE=1 ./Meta/ladybird.py test LibWeb

# Run with specific preset
BUILD_PRESET=Debug ./Meta/ladybird.py run test-web -f Text/input/basic.html

# Run with sanitizers
BUILD_PRESET=Sanitizers ./Meta/ladybird.py test LibWeb
```

## 5. Test Organization and Naming

### Directory Structure

```
Tests/LibWeb/
├── Text/
│   ├── input/              # Test HTML files
│   │   ├── basic.html
│   │   ├── DOM-cloneNode.html
│   │   └── wpt-import/     # Imported WPT tests
│   └── expected/           # Expected text output
│       ├── basic.txt
│       ├── DOM-cloneNode.txt
│       └── wpt-import/
├── Layout/
│   ├── input/
│   └── expected/           # Expected layout tree dumps
├── Ref/
│   ├── input/              # Test pages with <link rel="match">
│   └── expected/           # Reference HTML pages
└── Screenshot/
    ├── input/              # Test pages
    ├── expected/           # Reference HTML pages (with <img>)
    └── images/             # Reference PNG/JPEG images
```

### Naming Conventions

**Good test names**:
- `element-get-bounding-client-rect.html` - descriptive, kebab-case
- `css-grid-template-columns.html` - indicates feature area
- `input-element-value-setter.html` - specific API being tested
- `abspos-box-with-replaced-element.html` - layout scenario

**Bad test names**:
- `test1.html` - not descriptive
- `bug-fix.html` - doesn't describe what's tested
- `MyTest.html` - wrong case convention

**Subdirectories** (optional):
```
Tests/LibWeb/Text/input/
├── DOM/
│   ├── Element-cloneNode.html
│   └── Node-appendChild.html
├── CSS/
│   └── computed-style.html
└── HTML/
    └── canvas-api.html
```

## 6. Spec Compliance Patterns

### Algorithm Implementation

When implementing spec algorithms, match the structure:

**Spec**: [HTML Standard - fire an event](https://html.spec.whatwg.org/multipage/webappapis.html#fire-an-event)

```cpp
// Libraries/LibWeb/DOM/EventTarget.cpp

// "To fire an event named e at target..."
void fire_an_event(EventTarget& target, FlyString const& event_name)
{
    // 1. If e is not provided, let e be a new event...
    auto event = Event::create(event_name);

    // 2. Initialize event's type attribute to e
    // (handled in Event::create)

    // 3. Initialize event's isTrusted to true
    event->set_is_trusted(true);

    // 4. Return the result of dispatching event at target
    target.dispatch_event(event);
}
```

### Spec References in Code

```cpp
// Always link to relevant spec sections in comments
// https://dom.spec.whatwg.org/#dom-node-clonenode
JS::NonnullGCPtr<Node> Node::clone_node(Document* document, bool clone_children)
{
    // Step 1: If document is not given, let document be node's node document
    if (!document)
        document = &this->document();

    // Step 2: If node is an element, let copy be the result of creating an element...
    // ...
}
```

### Testing Spec Algorithms

```javascript
// Text test matching spec example
test(() => {
    // From: https://dom.spec.whatwg.org/#example-clonenode

    const div = document.createElement("div");
    const span = document.createElement("span");
    div.appendChild(span);

    // Shallow clone (clone_children = false)
    const shallowClone = div.cloneNode(false);
    println(`Shallow has children: ${shallowClone.hasChildNodes()}`); // false

    // Deep clone (clone_children = true)
    const deepClone = div.cloneNode(true);
    println(`Deep has children: ${deepClone.hasChildNodes()}`); // true
});
```

### Spec Ambiguities

When spec is ambiguous, test against browser behavior:

```javascript
asyncTest(async (done) => {
    // Edge case: spec unclear on timing
    // Test matches Chrome/Firefox behavior

    let order = [];

    queueMicrotask(() => order.push("microtask"));
    setTimeout(() => {
        order.push("timeout");
        println(`Order: ${order.join(", ")}`);
        done();
    }, 0);

    order.push("sync");
});

// Expected: Order: sync, microtask, timeout
```

## 7. Cross-Browser Compatibility

### Feature Detection

```javascript
test(() => {
    // Always test for feature existence
    if (!window.requestIdleCallback) {
        println("requestIdleCallback not supported");
        return;
    }

    // Test feature
    requestIdleCallback(() => {
        println("Idle callback executed");
    });
});
```

### Polyfill Patterns

```javascript
// Don't polyfill in tests - test actual implementation!
test(() => {
    // RIGHT: Test what's actually implemented
    if (!Element.prototype.toggleAttribute) {
        println("toggleAttribute not implemented");
        return;
    }

    const div = document.createElement("div");
    div.toggleAttribute("hidden");
    println(`Has hidden: ${div.hasAttribute("hidden")}`);
});

// WRONG: Don't do this in tests
if (!Element.prototype.toggleAttribute) {
    Element.prototype.toggleAttribute = function(name) {
        // polyfill implementation
    };
}
```

### Browser-Specific Workarounds

```javascript
test(() => {
    // Document browser-specific behavior
    const style = getComputedStyle(div);

    // Some browsers return "rgb(...)", others "rgba(...)"
    const color = style.color;
    const normalized = color.replace(/rgba?\(/, "").replace(/\)/, "");

    println(`Color values: ${normalized}`);
});
```

## 8. WPT Test Structure

### WPT Test Example

```html
<!DOCTYPE html>
<meta charset="utf-8">
<title>DOM: Node.cloneNode() method</title>
<link rel="help" href="https://dom.spec.whatwg.org/#dom-node-clonenode">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
test(() => {
    const div = document.createElement("div");
    const span = document.createElement("span");
    div.appendChild(span);

    const clone = div.cloneNode(true);

    assert_not_equals(clone, div, "Clone is different object");
    assert_equals(clone.childNodes.length, 1, "Clone has one child");
    assert_equals(clone.firstChild.nodeName, "SPAN", "Child is SPAN");
}, "cloneNode(true) creates deep clone");

test(() => {
    const div = document.createElement("div");
    const span = document.createElement("span");
    div.appendChild(span);

    const clone = div.cloneNode(false);

    assert_equals(clone.childNodes.length, 0, "Shallow clone has no children");
}, "cloneNode(false) creates shallow clone");
</script>
```

### WPT Test Harness

```javascript
// Common WPT patterns

// Basic test
test(() => {
    assert_equals(actual, expected, "Description");
}, "Test name");

// Async test
async_test(t => {
    someAsyncOperation(() => {
        assert_true(condition, "Description");
        t.done();
    });
}, "Async test name");

// Promise test
promise_test(async () => {
    const result = await fetch("/api");
    assert_equals(result.status, 200);
}, "Promise test name");

// Assertions
assert_true(value, "message");
assert_false(value, "message");
assert_equals(actual, expected, "message");
assert_not_equals(actual, expected, "message");
assert_array_equals(actual, expected, "message");
assert_throws_dom("InvalidStateError", () => { /* code */ });
assert_unreached("Should not reach here");
```

## 9. Common Testing Patterns

### Testing DOM Manipulation

```javascript
test(() => {
    const parent = document.createElement("div");
    const child = document.createElement("span");

    parent.appendChild(child);
    println(`Children: ${parent.childNodes.length}`); // 1

    parent.removeChild(child);
    println(`After remove: ${parent.childNodes.length}`); // 0
});
```

### Testing CSS Computed Styles

```javascript
test(() => {
    const div = document.createElement("div");
    div.style.color = "red";
    document.body.appendChild(div);

    const style = getComputedStyle(div);
    println(`Color: ${style.color}`);

    document.body.removeChild(div);
});
```

### Testing Events

```javascript
test(() => {
    let eventFired = false;
    const div = document.createElement("div");

    div.addEventListener("click", () => {
        eventFired = true;
    });

    div.dispatchEvent(new Event("click"));
    println(`Event fired: ${eventFired}`); // true
});
```

### Testing Async Operations

```javascript
asyncTest(async (done) => {
    const response = await fetch("/data.json");
    const data = await response.json();

    println(`Loaded: ${data.name}`);
    done();
});
```

### Testing Error Conditions

```javascript
test(() => {
    const div = document.createElement("div");

    try {
        div.appendChild(div); // Can't append to self
        println("ERROR: Should have thrown");
    } catch (e) {
        println(`Caught: ${e.name}`); // HierarchyRequestError
    }
});
```

## 10. Debugging Test Failures

### Understanding Test Output

**Text Test Failure**:
```
FAIL: Tests/LibWeb/Text/input/basic.html
Expected:
  Well hello
  friends!
Actual:
  Well hello

Diff:
  Well hello
- friends!
```

**Layout Test Failure**:
```
FAIL: Tests/LibWeb/Layout/input/flexbox.html
Expected flex-item at (8,8) size 200x20
Actual flex-item at (8,8) size 100x20
```

**Ref Test Failure**:
```
FAIL: Tests/LibWeb/Ref/input/gradient.html
Screenshot mismatch: 1024 pixels differ
```

### Debugging Strategies

```bash
# 1. Run test in visible browser
BUILD_PRESET=Debug ./Meta/ladybird.py run ladybird \
    --layout-test-mode Tests/LibWeb/Text/input/failing-test.html

# 2. Add debug output to test
# Edit test to add more println() calls

# 3. Run with GDB
./Meta/ladybird.py debug ladybird -- \
    --layout-test-mode Tests/LibWeb/Text/input/failing-test.html

# 4. Check for sanitizer violations
BUILD_PRESET=Sanitizers ./Meta/ladybird.py run test-web \
    -f Text/input/failing-test.html

# 5. Compare against other browsers
# Open in Chrome/Firefox to verify expected behavior
```

### Common Failure Causes

1. **Timing issues** - async operations not completing
2. **Layout not updated** - need to trigger layout before measurement
3. **Event order** - events firing in unexpected order
4. **Spec misinterpretation** - algorithm implemented incorrectly
5. **Missing feature** - dependent API not implemented

## 11. Best Practices

### Test Design

✅ **DO**:
- Test one thing per test
- Use descriptive test names
- Add spec references in comments
- Test edge cases and error conditions
- Keep tests simple and focused
- Rebaseline intentionally

❌ **DON'T**:
- Test multiple unrelated features in one test
- Use generic names like "test1.html"
- Skip error handling
- Create tests without spec references
- Rebaseline to hide failures

### Spec Implementation

✅ **DO**:
- Match spec algorithm names exactly
- Link to spec sections in code comments
- Implement all steps from spec algorithms
- Test spec examples explicitly
- Ask on Discord when spec is ambiguous

❌ **DON'T**:
- Rename spec algorithms for "clarity"
- Skip spec steps (even if they seem redundant)
- Implement based on browser behavior alone
- Guess at spec intent

### Test Maintenance

✅ **DO**:
- Update tests when spec changes
- Rebaseline after intentional changes
- Document why tests exist (spec refs)
- Group related tests in subdirectories

❌ **DON'T**:
- Delete failing tests without fixing
- Rebaseline without understanding why output changed
- Create duplicate tests for same feature

## 12. Quick Reference

### Test Creation Workflow

```bash
# 1. Create test
./Tests/LibWeb/add_libweb_test.py my-feature Text

# 2. Edit test
vim Tests/LibWeb/Text/input/my-feature.html

# 3. Run test (will fail first time)
./Meta/ladybird.py run test-web -f Text/input/my-feature.html

# 4. Rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-feature.html

# 5. Verify
./Meta/ladybird.py run test-web -f Text/input/my-feature.html
```

### WPT Workflow

```bash
# 1. Establish baseline
./Meta/WPT.sh run --log baseline.log css/css-color

# 2. Implement feature
# (make code changes)

# 3. Compare
./Meta/WPT.sh compare --log results.log baseline.log css/css-color

# 4. Import passing tests
./Meta/WPT.sh import css/css-color/color-rgb.html
```

### Test File Locations

| Type       | Input                           | Expected                              |
|------------|---------------------------------|---------------------------------------|
| Text       | `Text/input/*.html`             | `Text/expected/*.txt`                 |
| Layout     | `Layout/input/*.html`           | `Layout/expected/*.txt`               |
| Ref        | `Ref/input/*.html`              | `Ref/expected/*-ref.html`             |
| Screenshot | `Screenshot/input/*.html`       | `Screenshot/expected/*-ref.html`<br>`Screenshot/images/*-ref.png` |

### Spec Reference URLs

- **HTML**: https://html.spec.whatwg.org/
- **DOM**: https://dom.spec.whatwg.org/
- **Fetch**: https://fetch.spec.whatwg.org/
- **CSS**: https://www.w3.org/Style/CSS/
- **ECMAScript**: https://tc39.es/ecma262/
- **WebIDL**: https://webidl.spec.whatwg.org/

## 13. Examples Directory

See `examples/` for:
- `text-test-example.html` - Simple API test with println()
- `layout-test-example.html` - Layout tree structure test
- `ref-test-example.html` - Visual reference test
- `wpt-test-example.html` - WPT test harness structure

## 14. Scripts Directory

See `scripts/` for:
- `run_wpt_subset.sh` - Run specific WPT test categories
- `compare_wpt_results.sh` - Compare two WPT runs
- `create_libweb_test.sh` - Wrapper for add_libweb_test.py
- `rebaseline_tests.sh` - Update test expectations

## 15. Additional Resources

- **Documentation/Testing.md** - Comprehensive testing guide
- **Documentation/LibWebPatterns.md** - LibWeb code patterns
- **Documentation/AddNewIDLFile.md** - Adding Web IDL interfaces
- **Documentation/CSSProperties.md** - Adding CSS properties
- **Tests/LibWeb/add_libweb_test.py** - Test creation script
- **Meta/WPT.sh** - WPT integration script

## Related Skills

### Implementation Testing
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Test LibWeb implementations with Text, Layout, and Ref tests. Verify spec compliance for HTML/CSS/DOM features.
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Test JavaScript implementations with Test262 conformance suite. Verify ECMAScript spec compliance.

### Test Infrastructure
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build test-web runner and WPT infrastructure. Understand test target compilation.
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Run tests in CI across platforms. Configure WPT parallel execution and test reporting.

### Development Workflow
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Implement spec algorithms with TRY/ErrorOr. Use spec-matching function names.
- **[ladybird-git-workflow](../ladybird-git-workflow/SKILL.md)**: Commit tests with spec links. Follow commit message format for test additions.

### Quality Assurance
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Generate fuzzer corpus from WPT tests. Use web-platform-tests as seed inputs.
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug test failures with sanitizers. Run test suite with ASAN/UBSAN.
- **[browser-performance](../browser-performance/SKILL.md)**: Benchmark test execution performance. Measure WPT compliance progress.

### Documentation
- **[documentation-generation](../documentation-generation/SKILL.md)**: Document test patterns and spec compliance. Link to WHATWG/W3C/TC39 specs.
