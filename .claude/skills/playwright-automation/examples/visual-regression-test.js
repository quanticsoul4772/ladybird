/**
 * Visual Regression Test
 *
 * Demonstrates:
 * - Capturing screenshots for visual comparison
 * - Baseline vs current comparison
 * - Pixel difference calculation
 * - Regression detection
 *
 * Usage: Run this test to detect visual changes in the UI
 */

const TEST_NAME = "Visual Regression Test";
const fs = require('fs');
const path = require('path');

// Simple pixel comparison (in production, use pixelmatch or similar)
async function compareImages(currentPath, baselinePath, threshold = 0.1) {
  // Check if baseline exists
  if (!fs.existsSync(baselinePath)) {
    console.log("  No baseline found, creating baseline");
    fs.copyFileSync(currentPath, baselinePath);
    return {
      isBaseline: true,
      pixelDifference: 0,
      passed: true
    };
  }

  // In production, use proper image comparison library
  // For this example, we'll do a simple file size comparison
  const currentStats = fs.statSync(currentPath);
  const baselineStats = fs.statSync(baselinePath);

  const sizeDifference = Math.abs(currentStats.size - baselineStats.size);
  const percentDifference = (sizeDifference / baselineStats.size) * 100;

  return {
    isBaseline: false,
    pixelDifference: percentDifference,
    passed: percentDifference <= threshold,
    threshold: threshold
  };
}

async function run() {
  console.log(`[${TEST_NAME}] Starting...`);

  const BASELINE_DIR = path.join(__dirname, '../baselines');
  const CURRENT_DIR = path.join(__dirname, '../screenshots');

  // Ensure directories exist
  if (!fs.existsSync(BASELINE_DIR)) {
    fs.mkdirSync(BASELINE_DIR, { recursive: true });
  }
  if (!fs.existsSync(CURRENT_DIR)) {
    fs.mkdirSync(CURRENT_DIR, { recursive: true });
  }

  try {
    // Step 1: Navigate to page
    console.log("  Step 1: Navigating to test page");
    await mcp__playwright__browser_navigate({
      url: "https://example.com"
    });

    // Step 2: Wait for page to stabilize
    console.log("  Step 2: Waiting for page stability");
    await mcp__playwright__browser_wait_for({
      text: "Example Domain"
    });

    // Additional wait for animations/fonts
    await mcp__playwright__browser_wait_for({ time: 1 });

    // Step 3: Take screenshot
    console.log("  Step 3: Capturing current screenshot");
    const currentPath = path.join(CURRENT_DIR, "homepage-current.png");
    await mcp__playwright__browser_take_screenshot({
      fullPage: true,
      filename: currentPath
    });

    // Step 4: Compare with baseline
    console.log("  Step 4: Comparing with baseline");
    const baselinePath = path.join(BASELINE_DIR, "homepage-baseline.png");
    const comparison = await compareImages(currentPath, baselinePath, 0.5);

    if (comparison.isBaseline) {
      console.log(`[${TEST_NAME}] Baseline created`);
      return {
        status: "PASS",
        message: "Baseline screenshot created",
        evidence: {
          baseline: baselinePath
        }
      };
    }

    // Step 5: Check if regression detected
    if (!comparison.passed) {
      console.error(`[${TEST_NAME}] Visual regression detected`);

      return {
        status: "FAIL",
        reason: `Visual regression detected: ${comparison.pixelDifference.toFixed(2)}% difference (threshold: ${comparison.threshold}%)`,
        evidence: {
          current: currentPath,
          baseline: baselinePath,
          pixelDifference: comparison.pixelDifference
        }
      };
    }

    console.log(`[${TEST_NAME}] PASSED`);
    return {
      status: "PASS",
      evidence: {
        current: currentPath,
        baseline: baselinePath,
        pixelDifference: comparison.pixelDifference
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
