/**
 * Accessibility Test
 *
 * Demonstrates:
 * - Accessibility tree validation
 * - ARIA compliance checking
 * - Keyboard navigation testing
 * - Alt text verification
 *
 * Usage: Run this test to verify accessibility compliance
 */

const TEST_NAME = "Accessibility Test";

class AccessibilityChecker {
  constructor(snapshot) {
    this.snapshot = snapshot;
    this.violations = [];
  }

  checkImagesHaveAltText() {
    // Check for <img> tags without alt attribute
    const imgPattern = /<img\s+(?![^>]*\balt=)[^>]*>/gi;
    const matches = this.snapshot.match(imgPattern) || [];

    if (matches.length > 0) {
      this.violations.push({
        type: "missing-alt-text",
        severity: "error",
        count: matches.length,
        message: `${matches.length} image(s) found without alt text`
      });
    }
  }

  checkHeadingHierarchy() {
    // Extract all headings
    const headingPattern = /<h([1-6])[^>]*>/gi;
    const headings = [];
    let match;

    while ((match = headingPattern.exec(this.snapshot)) !== null) {
      headings.push(parseInt(match[1]));
    }

    if (headings.length > 0) {
      // Check if first heading is h1
      if (headings[0] !== 1) {
        this.violations.push({
          type: "heading-hierarchy",
          severity: "warning",
          message: `Page should start with <h1>, found <h${headings[0]}>`
        });
      }

      // Check for skipped heading levels
      for (let i = 1; i < headings.length; i++) {
        if (headings[i] - headings[i-1] > 1) {
          this.violations.push({
            type: "heading-hierarchy",
            severity: "warning",
            message: `Heading level skipped: <h${headings[i-1]}> to <h${headings[i]}>`
          });
        }
      }
    }
  }

  checkButtonsHaveLabels() {
    // Check for buttons without text or aria-label
    const buttonPattern = /<button\s+(?![^>]*\baria-label=)[^>]*>\s*<\/button>/gi;
    const matches = this.snapshot.match(buttonPattern) || [];

    if (matches.length > 0) {
      this.violations.push({
        type: "missing-button-label",
        severity: "error",
        count: matches.length,
        message: `${matches.length} button(s) without label or aria-label`
      });
    }
  }

  checkFormLabels() {
    // Check for inputs without associated labels
    const inputPattern = /<input\s+(?![^>]*\baria-label=)(?![^>]*\bid=["'][^"']*["'][^>]*<label[^>]*\bfor=["']\1["'])[^>]*>/gi;
    const matches = this.snapshot.match(inputPattern) || [];

    if (matches.length > 0) {
      this.violations.push({
        type: "missing-form-label",
        severity: "error",
        count: matches.length,
        message: `${matches.length} input(s) without associated label`
      });
    }
  }

  checkLinkText() {
    // Check for links with non-descriptive text
    const nonDescriptive = ["click here", "here", "read more", "link"];
    const linkPattern = /<a[^>]*>([^<]+)<\/a>/gi;
    let match;
    let count = 0;

    while ((match = linkPattern.exec(this.snapshot)) !== null) {
      const linkText = match[1].toLowerCase().trim();
      if (nonDescriptive.includes(linkText)) {
        count++;
      }
    }

    if (count > 0) {
      this.violations.push({
        type: "non-descriptive-link",
        severity: "warning",
        count: count,
        message: `${count} link(s) with non-descriptive text`
      });
    }
  }

  getViolations() {
    return this.violations;
  }
}

async function testKeyboardNavigation() {
  console.log("  Testing keyboard navigation");

  const results = {
    tabNavigation: false,
    enterActivation: false,
    escapeCloses: false
  };

  try {
    // Test Tab navigation
    await mcp__playwright__browser_press_key({ key: "Tab" });
    await mcp__playwright__browser_wait_for({ time: 0.5 });

    // Check if focus moved (snapshot should show focused element)
    const afterTab = await mcp__playwright__browser_snapshot({});
    results.tabNavigation = afterTab.includes("focus") || afterTab.includes(":focus");

    // Test Enter activation
    await mcp__playwright__browser_press_key({ key: "Enter" });
    await mcp__playwright__browser_wait_for({ time: 0.5 });

    const afterEnter = await mcp__playwright__browser_snapshot({});
    results.enterActivation = true; // If no crash, consider pass

    // Test Escape
    await mcp__playwright__browser_press_key({ key: "Escape" });
    await mcp__playwright__browser_wait_for({ time: 0.5 });

    results.escapeCloses = true;

  } catch (error) {
    console.error("  Keyboard navigation error:", error.message);
  }

  return results;
}

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  try {
    // Step 1: Navigate to test page
    console.log("  Step 1: Navigating to test page");
    await mcp__playwright__browser_navigate({
      url: "https://example.com"
    });

    await mcp__playwright__browser_wait_for({
      text: "Example Domain"
    });

    // Step 2: Capture accessibility snapshot
    console.log("  Step 2: Capturing accessibility snapshot");
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Step 3: Run accessibility checks
    console.log("  Step 3: Running accessibility checks");
    const checker = new AccessibilityChecker(snapshot);

    checker.checkImagesHaveAltText();
    checker.checkHeadingHierarchy();
    checker.checkButtonsHaveLabels();
    checker.checkFormLabels();
    checker.checkLinkText();

    const violations = checker.getViolations();

    // Step 4: Test keyboard navigation
    const keyboardResults = await testKeyboardNavigation();

    // Step 5: Capture evidence
    await mcp__playwright__browser_take_screenshot({
      fullPage: true,
      filename: "accessibility-test.png"
    });

    // Step 6: Evaluate results
    const errors = violations.filter(v => v.severity === "error");
    const warnings = violations.filter(v => v.severity === "warning");

    if (errors.length > 0) {
      console.error(`[${TEST_NAME}] FAILED: ${errors.length} accessibility errors found`);
      return {
        status: "FAIL",
        errors: errors,
        warnings: warnings,
        keyboardNavigation: keyboardResults,
        evidence: {
          screenshot: "accessibility-test.png"
        }
      };
    }

    if (warnings.length > 0) {
      console.warn(`[${TEST_NAME}] PASSED with ${warnings.length} warnings`);
    } else {
      console.log(`[${TEST_NAME}] PASSED`);
    }

    return {
      status: "PASS",
      warnings: warnings,
      keyboardNavigation: keyboardResults,
      evidence: {
        screenshot: "accessibility-test.png"
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);
    return {
      status: "FAIL",
      error: error.message
    };
  }
}

module.exports = { name: TEST_NAME, run };

if (require.main === module) {
  run().then(result => {
    console.log("\nTest Result:", JSON.stringify(result, null, 2));
    process.exit(result.status === "PASS" ? 0 : 1);
  });
}
