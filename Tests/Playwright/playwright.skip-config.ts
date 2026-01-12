/**
 * Tests to skip for Ladybird until features are implemented
 */
export const LADYBIRD_SKIP_TESTS = [
  // Tab management - keyboard shortcuts not implemented
  'TAB-001', 'TAB-002', 'TAB-003', 'TAB-004',
  'TAB-006', 'TAB-007', 'TAB-008', 'TAB-009', 'TAB-011',

  // History - private browsing mode
  'HIST-008',

  // Navigation - anchor fragments (use patched versions)
  'NAV-008', 'NAV-009',

  // Web APIs - storage in data: URLs (use patched versions)
  'JS-API-001', 'JS-API-002', 'JS-API-009',

  // Rendering - anchor scrolling (use patched version)
  'HTML-008',

  // CSS - pseudo-element encoding
  'VIS-014',
];

export function shouldSkipTest(testName: string): boolean {
  return LADYBIRD_SKIP_TESTS.some(skip => testName.includes(skip));
}
