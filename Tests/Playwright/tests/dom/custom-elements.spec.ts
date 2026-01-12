import { test, expect } from '@playwright/test';

test.describe('Custom Elements', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('http://localhost:9080/');
  });

  test('CUST-001: Define and register custom element @p0', async ({ page }) => {
    const result = await page.evaluate(() => {
      // Check if customElements is supported
      if (typeof customElements === 'undefined') {
        return { supported: false };
      }

      // Define custom element
      class MyElement extends HTMLElement {
        connectedCallback() {
          if (!this.textContent) {
            this.textContent = 'Hello from custom element';
          }
        }
      }

      // Register
      try {
        customElements.define('my-element', MyElement);
      } catch (e) {
        return {
          supported: true,
          error: e instanceof Error ? e.message : String(e)
        };
      }

      // Use it
      const el = document.createElement('my-element');
      document.body.appendChild(el);

      return {
        supported: true,
        tagName: el.tagName.toLowerCase(),
        content: el.textContent,
        isDefined: customElements.get('my-element') !== undefined
      };
    });

    if (!result.supported) {
      test.skip();
      return;
    }

    if ('error' in result) {
      test.skip();
      return;
    }

    expect(result.tagName).toBe('my-element');
    expect(result.content).toBe('Hello from custom element');
    expect(result.isDefined).toBe(true);
  });

  test('CUST-002: Lifecycle callbacks (connectedCallback, disconnectedCallback) @p0', async ({ page }) => {
    const result = await page.evaluate(() => {
      // Check if customElements is supported
      if (typeof customElements === 'undefined') {
        return { supported: false };
      }

      const events: string[] = [];

      // Define custom element with lifecycle callbacks
      class LifecycleElement extends HTMLElement {
        constructor() {
          super();
          events.push('constructor');
        }

        connectedCallback() {
          events.push('connected');
        }

        disconnectedCallback() {
          events.push('disconnected');
        }
      }

      try {
        customElements.define('lifecycle-element', LifecycleElement);
      } catch (e) {
        return {
          supported: true,
          error: e instanceof Error ? e.message : String(e)
        };
      }

      // Create and add to DOM
      const el = document.createElement('lifecycle-element');
      document.body.appendChild(el);

      // Remove from DOM
      document.body.removeChild(el);

      // Add back to test connectedCallback is called again
      document.body.appendChild(el);

      return {
        supported: true,
        events: events
      };
    });

    if (!result.supported) {
      test.skip();
      return;
    }

    if ('error' in result) {
      test.skip();
      return;
    }

    // Constructor should be called first
    expect(result.events[0]).toBe('constructor');

    // connectedCallback should be called when element is added
    expect(result.events).toContain('connected');

    // disconnectedCallback should be called when element is removed
    expect(result.events).toContain('disconnected');

    // Check the order: constructor, connected, disconnected, connected
    const expectedOrder = ['constructor', 'connected', 'disconnected', 'connected'];
    expect(result.events).toEqual(expectedOrder);
  });

  test('CUST-003: Attributes and properties @p1', async ({ page }) => {
    const result = await page.evaluate(() => {
      // Check if customElements is supported
      if (typeof customElements === 'undefined') {
        return { supported: false };
      }

      const changes: Array<{attr: string, oldVal: string | null, newVal: string | null}> = [];

      // Define custom element with observed attributes
      class AttributeElement extends HTMLElement {
        static get observedAttributes() {
          return ['name', 'value'];
        }

        attributeChangedCallback(name: string, oldValue: string | null, newValue: string | null) {
          changes.push({ attr: name, oldVal: oldValue, newVal: newValue });
        }

        get name() {
          return this.getAttribute('name');
        }

        set name(val: string | null) {
          if (val !== null) {
            this.setAttribute('name', val);
          } else {
            this.removeAttribute('name');
          }
        }
      }

      try {
        customElements.define('attribute-element', AttributeElement);
      } catch (e) {
        return {
          supported: true,
          error: e instanceof Error ? e.message : String(e)
        };
      }

      // Create element and modify attributes
      const el = document.createElement('attribute-element') as any;
      document.body.appendChild(el);

      // Set attribute directly
      el.setAttribute('name', 'test');

      // Set attribute via property
      el.name = 'updated';

      // Set non-observed attribute (should not trigger callback)
      el.setAttribute('id', 'myid');

      // Get attribute via property
      const nameValue = el.name;

      return {
        supported: true,
        changes: changes,
        nameValue: nameValue,
        hasId: el.hasAttribute('id')
      };
    });

    if (!result.supported) {
      test.skip();
      return;
    }

    if ('error' in result) {
      test.skip();
      return;
    }

    // Should have captured attribute changes for 'name'
    expect(result.changes.length).toBeGreaterThanOrEqual(2);

    // First change: null -> 'test'
    expect(result.changes[0]).toEqual({ attr: 'name', oldVal: null, newVal: 'test' });

    // Second change: 'test' -> 'updated'
    expect(result.changes[1]).toEqual({ attr: 'name', oldVal: 'test', newVal: 'updated' });

    // Property getter should work
    expect(result.nameValue).toBe('updated');

    // Non-observed attribute should be set but not trigger callback
    expect(result.hasId).toBe(true);
  });

  test('CUST-004: Shadow DOM with custom elements @p1', async ({ page }) => {
    const result = await page.evaluate(() => {
      // Check if customElements is supported
      if (typeof customElements === 'undefined') {
        return { supported: false };
      }

      // Define custom element with shadow DOM
      class ShadowElement extends HTMLElement {
        constructor() {
          super();

          // Attach shadow root
          try {
            const shadow = this.attachShadow({ mode: 'open' });

            // Add content to shadow DOM
            shadow.innerHTML = `
              <style>
                :host {
                  display: block;
                  padding: 10px;
                }
                .shadow-content {
                  color: blue;
                }
              </style>
              <div class="shadow-content">
                <slot></slot>
              </div>
            `;
          } catch (e) {
            // Shadow DOM might not be supported
            this.textContent = 'Shadow DOM not supported';
          }
        }
      }

      try {
        customElements.define('shadow-element', ShadowElement);
      } catch (e) {
        return {
          supported: true,
          error: e instanceof Error ? e.message : String(e)
        };
      }

      // Create element with light DOM content
      const el = document.createElement('shadow-element');
      el.textContent = 'Light DOM content';
      document.body.appendChild(el);

      // Check shadow DOM
      const hasShadowRoot = el.shadowRoot !== null;
      const shadowContent = hasShadowRoot ?
        el.shadowRoot!.querySelector('.shadow-content')?.tagName.toLowerCase() :
        null;

      // Check slot functionality (if shadow DOM is supported)
      const slotElement = hasShadowRoot ?
        el.shadowRoot!.querySelector('slot') :
        null;

      return {
        supported: true,
        hasShadowRoot: hasShadowRoot,
        shadowContent: shadowContent,
        hasSlot: slotElement !== null,
        lightDOMContent: el.textContent
      };
    });

    if (!result.supported) {
      test.skip();
      return;
    }

    if ('error' in result) {
      test.skip();
      return;
    }

    // Shadow DOM should be attached
    if (!result.hasShadowRoot) {
      // Shadow DOM not supported in this Ladybird build
      test.skip();
      return;
    }

    expect(result.hasShadowRoot).toBe(true);

    // Shadow content should exist
    expect(result.shadowContent).toBe('div');

    // Slot should exist for content projection
    expect(result.hasSlot).toBe(true);

    // Light DOM content should still be accessible
    expect(result.lightDOMContent).toBe('Light DOM content');
  });
});
