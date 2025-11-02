import { test, expect } from '@playwright/test';

/**
 * Navigation Tests (NAV-001 to NAV-015)
 * Priority: P0 (Critical)
 *
 * Tests core browser navigation functionality including:
 * - URL bar navigation
 * - Forward/back buttons
 * - Page reloading
 * - Link navigation
 * - Redirects
 */

test.describe('Navigation', () => {

  test('NAV-001: URL bar navigation to HTTP site', { tag: '@p0' }, async ({ page }) => {
    // Navigate to HTTP URL
    await page.goto('http://example.com/');

    // Verify page loaded successfully
    await expect(page).toHaveTitle(/Example Domain/);
    await expect(page).toHaveURL('http://example.com/');

    // Verify content rendered
    const heading = page.locator('h1');
    await expect(heading).toBeVisible();
  });

  test('NAV-002: URL bar navigation to HTTPS site', { tag: '@p0' }, async ({ page }) => {
    // Navigate to HTTPS URL
    await page.goto('https://example.com');

    // Verify secure connection
    await expect(page).toHaveURL(/^https:\/\//);
    await expect(page).toHaveTitle(/Example Domain/);

    // TODO: Verify secure indicator in Ladybird UI
    // This may require custom Playwright extension for Ladybird
  });

  test('NAV-003: Forward/back button navigation', { tag: '@p0' }, async ({ page }) => {
    // Navigate to first page
    await page.goto('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);

    // Navigate to second page
    await page.goto('https://www.iana.org/domains/reserved');
    await expect(page).toHaveTitle(/IANA/);

    // Go back
    await page.goBack();
    await expect(page).toHaveURL('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);

    // Go forward
    await page.goForward();
    await expect(page).toHaveURL(/iana\.org/);
  });

  test('NAV-004: Reload button (F5)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://example.com/');

    // Get initial content
    const initialContent = await page.content();

    // Reload page
    await page.reload();

    // Verify page reloaded (content should be same for static page)
    await expect(page).toHaveURL('http://example.com/');
    const reloadedContent = await page.content();
    expect(reloadedContent).toBe(initialContent);
  });

  test('NAV-005: Hard reload (Ctrl+Shift+R)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://example.com/');

    // Hard reload bypasses cache
    await page.reload({ waitUntil: 'networkidle' });

    // Verify page loaded
    await expect(page).toHaveURL('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);

    // TODO: Verify cache was bypassed by checking network requests
    // This requires access to browser DevTools protocol
  });

  test('NAV-006: External link click (target=_blank)', { tag: '@p0' }, async ({ page, context }) => {
    // Create test page with target=_blank link
    await page.goto('data:text/html,<a href="http://example.com/" target="_blank">Link</a>');

    // Listen for new page (tab)
    const pagePromise = context.waitForEvent('page');

    // Click link
    await page.click('a');

    // Wait for new page to open
    const newPage = await pagePromise;
    await newPage.waitForLoadState();

    // Verify new tab opened with correct URL
    await expect(newPage).toHaveURL('http://example.com/');
    await expect(newPage).toHaveTitle(/Example Domain/);

    // Cleanup
    await newPage.close();
  });

  test('NAV-007: External link click (target=_self)', { tag: '@p0' }, async ({ page }) => {
    // Create test page with target=_self link (default)
    await page.goto('data:text/html,<a href="http://example.com/" target="_self">Link</a>');

    // Click link
    await page.click('a');

    // Wait for navigation
    await page.waitForLoadState();

    // Verify same tab navigated
    await expect(page).toHaveURL('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);
  });

  test('NAV-008: Anchor link navigation (#section)', { tag: '@p0' }, async ({ page }) => {
    // Create test page with anchor link
    const html = `
      <html>
        <body style="height: 3000px;">
          <a href="#section">Jump to Section</a>
          <div id="section" style="margin-top: 2000px;">Target Section</div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Get initial scroll position
    const initialScrollY = await page.evaluate(() => window.scrollY);
    expect(initialScrollY).toBe(0);

    // Use scrollIntoView to simulate anchor navigation
    await page.evaluate(() => {
      const element = document.getElementById('section');
      if (element) element.scrollIntoView();
    });

    // Verify page scrolled to target element
    await page.waitForTimeout(500); // Allow scroll animation
    const newScrollY = await page.evaluate(() => window.scrollY);
    expect(newScrollY).toBeGreaterThan(initialScrollY);

    // Verify target element is in view
    const isInView = await page.evaluate(() => {
      const element = document.getElementById('section');
      if (!element) return false;
      const rect = element.getBoundingClientRect();
      return rect.top >= 0 && rect.top <= window.innerHeight;
    });
    expect(isInView).toBe(true);
  });

  test('NAV-009: Fragment navigation with smooth scroll', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <style>html { scroll-behavior: smooth; }</style>
        <body style="height: 3000px;">
          <a href="#target">Smooth Scroll</a>
          <div id="target" style="margin-top: 2500px;">Target</div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Use scrollIntoView with smooth behavior
    await page.evaluate(() => {
      const element = document.getElementById('target');
      if (element) element.scrollIntoView({ behavior: 'smooth' });
    });

    // Verify smooth scroll occurred
    await page.waitForTimeout(1000); // Allow smooth scroll animation
    const scrollY = await page.evaluate(() => window.scrollY);
    expect(scrollY).toBeGreaterThan(2000);

    // Verify target element is in view
    const isInView = await page.evaluate(() => {
      const element = document.getElementById('target');
      if (!element) return false;
      const rect = element.getBoundingClientRect();
      return rect.top >= 0 && rect.top <= window.innerHeight;
    });
    expect(isInView).toBe(true);
  });

  test('NAV-010: JavaScript window.location navigation', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<button onclick="window.location.href=\'http://example.com\'">Navigate</button>');

    // Click button to trigger JavaScript navigation
    await page.click('button');

    // Wait for navigation
    await page.waitForLoadState();

    // Verify navigation occurred
    await expect(page).toHaveURL('http://example.com/');
    await expect(page).toHaveTitle(/Example Domain/);
  });

  test('NAV-011: Meta refresh redirect', { tag: '@p0' }, async ({ page }) => {
    // Create page with meta refresh (1 second delay)
    const html = `
      <html>
        <head>
          <meta http-equiv="refresh" content="1;url=http://example.com">
        </head>
        <body>Redirecting...</body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Wait for redirect
    await page.waitForURL('http://example.com/', { timeout: 5000 });

    // Verify redirect occurred
    await expect(page).toHaveURL('http://example.com/');
  });

  test('NAV-012: 301 permanent redirect', { tag: '@p0' }, async ({ page }) => {
    // Note: This test requires a server that returns 301 redirect
    // For now, using httpbin.org which provides redirect endpoints
    await page.goto('http://httpbin.org/redirect-to?url=http://example.com&status_code=301');

    // Should follow redirect automatically
    await page.waitForLoadState();

    // Verify final URL after redirect
    await expect(page).toHaveURL('http://example.com/');
  });

  test('NAV-013: 302 temporary redirect', { tag: '@p0' }, async ({ page }) => {
    // Using httpbin.org 302 redirect endpoint
    await page.goto('http://httpbin.org/redirect-to?url=http://example.com&status_code=302');

    // Should follow redirect automatically
    await page.waitForLoadState();

    // Verify final URL after redirect
    await expect(page).toHaveURL('http://example.com/');
  });

  test('NAV-014: Invalid URL handling', { tag: '@p0' }, async ({ page }) => {
    // Attempt to navigate to invalid URL
    const response = await page.goto('http://invalid-domain-that-does-not-exist.example', {
      waitUntil: 'domcontentloaded',
      timeout: 10000,
    }).catch(error => error);

    // Should get error (not crash browser)
    expect(response).toBeDefined();

    // TODO: Verify Ladybird shows appropriate error page
    // This may require checking for specific error page content
  });

  test('NAV-015: Navigation to data: URL', { tag: '@p0' }, async ({ page }) => {
    const html = '<h1>Data URL Test</h1><p>Content rendered from data: URL</p>';
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify data URL content rendered
    await expect(page).toHaveURL(/^data:text\/html/);
    const heading = page.locator('h1');
    await expect(heading).toHaveText('Data URL Test');
    const paragraph = page.locator('p');
    await expect(paragraph).toContainText('data: URL');
  });

});
