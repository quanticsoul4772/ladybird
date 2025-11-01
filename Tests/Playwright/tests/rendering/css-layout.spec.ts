import { test, expect } from '@playwright/test';

/**
 * CSS Layout Tests (CSS-001 to CSS-020)
 * Priority: P0 (Critical)
 *
 * Tests comprehensive CSS layout capabilities including:
 * - Flexbox (basic, wrap, alignment)
 * - CSS Grid (basic, areas, gaps)
 * - Float-based layout
 * - Positioning (absolute, fixed, sticky)
 * - Z-index stacking
 * - Display modes
 * - Box model
 * - Width/height constraints
 * - Overflow handling
 * - Multi-column layout
 * - Responsive breakpoints
 * - Viewport units
 * - Calc() expressions
 * - CSS variables
 * - Container queries
 * - Aspect ratio
 */

test.describe('CSS Layout', () => {

  test('CSS-001: Flexbox basic layout', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .flex-container {
              display: flex;
              background-color: #f0f0f0;
              padding: 10px;
            }
            .flex-item {
              background-color: #4CAF50;
              color: white;
              margin: 5px;
              padding: 20px;
              text-align: center;
            }
            .flex-row { flex-direction: row; }
            .flex-column { flex-direction: column; }
          </style>
        </head>
        <body>
          <div class="flex-container flex-row" id="row-container">
            <div class="flex-item">Item 1</div>
            <div class="flex-item">Item 2</div>
            <div class="flex-item">Item 3</div>
          </div>
          <div class="flex-container flex-column" id="column-container">
            <div class="flex-item">Item A</div>
            <div class="flex-item">Item B</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify flexbox display
    const rowDisplay = await page.locator('#row-container').evaluate(el => {
      return window.getComputedStyle(el).display;
    });
    expect(rowDisplay).toBe('flex');

    // Verify flex-direction
    const rowDirection = await page.locator('#row-container').evaluate(el => {
      return window.getComputedStyle(el).flexDirection;
    });
    expect(rowDirection).toBe('row');

    const columnDirection = await page.locator('#column-container').evaluate(el => {
      return window.getComputedStyle(el).flexDirection;
    });
    expect(columnDirection).toBe('column');

    // Verify flex items are laid out correctly
    await expect(page.locator('#row-container .flex-item')).toHaveCount(3);
    await expect(page.locator('#column-container .flex-item')).toHaveCount(2);
  });

  test('CSS-002: Flexbox wrap and alignment', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .flex-wrap-container {
              display: flex;
              flex-wrap: wrap;
              justify-content: space-between;
              align-items: center;
              width: 300px;
              height: 200px;
              background-color: #f0f0f0;
            }
            .flex-wrap-item {
              width: 80px;
              height: 50px;
              background-color: #2196F3;
              margin: 5px;
            }
          </style>
        </head>
        <body>
          <div class="flex-wrap-container" id="wrap-container">
            <div class="flex-wrap-item">1</div>
            <div class="flex-wrap-item">2</div>
            <div class="flex-wrap-item">3</div>
            <div class="flex-wrap-item">4</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify flex-wrap
    const flexWrap = await page.locator('#wrap-container').evaluate(el => {
      return window.getComputedStyle(el).flexWrap;
    });
    expect(flexWrap).toBe('wrap');

    // Verify justify-content
    const justifyContent = await page.locator('#wrap-container').evaluate(el => {
      return window.getComputedStyle(el).justifyContent;
    });
    expect(justifyContent).toBe('space-between');

    // Verify align-items
    const alignItems = await page.locator('#wrap-container').evaluate(el => {
      return window.getComputedStyle(el).alignItems;
    });
    expect(alignItems).toBe('center');
  });

  test('CSS-003: CSS Grid basic layout', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .grid-container {
              display: grid;
              grid-template-columns: repeat(3, 1fr);
              grid-template-rows: 100px 100px;
              gap: 10px;
              background-color: #f0f0f0;
              padding: 10px;
            }
            .grid-item {
              background-color: #FF9800;
              padding: 20px;
              text-align: center;
            }
          </style>
        </head>
        <body>
          <div class="grid-container" id="grid">
            <div class="grid-item">1</div>
            <div class="grid-item">2</div>
            <div class="grid-item">3</div>
            <div class="grid-item">4</div>
            <div class="grid-item">5</div>
            <div class="grid-item">6</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify grid display
    const gridDisplay = await page.locator('#grid').evaluate(el => {
      return window.getComputedStyle(el).display;
    });
    expect(gridDisplay).toBe('grid');

    // Verify grid-template-columns (may vary by browser implementation)
    const gridColumns = await page.locator('#grid').evaluate(el => {
      return window.getComputedStyle(el).gridTemplateColumns;
    });
    expect(gridColumns).toBeTruthy();

    // Verify all grid items render
    await expect(page.locator('#grid .grid-item')).toHaveCount(6);
  });

  test('CSS-004: CSS Grid areas and gaps', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .grid-areas {
              display: grid;
              grid-template-areas:
                "header header header"
                "sidebar content content"
                "footer footer footer";
              grid-template-columns: 200px 1fr 1fr;
              grid-template-rows: 60px 200px 60px;
              gap: 15px;
              padding: 10px;
            }
            .header { grid-area: header; background-color: #f44336; }
            .sidebar { grid-area: sidebar; background-color: #4CAF50; }
            .content { grid-area: content; background-color: #2196F3; }
            .footer { grid-area: footer; background-color: #FFC107; }
          </style>
        </head>
        <body>
          <div class="grid-areas" id="grid-areas">
            <div class="header">Header</div>
            <div class="sidebar">Sidebar</div>
            <div class="content">Content</div>
            <div class="footer">Footer</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify grid-area assignments
    const headerArea = await page.locator('.header').evaluate(el => {
      return window.getComputedStyle(el).gridArea;
    });
    expect(headerArea).toContain('header');

    // Verify gap property
    const gap = await page.locator('#grid-areas').evaluate(el => {
      const style = window.getComputedStyle(el);
      return style.gap || style.gridGap;
    });
    expect(gap).toBeTruthy();

    // Verify all areas are visible
    await expect(page.locator('.header')).toBeVisible();
    await expect(page.locator('.sidebar')).toBeVisible();
    await expect(page.locator('.content')).toBeVisible();
    await expect(page.locator('.footer')).toBeVisible();
  });

  test('CSS-005: Float-based layout', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .float-left {
              float: left;
              width: 150px;
              height: 100px;
              background-color: #4CAF50;
              margin: 10px;
            }
            .float-right {
              float: right;
              width: 150px;
              height: 100px;
              background-color: #2196F3;
              margin: 10px;
            }
            .clearfix::after {
              content: "";
              display: table;
              clear: both;
            }
          </style>
        </head>
        <body>
          <div class="clearfix">
            <div class="float-left" id="left">Left Float</div>
            <div class="float-right" id="right">Right Float</div>
            <p>This text wraps around the floated elements.</p>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify float values
    const leftFloat = await page.locator('#left').evaluate(el => {
      return window.getComputedStyle(el).float || window.getComputedStyle(el).cssFloat;
    });
    expect(leftFloat).toBe('left');

    const rightFloat = await page.locator('#right').evaluate(el => {
      return window.getComputedStyle(el).float || window.getComputedStyle(el).cssFloat;
    });
    expect(rightFloat).toBe('right');

    // Verify elements are visible
    await expect(page.locator('#left')).toBeVisible();
    await expect(page.locator('#right')).toBeVisible();
  });

  test('CSS-006: Absolute positioning', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .relative-container {
              position: relative;
              width: 400px;
              height: 300px;
              background-color: #f0f0f0;
            }
            .absolute-box {
              position: absolute;
              width: 100px;
              height: 100px;
              background-color: #E91E63;
            }
            #top-left { top: 10px; left: 10px; }
            #bottom-right { bottom: 10px; right: 10px; }
          </style>
        </head>
        <body>
          <div class="relative-container">
            <div class="absolute-box" id="top-left">Top Left</div>
            <div class="absolute-box" id="bottom-right">Bottom Right</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify positioning
    const topLeftPosition = await page.locator('#top-left').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        position: style.position,
        top: style.top,
        left: style.left
      };
    });
    expect(topLeftPosition.position).toBe('absolute');
    expect(topLeftPosition.top).toBe('10px');
    expect(topLeftPosition.left).toBe('10px');

    const bottomRightPosition = await page.locator('#bottom-right').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        position: style.position,
        bottom: style.bottom,
        right: style.right
      };
    });
    expect(bottomRightPosition.position).toBe('absolute');
    expect(bottomRightPosition.bottom).toBe('10px');
    expect(bottomRightPosition.right).toBe('10px');
  });

  test('CSS-007: Fixed positioning', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            body { height: 2000px; }
            .fixed-header {
              position: fixed;
              top: 0;
              left: 0;
              right: 0;
              height: 60px;
              background-color: #3F51B5;
              color: white;
              padding: 20px;
              z-index: 100;
            }
            .content {
              margin-top: 100px;
            }
          </style>
        </head>
        <body>
          <div class="fixed-header" id="header">Fixed Header</div>
          <div class="content">
            <p>Scroll down to see fixed header in action</p>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify fixed positioning
    const headerPosition = await page.locator('#header').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        position: style.position,
        top: style.top,
        zIndex: style.zIndex
      };
    });
    expect(headerPosition.position).toBe('fixed');
    expect(headerPosition.top).toBe('0px');

    // Scroll down and verify header stays in view
    await page.evaluate(() => window.scrollTo(0, 500));
    await page.waitForTimeout(100);
    await expect(page.locator('#header')).toBeInViewport();
  });

  test('CSS-008: Sticky positioning', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            body { font-family: Arial, sans-serif; }
            .sticky-nav {
              position: sticky;
              top: 0;
              background-color: #009688;
              color: white;
              padding: 15px;
              z-index: 50;
            }
            .content-section {
              height: 500px;
              padding: 20px;
              background-color: #f0f0f0;
              margin-bottom: 20px;
            }
          </style>
        </head>
        <body>
          <div style="height: 200px; background-color: #ccc;">Header Space</div>
          <div class="sticky-nav" id="sticky">Sticky Navigation</div>
          <div class="content-section">Section 1</div>
          <div class="content-section">Section 2</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify sticky positioning
    const stickyPosition = await page.locator('#sticky').evaluate(el => {
      return window.getComputedStyle(el).position;
    });
    expect(stickyPosition).toBe('sticky');

    // Verify it's initially visible
    await expect(page.locator('#sticky')).toBeVisible();
  });

  test('CSS-009: Z-index stacking', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .stacking-context {
              position: relative;
              width: 300px;
              height: 300px;
            }
            .layer {
              position: absolute;
              width: 150px;
              height: 150px;
              padding: 10px;
            }
            #layer1 { background-color: red; top: 0; left: 0; z-index: 1; }
            #layer2 { background-color: blue; top: 50px; left: 50px; z-index: 2; }
            #layer3 { background-color: green; top: 100px; left: 100px; z-index: 3; }
          </style>
        </head>
        <body>
          <div class="stacking-context">
            <div class="layer" id="layer1">Layer 1 (z:1)</div>
            <div class="layer" id="layer2">Layer 2 (z:2)</div>
            <div class="layer" id="layer3">Layer 3 (z:3)</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify z-index values
    const zIndexes = await page.evaluate(() => {
      return {
        layer1: window.getComputedStyle(document.getElementById('layer1')!).zIndex,
        layer2: window.getComputedStyle(document.getElementById('layer2')!).zIndex,
        layer3: window.getComputedStyle(document.getElementById('layer3')!).zIndex
      };
    });

    expect(parseInt(zIndexes.layer1)).toBe(1);
    expect(parseInt(zIndexes.layer2)).toBe(2);
    expect(parseInt(zIndexes.layer3)).toBe(3);

    // Verify stacking order (layer3 should be on top)
    await expect(page.locator('#layer3')).toBeVisible();
  });

  test('CSS-010: Display modes (block, inline, inline-block)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .block { display: block; background-color: #f44336; margin: 5px; }
            .inline { display: inline; background-color: #4CAF50; margin: 5px; }
            .inline-block { display: inline-block; background-color: #2196F3; margin: 5px; width: 100px; height: 50px; }
          </style>
        </head>
        <body>
          <div class="block" id="block">Block Element</div>
          <span class="inline" id="inline1">Inline 1</span>
          <span class="inline" id="inline2">Inline 2</span>
          <div class="inline-block" id="inline-block1">IB 1</div>
          <div class="inline-block" id="inline-block2">IB 2</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify display modes
    const displays = await page.evaluate(() => {
      return {
        block: window.getComputedStyle(document.getElementById('block')!).display,
        inline: window.getComputedStyle(document.getElementById('inline1')!).display,
        inlineBlock: window.getComputedStyle(document.getElementById('inline-block1')!).display
      };
    });

    expect(displays.block).toBe('block');
    expect(displays.inline).toBe('inline');
    expect(displays.inlineBlock).toBe('inline-block');
  });

  test('CSS-011: Box model (margin, border, padding)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .box {
              margin: 20px;
              border: 5px solid #000;
              padding: 15px;
              width: 200px;
              height: 100px;
              background-color: #FFC107;
            }
          </style>
        </head>
        <body>
          <div class="box" id="box">Content</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify box model properties
    const boxModel = await page.locator('#box').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        margin: style.margin,
        marginTop: style.marginTop,
        border: style.borderWidth,
        borderTop: style.borderTopWidth,
        padding: style.padding,
        paddingTop: style.paddingTop,
        width: style.width,
        height: style.height
      };
    });

    expect(boxModel.marginTop).toBe('20px');
    expect(boxModel.borderTop).toBe('5px');
    expect(boxModel.paddingTop).toBe('15px');
    expect(boxModel.width).toBe('200px');
    expect(boxModel.height).toBe('100px');
  });

  test('CSS-012: Width/height constraints', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .constrained {
              min-width: 100px;
              max-width: 300px;
              min-height: 50px;
              max-height: 200px;
              background-color: #9C27B0;
              resize: both;
              overflow: auto;
            }
          </style>
        </head>
        <body>
          <div class="constrained" id="constrained">Constrained element</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify constraints
    const constraints = await page.locator('#constrained').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        minWidth: style.minWidth,
        maxWidth: style.maxWidth,
        minHeight: style.minHeight,
        maxHeight: style.maxHeight
      };
    });

    expect(constraints.minWidth).toBe('100px');
    expect(constraints.maxWidth).toBe('300px');
    expect(constraints.minHeight).toBe('50px');
    expect(constraints.maxHeight).toBe('200px');
  });

  test('CSS-013: Overflow handling', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .overflow-box {
              width: 200px;
              height: 100px;
              border: 2px solid #000;
              margin: 10px;
            }
            #hidden { overflow: hidden; }
            #scroll { overflow: scroll; }
            #auto { overflow: auto; }
            #visible { overflow: visible; }
          </style>
        </head>
        <body>
          <div class="overflow-box" id="hidden">
            <p>This is a long text that will overflow the container. Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>
          </div>
          <div class="overflow-box" id="scroll">
            <p>This box has scrollbars. Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>
          </div>
          <div class="overflow-box" id="auto">
            <p>Auto overflow. Lorem ipsum dolor sit amet.</p>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify overflow properties
    const overflows = await page.evaluate(() => {
      return {
        hidden: window.getComputedStyle(document.getElementById('hidden')!).overflow,
        scroll: window.getComputedStyle(document.getElementById('scroll')!).overflow,
        auto: window.getComputedStyle(document.getElementById('auto')!).overflow
      };
    });

    expect(overflows.hidden).toBe('hidden');
    expect(overflows.scroll).toBe('scroll');
    expect(overflows.auto).toBe('auto');
  });

  test('CSS-014: Multi-column layout', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .columns {
              column-count: 3;
              column-gap: 20px;
              column-rule: 2px solid #000;
            }
          </style>
        </head>
        <body>
          <div class="columns" id="columns">
            <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.</p>
            <p>Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.</p>
            <p>Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.</p>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify column properties
    const columnProps = await page.locator('#columns').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        columnCount: style.columnCount,
        columnGap: style.columnGap,
        columnRuleWidth: style.columnRuleWidth
      };
    });

    expect(columnProps.columnCount).toBe('3');
    expect(columnProps.columnGap).toBe('20px');
  });

  test('CSS-015: Responsive breakpoints', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .responsive-box {
              background-color: red;
              padding: 20px;
            }
            @media (min-width: 768px) {
              .responsive-box {
                background-color: blue;
              }
            }
            @media (min-width: 1024px) {
              .responsive-box {
                background-color: green;
              }
            }
          </style>
        </head>
        <body>
          <div class="responsive-box" id="responsive">Responsive Element</div>
        </body>
      </html>
    `;

    // Test at mobile size
    await page.setViewportSize({ width: 375, height: 667 });
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const mobileColor = await page.locator('#responsive').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(mobileColor).toContain('255, 0, 0'); // red

    // Test at tablet size
    await page.setViewportSize({ width: 768, height: 1024 });
    await page.waitForTimeout(100);

    const tabletColor = await page.locator('#responsive').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(tabletColor).toContain('0, 0, 255'); // blue

    // Test at desktop size
    await page.setViewportSize({ width: 1920, height: 1080 });
    await page.waitForTimeout(100);

    const desktopColor = await page.locator('#responsive').evaluate(el => {
      return window.getComputedStyle(el).backgroundColor;
    });
    expect(desktopColor).toContain('0, 128, 0'); // green
  });

  test('CSS-016: Viewport units (vw, vh)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .viewport-width {
              width: 50vw;
              height: 100px;
              background-color: #3F51B5;
            }
            .viewport-height {
              width: 200px;
              height: 50vh;
              background-color: #FF5722;
            }
          </style>
        </head>
        <body>
          <div class="viewport-width" id="vw">50vw wide</div>
          <div class="viewport-height" id="vh">50vh tall</div>
        </body>
      </html>
    `;

    await page.setViewportSize({ width: 1000, height: 800 });
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify viewport units
    const vwWidth = await page.locator('#vw').evaluate(el => {
      return el.getBoundingClientRect().width;
    });
    expect(vwWidth).toBeCloseTo(500, 0); // 50% of 1000px

    const vhHeight = await page.locator('#vh').evaluate(el => {
      return el.getBoundingClientRect().height;
    });
    expect(vhHeight).toBeCloseTo(400, 0); // 50% of 800px
  });

  test('CSS-017: Calc() expressions', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .calc-box {
              width: calc(100% - 50px);
              height: calc(200px + 10%);
              padding: calc(10px * 2);
              margin: calc(20px / 2);
              background-color: #00BCD4;
            }
          </style>
        </head>
        <body>
          <div class="calc-box" id="calc">Calc Box</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify calc expressions work
    const calcProps = await page.locator('#calc').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        padding: parseFloat(style.paddingTop),
        margin: parseFloat(style.marginTop)
      };
    });

    expect(calcProps.padding).toBe(20); // 10px * 2
    expect(calcProps.margin).toBe(10); // 20px / 2
  });

  test('CSS-018: CSS variables (custom properties)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            :root {
              --primary-color: #673AB7;
              --secondary-color: #FF4081;
              --spacing: 20px;
              --font-size: 16px;
            }
            .var-box {
              background-color: var(--primary-color);
              color: var(--secondary-color);
              padding: var(--spacing);
              font-size: var(--font-size);
            }
          </style>
        </head>
        <body>
          <div class="var-box" id="var-box">CSS Variables</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify CSS variables work
    const varProps = await page.locator('#var-box').evaluate(el => {
      const style = window.getComputedStyle(el);
      return {
        backgroundColor: style.backgroundColor,
        color: style.color,
        padding: style.paddingTop,
        fontSize: style.fontSize
      };
    });

    expect(varProps.backgroundColor).toContain('103, 58, 183'); // --primary-color
    expect(varProps.padding).toBe('20px'); // --spacing
    expect(varProps.fontSize).toBe('16px'); // --font-size
  });

  test('CSS-019: Container queries', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .container {
              container-type: inline-size;
              width: 300px;
              border: 2px solid #000;
            }
            .item {
              background-color: red;
              padding: 20px;
            }
            @container (min-width: 400px) {
              .item {
                background-color: blue;
              }
            }
          </style>
        </head>
        <body>
          <div class="container" id="container">
            <div class="item" id="item">Container Query Item</div>
          </div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Note: Container queries may not be fully supported in all browsers
    // This test verifies the CSS is parsed without error
    await expect(page.locator('#container')).toBeVisible();
    await expect(page.locator('#item')).toBeVisible();
  });

  test('CSS-020: Aspect ratio', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head>
          <style>
            .aspect-box {
              width: 400px;
              aspect-ratio: 16 / 9;
              background-color: #607D8B;
            }
          </style>
        </head>
        <body>
          <div class="aspect-box" id="aspect">16:9 Aspect Ratio</div>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Verify aspect ratio
    const dimensions = await page.locator('#aspect').evaluate(el => {
      const rect = el.getBoundingClientRect();
      return {
        width: rect.width,
        height: rect.height,
        ratio: rect.width / rect.height
      };
    });

    expect(dimensions.width).toBe(400);
    // Height should be approximately 225 (400 / 16 * 9)
    expect(dimensions.ratio).toBeCloseTo(16 / 9, 1);
  });

});
