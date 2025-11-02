/**
 * Phishing Detection Test Utilities
 *
 * Helper functions for testing Sentinel's PhishingURLAnalyzer
 */

import { Page, expect } from '@playwright/test';

/**
 * Expected detection result structure
 */
export interface PhishingDetectionResult {
  detected: boolean;
  isHomographAttack?: boolean;
  isTyposquatting?: boolean;
  hasSuspiciousTLD?: boolean;
  isIPAddress?: boolean;
  excessiveSubdomains?: boolean;
  phishingScore: number;
  confidence?: number;
  explanation?: string;
  closestLegitDomain?: string;
  shouldBlock?: boolean;
}

/**
 * Phishing test case structure
 */
export interface PhishingTestCase {
  testId: string;
  description: string;
  url: string;
  expected: PhishingDetectionResult;
  fixture?: string;
}

/**
 * Load a phishing test fixture and return test data
 */
export async function loadPhishingFixture(page: Page, fixturePath: string): Promise<any> {
  const fixtureUrl = `file://${process.cwd()}/fixtures/phishing/${fixturePath}`;
  await page.goto(fixtureUrl, { waitUntil: 'domcontentloaded' });

  // Wait for fixture to load and expose test data
  await page.waitForFunction(() => window.hasOwnProperty('__phishingTestData'));

  return await page.evaluate(() => (window as any).__phishingTestData);
}

/**
 * Trigger phishing detection by clicking a test button
 */
export async function triggerPhishingTest(page: Page, buttonSelector: string): Promise<void> {
  await page.click(buttonSelector);
  // Wait for detection results to be displayed
  await page.waitForTimeout(600);
}

/**
 * Get detection result from page
 */
export async function getDetectionResult(page: Page): Promise<PhishingDetectionResult | null> {
  return await page.evaluate(() => {
    const win = window as any;
    return win.__detectionResult || null;
  });
}

/**
 * Verify phishing alert is displayed
 */
export async function verifyPhishingAlert(page: Page, expectedKeywords: string[]): Promise<void> {
  const resultDiv = await page.locator('#result');
  await expect(resultDiv).toBeVisible();

  const resultText = await resultDiv.textContent();
  expect(resultText).not.toBeNull();

  for (const keyword of expectedKeywords) {
    expect(resultText!.toLowerCase()).toContain(keyword.toLowerCase());
  }
}

/**
 * Verify no false positive alert
 */
export async function verifyNoAlert(page: Page): Promise<void> {
  const result = await getDetectionResult(page);

  if (result) {
    expect(result.detected).toBe(false);
    expect(result.phishingScore).toBeLessThanOrEqual(0.2); // Low score threshold
  }
}

/**
 * Homograph database - Unicode confusables for testing
 */
export const HOMOGRAPH_DATABASE = {
  // Latin -> Cyrillic confusables
  'a': '\u0430', // Cyrillic 'а' (U+0430)
  'e': '\u0435', // Cyrillic 'е' (U+0435)
  'o': '\u043E', // Cyrillic 'о' (U+043E)
  'p': '\u0440', // Cyrillic 'р' (U+0440)
  'c': '\u0441', // Cyrillic 'с' (U+0441)
  'y': '\u0443', // Cyrillic 'у' (U+0443)
  'x': '\u0445', // Cyrillic 'х' (U+0445)

  // Latin -> Greek confusables
  'o_greek': '\u03BF', // Greek 'ο' (U+03BF)
  'a_greek': '\u03B1', // Greek 'α' (U+03B1)
  'v': '\u03BD',       // Greek 'ν' (U+03BD)

  // Test domains with homographs
  examples: {
    'apple.com': 'аpple.com',      // Cyrillic 'а'
    'google.com': 'gοοgle.com',    // Greek 'ο'
    'paypal.com': 'рaypal.com',    // Cyrillic 'р'
    'facebook.com': 'fаcebook.com' // Cyrillic 'а'
  }
};

/**
 * Popular domains list (subset of PhishingURLAnalyzer::popular_domains)
 */
export const POPULAR_DOMAINS = [
  'paypal.com',
  'chase.com',
  'bankofamerica.com',
  'google.com',
  'apple.com',
  'microsoft.com',
  'amazon.com',
  'facebook.com',
  'instagram.com',
  'twitter.com',
  'gmail.com',
  'outlook.com',
  'yahoo.com',
  'netflix.com',
  'coinbase.com',
  'github.com'
];

/**
 * Suspicious TLDs (matches PhishingURLAnalyzer::suspicious_tlds)
 */
export const SUSPICIOUS_TLDS = [
  'tk', 'ml', 'ga', 'cf', 'gq',           // Free Freenom TLDs
  'top', 'xyz', 'club', 'work', 'click',  // High abuse rate
  'link', 'download', 'stream', 'online',
  'site', 'website'
];

/**
 * Calculate Levenshtein distance (edit distance) between two strings
 */
export function levenshteinDistance(a: string, b: string): number {
  const m = a.length;
  const n = b.length;
  const dp: number[][] = Array(m + 1).fill(null).map(() => Array(n + 1).fill(0));

  for (let i = 0; i <= m; i++) dp[i][0] = i;
  for (let j = 0; j <= n; j++) dp[0][j] = j;

  for (let i = 1; i <= m; i++) {
    for (let j = 1; j <= n; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      dp[i][j] = Math.min(
        dp[i - 1][j] + 1,      // deletion
        dp[i][j - 1] + 1,      // insertion
        dp[i - 1][j - 1] + cost // substitution
      );
    }
  }

  return dp[m][n];
}

/**
 * Find closest popular domain by edit distance
 */
export function findClosestPopularDomain(domain: string): { domain: string; distance: number } | null {
  let minDistance = Infinity;
  let closestDomain: string | null = null;

  // Extract domain name without TLD
  const domainName = domain.split('.')[0];

  for (const popular of POPULAR_DOMAINS) {
    const popularName = popular.split('.')[0];
    const distance = levenshteinDistance(domainName, popularName);

    if (distance < minDistance) {
      minDistance = distance;
      closestDomain = popular;
    }
  }

  return closestDomain ? { domain: closestDomain, distance: minDistance } : null;
}

/**
 * Check if TLD is suspicious
 */
export function isSuspiciousTLD(tld: string): boolean {
  return SUSPICIOUS_TLDS.includes(tld.toLowerCase());
}

/**
 * Extract domain from URL
 */
export function extractDomain(url: string): string | null {
  try {
    const urlObj = new URL(url.startsWith('http') ? url : `http://${url}`);
    return urlObj.hostname;
  } catch {
    return null;
  }
}

/**
 * Check if URL uses IP address
 */
export function isIPAddress(hostname: string): boolean {
  // IPv4 regex
  const ipv4Regex = /^(\d{1,3}\.){3}\d{1,3}$/;
  // IPv6 regex (simplified)
  const ipv6Regex = /^\[?([0-9a-fA-F:]+)\]?$/;

  return ipv4Regex.test(hostname) || ipv6Regex.test(hostname);
}

/**
 * Count subdomains in hostname
 */
export function countSubdomains(hostname: string): number {
  const parts = hostname.split('.');
  // Subtract TLD and domain (last 2 parts)
  return Math.max(0, parts.length - 2);
}

/**
 * Test data for all phishing test cases
 */
export const PHISHING_TEST_CASES: PhishingTestCase[] = [
  {
    testId: 'PHISH-001',
    description: 'Homograph attack (IDN homograph)',
    url: 'http://xn--pple-43d.com',
    fixture: 'homograph-test.html',
    expected: {
      detected: true,
      isHomographAttack: true,
      phishingScore: 0.3,
      confidence: 1.0
    }
  },
  {
    testId: 'PHISH-002',
    description: 'Legitimate IDN domain (no false positive)',
    url: 'http://münchen.de',
    fixture: 'legitimate-idn.html',
    expected: {
      detected: false,
      isHomographAttack: false,
      phishingScore: 0.0,
      confidence: 1.0
    }
  },
  {
    testId: 'PHISH-003',
    description: 'Typosquatting detection',
    url: 'http://faceboook.com',
    fixture: 'typosquatting-test.html',
    expected: {
      detected: true,
      isTyposquatting: true,
      phishingScore: 0.25,
      closestLegitDomain: 'facebook.com'
    }
  },
  {
    testId: 'PHISH-004',
    description: 'Suspicious TLD',
    url: 'http://secure-bank.xyz',
    fixture: 'suspicious-tld-test.html',
    expected: {
      detected: true,
      hasSuspiciousTLD: true,
      phishingScore: 0.2
    }
  },
  {
    testId: 'PHISH-005',
    description: 'IP address URL',
    url: 'http://192.168.1.1/login',
    fixture: 'ip-address-test.html',
    expected: {
      detected: true,
      isIPAddress: true,
      phishingScore: 0.25,
      shouldBlock: true
    }
  },
  {
    testId: 'PHISH-006',
    description: 'Excessive subdomains',
    url: 'http://login.secure.account.paypal.phishing.com',
    fixture: 'subdomain-abuse-test.html',
    expected: {
      detected: true,
      excessiveSubdomains: true,
      phishingScore: 0.15
    }
  }
];

/**
 * Verify detection result matches expected
 */
export function verifyDetectionResult(
  actual: PhishingDetectionResult,
  expected: PhishingDetectionResult
): void {
  expect(actual.detected).toBe(expected.detected);
  expect(actual.phishingScore).toBeGreaterThanOrEqual(expected.phishingScore);

  if (expected.isHomographAttack !== undefined) {
    expect(actual.isHomographAttack).toBe(expected.isHomographAttack);
  }
  if (expected.isTyposquatting !== undefined) {
    expect(actual.isTyposquatting).toBe(expected.isTyposquatting);
  }
  if (expected.hasSuspiciousTLD !== undefined) {
    expect(actual.hasSuspiciousTLD).toBe(expected.hasSuspiciousTLD);
  }
  if (expected.isIPAddress !== undefined) {
    expect(actual.isIPAddress).toBe(expected.isIPAddress);
  }
}
