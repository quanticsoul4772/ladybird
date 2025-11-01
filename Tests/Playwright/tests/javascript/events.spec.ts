import { test, expect } from '@playwright/test';

/**
 * Event Handling Tests (JS-EVT-001 to JS-EVT-010)
 * Priority: P0 (Critical)
 *
 * Tests JavaScript event handling including:
 * - Event listener registration and removal
 * - Event bubbling and capturing
 * - Event methods (preventDefault, stopPropagation)
 * - Custom events
 * - Mouse events
 * - Keyboard events
 * - Form events
 * - Focus events
 */

test.describe('Event Handling', () => {

  test('JS-EVT-001: addEventListener/removeEventListener', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <button id="btn">Click Me</button>
          <div id="output"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const btn = document.getElementById('btn');
      const output = document.getElementById('output');
      let clickCount = 0;

      // Add event listener
      const handler = () => {
        clickCount++;
        if (output) output.textContent = `Clicked ${clickCount} times`;
      };

      btn?.addEventListener('click', handler);

      // Store handler for later removal
      (window as any).testHandler = handler;
      (window as any).testButton = btn;
    });

    // Click button multiple times
    await page.click('#btn');
    await page.click('#btn');

    let output = await page.locator('#output').textContent();
    expect(output).toBe('Clicked 2 times');

    // Remove event listener
    await page.evaluate(() => {
      const btn = (window as any).testButton;
      const handler = (window as any).testHandler;
      btn?.removeEventListener('click', handler);
    });

    // Click again - count should not increase
    await page.click('#btn');
    output = await page.locator('#output').textContent();
    expect(output).toBe('Clicked 2 times'); // Still 2, not 3
  });

  test('JS-EVT-002: Event bubbling', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="outer" style="padding: 50px; background: lightblue;">
            <div id="middle" style="padding: 50px; background: lightgreen;">
              <button id="inner">Click</button>
            </div>
          </div>
          <div id="log"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const log = document.getElementById('log');
      const events: string[] = [];

      // Add listeners to all levels (bubbling phase)
      document.getElementById('inner')?.addEventListener('click', () => {
        events.push('inner');
      });

      document.getElementById('middle')?.addEventListener('click', () => {
        events.push('middle');
      });

      document.getElementById('outer')?.addEventListener('click', () => {
        events.push('outer');
      });

      // Store for verification
      (window as any).events = events;
    });

    // Click the inner button
    await page.click('#inner');

    // Verify bubbling order: inner -> middle -> outer
    const events = await page.evaluate(() => (window as any).events);
    expect(events).toEqual(['inner', 'middle', 'outer']);
  });

  test('JS-EVT-003: Event capturing', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="outer">
            <div id="middle">
              <button id="inner">Click</button>
            </div>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const events: string[] = [];

      // Add listeners in capture phase (third parameter = true)
      document.getElementById('outer')?.addEventListener('click', () => {
        events.push('outer-capture');
      }, true);

      document.getElementById('middle')?.addEventListener('click', () => {
        events.push('middle-capture');
      }, true);

      document.getElementById('inner')?.addEventListener('click', () => {
        events.push('inner-bubble');
      }, false); // Bubbling phase

      (window as any).events = events;
    });

    await page.click('#inner');

    // Verify capture order: outer-capture -> middle-capture -> inner-bubble
    const events = await page.evaluate(() => (window as any).events);
    expect(events).toEqual(['outer-capture', 'middle-capture', 'inner-bubble']);
  });

  test('JS-EVT-004: Event.preventDefault()', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <form id="form" action="/submit">
            <input type="checkbox" id="checkbox">
            <button type="submit" id="submit">Submit</button>
          </form>
          <div id="status">Not submitted</div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const form = document.getElementById('form');
      const checkbox = document.getElementById('checkbox');
      const status = document.getElementById('status');

      // Prevent form submission
      form?.addEventListener('submit', (e) => {
        e.preventDefault();
        if (status) status.textContent = 'Prevented submission';
      });

      // Prevent checkbox default behavior
      checkbox?.addEventListener('click', (e) => {
        e.preventDefault();
      });
    });

    // Try to submit form
    await page.click('#submit');
    const status = await page.locator('#status').textContent();
    expect(status).toBe('Prevented submission');

    // Try to check checkbox
    await page.click('#checkbox');
    const isChecked = await page.evaluate(() => {
      return (document.getElementById('checkbox') as HTMLInputElement)?.checked;
    });
    expect(isChecked).toBe(false); // Should not be checked due to preventDefault
  });

  test('JS-EVT-005: Event.stopPropagation()', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="outer">
            <div id="middle">
              <button id="inner">Click</button>
            </div>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const events: string[] = [];

      document.getElementById('inner')?.addEventListener('click', (e) => {
        events.push('inner');
        e.stopPropagation(); // Stop bubbling here
      });

      document.getElementById('middle')?.addEventListener('click', () => {
        events.push('middle');
      });

      document.getElementById('outer')?.addEventListener('click', () => {
        events.push('outer');
      });

      (window as any).events = events;
    });

    await page.click('#inner');

    // Only inner should fire, propagation stopped
    const events = await page.evaluate(() => (window as any).events);
    expect(events).toEqual(['inner']);
  });

  test('JS-EVT-006: Custom events (CustomEvent)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="target"></div><div id="output"></div></body>');

    const result = await page.evaluate(() => {
      const target = document.getElementById('target');
      const output = document.getElementById('output');

      let receivedDetail: any = null;

      // Listen for custom event
      target?.addEventListener('myCustomEvent', ((e: CustomEvent) => {
        receivedDetail = e.detail;
        if (output) output.textContent = `Received: ${e.detail.message}`;
      }) as EventListener);

      // Dispatch custom event
      const customEvent = new CustomEvent('myCustomEvent', {
        detail: {
          message: 'Hello from custom event',
          timestamp: Date.now()
        },
        bubbles: true,
        cancelable: true
      });

      target?.dispatchEvent(customEvent);

      return {
        received: receivedDetail !== null,
        message: receivedDetail?.message
      };
    });

    expect(result.received).toBe(true);
    expect(result.message).toBe('Hello from custom event');

    const output = await page.locator('#output').textContent();
    expect(output).toContain('Received: Hello from custom event');
  });

  test('JS-EVT-007: Mouse events (click, dblclick, mouseover)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="target" style="width: 200px; height: 100px; background: lightblue;">
            Hover and click me
          </div>
          <div id="log"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const target = document.getElementById('target');
      const events: string[] = [];

      target?.addEventListener('click', () => events.push('click'));
      target?.addEventListener('dblclick', () => events.push('dblclick'));
      target?.addEventListener('mouseover', () => events.push('mouseover'));
      target?.addEventListener('mouseout', () => events.push('mouseout'));
      target?.addEventListener('mouseenter', () => events.push('mouseenter'));
      target?.addEventListener('mouseleave', () => events.push('mouseleave'));

      (window as any).events = events;
    });

    // Trigger mouse events
    await page.hover('#target');
    await page.click('#target');
    await page.dblclick('#target');
    await page.hover('body'); // Move away

    const events = await page.evaluate(() => (window as any).events);

    // Verify mouse events fired
    expect(events).toContain('mouseover');
    expect(events).toContain('mouseenter');
    expect(events).toContain('click');
    expect(events).toContain('dblclick');
    expect(events).toContain('mouseout');
    expect(events).toContain('mouseleave');
  });

  test('JS-EVT-008: Keyboard events (keydown, keyup, keypress)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <input type="text" id="input" placeholder="Type here">
          <div id="log"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const input = document.getElementById('input');
      const events: Array<{type: string, key: string}> = [];

      input?.addEventListener('keydown', (e) => {
        events.push({ type: 'keydown', key: (e as KeyboardEvent).key });
      });

      input?.addEventListener('keyup', (e) => {
        events.push({ type: 'keyup', key: (e as KeyboardEvent).key });
      });

      // Note: keypress is deprecated but may still be tested
      input?.addEventListener('keypress', (e) => {
        events.push({ type: 'keypress', key: (e as KeyboardEvent).key });
      });

      (window as any).events = events;
    });

    // Focus input and type
    await page.focus('#input');
    await page.keyboard.press('A');
    await page.keyboard.press('Enter');

    const events = await page.evaluate(() => (window as any).events);

    // Verify keyboard events
    const keydownEvents = events.filter((e: any) => e.type === 'keydown');
    const keyupEvents = events.filter((e: any) => e.type === 'keyup');

    expect(keydownEvents.length).toBeGreaterThan(0);
    expect(keyupEvents.length).toBeGreaterThan(0);

    // Check specific keys
    expect(events.some((e: any) => e.key === 'A' || e.key === 'a')).toBe(true);
    expect(events.some((e: any) => e.key === 'Enter')).toBe(true);
  });

  test('JS-EVT-009: Form events (submit, change, input)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <form id="form">
            <input type="text" id="text" name="text">
            <select id="select" name="select">
              <option value="1">Option 1</option>
              <option value="2">Option 2</option>
            </select>
            <button type="submit">Submit</button>
          </form>
          <div id="log"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const form = document.getElementById('form');
      const text = document.getElementById('text');
      const select = document.getElementById('select');
      const events: string[] = [];

      // Input event (fires on every keystroke)
      text?.addEventListener('input', () => {
        events.push('input');
      });

      // Change event (fires when value changes and loses focus)
      text?.addEventListener('change', () => {
        events.push('text-change');
      });

      select?.addEventListener('change', () => {
        events.push('select-change');
      });

      // Submit event
      form?.addEventListener('submit', (e) => {
        e.preventDefault();
        events.push('submit');
      });

      (window as any).events = events;
    });

    // Type in text field
    await page.focus('#text');
    await page.keyboard.type('test');

    // Change select
    await page.selectOption('#select', '2');

    // Submit form
    await page.click('button[type="submit"]');

    const events = await page.evaluate(() => (window as any).events);

    // Verify events fired
    expect(events).toContain('input'); // Multiple input events from typing
    expect(events).toContain('select-change');
    expect(events).toContain('submit');
  });

  test('JS-EVT-010: Focus events (focus, blur, focusin, focusout)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <input type="text" id="input1" placeholder="Input 1">
          <input type="text" id="input2" placeholder="Input 2">
          <div id="log"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.evaluate(() => {
      const input1 = document.getElementById('input1');
      const input2 = document.getElementById('input2');
      const events: Array<{type: string, target: string}> = [];

      // Focus and blur don't bubble
      input1?.addEventListener('focus', () => {
        events.push({ type: 'focus', target: 'input1' });
      });

      input1?.addEventListener('blur', () => {
        events.push({ type: 'blur', target: 'input1' });
      });

      input2?.addEventListener('focus', () => {
        events.push({ type: 'focus', target: 'input2' });
      });

      // Focusin and focusout do bubble
      document.body.addEventListener('focusin', (e) => {
        events.push({ type: 'focusin', target: (e.target as HTMLElement).id });
      });

      document.body.addEventListener('focusout', (e) => {
        events.push({ type: 'focusout', target: (e.target as HTMLElement).id });
      });

      (window as any).events = events;
    });

    // Focus first input
    await page.focus('#input1');
    await page.waitForTimeout(100);

    // Focus second input (blurs first)
    await page.focus('#input2');
    await page.waitForTimeout(100);

    // Blur second input
    await page.click('body');
    await page.waitForTimeout(100);

    const events = await page.evaluate(() => (window as any).events);

    // Verify focus events
    expect(events.some((e: any) => e.type === 'focus' && e.target === 'input1')).toBe(true);
    expect(events.some((e: any) => e.type === 'blur' && e.target === 'input1')).toBe(true);
    expect(events.some((e: any) => e.type === 'focus' && e.target === 'input2')).toBe(true);

    // Verify focusin events (these bubble)
    expect(events.some((e: any) => e.type === 'focusin' && e.target === 'input1')).toBe(true);
    expect(events.some((e: any) => e.type === 'focusin' && e.target === 'input2')).toBe(true);
    expect(events.some((e: any) => e.type === 'focusout')).toBe(true);
  });

});
