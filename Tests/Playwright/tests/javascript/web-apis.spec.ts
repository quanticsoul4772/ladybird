import { test, expect } from '@playwright/test';

/**
 * Web APIs Tests (JS-API-001 to JS-API-015)
 * Priority: P0 (Critical)
 *
 * Tests browser Web APIs including:
 * - Storage APIs (localStorage, sessionStorage)
 * - Console methods
 * - JSON serialization
 * - URL and URLSearchParams
 * - Navigator properties
 * - Window location and history
 * - Performance timers
 * - Crypto API
 * - Observer APIs (Intersection, Mutation, Resize)
 * - Animation frame
 */

test.describe('Web APIs', () => {

  test('JS-API-001: LocalStorage (set/get/remove)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Clear any existing data
      localStorage.clear();

      // Set items
      localStorage.setItem('key1', 'value1');
      localStorage.setItem('key2', 'value2');
      localStorage.setItem('json', JSON.stringify({ foo: 'bar' }));

      const afterSet = {
        length: localStorage.length,
        key1: localStorage.getItem('key1'),
        key2: localStorage.getItem('key2')
      };

      // Get by index
      const firstKey = localStorage.key(0);

      // Remove item
      localStorage.removeItem('key1');

      const afterRemove = {
        length: localStorage.length,
        key1: localStorage.getItem('key1'),
        key2: localStorage.getItem('key2')
      };

      // Clear all
      localStorage.clear();
      const afterClear = localStorage.length;

      return { afterSet, firstKey, afterRemove, afterClear };
    });

    expect(result.afterSet.length).toBe(3);
    expect(result.afterSet.key1).toBe('value1');
    expect(result.afterSet.key2).toBe('value2');
    expect(result.firstKey).toBeTruthy();
    expect(result.afterRemove.length).toBe(2);
    expect(result.afterRemove.key1).toBeNull();
    expect(result.afterRemove.key2).toBe('value2');
    expect(result.afterClear).toBe(0);
  });

  test('JS-API-002: SessionStorage', { tag: '@p0' }, async ({ page, context }) => {
    await page.goto('data:text/html,<body></body>');

    // Set sessionStorage in first page
    await page.evaluate(() => {
      sessionStorage.clear();
      sessionStorage.setItem('sessionKey', 'sessionValue');
      sessionStorage.setItem('counter', '42');
    });

    const firstPageData = await page.evaluate(() => {
      return {
        sessionKey: sessionStorage.getItem('sessionKey'),
        counter: sessionStorage.getItem('counter'),
        length: sessionStorage.length
      };
    });

    expect(firstPageData.sessionKey).toBe('sessionValue');
    expect(firstPageData.counter).toBe('42');
    expect(firstPageData.length).toBe(2);

    // Open new tab - sessionStorage should be separate
    const newPage = await context.newPage();
    await newPage.goto('data:text/html,<body></body>');

    const newPageData = await newPage.evaluate(() => {
      return {
        length: sessionStorage.length,
        hasSessionKey: sessionStorage.getItem('sessionKey') !== null
      };
    });

    // SessionStorage is per-tab/page
    expect(newPageData.length).toBe(0);
    expect(newPageData.hasSessionKey).toBe(false);

    await newPage.close();
  });

  test('JS-API-003: Console methods (log, warn, error)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    // Listen for console messages
    const messages: Array<{type: string, text: string}> = [];
    page.on('console', msg => {
      messages.push({
        type: msg.type(),
        text: msg.text()
      });
    });

    await page.evaluate(() => {
      console.log('Log message');
      console.warn('Warning message');
      console.error('Error message');
      console.info('Info message');
      console.debug('Debug message');
      console.log('Multiple', 'arguments', 123);
    });

    // Wait for messages to be collected
    await page.waitForTimeout(100);

    // Verify console methods were called
    expect(messages.some(m => m.type === 'log' && m.text.includes('Log message'))).toBe(true);
    expect(messages.some(m => m.type === 'warning' && m.text.includes('Warning message'))).toBe(true);
    expect(messages.some(m => m.type === 'error' && m.text.includes('Error message'))).toBe(true);
    expect(messages.some(m => m.text.includes('Multiple'))).toBe(true);
  });

  test('JS-API-004: JSON.parse/stringify', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const obj = {
        string: 'hello',
        number: 42,
        boolean: true,
        null: null,
        array: [1, 2, 3],
        nested: { foo: 'bar' }
      };

      // Stringify
      const json = JSON.stringify(obj);
      const jsonPretty = JSON.stringify(obj, null, 2);

      // Parse
      const parsed = JSON.parse(json);

      // Parse with error handling
      let parseError = null;
      try {
        JSON.parse('invalid json');
      } catch (error) {
        parseError = (error as Error).message;
      }

      return {
        json,
        isPrettyMultiline: jsonPretty.includes('\n'),
        parsed,
        parseError
      };
    });

    expect(result.json).toContain('"string":"hello"');
    expect(result.isPrettyMultiline).toBe(true);
    expect(result.parsed.string).toBe('hello');
    expect(result.parsed.number).toBe(42);
    expect(result.parsed.array).toEqual([1, 2, 3]);
    expect(result.parseError).toBeTruthy();
  });

  test('JS-API-005: URL parsing (new URL())', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const url = new URL('https://example.com:8080/path/to/page?query=value&foo=bar#section');

      return {
        href: url.href,
        protocol: url.protocol,
        hostname: url.hostname,
        port: url.port,
        pathname: url.pathname,
        search: url.search,
        hash: url.hash,
        origin: url.origin
      };
    });

    expect(result.protocol).toBe('https:');
    expect(result.hostname).toBe('example.com');
    expect(result.port).toBe('8080');
    expect(result.pathname).toBe('/path/to/page');
    expect(result.search).toBe('?query=value&foo=bar');
    expect(result.hash).toBe('#section');
    expect(result.origin).toBe('https://example.com:8080');
  });

  test('JS-API-006: URLSearchParams', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const params = new URLSearchParams('foo=bar&name=John&age=30');

      const initial = {
        foo: params.get('foo'),
        name: params.get('name'),
        has: params.has('age')
      };

      // Set new parameter
      params.set('city', 'NYC');

      // Append (allows duplicates)
      params.append('hobby', 'reading');
      params.append('hobby', 'gaming');

      // Get all values for key
      const hobbies = params.getAll('hobby');

      // Delete parameter
      params.delete('age');

      // Iterate
      const entries: string[] = [];
      params.forEach((value, key) => {
        entries.push(`${key}=${value}`);
      });

      return {
        initial,
        hobbies,
        hasAge: params.has('age'),
        toString: params.toString(),
        entries
      };
    });

    expect(result.initial.foo).toBe('bar');
    expect(result.initial.name).toBe('John');
    expect(result.initial.has).toBe(true);
    expect(result.hobbies).toEqual(['reading', 'gaming']);
    expect(result.hasAge).toBe(false);
    expect(result.toString).toContain('foo=bar');
    expect(result.toString).toContain('hobby=reading');
  });

  test('JS-API-007: Navigator properties', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      return {
        userAgent: navigator.userAgent,
        language: navigator.language,
        onLine: navigator.onLine,
        cookieEnabled: navigator.cookieEnabled,
        hasUserAgent: typeof navigator.userAgent === 'string',
        hasLanguage: typeof navigator.language === 'string',
        platform: navigator.platform || 'unknown'
      };
    });

    expect(result.hasUserAgent).toBe(true);
    expect(result.hasLanguage).toBe(true);
    expect(result.userAgent.length).toBeGreaterThan(0);
    expect(typeof result.onLine).toBe('boolean');
    expect(typeof result.cookieEnabled).toBe('boolean');
  });

  test('JS-API-008: Window.location', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const loc = window.location;

      return {
        href: loc.href,
        protocol: loc.protocol,
        host: loc.host,
        pathname: loc.pathname,
        search: loc.search,
        hash: loc.hash
      };
    });

    expect(result.href).toContain('data:text/html');
    expect(result.protocol).toBeTruthy();
  });

  test('JS-API-009: Window.history (pushState, replaceState)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    await page.evaluate(() => {
      // Push new state
      history.pushState({ page: 1 }, 'Page 1', '?page=1');
      history.pushState({ page: 2 }, 'Page 2', '?page=2');

      // Replace current state
      history.replaceState({ page: 2, modified: true }, 'Page 2 Modified', '?page=2&mod=1');
    });

    const url = await page.url();
    expect(url).toContain('page=2');
    expect(url).toContain('mod=1');

    // Test back navigation
    await page.goBack();
    const backUrl = await page.url();
    expect(backUrl).toContain('page=1');
  });

  test('JS-API-010: Timers (performance.now())', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      const start = performance.now();

      // Wait a bit
      await new Promise(resolve => setTimeout(resolve, 50));

      const end = performance.now();
      const elapsed = end - start;

      return {
        start,
        end,
        elapsed,
        isPositive: start > 0,
        endGreaterThanStart: end > start,
        elapsedReasonable: elapsed >= 50 && elapsed < 200
      };
    });

    expect(result.isPositive).toBe(true);
    expect(result.endGreaterThanStart).toBe(true);
    expect(result.elapsedReasonable).toBe(true);
  });

  test('JS-API-011: Crypto.getRandomValues()', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      try {
        // Test Uint8Array
        const array8 = new Uint8Array(10);
        crypto.getRandomValues(array8);

        // Test Uint32Array
        const array32 = new Uint32Array(5);
        crypto.getRandomValues(array32);

        return {
          hasNonZero8: Array.from(array8).some(v => v !== 0),
          hasNonZero32: Array.from(array32).some(v => v !== 0),
          length8: array8.length,
          length32: array32.length,
          allInRange: Array.from(array8).every(v => v >= 0 && v <= 255)
        };
      } catch (error) {
        return {
          error: (error as Error).message
        };
      }
    });

    if ('error' in result) {
      console.log('Crypto API not available:', result.error);
      test.skip();
    }

    // Random values should have at least some non-zero values
    expect(result.hasNonZero8).toBe(true);
    expect(result.hasNonZero32).toBe(true);
    expect(result.length8).toBe(10);
    expect(result.length32).toBe(5);
    expect(result.allInRange).toBe(true);
  });

  test('JS-API-012: IntersectionObserver', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body style="height: 3000px;">
          <div id="target" style="margin-top: 2000px; height: 100px; background: red;">
            Target Element
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const target = document.getElementById('target');
        const observations: any[] = [];

        try {
          const observer = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
              observations.push({
                isIntersecting: entry.isIntersecting,
                intersectionRatio: entry.intersectionRatio,
                targetId: (entry.target as HTMLElement).id
              });
            });
          });

          observer.observe(target!);

          // Scroll to make element visible
          setTimeout(() => {
            window.scrollTo(0, 2000);

            setTimeout(() => {
              observer.disconnect();
              resolve({
                observationCount: observations.length,
                observations
              });
            }, 200);
          }, 100);
        } catch (error) {
          resolve({ error: (error as Error).message });
        }
      });
    });

    if ('error' in result) {
      console.log('IntersectionObserver not available:', result.error);
      test.skip();
    }

    expect(result.observationCount).toBeGreaterThan(0);
  });

  test('JS-API-013: MutationObserver', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="container"></div></body>');

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const container = document.getElementById('container');
        const mutations: any[] = [];

        try {
          const observer = new MutationObserver((mutationsList) => {
            mutationsList.forEach(mutation => {
              mutations.push({
                type: mutation.type,
                addedNodes: mutation.addedNodes.length,
                removedNodes: mutation.removedNodes.length
              });
            });
          });

          observer.observe(container!, {
            childList: true,
            attributes: true,
            characterData: true
          });

          // Make changes
          const p = document.createElement('p');
          p.textContent = 'New paragraph';
          container?.appendChild(p);

          container?.setAttribute('data-modified', 'true');

          setTimeout(() => {
            observer.disconnect();
            resolve({
              mutationCount: mutations.length,
              hasChildList: mutations.some(m => m.type === 'childList'),
              hasAttributes: mutations.some(m => m.type === 'attributes')
            });
          }, 100);
        } catch (error) {
          resolve({ error: (error as Error).message });
        }
      });
    });

    if ('error' in result) {
      console.log('MutationObserver not available:', result.error);
      test.skip();
    }

    expect(result.mutationCount).toBeGreaterThan(0);
    expect(result.hasChildList).toBe(true);
    expect(result.hasAttributes).toBe(true);
  });

  test('JS-API-014: ResizeObserver', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="target" style="width: 100px; height: 100px;"></div></body>');

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const target = document.getElementById('target');
        const observations: any[] = [];

        try {
          const observer = new ResizeObserver((entries) => {
            entries.forEach(entry => {
              observations.push({
                width: entry.contentRect.width,
                height: entry.contentRect.height
              });
            });
          });

          observer.observe(target!);

          // Initial observation
          setTimeout(() => {
            // Resize element
            if (target) {
              target.style.width = '200px';
              target.style.height = '200px';
            }

            setTimeout(() => {
              observer.disconnect();
              resolve({
                observationCount: observations.length,
                observations
              });
            }, 200);
          }, 100);
        } catch (error) {
          resolve({ error: (error as Error).message });
        }
      });
    });

    if ('error' in result) {
      console.log('ResizeObserver not available:', result.error);
      test.skip();
    }

    expect(result.observationCount).toBeGreaterThan(0);
  });

  test('JS-API-015: requestAnimationFrame', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="box" style="width: 0px;"></div></body>');

    const result = await page.evaluate(() => {
      return new Promise((resolve) => {
        const box = document.getElementById('box');
        let frameCount = 0;
        let lastTimestamp = 0;

        function animate(timestamp: number) {
          frameCount++;

          // Update element
          if (box) {
            const width = parseInt(box.style.width);
            box.style.width = (width + 10) + 'px';
          }

          if (frameCount < 5) {
            requestAnimationFrame(animate);
          } else {
            resolve({
              frameCount,
              finalWidth: parseInt(box?.style.width || '0'),
              timestampIsNumber: typeof timestamp === 'number'
            });
          }
        }

        requestAnimationFrame(animate);
      });
    });

    expect(result.frameCount).toBe(5);
    expect(result.finalWidth).toBe(50); // 5 frames * 10px
    expect(result.timestampIsNumber).toBe(true);
  });

});
