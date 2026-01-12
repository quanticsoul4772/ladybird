# Sentinel Security Test Suite Index

**Location**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/security/`
**Last Updated**: 2025-11-01

---

## Test Suites

### 1. FormMonitor Credential Protection Tests

**Status**: ✅ Complete (12 tests)
**Priority**: P0 (Critical) - FLAGSHIP FORK FEATURE
**Coverage**: Credential exfiltration, PolicyGraph integration, anomaly detection

**Files**:
- 📄 `form-monitor.spec.ts` (1,162 lines) - Main test suite
- 📄 `form-monitor-helpers.ts` (524 lines) - Utility functions
- 📄 `README-FORM-MONITOR.md` (626 lines) - Comprehensive documentation
- 📄 `FORM-MONITOR-TEST-SUMMARY.md` - Implementation summary

**Test IDs**: FMON-001 to FMON-012

**Key Features Tested**:
- Cross-origin credential submission detection
- Same-origin false positive prevention
- Trusted relationship management (PolicyGraph)
- Autofill protection
- Form anomaly detection (hidden fields, timing attacks)
- Multiple password fields
- Dynamic form injection
- User interaction tracking
- Alert UI and decision handling
- Edge cases (empty passwords, special characters, subdomains)

**Run Tests**:
```bash
npx playwright test tests/security/form-monitor.spec.ts --project=ladybird
```

---

### 2. Fingerprinting Detection Tests

**Status**: ✅ Complete (12 tests)
**Priority**: P0 (Critical) - FORK FEATURE
**Coverage**: Browser fingerprinting detection across multiple APIs

**Files**:
- 📄 `fingerprinting.spec.ts` (500 lines)

**Test IDs**: FP-001 to FP-012

**Key Features Tested**:
- Canvas fingerprinting (toDataURL, getImageData)
- WebGL fingerprinting (getParameter, renderer info)
- Audio fingerprinting (OscillatorNode, AnalyserNode)
- Navigator enumeration
- Font enumeration
- Screen properties
- Aggressive fingerprinting scoring
- Rapid-fire API calls
- False positive prevention
- Real-world testing (browserleaks.com)

**Run Tests**:
```bash
npx playwright test tests/security/fingerprinting.spec.ts --project=ladybird
```

---

## Test Fixtures

### Form Testing Fixtures

**Location**: `/home/rbsmith4/ladybird/Tests/Playwright/fixtures/forms/`

| Fixture | Purpose | Expected Behavior |
|---------|---------|-------------------|
| `legitimate-login.html` | Same-origin login | ✅ No alert |
| `phishing-attack.html` | Credential exfiltration | ⚠️ HIGH severity alert |
| `oauth-flow.html` | OAuth/SSO flow | ⚠️ Alert first time, user trusts |
| `data-harvesting.html` | Excessive hidden fields | ⚠️ Anomaly detection |

**Load Fixtures**:
```bash
# In Ladybird browser
file:///home/rbsmith4/ladybird/Tests/Playwright/fixtures/forms/phishing-attack.html
```

---

## Test Utilities

### FormMonitor Helpers

**File**: `tests/security/form-monitor-helpers.ts`

**Functions**:
- `extractOrigin(url)` - Origin extraction with port normalization
- `isCrossOriginSubmission(formUrl, actionUrl)` - Cross-origin detection
- `createFormTestPage(config)` - Generate test HTML
- `waitForFormMonitorAlert(page)` - Alert detection
- `MockPolicyGraph` - In-memory PolicyGraph for testing
- `calculateExpectedAnomalyScore(event)` - Anomaly score calculator
- `generateTestForm(config)` - Test form generator
- `assertFormMonitorTestData(page, expected)` - Test data validator

**Import**:
```typescript
import {
  extractOrigin,
  isCrossOriginSubmission,
  createFormTestPage,
  MockPolicyGraph
} from './form-monitor-helpers';
```

---

## Documentation

### FormMonitor Documentation

1. **`README-FORM-MONITOR.md`** (626 lines)
   - Test architecture and strategy
   - Integration points diagram
   - Detailed test catalog (all 12 tests)
   - Code references for each test
   - Running instructions
   - Fixture descriptions
   - Known limitations
   - Future enhancements
   - Troubleshooting guide

2. **`FORM-MONITOR-TEST-SUMMARY.md`**
   - Implementation summary
   - Deliverables checklist
   - Code references table
   - Test strategy
   - Security value proposition
   - Next steps
   - Metrics and statistics

---

## Running Tests

### Run All Security Tests

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npx playwright test tests/security/ --project=ladybird
```

### Run Specific Suite

```bash
# FormMonitor tests
npx playwright test tests/security/form-monitor.spec.ts --project=ladybird

# Fingerprinting tests
npx playwright test tests/security/fingerprinting.spec.ts --project=ladybird
```

### Run Specific Test

```bash
# By test ID
npx playwright test tests/security/form-monitor.spec.ts -g "FMON-001"
npx playwright test tests/security/fingerprinting.spec.ts -g "FP-001"

# By test name
npx playwright test -g "Cross-origin password submission"
```

### Generate HTML Report

```bash
npx playwright test tests/security/ --project=ladybird --reporter=html
npx playwright show-report
```

### Debug Mode

```bash
DEBUG=pw:browser npx playwright test tests/security/form-monitor.spec.ts --project=ladybird
```

---

## Test Statistics

| Metric | FormMonitor | Fingerprinting | Total |
|--------|-------------|----------------|-------|
| Test Count | 12 | 12 | 24 |
| Test File Lines | 1,162 | 500 | 1,662 |
| Helper Lines | 524 | 0 | 524 |
| Documentation Lines | 626 | 0 | 626 |
| Fixture Files | 4 | 0 | 4 |
| Total Lines of Code | 2,312 | 500 | 2,812 |

---

## Integration with Ladybird

### Code References

#### FormMonitor Integration

| Component | File | Function |
|-----------|------|----------|
| Form submission entry | `Services/WebContent/PageClient.cpp:583` | `page_did_submit_form()` |
| FormMonitor instance | `Services/WebContent/PageClient.h:38` | `form_monitor()` |
| Suspicious check | `Services/WebContent/FormMonitor.cpp:127` | `is_suspicious_submission()` |
| Alert generation | `Services/WebContent/FormMonitor.cpp:157` | `analyze_submission()` |
| PolicyGraph integration | `Services/WebContent/FormMonitor.cpp:25` | `load_relationships_from_database()` |

#### FingerprintingDetector Integration

| Component | File | Function |
|-----------|------|----------|
| Detector instance | `Services/WebContent/PageClient.h:41` | `fingerprinting_detector()` |
| Canvas API hook | `Libraries/LibWeb/HTML/HTMLCanvasElement.cpp` | `to_data_url()` |
| WebGL API hook | `Libraries/LibWeb/WebGL/WebGLRenderingContextBase.h` | `get_parameter()` |
| Navigator hook | `Libraries/LibWeb/HTML/Navigator.cpp` | Property getters |

---

## Known Limitations

### All Security Tests

1. **Cross-Origin Testing**
   - Tests use `data:` URIs which have opaque origins
   - Cannot test true cross-origin scenarios
   - Need local HTTP server for real cross-origin tests

2. **Alert UI Detection**
   - Playwright cannot detect native Ladybird alert dialogs
   - Tests check `window.__testData` instead
   - Need Ladybird to expose alerts to DOM or console

3. **PolicyGraph Database**
   - Tests cannot directly manipulate SQLite database
   - Use `MockPolicyGraph` for in-memory testing
   - Need integration tests with real database

4. **Platform Differences**
   - Tests may behave differently on Qt vs AppKit
   - Some APIs may not be fully implemented yet
   - Need to test on multiple platforms

---

## Future Enhancements

### Phase 1: Integration Tests (Short-term)

- [ ] Set up local HTTP server for multi-origin tests
- [ ] Test with actual PolicyGraph database
- [ ] Verify alerts show in Ladybird UI
- [ ] Test user decision callbacks

### Phase 2: Real-World Testing (Medium-term)

- [ ] Test against known phishing sites (with consent)
- [ ] Test OAuth flows from real providers (Google, GitHub)
- [ ] Load forms from popular websites (Amazon, PayPal)
- [ ] Performance testing with complex forms

### Phase 3: Automation (Long-term)

- [ ] CI/CD integration (GitHub Actions)
- [ ] Code coverage reporting
- [ ] Regression testing on every commit
- [ ] Cross-platform testing (Linux, macOS, Windows)

### Phase 4: Fuzzing (Long-term)

- [ ] Fuzz form action URLs (data:, javascript:, file:)
- [ ] Fuzz password field values (Unicode, XSS, SQL injection)
- [ ] Fuzz PolicyGraph database (corruption, race conditions)
- [ ] Fuzz fingerprinting API calls (extreme values)

---

## Contributing

### Adding New Security Tests

1. **Choose Test Suite**:
   - Credential protection → `form-monitor.spec.ts`
   - Fingerprinting detection → `fingerprinting.spec.ts`
   - New feature → Create new `feature-name.spec.ts`

2. **Follow Naming Convention**:
   - Test ID: `FEATURE-XXX` (e.g., FMON-013)
   - Test description: Clear, security-focused
   - Tag: `{ tag: '@p0' }` for critical tests

3. **Document Expected Behavior**:
   - Add code references
   - Document security purpose
   - Explain threat scenario

4. **Update Documentation**:
   - Add test to catalog
   - Update test count
   - Add to this index

### Test Review Checklist

- [ ] Test has unique ID (FMON-XXX, FP-XXX)
- [ ] Test description is clear and security-focused
- [ ] Code references documented
- [ ] Security purpose explained
- [ ] Expected behavior documented
- [ ] Test runs without crash
- [ ] Test data accessible via `window.__testData`
- [ ] Added to documentation

---

## Troubleshooting

### Common Issues

**Issue**: Tests timeout waiting for page load
**Fix**: Increase `navigationTimeout` in config or test

**Issue**: `window.__testData` is undefined
**Fix**: Wait for page script to execute (`await page.waitForTimeout(200)`)

**Issue**: Tests fail on Ladybird but pass on Chromium
**Fix**: Check if feature is implemented in Ladybird, document limitation

**Issue**: PolicyGraph database locked
**Fix**: Close other Ladybird instances, use test database path

### Debug Logging

Enable Ladybird debug output:

```bash
WEBCONTENT_DEBUG=1 LIBWEB_DEBUG=1 ./Build/release/bin/Ladybird
```

Enable Playwright debug:

```bash
DEBUG=pw:* npx playwright test
```

---

## Contact and Support

### Documentation

- **FormMonitor**: See `docs/USER_GUIDE_CREDENTIAL_PROTECTION.md`
- **Fingerprinting**: See `docs/FINGERPRINTING_DETECTION_ARCHITECTURE.md`
- **PolicyGraph**: See `docs/SENTINEL_POLICY_GUIDE.md`
- **Sentinel Architecture**: See `docs/SENTINEL_ARCHITECTURE.md`

### Code

- **FormMonitor**: `Services/WebContent/FormMonitor.{h,cpp}`
- **FingerprintingDetector**: `Services/Sentinel/FingerprintingDetector.{h,cpp}`
- **PolicyGraph**: `Services/Sentinel/PolicyGraph.{h,cpp}`
- **PageClient**: `Services/WebContent/PageClient.{h,cpp}`

### Test Maintenance

- **Test Owner**: Security Test Agent
- **Last Updated**: 2025-11-01
- **Status**: ✅ Active, maintained

---

## Quick Reference

### Test Commands

```bash
# List all tests
npx playwright test tests/security/ --list

# Run FormMonitor tests
npx playwright test tests/security/form-monitor.spec.ts --project=ladybird

# Run Fingerprinting tests
npx playwright test tests/security/fingerprinting.spec.ts --project=ladybird

# Run specific test
npx playwright test -g "FMON-001"

# Generate report
npx playwright test tests/security/ --reporter=html && npx playwright show-report

# Debug mode
DEBUG=pw:browser npx playwright test tests/security/form-monitor.spec.ts
```

### File Locations

```
Tests/Playwright/
├── tests/security/
│   ├── form-monitor.spec.ts         (12 tests, 1,162 lines)
│   ├── form-monitor-helpers.ts      (524 lines)
│   ├── fingerprinting.spec.ts       (12 tests, 500 lines)
│   ├── README-FORM-MONITOR.md       (626 lines)
│   ├── FORM-MONITOR-TEST-SUMMARY.md
│   └── INDEX.md                     (this file)
└── fixtures/forms/
    ├── legitimate-login.html
    ├── phishing-attack.html
    ├── oauth-flow.html
    └── data-harvesting.html
```

### Test IDs

- **FMON-001** to **FMON-012**: FormMonitor credential protection
- **FP-001** to **FP-012**: Fingerprinting detection

---

**Document Version**: 1.0
**Last Updated**: 2025-11-01
**Status**: ✅ Complete
