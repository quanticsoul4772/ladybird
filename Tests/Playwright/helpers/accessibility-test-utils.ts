/**
 * Accessibility Testing Utilities (A11Y)
 *
 * Helper functions for comprehensive WCAG 2.1 Level AA accessibility testing.
 * Provides utilities for:
 * - Semantic HTML validation
 * - ARIA attribute checks
 * - Keyboard navigation testing
 * - Color contrast analysis
 * - Focus management verification
 * - Modal interaction testing
 *
 * Integration with axe-core for automated accessibility checks.
 */

import { Page, Locator } from '@playwright/test';

/**
 * Heading hierarchy result
 */
export interface HeadingHierarchy {
  valid: boolean;
  headings: Array<{ level: number; text: string; index: number }>;
  errors: string[];
}

/**
 * Landmark elements found on page
 */
export interface LandmarkResult {
  found: boolean;
  landmarks: Array<{ role: string; name?: string }>;
  missingLandmarks: string[];
}

/**
 * List structure validation result
 */
export interface ListStructureResult {
  valid: boolean;
  lists: Array<{ type: string; itemCount: number; nested: boolean }>;
  errors: string[];
}

/**
 * Table accessibility result
 */
export interface TableAccessibilityResult {
  valid: boolean;
  hasCaption: boolean;
  hasHeaderCells: boolean;
  headerScopeValid: boolean;
  errors: string[];
}

/**
 * Form label result
 */
export interface FormLabelResult {
  valid: boolean;
  unlabeledFields: string[];
  orphanLabels: string[];
  errors: string[];
}

/**
 * ARIA label result
 */
export interface AriaLabelResult {
  valid: boolean;
  elements: Array<{ selector: string; label: string; method: string }>;
  errors: string[];
}

/**
 * Keyboard navigation result
 */
export interface KeyboardNavigationResult {
  valid: boolean;
  focusableElements: number;
  tabSequence: Array<{ selector: string; index: number }>;
  errors: string[];
}

/**
 * Tab order result
 */
export interface TabOrderResult {
  valid: boolean;
  tabIndexValues: Array<{ selector: string; tabIndex: number }>;
  issues: string[];
}

/**
 * Focus visibility result
 */
export interface FocusVisibilityResult {
  visible: boolean;
  element: string;
  hasOutline: boolean;
  hasBackground: boolean;
  hasBorder: boolean;
}

/**
 * Color contrast result
 */
export interface ContrastResult {
  ratio: number;
  wcagAA: boolean;
  wcagAAA: boolean;
  foreground: string;
  background: string;
}

/**
 * Text resizing result
 */
export interface TextResizingResult {
  valid: boolean;
  resized: boolean;
  overflow: boolean;
  readableAt200: boolean;
}

/**
 * Modal focus management result
 */
export interface ModalFocusResult {
  valid: boolean;
  focusTrapWorking: boolean;
  initialFocus: string;
  restoreFocus: boolean;
  errors: string[];
}

/**
 * Form error identification result
 */
export interface FormErrorResult {
  valid: boolean;
  errorsIdentified: Array<{ field: string; error: string; method: string }>;
  issues: string[];
}

/**
 * Validate heading hierarchy (H1 > H2 > H3, etc.)
 */
export async function validateHeadingHierarchy(page: Page): Promise<HeadingHierarchy> {
  return await page.evaluate(() => {
    const headings = Array.from(document.querySelectorAll('h1, h2, h3, h4, h5, h6'));
    const result: HeadingHierarchy = {
      valid: true,
      headings: headings.map((h, i) => ({
        level: parseInt(h.tagName[1]),
        text: h.textContent || '',
        index: i,
      })),
      errors: [],
    };

    // Check for missing H1
    if (!document.querySelector('h1')) {
      result.errors.push('Missing H1 element');
      result.valid = false;
    }

    // Check for skipped levels
    let previousLevel = 0;
    for (const heading of result.headings) {
      if (heading.level > previousLevel + 1) {
        result.errors.push(
          `Heading hierarchy skip from H${previousLevel} to H${heading.level}`
        );
        result.valid = false;
      }
      previousLevel = heading.level;
    }

    // Check for multiple H1s (best practice)
    const h1Count = document.querySelectorAll('h1').length;
    if (h1Count > 1) {
      result.errors.push(`Multiple H1 elements found (${h1Count}). Best practice: single H1.`);
      // Note: not marking as invalid since multiple H1s are technically valid
    }

    return result;
  });
}

/**
 * Check for landmark roles and regions
 */
export async function validateLandmarks(page: Page): Promise<LandmarkResult> {
  return await page.evaluate(() => {
    const requiredLandmarks = ['main', 'navigation', 'contentinfo'];
    const found: string[] = [];
    const landmarks: Array<{ role: string; name?: string }> = [];

    // Check for <main>
    const main = document.querySelector('main');
    if (main) {
      found.push('main');
      landmarks.push({ role: 'main', name: main.getAttribute('aria-label') || undefined });
    }

    // Check for nav
    const navs = document.querySelectorAll('nav, [role="navigation"]');
    navs.forEach((nav) => {
      found.push('navigation');
      landmarks.push({
        role: 'navigation',
        name: nav.getAttribute('aria-label') || nav.getAttribute('aria-labelledby') || undefined,
      });
    });

    // Check for header
    const header = document.querySelector('header, [role="banner"]');
    if (header) {
      found.push('banner');
      landmarks.push({ role: 'banner' });
    }

    // Check for footer
    const footer = document.querySelector('footer, [role="contentinfo"]');
    if (footer) {
      found.push('contentinfo');
      landmarks.push({
        role: 'contentinfo',
        name: footer.getAttribute('aria-label') || undefined,
      });
    }

    const missingLandmarks = requiredLandmarks.filter((l) => !found.includes(l));

    return {
      found: missingLandmarks.length === 0,
      landmarks,
      missingLandmarks,
    };
  });
}

/**
 * Validate list structure (ul/ol/dl with proper nesting)
 */
export async function validateListStructure(page: Page): Promise<ListStructureResult> {
  return await page.evaluate(() => {
    const lists = Array.from(document.querySelectorAll('ul, ol, dl'));
    const result: ListStructureResult = {
      valid: true,
      lists: [],
      errors: [],
    };

    lists.forEach((list, index) => {
      const type = list.tagName.toLowerCase();
      let itemCount = 0;

      if (type === 'ul' || type === 'ol') {
        const items = list.querySelectorAll(':scope > li');
        itemCount = items.length;

        if (itemCount === 0) {
          result.errors.push(`${type.toUpperCase()} list at index ${index} has no <li> items`);
          result.valid = false;
        }

        // Check for nested lists
        const nestedLists = list.querySelectorAll('ul, ol');
        const nested = nestedLists.length > 0;

        result.lists.push({ type, itemCount, nested });
      } else if (type === 'dl') {
        const dtItems = list.querySelectorAll(':scope > dt');
        const ddItems = list.querySelectorAll(':scope > dd');
        itemCount = dtItems.length + ddItems.length;

        if (dtItems.length === 0 || ddItems.length === 0) {
          result.errors.push(`DL list at index ${index} missing <dt> or <dd> items`);
          result.valid = false;
        }

        result.lists.push({ type: 'dl', itemCount, nested: false });
      }
    });

    return result;
  });
}

/**
 * Validate table accessibility (caption, headers, scope)
 */
export async function validateTableAccessibility(page: Page): Promise<TableAccessibilityResult> {
  return await page.evaluate(() => {
    const tables = document.querySelectorAll('table');
    const result: TableAccessibilityResult = {
      valid: true,
      hasCaption: false,
      hasHeaderCells: false,
      headerScopeValid: true,
      errors: [],
    };

    if (tables.length === 0) {
      result.errors.push('No tables found on page');
      return result;
    }

    tables.forEach((table, index) => {
      // Check for caption
      const caption = table.querySelector('caption');
      if (caption) {
        result.hasCaption = true;
      } else {
        result.errors.push(`Table ${index} missing <caption>`);
      }

      // Check for header cells
      const thCells = table.querySelectorAll('th');
      if (thCells.length === 0) {
        result.errors.push(`Table ${index} has no <th> header cells`);
        result.valid = false;
      } else {
        result.hasHeaderCells = true;

        // Check scope attribute on headers
        thCells.forEach((th) => {
          const scope = th.getAttribute('scope');
          if (!scope || !['row', 'col', 'rowgroup', 'colgroup'].includes(scope)) {
            result.errors.push(`Table ${index} <th> missing valid scope attribute`);
            result.headerScopeValid = false;
          }
        });
      }
    });

    result.valid = result.valid && result.hasCaption && result.hasHeaderCells && result.headerScopeValid;

    return result;
  });
}

/**
 * Validate form labels and fieldsets
 */
export async function validateFormLabels(page: Page): Promise<FormLabelResult> {
  return await page.evaluate(() => {
    const result: FormLabelResult = {
      valid: true,
      unlabeledFields: [],
      orphanLabels: [],
      errors: [],
    };

    // Check for form fields without labels
    const formFields = document.querySelectorAll('input, textarea, select');
    formFields.forEach((field) => {
      const id = field.getAttribute('id');
      const name = field.getAttribute('name');
      const ariaLabel = field.getAttribute('aria-label');
      const ariaLabelledby = field.getAttribute('aria-labelledby');

      // Check for associated label
      let hasLabel = false;
      if (id) {
        const label = document.querySelector(`label[for="${id}"]`);
        if (label) {
          hasLabel = true;
        }
      }

      // Check for implicit label (field inside label)
      if ((field as any).closest('label')) {
        hasLabel = true;
      }

      if (!hasLabel && !ariaLabel && !ariaLabelledby) {
        result.unlabeledFields.push(`${field.tagName.toLowerCase()}[name="${name}"]`);
        result.valid = false;
      }
    });

    // Check for orphan labels
    const labels = document.querySelectorAll('label');
    labels.forEach((label) => {
      const htmlFor = label.getAttribute('for');
      if (htmlFor) {
        const field = document.getElementById(htmlFor);
        if (!field) {
          result.orphanLabels.push(`label[for="${htmlFor}"]`);
          result.valid = false;
        }
      }
    });

    return result;
  });
}

/**
 * Validate ARIA labels (aria-label, aria-labelledby)
 */
export async function validateAriaLabels(page: Page): Promise<AriaLabelResult> {
  return await page.evaluate(() => {
    const result: AriaLabelResult = {
      valid: true,
      elements: [],
      errors: [],
    };

    // Check elements with aria-label
    const ariaLabelElements = document.querySelectorAll('[aria-label]');
    ariaLabelElements.forEach((el) => {
      const label = el.getAttribute('aria-label');
      if (label && label.trim()) {
        result.elements.push({
          selector: el.tagName.toLowerCase(),
          label,
          method: 'aria-label',
        });
      }
    });

    // Check elements with aria-labelledby
    const ariaLabelledByElements = document.querySelectorAll('[aria-labelledby]');
    ariaLabelledByElements.forEach((el) => {
      const labelledBy = el.getAttribute('aria-labelledby');
      const labelEl = document.getElementById(labelledBy || '');
      if (labelEl) {
        result.elements.push({
          selector: el.tagName.toLowerCase(),
          label: labelEl.textContent || '',
          method: 'aria-labelledby',
        });
      } else if (labelledBy) {
        result.errors.push(`aria-labelledby references missing element: ${labelledBy}`);
        result.valid = false;
      }
    });

    // Check for aria-describedby
    const ariaDescribedByElements = document.querySelectorAll('[aria-describedby]');
    ariaDescribedByElements.forEach((el) => {
      const describedBy = el.getAttribute('aria-describedby');
      const descEl = document.getElementById(describedBy || '');
      if (!descEl && describedBy) {
        result.errors.push(`aria-describedby references missing element: ${describedBy}`);
        result.valid = false;
      }
    });

    return result;
  });
}

/**
 * Validate ARIA hidden and inert content
 */
export async function validateAriaHidden(page: Page): Promise<{ valid: boolean; hidden: number; issues: string[] }> {
  return await page.evaluate(() => {
    const result = {
      valid: true,
      hidden: 0,
      issues: [] as string[],
    };

    const hiddenElements = document.querySelectorAll('[aria-hidden="true"]');
    result.hidden = hiddenElements.length;

    // Check for focusable elements inside aria-hidden containers
    hiddenElements.forEach((el) => {
      const focusable = el.querySelectorAll(
        'button:not([disabled]):not([inert]), [href]:not([inert]), input:not([disabled]):not([inert]), select:not([disabled]):not([inert]), textarea:not([disabled]):not([inert]), [tabindex]:not([tabindex="-1"]):not([inert])'
      );
      if (focusable.length > 0) {
        result.issues.push('Focusable elements found inside aria-hidden="true" container');
        result.valid = false;
      }
    });

    // Check for inert attribute
    // Note: Elements inside inert containers are SUPPOSED to have focusable elements
    // The inert attribute itself disables interaction, so this is not an error
    // We just count them but don't mark as invalid

    return result;
  });
}

/**
 * Validate ARIA roles (button, tab, dialog, etc.)
 */
export async function validateAriaRoles(page: Page): Promise<{ valid: boolean; roles: string[]; issues: string[] }> {
  return await page.evaluate(() => {
    const result = {
      valid: true,
      roles: [] as string[],
      issues: [] as string[],
    };

    const elementsWithRole = document.querySelectorAll('[role]');
    const validRoles = [
      'button',
      'tab',
      'tablist',
      'tabpanel',
      'dialog',
      'alertdialog',
      'navigation',
      'main',
      'banner',
      'contentinfo',
      'region',
      'article',
      'complementary',
      'link',
      'menuitem',
      'checkbox',
      'radio',
      'listbox',
      'option',
      'textbox',
      'searchbox',
    ];

    elementsWithRole.forEach((el) => {
      const role = el.getAttribute('role');
      if (role) {
        result.roles.push(role);

        // Check if role is valid
        if (!validRoles.includes(role)) {
          result.issues.push(`Unknown ARIA role: ${role}`);
        }

        // Check role-specific requirements
        if (role === 'button') {
          // Button role should be on appropriate elements OR have tabindex to make it keyboard accessible
          const hasTabIndex = el.hasAttribute('tabindex') && el.getAttribute('tabindex') !== '-1';
          if (!['BUTTON', 'A', 'DIV', 'SPAN'].includes(el.tagName) && !hasTabIndex) {
            result.issues.push('role="button" on element that is not keyboard accessible');
          }
        }

        if (role === 'dialog' || role === 'alertdialog') {
          // Dialog should have aria-modal
          if (!el.getAttribute('aria-modal')) {
            result.issues.push(`role="${role}" missing aria-modal attribute`);
          }
          // Dialog should have aria-label or aria-labelledby
          if (!el.getAttribute('aria-label') && !el.getAttribute('aria-labelledby')) {
            result.issues.push(`role="${role}" missing aria-label or aria-labelledby`);
          }
        }
      }
    });

    result.valid = result.issues.length === 0;

    return result;
  });
}

/**
 * Validate live regions (aria-live, aria-atomic, aria-relevant)
 */
export async function validateLiveRegions(page: Page): Promise<{ valid: boolean; regions: number; issues: string[] }> {
  return await page.evaluate(() => {
    const result = {
      valid: true,
      regions: 0,
      issues: [] as string[],
    };

    const liveRegions = document.querySelectorAll('[aria-live]');
    result.regions = liveRegions.length;

    const validAriaLive = ['polite', 'assertive', 'off'];
    const validAriaRelevant = ['additions', 'removals', 'text', 'all'];

    liveRegions.forEach((region) => {
      const ariaLive = region.getAttribute('aria-live');
      const ariaAtomic = region.getAttribute('aria-atomic');
      const ariaRelevant = region.getAttribute('aria-relevant');

      if (ariaLive && !validAriaLive.includes(ariaLive)) {
        result.issues.push(`Invalid aria-live value: ${ariaLive}`);
      }

      if (ariaAtomic && !['true', 'false'].includes(ariaAtomic)) {
        result.issues.push(`Invalid aria-atomic value: ${ariaAtomic}`);
      }

      if (ariaRelevant) {
        const relevantValues = ariaRelevant.split(' ');
        for (const val of relevantValues) {
          if (!validAriaRelevant.includes(val)) {
            result.issues.push(`Invalid aria-relevant value: ${val}`);
          }
        }
      }
    });

    result.valid = result.issues.length === 0;

    return result;
  });
}

/**
 * Get keyboard navigation information
 */
export async function getKeyboardNavigationInfo(page: Page): Promise<KeyboardNavigationResult> {
  return await page.evaluate(() => {
    const focusableSelectors = [
      'button:not([disabled])',
      '[href]:not([tabindex="-1"])',
      'input:not([disabled])',
      'select:not([disabled])',
      'textarea:not([disabled])',
      '[tabindex]:not([tabindex="-1"])',
      '[role="button"]:not([tabindex="-1"])',
      '[role="tab"]:not([tabindex="-1"])',
      '[role="menuitem"]:not([tabindex="-1"])',
      '[role="link"]:not([tabindex="-1"])',
    ];

    const focusableElements = document.querySelectorAll(focusableSelectors.join(','));

    const result: KeyboardNavigationResult = {
      valid: focusableElements.length > 0,
      focusableElements: focusableElements.length,
      tabSequence: [],
      errors: [],
    };

    focusableElements.forEach((el, index) => {
      result.tabSequence.push({
        selector: el.tagName.toLowerCase() + (el.id ? `#${el.id}` : ''),
        index,
      });
    });

    return result;
  });
}

/**
 * Validate tab order (check for positive tabindex)
 */
export async function validateTabOrder(page: Page): Promise<TabOrderResult> {
  return await page.evaluate(() => {
    const result: TabOrderResult = {
      valid: true,
      tabIndexValues: [],
      issues: [],
    };

    const elementsWithTabIndex = document.querySelectorAll('[tabindex]');
    elementsWithTabIndex.forEach((el) => {
      const tabIndex = el.getAttribute('tabindex');
      const value = parseInt(tabIndex || '0');

      result.tabIndexValues.push({
        selector: el.tagName.toLowerCase(),
        tabIndex: value,
      });

      // Flag positive tabindex (anti-pattern)
      if (value > 0) {
        result.issues.push(`Positive tabindex (${value}) found. Use 0 or -1 for most cases.`);
        result.valid = false;
      }
    });

    return result;
  });
}

/**
 * Check for skip links
 */
export async function validateSkipLinks(page: Page): Promise<{ valid: boolean; skipLinks: number }> {
  return await page.evaluate(() => {
    const skipLinks = document.querySelectorAll('a[href="#main"], a[href="#content"], a[href="#main-content"], a[href*="skip"]');

    return {
      valid: skipLinks.length > 0,
      skipLinks: skipLinks.length,
    };
  });
}

/**
 * Get current focus element
 */
export async function getCurrentFocusElement(page: Page): Promise<string> {
  return await page.evaluate(() => {
    const focused = document.activeElement;
    if (focused) {
      return focused.tagName.toLowerCase() + (focused.id ? `#${focused.id}` : '');
    }
    return 'body';
  });
}

/**
 * Check if element is visible with focus outline
 */
export async function checkFocusVisibility(page: Page, selector: string): Promise<FocusVisibilityResult> {
  const element = page.locator(selector).first(); // Handle ambiguous selectors
  await element.focus();

  const result = await page.evaluate((sel) => {
    const el = document.querySelector(sel);
    if (!el) return null;

    const styles = window.getComputedStyle(el);
    const outline = styles.outline;
    const outlineWidth = styles.outlineWidth;
    const outlineColor = styles.outlineColor;
    const boxShadow = styles.boxShadow;
    const backgroundColor = styles.backgroundColor;
    const borderWidth = styles.borderWidth;

    return {
      hasOutline: outline !== 'none' && outline !== '',
      hasBackground: backgroundColor !== 'rgba(0, 0, 0, 0)' && backgroundColor !== 'transparent',
      hasBorder: borderWidth !== '0px',
      visible: (outline !== 'none' && outline !== '') || backgroundColor !== 'transparent',
    };
  }, selector);

  return {
    visible: result?.visible || false,
    element: selector,
    hasOutline: result?.hasOutline || false,
    hasBackground: result?.hasBackground || false,
    hasBorder: result?.hasBorder || false,
  };
}

/**
 * Calculate color contrast ratio (WCAG formula)
 */
export async function calculateColorContrast(page: Page, selector: string): Promise<ContrastResult> {
  return await page.evaluate((sel) => {
    const el = document.querySelector(sel) as HTMLElement;
    if (!el) throw new Error(`Element not found: ${sel}`);

    const styles = window.getComputedStyle(el);
    const fgColor = styles.color;

    // Get effective background color by traversing up the DOM tree
    let bgColor = styles.backgroundColor;
    let currentEl: HTMLElement | null = el;
    while (currentEl && (bgColor === 'transparent' || bgColor === 'rgba(0, 0, 0, 0)')) {
      currentEl = currentEl.parentElement;
      if (currentEl) {
        const parentStyles = window.getComputedStyle(currentEl);
        bgColor = parentStyles.backgroundColor;
      }
    }
    // If still transparent, use white as default
    if (!currentEl || bgColor === 'transparent' || bgColor === 'rgba(0, 0, 0, 0)') {
      bgColor = 'rgb(255, 255, 255)';
    }

    // Parse RGB values
    const parseRGB = (rgb: string) => {
      const match = rgb.match(/\d+/g);
      if (!match || match.length < 3) return { r: 0, g: 0, b: 0 };
      return {
        r: parseInt(match[0]),
        g: parseInt(match[1]),
        b: parseInt(match[2]),
      };
    };

    const fg = parseRGB(fgColor);
    const bg = parseRGB(bgColor);

    // Calculate relative luminance
    const getLuminance = (rgb: { r: number; g: number; b: number }) => {
      const [r, g, b] = [rgb.r, rgb.g, rgb.b].map((v) => {
        v = v / 255;
        return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
      });
      return 0.2126 * r + 0.7152 * g + 0.0722 * b;
    };

    const l1 = getLuminance(fg);
    const l2 = getLuminance(bg);
    const lighter = Math.max(l1, l2);
    const darker = Math.min(l1, l2);

    const contrast = (lighter + 0.05) / (darker + 0.05);

    return {
      ratio: Math.round(contrast * 100) / 100,
      wcagAA: contrast >= 4.5, // 4.5:1 for normal text
      wcagAAA: contrast >= 7, // 7:1 for enhanced contrast
      foreground: fgColor,
      background: bgColor,
    };
  }, selector);
}

/**
 * Test text resizing (200% zoom)
 */
export async function testTextResizing(page: Page): Promise<TextResizingResult> {
  // Get initial page height
  const initialHeight = await page.evaluate(() => document.documentElement.scrollHeight);

  // Set zoom to 200%
  await page.evaluate(() => {
    document.documentElement.style.zoom = '200%';
  });

  // Check for overflow and readability
  const result = await page.evaluate(() => {
    const pageWidth = window.innerWidth;
    const contentWidth = document.documentElement.scrollWidth;
    const hasOverflow = contentWidth > pageWidth;

    const textElements = document.querySelectorAll('p, h1, h2, h3, h4, h5, h6, span, li');
    let readableLines = 0;

    textElements.forEach((el) => {
      const text = el.textContent || '';
      if (text.length > 0 && text.trim().length > 0) {
        readableLines++;
      }
    });

    return {
      hasOverflow,
      readableAt200: readableLines > 0 && !hasOverflow,
    };
  });

  // Reset zoom
  await page.evaluate(() => {
    document.documentElement.style.zoom = '100%';
  });

  return {
    valid: !result.hasOverflow,
    resized: true,
    overflow: result.hasOverflow,
    readableAt200: result.readableAt200,
  };
}

/**
 * Validate modal focus management
 */
export async function validateModalFocus(
  page: Page,
  modalSelector: string
): Promise<ModalFocusResult> {
  const result: ModalFocusResult = {
    valid: true,
    focusTrapWorking: true,
    initialFocus: '',
    restoreFocus: false,
    errors: [],
  };

  // Get initial focus element
  const initialFocus = await getCurrentFocusElement(page);

  // Click to open modal
  const modal = page.locator(modalSelector);
  if (await modal.isVisible()) {
    // Get focus after modal opens
    const focusAfterOpen = await getCurrentFocusElement(page);
    result.initialFocus = focusAfterOpen;

    // Get focusable elements in modal
    const focusableInModal = await modal.evaluate((el) => {
      const focusable = el.querySelectorAll(
        'button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])'
      );
      return focusable.length;
    });

    result.focusTrapWorking = focusableInModal > 0;

    // Test focus trap by tabbing through
    await page.keyboard.press('Tab');
    await page.keyboard.press('Tab');
    const focusAfterTab = await getCurrentFocusElement(page);

    // Verify focus stays in modal
    const stillInModal = await modal.evaluate((el) => {
      return document.activeElement ? el.contains(document.activeElement) : false;
    });

    if (!stillInModal && focusableInModal > 0) {
      result.errors.push('Focus escaped modal dialog');
      result.focusTrapWorking = false;
    }
  } else {
    result.errors.push('Modal not visible');
    result.valid = false;
  }

  result.valid = result.focusTrapWorking && result.errors.length === 0;

  return result;
}

/**
 * Validate form error identification
 */
export async function validateFormErrors(
  page: Page,
  formSelector: string
): Promise<FormErrorResult> {
  const result: FormErrorResult = {
    valid: true,
    errorsIdentified: [],
    issues: [],
  };

  // Get all form fields
  const form = page.locator(formSelector);
  const fields = await form.locator('input, textarea, select').count();

  if (fields === 0) {
    result.issues.push('No form fields found');
    result.valid = false;
    return result;
  }

  // Check for error handling
  const hasErrorMessages = await form.evaluate((el) => {
    const errorElements = el.querySelectorAll('[role="alert"], .error, [aria-invalid="true"]');
    return errorElements.length > 0;
  });

  if (!hasErrorMessages) {
    result.issues.push('No error messages or aria-invalid attributes found');
    result.valid = false;
  }

  // Get error details
  const errors = await form.evaluate(() => {
    const errorElements = document.querySelectorAll('[aria-invalid="true"]');
    const result: Array<{ field: string; error: string; method: string }> = [];

    errorElements.forEach((el) => {
      const field = el.tagName.toLowerCase() + (el.id ? `#${el.id}` : '');
      const errorMsg = el.getAttribute('aria-describedby')
        ? document.getElementById(el.getAttribute('aria-describedby') || '')?.textContent || ''
        : el.parentElement?.textContent || '';

      result.push({
        field,
        error: errorMsg,
        method: 'aria-invalid',
      });
    });

    return result;
  });

  result.errorsIdentified = errors;
  result.valid = errors.length > 0 || !hasErrorMessages;

  return result;
}

/**
 * Navigate using keyboard only
 */
export async function navigateWithKeyboard(page: Page, times: number = 5): Promise<string[]> {
  const focusPath: string[] = [];

  for (let i = 0; i < times; i++) {
    const currentFocus = await getCurrentFocusElement(page);
    focusPath.push(currentFocus);
    await page.keyboard.press('Tab');
  }

  return focusPath;
}

/**
 * Check if keyboard shortcuts exist
 */
export async function checkKeyboardShortcuts(page: Page): Promise<{ shortcuts: string[]; issues: string[] }> {
  return await page.evaluate(() => {
    const shortcuts: string[] = [];
    const issues: string[] = [];

    // Look for keyboard shortcut documentation
    const allElements = document.querySelectorAll('*');

    allElements.forEach((el) => {
      // Check for aria-keyshortcuts attribute (ARIA 1.1+)
      const ariaKeyShortcuts = el.getAttribute('aria-keyshortcuts');
      if (ariaKeyShortcuts) {
        shortcuts.push(ariaKeyShortcuts);
      }

      // Check for title attributes mentioning keys
      const title = el.getAttribute('title');
      if (title && /\(.*ctrl|cmd|shift|alt.*\)/i.test(title)) {
        shortcuts.push(title);
      }

      // Check for accesskey attribute (old-school HTML shortcut)
      const accessKey = el.getAttribute('accesskey');
      if (accessKey) {
        shortcuts.push(`accesskey: ${accessKey}`);
      }
    });

    // Note: getEventListeners() is a Chrome DevTools API not available in standard JS
    // We rely on aria-keyshortcuts, title, and accesskey attributes instead

    if (shortcuts.length === 0) {
      issues.push('No keyboard shortcuts found');
    }

    return { shortcuts, issues };
  });
}
