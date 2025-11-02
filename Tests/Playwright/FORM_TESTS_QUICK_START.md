# Form Handling Tests - Quick Start Guide

## What's New

A comprehensive test suite for HTML form handling with **42 tests** covering:

- ✅ **Input Types** (12 tests) - All HTML5 input types
- ✅ **Form Submission** (8 tests) - GET, POST, multipart, JavaScript
- ✅ **Form Validation** (10 tests) - HTML5 validation attributes
- ✅ **Form Events** (6 tests) - input, change, submit, reset, focus/blur
- ✅ **Advanced Features** (6 tests) - fieldset, form binding, FormData API

## Files Created

```
tests/forms/
└── form-handling.spec.ts          # 42 comprehensive tests

fixtures/forms/
├── basic-form.html                # Registration form with all input types
├── validation-form.html           # Validation testing page
└── advanced-form.html             # Advanced features demo

helpers/
└── form-testing-utils.ts          # Reusable testing utilities

docs/
└── FORM_HANDLING_TESTS.md        # Complete documentation
```

## Quick Start

### 1. Start Test Server

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npm run test-server
# Server starts on localhost:8080 and localhost:8081
```

### 2. Run All Form Tests

```bash
npm test -- tests/forms/form-handling.spec.ts
```

### 3. Run Specific Tests

```bash
# Run one category
npm test -- tests/forms/form-handling.spec.ts -g "Input Types"
npm test -- tests/forms/form-handling.spec.ts -g "Form Validation"

# Run specific test
npm test -- tests/forms/form-handling.spec.ts -g "FORM-001"
```

### 4. View Results

```
playwright-report/index.html    # Open in browser
test-results/                   # Screenshots, videos, JSON results
```

## Test Structure

Each test follows this pattern:

```typescript
test('FORM-###: Description', { tag: '@p0' }, async ({ page }) => {
  // 1. Create or navigate to form
  const html = createFormHtml({ /* config */ });
  await page.goto(`data:text/html,${encodeURIComponent(html)}`);

  // 2. Interact with form
  await page.fill('#field', 'value');
  await page.click('#submit-btn');

  // 3. Assert behavior
  expect(await page.locator('#field').inputValue()).toBe('value');
});
```

## Key Helper Functions

```typescript
import * as formUtils from '../helpers/form-testing-utils';

// Get all form values
const values = await formUtils.getFormValues(page, 'test-form');

// Set multiple fields at once
await formUtils.setFormValues(page, {
  name: 'John',
  email: 'john@example.com'
}, 'test-form');

// Check form validity
const validation = await formUtils.checkFormValidity(page, 'test-form');
console.log(validation.isValid, validation.invalidFields);

// Submit form
const result = await formUtils.submitForm(page, 'test-form');

// Fill specific field
await formUtils.fillFormField(page, 'email', 'test@example.com');

// Get field value
const value = await formUtils.getFormFieldValue(page, 'email');

// Wait for form to be valid
await formUtils.waitForFormValid(page, 'test-form', 5000);

// Get all field info
const fields = await formUtils.getFormFieldsInfo(page, 'test-form');

// Check if field is disabled
const isDisabled = await formUtils.isFieldDisabled(page, 'field-name');

// Get form data as JSON
const data = await formUtils.getFormDataAsJSON(page, 'test-form');
```

## Test Categories at a Glance

### Input Types (FORM-001 to FORM-012)
- FORM-001: Text input
- FORM-002: Password input
- FORM-003: Email input
- FORM-004: Number input (with min/max/step)
- FORM-005: Tel (telephone) input
- FORM-006: URL input
- FORM-007: Date input
- FORM-008: Time input
- FORM-009: Checkbox (toggle & value)
- FORM-010: Radio buttons (exclusive)
- FORM-011: File input (accept attribute)
- FORM-012: Hidden input

### Form Submission (FORM-013 to FORM-020)
- FORM-013: GET method
- FORM-014: POST method
- FORM-015: multipart/form-data encoding
- FORM-016: JavaScript submission (form.submit())
- FORM-017: Enter key submission
- FORM-018: Submit button variants
- FORM-019: Validation prevents submission
- FORM-020: preventDefault() blocks submission

### Form Validation (FORM-021 to FORM-030)
- FORM-021: required attribute
- FORM-022: pattern attribute (regex)
- FORM-023: min/max (numbers)
- FORM-024: minlength/maxlength (strings)
- FORM-025: email type validation
- FORM-026: URL type validation
- FORM-027: setCustomValidity() API
- FORM-028: :invalid pseudo-class styling
- FORM-029: validationMessage property
- FORM-030: novalidate disables validation

### Form Events (FORM-031 to FORM-036)
- FORM-031: input event (on change)
- FORM-032: change event (on blur)
- FORM-033: submit event
- FORM-034: reset event
- FORM-035: focus and blur events
- FORM-036: Event bubbling

### Advanced Features (FORM-037 to FORM-042)
- FORM-037: autocomplete attribute
- FORM-038: disabled and readonly attributes
- FORM-039: fieldset and legend grouping
- FORM-040: form attribute binding
- FORM-041: formaction override on buttons
- FORM-042: FormData API

## Coverage Matrix

| Feature | Count | Tests |
|---------|-------|-------|
| Input Types | 12 | FORM-001 to FORM-012 |
| Submission Methods | 8 | FORM-013 to FORM-020 |
| Validation Attributes | 10 | FORM-021 to FORM-030 |
| Form Events | 6 | FORM-031 to FORM-036 |
| Advanced Features | 6 | FORM-037 to FORM-042 |
| **TOTAL** | **42** | **FORM-001 to FORM-042** |

## Example Test Usage

### Writing a New Form Test

```typescript
test('FORM-XXX: Feature name', { tag: '@p0' }, async ({ page }) => {
  // Create test HTML
  const html = createFormHtml({
    id: 'test-form',
    method: 'POST',
    action: '/submit',
    content: `
      <label>Name: <input type="text" name="name" id="name"></label>
      <label>Email: <input type="email" name="email" id="email" required></label>
    `
  });

  // Navigate
  await page.goto(`data:text/html,${encodeURIComponent(html)}`);

  // Use helper functions
  import * as formUtils from '../helpers/form-testing-utils';

  // Fill fields
  await formUtils.setFormValues(page, {
    name: 'John Doe',
    email: 'john@example.com'
  });

  // Verify values
  const values = await formUtils.getFormValues(page);
  expect(values.name).toBe('John Doe');
  expect(values.email).toBe('john@example.com');

  // Check validity
  const validation = await formUtils.checkFormValidity(page);
  expect(validation.isValid).toBe(true);

  // Submit
  const result = await formUtils.submitForm(page);
  expect(result.success).toBe(true);
});
```

### Testing with Test Server

```typescript
test('FORM-XXX: Cross-origin submission', { tag: '@p0' }, async ({ page }) => {
  // Use real server endpoints
  await page.goto('http://localhost:8080/forms/basic-form.html');

  // Fill and submit
  await page.fill('input[name="email"]', 'test@example.com');
  await page.fill('input[name="password"]', 'password123');

  // Wait for response
  const responsePromise = page.waitForResponse(
    response => response.request().method() === 'POST'
  );

  await page.click('button[type="submit"]');
  const response = await responsePromise;

  expect(response.ok()).toBe(true);
});
```

## Available Fixtures

### basic-form.html (localhost:8080/forms/basic-form.html)
Full registration form with:
- Text, email, password fields
- Phone, website, date inputs
- Age (number) and time inputs
- Checkboxes and radio buttons
- Select dropdown
- Textarea
- Form submission tracking
- Event listener tracking

### validation-form.html (localhost:8080/forms/validation-form.html)
Validation testing with:
- Required field testing
- Email validation
- URL validation
- Pattern matching (regex)
- Min/max testing
- Length constraints
- Custom validity
- Form-level validation

### advanced-form.html (localhost:8080/forms/advanced-form.html)
Advanced features including:
- Fieldset grouping and disabled state
- Disabled and readonly fields
- Form attribute binding
- FormAction override
- FormMethod override
- FormData API demo
- Autocomplete examples
- Multiple independent forms

## Debugging Tips

### Check Form State
```typescript
const state = await page.evaluate(() => (window as any).__formState);
console.log('Form state:', state);
```

### View Form Events
```typescript
const events = await page.evaluate(() => (window as any).__formEvents);
console.log('Form events:', events);
```

### Inspect All Field Info
```typescript
import * as formUtils from '../helpers/form-testing-utils';
const fields = await formUtils.getFormFieldsInfo(page);
console.table(fields);
```

### Test Invalid Cases
```typescript
// Test with invalid email
await formUtils.fillFormField(page, 'email', 'not-an-email');
const validation = await formUtils.checkFormValidity(page);
expect(validation.isValid).toBe(false);
expect(validation.invalidFields).toContain('email');
```

### Capture Screenshots
```typescript
// Automatic on failure, or manual:
await page.screenshot({ path: 'form-state.png' });
```

## Common Issues & Solutions

### Issue: Tests timeout waiting for server
**Solution:** Ensure test-server.js is running on port 8080 and 8081
```bash
npm run test-server  # Start in separate terminal
```

### Issue: Form not found
**Solution:** Verify form ID matches in test
```typescript
// Use same ID as in createFormHtml or page
await formUtils.getFormValues(page, 'test-form');
```

### Issue: Fields not filling
**Solution:** Check field type and use appropriate method
```typescript
// For checkboxes/radios, click instead of fill
const checkbox = await page.locator('#field');
if (type === 'checkbox') {
  await checkbox.click();
} else {
  await checkbox.fill('value');
}
```

### Issue: Validation not working
**Solution:** Ensure HTML5 validation is enabled (no novalidate)
```typescript
const hasNovalidate = await page.locator('form').getAttribute('novalidate');
expect(hasNovalidate).toBeNull(); // Should not have novalidate
```

## Best Practices

1. **Use Helper Functions** - Don't use raw selectors
   ```typescript
   // Good
   await formUtils.setFormValues(page, { email: 'test@example.com' });

   // Avoid
   await page.fill('input[name="email"]', 'test@example.com');
   ```

2. **Verify Before Acting** - Check form state before submission
   ```typescript
   const validation = await formUtils.checkFormValidity(page);
   if (validation.isValid) {
     await formUtils.submitForm(page);
   }
   ```

3. **Test Both Happy and Sad Paths**
   ```typescript
   // Valid submission
   // Invalid submission
   // Empty required fields
   // Invalid email format
   ```

4. **Use Data URLs for Simple Tests**
   ```typescript
   const html = createFormHtml({ /* config */ });
   await page.goto(`data:text/html,${encodeURIComponent(html)}`);
   ```

5. **Use Test Server for Complex Tests**
   ```typescript
   await page.goto('http://localhost:8080/forms/basic-form.html');
   ```

## Performance Notes

- Tests run in parallel by default (set in playwright.config.ts)
- Use appropriate timeouts (5-30 seconds depending on action)
- Reuse form state where possible
- Clear events between tests if tracking them

## Documentation

Full documentation available in: `docs/FORM_HANDLING_TESTS.md`

Includes:
- Detailed test descriptions
- All helper function signatures
- Best practices and patterns
- Debugging strategies
- CI/CD integration
- Future enhancements

## Next Steps

1. ✅ Run all tests: `npm test -- tests/forms/form-handling.spec.ts`
2. ✅ Check results: `open playwright-report/index.html`
3. ✅ Review failures in `test-results/`
4. ✅ Modify tests as needed for your browser
5. ✅ Add to CI/CD pipeline

## Support

- Check logs in `test-results/`
- Review HTML fixtures for expected behavior
- Consult `docs/FORM_HANDLING_TESTS.md` for detailed info
- Check `helpers/form-testing-utils.ts` for available functions

---

**Created:** November 1, 2024
**Total Tests:** 42
**Categories:** 5
**Fixtures:** 3
**Helper Utilities:** 25+

Happy testing! 🚀
