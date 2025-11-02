# FormMonitor Credential Protection Tests

**Priority**: P0 (Critical) - FLAGSHIP FORK FEATURE
**Test Count**: 12 tests (FMON-001 to FMON-012)
**Coverage**: Credential exfiltration detection, PolicyGraph integration, anomaly detection

---

## Overview

FormMonitor is Sentinel's **flagship security feature** that protects users from credential phishing and data harvesting attacks. It monitors all form submissions and alerts users when credentials are being sent to untrusted cross-origin domains.

**Zero test coverage before this suite** - these tests are critical for validating the fork's core value proposition.

---

## Test Architecture

### Test Strategy

1. **Unit-level validation**: Each test verifies specific FormMonitor behaviors using inline HTML
2. **Data URI approach**: Most tests use `data:` URIs to avoid multi-origin server setup
3. **Cross-origin simulation**: Tests document expected behavior when actual cross-origin occurs
4. **PolicyGraph integration**: Tests verify database persistence patterns (documented for future integration)
5. **Anomaly detection**: Tests validate FormMonitor's anomaly scoring algorithms

### Integration Points

```
Browser Page Load
       ↓
LibWeb HTMLFormElement::submit()
       ↓
page.client().page_did_submit_form(form, method, action)
       ↓
PageClient::page_did_submit_form()
       ↓
FormMonitor::on_form_submit(event)
       ↓
       ├─→ is_suspicious_submission() → analyze_submission() → Alert UI
       ├─→ is_trusted_relationship() → PolicyGraph lookup
       ├─→ calculate_anomaly_score() → Anomaly indicators
       └─→ is_blocked_submission() → Prevent submission
```

### Test Data Structure

All tests expose `window.__formMonitorTestData` for verification:

```typescript
interface FormMonitorTestData {
  documentUrl: string;
  actionUrl: string;
  formOrigin: string;
  actionOrigin: string;
  hasPasswordField: boolean;
  hasEmailField: boolean;
  isCrossOrigin: boolean;
  expectedAlert: string | null;
  expectedSeverity: 'low' | 'medium' | 'high' | 'critical';
  // ... additional test-specific fields
}
```

---

## Test Catalog

### FMON-001: Cross-origin password submission detection

**Purpose**: Detect credential exfiltration (phishing attack)

**Scenario**:
- Form loaded from `example.com`
- Form action: `https://evil.com/steal-credentials`
- Password field present

**Expected Behavior**:
- `FormMonitor.is_suspicious_submission()` returns `true`
- `FormMonitor.analyze_submission()` returns alert:
  - `alert_type`: `"credential_exfiltration"`
  - `severity`: `"high"`
  - `is_cross_origin`: `true`
- User sees security alert via IPC

**Code References**:
- `Services/WebContent/FormMonitor.cpp:127` - `is_suspicious_submission()`
- `Services/WebContent/FormMonitor.cpp:157` - `analyze_submission()`
- `Services/WebContent/FormMonitor.cpp:175` - Alert type logic

---

### FMON-002: Same-origin password submission (no alert)

**Purpose**: Prevent false positives for legitimate logins

**Scenario**:
- Form loaded from `example.com`
- Form action: `/login` (relative URL, resolves to same origin)
- Password field present

**Expected Behavior**:
- `FormMonitor.is_suspicious_submission()` returns `false`
- No alert shown
- Form proceeds normally

**Code References**:
- `Services/WebContent/FormMonitor.cpp:215` - `is_cross_origin_submission()`
- Early return at line 143 if not cross-origin

---

### FMON-003: Trusted relationship (whitelisted cross-origin)

**Purpose**: Allow users to trust legitimate OAuth/SSO flows

**Scenario**:
- Cross-origin submission to `auth0.com`
- User previously clicked "Trust" for this origin pair
- PolicyGraph contains trusted relationship

**Expected Behavior**:
- `FormMonitor.is_trusted_relationship()` returns `true`
- `FormMonitor.is_suspicious_submission()` returns `false` (early return at line 133)
- No alert shown
- Form proceeds

**Code References**:
- `Services/WebContent/FormMonitor.cpp:315` - `is_trusted_relationship()`
- `Services/WebContent/FormMonitor.cpp:280` - `learn_trusted_relationship()`
- `Services/Sentinel/PolicyGraph.cpp` - Database persistence

**PolicyGraph Schema**:
```sql
CREATE TABLE credential_relationships (
  id INTEGER PRIMARY KEY,
  form_origin TEXT NOT NULL,
  action_origin TEXT NOT NULL,
  relationship_type TEXT NOT NULL,  -- 'trusted' or 'blocked'
  created_at INTEGER NOT NULL,
  created_by TEXT,
  notes TEXT
);
```

---

### FMON-004: Autofill protection

**Purpose**: Detect when autofilled credentials are exfiltrated

**Scenario**:
- Browser autofills username/password
- Cross-origin submission without explicit user action
- Higher risk than manual entry

**Expected Behavior**:
- Alert includes autofill warning
- User prompted to verify they intended this submission
- One-time override mechanism available

**Code References**:
- `Services/WebContent/FormMonitor.cpp:388` - `grant_autofill_override()`
- `Services/WebContent/PageClient.cpp` - Autofill tracking integration point

---

### FMON-005: Hidden field detection (form anomaly)

**Purpose**: Detect data harvesting via hidden fields

**Scenario**:
- Form with 10 total fields
- 7 are hidden fields (70% hidden ratio)
- Cross-origin submission

**Expected Behavior**:
- `FormMonitor.check_hidden_field_ratio()` returns score > 0.5
- `anomaly_indicators` includes: `"High hidden field ratio (70.0%)"`
- Alert severity elevated due to anomaly

**Code References**:
- `Services/WebContent/FormMonitor.cpp:435` - `check_hidden_field_ratio()`
- `Services/WebContent/FormMonitor.cpp:572` - `calculate_anomaly_score()`

**Anomaly Scoring**:
```cpp
// Hidden ratio thresholds
< 0.3  → 0.0 (normal)
0.3-0.5 → 0.0-0.5 (suspicious)
> 0.5  → 0.5-1.0 (very suspicious)

// Weight: 0.3 (30% of total anomaly score)
```

---

### FMON-006: Multiple password fields

**Purpose**: Track all password fields in change-password forms

**Scenario**:
- Form with 3 password fields:
  - `old_password`
  - `new_password`
  - `confirm_password`
- Cross-origin submission

**Expected Behavior**:
- All password fields detected and monitored
- Alert still triggered for cross-origin
- Field count included in event data

**Code References**:
- `Services/WebContent/PageClient.cpp:593` - Field extraction logic
- Loops through all form controls to find password fields

---

### FMON-007: Dynamic form injection

**Purpose**: Detect XSS/malware-injected forms

**Scenario**:
- Page loads normally
- JavaScript injects form after 500ms
- Injected form has cross-origin action and password field

**Expected Behavior**:
- Dynamically created forms still monitored
- `page_did_submit_form()` called regardless of injection timing
- Alert shown if submission is suspicious

**Code References**:
- `Libraries/LibWeb/HTML/HTMLFormElement.cpp:231` - `page_did_submit_form()` call
- Called in `submit()` method, works for dynamic forms

---

### FMON-008: Form submission timing (rapid-fire detection)

**Purpose**: Detect automated/bot submissions

**Scenario**:
- 5 form submissions in 5 seconds
- Average interval < 1 second

**Expected Behavior**:
- `FormMonitor.check_submission_frequency()` detects rapid-fire
- Anomaly score = 1.0 (maximum)
- `anomaly_indicators` includes: `"Unusual submission frequency detected"`

**Code References**:
- `Services/WebContent/FormMonitor.cpp:519` - `check_submission_frequency()`
- `Services/WebContent/FormMonitor.cpp:136` - Timestamp tracking

**Frequency Thresholds**:
```cpp
5+ submissions in 5 seconds → score = 1.0 (bot)
3-4 submissions in 5 seconds → score = 0.7 (suspicious)
Low variance in intervals → score = 0.6 (automated)
```

---

### FMON-009: User interaction requirement

**Purpose**: Flag auto-submit forms (no user click)

**Scenario**:
- Form submitted via `form.submit()` JavaScript
- No user click or keyboard interaction

**Expected Behavior**:
- `had_user_interaction` flag = `false`
- Suspicion score increased
- User warned about automatic submission

**Code References**:
- PageClient tracks user interaction via mouse/keyboard events
- FormMonitor checks interaction flag in `on_form_submit()`

---

### FMON-010: PolicyGraph integration (CRUD operations)

**Purpose**: Verify persistent storage of user decisions

**Test Coverage**:
1. **Create**: `learn_trusted_relationship()` persists to database
2. **Read**: `is_trusted_relationship()` queries database
3. **Update**: Relationship notes can be modified
4. **Delete**: `remove_relationship()` deletes from database
5. **List**: `load_relationships_from_database()` loads all on startup

**Code References**:
- `Services/WebContent/FormMonitor.cpp:280` - Create (trust)
- `Services/WebContent/FormMonitor.cpp:334` - Create (block)
- `Services/WebContent/FormMonitor.cpp:315` - Read
- `Services/WebContent/FormMonitor.cpp:25` - Load on startup
- `Services/Sentinel/PolicyGraph.cpp` - Database operations

**Database Operations**:
```cpp
// Create
PolicyGraph::create_relationship(CredentialRelationship{
  form_origin: "https://app.com",
  action_origin: "https://oauth.com",
  relationship_type: "trusted",
  created_by: "user_decision"
}) → i64 relationship_id

// Read
PolicyGraph::has_relationship(
  "https://app.com",
  "https://oauth.com",
  "trusted"
) → bool

// List
PolicyGraph::list_relationships({}) → Vector<CredentialRelationship>
```

---

### FMON-011: Alert UI interaction

**Purpose**: Verify user decision handling

**User Choices**:
1. **Trust**: `learn_trusted_relationship()` → database, no future alerts
2. **Allow Once**: Form submits, temporary flag, alert next time
3. **Block**: `block_submission()` → database, future submissions prevented

**Expected Alert UI**:
```
⚠️ Credential Protection Warning

This form is sending your password to a different website!

Form origin: https://example.com
Destination: https://suspicious-domain.com

Alert type: credential_exfiltration
Severity: high

What would you like to do?
[ Trust ]  [ Allow Once ]  [ Block ]
```

**Code References**:
- `Services/WebContent/PageClient.cpp:649` - Alert generation
- ConnectionFromClient IPC for alert UI
- User choice sent back via IPC

---

### FMON-012: Edge cases

**Purpose**: Prevent false positives and handle edge cases

**Test Cases**:

1. **Empty password**: No alert if password field is empty
2. **Special characters**: Handle `P@ssw0rd!#$%^&*()` correctly
3. **Very long password**: 2000+ chars, no buffer overflow
4. **Email-only form**: Medium severity (not high like password)
5. **Same origin with port**: `https://example.com:443` == `https://example.com`
6. **Subdomain**: `login.example.com` != `example.com` (cross-origin)

**Code References**:
- `Services/WebContent/FormMonitor.cpp:192` - Origin extraction with port normalization
- Default port logic at lines 201-209
- Alert severity logic at lines 172-186

**Origin Normalization**:
```cpp
// Same origin
https://example.com/login → https://example.com:443/login
  (port 443 omitted as default for HTTPS)

// Cross-origin
https://app.example.com → https://api.example.com
  (different hosts = different origins)
```

---

## Running Tests

### Run All FormMonitor Tests

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npx playwright test tests/security/form-monitor.spec.ts --project=ladybird
```

### Run Specific Test

```bash
npx playwright test tests/security/form-monitor.spec.ts -g "FMON-001"
```

### Run with Debug Output

```bash
DEBUG=pw:browser npx playwright test tests/security/form-monitor.spec.ts --project=ladybird
```

### Generate HTML Report

```bash
npx playwright test tests/security/form-monitor.spec.ts --project=ladybird --reporter=html
npx playwright show-report
```

---

## Test Fixtures

HTML fixtures for manual testing and integration tests:

1. **`fixtures/forms/legitimate-login.html`**
   - Same-origin login form
   - Should NOT trigger alert

2. **`fixtures/forms/phishing-attack.html`**
   - Cross-origin credential exfiltration
   - Should trigger HIGH severity alert

3. **`fixtures/forms/oauth-flow.html`**
   - Legitimate OAuth/SSO flow
   - User should click "Trust"

4. **`fixtures/forms/data-harvesting.html`**
   - Excessive hidden fields (70% ratio)
   - Should trigger anomaly detection

### Load Fixtures

```bash
# In Ladybird browser
file:///home/rbsmith4/ladybird/Tests/Playwright/fixtures/forms/phishing-attack.html
```

---

## Known Limitations

### Current Test Limitations

1. **No Real Cross-Origin Testing**
   - Tests use `data:` URIs which have opaque origins
   - Need local HTTP server for true cross-origin tests
   - Future enhancement: Set up multi-origin test server

2. **Alert UI Detection**
   - Playwright cannot easily detect Ladybird's native alert dialogs
   - Tests check `window.__formMonitorTestData` instead
   - Need Ladybird to expose alerts to DOM or console for automated testing

3. **PolicyGraph Database Access**
   - Tests document expected database operations
   - No direct SQLite manipulation in current tests
   - Future enhancement: Use PolicyGraph API for integration tests

4. **Autofill Simulation**
   - Cannot trigger real browser autofill in tests
   - Manually populate fields to simulate autofill
   - Need Ladybird autofill hooks for accurate testing

### Ladybird Implementation Gaps

These tests may reveal missing functionality:

1. **IPC for Security Alerts**: Need dedicated alert IPC (not generic `page_did_request_alert`)
2. **User Decision Callbacks**: Need IPC to send user choice (Trust/Block/Allow) back to WebContent
3. **PolicyGraph Initialization**: PageClient needs to initialize FormMonitor with PolicyGraph path
4. **Console Logging**: FormMonitor uses `dbgln()` - alerts not visible in release builds

---

## Future Enhancements

### Phase 1: Integration Tests

- [ ] Set up local HTTP server for true cross-origin tests
- [ ] Test with actual PolicyGraph database
- [ ] Verify database persistence across browser restarts

### Phase 2: UI Testing

- [ ] Capture alert dialog screenshots
- [ ] Test user interaction with Trust/Block buttons
- [ ] Verify alert UI appears correctly in Qt/AppKit

### Phase 3: Real-World Testing

- [ ] Test against known phishing sites (browserleaks.com, etc.)
- [ ] Load OAuth flows from real providers (Google, GitHub, Auth0)
- [ ] Performance testing with large forms (100+ fields)

### Phase 4: Fuzzing

- [ ] Fuzz form action URLs (data:, javascript:, file:)
- [ ] Fuzz password field values (Unicode, null bytes, XSS)
- [ ] Fuzz PolicyGraph database (corrupted entries, SQL injection)

---

## Security Considerations

### Test Data Security

- All test credentials are fake (`secret123`, `mypassword`, etc.)
- No real user credentials in test fixtures
- Phishing test pages clearly labeled as tests

### PolicyGraph Database

- Tests should use isolated database path: `/tmp/sentinel_test.db`
- Clean up test data after test run
- Do not interfere with user's production PolicyGraph

### False Positive Prevention

These tests ensure FormMonitor doesn't break:

- Legitimate same-origin logins
- OAuth/SSO flows (after user trusts)
- Password managers
- Change password forms
- Multi-step login flows

---

## Integration with CI/CD

### GitHub Actions

```yaml
- name: Run FormMonitor Tests
  run: |
    cd Tests/Playwright
    npx playwright test tests/security/form-monitor.spec.ts --project=ladybird --reporter=json --output-dir=test-results

- name: Upload Test Results
  uses: actions/upload-artifact@v3
  with:
    name: form-monitor-test-results
    path: Tests/Playwright/test-results/
```

### Test Coverage Requirements

- All 12 tests must pass before merging FormMonitor changes
- Minimum 90% code coverage for FormMonitor.cpp
- No regression in existing LibWeb tests

---

## Troubleshooting

### Test Failures

**Symptom**: Tests timeout waiting for alerts
**Cause**: Ladybird not exposing alerts to Playwright
**Fix**: Add `console.log()` in FormMonitor for test builds

**Symptom**: Cross-origin detection fails
**Cause**: `data:` URIs have opaque origins
**Fix**: Use test HTTP server or document limitation

**Symptom**: PolicyGraph tests fail
**Cause**: Database path not found
**Fix**: Set `SENTINEL_DB_PATH` environment variable

### Debug Logging

Enable FormMonitor debug output:

```bash
WEBCONTENT_DEBUG=1 ./Build/release/bin/Ladybird
```

Check debug logs:

```cpp
dbgln("FormMonitor: Suspicious form submission detected");
dbgln("  Form origin: {}", alert->form_origin);
dbgln("  Action origin: {}", alert->action_origin);
```

---

## Contributing

### Adding New Tests

1. Follow naming convention: `FMON-XXX: Test description`
2. Add test to catalog in this README
3. Document expected behavior and code references
4. Add corresponding fixture if needed
5. Update test count in overview

### Code References Format

```markdown
**Code References**:
- `path/to/file.cpp:123` - Function name and purpose
- `path/to/file.h:45` - Data structure definition
```

---

## Contact

For questions about these tests or FormMonitor implementation:

- See `docs/USER_GUIDE_CREDENTIAL_PROTECTION.md`
- See `docs/MILESTONE_0.3_PLAN.md`
- Check `Services/WebContent/FormMonitor.cpp` implementation

**Test Owner**: Security Test Agent
**Created**: 2025-11-01
**Last Updated**: 2025-11-01
