import { test, expect } from '@playwright/test';

test.describe('SessionStorage API', () => {
  test.beforeEach(async ({ page }) => {
    // Clear sessionStorage before each test
    // Use localhost origin instead of about:blank for storage access
    await page.goto('http://localhost:9080/');
    await page.evaluate(() => {
      sessionStorage.clear();
    });
  });

  test('STORE-005: Basic sessionStorage operations @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      const testData = {
        operations: [] as Array<{ operation: string; key: string; value: string | null }>,
        length: 0,
      };

      // Set values
      sessionStorage.setItem('sessionKey1', 'sessionValue1');
      testData.operations.push({
        operation: 'setItem',
        key: 'sessionKey1',
        value: sessionStorage.getItem('sessionKey1'),
      });

      sessionStorage.setItem('sessionKey2', 'sessionValue2');
      testData.operations.push({
        operation: 'setItem',
        key: 'sessionKey2',
        value: sessionStorage.getItem('sessionKey2'),
      });

      // Check length
      testData.length = sessionStorage.length;

      // Remove item
      sessionStorage.removeItem('sessionKey1');
      testData.operations.push({
        operation: 'removeItem',
        key: 'sessionKey1',
        value: sessionStorage.getItem('sessionKey1'),
      });

      return testData;
    });

    expect(result.operations[0].value).toBe('sessionValue1');
    expect(result.operations[1].value).toBe('sessionValue2');
    expect(result.length).toBe(2);
    expect(result.operations[2].value).toBeNull();
  });

  test('STORE-006: Session isolation between tabs @p1', async ({ page, context }) => {
    // Create two separate pages
    const page1 = page;
    const page2 = await context.newPage();

    try {
      await page1.goto('http://localhost:9080/');
      await page2.goto('http://localhost:9080/');

      // Set sessionStorage on page1
      await page1.evaluate(() => {
        sessionStorage.setItem('page1Data', 'fromPage1');
      });

      // Set different sessionStorage on page2
      await page2.evaluate(() => {
        sessionStorage.setItem('page2Data', 'fromPage2');
      });

      // Verify isolation
      const page1Data = await page1.evaluate(() => ({
        ownData: sessionStorage.getItem('page1Data'),
        otherData: sessionStorage.getItem('page2Data'),
        length: sessionStorage.length,
      }));

      const page2Data = await page2.evaluate(() => ({
        ownData: sessionStorage.getItem('page2Data'),
        otherData: sessionStorage.getItem('page1Data'),
        length: sessionStorage.length,
      }));

      // Each page should only see its own sessionStorage
      expect(page1Data.ownData).toBe('fromPage1');
      expect(page1Data.otherData).toBeNull();
      expect(page1Data.length).toBe(1);

      expect(page2Data.ownData).toBe('fromPage2');
      expect(page2Data.otherData).toBeNull();
      expect(page2Data.length).toBe(1);
    } finally {
      await page2.close();
    }
  });

  test('STORE-007: Session persistence across navigation @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    // Set sessionStorage
    await page.evaluate(() => {
      sessionStorage.setItem('persistentKey', 'persistentValue');
    });

    // Navigate to another page in same session (reload same origin to keep sessionStorage)
    await page.reload();

    // Verify sessionStorage persists
    const result = await page.evaluate(() => ({
      value: sessionStorage.getItem('persistentKey'),
      length: sessionStorage.length,
    }));

    expect(result.value).toBe('persistentValue');
    expect(result.length).toBe(1);
  });

  test('STORE-008: sessionStorage vs localStorage behavior @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      const testData = {
        sessionStorageWorks: false,
        localStorageWorks: false,
        areSeparate: false,
      };

      // Clear both
      sessionStorage.clear();
      localStorage.clear();

      // Set same key in both
      sessionStorage.setItem('sharedKey', 'sessionValue');
      localStorage.setItem('sharedKey', 'localStorage');

      // Verify they store independently
      const sessionValue = sessionStorage.getItem('sharedKey');
      const localValue = localStorage.getItem('sharedKey');

      testData.sessionStorageWorks = sessionValue === 'sessionValue';
      testData.localStorageWorks = localValue === 'localStorage';
      testData.areSeparate = sessionValue !== localValue;

      // Clean up
      localStorage.clear();

      return testData;
    });

    expect(result.sessionStorageWorks).toBe(true);
    expect(result.localStorageWorks).toBe(true);
    expect(result.areSeparate).toBe(true);
  });
});
