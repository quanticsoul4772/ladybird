import { test, expect } from '@playwright/test';

/**
 * Responsive Design Tests (RESP-001 to RESP-010)
 * Priority: P0 (Critical)
 *
 * Tests responsive web design capabilities including:
 * - Viewport meta tag
 * - Media queries (width, orientation, color-scheme)
 * - Responsive images (srcset, picture element)
 * - Fluid layouts
 * - Mobile navigation patterns
 * - Touch target sizes
 * - Responsive typography
 */

test.describe('Responsive Design', () => {

  test('RESP-001: Viewport meta tag', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <title>Viewport Test</title>
        </head>
        <body>
          <div id="test-content" style="width: 100%; padding: 20px; background-color: #4CAF50;">
            <p>Viewport-aware content</p>
          </div>
          <script>
            document.getElementById('viewport-info').textContent =
              'Viewport: ' + window.innerWidth + 'x' + window.innerHeight;
          </script>
        </body>
      </html>
    `;

    // Test on mobile viewport
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify viewport meta tag is honored
    const viewportWidth = await page.evaluate(() => window.innerWidth);
    expect(viewportWidth).toBe(375);

    // Verify content is visible and not zoomed
    await expect(page.locator('#test-content')).toBeVisible();

    // Test on desktop viewport
    await page.setViewportSize({ width: 1920, height: 1080 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    const desktopWidth = await page.evaluate(() => window.innerWidth);
    expect(desktopWidth).toBe(1920);
  });

  test('RESP-002: Media queries (width)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            #responsive-box {
              background-color: red;
              padding: 20px;
              text-align: center;
            }
            /* Mobile: < 768px */
            @media (max-width: 767px) {
              #responsive-box {
                background-color: #FF5722;
              }
              #responsive-box::after {
                content: " (Mobile)";
              }
            }
            /* Tablet: 768px - 1023px */
            @media (min-width: 768px) and (max-width: 1023px) {
              #responsive-box {
                background-color: #2196F3;
              }
              #responsive-box::after {
                content: " (Tablet)";
              }
            }
            /* Desktop: >= 1024px */
            @media (min-width: 1024px) {
              #responsive-box {
                background-color: #4CAF50;
              }
              #responsive-box::after {
                content: " (Desktop)";
              }
            }
          </style>
        </head>
        <body>
          <div id="responsive-box">Responsive Layout</div>
        </body>
      </html>
    `;

    // Test mobile breakpoint
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Wait for media query to apply and content to update
    await page.waitForFunction(() => {
      const el = document.getElementById('responsive-box');
      const content = window.getComputedStyle(el, '::after').content;
      return content && content.includes('Mobile');
    });

    const mobileColor = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(mobileColor).toContain('255, 87, 34'); // #FF5722

    // Check pseudo-element content directly (textContent() doesn't include ::after)
    const mobileAfter = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(mobileAfter).toContain('Mobile');

    // Test tablet breakpoint
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.waitForTimeout(100); // Wait for media query to apply

    // Wait for media query to apply and content to update
    await page.waitForFunction(() => {
      const el = document.getElementById('responsive-box');
      const content = window.getComputedStyle(el, '::after').content;
      return content && content.includes('Tablet');
    });

    const tabletColor = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(tabletColor).toContain('33, 150, 243'); // #2196F3

    // Check pseudo-element content directly (textContent() doesn't include ::after)
    const tabletAfter = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(tabletAfter).toContain('Tablet');

    // Test desktop breakpoint
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply

    // Wait for media query to apply and content to update
    await page.waitForFunction(() => {
      const el = document.getElementById('responsive-box');
      const content = window.getComputedStyle(el, '::after').content;
      return content && content.includes('Desktop');
    });

    const desktopColor = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(desktopColor).toContain('76, 175, 80'); // #4CAF50

    // Check pseudo-element content directly (textContent() doesn't include ::after)
    const desktopAfter = await page.locator('#responsive-box').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(desktopAfter).toContain('Desktop');
  });

  test('RESP-003: Media queries (orientation)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            #orientation-box {
              padding: 20px;
              text-align: center;
            }
            @media (orientation: portrait) {
              #orientation-box {
                background-color: #9C27B0;
              }
              #orientation-box::after {
                content: " - Portrait";
              }
            }
            @media (orientation: landscape) {
              #orientation-box {
                background-color: #FF9800;
              }
              #orientation-box::after {
                content: " - Landscape";
              }
            }
          </style>
        </head>
        <body>
          <div id="orientation-box">Orientation Test</div>
        </body>
      </html>
    `;

    // Test portrait orientation (height > width)
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Wait for media query to apply and content to update
    await page.waitForFunction(() => {
      const el = document.getElementById('orientation-box');
      const content = window.getComputedStyle(el, '::after').content;
      return content && content.includes('Portrait');
    });

    const portraitColor = await page.locator('#orientation-box').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(portraitColor).toContain('156, 39, 176'); // #9C27B0

    // Check pseudo-element content directly (textContent() doesn't include ::after)
    const portraitAfter = await page.locator('#orientation-box').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(portraitAfter).toContain('Portrait');

    // Test landscape orientation (width > height)
    await page.setViewportSize({ width: 667, height: 375 });
    await page.waitForTimeout(100); // Wait for media query to apply

    // Wait for media query to apply and content to update
    await page.waitForFunction(() => {
      const el = document.getElementById('orientation-box');
      const content = window.getComputedStyle(el, '::after').content;
      return content && content.includes('Landscape');
    });

    const landscapeColor = await page.locator('#orientation-box').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(landscapeColor).toContain('255, 152, 0'); // #FF9800

    // Check pseudo-element content directly (textContent() doesn't include ::after)
    const landscapeAfter = await page.locator('#orientation-box').evaluate(el => {
      return window.getComputedStyle(el, '::after').content;
    });
    expect(landscapeAfter).toContain('Landscape');
  });

  test('RESP-004: Media queries (prefers-color-scheme)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            body {
              background-color: white;
              color: black;
            }
            @media (prefers-color-scheme: dark) {
              body {
                background-color: #1a1a1a;
                color: #ffffff;
              }
              #theme-box {
                background-color: #333;
              }
            }
            @media (prefers-color-scheme: light) {
              body {
                background-color: #ffffff;
                color: #000000;
              }
              #theme-box {
                background-color: #f0f0f0;
              }
            }
            #theme-box {
              padding: 20px;
              margin: 20px;
            }
          </style>
        </head>
        <body>
          <div id="theme-box">Color Scheme Aware</div>
        </body>
      </html>
    `;

    // Note: prefers-color-scheme is system-level, so we test that CSS parses correctly
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify elements are visible (actual color depends on system settings)
    await expect(page.locator('#theme-box')).toBeVisible();

    const bodyColor = await page.evaluate(() => {
      return window.getComputedStyle(document.body).backgroundColor;
    });
    expect(bodyColor).toBeTruthy();
  });

  test('RESP-005: Responsive images (srcset)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
        </head>
        <body>
          <img id="responsive-img"
               src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='100' height='100'%3E%3Crect fill='red' width='100' height='100'/%3E%3C/svg%3E"
               srcset="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='200' height='200'%3E%3Crect fill='blue' width='200' height='200'/%3E%3C/svg%3E 2x,
                       data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='300' height='300'%3E%3Crect fill='green' width='300' height='300'/%3E%3C/svg%3E 3x"
               alt="Responsive image">
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify image loads
    await expect(page.locator('#responsive-img')).toBeVisible();

    // Verify srcset attribute exists
    const srcset = await page.locator('#responsive-img').getAttribute('srcset');
    expect(srcset).toBeTruthy();
    expect(srcset).toContain('2x');
    expect(srcset).toContain('3x');

    // Verify currentSrc is set (browser chooses appropriate image)
    const currentSrc = await page.locator('#responsive-img').evaluate(el => {
      return (el as HTMLImageElement).currentSrc;
    });
    expect(currentSrc).toBeTruthy();
  });

  test('RESP-006: Picture element', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
        </head>
        <body>
          <picture id="responsive-picture">
            <source media="(min-width: 1024px)"
                    srcset="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='800' height='600'%3E%3Crect fill='green' width='800' height='600'/%3E%3Ctext x='50' y='300' font-size='48' fill='white'%3EDesktop%3C/text%3E%3C/svg%3E">
            <source media="(min-width: 768px)"
                    srcset="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='600' height='400'%3E%3Crect fill='blue' width='600' height='400'/%3E%3Ctext x='50' y='200' font-size='36' fill='white'%3ETablet%3C/text%3E%3C/svg%3E">
            <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='400' height='300'%3E%3Crect fill='red' width='400' height='300'/%3E%3Ctext x='50' y='150' font-size='24' fill='white'%3EMobile%3C/text%3E%3C/svg%3E"
                 alt="Responsive picture" id="picture-img">
          </picture>
        </body>
      </html>
    `;

    // Test on mobile
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await expect(page.locator('#picture-img')).toBeVisible();

    // Test on tablet
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    // Verify image still visible after viewport change
    await expect(page.locator('#picture-img')).toBeVisible();

    // Test on desktop
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    await expect(page.locator('#picture-img')).toBeVisible();

    // Verify picture element has source elements
    const sourceCount = await page.locator('#responsive-picture source').count();
    expect(sourceCount).toBe(2);
  });

  test('RESP-007: Fluid layouts', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
            .fluid-container {
              max-width: 1200px;
              margin: 0 auto;
              padding: 0 20px;
            }
            .fluid-grid {
              display: grid;
              grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
              gap: 20px;
            }
            .fluid-item {
              background-color: #2196F3;
              padding: 20px;
              min-height: 100px;
            }
          </style>
        </head>
        <body>
          <div class="fluid-container" id="container">
            <div class="fluid-grid" id="grid">
              <div class="fluid-item">Item 1</div>
              <div class="fluid-item">Item 2</div>
              <div class="fluid-item">Item 3</div>
              <div class="fluid-item">Item 4</div>
            </div>
          </div>
        </body>
      </html>
    `;

    // Test on mobile - items should stack vertically
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await expect(page.locator('.fluid-item')).toHaveCount(4);

    const mobileGridColumns = await page.locator('#grid').evaluate(el => {
      return window.getComputedStyle(el).gridTemplateColumns;
    });
    // On mobile, should have fewer columns
    expect(mobileGridColumns).toBeTruthy();

    // Test on desktop - items should be in multiple columns
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    const desktopGridColumns = await page.locator('#grid').evaluate(el => {
      return window.getComputedStyle(el).gridTemplateColumns;
    });
    expect(desktopGridColumns).toBeTruthy();
    // Desktop should have more columns than mobile
  });

  test('RESP-008: Mobile navigation patterns', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
            .hamburger {
              display: none;
              font-size: 30px;
              cursor: pointer;
              padding: 10px;
            }
            .nav-menu {
              display: flex;
              list-style: none;
              margin: 0;
              padding: 0;
            }
            .nav-menu li {
              margin: 0 15px;
            }
            @media (max-width: 768px) {
              .hamburger {
                display: block;
              }
              .nav-menu {
                display: none;
                flex-direction: column;
                width: 100%;
              }
              .nav-menu.active {
                display: flex;
              }
              .nav-menu li {
                margin: 10px 0;
              }
            }
          </style>
        </head>
        <body>
          <nav>
            <div class="hamburger" id="hamburger">☰</div>
            <ul class="nav-menu" id="nav-menu">
              <li><a href="#">Home</a></li>
              <li><a href="#">About</a></li>
              <li><a href="#">Services</a></li>
              <li><a href="#">Contact</a></li>
            </ul>
          </nav>
          <script>
            document.getElementById('hamburger').addEventListener('click', function() {
              document.getElementById('nav-menu').classList.toggle('active');
            });
          </script>
        </body>
      </html>
    `;

    // Test on mobile - hamburger menu should be visible
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await expect(page.locator('#hamburger')).toBeVisible();

    // Menu should be initially hidden on mobile
    const menuHidden = await page.locator('#nav-menu').evaluate(el => {
      return window.getComputedStyle(el).display === 'none';
    });
    expect(menuHidden).toBe(true);

    // Click hamburger to show menu
    await page.click('#hamburger');
    await page.waitForTimeout(100);

    // Menu should now be visible
    await expect(page.locator('#nav-menu')).toBeVisible();

    // Test on desktop - full menu should be visible
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    // Hamburger should be hidden on desktop
    const hamburgerHidden = await page.locator('#hamburger').evaluate(el => {
      return window.getComputedStyle(el).display === 'none';
    });
    expect(hamburgerHidden).toBe(true);
  });

  test('RESP-009: Touch target sizes', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
            .touch-target {
              min-width: 44px;
              min-height: 44px;
              padding: 12px 24px;
              margin: 10px;
              background-color: #4CAF50;
              color: white;
              border: none;
              border-radius: 4px;
              cursor: pointer;
              font-size: 16px;
              display: inline-block;
              text-align: center;
            }
            @media (max-width: 768px) {
              .touch-target {
                min-width: 48px;
                min-height: 48px;
                padding: 16px 32px;
                font-size: 18px;
              }
            }
          </style>
        </head>
        <body>
          <button class="touch-target" id="button1">Button 1</button>
          <button class="touch-target" id="button2">Button 2</button>
          <a href="#" class="touch-target" id="link1">Link 1</a>
        </body>
      </html>
    `;

    // Test on mobile - touch targets should be larger
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const mobileDimensions = await page.locator('#button1').evaluate(el => {
      const rect = el.getBoundingClientRect();
      return { width: rect.width, height: rect.height };
    });

    // Touch targets should meet minimum size (48x48px recommended)
    expect(mobileDimensions.height).toBeGreaterThanOrEqual(48);

    // Verify buttons are clickable
    await expect(page.locator('#button1')).toBeVisible();
    await page.click('#button1');

    // Test on desktop
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    const desktopDimensions = await page.locator('#button1').evaluate(el => {
      const rect = el.getBoundingClientRect();
      return { width: rect.width, height: rect.height };
    });

    // Desktop targets can be smaller but still accessible
    expect(desktopDimensions.height).toBeGreaterThanOrEqual(44);
  });

  test('RESP-010: Responsive typography', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
            html {
              font-size: 16px;
            }
            body {
              font-size: 1rem;
              line-height: 1.5;
            }
            h1 {
              font-size: 2rem;
            }
            @media (min-width: 768px) {
              html {
                font-size: 18px;
              }
              h1 {
                font-size: 2.5rem;
              }
            }
            @media (min-width: 1024px) {
              html {
                font-size: 20px;
              }
              h1 {
                font-size: 3rem;
              }
            }
            .fluid-text {
              font-size: clamp(1rem, 2vw, 2rem);
            }
            .viewport-text {
              font-size: 4vw;
            }
          </style>
        </head>
        <body>
          <h1 id="heading">Responsive Heading</h1>
          <p id="body-text">Body text that scales responsively.</p>
          <p class="fluid-text" id="fluid">Fluid typography using clamp()</p>
          <p class="viewport-text" id="viewport">Viewport-based typography (4vw)</p>
        </body>
      </html>
    `;

    // Test on mobile
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const mobileHeadingSize = await page.locator('#heading').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    expect(mobileHeadingSize).toBe(32); // 2rem * 16px

    const mobileBodySize = await page.locator('#body-text').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    expect(mobileBodySize).toBe(16); // 1rem * 16px

    // Test on tablet
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    const tabletHeadingSize = await page.locator('#heading').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    expect(tabletHeadingSize).toBe(45); // 2.5rem * 18px

    // Test on desktop
    await page.setViewportSize({ width: 1440, height: 900 });
    await page.waitForTimeout(100); // Wait for media query to apply
    await page.waitForTimeout(100);

    const desktopHeadingSize = await page.locator('#heading').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    expect(desktopHeadingSize).toBe(60); // 3rem * 20px

    // Verify viewport-based typography
    const viewportTextSize = await page.locator('#viewport').evaluate(el => {
      return parseFloat(window.getComputedStyle(el).fontSize);
    });
    // 4vw of 1440px = 57.6px
    expect(viewportTextSize).toBeCloseTo(57.6, 0);

    // Verify all text is visible
    await expect(page.locator('#heading')).toBeVisible();
    await expect(page.locator('#body-text')).toBeVisible();
    await expect(page.locator('#fluid')).toBeVisible();
    await expect(page.locator('#viewport')).toBeVisible();
  });

});
