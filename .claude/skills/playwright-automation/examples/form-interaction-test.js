/**
 * Form Interaction Test
 *
 * Demonstrates:
 * - Filling form fields
 * - Checkbox and dropdown interactions
 * - Form submission
 * - Result validation
 *
 * Usage: Run this test to verify form handling works correctly
 */

const TEST_NAME = "Form Interaction Test";

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  try {
    // Step 1: Navigate to form page
    console.log("  Step 1: Navigating to form test page");
    await mcp__playwright__browser_navigate({
      url: "file:///home/rbsmith4/ladybird/Tests/Playwright/fixtures/test-form.html"
    });

    // Step 2: Wait for form to be ready
    await mcp__playwright__browser_wait_for({
      text: "Test Form"
    });

    // Step 3: Capture initial state
    await mcp__playwright__browser_take_screenshot({
      filename: "form-initial.png"
    });

    // Step 4: Fill form using fill_form (recommended for multiple fields)
    console.log("  Step 2: Filling form fields");
    await mcp__playwright__browser_fill_form({
      fields: [
        {
          name: "First Name",
          type: "textbox",
          ref: "input[name='firstName']",
          value: "John"
        },
        {
          name: "Last Name",
          type: "textbox",
          ref: "input[name='lastName']",
          value: "Doe"
        },
        {
          name: "Email",
          type: "textbox",
          ref: "input[name='email']",
          value: "john.doe@example.com"
        },
        {
          name: "Subscribe to newsletter",
          type: "checkbox",
          ref: "input[name='newsletter']",
          value: "true"
        },
        {
          name: "Country",
          type: "combobox",
          ref: "select[name='country']",
          value: "United States"
        }
      ]
    });

    // Step 5: Verify form filled correctly
    console.log("  Step 3: Verifying form state");
    const filledSnapshot = await mcp__playwright__browser_snapshot({});

    if (!filledSnapshot.includes("john.doe@example.com")) {
      throw new Error("Email field not filled correctly");
    }

    // Step 6: Capture filled form
    await mcp__playwright__browser_take_screenshot({
      filename: "form-filled.png"
    });

    // Step 7: Submit form
    console.log("  Step 4: Submitting form");
    await mcp__playwright__browser_click({
      element: "Submit button",
      ref: "button[type='submit']"
    });

    // Step 8: Wait for submission result
    await mcp__playwright__browser_wait_for({
      text: "Form submitted successfully"
    });

    // Step 9: Verify success message
    console.log("  Step 5: Verifying submission success");
    const resultSnapshot = await mcp__playwright__browser_snapshot({});

    if (!resultSnapshot.includes("Form submitted successfully")) {
      throw new Error("Success message not found after submission");
    }

    // Step 10: Capture success state
    await mcp__playwright__browser_take_screenshot({
      filename: "form-success.png"
    });

    console.log(`[${TEST_NAME}] PASSED`);
    return {
      status: "PASS",
      evidence: {
        screenshots: [
          "form-initial.png",
          "form-filled.png",
          "form-success.png"
        ]
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);

    await mcp__playwright__browser_take_screenshot({
      filename: "form-FAILURE.png"
    });

    const consoleErrors = await mcp__playwright__browser_console_messages({
      onlyErrors: true
    });

    return {
      status: "FAIL",
      error: error.message,
      evidence: {
        screenshot: "form-FAILURE.png",
        consoleErrors: consoleErrors
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
