/**
 * [Test Name]
 *
 * Demonstrates:
 * - [Feature 1]
 * - [Feature 2]
 * - [Feature 3]
 *
 * Usage: [When to run this test]
 */

const TEST_NAME = "[Test Name]";

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  try {
    // Step 1: Setup
    console.log("  Step 1: [Description]");
    await mcp__playwright__browser_navigate({
      url: "[URL]"
    });

    await mcp__playwright__browser_wait_for({
      text: "[Expected content]"
    });

    // Step 2: Action
    console.log("  Step 2: [Description]");
    // ... your test actions here

    // Step 3: Verify
    console.log("  Step 3: [Description]");
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Add your assertions here
    if (!snapshot.includes("[Expected content]")) {
      throw new Error("[Assertion failed message]");
    }

    // Step 4: Capture evidence
    await mcp__playwright__browser_take_screenshot({
      filename: "[test-name]-success.png"
    });

    console.log(`[${TEST_NAME}] PASSED`);
    return {
      status: "PASS",
      evidence: {
        screenshot: "[test-name]-success.png"
        // Add other evidence here
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);

    // Capture failure state
    await mcp__playwright__browser_take_screenshot({
      filename: "[test-name]-FAILURE.png"
    });

    const consoleErrors = await mcp__playwright__browser_console_messages({
      onlyErrors: true
    });

    return {
      status: "FAIL",
      error: error.message,
      evidence: {
        screenshot: "[test-name]-FAILURE.png",
        consoleErrors: consoleErrors
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
