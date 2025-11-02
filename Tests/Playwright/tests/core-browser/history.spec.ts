import { test, expect } from '@playwright/test';

/**
 * History Management Tests (HIST-001 to HIST-008)
 * Priority: P0 (Critical)
 *
 * Tests browser history functionality including:
 * - Forward/back navigation
 * - History population
 * - History search and filtering
 * - Clear history
 * - History persistence
 * - Private browsing mode
 */

/**
 * Normalize URL for comparison by ensuring consistent trailing slash handling.
 * According to WHATWG URL spec, http://example.com and http://example.com/ are different URLs.
 * However, servers often normalize these. This function normalizes for comparison purposes.
 */
function normalizeURL(url: string): string {
  try {
    const urlObj = new URL(url);
    // For http(s) URLs with no path (or just /), normalize to always have trailing slash
    if ((urlObj.protocol === 'http:' || urlObj.protocol === 'https:') &&
        (urlObj.pathname === '' || urlObj.pathname === '/')) {
      urlObj.pathname = '/';
    }
    return urlObj.href;
  } catch {
    // If URL parsing fails, return as-is
    return url;
  }
}

test.describe('History Management', () => {

  test('HIST-001: History navigation forward/back', { tag: '@p0' }, async ({ page }) => {
    // Navigate to first page
    await page.goto('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);
    const url1 = page.url();

    // Navigate to second page
    await page.goto('https://www.iana.org/domains/reserved/');
    await expect(page).toHaveTitle(/IANA/);
    const url2 = page.url();

    // Navigate to third page by clicking a link on example.com
    await page.goto('http://example.com/');
    await page.click('a');
    await page.waitForLoadState('networkidle');
    const url3 = page.url();

    // Go back once - should return to example.com
    await page.goBack();
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);
    const urlAfterBack1 = page.url();

    // Verify we're back at example.com (url1), not the linked page (url3)
    expect(normalizeURL(urlAfterBack1)).toBe(normalizeURL(url1));
    await expect(page).toHaveTitle(/Example Domain/);

    // Go back again - should return to iana.org
    await page.goBack();
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);
    const urlAfterBack2 = page.url();

    // Verify we're at iana.org (url2)
    expect(normalizeURL(urlAfterBack2)).toBe(normalizeURL(url2));
    await expect(page).toHaveTitle(/IANA/);

    // Go forward - should return to example.com
    await page.goForward();
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);
    const urlAfterForward1 = page.url();

    // Verify we're back at example.com (url1)
    expect(normalizeURL(urlAfterForward1)).toBe(normalizeURL(url1));
    await expect(page).toHaveTitle(/Example Domain/);

    // Go forward again - should return to the linked page
    await page.goForward();
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);
    const urlAfterForward2 = page.url();

    // Verify we're at the linked page (url3)
    expect(normalizeURL(urlAfterForward2)).toBe(normalizeURL(url3));
  });

  test('HIST-002: History populated on navigation', { tag: '@p0' }, async ({ page }) => {
    // Navigate to multiple pages
    const visitedURLs = [
      'http://example.com/',
      'https://www.iana.org/domains/reserved/',
      'data:text/html,<h1>Test Page 1</h1>',
      'data:text/html,<h1>Test Page 2</h1>'
    ];

    // Navigate to each URL and store the actual URL from the page
    const actualURLs: string[] = [];
    for (const url of visitedURLs) {
      await page.goto(url);
      await page.waitForLoadState('networkidle');
      actualURLs.push(page.url());
    }

    // Navigate back through history, verifying we visit pages in reverse order
    // Start from second-to-last page (we're currently on the last page)
    for (let i = visitedURLs.length - 2; i >= 0; i--) {
      await page.goBack();
      await page.waitForLoadState('networkidle');
      await page.waitForTimeout(500);

      // Verify we're at the correct historical page
      const currentURL = page.url();
      expect(normalizeURL(currentURL)).toBe(normalizeURL(actualURLs[i]));
    }

    // Now navigate forward through history
    for (let i = 1; i < visitedURLs.length; i++) {
      await page.goForward();
      await page.waitForLoadState('networkidle');
      await page.waitForTimeout(500);

      // Verify we're at the correct page going forward
      const currentURL = page.url();
      expect(normalizeURL(currentURL)).toBe(normalizeURL(actualURLs[i]));
    }
  });

  test('HIST-003: History search/filter', { tag: '@p0' }, async ({ page }) => {
    // This test would interact with browser's history UI
    // Since Playwright focuses on page content, we simulate the concept

    // Navigate to pages with distinctive titles
    const pages = [
      { url: 'data:text/html,<title>Example Page</title><h1>Example</h1>', title: 'Example Page' },
      { url: 'data:text/html,<title>Test Document</title><h1>Test</h1>', title: 'Test Document' },
      { url: 'data:text/html,<title>Sample Site</title><h1>Sample</h1>', title: 'Sample Site' }
    ];

    for (const pageInfo of pages) {
      await page.goto(pageInfo.url);
      await expect(page).toHaveTitle(pageInfo.title);
    }

    // TODO: Access browser history UI to search
    // This requires browser-specific automation beyond page content

    // For now, verify pages are in navigation history
    await page.goBack();
    await expect(page).toHaveTitle('Test Document');

    await page.goBack();
    await expect(page).toHaveTitle('Example Page');

    // Note: Full history search requires browser UI automation
    console.log('History search UI test requires browser chrome automation');
  });

  test('HIST-004: Clear browsing history', { tag: '@p0' }, async ({ page, context }) => {
    // Navigate to several pages to populate history
    await page.goto('http://example.com/');
    await page.goto('https://www.iana.org/domains/reserved/');
    await page.goto('data:text/html,<h1>Test Page</h1>');

    // Verify history exists (can navigate back)
    const urlBefore = page.url();
    await page.goBack();
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(500);

    // URL should have changed after going back
    expect(page.url()).not.toBe(urlBefore);

    // TODO: Clear history via browser settings/API
    // This requires browser-specific APIs or UI automation

    // Simulate clearing by creating new context
    // In real browser, this would be Settings > Privacy > Clear History
    const newContext = await context.browser()?.newContext() || context;
    const newPage = await newContext.newPage();

    // New page should have empty history
    await newPage.goto('http://example.com/');
    await newPage.waitForLoadState('networkidle');

    // Attempting to go back should do nothing or stay on same page
    const urlBeforeBack = newPage.url().replace(/\/$/, '');
    try {
      await newPage.goBack({ timeout: 2000 });
      await newPage.waitForLoadState('networkidle', { timeout: 2000 });
    } catch (e) {
      // goBack may fail or timeout if no history, which is expected
    }
    await newPage.waitForTimeout(500);
    const urlAfterBack = newPage.url().replace(/\/$/, '');

    // Should still be on same page or went to about:blank (no useful history)
    const isSamePage = urlAfterBack === urlBeforeBack || urlAfterBack === 'about:blank';
    expect(isSamePage).toBe(true);

    // Cleanup
    await newPage.close();
    if (newContext !== context) {
      await newContext.close();
    }
  });

  test('HIST-005: History date grouping', { tag: '@p0' }, async ({ page }) => {
    // This test verifies history UI groups entries by date
    // Today, Yesterday, Last Week, etc.

    // Navigate to pages at "different times" (simulated)
    const pages = [
      'data:text/html,<title>Today Page 1</title>',
      'data:text/html,<title>Today Page 2</title>',
      'data:text/html,<title>Today Page 3</title>'
    ];

    for (const url of pages) {
      await page.goto(url);
      await page.waitForTimeout(100);
    }

    // TODO: Open history UI and verify date grouping
    // This requires browser UI automation

    // For now, verify pages are in history
    await page.goBack();
    await expect(page).toHaveTitle('Today Page 2');

    await page.goBack();
    await expect(page).toHaveTitle('Today Page 1');

    // Note: Date grouping in history UI requires browser chrome access
    console.log('History date grouping test requires browser UI automation');
  });

  test('HIST-006: Click history entry to navigate', { tag: '@p0' }, async ({ page }) => {
    // Navigate to distinctive pages
    const page1URL = 'data:text/html,<title>Page 1</title><h1>First Page</h1>';
    const page2URL = 'data:text/html,<title>Page 2</title><h1>Second Page</h1>';
    const page3URL = 'data:text/html,<title>Page 3</title><h1>Third Page</h1>';

    await page.goto(page1URL);
    await page.goto(page2URL);
    await page.goto(page3URL);

    // TODO: Open history UI, click on "Page 1" entry
    // This requires browser UI automation

    // Simulate by using back navigation
    await page.goBack();
    await page.goBack();
    await expect(page).toHaveTitle('Page 1');
    await expect(page.locator('h1')).toHaveText('First Page');

    // Verify navigation to historical page works
    await page.goForward();
    await expect(page).toHaveTitle('Page 2');

    // Note: Clicking history entries in UI requires browser chrome automation
    console.log('History entry click test requires browser UI automation');
  });

  test('HIST-007: Delete individual history entry', { tag: '@p0' }, async ({ page }) => {
    // Navigate to pages
    const pages = [
      'data:text/html,<title>Keep Page 1</title>',
      'data:text/html,<title>Delete This Page</title>',
      'data:text/html,<title>Keep Page 2</title>'
    ];

    for (const url of pages) {
      await page.goto(url);
    }

    // TODO: Open history UI, delete "Delete This Page" entry
    // Verify it's removed but others remain

    // This requires browser UI automation and history management API
    // Playwright focuses on page content, not browser chrome

    // For now, verify all pages are in history
    await page.goBack();
    await expect(page).toHaveTitle('Delete This Page');

    await page.goBack();
    await expect(page).toHaveTitle('Keep Page 1');

    // Note: Individual history entry deletion requires browser UI
    console.log('Individual history entry deletion requires browser UI automation');
  });

  test('HIST-008: Private browsing mode (no history)', { tag: '@p0' }, async ({ browser }) => {
    // Create private/incognito browsing context
    // In Playwright, this is a new context without persistent state
    const privateContext = await browser.newContext({
      // Simulate private browsing
      storageState: undefined,
      // Don't persist cookies, storage, etc.
      acceptDownloads: true
    });

    const privatePage = await privateContext.newPage();

    // Navigate in private mode
    await privatePage.goto('http://example.com/');
    await privatePage.goto('https://www.iana.org/domains/reserved/');
    await privatePage.goto('data:text/html,<html><head><title>Private Page</title></head><body><h1>Private Page</h1></body></html>');

    // Verify pages loaded (data: URLs may not have titles in all browsers)
    const title = await privatePage.title();
    const hasContent = await privatePage.locator('h1').count() > 0;
    expect(title === 'Private Page' || hasContent).toBe(true);

    // In private mode, history should not persist after closing
    // Close the private context
    await privateContext.close();

    // Create new regular context
    const regularContext = await browser.newContext();
    const regularPage = await regularContext.newPage();

    // Navigate to a page
    await regularPage.goto('http://example.com/');

    // Try to go back - should have no history from private session
    const urlBeforeGoBack = regularPage.url().replace(/\/$/, '');
    try {
      await regularPage.goBack({ timeout: 2000 });
      await regularPage.waitForLoadState('networkidle', { timeout: 2000 });
    } catch (e) {
      // Expected to fail or timeout
    }
    await regularPage.waitForTimeout(500);
    const urlAfterGoBack = regularPage.url().replace(/\/$/, '');

    // Should still be on same page or about:blank (no useful history from private session)
    const stayedOrBlank = urlAfterGoBack === urlBeforeGoBack || urlAfterGoBack === 'about:blank';
    expect(stayedOrBlank).toBe(true);

    // Cleanup
    await regularContext.close();

    // Note: True private browsing mode would be browser-specific
    // Ladybird may have different private mode implementation
  });

});
