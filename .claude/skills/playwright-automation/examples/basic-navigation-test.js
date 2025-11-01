/**
 * Basic Navigation Test
 *
 * Demonstrates:
 * - Simple page navigation
 * - Waiting for content
 * - Accessibility snapshot assertions
 * - Screenshot capture
 *
 * Usage: Run this test to verify basic browser navigation works
 */

const TEST_NAME = "Basic Navigation Test";

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  try {
    // Step 1: Navigate to test page
    console.log("  Step 1: Navigating to example.com");
    await mcp__playwright__browser_navigate({
      url: "https://example.com"
    });

    // Step 2: Wait for page to load
    console.log("  Step 2: Waiting for page content");
    await mcp__playwright__browser_wait_for({
      text: "Example Domain"
    });

    // Step 3: Capture accessibility snapshot
    console.log("  Step 3: Capturing page snapshot");
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Step 4: Verify page content
    console.log("  Step 4: Verifying page content");
    const hasTitle = snapshot.includes("Example Domain");
    const hasLink = snapshot.includes("More information");

    if (!hasTitle) {
      throw new Error("Page title 'Example Domain' not found");
    }

    if (!hasLink) {
      throw new Error("Expected link 'More information' not found");
    }

    // Step 5: Take screenshot as evidence
    console.log("  Step 5: Capturing screenshot");
    await mcp__playwright__browser_take_screenshot({
      fullPage: true,
      filename: "basic-navigation-success.png"
    });

    // Step 6: Test navigation to link
    console.log("  Step 6: Testing link navigation");
    await mcp__playwright__browser_click({
      element: "More information link",
      ref: "a"
    });

    await mcp__playwright__browser_wait_for({ time: 1 });

    // Step 7: Verify we navigated
    const afterClick = await mcp__playwright__browser_snapshot({});
    const navigated = !afterClick.includes("Example Domain");

    console.log(`[${TEST_NAME}] PASSED`);
    return {
      status: "PASS",
      evidence: {
        screenshot: "basic-navigation-success.png",
        titleFound: hasTitle,
        linkFound: hasLink,
        navigationWorked: navigated
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);

    // Capture failure screenshot
    await mcp__playwright__browser_take_screenshot({
      fullPage: true,
      filename: "basic-navigation-FAILURE.png"
    });

    return {
      status: "FAIL",
      error: error.message,
      evidence: {
        screenshot: "basic-navigation-FAILURE.png"
      }
    };
  }
}

// Export for test runner
module.exports = { name: TEST_NAME, run };

// Allow standalone execution
if (require.main === module) {
  run().then(result => {
    console.log("\nTest Result:", JSON.stringify(result, null, 2));
    process.exit(result.status === "PASS" ? 0 : 1);
  });
}
