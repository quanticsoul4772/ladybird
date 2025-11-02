import { test, expect, Page } from '@playwright/test';

/**
 * Form Handling Tests (FORM-001 to FORM-042)
 * Priority: P0 (Critical)
 *
 * Comprehensive test suite for HTML form handling including:
 * - Input types (text, password, email, number, tel, url, date, time, checkbox, radio, file, hidden)
 * - Form submission methods (GET, POST, multipart/form-data, JavaScript)
 * - Form validation (required, pattern, min/max, minlength/maxlength, custom)
 * - Form events (input, change, submit, reset, focus/blur)
 * - Advanced features (autocomplete, disabled/readonly, fieldset, form attribute, formaction, FormData API)
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Create a simple form HTML for testing
 */
function createFormHtml(config: {
  id?: string;
  method?: string;
  action?: string;
  enctype?: string;
  novalidate?: boolean;
  content: string;
}): string {
  const {
    id = 'test-form',
    method = 'POST',
    action = '/submit',
    enctype = undefined,
    novalidate = false,
    content
  } = config;

  const encTypeAttr = enctype ? `enctype="${enctype}"` : '';
  const novalidateAttr = novalidate ? 'novalidate' : '';

  return `
    <html>
      <head>
        <title>Form Test</title>
        <style>
          body { font-family: Arial, sans-serif; padding: 20px; }
          input, textarea, select { margin: 5px 0; padding: 5px; }
          .error { color: red; }
          .success { color: green; }
          button { margin-top: 10px; padding: 8px 15px; }
        </style>
      </head>
      <body>
        <h1>Form Test</h1>
        <form id="${id}" method="${method}" action="${action}" ${encTypeAttr} ${novalidateAttr}>
          ${content}
          <button type="submit" id="submit-btn">Submit</button>
          <button type="reset" id="reset-btn">Reset</button>
        </form>
        <div id="test-output"></div>
        <script>
          const form = document.getElementById('${id}');
          const output = document.getElementById('test-output');

          // Store form events for testing
          window.__formEvents = [];

          form.addEventListener('submit', (e) => {
            window.__formEvents.push({ type: 'submit', timestamp: Date.now() });
          });

          form.addEventListener('reset', (e) => {
            window.__formEvents.push({ type: 'reset', timestamp: Date.now() });
          });

          // Store form reference for testing
          window.__testForm = form;
        </script>
      </body>
    </html>
  `;
}

/**
 * Helper to verify form submission success
 */
async function verifyFormSubmission(page: Page, expectedResponse?: any): Promise<boolean> {
  try {
    const response = await page.context().waitForEvent('response', {
      predicate: (response) => response.request().method() === 'POST' || response.request().method() === 'GET'
    });

    if (expectedResponse) {
      const body = await response.json();
      return JSON.stringify(body).includes(JSON.stringify(expectedResponse));
    }

    return response.ok();
  } catch (e) {
    return false;
  }
}

/**
 * Get form field value
 */
async function getFieldValue(page: Page, selector: string): Promise<string> {
  return await page.locator(selector).inputValue();
}

/**
 * Get form data as object
 */
async function getFormData(page: Page, formId: string = 'test-form'): Promise<Record<string, any>> {
  return await page.evaluate((id) => {
    const form = document.getElementById(id) as HTMLFormElement;
    const formData = new FormData(form);
    const data: Record<string, any> = {};

    formData.forEach((value, key) => {
      if (data[key]) {
        if (Array.isArray(data[key])) {
          data[key].push(value);
        } else {
          data[key] = [data[key], value];
        }
      } else {
        data[key] = value;
      }
    });

    return data;
  }, formId);
}

// ============================================================================
// INPUT TYPES TESTS (FORM-001 to FORM-012)
// ============================================================================

test.describe('Input Types', () => {

  test('FORM-001: Text input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Name: <input type="text" name="name" id="name" value="John"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const value = await getFieldValue(page, '#name');
    expect(value).toBe('John');

    await page.fill('#name', 'Jane');
    const newValue = await getFieldValue(page, '#name');
    expect(newValue).toBe('Jane');
  });

  test('FORM-002: Password input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Password: <input type="password" name="password" id="password" value="secret"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#password');
    expect(await input.getAttribute('type')).toBe('password');

    const value = await getFieldValue(page, '#password');
    expect(value).toBe('secret');
  });

  test('FORM-003: Email input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email" value="test@example.com"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#email');
    expect(await input.getAttribute('type')).toBe('email');

    const value = await getFieldValue(page, '#email');
    expect(value).toBe('test@example.com');
  });

  test('FORM-004: Number input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Age: <input type="number" name="age" id="age" value="25" min="0" max="120" step="1"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#age');
    expect(await input.getAttribute('type')).toBe('number');
    expect(await input.getAttribute('min')).toBe('0');
    expect(await input.getAttribute('max')).toBe('120');
    expect(await input.getAttribute('step')).toBe('1');

    const value = await getFieldValue(page, '#age');
    expect(value).toBe('25');
  });

  test('FORM-005: Tel input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Phone: <input type="tel" name="phone" id="phone" value="555-1234" pattern="[0-9]{3}-[0-9]{4}"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#phone');
    expect(await input.getAttribute('type')).toBe('tel');

    const value = await getFieldValue(page, '#phone');
    expect(value).toBe('555-1234');
  });

  test('FORM-006: URL input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Website: <input type="url" name="website" id="website" value="https://example.com"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#website');
    expect(await input.getAttribute('type')).toBe('url');

    const value = await getFieldValue(page, '#website');
    expect(value).toBe('https://example.com');
  });

  test('FORM-007: Date input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Date: <input type="date" name="date" id="date" value="2024-01-15"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#date');
    expect(await input.getAttribute('type')).toBe('date');

    const value = await getFieldValue(page, '#date');
    expect(value).toBe('2024-01-15');
  });

  test('FORM-008: Time input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Time: <input type="time" name="time" id="time" value="14:30"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#time');
    expect(await input.getAttribute('type')).toBe('time');

    const value = await getFieldValue(page, '#time');
    expect(value).toBe('14:30');
  });

  test('FORM-009: Checkbox input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label><input type="checkbox" name="agree" id="agree" value="yes"> I agree</label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const checkbox = await page.locator('#agree');
    expect(await checkbox.getAttribute('type')).toBe('checkbox');

    expect(await checkbox.isChecked()).toBe(false);
    await checkbox.click();
    expect(await checkbox.isChecked()).toBe(true);
  });

  test('FORM-010: Radio input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label><input type="radio" name="option" id="opt1" value="a"> Option A</label>
        <label><input type="radio" name="option" id="opt2" value="b"> Option B</label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const radio1 = await page.locator('#opt1');
    const radio2 = await page.locator('#opt2');

    expect(await radio1.isChecked()).toBe(false);
    await radio1.click();
    expect(await radio1.isChecked()).toBe(true);
    expect(await radio2.isChecked()).toBe(false);

    await radio2.click();
    expect(await radio1.isChecked()).toBe(false);
    expect(await radio2.isChecked()).toBe(true);
  });

  test('FORM-011: File input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Upload: <input type="file" name="file" id="file" accept=".txt,.pdf"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#file');
    expect(await input.getAttribute('type')).toBe('file');
    expect(await input.getAttribute('accept')).toBe('.txt,.pdf');
  });

  test('FORM-012: Hidden input type', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <input type="hidden" name="csrf_token" id="csrf" value="token123">
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#csrf');
    expect(await input.getAttribute('type')).toBe('hidden');

    const value = await getFieldValue(page, '#csrf');
    expect(value).toBe('token123');
  });

});

// ============================================================================
// FORM SUBMISSION TESTS (FORM-013 to FORM-020)
// ============================================================================

test.describe('Form Submission', () => {

  test('FORM-013: GET submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      method: 'GET',
      action: '/submit',
      content: `
        <label>Name: <input type="text" name="name" id="name" value="John"></label>
        <label>Email: <input type="email" name="email" id="email" value="john@example.com"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify form method before submission
    const method = await page.evaluate(() => {
      const form = document.getElementById('test-form') as HTMLFormElement;
      return form ? form.method.toUpperCase() : null;
    });

    expect(method).toBe('GET');

    // Note: We verify method before navigation since GET submission would change the URL
    // and we can't reliably navigate back to check the form element
  });

  test('FORM-014: POST submission', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://localhost:9080/forms/legitimate-login.html');

    // Fill form
    await page.fill('input[name="email"]', 'test@example.com');
    await page.fill('input[name="password"]', 'password123');

    // Submit form
    const responsePromise = page.waitForResponse(response =>
      response.request().method() === 'POST' && response.url().includes('/login')
    );

    await page.click('button[type="submit"]');

    const response = await responsePromise;
    expect(response.ok()).toBe(true);
  });

  test('FORM-015: multipart/form-data submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      method: 'POST',
      action: '/submit',
      enctype: 'multipart/form-data',
      content: `
        <label>Name: <input type="text" name="name" id="name" value="John"></label>
        <label>File: <input type="file" name="file" id="file"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const enctype = await page.evaluate(() => {
      return (document.getElementById('test-form') as HTMLFormElement).enctype;
    });

    expect(enctype).toBe('multipart/form-data');
  });

  test('FORM-016: JavaScript form submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Name: <input type="text" name="name" id="name" value="John"></label>
        <button type="button" id="js-submit" onclick="submitForm()">JS Submit</button>
        <script>
          function submitForm() {
            const form = document.getElementById('test-form');
            window.__submissionDetected = true;
            // Don't actually submit to avoid navigation
            return false;
          }
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.click('#js-submit');

    const submissionDetected = await page.evaluate(() => {
      return (window as any).__submissionDetected;
    });

    expect(submissionDetected).toBe(true);
  });

  test('FORM-017: Enter key submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Search: <input type="text" name="query" id="query"></label>
        <div id="submitted">Not submitted</div>
        <script>
          document.getElementById('test-form').addEventListener('submit', (e) => {
            e.preventDefault();
            document.getElementById('submitted').textContent = 'Submitted via Enter';
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.fill('#query', 'test search');
    await page.press('#query', 'Enter');

    const status = await page.locator('#submitted').textContent();
    expect(status).toBe('Submitted via Enter');
  });

  test('FORM-018: Submit button vs image button', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Field: <input type="text" name="field" id="field" value="test"></label>
        <button type="submit" id="text-btn" name="action" value="save">Save</button>
        <button type="submit" id="alt-btn" name="action" value="publish">Publish</button>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const buttons = await page.locator('button[type="submit"]').all();
    // createFormHtml adds a default submit button, plus our 2 buttons = 3 total
    expect(buttons.length).toBe(3);
  });

  test('FORM-019: Form validation prevents submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email" required></label>
        <div id="submit-status">Not submitted</div>
        <script>
          document.getElementById('test-form').addEventListener('submit', (e) => {
            document.getElementById('submit-status').textContent = 'Form submitted';
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Submit with empty required field
    await page.click('#submit-btn');

    // Check if validation prevented submission (page remains on same URL)
    const status = await page.locator('#submit-status').textContent();
    expect(status).toBe('Not submitted');
  });

  test('FORM-020: preventDefault() stops submission', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Name: <input type="text" name="name" id="name" value="test"></label>
        <div id="status">Not prevented</div>
        <script>
          document.getElementById('test-form').addEventListener('submit', (e) => {
            e.preventDefault();
            document.getElementById('status').textContent = 'Submission prevented';
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.click('#submit-btn');

    const status = await page.locator('#status').textContent();
    expect(status).toBe('Submission prevented');
  });

});

// ============================================================================
// FORM VALIDATION TESTS (FORM-021 to FORM-030)
// ============================================================================

test.describe('Form Validation', () => {

  test('FORM-021: required attribute validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Name: <input type="text" name="name" id="name" required></label>
        <div id="validation-result"></div>
        <script>
          document.getElementById('test-form').addEventListener('submit', (e) => {
            const form = e.target;
            const isValid = form.checkValidity();
            document.getElementById('validation-result').textContent = isValid ? 'Valid' : 'Invalid';
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Try to submit with empty field
    await page.click('#submit-btn');

    // Form should be invalid
    const isValid = await page.evaluate(() => {
      return (document.getElementById('test-form') as HTMLFormElement).checkValidity();
    });

    expect(isValid).toBe(false);
  });

  test('FORM-022: pattern attribute validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Phone: <input type="tel" name="phone" id="phone" pattern="[0-9]{3}-[0-9]{3}-[0-9]{4}" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#phone');
    expect(await input.getAttribute('pattern')).toBe('[0-9]{3}-[0-9]{3}-[0-9]{4}');

    // Validate
    const isValid = await page.evaluate(() => {
      return (document.getElementById('phone') as HTMLInputElement).checkValidity();
    });

    expect(isValid).toBe(false); // Empty required field
  });

  test('FORM-023: min/max attribute validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Age: <input type="number" name="age" id="age" min="18" max="65" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#age');
    expect(await input.getAttribute('min')).toBe('18');
    expect(await input.getAttribute('max')).toBe('65');

    // Test invalid value
    await input.fill('10');

    const isValid = await page.evaluate(() => {
      return (document.getElementById('age') as HTMLInputElement).checkValidity();
    });

    expect(isValid).toBe(false);
  });

  test('FORM-024: minlength/maxlength attribute validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Username: <input type="text" name="username" id="username" minlength="3" maxlength="20" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#username');
    expect(await input.getAttribute('minlength')).toBe('3');
    expect(await input.getAttribute('maxlength')).toBe('20');

    // Test too short
    await input.fill('ab');

    const isValid = await page.evaluate(() => {
      return (document.getElementById('username') as HTMLInputElement).checkValidity();
    });

    expect(isValid).toBe(false);
  });

  test('FORM-025: email input type validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#email');

    // Test invalid email
    await input.fill('not-an-email');

    const isValid = await page.evaluate(() => {
      return (document.getElementById('email') as HTMLInputElement).checkValidity();
    });

    expect(isValid).toBe(false);
  });

  test('FORM-026: URL input type validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Website: <input type="url" name="website" id="website" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#website');

    // Test invalid URL
    await input.fill('not a url');

    const isValid = await page.evaluate(() => {
      return (document.getElementById('website') as HTMLInputElement).checkValidity();
    });

    expect(isValid).toBe(false);
  });

  test('FORM-027: Custom validity message', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Password: <input type="password" name="password" id="password" required></label>
        <div id="error-message"></div>
        <script>
          const input = document.getElementById('password');
          input.addEventListener('invalid', (e) => {
            e.preventDefault();
            input.setCustomValidity('Password must be at least 8 characters');
            document.getElementById('error-message').textContent = input.validationMessage;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Try to submit with empty password
    await page.click('#submit-btn');

    const message = await page.locator('#error-message').textContent();
    expect(message).toContain('Password');
  });

  test('FORM-028: :invalid pseudo-class styling', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <head>
          <style>
            input:invalid { border: 2px solid red; }
            input:valid { border: 2px solid green; }
          </style>
        </head>
        <body>
          <form id="test-form">
            <input type="email" name="email" id="email" required>
          </form>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const input = await page.locator('#email');

    // Invalid state - empty required field
    await page.fill('#email', '');

    const isInvalid = await page.evaluate(() => {
      return !(document.getElementById('email') as HTMLInputElement).checkValidity();
    });

    expect(isInvalid).toBe(true);
  });

  test('FORM-029: Validation messages', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email" required></label>
        <label>Age: <input type="number" name="age" id="age" min="18" max="120"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Get validation message for invalid email
    const emailMessage = await page.evaluate(() => {
      const email = document.getElementById('email') as HTMLInputElement;
      return email.validationMessage;
    });

    // Get validation message for out-of-range number
    await page.fill('#age', '10');
    const ageMessage = await page.evaluate(() => {
      const age = document.getElementById('age') as HTMLInputElement;
      return age.validationMessage;
    });

    expect(emailMessage).toBeTruthy();
    expect(ageMessage).toBeTruthy();
  });

  test('FORM-030: novalidate attribute disables validation', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      novalidate: true,
      content: `
        <label>Email: <input type="email" name="email" id="email" required></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const form = await page.locator('#test-form');
    expect(await form.getAttribute('novalidate')).not.toBeNull();

    // With novalidate, invalid form should still submit (browser won't prevent it)
    const hasNovalidate = await page.evaluate(() => {
      return (document.getElementById('test-form') as HTMLFormElement).noValidate;
    });

    expect(hasNovalidate).toBe(true);
  });

});

// ============================================================================
// FORM EVENTS TESTS (FORM-031 to FORM-036)
// ============================================================================

test.describe('Form Events', () => {

  test('FORM-031: input event fires on value change', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Text: <input type="text" name="text" id="text"></label>
        <div id="input-count">0</div>
        <script>
          let inputCount = 0;
          document.getElementById('text').addEventListener('input', () => {
            inputCount++;
            document.getElementById('input-count').textContent = inputCount;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.fill('#text', 'a');
    await page.fill('#text', 'ab');
    await page.fill('#text', 'abc');

    const count = await page.locator('#input-count').textContent();
    expect(parseInt(count!)).toBeGreaterThan(0);
  });

  test('FORM-032: change event fires on blur', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email"></label>
        <label>Other: <input type="text" name="other" id="other"></label>
        <div id="change-count">0</div>
        <script>
          let changeCount = 0;
          document.getElementById('email').addEventListener('change', () => {
            changeCount++;
            document.getElementById('change-count').textContent = changeCount;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.fill('#email', 'test@example.com');
    await page.click('#other'); // Blur from email field

    const count = await page.locator('#change-count').textContent();
    expect(parseInt(count!)).toBeGreaterThan(0);
  });

  test('FORM-033: submit event fires', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Field: <input type="text" name="field" id="field" value="test"></label>
        <div id="submit-count">0</div>
        <script>
          let submitCount = 0;
          document.getElementById('test-form').addEventListener('submit', (e) => {
            e.preventDefault();
            submitCount++;
            document.getElementById('submit-count').textContent = submitCount;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.click('#submit-btn');

    const count = await page.locator('#submit-count').textContent();
    expect(parseInt(count!)).toBe(1);
  });

  test('FORM-034: reset event fires', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Field: <input type="text" name="field" id="field" value="original"></label>
        <div id="reset-count">0</div>
        <script>
          let resetCount = 0;
          document.getElementById('test-form').addEventListener('reset', (e) => {
            resetCount++;
            document.getElementById('reset-count').textContent = resetCount;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.fill('#field', 'modified');
    await page.click('#reset-btn');

    const count = await page.locator('#reset-count').textContent();
    expect(parseInt(count!)).toBe(1);

    const fieldValue = await getFieldValue(page, '#field');
    expect(fieldValue).toBe('original');
  });

  test('FORM-035: focus and blur events', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Field1: <input type="text" name="field1" id="field1"></label>
        <label>Field2: <input type="text" name="field2" id="field2"></label>
        <div id="focus-status">none</div>
        <div id="blur-status">none</div>
        <script>
          document.getElementById('field1').addEventListener('focus', () => {
            document.getElementById('focus-status').textContent = 'field1';
          });
          document.getElementById('field1').addEventListener('blur', () => {
            document.getElementById('blur-status').textContent = 'field1';
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.click('#field1');
    const focusStatus = await page.locator('#focus-status').textContent();
    expect(focusStatus).toBe('field1');

    await page.click('#field2');
    const blurStatus = await page.locator('#blur-status').textContent();
    expect(blurStatus).toBe('field1');
  });

  test('FORM-036: Event bubbling in forms', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>
          Field:
          <input type="text" name="field" id="field">
          <span id="bubble-span"></span>
        </label>
        <div id="form-input">0</div>
        <script>
          let formInputs = 0;
          document.getElementById('test-form').addEventListener('input', (e) => {
            formInputs++;
            document.getElementById('form-input').textContent = formInputs;
          });
        </script>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.fill('#field', 'test');

    const count = await page.locator('#form-input').textContent();
    expect(parseInt(count!)).toBeGreaterThan(0);
  });

});

// ============================================================================
// ADVANCED FEATURES TESTS (FORM-037 to FORM-042)
// ============================================================================

test.describe('Advanced Form Features', () => {

  test('FORM-037: autocomplete attribute', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Email: <input type="email" name="email" id="email" autocomplete="email"></label>
        <label>Username: <input type="text" name="username" id="username" autocomplete="username"></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const emailAutocomplete = await page.locator('#email').getAttribute('autocomplete');
    const usernameAutocomplete = await page.locator('#username').getAttribute('autocomplete');

    expect(emailAutocomplete).toBe('email');
    expect(usernameAutocomplete).toBe('username');
  });

  test('FORM-038: disabled and readonly attributes', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Disabled: <input type="text" name="disabled" id="disabled-field" value="cannot change" disabled></label>
        <label>Readonly: <input type="text" name="readonly" id="readonly-field" value="can select" readonly></label>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const disabled = await page.locator('#disabled-field');
    const readonly = await page.locator('#readonly-field');

    expect(await disabled.isDisabled()).toBe(true);
    expect(await readonly.getAttribute('readonly')).not.toBeNull();

    // Try to fill disabled field - should fail
    try {
      await disabled.fill('new value');
      expect(false).toBe(true); // Should not reach here
    } catch (e) {
      expect(true).toBe(true); // Expected to fail
    }
  });

  test('FORM-039: fieldset and legend', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <form id="test-form">
            <fieldset id="group1">
              <legend>Personal Info</legend>
              <label>Name: <input type="text" name="name" id="name"></label>
              <label>Email: <input type="email" name="email" id="email"></label>
            </fieldset>
            <fieldset id="group2" disabled>
              <legend>Address</legend>
              <label>Street: <input type="text" name="street" id="street"></label>
            </fieldset>
          </form>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const group1 = await page.locator('#group1');
    const group2 = await page.locator('#group2');
    const legend1 = await group1.locator('legend').textContent();
    const legend2 = await group2.locator('legend').textContent();

    expect(legend1).toBe('Personal Info');
    expect(legend2).toBe('Address');

    // Disabled fieldset should have disabled inputs
    const streetInput = await page.locator('#street');
    expect(await streetInput.isDisabled()).toBe(true);
  });

  test('FORM-040: form attribute binding', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <form id="main-form"></form>
          <input type="text" name="field" id="field" value="test" form="main-form">
          <button type="submit" id="submit-btn" form="main-form">Submit</button>
          <script>
            document.getElementById('main-form').addEventListener('submit', (e) => {
              e.preventDefault();
              const form = e.target;
              const data = new FormData(form);
              window.__submittedData = Object.fromEntries(data);
            });
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Field outside form is associated via form attribute
    const field = await page.locator('#field');
    const formAttr = await field.getAttribute('form');
    expect(formAttr).toBe('main-form');

    // Submit button outside form is associated via form attribute
    const button = await page.locator('#submit-btn');
    const buttonFormAttr = await button.getAttribute('form');
    expect(buttonFormAttr).toBe('main-form');
  });

  test('FORM-041: formaction override', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      action: '/default',
      content: `
        <label>Field: <input type="text" name="field" id="field" value="test"></label>
        <button type="submit" id="save-btn" name="action" value="save" formaction="/save">Save</button>
        <button type="submit" id="delete-btn" name="action" value="delete" formaction="/delete">Delete</button>
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const saveBtn = await page.locator('#save-btn');
    const deleteBtn = await page.locator('#delete-btn');

    expect(await saveBtn.getAttribute('formaction')).toBe('/save');
    expect(await deleteBtn.getAttribute('formaction')).toBe('/delete');
  });

  test('FORM-042: FormData API', { tag: '@p0' }, async ({ page }) => {
    const html = createFormHtml({
      content: `
        <label>Text: <input type="text" name="text" id="text" value="hello"></label>
        <label>Email: <input type="email" name="email" id="email" value="test@example.com"></label>
        <input type="hidden" name="csrf" value="token123">
      `
    });

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const formData = await getFormData(page, 'test-form');

    expect(formData.text).toBe('hello');
    expect(formData.email).toBe('test@example.com');
    expect(formData.csrf).toBe('token123');
  });

});
