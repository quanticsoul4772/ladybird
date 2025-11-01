# JavaScript Execution & DOM Manipulation Tests

This directory contains comprehensive Playwright E2E tests for JavaScript execution and Web API functionality in Ladybird browser.

## Test Files

### 1. dom-manipulation.spec.ts (12 tests)
Tests core DOM manipulation capabilities:
- **JS-DOM-001 to JS-DOM-012**: querySelector, getElementById, createElement, appendChild, removeChild, replaceChild, attributes, classList, innerHTML/textContent, DOM traversal, cloneNode, document fragments, Shadow DOM

### 2. events.spec.ts (10 tests)
Tests JavaScript event handling:
- **JS-EVT-001 to JS-EVT-010**: addEventListener/removeEventListener, event bubbling, event capturing, preventDefault, stopPropagation, custom events, mouse events, keyboard events, form events, focus events

### 3. async-operations.spec.ts (12 tests)
Tests asynchronous JavaScript operations:
- **JS-ASYNC-001 to JS-ASYNC-012**: setTimeout, setInterval, Promises (resolve/reject/chaining), Promise.all(), Promise.race(), async/await, error handling, Fetch API (GET/POST), Request/Response objects

### 4. web-apis.spec.ts (15 tests)
Tests browser Web APIs:
- **JS-API-001 to JS-API-015**: localStorage, sessionStorage, console methods, JSON, URL parsing, URLSearchParams, Navigator, window.location, window.history, performance.now(), Crypto.getRandomValues(), IntersectionObserver, MutationObserver, ResizeObserver, requestAnimationFrame

### 5. es6-features.spec.ts (10 tests)
Tests modern JavaScript (ES6+) features:
- **JS-ES6-001 to JS-ES6-010**: Arrow functions, template literals, destructuring, spread operator, rest parameters, classes, inheritance, modules, Map/Set, Symbols

## Test Statistics

- **Total Test Files**: 5
- **Total Tests**: 59
- **Total Lines of Code**: ~2,553
- **All tests are async**: Using Playwright's async page context
- **Priority**: P0 (Critical) - Core browser functionality

## Running Tests

```bash
# Run all JavaScript tests
npm test -- tests/javascript/

# Run specific test file
npm test -- tests/javascript/dom-manipulation.spec.ts

# Run tests with specific tag
npm test -- --grep @p0

# Run in headed mode (see browser)
npm test -- tests/javascript/ --headed

# Run with debug output
DEBUG=pw:api npm test -- tests/javascript/
```

## Test Patterns

All tests follow consistent patterns:

1. **Setup**: Navigate to data URL with test HTML
2. **Execute**: Use `page.evaluate()` to run JavaScript in browser context
3. **Verify**: Assert results using Playwright's expect API
4. **Error Handling**: Graceful degradation for unsupported features

Example:
```typescript
test('JS-DOM-001: querySelector/querySelectorAll', async ({ page }) => {
  const html = `<body><p class="text">Content</p></body>`;
  await page.goto(`data:text/html,${encodeURIComponent(html)}`);

  const result = await page.evaluate(() => {
    return document.querySelector('.text')?.textContent;
  });

  expect(result).toBe('Content');
});
```

## Implementation Notes

### Browser Context Execution
- All JavaScript code runs in browser context via `page.evaluate()`
- Complex async operations use Promise wrappers
- Console messages captured via `page.on('console')` listener

### Error Handling
- Tests include try/catch for optional Web APIs
- Unsupported features skip gracefully with test.skip()
- Network errors in Fetch tests are expected in some environments

### Test Data
- Most tests use data URLs for self-contained test cases
- External requests use httpbin.org for predictable responses
- No file system dependencies

### Validation Approach
- Tests validate both successful execution and error conditions
- Tests check side effects (DOM mutations, console output)
- Tests verify API contracts (return types, parameters)

## Coverage

These tests align with the TEST_MATRIX.md specification:
- **Section 2.3: JavaScript Execution (P0)** - Fully covered
- Validates LibJS engine integration with LibWeb
- Tests both synchronous and asynchronous JavaScript
- Covers modern ES6+ features and Web APIs

## Challenges & Considerations

1. **Shadow DOM**: May not be fully implemented - test uses test.skip() if unsupported
2. **Fetch API**: Network requests may fail in isolated test environments
3. **Observer APIs**: IntersectionObserver, MutationObserver, ResizeObserver may have limited support
4. **Module System**: True ES6 modules require special context, simulated in tests
5. **Timing**: setTimeout/setInterval tests use reasonable timeouts to avoid flakiness

## Future Enhancements

- Add WebGL context tests
- Add Web Workers tests
- Add ServiceWorker tests
- Add WebAssembly tests
- Add WebRTC tests
- Performance benchmarking for JavaScript execution
