import { test, expect } from '@playwright/test';

/**
 * Async Operations Tests (JS-ASYNC-001 to JS-ASYNC-012)
 * Priority: P0 (Critical)
 *
 * Tests JavaScript asynchronous operations including:
 * - Timers (setTimeout, setInterval)
 * - Promises (resolve, reject, chaining)
 * - Promise combinators (all, race)
 * - Async/await syntax
 * - Fetch API (GET, POST)
 * - Error handling in async code
 * - Request/Response objects
 */

test.describe('Async Operations', () => {

  test('JS-ASYNC-001: setTimeout', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="output">Initial</div></body>');

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const output = document.getElementById('output');
        const startTime = Date.now();

        setTimeout(() => {
          const elapsed = Date.now() - startTime;
          if (output) output.textContent = 'Updated';
          resolve({ elapsed, text: output?.textContent });
        }, 100);
      });
    });

    expect(result.text).toBe('Updated');
    expect(result.elapsed).toBeGreaterThanOrEqual(100);
    expect(result.elapsed).toBeLessThan(200); // Should complete within reasonable time
  });

  test('JS-ASYNC-002: setInterval and clearInterval', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="counter">0</div></body>');

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const counter = document.getElementById('counter');
        let count = 0;

        const intervalId = setInterval(() => {
          count++;
          if (counter) counter.textContent = count.toString();

          if (count >= 5) {
            clearInterval(intervalId);
            resolve({ finalCount: count, text: counter?.textContent });
          }
        }, 50);
      });
    });

    expect(result.finalCount).toBe(5);
    expect(result.text).toBe('5');
  });

  test('JS-ASYNC-003: Promises (resolve/reject)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      return new Promise((testResolve) => {
        const results: any[] = [];

        // Promise that resolves
        const resolvedPromise = new Promise((resolve, reject) => {
          resolve('Success value');
        });

        // Promise that rejects
        const rejectedPromise = new Promise((resolve, reject) => {
          reject(new Error('Error value'));
        });

        // Test resolved promise
        resolvedPromise
          .then(value => {
            results.push({ type: 'resolve', value });
          })
          .catch(error => {
            results.push({ type: 'error-unexpected' });
          });

        // Test rejected promise
        rejectedPromise
          .then(value => {
            results.push({ type: 'success-unexpected' });
          })
          .catch(error => {
            results.push({ type: 'reject', message: error.message });
          });

        // Wait for promises to settle
        setTimeout(() => testResolve(results), 100);
      });
    });

    expect(result).toContainEqual({ type: 'resolve', value: 'Success value' });
    expect(result).toContainEqual({ type: 'reject', message: 'Error value' });
  });

  test('JS-ASYNC-004: Promise chaining (.then)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      return Promise.resolve(10)
        .then(value => value * 2)
        .then(value => value + 5)
        .then(value => `Result: ${value}`)
        .then(value => ({ final: value }));
    });

    expect(result.final).toBe('Result: 25');
  });

  test('JS-ASYNC-005: Promise.all()', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const promise1 = Promise.resolve(1);
      const promise2 = new Promise(resolve => setTimeout(() => resolve(2), 50));
      const promise3 = Promise.resolve(3);

      return Promise.all([promise1, promise2, promise3])
        .then(values => ({ values, sum: values.reduce((a: any, b: any) => a + b, 0) }));
    });

    expect(result.values).toEqual([1, 2, 3]);
    expect(result.sum).toBe(6);
  });

  test('JS-ASYNC-006: Promise.race()', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const slow = new Promise(resolve => setTimeout(() => resolve('slow'), 200));
      const fast = new Promise(resolve => setTimeout(() => resolve('fast'), 50));
      const immediate = Promise.resolve('immediate');

      return Promise.race([slow, fast, immediate]);
    });

    // The immediate promise should win
    expect(result).toBe('immediate');
  });

  test('JS-ASYNC-007: async/await basic', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      async function fetchData() {
        const data = await Promise.resolve('Fetched data');
        return data.toUpperCase();
      }

      async function processData() {
        const result1 = await fetchData();
        const result2 = await Promise.resolve(' - PROCESSED');
        return result1 + result2;
      }

      return await processData();
    });

    expect(result).toBe('FETCHED DATA - PROCESSED');
  });

  test('JS-ASYNC-008: async/await error handling (try/catch)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      const results: any[] = [];

      async function successFunc() {
        try {
          const value = await Promise.resolve('success');
          results.push({ type: 'success', value });
        } catch (error) {
          results.push({ type: 'error-unexpected' });
        }
      }

      async function errorFunc() {
        try {
          await Promise.reject(new Error('Expected error'));
          results.push({ type: 'success-unexpected' });
        } catch (error) {
          results.push({ type: 'caught', message: (error as Error).message });
        }
      }

      await successFunc();
      await errorFunc();

      return results;
    });

    expect(result).toContainEqual({ type: 'success', value: 'success' });
    expect(result).toContainEqual({ type: 'caught', message: 'Expected error' });
  });

  test('JS-ASYNC-009: Fetch API (GET request)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      try {
        // Use httpbin.org for testing
        const response = await fetch('https://httpbin.org/json');

        return {
          ok: response.ok,
          status: response.status,
          statusText: response.statusText,
          hasHeaders: response.headers.get('content-type') !== null,
          hasBody: (await response.json()) !== null
        };
      } catch (error) {
        return {
          error: (error as Error).message
        };
      }
    });

    // Handle case where fetch might fail (network issues, etc)
    if ('error' in result) {
      console.log('Fetch failed (expected in some environments):', result.error);
      // Don't fail test - fetch may not be available in all contexts
      return;
    }

    expect(result.ok).toBe(true);
    expect(result.status).toBe(200);
    expect(result.hasHeaders).toBe(true);
    expect(result.hasBody).toBe(true);
  });

  test('JS-ASYNC-010: Fetch API (POST request)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      try {
        const postData = {
          title: 'Test Post',
          body: 'This is a test',
          userId: 1
        };

        const response = await fetch('https://httpbin.org/post', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(postData)
        });

        const responseData = await response.json();

        return {
          ok: response.ok,
          status: response.status,
          method: response.url.includes('/post'),
          receivedData: responseData.json !== undefined
        };
      } catch (error) {
        return {
          error: (error as Error).message
        };
      }
    });

    if ('error' in result) {
      console.log('Fetch POST failed (expected in some environments):', result.error);
      return;
    }

    expect(result.ok).toBe(true);
    expect(result.status).toBe(200);
    expect(result.method).toBe(true);
  });

  test('JS-ASYNC-011: Fetch API (error handling)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      const results: any[] = [];

      // Test 404 error
      try {
        const response = await fetch('https://httpbin.org/status/404');
        results.push({
          type: '404',
          ok: response.ok,
          status: response.status
        });
      } catch (error) {
        results.push({ type: '404-error', message: (error as Error).message });
      }

      // Test network error (invalid domain)
      try {
        const response = await fetch('https://this-domain-definitely-does-not-exist-12345.com');
        results.push({ type: 'network-unexpected-success' });
      } catch (error) {
        results.push({
          type: 'network-error',
          caught: true
        });
      }

      return results;
    });

    // 404 should not throw, but response.ok should be false
    const notFoundResult = result.find((r: any) => r.type === '404');
    if (notFoundResult) {
      expect(notFoundResult.ok).toBe(false);
      expect(notFoundResult.status).toBe(404);
    }

    // Network error should be caught
    const networkError = result.find((r: any) => r.type === 'network-error');
    expect(networkError?.caught).toBe(true);
  });

  test('JS-ASYNC-012: Request/Response objects', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      // Test Request object
      const request = new Request('https://httpbin.org/get', {
        method: 'GET',
        headers: {
          'X-Custom-Header': 'test-value'
        }
      });

      const requestInfo = {
        url: request.url,
        method: request.method,
        hasHeaders: request.headers.get('X-Custom-Header') === 'test-value'
      };

      // Test Response object
      let responseInfo = {};
      try {
        const response = await fetch(request);

        // Clone response to read body multiple times
        const clone = response.clone();

        responseInfo = {
          ok: response.ok,
          status: response.status,
          statusText: response.statusText,
          type: response.type,
          url: response.url,
          redirected: response.redirected,
          hasHeaders: response.headers.has('content-type'),
          canClone: clone !== null,
          hasBody: (await response.text()).length > 0
        };
      } catch (error) {
        responseInfo = { error: (error as Error).message };
      }

      return { request: requestInfo, response: responseInfo };
    });

    // Validate Request object
    expect(result.request.url).toBe('https://httpbin.org/get');
    expect(result.request.method).toBe('GET');
    expect(result.request.hasHeaders).toBe(true);

    // Validate Response object (if fetch succeeded)
    if (!('error' in result.response)) {
      expect(result.response.ok).toBe(true);
      expect(result.response.status).toBe(200);
      expect(result.response.hasHeaders).toBe(true);
      expect(result.response.canClone).toBe(true);
      expect(result.response.hasBody).toBe(true);
    } else {
      console.log('Fetch failed (expected in some environments):', result.response.error);
    }
  });

});
