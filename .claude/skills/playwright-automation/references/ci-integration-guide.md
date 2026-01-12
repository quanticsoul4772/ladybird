# CI/CD Integration Guide for Playwright Tests

Guide to running Playwright tests in continuous integration pipelines.

## GitHub Actions Integration

### Basic Workflow

```yaml
# .github/workflows/playwright-tests.yml
name: Playwright E2E Tests

on:
  push:
    branches: [master, main]
  pull_request:
    branches: [master, main]

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  playwright-tests:
    runs-on: ubuntu-latest
    timeout-minutes: 30

    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '20'

      - name: Build Ladybird
        run: |
          cmake --preset Release
          cmake --build Build/release --target Ladybird

      - name: Start Ladybird
        run: |
          ./Build/release/bin/Ladybird &
          sleep 5  # Wait for browser to start

      - name: Run Playwright tests
        run: |
          node Tests/Playwright/run-all-tests.js

      - name: Upload screenshots on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: test-failure-screenshots
          path: Tests/Playwright/screenshots/
          retention-days: 30

      - name: Upload test reports
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-reports
          path: Tests/Playwright/reports/
          retention-days: 30
```

### Matrix Strategy

Run tests across multiple configurations:

```yaml
jobs:
  playwright-tests:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest]
        browser-config: [Release, Debug]
        test-suite:
          - name: 'E2E Tests'
            path: 'Tests/Playwright/e2e/'
          - name: 'Visual Regression'
            path: 'Tests/Playwright/visual/'
          - name: 'Accessibility Tests'
            path: 'Tests/Playwright/accessibility/'
          - name: 'Security Tests'
            path: 'Tests/Playwright/security/'

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Build Ladybird (${{ matrix.browser-config }})
        run: |
          cmake --preset ${{ matrix.browser-config }}
          cmake --build Build/${{ matrix.browser-config }}

      - name: Run ${{ matrix.test-suite.name }}
        run: |
          node Tests/Playwright/run-tests.js \
            --path="${{ matrix.test-suite.path }}" \
            --config="${{ matrix.browser-config }}"

      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: results-${{ matrix.os }}-${{ matrix.browser-config }}-${{ matrix.test-suite.name }}
          path: Tests/Playwright/results/
```

### Scheduled Runs

Run comprehensive tests nightly:

```yaml
# .github/workflows/nightly-playwright.yml
name: Nightly Playwright Tests

on:
  schedule:
    - cron: '0 2 * * *'  # 2 AM daily
  workflow_dispatch:  # Allow manual trigger

jobs:
  comprehensive-tests:
    runs-on: ubuntu-latest
    timeout-minutes: 120

    steps:
      - uses: actions/checkout@v4

      - name: Build Ladybird
        run: |
          cmake --preset Release
          cmake --build Build/release

      - name: Run full test suite
        run: |
          node Tests/Playwright/run-all-tests.js --comprehensive

      - name: Generate test report
        if: always()
        run: |
          node Tests/Playwright/generate-report.js

      - name: Upload comprehensive report
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: nightly-test-report
          path: Tests/Playwright/reports/

      - name: Notify on failure
        if: failure()
        uses: actions/github-script@v7
        with:
          script: |
            github.rest.issues.create({
              owner: context.repo.owner,
              repo: context.repo.repo,
              title: 'Nightly Playwright tests failed',
              body: 'Nightly test run failed. Check artifacts for details.',
              labels: ['automated-test-failure']
            })
```

## Test Runner Script

### Master Test Runner

```javascript
// Tests/Playwright/run-all-tests.js
const fs = require('fs');
const path = require('path');

const config = {
  testDirs: [
    'Tests/Playwright/e2e',
    'Tests/Playwright/visual',
    'Tests/Playwright/accessibility',
    'Tests/Playwright/security'
  ],
  screenshotDir: 'Tests/Playwright/screenshots',
  reportDir: 'Tests/Playwright/reports',
  comprehensive: process.argv.includes('--comprehensive')
};

// Ensure directories exist
[config.screenshotDir, config.reportDir].forEach(dir => {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
});

async function loadTests() {
  const tests = [];

  for (const dir of config.testDirs) {
    if (!fs.existsSync(dir)) {
      console.log(`Skipping ${dir} (does not exist)`);
      continue;
    }

    const files = fs.readdirSync(dir);

    for (const file of files) {
      if (file.endsWith('.js') && file.includes('test')) {
        const testPath = path.join(process.cwd(), dir, file);
        const test = require(testPath);

        tests.push({
          ...test,
          file: file,
          dir: dir
        });
      }
    }
  }

  return tests;
}

async function runTest(test) {
  const startTime = Date.now();

  console.log(`\n[${test.name}] Running...`);

  try {
    const result = await test.run();
    const duration = Date.now() - startTime;

    return {
      name: test.name,
      file: test.file,
      dir: test.dir,
      status: result.status,
      duration: duration,
      evidence: result.evidence || {},
      details: result
    };

  } catch (error) {
    const duration = Date.now() - startTime;

    console.error(`[${test.name}] ERROR: ${error.message}`);

    return {
      name: test.name,
      file: test.file,
      dir: test.dir,
      status: 'ERROR',
      duration: duration,
      error: error.message,
      stack: error.stack
    };
  }
}

async function runAllTests() {
  console.log('=== Playwright Test Runner ===\n');

  const tests = await loadTests();
  console.log(`Found ${tests.length} tests\n`);

  const results = [];
  let passed = 0;
  let failed = 0;
  let errors = 0;

  const startTime = Date.now();

  for (const test of tests) {
    const result = await runTest(test);
    results.push(result);

    if (result.status === 'PASS') {
      passed++;
      console.log(`  ✓ ${result.name} (${result.duration}ms)`);
    } else if (result.status === 'FAIL') {
      failed++;
      console.error(`  ✗ ${result.name} (${result.duration}ms)`);
    } else {
      errors++;
      console.error(`  ⚠ ${result.name} ERROR (${result.duration}ms)`);
    }
  }

  const totalDuration = Date.now() - startTime;

  // Generate summary
  console.log('\n=== Test Summary ===');
  console.log(`Total: ${results.length}`);
  console.log(`Passed: ${passed}`);
  console.log(`Failed: ${failed}`);
  console.log(`Errors: ${errors}`);
  console.log(`Duration: ${totalDuration}ms`);

  // Save detailed report
  const report = {
    timestamp: new Date().toISOString(),
    totalTests: results.length,
    passed: passed,
    failed: failed,
    errors: errors,
    duration: totalDuration,
    comprehensive: config.comprehensive,
    results: results
  };

  const reportPath = path.join(
    config.reportDir,
    `test-report-${new Date().toISOString().replace(/:/g, '-')}.json`
  );

  fs.writeFileSync(reportPath, JSON.stringify(report, null, 2));
  console.log(`\nReport saved to: ${reportPath}`);

  // Generate HTML report
  generateHTMLReport(report, reportPath.replace('.json', '.html'));

  // Exit with error code if any failures
  process.exit(failed > 0 || errors > 0 ? 1 : 0);
}

function generateHTMLReport(report, outputPath) {
  const html = `
<!DOCTYPE html>
<html>
<head>
  <title>Playwright Test Report</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      max-width: 1200px;
      margin: 40px auto;
      padding: 0 20px;
    }
    .summary {
      background: #f5f5f5;
      padding: 20px;
      border-radius: 8px;
      margin-bottom: 30px;
    }
    .summary h1 {
      margin-top: 0;
    }
    .stats {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 20px;
      margin-top: 20px;
    }
    .stat {
      background: white;
      padding: 15px;
      border-radius: 4px;
      text-align: center;
    }
    .stat-value {
      font-size: 32px;
      font-weight: bold;
    }
    .stat-label {
      color: #666;
      margin-top: 5px;
    }
    .pass { color: #28a745; }
    .fail { color: #dc3545; }
    .error { color: #ffc107; }
    .test-result {
      background: white;
      border: 1px solid #ddd;
      border-radius: 4px;
      padding: 15px;
      margin-bottom: 10px;
    }
    .test-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .test-details {
      margin-top: 10px;
      padding-top: 10px;
      border-top: 1px solid #eee;
      color: #666;
      font-size: 14px;
    }
  </style>
</head>
<body>
  <div class="summary">
    <h1>Playwright Test Report</h1>
    <p>${report.timestamp}</p>

    <div class="stats">
      <div class="stat">
        <div class="stat-value">${report.totalTests}</div>
        <div class="stat-label">Total Tests</div>
      </div>
      <div class="stat">
        <div class="stat-value pass">${report.passed}</div>
        <div class="stat-label">Passed</div>
      </div>
      <div class="stat">
        <div class="stat-value fail">${report.failed}</div>
        <div class="stat-label">Failed</div>
      </div>
      <div class="stat">
        <div class="stat-value error">${report.errors}</div>
        <div class="stat-label">Errors</div>
      </div>
    </div>

    <p style="margin-top: 20px;">
      <strong>Duration:</strong> ${(report.duration / 1000).toFixed(2)}s
    </p>
  </div>

  <h2>Test Results</h2>
  ${report.results.map(result => `
    <div class="test-result">
      <div class="test-header">
        <span>
          <strong class="${result.status.toLowerCase()}">${result.status}</strong>
          ${result.name}
        </span>
        <span>${result.duration}ms</span>
      </div>
      <div class="test-details">
        <div><strong>File:</strong> ${result.file}</div>
        ${result.error ? `<div style="color: #dc3545;"><strong>Error:</strong> ${result.error}</div>` : ''}
        ${result.evidence && result.evidence.screenshot ?
          `<div><strong>Screenshot:</strong> ${result.evidence.screenshot}</div>` : ''}
      </div>
    </div>
  `).join('')}
</body>
</html>
  `;

  fs.writeFileSync(outputPath, html);
  console.log(`HTML report saved to: ${outputPath}`);
}

// Run tests
runAllTests().catch(error => {
  console.error('Fatal error:', error);
  process.exit(1);
});
```

## Environment Configuration

### CI Environment Variables

```bash
# .env.ci
CI=true
PLAYWRIGHT_TIMEOUT=30000
PLAYWRIGHT_RETRY=2
BROWSER_HEADLESS=true
SCREENSHOT_ON_FAILURE=true
VERBOSE_LOGGING=true
```

### CI-Specific Test Config

```javascript
// Tests/Playwright/config.js
module.exports = {
  isCI: process.env.CI === 'true',

  timeout: process.env.CI === 'true' ? 30000 : 10000,

  retryCount: process.env.CI === 'true' ? 2 : 0,

  screenshotOnFailure: process.env.SCREENSHOT_ON_FAILURE !== 'false',

  slowMo: process.env.SLOW_MO ? parseInt(process.env.SLOW_MO) : 0,

  baseURL: process.env.BASE_URL || 'https://example.com',

  // CI-specific adjustments
  ciAdjustments: {
    waitMultiplier: 1.5,  // Wait longer in CI
    networkTimeout: 10000,
    maxRetries: 3
  }
};
```

## Artifact Management

### Screenshot Organization

```javascript
// Organize screenshots by test run
function saveScreenshot(testName, status) {
  const timestamp = new Date().toISOString().replace(/:/g, '-');
  const runId = process.env.GITHUB_RUN_ID || 'local';

  const dir = path.join(
    'Tests/Playwright/screenshots',
    runId,
    testName
  );

  fs.mkdirSync(dir, { recursive: true });

  const filename = `${status}-${timestamp}.png`;

  return path.join(dir, filename);
}
```

### Report Retention

```yaml
# Keep test artifacts for different durations
- name: Upload failure screenshots
  if: failure()
  uses: actions/upload-artifact@v4
  with:
    name: failure-screenshots
    path: Tests/Playwright/screenshots/
    retention-days: 90  # Keep failures longer

- name: Upload success reports
  if: success()
  uses: actions/upload-artifact@v4
  with:
    name: test-reports
    path: Tests/Playwright/reports/
    retention-days: 7  # Keep successes shorter
```

## Performance Optimization

### Parallel Test Execution

```yaml
jobs:
  playwright-tests:
    strategy:
      matrix:
        shard: [1, 2, 3, 4]

    steps:
      - name: Run tests (shard ${{ matrix.shard }}/4)
        run: |
          node Tests/Playwright/run-tests.js \
            --shard=${{ matrix.shard }} \
            --total-shards=4
```

### Caching

```yaml
- name: Cache build artifacts
  uses: actions/cache@v4
  with:
    path: |
      Build/
      ~/.ccache
    key: ${{ runner.os }}-build-${{ hashFiles('**/CMakeLists.txt') }}

- name: Cache test baselines
  uses: actions/cache@v4
  with:
    path: Tests/Playwright/baselines/
    key: visual-baselines-${{ hashFiles('Tests/Playwright/baselines/**') }}
```

## Notifications

### Slack Notification

```yaml
- name: Notify on failure
  if: failure()
  uses: slackapi/slack-github-action@v1
  with:
    payload: |
      {
        "text": "Playwright tests failed",
        "blocks": [
          {
            "type": "section",
            "text": {
              "type": "mrkdwn",
              "text": "*Playwright Tests Failed*\nRun: ${{ github.run_id }}\nCommit: ${{ github.sha }}"
            }
          }
        ]
      }
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK }}
```

## Best Practices

1. **Use CI-specific timeouts** - Longer timeouts in CI
2. **Retry flaky tests** - Automatic retry for intermittent failures
3. **Capture comprehensive evidence** - Screenshots, logs, snapshots on failure
4. **Organize artifacts** - Clear directory structure for debugging
5. **Cache aggressively** - Build artifacts, dependencies, baselines
6. **Run in parallel** - Shard tests across multiple runners
7. **Monitor performance** - Track test duration trends
8. **Notify on failures** - Slack/email notifications for failures
9. **Keep baselines in version control** - For visual regression tests
10. **Generate HTML reports** - Human-readable test results
