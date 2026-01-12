import { defineConfig, devices } from '@playwright/test';

/**
 * Playwright configuration for Ladybird browser E2E tests
 * See https://playwright.dev/docs/test-configuration
 */
export default defineConfig({
  testDir: './tests',

  /* Run tests in files in parallel */
  fullyParallel: true,

  /* Fail the build on CI if you accidentally left test.only in the source code */
  forbidOnly: !!process.env.CI,

  /* Retry on CI only */
  retries: process.env.CI ? 2 : 0,

  /* Opt out of parallel tests on CI */
  workers: process.env.CI ? 1 : undefined,

  /* Reporter to use */
  reporter: [
    ['html'],
    ['list'],
    ['json', { outputFile: 'test-results/results.json' }]
  ],

  /* Shared settings for all the projects below */
  use: {
    /* Base URL to use in actions like `await page.goto('/')` */
    baseURL: 'http://localhost:3000',

    /* Collect trace when retrying the failed test */
    trace: 'on-first-retry',

    /* Screenshot on failure */
    screenshot: 'only-on-failure',

    /* Video on failure */
    video: 'retain-on-failure',

    /* Maximum time each action such as `click()` can take */
    actionTimeout: 10000,

    /* Maximum time each navigation can take */
    navigationTimeout: 30000,
  },

  /* Configure projects for major browsers */
  projects: [
    {
      name: 'ladybird',
      use: {
        ...devices['Desktop Chrome'],
        // Launch actual Ladybird browser
        executablePath: '/home/rbsmith4/ladybird/Build/release/bin/Ladybird',
        // Accept self-signed certificates for HTTPS testing
        ignoreHTTPSErrors: true,
        // Ladybird-specific configurations
        launchOptions: {
          args: [
            // Add any Ladybird-specific args here if needed
          ],
        },
      },
    },

    // Keep Chromium for comparison/debugging
    {
      name: 'chromium-reference',
      use: {
        ...devices['Desktop Chrome'],
        channel: 'chromium',
      },
    },
  ],

  /* Global setup/teardown */
  // globalSetup: require.resolve('./global-setup'),
  // globalTeardown: require.resolve('./global-teardown'),

  /* Folder for test artifacts such as screenshots, videos, traces, etc. */
  outputDir: 'test-results/',

  /* Test timeout */
  timeout: 60000,

  /* Expect timeout */
  expect: {
    timeout: 5000,
  },

  /* Run local dev server before starting tests (if needed) */
  // webServer: {
  //   command: 'npm run start',
  //   url: 'http://localhost:3000',
  //   reuseExistingServer: !process.env.CI,
  // },

  /**
   * Local HTTP/HTTPS test servers for cross-origin and security testing
   * Automatically started/stopped with Playwright tests
   *
   * Servers:
   * - Port 9080: Primary HTTP test server (Origin A)
   * - Port 9081: Secondary HTTP test server (Origin B for cross-origin)
   * - Port 9443: HTTPS test server (for mixed content, HSTS, etc.)
   *
   * Usage in tests:
   *   await page.goto('http://localhost:9080/forms/legitimate-login.html');
   *   await page.goto('http://localhost:9081/forms/phishing-attack.html');
   *   await page.goto('https://localhost:9443/'); // HTTPS tests
   */
  webServer: [
    {
      command: 'PORT=9080 node test-server.js',
      port: 9080,
      timeout: 10000,
      reuseExistingServer: !process.env.CI,
      stdout: 'pipe',
      stderr: 'pipe',
    },
    {
      command: 'PORT=9081 node test-server.js',
      port: 9081,
      timeout: 10000,
      reuseExistingServer: !process.env.CI,
      stdout: 'pipe',
      stderr: 'pipe',
    },
    {
      command: 'USE_HTTPS=true PORT=9443 node test-server.js',
      port: 9443,
      timeout: 10000,
      reuseExistingServer: !process.env.CI,
      stdout: 'pipe',
      stderr: 'pipe',
    },
  ],
});
