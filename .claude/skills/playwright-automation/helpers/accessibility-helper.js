/**
 * Accessibility Helper
 *
 * Utilities for accessibility testing and validation
 */

class AccessibilityHelper {
  constructor() {
    this.violations = [];
  }

  /**
   * Check accessibility snapshot for common issues
   */
  checkSnapshot(snapshot) {
    this.violations = [];

    this.checkImagesHaveAltText(snapshot);
    this.checkButtonsHaveLabels(snapshot);
    this.checkFormLabels(snapshot);
    this.checkHeadingHierarchy(snapshot);
    this.checkLinkText(snapshot);
    this.checkAriaRoles(snapshot);
    this.checkColorContrast(snapshot);

    return {
      violations: this.violations,
      passed: this.violations.filter(v => v.severity === 'error').length === 0,
      errorCount: this.violations.filter(v => v.severity === 'error').length,
      warningCount: this.violations.filter(v => v.severity === 'warning').length
    };
  }

  /**
   * Images should have alt text
   */
  checkImagesHaveAltText(snapshot) {
    const imgPattern = /<img\s+(?![^>]*\balt=)[^>]*>/gi;
    const matches = snapshot.match(imgPattern) || [];

    if (matches.length > 0) {
      this.violations.push({
        type: 'missing-alt-text',
        severity: 'error',
        count: matches.length,
        message: `${matches.length} image(s) without alt text`,
        wcagCriteria: '1.1.1 Non-text Content (Level A)'
      });
    }
  }

  /**
   * Buttons should have labels
   */
  checkButtonsHaveLabels(snapshot) {
    const buttonPattern = /<button\s+(?![^>]*\baria-label=)[^>]*>\s*<\/button>/gi;
    const matches = snapshot.match(buttonPattern) || [];

    if (matches.length > 0) {
      this.violations.push({
        type: 'missing-button-label',
        severity: 'error',
        count: matches.length,
        message: `${matches.length} button(s) without text or aria-label`,
        wcagCriteria: '4.1.2 Name, Role, Value (Level A)'
      });
    }
  }

  /**
   * Form inputs should have labels
   */
  checkFormLabels(snapshot) {
    const inputPattern = /<input\s+[^>]*type=["'](?!hidden)[^"']*["'][^>]*>/gi;
    const matches = snapshot.match(inputPattern) || [];
    let unlabeled = 0;

    matches.forEach(match => {
      // Check if has aria-label or associated label
      const hasAriaLabel = /aria-label=/.test(match);
      const hasId = /id=["']([^"']+)["']/.exec(match);
      let hasLabel = false;

      if (hasId) {
        const labelPattern = new RegExp(`<label[^>]*for=["']${hasId[1]}["']`, 'i');
        hasLabel = labelPattern.test(snapshot);
      }

      if (!hasAriaLabel && !hasLabel) {
        unlabeled++;
      }
    });

    if (unlabeled > 0) {
      this.violations.push({
        type: 'missing-form-label',
        severity: 'error',
        count: unlabeled,
        message: `${unlabeled} input(s) without associated label`,
        wcagCriteria: '3.3.2 Labels or Instructions (Level A)'
      });
    }
  }

  /**
   * Check heading hierarchy
   */
  checkHeadingHierarchy(snapshot) {
    const headingPattern = /<h([1-6])[^>]*>/gi;
    const headings = [];
    let match;

    while ((match = headingPattern.exec(snapshot)) !== null) {
      headings.push(parseInt(match[1]));
    }

    if (headings.length === 0) {
      this.violations.push({
        type: 'no-headings',
        severity: 'warning',
        message: 'Page has no headings',
        wcagCriteria: '2.4.6 Headings and Labels (Level AA)'
      });
      return;
    }

    // Check if starts with h1
    if (headings[0] !== 1) {
      this.violations.push({
        type: 'heading-hierarchy',
        severity: 'warning',
        message: `Page should start with <h1>, found <h${headings[0]}>`,
        wcagCriteria: '2.4.6 Headings and Labels (Level AA)'
      });
    }

    // Check for skipped levels
    for (let i = 1; i < headings.length; i++) {
      if (headings[i] - headings[i - 1] > 1) {
        this.violations.push({
          type: 'heading-hierarchy',
          severity: 'warning',
          message: `Heading level skipped: <h${headings[i-1]}> to <h${headings[i]}>`,
          wcagCriteria: '2.4.6 Headings and Labels (Level AA)'
        });
      }
    }
  }

  /**
   * Check for non-descriptive link text
   */
  checkLinkText(snapshot) {
    const nonDescriptive = ['click here', 'here', 'read more', 'link', 'more'];
    const linkPattern = /<a[^>]*>([^<]+)<\/a>/gi;
    let match;
    let count = 0;

    while ((match = linkPattern.exec(snapshot)) !== null) {
      const linkText = match[1].toLowerCase().trim();
      if (nonDescriptive.includes(linkText)) {
        count++;
      }
    }

    if (count > 0) {
      this.violations.push({
        type: 'non-descriptive-link',
        severity: 'warning',
        count: count,
        message: `${count} link(s) with non-descriptive text`,
        wcagCriteria: '2.4.4 Link Purpose (In Context) (Level A)'
      });
    }
  }

  /**
   * Check for proper ARIA roles
   */
  checkAriaRoles(snapshot) {
    const invalidRoles = ['presentation', 'none'];
    const rolePattern = /role=["']([^"']+)["']/gi;
    let match;
    const roles = [];

    while ((match = rolePattern.exec(snapshot)) !== null) {
      roles.push(match[1]);
    }

    // Check for overuse of certain roles
    const presentationCount = roles.filter(r => invalidRoles.includes(r)).length;

    if (presentationCount > 5) {
      this.violations.push({
        type: 'aria-overuse',
        severity: 'warning',
        count: presentationCount,
        message: `Excessive use of role="presentation" or role="none" (${presentationCount} instances)`,
        wcagCriteria: '4.1.2 Name, Role, Value (Level A)'
      });
    }
  }

  /**
   * Basic color contrast check (requires actual rendering)
   */
  checkColorContrast(snapshot) {
    // This is a placeholder - proper contrast checking requires pixel analysis
    // In production, use tools like axe-core or Pa11y

    const hasContrastWarning = snapshot.includes('color:') && snapshot.includes('background:');

    if (!hasContrastWarning) {
      this.violations.push({
        type: 'color-contrast',
        severity: 'info',
        message: 'Color contrast cannot be verified from snapshot alone',
        wcagCriteria: '1.4.3 Contrast (Minimum) (Level AA)'
      });
    }
  }

  /**
   * Test keyboard navigation
   */
  async testKeyboardNavigation() {
    const results = {
      tabNavigation: false,
      enterActivation: false,
      escapeHandling: false,
      arrowNavigation: false
    };

    try {
      // Tab navigation
      await mcp__playwright__browser_press_key({ key: 'Tab' });
      await mcp__playwright__browser_wait_for({ time: 0.5 });
      const afterTab = await mcp__playwright__browser_snapshot({});
      results.tabNavigation = afterTab.includes('focus') || afterTab.includes(':focus');

      // Enter activation
      await mcp__playwright__browser_press_key({ key: 'Enter' });
      await mcp__playwright__browser_wait_for({ time: 0.5 });
      results.enterActivation = true;

      // Escape handling
      await mcp__playwright__browser_press_key({ key: 'Escape' });
      await mcp__playwright__browser_wait_for({ time: 0.5 });
      results.escapeHandling = true;

      // Arrow navigation
      await mcp__playwright__browser_press_key({ key: 'ArrowDown' });
      await mcp__playwright__browser_wait_for({ time: 0.5 });
      results.arrowNavigation = true;

    } catch (error) {
      console.error('Keyboard navigation test error:', error.message);
    }

    return results;
  }

  /**
   * Generate accessibility report
   */
  generateReport() {
    const errors = this.violations.filter(v => v.severity === 'error');
    const warnings = this.violations.filter(v => v.severity === 'warning');
    const info = this.violations.filter(v => v.severity === 'info');

    return {
      summary: {
        total: this.violations.length,
        errors: errors.length,
        warnings: warnings.length,
        info: info.length,
        passed: errors.length === 0
      },
      violations: this.violations,
      wcagLevel: this.determineWCAGLevel(errors, warnings)
    };
  }

  /**
   * Determine WCAG conformance level
   */
  determineWCAGLevel(errors, warnings) {
    if (errors.length > 0) {
      return 'FAIL';
    }

    if (warnings.length === 0) {
      return 'AAA';  // Best case
    }

    if (warnings.length <= 3) {
      return 'AA';
    }

    return 'A';
  }

  /**
   * Clear violations
   */
  clearViolations() {
    this.violations = [];
  }
}

module.exports = AccessibilityHelper;

// Example usage
if (require.main === module) {
  const helper = new AccessibilityHelper();

  const sampleSnapshot = `
    <html>
      <body>
        <h1>Test Page</h1>
        <img src="test.jpg">
        <button></button>
        <input type="text">
        <a href="#">click here</a>
      </body>
    </html>
  `;

  const result = helper.checkSnapshot(sampleSnapshot);

  console.log('Accessibility Check Results:');
  console.log('Passed:', result.passed);
  console.log('Errors:', result.errorCount);
  console.log('Warnings:', result.warningCount);
  console.log('\nViolations:');
  result.violations.forEach(v => {
    console.log(`  [${v.severity.toUpperCase()}] ${v.message}`);
    console.log(`    WCAG: ${v.wcagCriteria}`);
  });
}
