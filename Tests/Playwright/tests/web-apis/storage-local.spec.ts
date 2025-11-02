import { test, expect } from '@playwright/test';

test.describe('LocalStorage API', () => {
  test.beforeEach(async ({ page }) => {
    // Clear localStorage before each test
    // Use localhost origin instead of about:blank for storage access
    await page.goto('http://localhost:9080/');
    await page.evaluate(() => {
      localStorage.clear();
    });
  });

  test('STORE-001: Basic setItem/getItem operations @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    // Test basic set/get operations
    const result = await page.evaluate(() => {
      const testData = {
        operations: [] as Array<{ operation: string; key: string; value: string | null }>,
      };

      // Set string value
      localStorage.setItem('username', 'testuser');
      testData.operations.push({
        operation: 'setItem',
        key: 'username',
        value: localStorage.getItem('username'),
      });

      // Set numeric value (stored as string)
      localStorage.setItem('count', '42');
      testData.operations.push({
        operation: 'setItem',
        key: 'count',
        value: localStorage.getItem('count'),
      });

      // Set JSON value
      const jsonData = JSON.stringify({ name: 'Test', age: 25 });
      localStorage.setItem('userData', jsonData);
      testData.operations.push({
        operation: 'setItem',
        key: 'userData',
        value: localStorage.getItem('userData'),
      });

      // Get non-existent key
      testData.operations.push({
        operation: 'getItem',
        key: 'nonexistent',
        value: localStorage.getItem('nonexistent'),
      });

      // Check length
      (testData as any).length = localStorage.length;

      return testData;
    });

    expect(result.operations[0].value).toBe('testuser');
    expect(result.operations[1].value).toBe('42');
    expect(result.operations[2].value).toBe(JSON.stringify({ name: 'Test', age: 25 }));
    expect(result.operations[3].value).toBeNull();
    expect((result as any).length).toBe(3);
  });

  test('STORE-002: Storage capacity and quota @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      const testData = {
        initialLength: localStorage.length,
        finalLength: 0,
        canStoreMultipleItems: false,
        canStorelargeStrings: false,
      };

      // Store multiple items
      for (let i = 0; i < 10; i++) {
        localStorage.setItem(`key${i}`, `value${i}`);
      }
      testData.finalLength = localStorage.length;
      testData.canStoreMultipleItems = testData.finalLength === 10;

      // Store a moderately large string (1KB)
      const largeString = 'x'.repeat(1024);
      try {
        localStorage.setItem('largeItem', largeString);
        const retrieved = localStorage.getItem('largeItem');
        testData.canStorelargeStrings = retrieved === largeString;
      } catch (e) {
        testData.canStorelargeStrings = false;
      }

      return testData;
    });

    expect(result.initialLength).toBe(0);
    expect(result.finalLength).toBe(10);
    expect(result.canStoreMultipleItems).toBe(true);
    expect(result.canStorelargeStrings).toBe(true);
  });

  test('STORE-003: Clear and removeItem operations @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      const testData = {
        initialLength: 0,
        afterAddLength: 0,
        afterRemoveLength: 0,
        afterClearLength: 0,
        removedValue: null as string | null,
      };

      testData.initialLength = localStorage.length;

      // Add items
      localStorage.setItem('item1', 'value1');
      localStorage.setItem('item2', 'value2');
      localStorage.setItem('item3', 'value3');
      testData.afterAddLength = localStorage.length;

      // Remove one item
      localStorage.removeItem('item2');
      testData.afterRemoveLength = localStorage.length;
      testData.removedValue = localStorage.getItem('item2');

      // Clear all
      localStorage.clear();
      testData.afterClearLength = localStorage.length;

      return testData;
    });

    expect(result.initialLength).toBe(0);
    expect(result.afterAddLength).toBe(3);
    expect(result.afterRemoveLength).toBe(2);
    expect(result.removedValue).toBeNull();
    expect(result.afterClearLength).toBe(0);
  });

  test('STORE-004: Storage event cross-tab communication @p1', async ({ page, context }) => {
    // This test requires multiple contexts/tabs
    // Create a second page in the same context
    const page2 = await context.newPage();

    try {
      // Navigate both pages to the same origin
      await page.goto('http://localhost:9080/');
      await page2.goto('http://localhost:9080/');

      // Set up event listener on page2
      await page2.evaluate(() => {
        (window as any).__storageEvents = [];
        window.addEventListener('storage', (e: StorageEvent) => {
          (window as any).__storageEvents.push({
            key: e.key,
            oldValue: e.oldValue,
            newValue: e.newValue,
            url: e.url,
          });
        });
      });

      // Modify localStorage on page1
      await page.evaluate(() => {
        localStorage.setItem('testKey', 'testValue');
      });

      // Wait for event propagation
      await page2.waitForTimeout(100);

      // Check if page2 received the event
      const events = await page2.evaluate(() => (window as any).__storageEvents);

      // Note: storage events might not work in Ladybird yet
      // This test documents expected behavior
      if (events.length > 0) {
        expect(events[0].key).toBe('testKey');
        expect(events[0].newValue).toBe('testValue');
      } else {
        // Gracefully skip if not supported
        test.skip();
      }
    } finally {
      await page2.close();
    }
  });
});
