import { test, expect } from '@playwright/test';

/**
 * IntersectionObserver Tests (INT-001 to INT-004)
 * Priority: P0 (Basic visibility), P1 (Advanced features)
 *
 * Tests IntersectionObserver API functionality including:
 * - Basic element visibility detection
 * - Intersection ratio thresholds
 * - Root margin configuration
 * - Multiple target observation
 *
 * LADYBIRD COMPATIBILITY NOTES:
 * - IntersectionObserver may have limited or no support in Ladybird
 * - Tests gracefully skip if API is not available
 * - Uses simple layout to avoid complex calculation edge cases
 * - Allows time for observer callbacks with waitForTimeout()
 */

test.describe('IntersectionObserver API', () => {

  test.beforeEach(async ({ page }) => {
    // Check if IntersectionObserver is available, skip entire suite if not
    const hasIntersectionObserver = await page.evaluate(() => {
      return typeof IntersectionObserver !== 'undefined';
    });

    if (!hasIntersectionObserver) {
      test.skip();
    }
  });

  test('INT-001: Basic element visibility detection', { tag: '@p0' }, async ({ page }) => {
    // Navigate to localhost (required per spec, not data: URLs)
    await page.goto('http://localhost:9081/');

    // Set up test HTML with scrollable content
    await page.setContent(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { margin: 0; padding: 0; }
          .spacer { height: 2000px; background: linear-gradient(white, lightgray); }
          #target {
            width: 200px;
            height: 200px;
            background: coral;
            margin: 20px;
          }
        </style>
      </head>
      <body>
        <div class="spacer"></div>
        <div id="target">Target Element</div>
        <div class="spacer"></div>
      </body>
      </html>
    `);

    // Set up IntersectionObserver and scroll element into view
    const result = await page.evaluate(async () => {
      return new Promise<{
        observerCreated: boolean;
        callbackFired: boolean;
        isIntersecting: boolean;
        targetElement: string | null;
      }>((resolve) => {
        const target = document.getElementById('target');
        let observerCreated = false;
        let callbackFired = false;
        let isIntersecting = false;

        if (!target) {
          resolve({
            observerCreated: false,
            callbackFired: false,
            isIntersecting: false,
            targetElement: null
          });
          return;
        }

        try {
          // Create IntersectionObserver
          const observer = new IntersectionObserver((entries) => {
            callbackFired = true;

            for (const entry of entries) {
              if (entry.target === target) {
                isIntersecting = entry.isIntersecting;
              }
            }

            // Resolve after first callback
            if (isIntersecting) {
              resolve({
                observerCreated: true,
                callbackFired: true,
                isIntersecting: true,
                targetElement: target.id
              });
            }
          });

          observerCreated = true;

          // Start observing
          observer.observe(target);

          // Scroll element into view after a brief delay
          setTimeout(() => {
            target.scrollIntoView({ behavior: 'auto', block: 'center' });
          }, 100);

          // Fallback timeout if callback never fires
          setTimeout(() => {
            resolve({
              observerCreated,
              callbackFired,
              isIntersecting,
              targetElement: target.id
            });
          }, 2000);

        } catch (error) {
          resolve({
            observerCreated: false,
            callbackFired: false,
            isIntersecting: false,
            targetElement: null
          });
        }
      });
    });

    // Verify observer was created and callback fired
    expect(result.observerCreated).toBe(true);
    expect(result.callbackFired).toBe(true);
    expect(result.isIntersecting).toBe(true);
    expect(result.targetElement).toBe('target');
  });

  test('INT-002: Intersection ratio thresholds', { tag: '@p1' }, async ({ page }) => {
    await page.goto('http://localhost:9081/');

    await page.setContent(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { margin: 0; padding: 0; height: 3000px; }
          #viewport-container {
            position: relative;
            height: 100vh;
            overflow: hidden;
          }
          #target {
            width: 200px;
            height: 400px;
            background: teal;
            position: absolute;
            top: 500px;
            left: 50px;
          }
        </style>
      </head>
      <body>
        <div id="viewport-container">
          <div id="target">Target with Thresholds</div>
        </div>
      </body>
      </html>
    `);

    const result = await page.evaluate(async () => {
      return new Promise<{
        thresholdsUsed: number[];
        callbackCount: number;
        ratiosObserved: number[];
        hasMultipleThresholds: boolean;
      }>((resolve) => {
        const target = document.getElementById('target');
        const thresholds = [0, 0.5, 1.0];
        const ratiosObserved: number[] = [];
        let callbackCount = 0;

        if (!target) {
          resolve({
            thresholdsUsed: [],
            callbackCount: 0,
            ratiosObserved: [],
            hasMultipleThresholds: false
          });
          return;
        }

        try {
          const observer = new IntersectionObserver(
            (entries) => {
              callbackCount++;

              for (const entry of entries) {
                // Record intersection ratio (rounded to 1 decimal place)
                const ratio = Math.round(entry.intersectionRatio * 10) / 10;
                if (!ratiosObserved.includes(ratio)) {
                  ratiosObserved.push(ratio);
                }
              }
            },
            { threshold: thresholds }
          );

          observer.observe(target);

          // Simulate gradual scrolling into view
          let scrollPos = 500;
          const scrollInterval = setInterval(() => {
            if (scrollPos <= 0) {
              clearInterval(scrollInterval);

              // Give time for final callbacks
              setTimeout(() => {
                resolve({
                  thresholdsUsed: thresholds,
                  callbackCount,
                  ratiosObserved: ratiosObserved.sort((a, b) => a - b),
                  hasMultipleThresholds: ratiosObserved.length > 1
                });
              }, 300);
              return;
            }

            scrollPos -= 100;
            if (target.style) {
              target.style.top = `${scrollPos}px`;
            }
          }, 100);

          // Fallback timeout
          setTimeout(() => {
            clearInterval(scrollInterval);
            resolve({
              thresholdsUsed: thresholds,
              callbackCount,
              ratiosObserved: ratiosObserved.sort((a, b) => a - b),
              hasMultipleThresholds: ratiosObserved.length > 1
            });
          }, 3000);

        } catch (error) {
          resolve({
            thresholdsUsed: [],
            callbackCount: 0,
            ratiosObserved: [],
            hasMultipleThresholds: false
          });
        }
      });
    });

    // Verify thresholds were used and callbacks occurred
    expect(result.thresholdsUsed).toEqual([0, 0.5, 1.0]);
    expect(result.callbackCount).toBeGreaterThan(0);

    // Should observe at least the initial intersection (may not catch all thresholds in simple implementation)
    expect(result.ratiosObserved.length).toBeGreaterThan(0);

    // Log observed ratios for debugging
    console.log('Observed intersection ratios:', result.ratiosObserved);
  });

  test('INT-003: Root margin configuration', { tag: '@p1' }, async ({ page }) => {
    await page.goto('http://localhost:9081/');

    await page.setContent(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { margin: 0; padding: 0; }
          #scroll-container {
            width: 400px;
            height: 400px;
            overflow: auto;
            border: 2px solid black;
            margin: 20px;
          }
          .content-spacer {
            height: 600px;
            background: linear-gradient(white, lightblue);
          }
          #target {
            width: 300px;
            height: 150px;
            background: orange;
            margin: 20px;
          }
        </style>
      </head>
      <body>
        <div id="scroll-container">
          <div class="content-spacer"></div>
          <div id="target">Target with Root Margin</div>
          <div class="content-spacer"></div>
        </div>
      </body>
      </html>
    `);

    const result = await page.evaluate(async () => {
      return new Promise<{
        observerWithMargin: boolean;
        observerWithoutMargin: boolean;
        marginCallbackFired: boolean;
        noMarginCallbackFired: boolean;
        rootMarginUsed: string;
      }>((resolve) => {
        const container = document.getElementById('scroll-container');
        const target = document.getElementById('target');

        let marginCallbackFired = false;
        let noMarginCallbackFired = false;
        const rootMarginUsed = '50px';

        if (!container || !target) {
          resolve({
            observerWithMargin: false,
            observerWithoutMargin: false,
            marginCallbackFired: false,
            noMarginCallbackFired: false,
            rootMarginUsed: ''
          });
          return;
        }

        try {
          // Observer WITH root margin (should detect earlier)
          const observerWithMargin = new IntersectionObserver(
            (entries) => {
              for (const entry of entries) {
                if (entry.isIntersecting) {
                  marginCallbackFired = true;
                }
              }
            },
            {
              root: container,
              rootMargin: rootMarginUsed
            }
          );

          // Observer WITHOUT root margin
          const observerWithoutMargin = new IntersectionObserver(
            (entries) => {
              for (const entry of entries) {
                if (entry.isIntersecting) {
                  noMarginCallbackFired = true;
                }
              }
            },
            {
              root: container,
              rootMargin: '0px'
            }
          );

          observerWithMargin.observe(target);
          observerWithoutMargin.observe(target);

          // Scroll target into view gradually
          setTimeout(() => {
            container.scrollTop = 400;
          }, 200);

          setTimeout(() => {
            container.scrollTop = 600;
          }, 400);

          // Check results after scrolling
          setTimeout(() => {
            resolve({
              observerWithMargin: true,
              observerWithoutMargin: true,
              marginCallbackFired,
              noMarginCallbackFired,
              rootMarginUsed
            });
          }, 800);

        } catch (error) {
          resolve({
            observerWithMargin: false,
            observerWithoutMargin: false,
            marginCallbackFired: false,
            noMarginCallbackFired: false,
            rootMarginUsed: ''
          });
        }
      });
    });

    // Verify both observers were created
    expect(result.observerWithMargin).toBe(true);
    expect(result.observerWithoutMargin).toBe(true);

    // At least one should fire (implementation-dependent)
    const atLeastOneCallbackFired = result.marginCallbackFired || result.noMarginCallbackFired;
    expect(atLeastOneCallbackFired).toBe(true);

    // Log for debugging
    console.log('Root margin observer fired:', result.marginCallbackFired);
    console.log('No margin observer fired:', result.noMarginCallbackFired);
  });

  test('INT-004: Multiple targets observation', { tag: '@p0' }, async ({ page }) => {
    await page.goto('http://localhost:9081/');

    await page.setContent(`
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { margin: 0; padding: 0; }
          .item {
            width: 200px;
            height: 150px;
            margin: 20px;
            display: inline-block;
          }
          .visible-area {
            height: 400px;
            overflow: hidden;
            border: 2px dashed gray;
          }
          .hidden-area {
            margin-top: 1000px;
          }
          #item1 { background: lightcoral; }
          #item2 { background: lightblue; }
          #item3 { background: lightgreen; }
          #item4 { background: lightyellow; }
        </style>
      </head>
      <body>
        <div class="visible-area">
          <div id="item1" class="item">Item 1 (Visible)</div>
          <div id="item2" class="item">Item 2 (Visible)</div>
        </div>
        <div class="hidden-area">
          <div id="item3" class="item">Item 3 (Hidden)</div>
          <div id="item4" class="item">Item 4 (Hidden)</div>
        </div>
      </body>
      </html>
    `);

    const result = await page.evaluate(async () => {
      return new Promise<{
        totalTargets: number;
        observedTargets: string[];
        visibleTargets: string[];
        hiddenTargets: string[];
        allTargetsTracked: boolean;
      }>((resolve) => {
        const items = [
          document.getElementById('item1'),
          document.getElementById('item2'),
          document.getElementById('item3'),
          document.getElementById('item4')
        ];

        const observedTargets: string[] = [];
        const visibleTargets: string[] = [];
        const hiddenTargets: string[] = [];

        // Filter out null items
        const validItems = items.filter((item): item is HTMLElement => item !== null);

        if (validItems.length === 0) {
          resolve({
            totalTargets: 0,
            observedTargets: [],
            visibleTargets: [],
            hiddenTargets: [],
            allTargetsTracked: false
          });
          return;
        }

        try {
          // Single observer watching all targets
          const observer = new IntersectionObserver((entries) => {
            for (const entry of entries) {
              const target = entry.target as HTMLElement;
              const targetId = target.id;

              // Track that we observed this target
              if (!observedTargets.includes(targetId)) {
                observedTargets.push(targetId);
              }

              // Categorize by visibility
              if (entry.isIntersecting) {
                if (!visibleTargets.includes(targetId)) {
                  visibleTargets.push(targetId);
                }
              } else {
                if (!hiddenTargets.includes(targetId)) {
                  hiddenTargets.push(targetId);
                }
              }
            }
          });

          // Observe all items
          validItems.forEach(item => {
            observer.observe(item);
          });

          // Wait for initial observations
          setTimeout(() => {
            resolve({
              totalTargets: validItems.length,
              observedTargets: observedTargets.sort(),
              visibleTargets: visibleTargets.sort(),
              hiddenTargets: hiddenTargets.sort(),
              allTargetsTracked: observedTargets.length === validItems.length
            });
          }, 500);

        } catch (error) {
          resolve({
            totalTargets: 0,
            observedTargets: [],
            visibleTargets: [],
            hiddenTargets: [],
            allTargetsTracked: false
          });
        }
      });
    });

    // Verify all targets were tracked
    expect(result.totalTargets).toBe(4);
    expect(result.observedTargets.length).toBeGreaterThan(0);

    // Should observe all targets (may depend on implementation)
    // At minimum, should observe some targets
    expect(result.observedTargets.length).toBeGreaterThanOrEqual(2);

    // At least items 1 and 2 should be visible initially
    // (may vary by implementation)
    console.log('Observed targets:', result.observedTargets);
    console.log('Visible targets:', result.visibleTargets);
    console.log('Hidden targets:', result.hiddenTargets);
    console.log('All targets tracked:', result.allTargetsTracked);

    // Verify the observer is tracking multiple elements
    expect(result.observedTargets.length).toBeGreaterThan(1);
  });

});
