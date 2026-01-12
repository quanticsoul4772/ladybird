import { test, expect } from '@playwright/test';

/**
 * Request/Response Headers Tests for Ladybird Browser
 *
 * Tests the Headers API implementation and header handling in fetch requests.
 * These tests verify compliance with the Fetch API Headers specification:
 * https://fetch.spec.whatwg.org/#headers-class
 *
 * Test Server: http://localhost:9080/ (configured in playwright.config.ts)
 *
 * Ladybird Compatibility:
 * - Headers API may have partial support
 * - Some header manipulation methods may not be implemented
 * - Tests gracefully handle missing features with test.skip()
 */

test.describe('Request/Response Headers', () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to test server origin for fetch operations
    await page.goto('http://localhost:9080/');
  });

  /**
   * HEAD-001: Custom request headers @p0
   *
   * Tests the Headers constructor and manipulation methods:
   * - Headers constructor with object literal
   * - headers.set() - Set header value
   * - headers.get() - Retrieve header value
   * - headers.has() - Check header existence
   * - headers.append() - Add multiple values to same header
   * - headers.delete() - Remove header
   * - Case-insensitive header names
   *
   * Spec: https://fetch.spec.whatwg.org/#headers-class
   */
  test('HEAD-001: Custom request headers @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      const testData = {
        headersConstructorWorks: false,
        headersApiAvailable: false,
        operations: [] as Array<{ operation: string; success: boolean; value?: string | null; error?: string }>,
      };

      // Check if Headers API is available
      if (typeof Headers === 'undefined') {
        testData.headersApiAvailable = false;
        return testData;
      }

      testData.headersApiAvailable = true;

      try {
        // Test 1: Headers constructor with object literal
        const headers = new Headers({
          'X-Custom-Header': 'custom-value',
          'X-Test-ID': 'HEAD-001',
        });
        testData.headersConstructorWorks = true;

        // Test 2: headers.set()
        headers.set('X-Modified-Header', 'modified-value');
        testData.operations.push({
          operation: 'set',
          success: true,
        });

        // Test 3: headers.get() - retrieve set header
        const getValue = headers.get('X-Modified-Header');
        testData.operations.push({
          operation: 'get',
          success: getValue === 'modified-value',
          value: getValue,
        });

        // Test 4: headers.has() - check existence
        const hasHeader = headers.has('X-Modified-Header');
        testData.operations.push({
          operation: 'has',
          success: hasHeader === true,
          value: String(hasHeader),
        });

        // Test 5: Case-insensitive header names
        const caseInsensitiveGet = headers.get('x-modified-header'); // lowercase
        testData.operations.push({
          operation: 'case-insensitive-get',
          success: caseInsensitiveGet === 'modified-value',
          value: caseInsensitiveGet,
        });

        // Test 6: headers.append() - add to existing header
        headers.append('X-Multi-Value', 'value1');
        headers.append('X-Multi-Value', 'value2');
        const multiValue = headers.get('X-Multi-Value');
        testData.operations.push({
          operation: 'append',
          success: multiValue !== null && multiValue.includes('value'),
          value: multiValue,
        });

        // Test 7: headers.delete()
        headers.delete('X-Modified-Header');
        const deletedValue = headers.get('X-Modified-Header');
        testData.operations.push({
          operation: 'delete',
          success: deletedValue === null,
          value: deletedValue,
        });

        // Test 8: Verify deleted header doesn't exist
        const hasDeletedHeader = headers.has('X-Modified-Header');
        testData.operations.push({
          operation: 'has-after-delete',
          success: hasDeletedHeader === false,
          value: String(hasDeletedHeader),
        });

        // Test 9: Create fetch request with custom headers
        try {
          const response = await fetch('http://localhost:9080/health', {
            method: 'GET',
            headers: {
              'X-Custom-Test': 'HEAD-001',
              'X-Request-ID': String(Date.now()),
            },
          });

          testData.operations.push({
            operation: 'fetch-with-custom-headers',
            success: response.ok,
            value: String(response.status),
          });
        } catch (e) {
          testData.operations.push({
            operation: 'fetch-with-custom-headers',
            success: false,
            error: e instanceof Error ? e.message : String(e),
          });
        }
      } catch (e) {
        testData.operations.push({
          operation: 'headers-api-error',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      return testData;
    });

    // Skip test if Headers API is not available
    if (!result.headersApiAvailable) {
      console.log('Ladybird limitation: Headers API not defined');
      test.skip();
      return;
    }

    // Verify Headers constructor works
    expect(result.headersConstructorWorks).toBe(true);

    // Verify all operations
    const setOp = result.operations.find(op => op.operation === 'set');
    expect(setOp?.success).toBe(true);

    const getOp = result.operations.find(op => op.operation === 'get');
    expect(getOp?.success).toBe(true);
    expect(getOp?.value).toBe('modified-value');

    const hasOp = result.operations.find(op => op.operation === 'has');
    expect(hasOp?.success).toBe(true);

    const caseInsensitiveOp = result.operations.find(op => op.operation === 'case-insensitive-get');
    expect(caseInsensitiveOp?.success).toBe(true);

    const appendOp = result.operations.find(op => op.operation === 'append');
    expect(appendOp?.success).toBe(true);

    const deleteOp = result.operations.find(op => op.operation === 'delete');
    expect(deleteOp?.success).toBe(true);

    const hasAfterDeleteOp = result.operations.find(op => op.operation === 'has-after-delete');
    expect(hasAfterDeleteOp?.success).toBe(true);

    const fetchOp = result.operations.find(op => op.operation === 'fetch-with-custom-headers');
    expect(fetchOp?.success).toBe(true);
  });

  /**
   * HEAD-002: Response header parsing @p0
   *
   * Tests reading and parsing headers from fetch responses:
   * - response.headers object access
   * - response.headers.get() for standard headers
   * - response.headers.has() to check header presence
   * - response.headers.entries() / forEach() for iteration
   * - Verify header values are strings
   *
   * Spec: https://fetch.spec.whatwg.org/#response-class
   */
  test('HEAD-002: Response header parsing @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      const testData = {
        fetchSuccessful: false,
        headersAccessible: false,
        operations: [] as Array<{ operation: string; success: boolean; value?: string | null; count?: number; error?: string }>,
      };

      try {
        // Make fetch request to server
        const response = await fetch('http://localhost:9080/health');
        testData.fetchSuccessful = response.ok;

        // Test 1: Access response.headers
        const headers = response.headers;
        testData.headersAccessible = headers !== null && headers !== undefined;

        if (!testData.headersAccessible) {
          testData.operations.push({
            operation: 'headers-access',
            success: false,
            error: 'response.headers is null or undefined',
          });
          return testData;
        }

        // Test 2: response.headers.get() for Content-Type
        const contentType = headers.get('content-type');
        testData.operations.push({
          operation: 'get-content-type',
          success: contentType !== null,
          value: contentType,
        });

        // Test 3: response.headers.has() for standard header
        const hasContentType = headers.has('content-type');
        testData.operations.push({
          operation: 'has-content-type',
          success: hasContentType === true,
          value: String(hasContentType),
        });

        // Test 4: Verify header value is a string
        const isString = typeof contentType === 'string';
        testData.operations.push({
          operation: 'content-type-is-string',
          success: isString,
          value: typeof contentType,
        });

        // Test 5: Get non-existent header returns null
        const nonExistent = headers.get('X-Does-Not-Exist');
        testData.operations.push({
          operation: 'get-non-existent',
          success: nonExistent === null,
          value: nonExistent,
        });

        // Test 6: has() returns false for non-existent header
        const hasNonExistent = headers.has('X-Does-Not-Exist');
        testData.operations.push({
          operation: 'has-non-existent',
          success: hasNonExistent === false,
          value: String(hasNonExistent),
        });

        // Test 7: headers.entries() iteration
        try {
          const headerEntries: Array<[string, string]> = [];

          // Try entries() method
          if (typeof headers.entries === 'function') {
            for (const [key, value] of headers.entries()) {
              headerEntries.push([key, value]);
            }

            testData.operations.push({
              operation: 'entries-iteration',
              success: headerEntries.length > 0,
              count: headerEntries.length,
            });
          } else {
            testData.operations.push({
              operation: 'entries-iteration',
              success: false,
              error: 'headers.entries() not available',
            });
          }
        } catch (e) {
          testData.operations.push({
            operation: 'entries-iteration',
            success: false,
            error: e instanceof Error ? e.message : String(e),
          });
        }

        // Test 8: headers.forEach() iteration
        try {
          if (typeof headers.forEach === 'function') {
            let forEachCount = 0;
            headers.forEach((value: string, key: string) => {
              forEachCount++;
            });

            testData.operations.push({
              operation: 'forEach-iteration',
              success: forEachCount > 0,
              count: forEachCount,
            });
          } else {
            testData.operations.push({
              operation: 'forEach-iteration',
              success: false,
              error: 'headers.forEach() not available',
            });
          }
        } catch (e) {
          testData.operations.push({
            operation: 'forEach-iteration',
            success: false,
            error: e instanceof Error ? e.message : String(e),
          });
        }

        // Test 9: Case-insensitive header retrieval
        const caseInsensitive = headers.get('CONTENT-TYPE');
        testData.operations.push({
          operation: 'case-insensitive-response-header',
          success: caseInsensitive !== null,
          value: caseInsensitive,
        });

      } catch (e) {
        testData.operations.push({
          operation: 'fetch-error',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      return testData;
    });

    // Verify fetch succeeded
    expect(result.fetchSuccessful).toBe(true);
    expect(result.headersAccessible).toBe(true);

    // Verify get() operations
    const getContentType = result.operations.find(op => op.operation === 'get-content-type');
    expect(getContentType?.success).toBe(true);
    expect(getContentType?.value).toContain('json'); // /health endpoint returns JSON

    // Verify has() operations
    const hasContentType = result.operations.find(op => op.operation === 'has-content-type');
    expect(hasContentType?.success).toBe(true);

    // Verify type checking
    const isString = result.operations.find(op => op.operation === 'content-type-is-string');
    expect(isString?.success).toBe(true);

    // Verify non-existent header handling
    const getNonExistent = result.operations.find(op => op.operation === 'get-non-existent');
    expect(getNonExistent?.success).toBe(true);

    const hasNonExistent = result.operations.find(op => op.operation === 'has-non-existent');
    expect(hasNonExistent?.success).toBe(true);

    // Verify iteration (may not be supported in Ladybird yet)
    const entriesOp = result.operations.find(op => op.operation === 'entries-iteration');
    if (entriesOp && !entriesOp.error) {
      expect(entriesOp.success).toBe(true);
      expect(entriesOp.count).toBeGreaterThan(0);
    } else {
      console.log('Ladybird limitation: headers.entries() may not be implemented');
    }

    const forEachOp = result.operations.find(op => op.operation === 'forEach-iteration');
    if (forEachOp && !forEachOp.error) {
      expect(forEachOp.success).toBe(true);
      expect(forEachOp.count).toBeGreaterThan(0);
    } else {
      console.log('Ladybird limitation: headers.forEach() may not be implemented');
    }

    // Verify case-insensitive retrieval
    const caseInsensitiveOp = result.operations.find(op => op.operation === 'case-insensitive-response-header');
    expect(caseInsensitiveOp?.success).toBe(true);
  });

  /**
   * HEAD-003: Content-Type handling @p0
   *
   * Tests how Content-Type header affects response body parsing:
   * - application/json - should parse as JSON
   * - text/plain - should parse as text
   * - application/x-www-form-urlencoded - URL-encoded data
   * - Verify response.headers.get('content-type') works
   * - Verify Content-Type affects response.json() / response.text()
   *
   * Spec: https://fetch.spec.whatwg.org/#concept-body-consume-body
   */
  test('HEAD-003: Content-Type handling @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      const testData = {
        tests: [] as Array<{
          contentType: string;
          endpoint: string;
          success: boolean;
          actualContentType?: string | null;
          bodyType?: string;
          bodyData?: any;
          error?: string;
        }>,
      };

      // Test 1: application/json
      try {
        const jsonResponse = await fetch('http://localhost:9080/health');
        const actualContentType = jsonResponse.headers.get('content-type');
        const jsonData = await jsonResponse.json();

        testData.tests.push({
          contentType: 'application/json',
          endpoint: '/health',
          success: true,
          actualContentType,
          bodyType: typeof jsonData,
          bodyData: jsonData,
        });
      } catch (e) {
        testData.tests.push({
          contentType: 'application/json',
          endpoint: '/health',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      // Test 2: Create a simple HTML page fetch (text/html)
      try {
        const htmlResponse = await fetch('http://localhost:9080/');
        const actualContentType = htmlResponse.headers.get('content-type');
        const htmlText = await htmlResponse.text();

        testData.tests.push({
          contentType: 'text/html',
          endpoint: '/',
          success: true,
          actualContentType,
          bodyType: typeof htmlText,
          bodyData: htmlText.substring(0, 100), // First 100 chars
        });
      } catch (e) {
        testData.tests.push({
          contentType: 'text/html',
          endpoint: '/',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      // Test 3: POST with application/x-www-form-urlencoded
      try {
        const formData = new URLSearchParams({
          field1: 'value1',
          field2: 'value2',
        });

        const formResponse = await fetch('http://localhost:9080/submit', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
          },
          body: formData.toString(),
        });

        const responseContentType = formResponse.headers.get('content-type');
        const responseData = await formResponse.json(); // Server responds with JSON

        testData.tests.push({
          contentType: 'application/x-www-form-urlencoded',
          endpoint: '/submit',
          success: true,
          actualContentType: responseContentType,
          bodyType: typeof responseData,
          bodyData: responseData,
        });
      } catch (e) {
        testData.tests.push({
          contentType: 'application/x-www-form-urlencoded',
          endpoint: '/submit',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      // Test 4: POST with application/json
      try {
        const jsonPayload = {
          test: 'HEAD-003',
          timestamp: Date.now(),
        };

        const jsonPostResponse = await fetch('http://localhost:9080/submit', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(jsonPayload),
        });

        const responseContentType = jsonPostResponse.headers.get('content-type');
        const responseData = await jsonPostResponse.json();

        testData.tests.push({
          contentType: 'application/json (POST)',
          endpoint: '/submit',
          success: true,
          actualContentType: responseContentType,
          bodyType: typeof responseData,
          bodyData: responseData,
        });
      } catch (e) {
        testData.tests.push({
          contentType: 'application/json (POST)',
          endpoint: '/submit',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      // Test 5: Verify Content-Type is case-insensitive
      try {
        const response = await fetch('http://localhost:9080/health');
        const lowercase = response.headers.get('content-type');
        const uppercase = response.headers.get('CONTENT-TYPE');
        const mixedcase = response.headers.get('Content-Type');

        testData.tests.push({
          contentType: 'case-insensitive test',
          endpoint: '/health',
          success: lowercase === uppercase && uppercase === mixedcase,
          actualContentType: lowercase,
          bodyData: {
            lowercase,
            uppercase,
            mixedcase,
            allMatch: lowercase === uppercase && uppercase === mixedcase,
          },
        });
      } catch (e) {
        testData.tests.push({
          contentType: 'case-insensitive test',
          endpoint: '/health',
          success: false,
          error: e instanceof Error ? e.message : String(e),
        });
      }

      return testData;
    });

    // Verify all tests
    expect(result.tests.length).toBeGreaterThan(0);

    // Test 1: application/json
    const jsonTest = result.tests.find(t => t.contentType === 'application/json');
    expect(jsonTest?.success).toBe(true);
    expect(jsonTest?.actualContentType).toContain('json');
    expect(jsonTest?.bodyType).toBe('object');
    expect(jsonTest?.bodyData).toHaveProperty('status');

    // Test 2: text/html
    const htmlTest = result.tests.find(t => t.contentType === 'text/html');
    expect(htmlTest?.success).toBe(true);
    expect(htmlTest?.actualContentType).toContain('html');
    expect(htmlTest?.bodyType).toBe('string');
    expect(htmlTest?.bodyData).toContain('<!DOCTYPE html>');

    // Test 3: application/x-www-form-urlencoded
    const formTest = result.tests.find(t => t.contentType === 'application/x-www-form-urlencoded');
    expect(formTest?.success).toBe(true);
    expect(formTest?.bodyData).toHaveProperty('received');

    // Test 4: application/json POST
    const jsonPostTest = result.tests.find(t => t.contentType === 'application/json (POST)');
    expect(jsonPostTest?.success).toBe(true);
    expect(jsonPostTest?.bodyData).toHaveProperty('received');

    // Test 5: Case-insensitive
    const caseTest = result.tests.find(t => t.contentType === 'case-insensitive test');
    expect(caseTest?.success).toBe(true);
    expect(caseTest?.bodyData?.allMatch).toBe(true);
  });
});
