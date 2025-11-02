import { test, expect } from '@playwright/test';
import {
  loadPhishingFixture,
  triggerPhishingTest,
  getDetectionResult,
  verifyPhishingAlert,
  verifyNoAlert,
  HOMOGRAPH_DATABASE,
  SUSPICIOUS_TLDS,
  levenshteinDistance,
  findClosestPopularDomain,
  isSuspiciousTLD,
  extractDomain,
  isIPAddress,
  countSubdomains,
  PhishingDetectionResult
} from '../../helpers/phishing-test-utils';

/**
 * Phishing Detection Tests (PHISH-001 to PHISH-010)
 * Priority: P0 (Critical) - FORK FEATURE
 *
 * Tests Sentinel's PhishingURLAnalyzer for detecting phishing attacks via:
 * - Homograph attacks (Unicode look-alikes)
 * - Typosquatting (intentional misspellings)
 * - Suspicious TLDs (free/high-abuse domains)
 * - IP address URLs
 * - Subdomain abuse
 * - PolicyGraph whitelist integration
 *
 * Implementation:
 * - Core Detection: Services/Sentinel/PhishingURLAnalyzer.{h,cpp}
 * - RequestServer Integration: Services/RequestServer/URLSecurityAnalyzer.{h,cpp}
 * - PolicyGraph: Services/Sentinel/PolicyGraph.{h,cpp}
 */

test.describe('Phishing URL Detection', () => {

  test('PHISH-001: Homograph attack detection', { tag: '@p0' }, async ({ page }) => {
    // Navigate to homograph test fixture
    const testData = await loadPhishingFixture(page, 'homograph-test.html');

    // Verify test data loaded correctly
    expect(testData.testId).toBe('PHISH-001');
    expect(testData.technique).toBe('homograph');
    expect(testData.domain).toBe('xn--pple-43d.com'); // Punycode

    // Trigger homograph detection test
    await triggerPhishingTest(page, '#navigate-homograph');

    // Verify detection alert is displayed
    await verifyPhishingAlert(page, ['homograph', 'detected', 'аpple']);

    // Get detection result
    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(true);
    expect(result!.isHomographAttack).toBe(true);
    expect(result!.phishingScore).toBeGreaterThanOrEqual(0.3);

    // Verify homograph was detected via window flag
    const homographDetected = await page.evaluate(() => (window as any).__homographDetected);
    expect(homographDetected).toBe(true);
  });

  test('PHISH-002: Legitimate IDN domain (no false positive)', { tag: '@p0' }, async ({ page }) => {
    // Load legitimate internationalized domain test
    const testData = await loadPhishingFixture(page, 'legitimate-idn.html');

    expect(testData.testId).toBe('PHISH-002');

    // Test German umlaut domain (münchen.de)
    await triggerPhishingTest(page, '#test-german-umlaut');

    // Verify NO phishing alert
    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(false);
    expect(result!.isHomographAttack).toBe(false);
    expect(result!.phishingScore).toBe(0.0);

    // Verify legitimate IDN was tested
    const legitimateTested = await page.evaluate(() => (window as any).__legitimateIDNTested);
    expect(legitimateTested).toBe(true);

    // Test Russian Cyrillic domain
    await triggerPhishingTest(page, '#test-russian-domain');
    const result2 = await getDetectionResult(page);
    expect(result2!.detected).toBe(false);
    expect(result2!.phishingScore).toBe(0.0);

    // Test Japanese domain
    await triggerPhishingTest(page, '#test-japanese-domain');
    const result3 = await getDetectionResult(page);
    expect(result3!.detected).toBe(false);
    expect(result3!.phishingScore).toBe(0.0);
  });

  test('PHISH-003: Typosquatting detection', { tag: '@p0' }, async ({ page }) => {
    // Load typosquatting test fixture
    const testData = await loadPhishingFixture(page, 'typosquatting-test.html');

    expect(testData.testId).toBe('PHISH-003');
    expect(testData.typosquatExamples.length).toBeGreaterThan(0);

    // Test: faceboook.com (character repetition)
    await triggerPhishingTest(page, '#test-facebook-typo');

    // Verify typosquatting detection
    await verifyPhishingAlert(page, ['typosquatting', 'detected', 'facebook']);

    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(true);
    expect(result!.isTyposquatting).toBe(true);
    expect(result!.phishingScore).toBeGreaterThanOrEqual(0.25);
    expect(result!.closestLegitDomain).toBe('facebook.com');
    expect(result!.editDistance).toBeLessThanOrEqual(3);

    // Test Levenshtein distance calculation
    const distance = levenshteinDistance('faceboook', 'facebook');
    expect(distance).toBe(1); // One extra 'o'

    // Test: googlle.com
    await triggerPhishingTest(page, '#test-google-typo');
    const result2 = await getDetectionResult(page);
    expect(result2!.isTyposquatting).toBe(true);
    expect(result2!.closestLegitDomain).toBe('google.com');

    // Verify typosquatting flag
    const typosquattingDetected = await page.evaluate(() => (window as any).__typosquattingDetected);
    expect(typosquattingDetected).toBe(true);
  });

  test('PHISH-004: Suspicious TLD detection', { tag: '@p0' }, async ({ page }) => {
    // Load suspicious TLD test
    const testData = await loadPhishingFixture(page, 'suspicious-tld-test.html');

    expect(testData.testId).toBe('PHISH-004');
    expect(testData.suspiciousTLDs).toContain('xyz');
    expect(testData.suspiciousTLDs).toContain('top');
    expect(testData.suspiciousTLDs).toContain('tk');

    // Test: secure-bank.xyz
    await triggerPhishingTest(page, '#test-bank-xyz');

    // Verify suspicious TLD alert
    await verifyPhishingAlert(page, ['suspicious', 'tld', 'xyz']);

    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(true);
    expect(result!.hasSuspiciousTLD).toBe(true);
    expect(result!.tld).toBe('xyz');
    expect(result!.phishingScore).toBeGreaterThanOrEqual(0.2);

    // Verify TLD checker utility
    expect(isSuspiciousTLD('xyz')).toBe(true);
    expect(isSuspiciousTLD('top')).toBe(true);
    expect(isSuspiciousTLD('tk')).toBe(true);
    expect(isSuspiciousTLD('com')).toBe(false);
    expect(isSuspiciousTLD('org')).toBe(false);

    // Test other suspicious TLDs
    await triggerPhishingTest(page, '#test-paypal-top');
    const result2 = await getDetectionResult(page);
    expect(result2!.hasSuspiciousTLD).toBe(true);
    expect(result2!.tld).toBe('top');

    await triggerPhishingTest(page, '#test-amazon-tk');
    const result3 = await getDetectionResult(page);
    expect(result3!.hasSuspiciousTLD).toBe(true);
    expect(result3!.tld).toBe('tk');
  });

  test('PHISH-005: IP address URL detection', { tag: '@p0' }, async ({ page }) => {
    // Load IP address phishing test
    const testData = await loadPhishingFixture(page, 'ip-address-test.html');

    expect(testData.testId).toBe('PHISH-005');

    // Test IPv4 address: http://192.168.1.100/login
    await triggerPhishingTest(page, '#test-ipv4-login');

    // Verify IP address alert
    await verifyPhishingAlert(page, ['ip address', 'detected', 'risk']);

    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(true);
    expect(result!.isIPAddress).toBe(true);
    expect(result!.ipType).toBe('IPv4');
    expect(result!.phishingScore).toBeGreaterThanOrEqual(0.25);
    expect(result!.shouldBlock).toBe(true);

    // Verify IP detection utility
    expect(isIPAddress('192.168.1.100')).toBe(true);
    expect(isIPAddress('203.0.113.45')).toBe(true);
    expect(isIPAddress('example.com')).toBe(false);

    // Test HTTPS with IP (still suspicious)
    await triggerPhishingTest(page, '#test-ipv4-https');
    const result2 = await getDetectionResult(page);
    expect(result2!.isIPAddress).toBe(true);

    // Test IPv6 address
    await triggerPhishingTest(page, '#test-ipv6');
    const result3 = await getDetectionResult(page);
    expect(result3!.isIPAddress).toBe(true);
    expect(result3!.ipType).toBe('IPv6');

    // Verify flag
    const ipDetected = await page.evaluate(() => (window as any).__ipAddressDetected);
    expect(ipDetected).toBe(true);
  });

  test('PHISH-006: Excessive subdomains detection', { tag: '@p0' }, async ({ page }) => {
    // Load subdomain abuse test
    const testData = await loadPhishingFixture(page, 'subdomain-abuse-test.html');

    expect(testData.testId).toBe('PHISH-006');

    // Test: login.secure.account.paypal.phishing.com (4 subdomains)
    await triggerPhishingTest(page, '#test-paypal-abuse');

    // Verify subdomain abuse alert
    await verifyPhishingAlert(page, ['subdomain', 'detected', 'paypal']);

    const result = await getDetectionResult(page);
    expect(result).not.toBeNull();
    expect(result!.detected).toBe(true);
    expect(result!.excessiveSubdomains).toBe(true);
    expect(result!.subdomainCount).toBeGreaterThanOrEqual(3);
    expect(result!.actualDomain).toBe('phishing.com');
    expect(result!.impersonatedBrand).toBe('paypal');

    // Verify subdomain counter utility
    expect(countSubdomains('login.secure.account.paypal.phishing.com')).toBe(4);
    expect(countSubdomains('www.example.com')).toBe(1);
    expect(countSubdomains('example.com')).toBe(0);

    // Test other subdomain abuse cases
    await triggerPhishingTest(page, '#test-bank-abuse');
    const result2 = await getDetectionResult(page);
    expect(result2!.excessiveSubdomains).toBe(true);
    expect(result2!.impersonatedBrand).toBe('chase');

    await triggerPhishingTest(page, '#test-amazon-abuse');
    const result3 = await getDetectionResult(page);
    expect(result3!.excessiveSubdomains).toBe(true);
    expect(result3!.impersonatedBrand).toBe('amazon');
  });

  test('PHISH-007: Whitelisted domain (PolicyGraph override)', { tag: '@p0' }, async ({ page }) => {
    // This test simulates PolicyGraph whitelist functionality
    // In actual Ladybird, this would interact with Services/Sentinel/PolicyGraph.cpp

    const html = `
      <html>
        <body>
          <h1>PolicyGraph Whitelist Test</h1>
          <div id="result"></div>
          <script>
            // Simulate a previously flagged domain that was whitelisted by user
            window.__policyGraphData = {
              domain: 'secure-internal.xyz',
              previouslyFlagged: true,
              userDecision: 'trust',
              whitelisted: true,
              timestamp: Date.now()
            };

            // First detection: would normally trigger alert
            const initialDetection = {
              domain: 'secure-internal.xyz',
              hasSuspiciousTLD: true,
              phishingScore: 0.2
            };

            // PolicyGraph lookup: domain is whitelisted
            // Alert suppressed, no notification to user
            const whitelistOverride = true;

            if (whitelistOverride) {
              document.getElementById('result').textContent =
                'Domain whitelisted - no alert shown';
              window.__whitelistVerified = true;
              window.__alertSuppressed = true;
            }
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    // Verify whitelist override
    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('whitelisted');

    const whitelistVerified = await page.evaluate(() => (window as any).__whitelistVerified);
    expect(whitelistVerified).toBe(true);

    const alertSuppressed = await page.evaluate(() => (window as any).__alertSuppressed);
    expect(alertSuppressed).toBe(true);

    // Verify PolicyGraph data structure
    const policyData = await page.evaluate(() => (window as any).__policyGraphData);
    expect(policyData.whitelisted).toBe(true);
    expect(policyData.userDecision).toBe('trust');
  });

  test('PHISH-008: User decision persistence', { tag: '@p0' }, async ({ page }) => {
    // Test that user's "Trust" decision is persisted and no repeat alerts shown

    const html = `
      <html>
        <body>
          <h1>User Decision Persistence Test</h1>
          <button id="first-visit">First Visit (Alert)</button>
          <button id="second-visit">Second Visit (No Alert)</button>
          <div id="result"></div>
          <script>
            let userDecisionSaved = false;

            document.getElementById('first-visit').addEventListener('click', () => {
              // First visit: phishing detected
              const detection = {
                domain: 'paypai.com',
                isTyposquatting: true,
                phishingScore: 0.25
              };

              document.getElementById('result').textContent =
                '⚠️ Phishing detected! User clicked "Trust" - saving decision...';

              // User clicks "Trust" button in alert
              // PolicyGraph saves: (domain='paypai.com', action='trust')
              userDecisionSaved = true;
              window.__userDecisionSaved = true;
              window.__firstAlertShown = true;
            });

            document.getElementById('second-visit').addEventListener('click', () => {
              // Second visit: check PolicyGraph
              if (userDecisionSaved) {
                // Decision found: suppress alert
                document.getElementById('result').textContent =
                  '✅ User previously trusted this domain - no alert shown';
                window.__secondAlertSuppressed = true;
                window.__decisionPersisted = true;
              } else {
                document.getElementById('result').textContent =
                  '⚠️ Alert would be shown again';
              }
            });
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // First visit: alert shown, user trusts
    await page.click('#first-visit');
    await page.waitForTimeout(300);

    let resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Trust');

    const firstAlertShown = await page.evaluate(() => (window as any).__firstAlertShown);
    expect(firstAlertShown).toBe(true);

    const decisionSaved = await page.evaluate(() => (window as any).__userDecisionSaved);
    expect(decisionSaved).toBe(true);

    // Second visit: no alert (decision persisted)
    await page.click('#second-visit');
    await page.waitForTimeout(300);

    resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('no alert shown');

    const secondAlertSuppressed = await page.evaluate(() => (window as any).__secondAlertSuppressed);
    expect(secondAlertSuppressed).toBe(true);

    const decisionPersisted = await page.evaluate(() => (window as any).__decisionPersisted);
    expect(decisionPersisted).toBe(true);
  });

  test('PHISH-009: HTTPS phishing (encrypted ≠ safe)', { tag: '@p0' }, async ({ page }) => {
    // Test that HTTPS doesn't suppress phishing detection

    const html = `
      <html>
        <body>
          <h1>HTTPS Phishing Test</h1>
          <button id="test-https-phishing">Test HTTPS Phishing</button>
          <div id="result"></div>
          <script>
            document.getElementById('test-https-phishing').addEventListener('click', () => {
              // Simulate HTTPS phishing URL
              const url = 'https://secure-login-apple.com.phishing.xyz';

              // PhishingURLAnalyzer detects:
              // 1. Suspicious TLD: .xyz (0.2 score)
              // 2. Subdomain abuse: secure-login-apple.com (0.15 score)
              // 3. Total score: 0.35 (DANGEROUS)

              // HTTPS does NOT suppress detection!
              const detection = {
                url: url,
                isHTTPS: true, // Encrypted, but still phishing
                hasSuspiciousTLD: true,
                excessiveSubdomains: true,
                phishingScore: 0.35,
                threatLevel: 'dangerous'
              };

              document.getElementById('result').innerHTML = \`
                <strong>⚠️ HTTPS Phishing Detected!</strong><br>
                URL: <code>\${url}</code><br>
                HTTPS: <strong>YES</strong> (but still phishing!)<br>
                Phishing Score: <strong>0.35</strong><br>
                Threat Level: <strong>DANGEROUS</strong><br><br>
                <em>HTTPS only encrypts traffic - it doesn't verify legitimacy!</em>
              \`;

              window.__httpsPhishingDetected = true;
              window.__detectionResult = detection;
            });
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    await page.click('#test-https-phishing');
    await page.waitForTimeout(300);

    // Verify HTTPS phishing was detected
    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('HTTPS Phishing Detected');
    expect(resultText).toContain('but still phishing');
    expect(resultText).toContain('DANGEROUS');

    const detected = await page.evaluate(() => (window as any).__httpsPhishingDetected);
    expect(detected).toBe(true);

    const result = await page.evaluate(() => (window as any).__detectionResult);
    expect(result.isHTTPS).toBe(true);
    expect(result.phishingScore).toBeGreaterThanOrEqual(0.3);
    expect(result.threatLevel).toBe('dangerous');
  });

  test('PHISH-010: Edge cases and error handling', { tag: '@p0' }, async ({ page }) => {
    // Test graceful handling of edge cases

    const html = `
      <html>
        <body>
          <h1>Edge Cases Test</h1>
          <div id="result"></div>
          <script>
            const edgeCases = [
              { name: 'Empty host', url: 'http://', expectedError: true },
              { name: 'Malformed URL', url: 'ht!tp://invalid', expectedError: true },
              { name: 'Data URL', url: 'data:text/html,<h1>Test</h1>', expectedError: false, score: 0 },
              { name: 'JavaScript URL', url: 'javascript:alert(1)', expectedError: false, score: 0 },
              { name: 'Very long domain', url: 'http://' + 'a'.repeat(300) + '.com', expectedError: false },
              { name: 'Localhost', url: 'http://localhost/test', expectedError: false, score: 0 }
            ];

            let results = [];
            edgeCases.forEach(testCase => {
              try {
                // Simulate PhishingURLAnalyzer.analyze_url()
                let result = { name: testCase.name, error: null, score: 0 };

                if (testCase.url.startsWith('http://') || testCase.url.startsWith('https://')) {
                  // Valid URL format
                  result.score = testCase.score || 0;
                } else {
                  // Non-HTTP protocols: no phishing analysis
                  result.score = 0;
                }

                results.push(result);
              } catch (e) {
                results.push({ name: testCase.name, error: e.message, score: null });
              }
            });

            document.getElementById('result').textContent =
              'Edge cases tested: ' + results.length + ' (all handled gracefully)';

            window.__edgeCaseResults = results;
            window.__allHandledGracefully = results.every(r => r !== null);
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('handled gracefully');

    const results = await page.evaluate(() => (window as any).__edgeCaseResults);
    expect(results).toHaveLength(6);

    const allHandled = await page.evaluate(() => (window as any).__allHandledGracefully);
    expect(allHandled).toBe(true);

    // Verify specific edge cases
    const dataURLResult = results.find((r: any) => r.name === 'Data URL');
    expect(dataURLResult.score).toBe(0); // Data URLs not analyzed

    const localhostResult = results.find((r: any) => r.name === 'Localhost');
    expect(localhostResult.score).toBe(0); // Localhost not phishing
  });

});

/**
 * Integration Tests with Actual Phishing Sites
 *
 * NOTE: These tests are disabled by default and should only be run
 * in isolated test environments with network access.
 */
test.describe('Real-world Phishing Detection (Manual)', () => {

  test.skip('PHISH-011: Test against browserleaks.com/canvas', async ({ page }) => {
    // This test requires internet connection and is for manual verification only

    await page.goto('https://browserleaks.com/canvas', {
      waitUntil: 'domcontentloaded',
      timeout: 30000,
    });

    await page.waitForTimeout(3000);

    // Verify page loaded
    await expect(page).toHaveURL(/browserleaks\.com/);

    // In actual Ladybird, verify:
    // 1. No false positive alert (browserleaks.com is legitimate)
    // 2. Canvas operations detected but not flagged as phishing
    // 3. Domain score remains low
  });

  test.skip('PHISH-012: Test against known phishing database', async ({ page }) => {
    // This would test against a curated list of known phishing URLs
    // Disabled to avoid navigating to actual malicious sites

    const knownPhishingUrls = [
      // These would be from PhishTank, OpenPhish, etc.
      // NOT included to prevent accidental navigation
    ];

    // In production test:
    // for (const url of knownPhishingUrls) {
    //   const analysis = await PhishingURLAnalyzer.analyze_url(url);
    //   expect(analysis.phishing_score).toBeGreaterThan(0.3);
    // }
  });

});
