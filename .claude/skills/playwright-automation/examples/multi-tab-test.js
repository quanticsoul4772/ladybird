/**
 * Multi-Tab Test
 *
 * Demonstrates:
 * - Creating multiple tabs
 * - Switching between tabs
 * - Tab isolation
 * - Tab management
 *
 * Usage: Run this test to verify multi-tab functionality
 * Particularly important for Ladybird's multi-process architecture
 */

const TEST_NAME = "Multi-Tab Test";

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  try {
    // Step 1: Navigate in first tab
    console.log("  Step 1: Loading page in first tab");
    await mcp__playwright__browser_navigate({
      url: "https://example.com"
    });

    await mcp__playwright__browser_wait_for({
      text: "Example Domain"
    });

    const tab1Snapshot = await mcp__playwright__browser_snapshot({});

    if (!tab1Snapshot.includes("Example Domain")) {
      throw new Error("Tab 1 content not loaded");
    }

    await mcp__playwright__browser_take_screenshot({
      filename: "tab1-initial.png"
    });

    // Step 2: Create second tab
    console.log("  Step 2: Creating second tab");
    await mcp__playwright__browser_tabs({ action: "new" });

    // Step 3: Navigate in second tab
    console.log("  Step 3: Loading different page in second tab");
    await mcp__playwright__browser_navigate({
      url: "https://www.iana.org"
    });

    await mcp__playwright__browser_wait_for({ time: 2 });

    const tab2Snapshot = await mcp__playwright__browser_snapshot({});

    await mcp__playwright__browser_take_screenshot({
      filename: "tab2-initial.png"
    });

    // Step 4: Create third tab
    console.log("  Step 4: Creating third tab");
    await mcp__playwright__browser_tabs({ action: "new" });

    await mcp__playwright__browser_navigate({
      url: "data:text/html,<h1>Tab 3 Content</h1>"
    });

    await mcp__playwright__browser_wait_for({
      text: "Tab 3 Content"
    });

    // Step 5: List all tabs
    console.log("  Step 5: Listing all tabs");
    const tabs = await mcp__playwright__browser_tabs({ action: "list" });

    if (tabs.length !== 3) {
      throw new Error(`Expected 3 tabs, found ${tabs.length}`);
    }

    // Step 6: Switch to first tab
    console.log("  Step 6: Switching back to first tab");
    await mcp__playwright__browser_tabs({ action: "select", index: 0 });

    const backToTab1 = await mcp__playwright__browser_snapshot({});

    if (!backToTab1.includes("Example Domain")) {
      throw new Error("Tab 1 content lost after switching");
    }

    // Step 7: Switch to second tab
    console.log("  Step 7: Switching to second tab");
    await mcp__playwright__browser_tabs({ action: "select", index: 1 });

    const backToTab2 = await mcp__playwright__browser_snapshot({});

    // Step 8: Test tab isolation (important for Ladybird's process-per-tab)
    console.log("  Step 8: Testing tab isolation");

    // Execute script in tab 2
    await mcp__playwright__browser_evaluate({
      function: "() => { window.testVariable = 'tab2-value'; return window.testVariable; }"
    });

    // Switch to tab 3
    await mcp__playwright__browser_tabs({ action: "select", index: 2 });

    // Check if variable leaked (it shouldn't)
    const isolationTest = await mcp__playwright__browser_evaluate({
      function: "() => typeof window.testVariable"
    });

    const tabsIsolated = isolationTest === "undefined";

    if (!tabsIsolated) {
      throw new Error("Tab isolation failed: variables leaked between tabs");
    }

    // Step 9: Close middle tab
    console.log("  Step 9: Closing second tab");
    await mcp__playwright__browser_tabs({ action: "close", index: 1 });

    const tabsAfterClose = await mcp__playwright__browser_tabs({ action: "list" });

    if (tabsAfterClose.length !== 2) {
      throw new Error(`Expected 2 tabs after close, found ${tabsAfterClose.length}`);
    }

    // Step 10: Verify remaining tabs still work
    console.log("  Step 10: Verifying remaining tabs");
    await mcp__playwright__browser_tabs({ action: "select", index: 0 });

    const finalTab1 = await mcp__playwright__browser_snapshot({});

    if (!finalTab1.includes("Example Domain")) {
      throw new Error("Tab 1 corrupted after closing tab 2");
    }

    await mcp__playwright__browser_take_screenshot({
      filename: "multi-tab-final.png"
    });

    console.log(`[${TEST_NAME}] PASSED`);
    return {
      status: "PASS",
      evidence: {
        screenshots: ["tab1-initial.png", "tab2-initial.png", "multi-tab-final.png"],
        totalTabsCreated: 3,
        tabsAfterClose: 2,
        tabIsolation: tabsIsolated
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);

    await mcp__playwright__browser_take_screenshot({
      filename: "multi-tab-FAILURE.png"
    });

    return {
      status: "FAIL",
      error: error.message,
      evidence: {
        screenshot: "multi-tab-FAILURE.png"
      }
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
