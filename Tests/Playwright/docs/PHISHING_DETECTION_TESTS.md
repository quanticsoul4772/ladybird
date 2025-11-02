# Phishing Detection Test Suite

## Overview

Comprehensive Playwright test suite for Sentinel's phishing URL detection system. Tests cover homograph attacks, typosquatting, suspicious TLDs, IP addresses, subdomain abuse, and PolicyGraph integration.

**Test Suite**: `tests/security/phishing-detection.spec.ts`
**Fixtures**: `fixtures/phishing/*.html`
**Utilities**: `helpers/phishing-test-utils.ts`

## Test Coverage

### PHISH-001: Homograph Attack Detection
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Tests ICU-based Unicode homograph detection.

**Test Case**:
- Domain: `аpple.com` (Cyrillic 'а' - U+0430)
- Punycode: `xn--pple-43d.com`
- Target: `apple.com`

**Expected Behavior**:
- `isHomographAttack: true`
- `phishingScore >= 0.3` (30% weight)
- Alert displayed with explanation
- ICU spoofchecker flags confusable characters

**Implementation**:
- Core: `Services/Sentinel/PhishingURLAnalyzer::detect_homograph()`
- Algorithm: ICU `uspoof_checkUTF8()` with `USPOOF_CONFUSABLE`
- Fixture: `fixtures/phishing/homograph-test.html`

---

### PHISH-002: Legitimate IDN (No False Positive)
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Ensures legitimate internationalized domains don't trigger false alerts.

**Test Cases**:
- `münchen.de` (German umlaut)
- `президент.рф` (Russian Cyrillic)
- `日本.jp` (Japanese)

**Expected Behavior**:
- `detected: false`
- `phishingScore: 0.0`
- No alert displayed
- Legitimate IDN recognized

**Key Distinction**:
- Single-script domains (all Cyrillic, all Arabic) = legitimate
- Mixed-script with visual similarity = homograph attack

---

### PHISH-003: Typosquatting Detection
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Detects intentional misspellings of popular domains.

**Test Cases**:
- `faceboook.com` → `facebook.com` (edit distance: 1)
- `googlle.com` → `google.com` (edit distance: 2)
- `paypai.com` → `paypal.com` (edit distance: 1)
- `amazom.com` → `amazon.com` (edit distance: 1)

**Expected Behavior**:
- `isTyposquatting: true`
- `phishingScore >= 0.25` (25% weight)
- `closestLegitDomain` identified
- `editDistance <= 3`

**Implementation**:
- Algorithm: Levenshtein distance calculation
- Database: 100+ popular domains
- Threshold: Distance 1-3 = typosquatting

---

### PHISH-004: Suspicious TLD Detection
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Flags free and high-abuse TLDs.

**Suspicious TLDs**:
- Free Freenom: `.tk`, `.ml`, `.ga`, `.cf`, `.gq`
- High Abuse: `.xyz`, `.top`, `.club`, `.work`, `.click`, `.link`
- Other: `.download`, `.stream`, `.online`, `.site`, `.website`

**Test Cases**:
- `secure-bank.xyz`
- `paypal-verify.top`
- `amazon-login.tk`

**Expected Behavior**:
- `hasSuspiciousTLD: true`
- `phishingScore >= 0.2` (20% weight)
- TLD included in alert message

---

### PHISH-005: IP Address URL Detection
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Detects phishing via raw IP addresses.

**Test Cases**:
- IPv4: `http://192.168.1.100/login`
- HTTPS IPv4: `https://203.0.113.45/secure`
- IPv6: `http://[2001:db8::1]/banking`

**Expected Behavior**:
- `isIPAddress: true`
- `phishingScore >= 0.25` (25% weight)
- `shouldBlock: true`
- Strong warning displayed

**Rationale**:
- Legitimate sites use domain names, not IPs
- IP addresses evade SSL certificate verification
- Often used for credential phishing

---

### PHISH-006: Excessive Subdomains
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Detects subdomain abuse to impersonate brands.

**Test Cases**:
- `login.secure.account.paypal.phishing.com` (4 subdomains)
- `verify.secure.chase.bank-login.xyz` (3 subdomains)
- `account.verify.amazon.secure-login.top` (3 subdomains)

**Expected Behavior**:
- `excessiveSubdomains: true`
- `phishingScore >= 0.15` (15% weight)
- `actualDomain` identified (e.g., `phishing.com`)
- `impersonatedBrand` detected

**Detection Logic**:
- Count subdomains (parts before actual domain)
- Threshold: 3+ subdomains suspicious
- Check for popular brand names in subdomains

---

### PHISH-007: PolicyGraph Whitelist
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Tests PolicyGraph whitelist override.

**Scenario**:
1. Domain flagged as phishing (e.g., `secure-internal.xyz`)
2. User previously chose "Trust"
3. PolicyGraph stores decision
4. No alert on subsequent visits

**Expected Behavior**:
- First visit: Alert shown
- User clicks "Trust"
- Decision saved in PolicyGraph
- Second visit: Alert suppressed
- `whitelisted: true`

**Implementation**:
- PolicyGraph: `Services/Sentinel/PolicyGraph.cpp`
- Storage: SQLite database
- Key: `(domain, resource_type) → action`

---

### PHISH-008: User Decision Persistence
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Verifies user decisions persist across sessions.

**Scenario**:
1. Phishing detected on `paypai.com`
2. User clicks "Trust" button
3. Decision saved: `(domain='paypai.com', action='trust')`
4. Browser closed and reopened
5. Navigate to `paypai.com` again
6. No alert shown (decision persisted)

**Expected Behavior**:
- `userDecisionSaved: true`
- `decisionPersisted: true`
- No repeat alerts
- PolicyGraph database intact after restart

---

### PHISH-009: HTTPS Phishing
**Status**: ✅ Implemented
**Priority**: P0 (Critical)

Tests that HTTPS doesn't suppress phishing detection.

**Key Point**: **Encryption ≠ Safety**

**Test Case**:
- URL: `https://secure-login-apple.com.phishing.xyz`
- HTTPS: ✅ (encrypted)
- Phishing: ✅ (still malicious)

**Detection**:
- Suspicious TLD: `.xyz` (+0.2)
- Subdomain abuse: `secure-login-apple.com` (+0.15)
- Total score: `0.35` (DANGEROUS)

**Expected Behavior**:
- `isHTTPS: true`
- `phishingScore >= 0.3`
- `threatLevel: 'dangerous'`
- Alert shown regardless of HTTPS

**User Education**:
> "HTTPS only encrypts traffic - it doesn't verify legitimacy!"

---

### PHISH-010: Edge Cases
**Status**: ✅ Implemented
**Priority**: P1 (High)

Tests graceful error handling.

**Edge Cases**:
1. Empty host: `http://`
2. Malformed URL: `ht!tp://invalid`
3. Data URL: `data:text/html,<h1>Test</h1>`
4. JavaScript URL: `javascript:alert(1)`
5. Very long domain: 300+ characters
6. Localhost: `http://localhost/test`

**Expected Behavior**:
- No crashes or exceptions
- Invalid URLs: `error` returned
- Non-HTTP protocols: Score `0.0` (not analyzed)
- Localhost: Score `0.0` (trusted)
- All cases handled gracefully

---

## Test Utilities

### `phishing-test-utils.ts`

**Functions**:
- `loadPhishingFixture(page, fixture)`: Load test HTML
- `triggerPhishingTest(page, button)`: Click test button
- `getDetectionResult(page)`: Get detection data
- `verifyPhishingAlert(page, keywords)`: Check alert displayed
- `verifyNoAlert(page)`: Verify no false positive
- `levenshteinDistance(a, b)`: Calculate edit distance
- `findClosestPopularDomain(domain)`: Find typosquatting target
- `isSuspiciousTLD(tld)`: Check TLD against blocklist
- `isIPAddress(hostname)`: Detect IP addresses
- `countSubdomains(hostname)`: Count subdomain levels

**Constants**:
- `HOMOGRAPH_DATABASE`: Unicode confusables
- `POPULAR_DOMAINS`: 100+ domains for typosquatting checks
- `SUSPICIOUS_TLDS`: Free and high-abuse TLDs

---

## Running Tests

### Run All Phishing Tests

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npx playwright test tests/security/phishing-detection.spec.ts --project=ladybird
```

### Run Single Test

```bash
# Homograph attack test
npx playwright test tests/security/phishing-detection.spec.ts -g "PHISH-001"

# Typosquatting test
npx playwright test tests/security/phishing-detection.spec.ts -g "PHISH-003"

# HTTPS phishing test
npx playwright test tests/security/phishing-detection.spec.ts -g "PHISH-009"
```

### Debug Mode

```bash
npx playwright test tests/security/phishing-detection.spec.ts --debug
```

### Generate HTML Report

```bash
npx playwright test tests/security/phishing-detection.spec.ts --reporter=html
npx playwright show-report
```

---

## Test Fixtures

### Fixture Structure

Each test has a dedicated HTML fixture:

```
fixtures/phishing/
├── homograph-test.html          # PHISH-001
├── legitimate-idn.html          # PHISH-002
├── typosquatting-test.html      # PHISH-003
├── suspicious-tld-test.html     # PHISH-004
├── ip-address-test.html         # PHISH-005
└── subdomain-abuse-test.html    # PHISH-006
```

### Fixture Features

- **Interactive UI**: Buttons trigger detection tests
- **Test Data**: `window.__phishingTestData` exposed for Playwright
- **Visual Feedback**: Results displayed in page
- **Documentation**: Explains detection technique
- **Examples**: Real-world phishing patterns

### Loading Fixtures

```typescript
const testData = await loadPhishingFixture(page, 'homograph-test.html');
// testData contains test metadata, expected results, etc.

await triggerPhishingTest(page, '#test-button');
// Clicks button, waits for detection

const result = await getDetectionResult(page);
// result = { detected: true, phishingScore: 0.3, ... }
```

---

## Integration with Ladybird

### Detection Flow

1. **User navigates to URL**
   - `Services/RequestServer/ConnectionFromClient.cpp`

2. **URLSecurityAnalyzer invoked**
   - `Services/RequestServer/URLSecurityAnalyzer::analyze_url()`

3. **PhishingURLAnalyzer analyzes**
   - `Services/Sentinel/PhishingURLAnalyzer::analyze_url()`
   - Homograph detection (ICU)
   - Typosquatting (Levenshtein distance)
   - Suspicious TLD check
   - IP address detection
   - Domain entropy analysis

4. **PolicyGraph consulted**
   - Check if domain whitelisted
   - Retrieve previous user decision

5. **Threat score calculated**
   - Combine all indicators
   - Score: 0.0 (safe) to 1.0 (dangerous)
   - Threshold: 0.3+ = suspicious, 0.6+ = dangerous

6. **User alerted** (if score >= 0.3)
   - PageClient displays security dialog
   - User chooses: Trust / Block / View Details
   - Decision saved to PolicyGraph

7. **Navigation proceeds or blocked**
   - Based on user decision or policy

### Key Files

**Detection Core**:
- `Services/Sentinel/PhishingURLAnalyzer.{h,cpp}`
- `Services/Sentinel/PolicyGraph.{h,cpp}`

**Integration**:
- `Services/RequestServer/URLSecurityAnalyzer.{h,cpp}`
- `Services/RequestServer/ConnectionFromClient.cpp`

**UI**:
- `Services/WebContent/PageClient.{h,cpp}`

**Tests**:
- `Tests/Playwright/tests/security/phishing-detection.spec.ts`
- `Tests/Playwright/fixtures/phishing/*.html`
- `Tests/Playwright/helpers/phishing-test-utils.ts`

---

## Verification Checklist

Before marking tests as complete, verify:

### ✅ Test Execution
- [ ] All 10 tests pass
- [ ] No crashes or exceptions
- [ ] Fixtures load correctly
- [ ] Test data exposed properly

### ✅ Detection Accuracy
- [ ] Homograph attacks detected
- [ ] Legitimate IDNs not flagged
- [ ] Typosquatting identified correctly
- [ ] Suspicious TLDs flagged
- [ ] IP addresses detected
- [ ] Subdomain abuse caught

### ✅ Scoring
- [ ] Homograph: +0.3 score
- [ ] Typosquatting: +0.25 score
- [ ] Suspicious TLD: +0.2 score
- [ ] IP address: +0.25 score
- [ ] Subdomain abuse: +0.15 score
- [ ] Scores combine correctly

### ✅ PolicyGraph Integration
- [ ] Whitelist overrides detection
- [ ] User decisions persisted
- [ ] No repeat alerts for trusted domains
- [ ] Database survives restarts

### ✅ Edge Cases
- [ ] Malformed URLs handled
- [ ] Data/JavaScript URLs ignored
- [ ] Localhost trusted
- [ ] Very long domains handled
- [ ] Empty hosts graceful

### ✅ Documentation
- [ ] All tests documented
- [ ] Homograph database complete
- [ ] Fixtures have clear instructions
- [ ] Utilities have JSDoc comments

---

## Future Enhancements

### Planned Tests (Not in Scope)

- **PHISH-011**: Real-world testing against PhishTank database
- **PHISH-012**: Machine learning false positive analysis
- **PHISH-013**: Internationalized typosquatting (e.g., Cyrillic to Latin)
- **PHISH-014**: Combo attacks (homograph + suspicious TLD + subdomain)
- **PHISH-015**: Performance testing (10,000+ domains)

### Additional Features

1. **Domain Reputation Integration**
   - VirusTotal API
   - Google Safe Browsing
   - OpenPhish database

2. **Advanced Heuristics**
   - Brand impersonation detection (keywords in path)
   - Short-lived domain detection (WHOIS age)
   - Parking page detection

3. **User Feedback Loop**
   - Report false positives
   - Contribute to blocklist
   - Community reputation scores

---

## References

- **Homograph Database**: `docs/PHISHING_HOMOGRAPH_DATABASE.md`
- **PhishingURLAnalyzer**: `Services/Sentinel/PhishingURLAnalyzer.cpp`
- **Sentinel Architecture**: `docs/SENTINEL_ARCHITECTURE.md`
- **ICU Spoofchecker**: https://unicode-org.github.io/icu/userguide/transforms/security.html
- **Unicode Security**: https://www.unicode.org/reports/tr39/

---

**Last Updated**: 2025-11-01
**Test Suite Version**: 1.0
**Status**: ✅ All 10 tests implemented and documented
