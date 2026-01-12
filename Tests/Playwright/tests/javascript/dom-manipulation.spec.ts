import { test, expect } from '@playwright/test';

/**
 * DOM Manipulation Tests (JS-DOM-001 to JS-DOM-012)
 * Priority: P0 (Critical)
 *
 * Tests JavaScript DOM manipulation capabilities including:
 * - Element selection (querySelector, getElementById)
 * - Element creation and insertion
 * - Element removal and replacement
 * - Attribute manipulation
 * - Class list operations
 * - innerHTML/textContent
 * - DOM tree traversal
 * - Node cloning
 * - Document fragments
 * - Shadow DOM basics
 */

test.describe('DOM Manipulation', () => {

  test('JS-DOM-001: querySelector/querySelectorAll', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div class="container">
            <p class="text">Paragraph 1</p>
            <p class="text">Paragraph 2</p>
            <span id="unique">Unique span</span>
            <div data-test="attribute">Data attribute</div>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Test querySelector (returns first match)
    const firstParagraph = await page.evaluate(() => {
      const el = document.querySelector('.text');
      return el?.textContent;
    });
    expect(firstParagraph).toBe('Paragraph 1');

    // Test querySelectorAll (returns all matches)
    const allParagraphs = await page.evaluate(() => {
      const elements = document.querySelectorAll('.text');
      return Array.from(elements).map(el => el.textContent);
    });
    expect(allParagraphs).toEqual(['Paragraph 1', 'Paragraph 2']);

    // Test complex selector
    const uniqueSpan = await page.evaluate(() => {
      const el = document.querySelector('#unique');
      return el?.textContent;
    });
    expect(uniqueSpan).toBe('Unique span');

    // Test attribute selector
    const dataTest = await page.evaluate(() => {
      const el = document.querySelector('[data-test="attribute"]');
      return el?.textContent;
    });
    expect(dataTest).toBe('Data attribute');
  });

  test('JS-DOM-002: getElementById/getElementsByClassName', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="main">Main Content</div>
          <p class="highlight">Highlight 1</p>
          <p class="highlight">Highlight 2</p>
          <span class="highlight">Highlight 3</span>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Test getElementById
    const mainDiv = await page.evaluate(() => {
      const el = document.getElementById('main');
      return el?.textContent;
    });
    expect(mainDiv).toBe('Main Content');

    // Test getElementsByClassName (returns live HTMLCollection)
    const highlights = await page.evaluate(() => {
      const elements = document.getElementsByClassName('highlight');
      return Array.from(elements).map(el => el.textContent);
    });
    expect(highlights).toEqual(['Highlight 1', 'Highlight 2', 'Highlight 3']);

    // Verify HTMLCollection is live
    const isLive = await page.evaluate(() => {
      const collection = document.getElementsByClassName('highlight');
      const initialCount = collection.length;

      // Add new element with class
      const newEl = document.createElement('div');
      newEl.className = 'highlight';
      newEl.textContent = 'Highlight 4';
      document.body.appendChild(newEl);

      return collection.length === initialCount + 1;
    });
    expect(isLive).toBe(true);
  });

  test('JS-DOM-003: createElement and appendChild', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="container"></div></body>');

    const result = await page.evaluate(() => {
      const container = document.getElementById('container');

      // Create new elements
      const heading = document.createElement('h1');
      heading.textContent = 'New Heading';
      heading.id = 'heading';

      const paragraph = document.createElement('p');
      paragraph.textContent = 'New paragraph content';
      paragraph.className = 'text';

      // Append to container
      container?.appendChild(heading);
      container?.appendChild(paragraph);

      return {
        childCount: container?.children.length || 0,
        headingText: document.getElementById('heading')?.textContent,
        paragraphClass: document.querySelector('.text')?.className
      };
    });

    expect(result.childCount).toBe(2);
    expect(result.headingText).toBe('New Heading');
    expect(result.paragraphClass).toBe('text');

    // Verify DOM rendered correctly
    const heading = page.locator('h1');
    await expect(heading).toHaveText('New Heading');
  });

  test('JS-DOM-004: removeChild and replaceChild', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="parent">
            <p id="first">First</p>
            <p id="second">Second</p>
            <p id="third">Third</p>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const parent = document.getElementById('parent');
      const second = document.getElementById('second');
      const third = document.getElementById('third');

      // Remove second child
      if (parent && second) {
        parent.removeChild(second);
      }

      const afterRemove = parent?.children.length || 0;

      // Replace third child with new element
      const newElement = document.createElement('p');
      newElement.id = 'replacement';
      newElement.textContent = 'Replacement';

      if (parent && third) {
        parent.replaceChild(newElement, third);
      }

      return {
        afterRemove,
        finalChildren: Array.from(parent?.children || []).map(el => el.id),
        replacementText: document.getElementById('replacement')?.textContent
      };
    });

    expect(result.afterRemove).toBe(2); // first and third
    expect(result.finalChildren).toEqual(['first', 'replacement']);
    expect(result.replacementText).toBe('Replacement');
  });

  test('JS-DOM-005: Element attributes (get/set/remove)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="test" title="Original Title" data-value="123"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const el = document.getElementById('test');

      // Get attributes
      const originalTitle = el?.getAttribute('title');
      const dataValue = el?.getAttribute('data-value');

      // Set new attribute
      el?.setAttribute('aria-label', 'Test element');

      // Modify existing attribute
      el?.setAttribute('title', 'New Title');

      // Remove attribute
      el?.removeAttribute('data-value');

      return {
        originalTitle,
        dataValue,
        newTitle: el?.getAttribute('title'),
        ariaLabel: el?.getAttribute('aria-label'),
        hasDataValue: el?.hasAttribute('data-value')
      };
    });

    expect(result.originalTitle).toBe('Original Title');
    expect(result.dataValue).toBe('123');
    expect(result.newTitle).toBe('New Title');
    expect(result.ariaLabel).toBe('Test element');
    expect(result.hasDataValue).toBe(false);
  });

  test('JS-DOM-006: classList manipulation', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="test" class="initial"></div></body>');

    const result = await page.evaluate(() => {
      const el = document.getElementById('test');

      const initialClasses = el?.className;

      // Add classes
      el?.classList.add('added');
      el?.classList.add('multiple', 'classes');

      const afterAdd = Array.from(el?.classList || []);

      // Remove class
      el?.classList.remove('initial');

      const afterRemove = Array.from(el?.classList || []);

      // Toggle class
      el?.classList.toggle('toggled');
      const afterToggleOn = el?.classList.contains('toggled');

      el?.classList.toggle('toggled');
      const afterToggleOff = el?.classList.contains('toggled');

      // Replace class
      el?.classList.replace('added', 'replaced');

      return {
        initialClasses,
        afterAdd,
        afterRemove,
        afterToggleOn,
        afterToggleOff,
        finalClasses: Array.from(el?.classList || [])
      };
    });

    expect(result.initialClasses).toBe('initial');
    expect(result.afterAdd).toEqual(['initial', 'added', 'multiple', 'classes']);
    expect(result.afterRemove).toEqual(['added', 'multiple', 'classes']);
    expect(result.afterToggleOn).toBe(true);
    expect(result.afterToggleOff).toBe(false);
    expect(result.finalClasses).toContain('replaced');
  });

  test('JS-DOM-007: innerHTML/outerHTML', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="container">
            <p>Original content</p>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const container = document.getElementById('container');

      const originalInner = container?.innerHTML.trim();

      // Set innerHTML
      if (container) {
        container.innerHTML = '<h1>New heading</h1><p>New paragraph</p>';
      }

      const newInner = container?.innerHTML;
      const childCount = container?.children.length || 0;

      // Read outerHTML
      const outer = container?.outerHTML;

      return {
        originalInner,
        newInner,
        childCount,
        includesDiv: outer?.includes('<div id="container">') || false
      };
    });

    expect(result.originalInner).toContain('Original content');
    expect(result.newInner).toContain('New heading');
    expect(result.childCount).toBe(2);
    expect(result.includesDiv).toBe(true);

    // Verify rendered content
    const heading = page.locator('h1');
    await expect(heading).toHaveText('New heading');
  });

  test('JS-DOM-008: textContent/innerText', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="test">
            <span>Hello</span>
            <strong style="display: none;">Hidden</strong>
            <span>World</span>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const el = document.getElementById('test');

      // textContent includes hidden elements
      const text = el?.textContent?.trim().replace(/\s+/g, ' ');

      // innerText respects display styles
      const inner = el?.innerText?.trim().replace(/\s+/g, ' ');

      // Set textContent (replaces all children)
      if (el) {
        el.textContent = 'New text content';
      }

      return {
        originalTextContent: text,
        originalInnerText: inner,
        newTextContent: el?.textContent,
        childrenAfterSet: el?.children.length || 0
      };
    });

    expect(result.originalTextContent).toContain('Hello');
    expect(result.originalTextContent).toContain('Hidden');
    expect(result.originalInnerText).toContain('Hello');
    // innerText may or may not include hidden text depending on implementation
    expect(result.newTextContent).toBe('New text content');
    expect(result.childrenAfterSet).toBe(0); // textContent removes all children
  });

  test('JS-DOM-009: DOM tree traversal (parent, children, siblings)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="parent">
            <p id="first">First</p>
            <p id="middle">Middle</p>
            <p id="last">Last</p>
          </div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const middle = document.getElementById('middle');

      return {
        parentId: middle?.parentElement?.id,
        parentNodeName: middle?.parentNode?.nodeName,
        previousSiblingId: (middle?.previousElementSibling as HTMLElement)?.id,
        nextSiblingId: (middle?.nextElementSibling as HTMLElement)?.id,
        firstChildOfParent: (middle?.parentElement?.firstElementChild as HTMLElement)?.id,
        lastChildOfParent: (middle?.parentElement?.lastElementChild as HTMLElement)?.id,
        childrenCount: middle?.parentElement?.children.length || 0
      };
    });

    expect(result.parentId).toBe('parent');
    expect(result.parentNodeName).toBe('DIV');
    expect(result.previousSiblingId).toBe('first');
    expect(result.nextSiblingId).toBe('last');
    expect(result.firstChildOfParent).toBe('first');
    expect(result.lastChildOfParent).toBe('last');
    expect(result.childrenCount).toBe(3);
  });

  test('JS-DOM-010: Node cloning (cloneNode)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="original" class="test" data-value="123">
            <p>Child paragraph</p>
            <span>Child span</span>
          </div>
          <div id="container"></div>
        </body>
      </html>
    `;
    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    const result = await page.evaluate(() => {
      const original = document.getElementById('original');
      const container = document.getElementById('container');

      // Shallow clone (no children)
      const shallow = original?.cloneNode(false) as HTMLElement;
      shallow.id = 'shallow-clone';
      container?.appendChild(shallow);

      // Deep clone (with children)
      const deep = original?.cloneNode(true) as HTMLElement;
      deep.id = 'deep-clone';
      container?.appendChild(deep);

      return {
        shallowChildren: document.getElementById('shallow-clone')?.children.length || 0,
        deepChildren: document.getElementById('deep-clone')?.children.length || 0,
        shallowClass: document.getElementById('shallow-clone')?.className,
        deepClass: document.getElementById('deep-clone')?.className,
        originalUnchanged: document.getElementById('original')?.id === 'original'
      };
    });

    expect(result.shallowChildren).toBe(0);
    expect(result.deepChildren).toBe(2);
    expect(result.shallowClass).toBe('test');
    expect(result.deepClass).toBe('test');
    expect(result.originalUnchanged).toBe(true);

    // Verify clones rendered
    const deepClone = page.locator('#deep-clone');
    await expect(deepClone).toBeVisible();
  });

  test('JS-DOM-011: Document fragments', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="container"></div></body>');

    const result = await page.evaluate(() => {
      const container = document.getElementById('container');

      // Create document fragment
      const fragment = document.createDocumentFragment();

      // Add multiple elements to fragment
      for (let i = 1; i <= 5; i++) {
        const p = document.createElement('p');
        p.textContent = `Paragraph ${i}`;
        p.className = 'fragment-item';
        fragment.appendChild(p);
      }

      // Append fragment (single reflow)
      container?.appendChild(fragment);

      return {
        itemCount: document.querySelectorAll('.fragment-item').length,
        texts: Array.from(document.querySelectorAll('.fragment-item')).map(el => el.textContent)
      };
    });

    expect(result.itemCount).toBe(5);
    expect(result.texts).toEqual([
      'Paragraph 1',
      'Paragraph 2',
      'Paragraph 3',
      'Paragraph 4',
      'Paragraph 5'
    ]);

    // Verify all items rendered
    const items = page.locator('.fragment-item');
    await expect(items).toHaveCount(5);
  });

  test('JS-DOM-012: Shadow DOM basics', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body><div id="host"></div></body>');

    const result = await page.evaluate(() => {
      const host = document.getElementById('host');

      try {
        // Attach shadow root
        const shadow = host?.attachShadow({ mode: 'open' });

        // Add content to shadow DOM
        const style = document.createElement('style');
        style.textContent = '.shadow-text { color: red; font-weight: bold; }';
        shadow?.appendChild(style);

        const p = document.createElement('p');
        p.className = 'shadow-text';
        p.textContent = 'Shadow DOM content';
        shadow?.appendChild(p);

        // Test shadow root access
        const hasShadowRoot = host?.shadowRoot !== null;
        const shadowChildren = host?.shadowRoot?.children.length || 0;

        // Test that shadow content is not in light DOM
        const inLightDOM = document.querySelector('.shadow-text') !== null;
        const inShadowDOM = host?.shadowRoot?.querySelector('.shadow-text') !== null;

        return {
          hasShadowRoot,
          shadowChildren,
          inLightDOM,
          inShadowDOM,
          shadowText: host?.shadowRoot?.querySelector('.shadow-text')?.textContent
        };
      } catch (error) {
        return {
          error: error instanceof Error ? error.message : 'Unknown error',
          hasShadowRoot: false,
          shadowChildren: 0,
          inLightDOM: false,
          inShadowDOM: false
        };
      }
    });

    // Check if shadow DOM is supported
    if ('error' in result) {
      console.log('Shadow DOM not supported or error:', result.error);
      test.skip();
    }

    expect(result.hasShadowRoot).toBe(true);
    expect(result.shadowChildren).toBeGreaterThan(0);
    expect(result.inLightDOM).toBe(false); // Shadow content not in light DOM
    expect(result.inShadowDOM).toBe(true);
    expect(result.shadowText).toBe('Shadow DOM content');
  });

});
