# Browser Feature Compatibility Matrix

Quick reference for checking feature support across browsers when implementing web standards.

## Browser Versions (as of 2025)

| Browser | Current Version | Engine | JavaScript Engine |
|---------|----------------|--------|-------------------|
| Chrome | 131+ | Blink | V8 |
| Firefox | 133+ | Gecko | SpiderMonkey |
| Safari | 18+ | WebKit | JavaScriptCore |
| Edge | 131+ | Blink | V8 |
| **Ladybird** | Development | LibWeb | LibJS |

## Compatibility Checking Resources

### Online Tools

**Can I Use** - https://caniuse.com/
- Browser support tables for web features
- Usage statistics
- Known issues

**MDN Browser Compatibility Data** - https://github.com/mdn/browser-compat-data
- Comprehensive compatibility data
- Used by MDN documentation
- Machine-readable JSON

**WPT Dashboard** - https://wpt.fyi/
- Web Platform Tests results across browsers
- Shows which tests pass/fail per browser

### Feature Detection

**Don't use browser detection**:
```javascript
// WRONG: Browser sniffing
if (navigator.userAgent.includes("Chrome")) {
    // Use feature
}
```

**Use feature detection instead**:
```javascript
// RIGHT: Feature detection
if ("requestIdleCallback" in window) {
    requestIdleCallback(() => { /* ... */ });
} else {
    // Fallback or skip
}
```

## Key Web Platform Features

### HTML5 APIs

| Feature | Chrome | Firefox | Safari | Ladybird | Spec |
|---------|--------|---------|--------|----------|------|
| Canvas 2D | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG HTML |
| WebGL 1.0 | ✅ | ✅ | ✅ | 🟡 Partial | Khronos |
| WebGL 2.0 | ✅ | ✅ | ✅ | 🟡 Partial | Khronos |
| Web Storage | ✅ | ✅ | ✅ | ✅ | WHATWG HTML |
| Web Workers | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG HTML |
| Service Workers | ✅ | ✅ | ✅ | ❌ | WHATWG HTML |
| WebSocket | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG |
| Fetch API | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG Fetch |

### DOM APIs

| Feature | Chrome | Firefox | Safari | Ladybird | Spec |
|---------|--------|---------|--------|----------|------|
| querySelector | ✅ | ✅ | ✅ | ✅ | WHATWG DOM |
| MutationObserver | ✅ | ✅ | ✅ | ✅ | WHATWG DOM |
| Custom Elements | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG HTML |
| Shadow DOM | ✅ | ✅ | ✅ | 🟡 Partial | WHATWG DOM |
| Intersection Observer | ✅ | ✅ | ✅ | 🟡 Partial | W3C |

### CSS Features

| Feature | Chrome | Firefox | Safari | Ladybird | Spec |
|---------|--------|---------|--------|----------|------|
| Flexbox | ✅ | ✅ | ✅ | 🟡 Partial | W3C CSS |
| Grid | ✅ | ✅ | ✅ | 🟡 Partial | W3C CSS |
| Custom Properties | ✅ | ✅ | ✅ | ✅ | W3C CSS |
| calc() | ✅ | ✅ | ✅ | ✅ | W3C CSS |
| Transforms | ✅ | ✅ | ✅ | 🟡 Partial | W3C CSS |
| Transitions | ✅ | ✅ | ✅ | 🟡 Partial | W3C CSS |
| Animations | ✅ | ✅ | ✅ | 🟡 Partial | W3C CSS |
| Container Queries | ✅ | ✅ | ✅ | ❌ | W3C CSS |

### JavaScript (ECMAScript)

| Feature | Chrome | Firefox | Safari | Ladybird | Spec |
|---------|--------|---------|--------|----------|------|
| ES6 (ES2015) | ✅ | ✅ | ✅ | 🟡 Partial | TC39 |
| Promises | ✅ | ✅ | ✅ | ✅ | TC39 |
| async/await | ✅ | ✅ | ✅ | ✅ | TC39 |
| Modules (import/export) | ✅ | ✅ | ✅ | 🟡 Partial | TC39 |
| Classes | ✅ | ✅ | ✅ | ✅ | TC39 |
| Proxy | ✅ | ✅ | ✅ | 🟡 Partial | TC39 |
| WeakMap/WeakSet | ✅ | ✅ | ✅ | ✅ | TC39 |

**Legend**:
- ✅ Full support
- 🟡 Partial support
- ❌ Not supported

## Testing Compatibility in Ladybird

### 1. Check Feature Implementation

```bash
# Search for feature in codebase
cd /home/rbsmith4/ladybird
grep -r "requestIdleCallback" Libraries/LibWeb/
```

### 2. Check WPT Results

```bash
# Run WPT tests for feature
./Meta/WPT.sh run --log results.log html/webappapis/idle-callbacks

# Check pass/fail status
grep "requestIdleCallback" results.log
```

### 3. Write Feature Detection Test

```javascript
// Tests/LibWeb/Text/input/feature-detection.html
test(() => {
    // Check if feature exists
    if ("requestIdleCallback" in window) {
        println("requestIdleCallback: supported");

        // Test basic functionality
        let called = false;
        requestIdleCallback(() => {
            called = true;
        });

        println(`Callback queued: ${called}`);
    } else {
        println("requestIdleCallback: not supported");
    }
});
```

## Common Incompatibilities to Watch For

### 1. Vendor Prefixes (Legacy)

```css
/* Old approach (avoid in new code) */
.box {
    -webkit-transform: rotate(45deg);
    -moz-transform: rotate(45deg);
    -ms-transform: rotate(45deg);
    transform: rotate(45deg);
}
```

Most modern features don't need prefixes. Check Can I Use for current status.

### 2. Different Implementations

**Event.composedPath()** - Paths differ slightly between browsers:

```javascript
// Test in multiple browsers
test(() => {
    const div = document.createElement("div");
    const span = document.createElement("span");
    div.appendChild(span);
    document.body.appendChild(div);

    let path;
    span.addEventListener("click", (e) => {
        path = e.composedPath();
    });

    span.click();

    // Path includes: span, div, body, html, document, window
    println(`Path length: ${path.length}`);
    println(`Path[0]: ${path[0].nodeName}`);
});
```

### 3. Timing Differences

**MutationObserver** timing varies:

```javascript
asyncTest(async (done) => {
    const div = document.createElement("div");
    const observer = new MutationObserver((mutations) => {
        println(`Mutations: ${mutations.length}`);
        done();
    });

    observer.observe(div, { childList: true });

    // Mutation is queued as microtask
    div.appendChild(document.createElement("span"));

    // Observer fires in microtask queue
});
```

## Browser-Specific Quirks

### Chrome/Blink

- Aggressive optimizations (may hide timing bugs)
- Often first to implement new features
- Used as reference for Ladybird (common baseline)

### Firefox/Gecko

- Strict spec compliance
- Good error messages
- Useful for verifying edge cases

### Safari/WebKit

- Sometimes lags on new features
- Strict security model
- Good for testing iOS compatibility

### Ladybird

- Focus on spec compliance over browser compatibility
- Smaller feature set (growing)
- Useful for learning browser internals

## Feature Flags

Browsers often have experimental features behind flags:

**Chrome**: `chrome://flags`
**Firefox**: `about:config`
**Safari**: Develop menu → Experimental Features

**Ladybird**: Check `LibWeb/Bindings/` for enabled features

## Testing Against Multiple Browsers

### 1. Use WPT

WPT tests run across all browsers:

```bash
# Ladybird WPT results
./Meta/WPT.sh run --log ladybird.log css

# Compare with online WPT dashboard
# https://wpt.fyi/results/css
```

### 2. Manual Cross-Browser Testing

```bash
# Test in Ladybird
./Build/release/bin/Ladybird test-page.html

# Test in Chrome
google-chrome test-page.html

# Test in Firefox
firefox test-page.html
```

### 3. Automated Testing

Use Playwright or Selenium for automated cross-browser tests:

```javascript
// Playwright example (not in Ladybird yet)
const { chromium, firefox, webkit } = require('playwright');

for (const browserType of [chromium, firefox, webkit]) {
    const browser = await browserType.launch();
    const page = await browser.newPage();
    await page.goto('http://localhost:8000/test.html');
    // Run tests
    await browser.close();
}
```

## When Browsers Disagree

### 1. Check Spec

Spec is authoritative:

```
Chrome does X
Firefox does Y
Spec says: Z

→ Implement Z (spec wins)
```

### 2. Check Other Browsers

If majority agree, likely correct:

```
Chrome: X
Firefox: X
Safari: X
Old spec: Y

→ Implement X (likely spec was updated)
```

### 3. Ask on Discord

Ladybird Discord has browser experts who can clarify ambiguities.

## Resources for Compatibility

**MDN Web Docs** - https://developer.mozilla.org/
- Browser compatibility tables
- Feature documentation
- Code examples

**Can I Use** - https://caniuse.com/
- Feature support tables
- Usage statistics

**WPT Dashboard** - https://wpt.fyi/
- Test results across browsers
- Interoperability scores

**Baseline** - https://web.dev/baseline
- Web features widely supported across browsers
- Categorized by year

## Summary

1. **Use feature detection**, not browser detection
2. **Check WPT results** for cross-browser status
3. **Test in multiple browsers** when possible
4. **Follow spec** when browsers disagree
5. **Use Can I Use** for quick compatibility checks
6. **Consult MDN** for detailed browser notes
