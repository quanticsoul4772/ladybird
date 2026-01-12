/**
 * Visual Diff Helper
 *
 * Utilities for screenshot comparison and visual regression testing
 *
 * NOTE: For production use, integrate with pixelmatch, looks-same, or similar
 */

const fs = require('fs');
const path = require('path');

class VisualDiffHelper {
  constructor(options = {}) {
    this.baselineDir = options.baselineDir || path.join(__dirname, '../baselines');
    this.currentDir = options.currentDir || path.join(__dirname, '../screenshots');
    this.diffDir = options.diffDir || path.join(__dirname, '../diffs');
    this.threshold = options.threshold || 0.1; // 0.1% difference allowed

    // Ensure directories exist
    this.ensureDirectories();
  }

  ensureDirectories() {
    [this.baselineDir, this.currentDir, this.diffDir].forEach(dir => {
      if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
      }
    });
  }

  /**
   * Compare current screenshot with baseline
   */
  async compareWithBaseline(testName, currentFilename) {
    const currentPath = path.join(this.currentDir, currentFilename);
    const baselinePath = path.join(this.baselineDir, `${testName}-baseline.png`);

    if (!fs.existsSync(currentPath)) {
      throw new Error(`Current screenshot not found: ${currentPath}`);
    }

    // If no baseline, create it
    if (!fs.existsSync(baselinePath)) {
      fs.copyFileSync(currentPath, baselinePath);
      return {
        result: "BASELINE_CREATED",
        baselinePath: baselinePath,
        currentPath: currentPath,
        pixelDifference: 0
      };
    }

    // Simple comparison (file size)
    // In production, use proper image diff library
    const comparison = this.simpleCompare(currentPath, baselinePath);

    if (comparison.pixelDifference > this.threshold) {
      // Save diff info
      const diffPath = path.join(this.diffDir, `${testName}-diff.json`);
      fs.writeFileSync(diffPath, JSON.stringify({
        testName: testName,
        timestamp: new Date().toISOString(),
        currentPath: currentPath,
        baselinePath: baselinePath,
        pixelDifference: comparison.pixelDifference,
        threshold: this.threshold,
        result: "REGRESSION_DETECTED"
      }, null, 2));

      return {
        result: "REGRESSION_DETECTED",
        baselinePath: baselinePath,
        currentPath: currentPath,
        diffPath: diffPath,
        pixelDifference: comparison.pixelDifference,
        threshold: this.threshold
      };
    }

    return {
      result: "PASS",
      baselinePath: baselinePath,
      currentPath: currentPath,
      pixelDifference: comparison.pixelDifference,
      threshold: this.threshold
    };
  }

  /**
   * Simple file-based comparison
   * Replace with proper image diff in production
   */
  simpleCompare(currentPath, baselinePath) {
    const currentStats = fs.statSync(currentPath);
    const baselineStats = fs.statSync(baselinePath);

    const sizeDiff = Math.abs(currentStats.size - baselineStats.size);
    const percentDiff = (sizeDiff / baselineStats.size) * 100;

    return {
      pixelDifference: percentDiff,
      currentSize: currentStats.size,
      baselineSize: baselineStats.size
    };
  }

  /**
   * Update baseline with current screenshot
   */
  updateBaseline(testName, currentFilename) {
    const currentPath = path.join(this.currentDir, currentFilename);
    const baselinePath = path.join(this.baselineDir, `${testName}-baseline.png`);

    if (!fs.existsSync(currentPath)) {
      throw new Error(`Current screenshot not found: ${currentPath}`);
    }

    fs.copyFileSync(currentPath, baselinePath);

    return {
      result: "BASELINE_UPDATED",
      baselinePath: baselinePath
    };
  }

  /**
   * Get diff report for a test
   */
  getDiffReport(testName) {
    const diffPath = path.join(this.diffDir, `${testName}-diff.json`);

    if (!fs.existsSync(diffPath)) {
      return null;
    }

    return JSON.parse(fs.readFileSync(diffPath, 'utf8'));
  }

  /**
   * Clear diffs for a test
   */
  clearDiff(testName) {
    const diffPath = path.join(this.diffDir, `${testName}-diff.json`);

    if (fs.existsSync(diffPath)) {
      fs.unlinkSync(diffPath);
      return { result: "DIFF_CLEARED" };
    }

    return { result: "NO_DIFF_FOUND" };
  }

  /**
   * Get all tests with regressions
   */
  getAllRegressions() {
    const regressions = [];

    if (!fs.existsSync(this.diffDir)) {
      return regressions;
    }

    const files = fs.readdirSync(this.diffDir);

    files.forEach(file => {
      if (file.endsWith('-diff.json')) {
        const content = fs.readFileSync(path.join(this.diffDir, file), 'utf8');
        const diff = JSON.parse(content);
        regressions.push(diff);
      }
    });

    return regressions;
  }
}

module.exports = VisualDiffHelper;

// Example usage
if (require.main === module) {
  const helper = new VisualDiffHelper();

  console.log("Visual Diff Helper initialized");
  console.log("Baseline directory:", helper.baselineDir);
  console.log("Current directory:", helper.currentDir);
  console.log("Diff directory:", helper.diffDir);
  console.log("Threshold:", helper.threshold);

  const regressions = helper.getAllRegressions();
  console.log(`\nFound ${regressions.length} visual regressions`);

  regressions.forEach(reg => {
    console.log(`  - ${reg.testName}: ${reg.pixelDifference.toFixed(2)}% difference`);
  });
}
