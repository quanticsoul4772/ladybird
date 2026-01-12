import { test, expect } from '@playwright/test';

test.describe('Web APIs - Storage (Ladybird-specific)', () => {
  test('JS-API-001: LocalStorage (set/get/remove) - using real URL', { tag: '@p0' }, async ({ page }) => {
    // Use real URL instead of data: URL (storage doesn't work in data: URLs)
    await page.goto('http://example.com');

    const result = await page.evaluate(() => {
      // Clear any existing data
      localStorage.clear();

      // Test set and get
      localStorage.setItem('testKey', 'testValue');
      const getValue = localStorage.getItem('testKey');

      // Test length
      const length = localStorage.length;

      // Test key()
      const key = localStorage.key(0);

      // Test remove
      localStorage.removeItem('testKey');
      const removedValue = localStorage.getItem('testKey');

      return {
        getValue,
        length,
        key,
        removedValue: removedValue === null
      };
    });

    expect(result.getValue).toBe('testValue');
    expect(result.length).toBe(1);
    expect(result.key).toBe('testKey');
    expect(result.removedValue).toBe(true);
  });

  test('JS-API-002: SessionStorage - using real URL', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://example.com');

    const result = await page.evaluate(() => {
      sessionStorage.clear();
      sessionStorage.setItem('session', 'data');
      const value = sessionStorage.getItem('session');
      return { value };
    });

    expect(result.value).toBe('data');
  });

  test('JS-API-009: Window.history (pushState, replaceState) - using real URL', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://example.com');

    const result = await page.evaluate(() => {
      // Test pushState
      window.history.pushState({ page: 1 }, 'Title 1', '?page=1');
      const url1 = window.location.href;

      // Test replaceState
      window.history.replaceState({ page: 2 }, 'Title 2', '?page=2');
      const url2 = window.location.href;

      return { url1, url2 };
    });

    expect(result.url1).toContain('?page=1');
    expect(result.url2).toContain('?page=2');
  });
});
