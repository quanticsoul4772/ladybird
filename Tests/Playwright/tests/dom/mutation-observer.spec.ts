import { test, expect } from '@playwright/test';

/**
 * MutationObserver Tests (MUT-001 to MUT-004)
 * Priority: P0 (Basic), P1 (Advanced)
 *
 * Tests MutationObserver API functionality including:
 * - childList mutations (add/remove children)
 * - Attribute changes (modify, add, remove attributes)
 * - CharacterData changes (text node modifications)
 * - Subtree observation and filtering
 */

test.describe('MutationObserver', () => {

  test('MUT-001: Observe childList mutations @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      return new Promise<{
        mutations: Array<{
          type: string;
          addedCount: number;
          removedCount: number;
          addedNodeTypes: string[];
          removedNodeTypes: string[];
        }>;
        finalChildCount: number;
      }>((resolve) => {
        // Create container div
        const container = document.createElement('div');
        container.id = 'container';
        document.body.appendChild(container);

        const mutations: Array<{
          type: string;
          addedCount: number;
          removedCount: number;
          addedNodeTypes: string[];
          removedNodeTypes: string[];
        }> = [];

        // Set up MutationObserver
        const observer = new MutationObserver((mutationsList) => {
          for (const mutation of mutationsList) {
            mutations.push({
              type: mutation.type,
              addedCount: mutation.addedNodes.length,
              removedCount: mutation.removedNodes.length,
              addedNodeTypes: Array.from(mutation.addedNodes).map(node => node.nodeName),
              removedNodeTypes: Array.from(mutation.removedNodes).map(node => node.nodeName),
            });
          }
        });

        // Observe childList changes
        observer.observe(container, { childList: true });

        // Add a child element
        const child1 = document.createElement('p');
        child1.id = 'child1';
        child1.textContent = 'First child';
        container.appendChild(child1);

        // Add another child element
        const child2 = document.createElement('span');
        child2.id = 'child2';
        child2.textContent = 'Second child';
        container.appendChild(child2);

        // Remove a child element
        container.removeChild(child1);

        // Wait for observer to process mutations
        setTimeout(() => {
          observer.disconnect();
          resolve({
            mutations,
            finalChildCount: container.children.length,
          });
        }, 100);
      });
    });

    // Verify mutations were recorded correctly
    expect(result.mutations.length).toBeGreaterThanOrEqual(3);

    // First mutation: added child1 (P element)
    expect(result.mutations[0].type).toBe('childList');
    expect(result.mutations[0].addedCount).toBe(1);
    expect(result.mutations[0].addedNodeTypes).toContain('P');
    expect(result.mutations[0].removedCount).toBe(0);

    // Second mutation: added child2 (SPAN element)
    expect(result.mutations[1].type).toBe('childList');
    expect(result.mutations[1].addedCount).toBe(1);
    expect(result.mutations[1].addedNodeTypes).toContain('SPAN');
    expect(result.mutations[1].removedCount).toBe(0);

    // Third mutation: removed child1 (P element)
    expect(result.mutations[2].type).toBe('childList');
    expect(result.mutations[2].addedCount).toBe(0);
    expect(result.mutations[2].removedCount).toBe(1);
    expect(result.mutations[2].removedNodeTypes).toContain('P');

    // Verify final state
    expect(result.finalChildCount).toBe(1);
  });

  test('MUT-002: Observe attribute changes @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      return new Promise<{
        mutations: Array<{
          type: string;
          attributeName: string | null;
          oldValue: string | null;
        }>;
        finalAttributes: {
          title: string | null;
          'data-test': string | null;
          class: string | null;
        };
      }>((resolve) => {
        // Create element with initial attributes
        const element = document.createElement('div');
        element.id = 'test-element';
        element.setAttribute('title', 'Initial Title');
        element.setAttribute('class', 'initial-class');
        document.body.appendChild(element);

        const mutations: Array<{
          type: string;
          attributeName: string | null;
          oldValue: string | null;
        }> = [];

        // Set up MutationObserver with attributeOldValue
        const observer = new MutationObserver((mutationsList) => {
          for (const mutation of mutationsList) {
            mutations.push({
              type: mutation.type,
              attributeName: mutation.attributeName,
              oldValue: mutation.oldValue,
            });
          }
        });

        // Observe attribute changes
        observer.observe(element, {
          attributes: true,
          attributeOldValue: true,
        });

        // Modify an existing attribute
        element.setAttribute('title', 'Modified Title');

        // Add a new attribute
        element.setAttribute('data-test', 'new-value');

        // Remove an attribute
        element.removeAttribute('class');

        // Modify another attribute
        element.setAttribute('data-test', 'updated-value');

        // Wait for observer to process mutations
        setTimeout(() => {
          observer.disconnect();
          resolve({
            mutations,
            finalAttributes: {
              title: element.getAttribute('title'),
              'data-test': element.getAttribute('data-test'),
              class: element.getAttribute('class'),
            },
          });
        }, 100);
      });
    });

    // Verify all mutations were recorded
    expect(result.mutations.length).toBe(4);

    // All should be attribute mutations
    result.mutations.forEach(mutation => {
      expect(mutation.type).toBe('attributes');
    });

    // First mutation: title modified (should have old value)
    expect(result.mutations[0].attributeName).toBe('title');
    expect(result.mutations[0].oldValue).toBe('Initial Title');

    // Second mutation: data-test added (old value should be null)
    expect(result.mutations[1].attributeName).toBe('data-test');
    expect(result.mutations[1].oldValue).toBeNull();

    // Third mutation: class removed (should have old value)
    expect(result.mutations[2].attributeName).toBe('class');
    expect(result.mutations[2].oldValue).toBe('initial-class');

    // Fourth mutation: data-test modified
    expect(result.mutations[3].attributeName).toBe('data-test');
    expect(result.mutations[3].oldValue).toBe('new-value');

    // Verify final state
    expect(result.finalAttributes.title).toBe('Modified Title');
    expect(result.finalAttributes['data-test']).toBe('updated-value');
    expect(result.finalAttributes.class).toBeNull();
  });

  test('MUT-003: Observe characterData changes @p0', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      return new Promise<{
        mutations: Array<{
          type: string;
          oldValue: string | null;
        }>;
        mutationCount: number;
        finalText: string;
      }>((resolve) => {
        // Create a text node
        const container = document.createElement('div');
        container.id = 'text-container';
        const textNode = document.createTextNode('Initial text content');
        container.appendChild(textNode);
        document.body.appendChild(container);

        const mutations: Array<{
          type: string;
          oldValue: string | null;
        }> = [];

        // Set up MutationObserver
        const observer = new MutationObserver((mutationsList) => {
          for (const mutation of mutationsList) {
            mutations.push({
              type: mutation.type,
              oldValue: mutation.oldValue,
            });
          }
        });

        // Observe characterData changes with oldValue tracking
        observer.observe(textNode, {
          characterData: true,
          characterDataOldValue: true,
        });

        // Modify text content
        textNode.textContent = 'Modified text content';

        // Modify again
        textNode.textContent = 'Final text content';

        // Wait for observer to process mutations
        setTimeout(() => {
          observer.disconnect();
          resolve({
            mutations,
            mutationCount: mutations.length,
            finalText: textNode.textContent || '',
          });
        }, 100);
      });
    });

    // Verify characterData mutations were recorded
    expect(result.mutationCount).toBe(2);

    // First mutation - should track old value
    expect(result.mutations[0].type).toBe('characterData');
    expect(result.mutations[0].oldValue).toBe('Initial text content');

    // Second mutation - should track old value
    expect(result.mutations[1].type).toBe('characterData');
    expect(result.mutations[1].oldValue).toBe('Modified text content');

    // Verify final state
    expect(result.finalText).toBe('Final text content');
  });

  test('MUT-004: Subtree observation and filtering @p1', async ({ page }) => {
    await page.goto('http://localhost:9080/');

    const result = await page.evaluate(() => {
      return new Promise<{
        allMutations: Array<{
          type: string;
          targetId: string;
          attributeName?: string | null;
        }>;
        filteredMutations: Array<{
          type: string;
          targetId: string;
          attributeName?: string | null;
        }>;
      }>((resolve) => {
        // Create nested DOM structure
        const root = document.createElement('div');
        root.id = 'root';
        document.body.appendChild(root);

        const level1 = document.createElement('div');
        level1.id = 'level1';
        root.appendChild(level1);

        const level2 = document.createElement('div');
        level2.id = 'level2';
        level1.appendChild(level2);

        const level3 = document.createElement('div');
        level3.id = 'level3';
        level2.appendChild(level3);

        const allMutations: Array<{
          type: string;
          targetId: string;
          attributeName?: string | null;
        }> = [];
        const filteredMutations: Array<{
          type: string;
          targetId: string;
          attributeName?: string | null;
        }> = [];

        // Observer 1: Watch subtree for all attribute changes
        const observerAll = new MutationObserver((mutationsList) => {
          for (const mutation of mutationsList) {
            allMutations.push({
              type: mutation.type,
              targetId: (mutation.target as HTMLElement).id,
              attributeName: mutation.attributeName,
            });
          }
        });

        // Observer 2: Watch subtree but filter only 'data-test' attribute
        const observerFiltered = new MutationObserver((mutationsList) => {
          for (const mutation of mutationsList) {
            filteredMutations.push({
              type: mutation.type,
              targetId: (mutation.target as HTMLElement).id,
              attributeName: mutation.attributeName,
            });
          }
        });

        // Observe with subtree: true
        observerAll.observe(root, {
          attributes: true,
          subtree: true,
        });

        // Observe with attributeFilter
        observerFiltered.observe(root, {
          attributes: true,
          subtree: true,
          attributeFilter: ['data-test'],
        });

        // Make changes at different levels
        root.setAttribute('class', 'root-class');
        root.setAttribute('data-test', 'root-value');

        level1.setAttribute('title', 'Level 1 Title');
        level1.setAttribute('data-test', 'level1-value');

        level2.setAttribute('aria-label', 'Level 2 Label');
        level2.setAttribute('data-test', 'level2-value');

        level3.setAttribute('style', 'color: red;');
        level3.setAttribute('data-test', 'level3-value');

        // Wait for observers to process mutations
        setTimeout(() => {
          observerAll.disconnect();
          observerFiltered.disconnect();
          resolve({
            allMutations,
            filteredMutations,
          });
        }, 100);
      });
    });

    // Verify subtree observation captured all changes
    expect(result.allMutations.length).toBe(8); // 8 attribute changes total

    // Verify all targets were observed
    const allTargets = result.allMutations.map(m => m.targetId);
    expect(allTargets).toContain('root');
    expect(allTargets).toContain('level1');
    expect(allTargets).toContain('level2');
    expect(allTargets).toContain('level3');

    // Verify all attribute names were captured
    const allAttributes = result.allMutations.map(m => m.attributeName);
    expect(allAttributes).toContain('class');
    expect(allAttributes).toContain('data-test');
    expect(allAttributes).toContain('title');
    expect(allAttributes).toContain('aria-label');
    expect(allAttributes).toContain('style');

    // Verify filtered observer only captured data-test attributes
    expect(result.filteredMutations.length).toBe(4); // Only 4 data-test changes

    result.filteredMutations.forEach(mutation => {
      expect(mutation.type).toBe('attributes');
      expect(mutation.attributeName).toBe('data-test');
    });

    // Verify filtered mutations captured all levels
    const filteredTargets = result.filteredMutations.map(m => m.targetId);
    expect(filteredTargets).toContain('root');
    expect(filteredTargets).toContain('level1');
    expect(filteredTargets).toContain('level2');
    expect(filteredTargets).toContain('level3');
  });

});
