# Form Handling Tests Documentation

## Overview

This document provides comprehensive documentation for the **42 Form Handling Tests** (FORM-001 to FORM-042) that test HTML form functionality in the Ladybird browser.

## Test Coverage

### Total Tests: 42

The test suite is organized into 6 categories:

1. **Input Types** (12 tests: FORM-001 to FORM-012)
2. **Form Submission** (8 tests: FORM-013 to FORM-020)
3. **Form Validation** (10 tests: FORM-021 to FORM-030)
4. **Form Events** (6 tests: FORM-031 to FORM-036)
5. **Advanced Features** (6 tests: FORM-037 to FORM-042)

---

## Test Categories

### 1. Input Types (FORM-001 to FORM-012)

Tests for all standard HTML input types and form controls.

| Test ID | Name | Description |
|---------|------|-------------|
| FORM-001 | Text Input Type | Tests basic text input field functionality |
| FORM-002 | Password Input Type | Tests password field masking and value handling |
| FORM-003 | Email Input Type | Tests email field with type validation |
| FORM-004 | Number Input Type | Tests number input with min/max/step attributes |
| FORM-005 | Tel Input Type | Tests telephone input field |
| FORM-006 | URL Input Type | Tests URL input field |
| FORM-007 | Date Input Type | Tests date picker input |
| FORM-008 | Time Input Type | Tests time picker input |
| FORM-009 | Checkbox Input Type | Tests checkbox toggling and multi-select |
| FORM-010 | Radio Input Type | Tests radio button exclusive selection |
| FORM-011 | File Input Type | Tests file upload field |
| FORM-012 | Hidden Input Type | Tests hidden form fields |

**Key Assertions:**
- Input type attributes are correct
- Values can be set and retrieved
- Checkboxes toggle properly
- Radio buttons are mutually exclusive
- File inputs accept `accept` attribute

**Test Coverage:**
- All 12 standard input types
- Basic interaction (fill, click, get value)
- Type-specific behavior (checked state for checkboxes, etc.)
- Attributes (min, max, pattern, accept, etc.)

---

### 2. Form Submission (FORM-013 to FORM-020)

Tests for form submission methods and techniques.

| Test ID | Name | Description |
|---------|------|-------------|
| FORM-013 | GET Submission | Tests form submission with GET method |
| FORM-014 | POST Submission | Tests form submission with POST method |
| FORM-015 | multipart/form-data | Tests multipart form encoding |
| FORM-016 | JavaScript Form Submission | Tests programmatic form submission |
| FORM-017 | Enter Key Submission | Tests form submission via Enter key |
| FORM-018 | Submit Button Variants | Tests different submit button types |
| FORM-019 | Validation Prevents Submission | Tests that invalid forms don't submit |
| FORM-020 | preventDefault() Stops Submission | Tests event.preventDefault() in submit handler |

**Key Assertions:**
- Forms submit with correct HTTP method
- Form data is properly encoded
- Enter key triggers submission
- preventDefault() blocks default submission
- Invalid forms don't submit

**Test Coverage:**
- GET vs POST methods
- Form encoding types (application/x-www-form-urlencoded, multipart/form-data)
- Keyboard submission (Enter key)
- JavaScript event handling
- Form validation interaction with submission

---

### 3. Form Validation (FORM-021 to FORM-030)

Tests for HTML5 form validation features.

| Test ID | Name | Description |
|---------|------|-------------|
| FORM-021 | Required Attribute | Tests required field validation |
| FORM-022 | Pattern Attribute | Tests regex pattern validation |
| FORM-023 | Min/Max Attributes | Tests min/max validation on numbers |
| FORM-024 | MinLength/MaxLength | Tests string length validation |
| FORM-025 | Email Validation | Tests email type validation |
| FORM-026 | URL Validation | Tests URL type validation |
| FORM-027 | Custom Validity | Tests setCustomValidity() API |
| FORM-028 | :invalid Pseudo-class | Tests CSS styling of invalid fields |
| FORM-029 | Validation Messages | Tests validationMessage property |
| FORM-030 | novalidate Attribute | Tests disabling browser validation |

**Key Assertions:**
- checkValidity() returns correct state
- validationMessage contains appropriate text
- Invalid fields prevent submission
- Custom validity messages work
- novalidate disables HTML5 validation
- :invalid pseudo-class applies correctly

**Test Coverage:**
- All HTML5 validation attributes
- Constraint validation API (checkValidity, validationMessage)
- Custom validity messages
- CSS validation pseudo-classes
- Form-level vs field-level validation

---

### 4. Form Events (FORM-031 to FORM-036)

Tests for form and form field events.

| Test ID | Name | Description |
|---------|------|-------------|
| FORM-031 | Input Event | Tests input event fires on value change |
| FORM-032 | Change Event | Tests change event fires on blur |
| FORM-033 | Submit Event | Tests form submit event |
| FORM-034 | Reset Event | Tests form reset event |
| FORM-035 | Focus/Blur Events | Tests focus and blur events |
| FORM-036 | Event Bubbling | Tests event bubbling in forms |

**Key Assertions:**
- input event fires on text changes
- change event fires on blur
- submit event fires when form is submitted
- reset event fires when form is reset
- focus/blur events fire correctly
- Events bubble correctly through form hierarchy

**Test Coverage:**
- All major form-related events
- Event order and timing
- Event bubbling and capturing
- Event delegation in forms

---

### 5. Advanced Features (FORM-037 to FORM-042)

Tests for advanced form features and APIs.

| Test ID | Name | Description |
|---------|------|-------------|
| FORM-037 | Autocomplete Attribute | Tests autocomplete hints |
| FORM-038 | Disabled/Readonly | Tests disabled and readonly attributes |
| FORM-039 | Fieldset/Legend | Tests fieldset grouping and legend |
| FORM-040 | Form Attribute Binding | Tests form attribute for elements outside form |
| FORM-041 | FormAction Override | Tests formaction attribute on buttons |
| FORM-042 | FormData API | Tests FormData API for collecting form data |

**Key Assertions:**
- disabled fields are not submitted
- readonly fields are submitted
- fieldset[disabled] disables all contained fields
- form attribute associates elements with forms
- formaction overrides form action
- FormData API correctly collects form values
- Form elements outside form can be associated via form attribute

**Test Coverage:**
- Form state attributes (disabled, readonly)
- Form grouping (fieldset, legend)
- Form element association (form attribute)
- Form submission customization (formaction, formmethod)
- Modern APIs (FormData)

---

## Running the Tests

### Run All Form Tests

```bash
npm test -- tests/forms/form-handling.spec.ts
```

### Run Specific Test Category

```bash
# Input Types only
npm test -- tests/forms/form-handling.spec.ts -g "Input Types"

# Form Submission only
npm test -- tests/forms/form-handling.spec.ts -g "Form Submission"

# Form Validation only
npm test -- tests/forms/form-handling.spec.ts -g "Form Validation"

# Form Events only
npm test -- tests/forms/form-handling.spec.ts -g "Form Events"

# Advanced Features only
npm test -- tests/forms/form-handling.spec.ts -g "Advanced Form Features"
```

### Run Specific Test

```bash
npm test -- tests/forms/form-handling.spec.ts -g "FORM-001"
```

### Run with Different Browser

```bash
# Ladybird (default)
npm test -- tests/forms/form-handling.spec.ts

# Chromium comparison
npx playwright test --project chromium-reference tests/forms/form-handling.spec.ts
```

---

## Test Fixtures

The test suite includes several HTML form fixtures in `/fixtures/forms/`:

### `basic-form.html`
A comprehensive registration form with:
- All input types
- Form submission
- Event tracking
- Validation helpers
- User-friendly styling

**Features:**
- Full registration form with personal info, password, phone, website, date of birth, contact time
- Checkboxes for interests
- Radio buttons for newsletter
- Select dropdown for country
- Textarea for comments
- Terms and conditions checkbox
- Form status messages

### `validation-form.html`
Dedicated validation testing page with:
- Required field validation
- Email validation
- URL validation
- Pattern validation
- Min/Max validation
- MinLength/MaxLength validation
- Custom validity examples
- novalidate testing
- Combined validation rules

**Features:**
- Separate forms for each validation type
- Visual feedback (color-coded results)
- Detailed validation messages
- Multiple validation rules combined

### `advanced-form.html`
Advanced form features testing page with:
- Fieldset and legend grouping
- Disabled and readonly fields
- Form attribute binding (elements outside form)
- FormAction override
- FormMethod override
- FormData API examples
- Autocomplete attributes
- Multiple independent forms

**Features:**
- Toggle fieldset disabled state
- Test disabled vs readonly behavior
- FormData API demonstrations
- Autocomplete field examples
- Multi-action form handling

---

## Helper Utilities

The test suite includes comprehensive helper functions in `/helpers/form-testing-utils.ts`:

### Core Functions

#### `getFormValues(page, formId)`
Get all form field values as an object.
```typescript
const values = await getFormValues(page, 'test-form');
// Returns: { name: 'John', email: 'john@example.com', ... }
```

#### `setFormValues(page, values, formId)`
Set multiple form field values at once.
```typescript
await setFormValues(page, {
  name: 'John Doe',
  email: 'john@example.com',
  agree: true
}, 'test-form');
```

#### `submitForm(page, formId, submitButtonSelector)`
Submit a form and get submission details.
```typescript
const result = await submitForm(page, 'test-form');
// Returns: { success, method, action, data, timestamp }
```

#### `resetForm(page, formId)`
Reset a form to its initial state.
```typescript
await resetForm(page, 'test-form');
```

#### `checkFormValidity(page, formId)`
Check form validity and get validation details.
```typescript
const validation = await checkFormValidity(page, 'test-form');
// Returns: { isValid, invalidFields, validationMessages, timestamp }
```

#### `fillFormField(page, fieldName, value, formId)`
Fill a specific form field.
```typescript
await fillFormField(page, 'email', 'test@example.com', 'test-form');
```

#### `getFormFieldValue(page, fieldName, formId)`
Get value of a specific field.
```typescript
const email = await getFormFieldValue(page, 'email', 'test-form');
```

#### `waitForFormValid(page, formId, timeout)`
Wait for form to become valid.
```typescript
await waitForFormValid(page, 'test-form', 5000);
```

#### `getFormFieldsInfo(page, formId)`
Get info about all form fields.
```typescript
const fields = await getFormFieldsInfo(page, 'test-form');
// Returns array of FormFieldValue objects
```

#### `isFieldDisabled(page, fieldName, formId)`
Check if a field is disabled.
```typescript
const disabled = await isFieldDisabled(page, 'address-field', 'test-form');
```

#### `getFormEvents(page)`
Get tracked form events (if page implements tracking).
```typescript
const events = await getFormEvents(page);
// Returns: { submit: 1, reset: 0, input: 5, change: 3, ... }
```

#### `getFormDataAsJSON(page, formId)`
Get FormData API result as JSON.
```typescript
const data = await getFormDataAsJSON(page, 'test-form');
```

---

## Test Server Integration

The tests use the local HTTP test server for cross-origin testing:

- **Port 8080**: Primary test server (localhost:8080)
- **Port 8081**: Secondary server for cross-origin tests (localhost:8081)

### Available Endpoints

#### `/submit` (POST)
Generic form submission endpoint.

**Response:**
```json
{
  "received": { /* submitted form data */ },
  "metadata": {
    "origin": "http://localhost:8080",
    "referer": "...",
    "userAgent": "...",
    "timestamp": "2024-01-15T10:30:00.000Z"
  },
  "hasPasswordField": true,
  "hasEmailField": true
}
```

#### `/login` (POST)
Simulates authentication endpoint.

**Request:**
```
email=user@example.com&password=secret123
```

**Response (Success):**
```json
{
  "success": true,
  "message": "Login successful",
  "user": {
    "email": "user@example.com",
    "token": "mock-auth-token-1234567890"
  }
}
```

---

## Best Practices for Form Testing

### 1. Always Use Explicit Waits
```typescript
// Good
await page.waitForSelector('button[type="submit"]');
await page.click('button[type="submit"]');

// Avoid
await page.click('button[type="submit"]'); // May fail if not visible
```

### 2. Test Both Happy Path and Error Cases
```typescript
// Test valid submission
await fillFormField(page, 'email', 'valid@example.com');
expect(await checkFormValidity(page)).resolves.toBeTruthy();

// Test invalid submission
await fillFormField(page, 'email', 'invalid-email');
expect(await checkFormValidity(page)).resolves.toBeFalsy();
```

### 3. Use Helper Functions Instead of Raw Selectors
```typescript
// Good
await fillFormField(page, 'email', 'test@example.com', 'test-form');

// Avoid
await page.fill('input[name="email"]', 'test@example.com');
```

### 4. Verify Form State Before Actions
```typescript
const isValid = await checkFormValidity(page, 'test-form');
if (!isValid) {
  // Handle invalid state
}
```

### 5. Test Data Collection
```typescript
const values = await getFormValues(page, 'test-form');
expect(values.name).toBe('John Doe');
expect(values.email).toBe('john@example.com');
```

### 6. Verify Event Firing
```typescript
const events = await getFormEvents(page);
expect(events.submit).toBe(1);
expect(events.input).toBeGreaterThan(0);
```

---

## Debugging Form Tests

### View Form State
```typescript
const state = await getFormState(page);
console.log('Form state:', state);

const values = await getFormValues(page, 'test-form');
console.log('Form values:', values);

const validation = await checkFormValidity(page, 'test-form');
console.log('Validation:', validation);
```

### Inspect Field Properties
```typescript
const fieldInfo = await getFormFieldsInfo(page, 'test-form');
console.log('Fields:', fieldInfo);

const isDisabled = await isFieldDisabled(page, 'field-name');
console.log('Disabled:', isDisabled);
```

### Take Screenshots on Failure
The test configuration automatically captures screenshots on failure:
```
test-results/[test-name]-screenshot-1.png
```

### Review Videos
Test videos are captured for failed tests:
```
test-results/[test-name].webm
```

---

## Coverage Summary

| Category | Tests | Coverage |
|----------|-------|----------|
| Input Types | 12 | text, password, email, number, tel, url, date, time, checkbox, radio, file, hidden |
| Submission | 8 | GET, POST, multipart, JS, keyboard, button variants, validation, preventDefault |
| Validation | 10 | required, pattern, min/max, length, email, URL, custom, CSS, messages, novalidate |
| Events | 6 | input, change, submit, reset, focus/blur, bubbling |
| Advanced | 6 | autocomplete, disabled/readonly, fieldset, form attr, formaction, FormData |
| **TOTAL** | **42** | **Comprehensive HTML form coverage** |

---

## Known Limitations

1. **File Upload**: Tests for file input type don't perform actual file uploads due to browser automation limitations
2. **Browser Validation UI**: Tests verify validation state, not the visual validation UI (e.g., browser error messages)
3. **Complex Patterns**: Very complex regex patterns in pattern attributes may have browser-specific behavior
4. **Autocomplete**: Browser autocomplete dropdown behavior cannot be reliably tested in automation

---

## CI/CD Integration

### GitHub Actions Configuration

Add to `.github/workflows/test.yml`:
```yaml
- name: Run Form Handling Tests
  run: npm test -- tests/forms/form-handling.spec.ts
```

### Test Results

Test results are saved to:
- `test-results/results.json` - JSON format
- `playwright-report/index.html` - HTML report
- Screenshots and videos for failed tests

---

## Future Enhancements

Potential improvements for form testing:

1. **Advanced Pattern Tests** - More complex regex patterns
2. **File Upload Tests** - Integration with actual file handling
3. **Accessibility Tests** - ARIA attributes and semantic HTML
4. **Performance Tests** - Form interaction timing and responsiveness
5. **Security Tests** - XSS prevention, CSRF token validation
6. **Cross-browser Tests** - Safari, Firefox specific behavior
7. **Mobile Tests** - Touch interactions and mobile-specific inputs
8. **Internationalization** - Locale-specific validation and formatting

---

## Maintenance Notes

- **Update Test Fixtures**: If modifying HTML fixtures, ensure backward compatibility
- **Helper Function Updates**: Keep utility functions generic for reusability
- **Test Server**: Ensure test-server.js is running before tests
- **Dependencies**: Update Playwright regularly for browser compatibility
- **Browser Updates**: Test with latest Ladybird builds for regression testing

---

## Contact and Issues

For issues or questions about form tests:

1. Check existing GitHub issues
2. Review test logs in `test-results/`
3. Consult documentation in `docs/`
4. Run tests with verbose output for debugging

---

## References

- [MDN HTML Forms](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/form)
- [HTML Standard - Forms](https://html.spec.whatwg.org/multipage/forms.html)
- [Constraint Validation API](https://html.spec.whatwg.org/multipage/forms.html#constraint-validation)
- [Playwright Test Documentation](https://playwright.dev/docs/intro)
- [FormData API](https://developer.mozilla.org/en-US/docs/Web/API/FormData)

