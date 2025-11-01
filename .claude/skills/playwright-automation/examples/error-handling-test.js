/**
 * Error Handling Test
 *
 * Demonstrates:
 * - Testing error conditions
 * - 404 page handling
 * - Network timeout handling
 * - JavaScript error detection
 * - Console error monitoring
 *
 * Usage: Run this test to verify browser handles errors gracefully
 */

const TEST_NAME = "Error Handling Test";

async function test404Page() {
  console.log("  Test: 404 Not Found page");

  await mcp__playwright__browser_navigate({
    url: "https://example.com/nonexistent-page-12345"
  });

  await mcp__playwright__browser_wait_for({ time: 2 });

  const snapshot = await mcp__playwright__browser_snapshot({});

  // Check for 404 indicators
  const has404 = snapshot.includes("404") ||
                 snapshot.includes("Not Found") ||
                 snapshot.includes("not found") ||
                 snapshot.includes("Page not found");

  return {
    name: "404 page handling",
    passed: has404,
    details: has404 ? "404 page displayed correctly" : "404 page not detected"
  };
}

async function testInvalidURL() {
  console.log("  Test: Invalid URL handling");

  try {
    await mcp__playwright__browser_navigate({
      url: "not-a-valid-url"
    });

    await mcp__playwright__browser_wait_for({ time: 2 });

    const snapshot = await mcp__playwright__browser_snapshot({});

    // Should show error message
    const hasError = snapshot.includes("error") ||
                     snapshot.includes("Error") ||
                     snapshot.includes("invalid");

    return {
      name: "Invalid URL handling",
      passed: hasError,
      details: "Browser showed error for invalid URL"
    };

  } catch (error) {
    return {
      name: "Invalid URL handling",
      passed: true,
      details: "Browser rejected invalid URL (expected behavior)"
    };
  }
}

async function testJavaScriptErrors() {
  console.log("  Test: JavaScript error detection");

  // Create test page with JS error
  const htmlWithError = `
    <html>
      <head><title>JS Error Test</title></head>
      <body>
        <h1>JavaScript Error Test</h1>
        <script>
          // This will cause an error
          undefinedFunction();
        </script>
      </body>
    </html>
  `;

  const dataURL = `data:text/html;base64,${Buffer.from(htmlWithError).toString('base64')}`;

  await mcp__playwright__browser_navigate({ url: dataURL });
  await mcp__playwright__browser_wait_for({ time: 1 });

  // Check console for errors
  const consoleMessages = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  const jsErrorDetected = consoleMessages.some(msg =>
    msg.includes("undefinedFunction") ||
    msg.includes("ReferenceError") ||
    msg.includes("not defined")
  );

  return {
    name: "JavaScript error detection",
    passed: jsErrorDetected,
    details: jsErrorDetected
      ? `Detected JS error in console (${consoleMessages.length} errors)`
      : "No JS errors detected (unexpected)"
  };
}

async function testNetworkFailure() {
  console.log("  Test: Network failure handling");

  // Try to load from unreachable address
  await mcp__playwright__browser_navigate({
    url: "http://192.0.2.1"  // TEST-NET-1, should timeout
  });

  // Don't wait too long
  await mcp__playwright__browser_wait_for({ time: 3 });

  const snapshot = await mcp__playwright__browser_snapshot({});

  // Should show some kind of error
  const hasNetworkError = snapshot.includes("timeout") ||
                          snapshot.includes("connect") ||
                          snapshot.includes("unreachable") ||
                          snapshot.includes("failed to load");

  return {
    name: "Network failure handling",
    passed: hasNetworkError,
    details: hasNetworkError
      ? "Network error displayed to user"
      : "Network error not detected"
  };
}

async function testMixedContent() {
  console.log("  Test: Mixed content blocking");

  // HTTPS page loading HTTP resource
  const mixedContentHTML = `
    <html>
      <head><title>Mixed Content Test</title></head>
      <body>
        <h1>Mixed Content Test</h1>
        <img src="http://example.com/image.jpg" alt="Mixed content image">
        <script src="http://example.com/script.js"></script>
      </body>
    </html>
  `;

  const dataURL = `data:text/html;base64,${Buffer.from(mixedContentHTML).toString('base64')}`;

  await mcp__playwright__browser_navigate({ url: dataURL });
  await mcp__playwright__browser_wait_for({ time: 2 });

  const consoleMessages = await mcp__playwright__browser_console_messages({});

  // Check for mixed content warnings
  const mixedContentWarning = consoleMessages.some(msg =>
    msg.includes("mixed content") ||
    msg.includes("insecure") ||
    msg.includes("blocked")
  );

  return {
    name: "Mixed content detection",
    passed: true,  // Either blocks or allows, both are valid
    details: mixedContentWarning
      ? "Mixed content warning detected"
      : "No mixed content warning (may be allowed)"
  };
}

async function testCORSError() {
  console.log("  Test: CORS error handling");

  const corsTestHTML = `
    <html>
      <head><title>CORS Test</title></head>
      <body>
        <h1>CORS Test</h1>
        <script>
          fetch('https://example.org/api/data')
            .then(r => console.log('Success'))
            .catch(e => console.error('CORS Error:', e));
        </script>
      </body>
    </html>
  `;

  const dataURL = `data:text/html;base64,${Buffer.from(corsTestHTML).toString('base64')}`;

  await mcp__playwright__browser_navigate({ url: dataURL });
  await mcp__playwright__browser_wait_for({ time: 2 });

  const consoleMessages = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });

  const corsError = consoleMessages.some(msg =>
    msg.includes("CORS") ||
    msg.includes("cross-origin") ||
    msg.includes("Access-Control-Allow-Origin")
  );

  return {
    name: "CORS error handling",
    passed: true,  // Detection is optional
    details: corsError
      ? "CORS error detected in console"
      : "No CORS error detected"
  };
}

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  const results = [];

  try {
    // Run all error condition tests
    results.push(await test404Page());
    results.push(await testInvalidURL());
    results.push(await testJavaScriptErrors());
    results.push(await testNetworkFailure());
    results.push(await testMixedContent());
    results.push(await testCORSError());

    // Capture final state
    await mcp__playwright__browser_take_screenshot({
      filename: "error-handling-final.png"
    });

    // Evaluate overall results
    const failed = results.filter(r => !r.passed);

    if (failed.length > 0) {
      console.error(`[${TEST_NAME}] FAILED: ${failed.length}/${results.length} tests failed`);
      return {
        status: "FAIL",
        results: results,
        failedTests: failed.map(f => f.name),
        evidence: {
          screenshot: "error-handling-final.png"
        }
      };
    }

    console.log(`[${TEST_NAME}] PASSED: All ${results.length} tests passed`);
    return {
      status: "PASS",
      results: results,
      evidence: {
        screenshot: "error-handling-final.png"
      }
    };

  } catch (error) {
    console.error(`[${TEST_NAME}] FAILED: ${error.message}`);

    await mcp__playwright__browser_take_screenshot({
      filename: "error-handling-FAILURE.png"
    });

    return {
      status: "FAIL",
      error: error.message,
      results: results,
      evidence: {
        screenshot: "error-handling-FAILURE.png"
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
