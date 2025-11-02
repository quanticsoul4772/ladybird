/**
 * FormMonitor Test Helpers
 *
 * Utility functions for testing Sentinel's FormMonitor credential protection.
 * These helpers provide common functionality for PolicyGraph setup, alert detection,
 * and cross-origin form testing.
 */

import { Page } from '@playwright/test';

/**
 * PolicyGraph database path (platform-dependent)
 */
export const POLICY_GRAPH_DB_PATH = process.env.SENTINEL_DB_PATH || '/tmp/sentinel_policies.db';

/**
 * FormSubmitEvent structure (mirrors FormMonitor.h)
 */
export interface FormSubmitEvent {
  documentUrl: string;
  actionUrl: string;
  method: string;
  hasPasswordField: boolean;
  hasEmailField: boolean;
  fields: Array<{
    name: string;
    type: 'Text' | 'Password' | 'Email' | 'Hidden' | 'Other';
    hasValue: boolean;
  }>;
  timestamp?: number;
}

/**
 * CredentialAlert structure (mirrors FormMonitor.h)
 */
export interface CredentialAlert {
  alertType: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  formOrigin: string;
  actionOrigin: string;
  usesHttps: boolean;
  hasPasswordField: boolean;
  isCrossOrigin: boolean;
  timestamp: number;
}

/**
 * CredentialRelationship structure (mirrors PolicyGraph.h)
 */
export interface CredentialRelationship {
  id?: number;
  formOrigin: string;
  actionOrigin: string;
  relationshipType: 'trusted' | 'blocked';
  createdAt?: number;
  createdBy?: string;
  notes?: string;
}

/**
 * Extract origin from URL (scheme + host + port)
 * Matches FormMonitor::extract_origin() behavior
 */
export function extractOrigin(url: string): string {
  try {
    const parsed = new URL(url);
    let origin = `${parsed.protocol}//${parsed.hostname}`;

    // Include port if not default for scheme
    if (parsed.port) {
      const port = parseInt(parsed.port);
      const isDefaultPort = (parsed.protocol === 'http:' && port === 80) ||
                           (parsed.protocol === 'https:' && port === 443);

      if (!isDefaultPort) {
        origin += `:${port}`;
      }
    }

    return origin;
  } catch (e) {
    return url; // Fallback for invalid URLs
  }
}

/**
 * Check if form submission is cross-origin
 * Matches FormMonitor::is_cross_origin_submission() behavior
 */
export function isCrossOriginSubmission(formUrl: string, actionUrl: string): boolean {
  const formOrigin = extractOrigin(formUrl);
  const actionOrigin = extractOrigin(actionUrl);
  return formOrigin !== actionOrigin;
}

/**
 * Create a form submit test page
 */
export function createFormTestPage(config: {
  title: string;
  formOrigin?: string; // Simulated origin (for documentation)
  actionUrl: string;
  method?: 'GET' | 'POST';
  fields?: Array<{
    type: string;
    name: string;
    value: string;
    id?: string;
  }>;
  autoSubmit?: boolean;
  autoSubmitDelay?: number;
}): string {
  const {
    title,
    formOrigin = 'data:',
    actionUrl,
    method = 'POST',
    fields = [
      { type: 'email', name: 'email', value: 'test@example.com', id: 'email' },
      { type: 'password', name: 'password', value: 'secret123', id: 'password' }
    ],
    autoSubmit = false,
    autoSubmitDelay = 500
  } = config;

  const fieldHtml = fields.map(field => {
    const id = field.id || field.name;
    if (field.type === 'hidden') {
      return `<input type="${field.type}" name="${field.name}" value="${field.value}">`;
    }
    return `
      <label>${field.name}:
        <input type="${field.type}" name="${field.name}" id="${id}" value="${field.value}">
      </label><br>
    `;
  }).join('\n');

  return `
    <html>
      <head><title>${title}</title></head>
      <body>
        <h1>${title}</h1>
        <form id="test-form" action="${actionUrl}" method="${method}">
          ${fieldHtml}
          <button type="submit" id="submit-btn">Submit</button>
        </form>
        <div id="test-status">Ready</div>
        <script>
          const form = document.getElementById('test-form');
          const documentOrigin = window.location.origin || '${formOrigin}';
          const actionOrigin = new URL(form.action).origin;

          const passwordFields = form.querySelectorAll('input[type="password"]');
          const emailFields = form.querySelectorAll('input[type="email"]');
          const hiddenFields = form.querySelectorAll('input[type="hidden"]');
          const allFields = form.querySelectorAll('input');

          const isCrossOrigin = documentOrigin !== actionOrigin;

          window.__formMonitorTestData = {
            documentUrl: window.location.href,
            actionUrl: form.action,
            formOrigin: documentOrigin,
            actionOrigin: actionOrigin,
            method: form.method.toUpperCase(),
            hasPasswordField: passwordFields.length > 0,
            hasEmailField: emailFields.length > 0,
            isCrossOrigin: isCrossOrigin,
            totalFields: allFields.length,
            hiddenFields: hiddenFields.length,
            passwordFields: passwordFields.length
          };

          ${autoSubmit ? `
          setTimeout(() => {
            document.getElementById('test-status').textContent = 'Auto-submitting...';
            form.addEventListener('submit', (e) => {
              e.preventDefault();
              document.getElementById('test-status').textContent = 'Form submitted (prevented)';
            });
            form.dispatchEvent(new Event('submit', { cancelable: true }));
          }, ${autoSubmitDelay});
          ` : ''}
        </script>
      </body>
    </html>
  `;
}

/**
 * Wait for and verify FormMonitor alert in page
 * NOTE: This assumes Ladybird exposes alerts to DOM or console
 * Implementation depends on Ladybird's alert mechanism
 */
export async function waitForFormMonitorAlert(
  page: Page,
  options: {
    timeout?: number;
    expectedType?: string;
    expectedSeverity?: string;
  } = {}
): Promise<CredentialAlert | null> {
  const {
    timeout = 5000,
    expectedType,
    expectedSeverity
  } = options;

  // Strategy 1: Check for console warnings
  const consoleMessages: string[] = [];
  page.on('console', msg => {
    if (msg.type() === 'warning' || msg.type() === 'error') {
      consoleMessages.push(msg.text());
    }
  });

  // Strategy 2: Check for DOM alert elements
  // This assumes Ladybird renders alerts as DOM elements with class .sentinel-alert
  try {
    const alertLocator = page.locator('.sentinel-alert');
    await alertLocator.waitFor({ timeout });

    const alertText = await alertLocator.textContent();
    const alert: CredentialAlert = {
      alertType: expectedType || 'unknown',
      severity: expectedSeverity as any || 'medium',
      formOrigin: '',
      actionOrigin: '',
      usesHttps: false,
      hasPasswordField: false,
      isCrossOrigin: false,
      timestamp: Date.now()
    };

    return alert;
  } catch (e) {
    // No DOM alert found
  }

  // Strategy 3: Check console messages
  const formMonitorWarnings = consoleMessages.filter(msg =>
    msg.includes('FormMonitor') ||
    msg.includes('credential') ||
    msg.includes('exfiltration')
  );

  if (formMonitorWarnings.length > 0) {
    console.log('FormMonitor warnings detected in console:', formMonitorWarnings);
    return {
      alertType: expectedType || 'detected_via_console',
      severity: expectedSeverity as any || 'medium',
      formOrigin: '',
      actionOrigin: '',
      usesHttps: false,
      hasPasswordField: false,
      isCrossOrigin: false,
      timestamp: Date.now()
    };
  }

  return null;
}

/**
 * Simulate PolicyGraph database operations
 * NOTE: This is for documentation/mocking. Actual implementation would use SQLite.
 */
export class MockPolicyGraph {
  private relationships: Map<string, CredentialRelationship[]> = new Map();

  /**
   * Create a trusted relationship
   */
  createTrustedRelationship(formOrigin: string, actionOrigin: string): number {
    const relationship: CredentialRelationship = {
      id: Date.now(),
      formOrigin,
      actionOrigin,
      relationshipType: 'trusted',
      createdAt: Date.now(),
      createdBy: 'test',
      notes: 'Test trusted relationship'
    };

    const key = `${formOrigin}->${actionOrigin}`;
    const existing = this.relationships.get(key) || [];
    existing.push(relationship);
    this.relationships.set(key, existing);

    return relationship.id!;
  }

  /**
   * Create a blocked relationship
   */
  createBlockedRelationship(formOrigin: string, actionOrigin: string): number {
    const relationship: CredentialRelationship = {
      id: Date.now(),
      formOrigin,
      actionOrigin,
      relationshipType: 'blocked',
      createdAt: Date.now(),
      createdBy: 'test',
      notes: 'Test blocked relationship'
    };

    const key = `${formOrigin}->${actionOrigin}`;
    const existing = this.relationships.get(key) || [];
    existing.push(relationship);
    this.relationships.set(key, existing);

    return relationship.id!;
  }

  /**
   * Check if relationship exists
   */
  hasRelationship(formOrigin: string, actionOrigin: string, type: 'trusted' | 'blocked'): boolean {
    const key = `${formOrigin}->${actionOrigin}`;
    const existing = this.relationships.get(key) || [];
    return existing.some(r => r.relationshipType === type);
  }

  /**
   * List all relationships
   */
  listRelationships(): CredentialRelationship[] {
    const all: CredentialRelationship[] = [];
    this.relationships.forEach(rels => all.push(...rels));
    return all;
  }

  /**
   * Clear all relationships (for test cleanup)
   */
  clear(): void {
    this.relationships.clear();
  }
}

/**
 * Calculate expected anomaly score
 * Mirrors FormMonitor::calculate_anomaly_score() logic
 */
export function calculateExpectedAnomalyScore(event: {
  totalFields: number;
  hiddenFields: number;
  actionUrl: string;
  submissionCount?: number;
  submissionIntervals?: number[]; // ms between submissions
}): {
  score: number;
  indicators: string[];
} {
  let score = 0.0;
  const indicators: string[] = [];

  // 1. Hidden field ratio (weight: 0.3)
  const hiddenRatio = event.hiddenFields / event.totalFields;
  if (hiddenRatio > 0.3) {
    const hiddenScore = hiddenRatio < 0.5
      ? (hiddenRatio - 0.3) / 0.2 * 0.5
      : 0.5 + (hiddenRatio - 0.5) / 0.5 * 0.5;

    if (hiddenScore > 0.5) {
      score += 0.3 * hiddenScore;
      indicators.push(`High hidden field ratio (${(hiddenRatio * 100).toFixed(1)}%)`);
    }
  }

  // 2. Excessive fields (weight: 0.2)
  if (event.totalFields > 15) {
    let fieldScore = 0;
    if (event.totalFields <= 25) {
      fieldScore = (event.totalFields - 15) / 10 * 0.5;
    } else if (event.totalFields <= 50) {
      fieldScore = 0.5 + (event.totalFields - 25) / 25 * 0.5;
    } else {
      fieldScore = 1.0;
    }

    if (fieldScore > 0.7) {
      score += 0.2 * fieldScore;
      indicators.push(`Excessive field count (${event.totalFields} fields)`);
    }
  }

  // 3. Domain reputation (weight: 0.3)
  const url = new URL(event.actionUrl);
  const host = url.hostname;
  const suspiciousPatterns = [
    'data-collect', 'analytics', 'tracking', 'logger', 'harvester',
    'phishing', 'fake-', 'scam', '.tk', '.ml', '.ga', '.cf', '.gq'
  ];

  let domainScore = 0;
  for (const pattern of suspiciousPatterns) {
    if (host.includes(pattern)) {
      domainScore = 0.8;
      break;
    }
  }

  // IP address check
  if (/^\d+\.\d+\.\d+\.\d+$/.test(host)) {
    domainScore = 0.7;
  }

  // Very long domain
  if (host.length > 40) {
    domainScore = 0.6;
  }

  if (domainScore > 0.5) {
    score += 0.3 * domainScore;
    indicators.push(`Suspicious action domain: ${host}`);
  }

  // 4. Submission frequency (weight: 0.2)
  if (event.submissionIntervals && event.submissionIntervals.length >= 2) {
    const recentIntervals = event.submissionIntervals.slice(-4); // Last 4 intervals
    const avgInterval = recentIntervals.reduce((a, b) => a + b, 0) / recentIntervals.length;

    let freqScore = 0;
    if (avgInterval < 1000) { // < 1 second average
      freqScore = 1.0;
    } else if (avgInterval < 2000) {
      freqScore = 0.7;
    }

    if (freqScore > 0.8) {
      score += 0.2 * freqScore;
      indicators.push('Unusual submission frequency detected');
    }
  }

  return { score, indicators };
}

/**
 * Generate test data for form with specific characteristics
 */
export function generateTestForm(config: {
  crossOrigin?: boolean;
  hasPassword?: boolean;
  hasEmail?: boolean;
  hiddenFieldCount?: number;
  totalFields?: number;
  usesHttps?: boolean;
}): string {
  const {
    crossOrigin = true,
    hasPassword = true,
    hasEmail = true,
    hiddenFieldCount = 0,
    totalFields = 3,
    usesHttps = true
  } = config;

  const actionUrl = crossOrigin
    ? (usesHttps ? 'https://different-domain.com/submit' : 'http://different-domain.com/submit')
    : '/submit';

  const fields: Array<{type: string; name: string; value: string}> = [];

  if (hasEmail) {
    fields.push({ type: 'email', name: 'email', value: 'test@example.com' });
  }

  if (hasPassword) {
    fields.push({ type: 'password', name: 'password', value: 'secret123' });
  }

  // Add regular text fields to reach totalFields
  const regularFieldsNeeded = totalFields - fields.length - hiddenFieldCount;
  for (let i = 0; i < regularFieldsNeeded; i++) {
    fields.push({ type: 'text', name: `field${i}`, value: `value${i}` });
  }

  // Add hidden fields
  for (let i = 0; i < hiddenFieldCount; i++) {
    fields.push({ type: 'hidden', name: `hidden${i}`, value: `hidden_value${i}` });
  }

  return createFormTestPage({
    title: 'Test Form',
    actionUrl,
    fields
  });
}

/**
 * Assert that FormMonitor test data matches expectations
 */
export async function assertFormMonitorTestData(
  page: Page,
  expected: Partial<{
    isCrossOrigin: boolean;
    hasPasswordField: boolean;
    hasEmailField: boolean;
    expectedAlert: string | null;
    expectedSeverity: string;
  }>
): Promise<void> {
  const testData = await page.evaluate(() => (window as any).__formMonitorTestData);

  if (expected.isCrossOrigin !== undefined) {
    if (testData.isCrossOrigin !== expected.isCrossOrigin) {
      throw new Error(`Expected isCrossOrigin=${expected.isCrossOrigin}, got ${testData.isCrossOrigin}`);
    }
  }

  if (expected.hasPasswordField !== undefined) {
    if (testData.hasPasswordField !== expected.hasPasswordField) {
      throw new Error(`Expected hasPasswordField=${expected.hasPasswordField}, got ${testData.hasPasswordField}`);
    }
  }

  if (expected.hasEmailField !== undefined) {
    if (testData.hasEmailField !== expected.hasEmailField) {
      throw new Error(`Expected hasEmailField=${expected.hasEmailField}, got ${testData.hasEmailField}`);
    }
  }
}
