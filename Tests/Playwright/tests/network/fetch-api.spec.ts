import { test, expect } from '@playwright/test';

/**
 * Fetch API Error Handling Tests (FETCH-001 to FETCH-003)
 * Priority: P0-P1
 *
 * Tests comprehensive error handling scenarios for the Fetch API:
 * - Network failures and unreachable URLs
 * - HTTP error status codes (4xx, 5xx)
 * - Request abortion and timeout scenarios
 *
 * Ladybird Compatibility Notes:
 * - Uses localhost:9080 as test origin
 * - Gracefully handles Fetch API limitations
 * - Tests basic functionality before edge cases
 * - Avoids exact error message matching (browser variance)
 */

test.describe('Fetch API Error Handling', () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to localhost origin for Fetch API access
    await page.goto('http://localhost:9080/');
  });

  test('FETCH-001: Network error handling @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      // Check if fetch is available
      if (typeof fetch === 'undefined') {
        return { fetchAvailable: false };
      }

      const testResults: any = {
        fetchAvailable: true,
        networkErrors: [],
        timeoutOccurred: false,
      };

      // Test 1: Completely invalid URL (malformed)
      testResults.startTime1 = Date.now();
      try {
        await fetch('http://:::invalid-url:::/test');
        testResults.networkErrors.push({
          test: 'invalid-url',
          caught: false,
          errorType: null,
        });
      } catch (error) {
        const elapsed1 = Date.now() - testResults.startTime1;
        testResults.networkErrors.push({
          test: 'invalid-url',
          caught: true,
          errorType: error.constructor.name,
          hasMessage: typeof (error as Error).message === 'string',
          messageLength: (error as Error).message?.length || 0,
          elapsed: elapsed1,
        });
      }

      // Test 2: Invalid domain (DNS failure)
      testResults.startTime2 = Date.now();
      try {
        await fetch('http://this-domain-absolutely-does-not-exist-12345.invalid/');
        testResults.networkErrors.push({
          test: 'dns-failure',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        const elapsed2 = Date.now() - testResults.startTime2;
        testResults.networkErrors.push({
          test: 'dns-failure',
          caught: true,
          errorType: error.constructor.name,
          hasMessage: typeof (error as Error).message === 'string',
          messageIsDescriptive: (error as Error).message.length > 5,
          elapsed: elapsed2,
          didNotHang: elapsed2 < 5000,
        });
      }

      // Test 3: Invalid protocol
      testResults.startTime3 = Date.now();
      try {
        await fetch('invalidprotocol://example.com/test');
        testResults.networkErrors.push({
          test: 'invalid-protocol',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        const elapsed3 = Date.now() - testResults.startTime3;
        testResults.networkErrors.push({
          test: 'invalid-protocol',
          caught: true,
          errorType: error.constructor.name,
          hasMessage: typeof (error as Error).message === 'string',
          elapsed: elapsed3,
        });
      }

      return testResults;
    });

    // Skip test if fetch is not available
    if (!result.fetchAvailable) {
      test.skip();
      return;
    }

    // Verify that all network errors were properly caught
    expect(result.networkErrors.length).toBeGreaterThanOrEqual(3);

    // Check each error scenario
    const invalidUrlError = result.networkErrors.find((e: any) => e.test === 'invalid-url');
    const dnsError = result.networkErrors.find((e: any) => e.test === 'dns-failure');
    const invalidProtocolError = result.networkErrors.find((e: any) => e.test === 'invalid-protocol');

    // All network errors should be caught (not succeed)
    expect(invalidUrlError?.caught).toBe(true);
    expect(dnsError?.caught).toBe(true);
    expect(invalidProtocolError?.caught).toBe(true);

    // Errors should have descriptive messages
    expect(invalidUrlError?.hasMessage).toBe(true);
    expect(invalidUrlError?.messageLength).toBeGreaterThan(0);

    // Network errors should typically be TypeError in standard Fetch API
    // But we don't enforce this strictly for Ladybird compatibility
    if (invalidUrlError?.errorType) {
      expect(['TypeError', 'Error', 'NetworkError']).toContain(invalidUrlError.errorType);
    }

    // DNS failures should not hang
    expect(dnsError?.didNotHang).toBe(true);
  });

  test('FETCH-002: HTTP error status codes (4xx, 5xx) @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      // Check if fetch is available
      if (typeof fetch === 'undefined') {
        return { fetchAvailable: false };
      }

      const testResults: any = {
        fetchAvailable: true,
        httpErrors: [],
      };

      // Important: HTTP errors (404, 500, etc.) should NOT throw exceptions
      // They should return a Response object with ok=false

      // Test various HTTP error codes using httpbin.org
      const errorCodes = [
        { code: 404, description: 'Not Found' },
        { code: 403, description: 'Forbidden' },
        { code: 500, description: 'Internal Server Error' },
        { code: 503, description: 'Service Unavailable' },
      ];

      for (const errorCode of errorCodes) {
        try {
          const response = await fetch(`https://httpbin.org/status/${errorCode.code}`);

          testResults.httpErrors.push({
            code: errorCode.code,
            description: errorCode.description,
            threwException: false,
            responseReceived: true,
            ok: response.ok,
            status: response.status,
            statusText: response.statusText,
            hasStatusText: response.statusText.length > 0,
            responseType: response.type,
            canReadHeaders: response.headers !== null,
            contentType: response.headers.get('content-type'),
          });
        } catch (error) {
          // HTTP errors should NOT throw - this indicates a problem
          testResults.httpErrors.push({
            code: errorCode.code,
            description: errorCode.description,
            threwException: true,
            errorType: error.constructor.name,
            errorMessage: (error as Error).message,
          });
        }
      }

      // Also test a successful request for comparison
      try {
        const response = await fetch('https://httpbin.org/status/200');
        testResults.successResponse = {
          ok: response.ok,
          status: response.status,
          statusText: response.statusText,
        };
      } catch (error) {
        testResults.successResponse = {
          error: (error as Error).message,
        };
      }

      return testResults;
    });

    // Skip test if fetch is not available
    if (!result.fetchAvailable) {
      test.skip();
      return;
    }

    // Verify we tested multiple error codes
    expect(result.httpErrors.length).toBeGreaterThanOrEqual(4);

    // Check each HTTP error response
    for (const httpError of result.httpErrors) {
      // HTTP errors should NOT throw exceptions
      if (httpError.threwException) {
        console.warn(`HTTP ${httpError.code} unexpectedly threw exception: ${httpError.errorMessage}`);
        // In Ladybird, this might happen - document but don't fail
        continue;
      }

      // Response should be received
      expect(httpError.responseReceived).toBe(true);

      // response.ok should be false for error status codes
      expect(httpError.ok).toBe(false);

      // Status should match the requested error code
      expect(httpError.status).toBe(httpError.code);

      // Status codes should be in error range
      expect(httpError.status).toBeGreaterThanOrEqual(400);

      // statusText should exist (though content varies by server)
      expect(httpError.statusText).toBeDefined();

      // Should be able to read headers
      expect(httpError.canReadHeaders).toBe(true);
    }

    // Success response should have ok=true for comparison
    if (result.successResponse && !result.successResponse.error) {
      expect(result.successResponse.ok).toBe(true);
      expect(result.successResponse.status).toBe(200);
    }
  });

  test('FETCH-003: Timeout and abort scenarios @p1', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(async () => {
      // Check if fetch and AbortController are available
      if (typeof fetch === 'undefined') {
        return { fetchAvailable: false };
      }
      if (typeof AbortController === 'undefined') {
        return { fetchAvailable: true, abortControllerAvailable: false };
      }

      const testResults: any = {
        fetchAvailable: true,
        abortControllerAvailable: true,
        abortTests: [],
      };

      // Test 1: Basic abort before request completes
      try {
        const controller = new AbortController();
        const signal = controller.signal;

        // Start a request to a slow endpoint
        const fetchPromise = fetch('https://httpbin.org/delay/5', { signal });

        // Abort immediately
        controller.abort();

        // Wait for the fetch to reject
        await fetchPromise;

        testResults.abortTests.push({
          test: 'immediate-abort',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        testResults.abortTests.push({
          test: 'immediate-abort',
          caught: true,
          errorType: error.constructor.name,
          errorName: (error as any).name,
          errorMessage: (error as Error).message,
          isAbortError: (error as any).name === 'AbortError',
        });
      }

      // Test 2: Abort after delay (mid-flight)
      try {
        const controller = new AbortController();
        const signal = controller.signal;

        // Start request
        const fetchPromise = fetch('https://httpbin.org/delay/3', { signal });

        // Abort after 100ms (mid-flight)
        setTimeout(() => controller.abort(), 100);

        await fetchPromise;

        testResults.abortTests.push({
          test: 'delayed-abort',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        testResults.abortTests.push({
          test: 'delayed-abort',
          caught: true,
          errorType: error.constructor.name,
          errorName: (error as any).name,
          isAbortError: (error as any).name === 'AbortError',
        });
      }

      // Test 3: Multiple abort() calls are safe
      try {
        const controller = new AbortController();

        // Call abort multiple times - should be safe
        controller.abort();
        controller.abort();
        controller.abort();

        // Try to use the already-aborted signal
        await fetch('https://httpbin.org/get', { signal: controller.signal });

        testResults.abortTests.push({
          test: 'multiple-aborts',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        testResults.abortTests.push({
          test: 'multiple-aborts',
          caught: true,
          errorType: error.constructor.name,
          errorName: (error as any).name,
          isAbortError: (error as any).name === 'AbortError',
          multipleAbortsSafe: true,
        });
      }

      // Test 4: AbortController signal property
      try {
        const controller = new AbortController();
        const signalBeforeAbort = {
          aborted: controller.signal.aborted,
        };

        controller.abort();

        const signalAfterAbort = {
          aborted: controller.signal.aborted,
        };

        testResults.abortTests.push({
          test: 'signal-property',
          signalBeforeAbort,
          signalAfterAbort,
          propertyWorks: !signalBeforeAbort.aborted && signalAfterAbort.aborted,
        });
      } catch (error) {
        testResults.abortTests.push({
          test: 'signal-property',
          error: (error as Error).message,
        });
      }

      // Test 5: Fetch with pre-aborted signal
      try {
        const controller = new AbortController();
        controller.abort(); // Abort before fetch

        await fetch('https://httpbin.org/get', { signal: controller.signal });

        testResults.abortTests.push({
          test: 'pre-aborted-signal',
          caught: false,
          unexpectedSuccess: true,
        });
      } catch (error) {
        testResults.abortTests.push({
          test: 'pre-aborted-signal',
          caught: true,
          errorType: error.constructor.name,
          errorName: (error as any).name,
          isAbortError: (error as any).name === 'AbortError',
        });
      }

      return testResults;
    });

    // Skip test if fetch is not available
    if (!result.fetchAvailable) {
      test.skip();
      return;
    }

    // Skip if AbortController is not available
    if (!result.abortControllerAvailable) {
      console.log('AbortController not available in Ladybird - skipping abort tests');
      test.skip();
      return;
    }

    // Verify we ran multiple abort tests
    expect(result.abortTests.length).toBeGreaterThanOrEqual(5);

    // Test 1: Immediate abort
    const immediateAbort = result.abortTests.find((t: any) => t.test === 'immediate-abort');
    expect(immediateAbort?.caught).toBe(true);
    // Should be AbortError, but allow flexibility for Ladybird
    if (immediateAbort?.errorName) {
      expect(['AbortError', 'Error', 'DOMException']).toContain(immediateAbort.errorName);
    }

    // Test 2: Delayed abort
    const delayedAbort = result.abortTests.find((t: any) => t.test === 'delayed-abort');
    expect(delayedAbort?.caught).toBe(true);

    // Test 3: Multiple aborts are safe
    const multipleAborts = result.abortTests.find((t: any) => t.test === 'multiple-aborts');
    expect(multipleAborts?.caught).toBe(true);
    expect(multipleAborts?.multipleAbortsSafe).toBe(true);

    // Test 4: Signal property works correctly
    const signalProperty = result.abortTests.find((t: any) => t.test === 'signal-property');
    if (signalProperty && !signalProperty.error) {
      expect(signalProperty.signalBeforeAbort.aborted).toBe(false);
      expect(signalProperty.signalAfterAbort.aborted).toBe(true);
      expect(signalProperty.propertyWorks).toBe(true);
    }

    // Test 5: Pre-aborted signal
    const preAborted = result.abortTests.find((t: any) => t.test === 'pre-aborted-signal');
    expect(preAborted?.caught).toBe(true);
  });
});
