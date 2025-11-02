/**
 * Form Testing Utilities
 *
 * Helper functions and utilities for comprehensive form testing in Playwright.
 * Provides common functionality for form manipulation, validation, and event tracking.
 */

import { Page, Locator } from '@playwright/test';

/**
 * Form field value representation
 */
export interface FormFieldValue {
  name: string;
  type: string;
  value: string | string[] | boolean;
  disabled?: boolean;
  readonly?: boolean;
}

/**
 * Form submission result
 */
export interface FormSubmissionResult {
  success: boolean;
  method: string;
  action: string;
  data: Record<string, any>;
  timestamp: number;
  responseStatus?: number;
  responseData?: any;
}

/**
 * Form validation state
 */
export interface FormValidationState {
  isValid: boolean;
  invalidFields: string[];
  validationMessages: Record<string, string>;
  timestamp: number;
}

/**
 * Form field configuration
 */
export interface FormFieldConfig {
  name: string;
  type: string;
  value?: string;
  id?: string;
  attributes?: Record<string, string>;
  options?: string[]; // For select and checkbox/radio groups
}

/**
 * Get all form field values as an object
 */
export async function getFormValues(page: Page, formId: string = 'test-form'): Promise<Record<string, any>> {
  return await page.evaluate((id) => {
    const form = document.getElementById(id) as HTMLFormElement;
    if (!form) throw new Error(`Form with id "${id}" not found`);

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

/**
 * Set form field values
 */
export async function setFormValues(
  page: Page,
  values: Record<string, string | boolean | string[]>,
  formId: string = 'test-form'
): Promise<void> {
  for (const [fieldName, fieldValue] of Object.entries(values)) {
    const selector = `#${formId} [name="${fieldName}"]`;
    const elements = await page.locator(selector).all();

    if (elements.length === 0) {
      throw new Error(`Field "${fieldName}" not found in form "${formId}"`);
    }

    const firstElement = elements[0];
    const type = await firstElement.getAttribute('type');

    if (type === 'checkbox' || type === 'radio') {
      // For checkboxes/radios, handle multiple values
      const valuesToSet = Array.isArray(fieldValue) ? fieldValue : [fieldValue as string];

      for (const element of elements) {
        const elemValue = await element.getAttribute('value');
        const shouldCheck = valuesToSet.includes(elemValue!);
        const isChecked = await element.isChecked();

        if (shouldCheck && !isChecked) {
          await element.click();
        } else if (!shouldCheck && isChecked) {
          await element.click();
        }
      }
    } else {
      // For text inputs, select, textarea, etc.
      const value = Array.isArray(fieldValue) ? fieldValue[0] : fieldValue;
      await firstElement.fill(String(value));
    }
  }
}

/**
 * Submit a form
 */
export async function submitForm(
  page: Page,
  formId: string = 'test-form',
  submitButtonSelector?: string
): Promise<FormSubmissionResult> {
  const form = await page.locator(`#${formId}`);

  if (!await form.isVisible()) {
    throw new Error(`Form with id "${formId}" is not visible`);
  }

  // Get form properties before submission
  const formAction = await form.getAttribute('action');
  const formMethod = await form.getAttribute('method') || 'GET';

  // Get form data
  const data = await getFormValues(page, formId);

  // Click submit button or trigger submit event
  if (submitButtonSelector) {
    await page.click(submitButtonSelector);
  } else {
    const submitButton = await form.locator('button[type="submit"]');
    if (await submitButton.isVisible()) {
      await submitButton.click();
    } else {
      // Trigger submit event
      await form.evaluate((el: HTMLFormElement) => {
        el.dispatchEvent(new Event('submit', { cancelable: true }));
      });
    }
  }

  return {
    success: true,
    method: formMethod,
    action: formAction || '',
    data,
    timestamp: Date.now()
  };
}

/**
 * Reset a form
 */
export async function resetForm(page: Page, formId: string = 'test-form'): Promise<void> {
  const form = await page.locator(`#${formId}`);
  const resetButton = form.locator('button[type="reset"]');

  if (await resetButton.isVisible()) {
    await resetButton.click();
  } else {
    await form.evaluate((el: HTMLFormElement) => {
      el.reset();
    });
  }
}

/**
 * Check form validity
 */
export async function checkFormValidity(
  page: Page,
  formId: string = 'test-form'
): Promise<FormValidationState> {
  return await page.evaluate((id) => {
    const form = document.getElementById(id) as HTMLFormElement;
    if (!form) throw new Error(`Form with id "${id}" not found`);

    const isValid = form.checkValidity();
    const invalidFields: string[] = [];
    const validationMessages: Record<string, string> = {};

    const elements = form.querySelectorAll('input, select, textarea');
    elements.forEach((el) => {
      const input = el as any;
      if (!input.checkValidity?.()) {
        invalidFields.push(input.name);
        validationMessages[input.name] = input.validationMessage || 'Invalid field';
      }
    });

    return {
      isValid,
      invalidFields,
      validationMessages,
      timestamp: Date.now()
    };
  }, formId);
}

/**
 * Get form field by name or id
 */
export async function getFormField(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<Locator | null> {
  const selector = formId
    ? `#${formId} [name="${fieldName}"], #${formId} #${fieldName}`
    : `[name="${fieldName}"], #${fieldName}`;

  const elements = await page.locator(selector).all();
  return elements.length > 0 ? elements[0] : null;
}

/**
 * Fill form field by name
 */
export async function fillFormField(
  page: Page,
  fieldName: string,
  value: string,
  formId?: string
): Promise<void> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  const type = await field.getAttribute('type');

  if (type === 'checkbox' || type === 'radio') {
    await field.click();
  } else {
    await field.fill(value);
  }
}

/**
 * Get form field value
 */
export async function getFormFieldValue(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<string | boolean | string[] | null> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  const type = await field.getAttribute('type');

  if (type === 'checkbox' || type === 'radio') {
    const isChecked = await field.isChecked();
    // If it's a multi-value field (multiple checkboxes with same name), get all checked values
    const elements = await page.locator(`[name="${fieldName}"]`).all();
    if (elements.length > 1 && type === 'checkbox') {
      const checkedValues: string[] = [];
      for (const element of elements) {
        if (await element.isChecked()) {
          const val = await element.getAttribute('value');
          if (val) checkedValues.push(val);
        }
      }
      return checkedValues.length > 0 ? checkedValues : null;
    }
    return isChecked;
  } else {
    return await field.inputValue();
  }
}

/**
 * Wait for form to be valid (check validation periodically)
 */
export async function waitForFormValid(
  page: Page,
  formId: string = 'test-form',
  timeout: number = 5000
): Promise<boolean> {
  const startTime = Date.now();

  while (Date.now() - startTime < timeout) {
    const validation = await checkFormValidity(page, formId);
    if (validation.isValid) {
      return true;
    }
    await page.waitForTimeout(100);
  }

  throw new Error(`Form did not become valid within ${timeout}ms`);
}

/**
 * Count form fields of a specific type
 */
export async function countFormFieldsByType(
  page: Page,
  fieldType: string,
  formId?: string
): Promise<number> {
  const selector = formId
    ? `#${formId} input[type="${fieldType}"]`
    : `input[type="${fieldType}"]`;

  return await page.locator(selector).count();
}

/**
 * Get all form fields info
 */
export async function getFormFieldsInfo(
  page: Page,
  formId: string = 'test-form'
): Promise<FormFieldValue[]> {
  return await page.evaluate((id) => {
    const form = document.getElementById(id) as HTMLFormElement;
    if (!form) throw new Error(`Form with id "${id}" not found`);

    const fields: FormFieldValue[] = [];
    const elements = form.querySelectorAll('input, select, textarea');

    elements.forEach((el) => {
      const input = el as any;
      const type = input.type || input.tagName.toLowerCase();

      let value: string | string[] | boolean;

      if (input.type === 'checkbox' || input.type === 'radio') {
        value = input.checked;
      } else if (input.tagName === 'SELECT') {
        value = input.value;
      } else {
        value = input.value;
      }

      fields.push({
        name: input.name,
        type: type,
        value: value,
        disabled: input.disabled,
        readonly: input.readOnly
      });
    });

    return fields;
  }, formId);
}

/**
 * Set field validation message
 */
export async function setCustomValidityMessage(
  page: Page,
  fieldName: string,
  message: string,
  formId?: string
): Promise<void> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  await field.evaluate((el: any, msg: string) => {
    el.setCustomValidity(msg);
  }, message);
}

/**
 * Get field validation message
 */
export async function getFieldValidationMessage(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<string> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  return await field.evaluate((el: any) => {
    return el.validationMessage || '';
  });
}

/**
 * Focus on a field
 */
export async function focusField(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<void> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  await field.focus();
}

/**
 * Blur a field (remove focus)
 */
export async function blurField(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<void> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  await field.blur();
}

/**
 * Check if field is disabled
 */
export async function isFieldDisabled(
  page: Page,
  fieldName: string,
  formId?: string
): Promise<boolean> {
  const field = await getFormField(page, fieldName, formId);

  if (!field) {
    throw new Error(`Field "${fieldName}" not found`);
  }

  return await field.isDisabled();
}

/**
 * Get all form event listeners (from window.__formEvents if available)
 */
export async function getFormEvents(page: Page): Promise<Record<string, number>> {
  return await page.evaluate(() => {
    return (window as any).__formEvents || {};
  });
}

/**
 * Clear all tracked form events
 */
export async function clearFormEvents(page: Page): Promise<void> {
  await page.evaluate(() => {
    (window as any).__formEvents = {};
  });
}

/**
 * Get form state (custom state set by test page)
 */
export async function getFormState(page: Page): Promise<Record<string, any>> {
  return await page.evaluate(() => {
    return (window as any).__formState || {};
  });
}

/**
 * Create FormData and get as JSON (FormData API is not directly JSON-serializable)
 */
export async function getFormDataAsJSON(page: Page, formId: string = 'test-form'): Promise<Record<string, any>> {
  return await page.evaluate((id) => {
    const form = document.getElementById(id) as HTMLFormElement;
    if (!form) throw new Error(`Form with id "${id}" not found`);

    const formData = new FormData(form);
    const result: Record<string, any> = {};

    formData.forEach((value, key) => {
      if (result[key]) {
        if (Array.isArray(result[key])) {
          result[key].push(value);
        } else {
          result[key] = [result[key], value];
        }
      } else {
        result[key] = value;
      }
    });

    return result;
  }, formId);
}

/**
 * Simulate form submission with FormData (via fetch)
 */
export async function submitFormWithFetch(
  page: Page,
  formId: string = 'test-form',
  url?: string
): Promise<FormSubmissionResult> {
  const form = await page.locator(`#${formId}`);
  const formAction = url || (await form.getAttribute('action')) || '/submit';
  const formMethod = await form.getAttribute('method') || 'POST';

  const result = await page.evaluate(
    async (id: string, action: string, method: string) => {
      const form = document.getElementById(id) as HTMLFormElement;
      const formData = new FormData(form);

      try {
        const response = await fetch(action, {
          method: method,
          body: method === 'GET' ? undefined : formData
        });

        const data = await getFormDataAsJSON(id);

        return {
          success: response.ok,
          method: method,
          action: action,
          data: data,
          timestamp: Date.now(),
          responseStatus: response.status
        };
      } catch (error) {
        return {
          success: false,
          method: method,
          action: action,
          data: {},
          timestamp: Date.now(),
          error: (error as Error).message
        };
      }
    },
    formId,
    formAction,
    formMethod
  );

  function getFormDataAsJSON(id: string) {
    const form = document.getElementById(id) as HTMLFormElement;
    const formData = new FormData(form);
    const result: Record<string, any> = {};

    formData.forEach((value, key) => {
      if (result[key]) {
        if (Array.isArray(result[key])) {
          result[key].push(value);
        } else {
          result[key] = [result[key], value];
        }
      } else {
        result[key] = value;
      }
    });

    return result;
  }

  return result as FormSubmissionResult;
}

/**
 * Wait for form field to have a specific value
 */
export async function waitForFieldValue(
  page: Page,
  fieldName: string,
  expectedValue: string,
  timeout: number = 5000
): Promise<boolean> {
  const startTime = Date.now();

  while (Date.now() - startTime < timeout) {
    const value = await getFormFieldValue(page, fieldName);
    if (value === expectedValue) {
      return true;
    }
    await page.waitForTimeout(100);
  }

  throw new Error(`Field "${fieldName}" did not reach value "${expectedValue}" within ${timeout}ms`);
}

/**
 * Assert form is in expected state
 */
export async function assertFormState(
  page: Page,
  formId: string,
  expectedState: {
    isValid?: boolean;
    fieldCount?: number;
    submissionCount?: number;
    resetCount?: number;
  }
): Promise<void> {
  const state = await getFormState(page);

  if (expectedState.isValid !== undefined) {
    const actual = state.isValid;
    if (actual !== expectedState.isValid) {
      throw new Error(`Expected form isValid=${expectedState.isValid}, got ${actual}`);
    }
  }

  if (expectedState.fieldCount !== undefined) {
    const actual = state.fieldCount;
    if (actual !== expectedState.fieldCount) {
      throw new Error(`Expected fieldCount=${expectedState.fieldCount}, got ${actual}`);
    }
  }

  if (expectedState.submissionCount !== undefined) {
    const actual = state.submissionCount;
    if (actual !== expectedState.submissionCount) {
      throw new Error(`Expected submissionCount=${expectedState.submissionCount}, got ${actual}`);
    }
  }

  if (expectedState.resetCount !== undefined) {
    const actual = state.resetCount;
    if (actual !== expectedState.resetCount) {
      throw new Error(`Expected resetCount=${expectedState.resetCount}, got ${actual}`);
    }
  }
}
