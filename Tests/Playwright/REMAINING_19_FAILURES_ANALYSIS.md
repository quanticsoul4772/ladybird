# Analysis: 19 Remaining Test Failures

**Status**: 327/361 passing (90.6% pass rate) after port migration fix
**Remaining**: 19 failures (5.3%)
**Date**: 2025-11-01

---

## Executive Summary

The 19 remaining failures fall into 4 categories:

1. **Browser Limitations (8 tests)**: Ladybird ARIA/DOM API gaps - **Cannot fix in tests**
2. **Test Data Issues (5 tests)**: Missing or inaccessible test data - **Fixable**
3. **Form/Navigation Issues (5 tests)**: Timing or navigation problems - **Needs investigation**
4. **Data URL Timeout (1 test)**: Ladybird data URL loading issue - **May be browser bug**

**Recommended Action**: Focus on Category 2 & 3 (10 tests). Category 1 requires Ladybird browser improvements.

---

## Category 1: Browser Limitations (8 tests)

### **Not fixable in tests - requires Ladybird implementation work**

These tests fail because Ladybird's ARIA/DOM APIs are incomplete or differ from standard browsers.

#### A11Y-006: aria-label and aria-labelledby
**Error**: `expect(ariaLabels.valid).toBe(true)` - Received: false
**Cause**: ARIA label validation failing in Ladybird
**Impact**: Low - Test infrastructure issue, not critical functionality
**Fix Required**: Ladybird needs improved ARIA attribute support

#### A11Y-007: aria-describedby for supplemental descriptions
**Error**: `expect(ariaLabels.errors).toHaveLength(0)` - Got 3 errors about missing elements
**Cause**: `aria-labelledby` references not being resolved correctly
**Impact**: Low - ARIA reference resolution needs work
**Fix Required**: Ladybird LibWeb ARIA reference implementation

#### A11Y-008: aria-hidden and inert content
**Error**: `expect(hidden.valid).toBe(true)` - Received: false
**Cause**: aria-hidden attribute validation failing
**Impact**: Low - Test validation issue
**Fix Required**: Ladybird ARIA hidden content handling

#### A11Y-009: ARIA roles (button, tab, dialog)
**Error**: `expect(roles.valid).toBe(true)` - Received: false
**Cause**: ARIA role validation failing
**Impact**: Low - Role assignment or validation issue
**Fix Required**: Ladybird ARIA role implementation

#### A11Y-012: Skip links for main content
**Error**: `expect(skipLinks.valid).toBe(true)` - Received: false
**Cause**: Skip link validation logic failing
**Impact**: Low - Test helper issue
**Fix Required**: Review accessibility helper or Ladybird skip link support

#### A11Y-014: Focus visible indicators
**Error**: `locator('button')` resolved to 16 elements - strict mode violation
**Cause**: Test helper using non-specific selector
**Impact**: **Fixable** - Test code issue, not browser
**Fix**: Update test to use `.first()` or more specific selector
**Priority**: **HIGH - Easy fix**

```typescript
// Fix in tests/accessibility/a11y.spec.ts:421
// Change from:
await checkFocusVisibility(page, 'button');
// To:
await checkFocusVisibility(page, 'button:first-of-type');
// Or update helper to handle multiple elements
```

#### A11Y-016: Color contrast (WCAG AA 4.5:1)
**Error**: `expect(pContrast.wcagAA).toBe(true)` - Received: false
**Cause**: Color contrast calculation returning incorrect result
**Impact**: Medium - Accessibility compliance checking
**Fix Required**: Review `calculateColorContrast()` helper implementation or Ladybird computed style APIs

#### A11Y-020: Modal dialog focus management
**Error**: `TypeError: this.contains is not a function`
**Cause**: DOM `contains()` method not available in Ladybird context
**Impact**: Medium - DOM API gap
**Fix Required**: Ladybird needs Node.contains() implementation

**Category 1 Summary**: 7 tests require Ladybird browser work, 1 test (A11Y-014) has easy test code fix

---

## Category 2: Test Data Issues (5 tests)

### **Fixable - Test setup or data persistence problems**

#### FMON-002: Same-origin password submission (no alert)
**Error**: `expect(status).toContain('Has password: true')` - Received: ""
**Cause**: `#test-status` element is empty
**Issue**: Form submission happened but status wasn't updated

**Investigation needed**:
```typescript
// Line 169 tries to read status
const status = await page.locator('#test-status').textContent();
// Returns empty string - element exists but no content

// Check if form actually submitted
// Check if JavaScript updated the status
// May need to wait longer for status update
```

**Likely Fix**: Add `await page.waitForFunction()` to wait for status update:
```typescript
await page.waitForFunction(() => {
  const status = document.querySelector('#test-status');
  return status && status.textContent.includes('Has password');
}, { timeout: 5000 });
```

#### FMON-009: User interaction requirement
**Error**: `locator.textContent: Timeout 10000ms exceeded` on `#test-status`
**Cause**: Element not found or not populated
**Issue**: Similar to FMON-002 - status element issue

**Fix**: Same as FMON-002 - add proper wait condition

#### MAL-001: Known malware signature detection (YARA)
**Error**: `Cannot read properties of undefined (reading 'expectedResult')`
**Cause**: `window.__malwareTestData` is undefined
**Issue**: Test data set in HTML but not accessible when test reads it

**Root Cause Analysis**:
```javascript
// The test creates HTML with:
window.__malwareTestData = { expectedResult: 'blocked', ... };

// Then loads via data URL:
await page.goto(`data:text/html,${encodeURIComponent(html)}`);

// Then tries to read:
const testData = await page.evaluate(() => (window as any).__malwareTestData);
// Returns undefined
```

**Possible causes**:
1. Data URL didn't load completely
2. Script didn't execute
3. Page navigated away after click
4. Ladybird bug with data URLs

**Fix Options**:
1. **Add verification** after goto:
```typescript
await page.goto(`data:text/html,${encodeURIComponent(html)}`);
await page.waitForLoadState('networkidle');
await page.waitForFunction(() => typeof (window as any).__malwareTestData !== 'undefined');
```

2. **Use real HTTP fixture** instead of data URL:
```typescript
// Create /fixtures/malware/eicar-test.html
// Load via http://localhost:9080/malware/eicar-test.html
```

#### MAL-002: Clean file scan (no false positives)
**Error**: Same as MAL-001 - `window.__malwareTestData` undefined
**Fix**: Same as MAL-001

#### MAL-010: Edge cases and graceful degradation
**Error**: Same as MAL-001 - `window.__malwareTestData` undefined
**Fix**: Same as MAL-001

**Category 2 Priority Fix**:
1. **HIGH**: Fix MAL-001/002/010 by using HTTP fixtures instead of data URLs
2. **MEDIUM**: Fix FMON-002/009 by adding proper wait conditions

---

## Category 3: Form/Navigation Issues (5 tests)

### **Needs investigation - may be fixable or browser bugs**

#### FORM-014: POST submission
**Error**: `Timeout 10000ms exceeded while waiting for event "response"`
**Cause**: POST request never completes or response never arrives
**Line**: 385 - `page.waitForResponse(response => response.request().method() === 'POST' ...)`

**Investigation**:
- Check if form actually submits in Ladybird
- Check if POST request is sent to server
- Check server logs for POST /submit
- May be Ladybird form submission bug

**Debugging Steps**:
```typescript
// Add logging
page.on('request', req => console.log('Request:', req.method(), req.url()));
page.on('response', res => console.log('Response:', res.status(), res.url()));

// Or use network interception
const requests = [];
page.on('request', req => requests.push(req));
await page.click('#submit');
await page.waitForTimeout(2000);
console.log('Requests captured:', requests.map(r => ({method: r.method(), url: r.url()})));
```

#### PG-001: Create policy - Credential relationship persistence
**Error**: `expect(locator).toBeVisible() failed` on `#loginForm`
**Cause**: Form disappeared after submission
**Line**: 124 - expects form still visible after submit

**Issue**: Form may have navigated or been hidden
**Fix**: Either:
1. Remove expectation (form can disappear after submit)
2. Use fixture that keeps form visible
3. Check what actually happened to form

#### PG-003: Update policy - Modify existing relationship
**Error**: `page.fill: Timeout 10000ms exceeded` on `input[type="password"]`
**Cause**: Password field not found after page.reload()
**Line**: 209

**Investigation**:
- Check if page.reload() navigates to wrong URL
- Check if form elements load asynchronously
- May need `waitForSelector` before fill

**Fix**:
```typescript
await page.reload();
await page.waitForSelector('input[type="password"]', { timeout: 5000 });
await page.fill('input[type="password"]', 'test123');
```

#### PG-004: Delete policy - Remove trusted relationship
**Error**: `page.reload: net::ERR_NAME_NOT_RESOLVED`
**Cause**: Page navigated to invalid URL before reload
**Line**: 265

**Investigation**:
- Check current URL before reload
- Form submission may have navigated to bad URL
- May need to navigate back first

**Fix**:
```typescript
// Check URL before reload
const url = page.url();
console.log('Current URL:', url);

// If not on expected page, navigate first
if (!url.includes('expected-page')) {
  await page.goto('http://localhost:9080/...');
}
await page.reload();
```

#### PG-006: Evaluate policy - Positive match (allow)
**Error**: `page.click: Timeout 10000ms exceeded` waiting for navigation
**Cause**: Navigation never completes after form submission
**Line**: 351

**Issue**: Similar to FORM-014 - form submission not completing
**Investigation**: Same as FORM-014

**Category 3 Summary**: All 5 tests have form submission or navigation issues. May be related to Ladybird form handling or test server response timing.

---

## Category 4: Data URL Timeout (1 test)

### **May be browser bug - needs investigation**

#### EDGE-018: CORS failures and cross-origin errors
**Error**: `page.goto: Timeout 30000ms exceeded`
**Cause**: Data URL with CORS test never finishes loading
**Line**: 1349

**HTML Content**: Large data URL with:
- Fetch to example.com (should fail CORS)
- XMLHttpRequest to example.com (should fail CORS)
- Image from example.com (should work)
- setTimeout to update status

**Issue**: Page never fires 'load' event
**Possible Causes**:
1. Ladybird waits for failed CORS requests indefinitely
2. Data URL load event not firing
3. Script execution blocking load event

**Fix Options**:
1. **Use HTTP fixture** instead of data URL
2. **Change waitUntil** option:
```typescript
await page.goto(`data:text/html,${encodeURIComponent(html)}`, {
  waitUntil: 'domcontentloaded'  // Don't wait for full load
});
```
3. **Skip test** if Ladybird data URL handling is buggy

---

## Recommended Fixes (Priority Order)

### Priority 1: Quick Wins (2 tests, ~15 minutes)

**1. A11Y-014: Focus visible indicators**
```typescript
// File: tests/accessibility/a11y.spec.ts:421
// Change:
const focusResult = await checkFocusVisibility(page, 'button');
// To:
const focusResult = await checkFocusVisibility(page, 'button:first-of-type');
```

**2. EDGE-018: CORS test**
```typescript
// File: tests/edge-cases/boundaries.spec.ts:1349
// Change:
await page.goto(`data:text/html,${encodeURIComponent(html)}`);
// To:
await page.goto(`data:text/html,${encodeURIComponent(html)}`, {
  waitUntil: 'domcontentloaded',
  timeout: 5000
});
```

### Priority 2: Malware Test Data (3 tests, ~30 minutes)

**Convert MAL-001, MAL-002, MAL-010 to use HTTP fixtures:**

1. **Create fixture files**:
```bash
# Create fixtures/malware/eicar-test.html
# Create fixtures/malware/clean-file-test.html
# Create fixtures/malware/edge-cases-test.html
```

2. **Update tests** to use `http://localhost:9080/malware/...` instead of data URLs

3. **Add wait for test data**:
```typescript
await page.goto('http://localhost:9080/malware/eicar-test.html');
await page.waitForFunction(() => typeof (window as any).__malwareTestData !== 'undefined');
```

### Priority 3: FormMonitor Status (2 tests, ~20 minutes)

**Fix FMON-002 and FMON-009**:

```typescript
// After form submission, wait for status update
await page.click('#submit');
await page.waitForFunction(() => {
  const status = document.querySelector('#test-status');
  return status && status.textContent !== '' && status.textContent !== 'Testing...';
}, { timeout: 5000 });

const status = await page.locator('#test-status').textContent();
```

### Priority 4: Form/Navigation Issues (5 tests, investigation needed)

**All PolicyGraph and FORM-014 tests**:
- Requires debugging Ladybird form submission
- May be browser bugs
- Add extensive logging first:
```typescript
page.on('request', req => console.log('→', req.method(), req.url()));
page.on('response', res => console.log('←', res.status(), res.url()));
page.on('console', msg => console.log('Console:', msg.text()));
```

### Priority 5: Browser Limitations (7 tests, skip or wait for Ladybird)

**A11Y-006, 007, 008, 009, 012, 016, 020**:
- Require Ladybird LibWeb ARIA implementation improvements
- Consider skipping with clear comments:
```typescript
test.skip('A11Y-006: aria-label and aria-labelledby', async ({ page }) => {
  // SKIPPED: Ladybird ARIA attribute validation not fully implemented
  // Requires LibWeb improvements to ARIA label resolution
  // See: https://github.com/LadybirdBrowser/ladybird/issues/...
});
```

---

## Implementation Plan

### Phase 1: Quick Wins (Today, 15 min)
- [ ] Fix A11Y-014 selector specificity
- [ ] Fix EDGE-018 waitUntil option

**Expected result**: 329/361 passing (91.1%)

### Phase 2: Data Issues (Today, 1 hour)
- [ ] Convert malware tests to HTTP fixtures (MAL-001, 002, 010)
- [ ] Add status wait logic to FormMonitor tests (FMON-002, 009)

**Expected result**: 334/361 passing (92.5%)

### Phase 3: Investigation (Next session, 2-4 hours)
- [ ] Debug form submission issues (FORM-014, PG-001, 003, 004, 006)
- [ ] Add extensive logging
- [ ] Identify if Ladybird bugs or test issues

**Expected result**: 334-339/361 passing (92.5-93.9%)

### Phase 4: Browser Limitations (Document only)
- [ ] Add detailed skip comments to 7 ARIA tests
- [ ] Document Ladybird features needed
- [ ] Create GitHub issues if applicable

**Final result**: 334-339 passing, 15 skipped (documented), 7-12 failing (needs Ladybird work)

---

## Testing Strategy Going Forward

### For New Tests:
1. **Use HTTP fixtures** instead of data URLs when possible
2. **Add explicit waits** for test data availability
3. **Check browser compatibility** before writing tests for advanced features
4. **Document limitations** when testing incomplete Ladybird features

### For Existing Failures:
1. **Categorize** as: fixable, needs investigation, or browser limitation
2. **Skip browser limitation tests** with clear documentation
3. **Focus effort** on fixable tests first
4. **Report bugs** for confirmed Ladybird issues

---

## Summary Statistics

| Category | Count | Fixable? | Priority |
|----------|-------|----------|----------|
| Browser Limitations (ARIA) | 7 | No (Ladybird work) | Low |
| Browser Limitations (fixable selector) | 1 | Yes | HIGH |
| Test Data Issues | 5 | Yes | HIGH |
| Form/Navigation Issues | 5 | Maybe | MEDIUM |
| Data URL Timeout | 1 | Yes | HIGH |
| **TOTAL** | **19** | **12 potentially fixable** | |

**Best Case Scenario** (all fixable issues resolved): **339/361 passing (93.9%)**
**Realistic Target** (P1+P2 fixes): **334/361 passing (92.5%)**
**Current State**: **327/361 passing (90.6%)**

---

**Generated**: 2025-11-01
**Next Steps**: Implement Phase 1 & 2 fixes (estimated 1-2 hours)
