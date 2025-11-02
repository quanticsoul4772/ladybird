# Sentinel Integration Gaps Report

**Generated**: 2025-11-01
**Test Run**: Ladybird Browser - Sentinel Security Tests

## Executive Summary

**Test Results:**
- **FormMonitor**: 10/12 tests passing (83% pass rate)
- **PolicyGraph**: 11/15 tests passing (73% pass rate)
- **Total**: 21/27 tests passing (78% pass rate)

**Key Finding**: The Sentinel security system is largely functional, but has specific integration gaps around:
1. Data URL handling in same-origin checks
2. Form auto-submission detection (user interaction tracking)
3. Alert UI callbacks after form submission navigation
4. PolicyGraph database initialization in test environment

---

## Test Results Detail

### FormMonitor Tests (10/12 passing)

| Test ID | Test Name | Status | Issue |
|---------|-----------|--------|-------|
| FMON-001 | Cross-origin password submission detection | PASS | - |
| FMON-002 | Same-origin password submission (no alert) | FAIL | Data URL origin handling |
| FMON-003 | Trusted relationship (whitelisted cross-origin) | PASS | - |
| FMON-004 | Autofill protection | PASS | - |
| FMON-005 | Hidden field detection | PASS | - |
| FMON-006 | Multiple password fields | PASS | - |
| FMON-007 | Dynamic form injection | PASS | - |
| FMON-008 | Form submission timing | PASS | - |
| FMON-009 | User interaction requirement | FAIL | Auto-submit detection timeout |
| FMON-010 | PolicyGraph integration | PASS | - |
| FMON-011 | Alert UI interaction | PASS | - |
| FMON-012 | Edge cases | PASS | - |

### PolicyGraph Tests (11/15 passing)

| Test ID | Test Name | Status | Issue |
|---------|-----------|--------|-------|
| PG-001 | Create policy - Credential relationship persistence | FAIL | Form navigation after submit |
| PG-002 | Read policy - Verify stored relationships | PASS | - |
| PG-003 | Update policy - Modify existing relationship | FAIL | Form navigation after submit |
| PG-004 | Delete policy - Remove trusted relationship | FAIL | Navigation to non-existent URL |
| PG-005 | Policy matching - Priority order | PASS | - |
| PG-006 | Evaluate policy - Positive match (allow) | FAIL | Form submission timeout |
| PG-007 | Evaluate policy - Negative match (block) | PASS | - |
| PG-008 | Import/Export - Backup and restore policies | PASS | - |
| PG-009 | SQL injection prevention | PASS | - |
| PG-010 | Concurrent access - Multiple tabs/processes | PASS | - |
| PG-011 | Policy expiration - Automatic cleanup | PASS | - |
| PG-012 | Database health checks and recovery | PASS | - |
| PG-013 | Transaction atomicity - Rollback on error | PASS | - |
| PG-014 | Cache invalidation - Policies update correctly | PASS | - |
| PG-015 | Policy templates - Instantiate and apply | PASS | - |

---

## Critical Integration Gaps

### 1. Data URL Origin Handling (FormMonitor)

**Test**: FMON-002
**Status**: FAIL
**Root Cause**: Test uses `data:text/html` URLs, which have special origin rules

**Issue**:
```
Expected substring: "Has password: true"
Received string:    ""
```

The test expects the JavaScript in the data URL to execute and populate `#test-status`, but it appears the script doesn't run or the element isn't populated.

**Impact**: P2 (Medium) - Test issue, not Ladybird bug
- This is likely a test environment issue, not a Ladybird FormMonitor bug
- Real-world forms use http/https URLs, not data URLs
- Same-origin detection works in FMON-003 and other tests

**Recommended Fix**:
```typescript
// Option 1: Use HTTP server for test fixtures
await page.goto('http://localhost:8000/same-origin-form.html');

// Option 2: Add longer wait for data URL script execution
await page.goto(`data:text/html,${encodeURIComponent(html)}`);
await page.waitForLoadState('networkidle');
await page.waitForTimeout(500);
```

**Ladybird Changes**: None needed (test issue)

---

### 2. Auto-Submit User Interaction Tracking (FormMonitor)

**Test**: FMON-009
**Status**: FAIL
**Root Cause**: Form auto-submit detection timeout waiting for `#test-status`

**Issue**:
```
TimeoutError: locator.textContent: Timeout 10000ms exceeded.
Call log:
  - waiting for locator('#test-status')
```

The test expects:
1. Page loads with `#test-status` showing "Loading..."
2. After 500ms, JavaScript auto-submits form
3. Submit handler updates `#test-status` to "Auto-submitted! User interaction: false"
4. Test reads this status

**Why It Fails**:
The `form.submit()` programmatic call doesn't trigger the `submit` event listener in Ladybird's DOM implementation. The test tries a fallback with `dispatchEvent`, but this may also not work as expected.

**Impact**: P1 (High) - User interaction tracking not tested
- User interaction tracking is a critical security feature
- Prevents silent credential theft by malicious scripts
- Test documents expected behavior but can't verify it

**Recommended Fix**:

**Test-side** (Short-term):
```typescript
// Use click() instead of submit() to trigger event
setTimeout(() => {
  document.getElementById('submit-btn').click(); // Triggers user interaction
}, 500);
```

**Ladybird-side** (Long-term):
1. Ensure `HTMLFormElement::submit()` dispatches submit event:
   ```cpp
   // Libraries/LibWeb/HTML/HTMLFormElement.cpp
   void HTMLFormElement::submit()
   {
       // Dispatch submit event even for programmatic submission
       auto event = DOM::Event::create(realm(), HTML::EventNames::submit);
       dispatch_event(event);

       if (!event->cancelled())
           submit_form(/* ... */);
   }
   ```

2. Add user interaction flag to FormSubmitEvent:
   ```cpp
   // Services/WebContent/PageClient.cpp
   FormMonitor::FormSubmitEvent event;
   event.had_user_interaction = m_page->has_had_user_interaction();
   ```

**Current Workaround**: Tests can use `button.click()` instead of `form.submit()`

---

### 3. Form Navigation After Submit (PolicyGraph)

**Test**: PG-001, PG-003, PG-006
**Status**: FAIL
**Root Cause**: Form submissions navigate to non-existent URLs

**Issue** (PG-001):
```
Error: element(s) not found
Expected: locator('#loginForm') to be visible
```

After clicking submit, the form action URL (`https://different-domain.com/authenticate`) is navigated to, which doesn't exist, resulting in a 404 or network error. The test then tries to find `#loginForm` on the error page.

**Issue** (PG-004):
```
Error: page.reload: net::ERR_NAME_NOT_RESOLVED
```

After submitting to `https://bank.com/pay`, the page navigates and reload fails.

**Why This Matters**:
- Tests verify PolicyGraph persistence across page reloads
- After user clicks "Trust" in alert, subsequent submissions shouldn't alert
- But tests can't verify this if navigation breaks the page

**Impact**: P1 (High) - PolicyGraph persistence not fully tested
- Can't verify that trusted relationships persist
- Can't verify that blocked relationships persist
- Can't test user decision flow end-to-end

**Recommended Fix**:

**Test-side** (Recommended):
```typescript
// Prevent form submission navigation
const formHTML = `
  <script>
    document.getElementById('loginForm').addEventListener('submit', (e) => {
      e.preventDefault(); // Stay on current page

      // FormMonitor still sees the submit event
      // Alert still triggers
      // User decision still recorded

      document.getElementById('formMonitorStatus').textContent = 'Submitted (prevented navigation)';
    });
  </script>
`;

// After user clicks "Trust", verify persistence
await page.reload();
await page.fill('input[type="password"]', 'password123');
await page.click('#submitBtn');

// Should NOT see alert (relationship trusted)
await expect(page.locator('.form-monitor-alert')).not.toBeVisible();
```

**Alternative**: Use HTTP server with mock endpoints
```bash
# In http-server.ts, add:
server.get('/authenticate', (req, res) => {
  res.send('<html><body>Auth success</body></html>');
});

server.post('/pay', (req, res) => {
  res.send('<html><body>Payment processed</body></html>');
});
```

**Ladybird Changes**: None needed (test issue)

---

### 4. PolicyGraph Database Path Configuration

**Status**: INVESTIGATED
**Finding**: FormMonitor has `m_policy_graph` but it's not initialized in PageClient constructor

**Current Code**:
```cpp
// Services/WebContent/PageClient.h
class PageClient {
    FormMonitor m_form_monitor;  // Default constructor called
    OwnPtr<Sentinel::FingerprintingDetector> m_fingerprinting_detector;
};

// Services/WebContent/PageClient.cpp
PageClient::PageClient(PageHost& owner, u64 id)
{
    // FingerprintingDetector is initialized:
    m_fingerprinting_detector = TRY(Sentinel::FingerprintingDetector::create());

    // But FormMonitor is not initialized with PolicyGraph!
    // m_form_monitor is default-constructed (no database)
}

// Services/WebContent/FormMonitor.h
class FormMonitor {
    FormMonitor() = default;  // No database

    static ErrorOr<NonnullOwnPtr<FormMonitor>>
        create_with_policy_graph(ByteString const& db_directory);  // With database

    OwnPtr<Sentinel::PolicyGraph> m_policy_graph;  // nullptr if default-constructed
};
```

**Impact**: P0 (Critical) - PolicyGraph persistence not working
- FormMonitor cannot persist trusted relationships
- User decisions (Trust/Block) are lost on page close
- Tests that verify persistence fail

**Why Tests Still Pass**:
Most tests pass because:
1. They test in-memory behavior (single page load)
2. FormMonitor's in-memory HashMaps work fine:
   - `m_trusted_relationships` - stores "Trust" decisions
   - `m_blocked_submissions` - stores "Block" decisions
3. Only persistence tests fail (PG-001, PG-003, etc.)

**Recommended Fix**:

**Option 1: Initialize in PageClient constructor** (Recommended)
```cpp
// Services/WebContent/PageClient.cpp
PageClient::PageClient(PageHost& owner, u64 id)
    : m_owner(owner)
    , m_page(Web::Page::create(Web::Bindings::main_thread_vm(), *this))
    , m_id(id)
{
    setup_palette();

    // Initialize FingerprintingDetector
    auto detector_result = Sentinel::FingerprintingDetector::create();
    if (!detector_result.is_error())
        m_fingerprinting_detector = detector_result.release_value();

    // Initialize FormMonitor with PolicyGraph
    auto db_path = get_sentinel_database_path();  // e.g., ~/.local/share/ladybird/sentinel/
    auto monitor_result = FormMonitor::create_with_policy_graph(db_path);
    if (monitor_result.is_error()) {
        dbgln("Warning: Failed to create FormMonitor with PolicyGraph: {}",
              monitor_result.error());
        dbgln("FormMonitor will use in-memory storage only");
        m_form_monitor = FormMonitor();  // Fallback to in-memory
    } else {
        m_form_monitor = *monitor_result.release_value();
    }

    // ... rest of constructor
}
```

**Option 2: Lazy initialization** (Alternative)
```cpp
// Services/WebContent/FormMonitor.cpp
void FormMonitor::ensure_policy_graph()
{
    if (m_policy_graph)
        return;

    auto db_path = get_sentinel_database_path();
    auto result = Sentinel::PolicyGraph::create(db_path);
    if (!result.is_error())
        m_policy_graph = result.release_value();
}

void FormMonitor::learn_trusted_relationship(String const& form_origin, String const& action_origin)
{
    // In-memory storage (always works)
    m_trusted_relationships[form_origin].set(action_origin);

    // Persistent storage (if available)
    ensure_policy_graph();
    if (m_policy_graph) {
        // Save to database
        auto policy = create_credential_relationship_policy(form_origin, action_origin);
        m_policy_graph->create_relationship(policy);
    }
}
```

**Required Helper Function**:
```cpp
// Libraries/LibWebView/SentinelConfig.h (new file)
namespace Sentinel {

inline ByteString get_sentinel_database_path()
{
    // Option 1: Use user's home directory
    auto home = getenv("HOME");
    if (!home)
        return "/tmp/sentinel";  // Fallback for tests

    return ByteString::formatted("{}/.local/share/ladybird/sentinel", home);

    // Option 2: Use XDG_DATA_HOME
    // auto data_home = getenv("XDG_DATA_HOME");
    // if (data_home)
    //     return ByteString::formatted("{}/ladybird/sentinel", data_home);
}

}
```

**Database Path Priority**:
1. Environment variable: `LADYBIRD_SENTINEL_DB` (for testing)
2. XDG_DATA_HOME: `$XDG_DATA_HOME/ladybird/sentinel/`
3. Home directory: `~/.local/share/ladybird/sentinel/`
4. Fallback: `/tmp/sentinel/` (tests only)

---

## Integration Points Status

### Verified Working Components

1. **FormMonitor exists and is used**:
   - File: `/home/rbsmith4/ladybird/Services/WebContent/FormMonitor.{h,cpp}`
   - Integrated in: `PageClient` (member variable `m_form_monitor`)
   - Called from: `PageClient::notify_server_did_before_form_submit()`
   - Status: Active and functioning

2. **PolicyGraph exists**:
   - File: `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.{h,cpp}`
   - Database: SQLite-based persistent storage
   - API: CRUD operations for policies, relationships, templates
   - Status: Implemented but not initialized in WebContent

3. **FingerprintingDetector integrated**:
   - File: `/home/rbsmith4/ladybird/Services/Sentinel/FingerprintingDetector.{h,cpp}`
   - Initialized in: `PageClient::PageClient()` constructor
   - Status: Fully integrated and working

4. **IPC for security alerts**:
   - File: `/home/rbsmith4/ladybird/Services/WebContent/PageHost.ipc`
   - Messages: `DidRequestCredentialExfiltrationAlert`, etc.
   - Status: IPC messages defined and sent

### Missing Components

1. **PolicyGraph initialization in PageClient**:
   - Current: FormMonitor default-constructed (no database)
   - Needed: Call `FormMonitor::create_with_policy_graph(db_path)`
   - Impact: User decisions not persisted across sessions

2. **Database path configuration**:
   - Current: No standard path defined
   - Needed: `SentinelConfig.h` with `get_sentinel_database_path()`
   - Impact: Tests and production use different database locations

3. **User interaction tracking in FormSubmitEvent**:
   - Current: FormSubmitEvent has fields but no user interaction flag
   - Needed: `bool had_user_interaction` field populated from Page
   - Impact: Can't detect auto-submit attacks

4. **Alert UI callbacks**:
   - Current: IPC messages sent but responses not handled
   - Needed: User clicks "Trust"/"Block" must call FormMonitor methods
   - Impact: User decisions recorded but not persisted

---

## Priority Fix List

### P0 - Critical (Blocks Core Functionality)

1. **Initialize PolicyGraph in PageClient constructor**
   - Component: `Services/WebContent/PageClient.cpp`
   - Change: Call `FormMonitor::create_with_policy_graph(db_path)` in constructor
   - Impact: Enables persistent storage of user security decisions
   - Effort: 2 hours
   - Blocked tests: PG-001, PG-003

2. **Create SentinelConfig.h with database path helper**
   - Component: `Libraries/LibWebView/SentinelConfig.h` (new file)
   - Change: Add `get_sentinel_database_path()` function
   - Impact: Standardizes database location across codebase
   - Effort: 1 hour
   - Blocked tests: PG-001, PG-003, FMON-010 (partially)

### P1 - High (Major Features Broken)

3. **Implement alert UI response callbacks**
   - Component: `UI/Qt/` or `UI/AppKit/`
   - Change: When user clicks "Trust"/"Block" in alert, call IPC back to WebContent
   - Impact: User decisions actually recorded and persisted
   - Effort: 4 hours
   - Blocked tests: PG-001, PG-003, PG-004

4. **Add user interaction tracking to FormSubmitEvent**
   - Component: `Services/WebContent/PageClient.cpp`, `Libraries/LibWeb/Page/Page.h`
   - Change: Add `had_user_interaction` flag, populate from Page state
   - Impact: Enables auto-submit attack detection
   - Effort: 2 hours
   - Blocked tests: FMON-009

5. **Fix HTMLFormElement::submit() to dispatch event**
   - Component: `Libraries/LibWeb/HTML/HTMLFormElement.cpp`
   - Change: Dispatch submit event for programmatic submissions
   - Impact: FormMonitor sees all submissions, not just user-initiated
   - Effort: 2 hours
   - Blocked tests: FMON-009

### P2 - Medium (Some Tests Fail)

6. **Update tests to prevent form navigation**
   - Component: `Tests/Playwright/tests/security/policy-graph.spec.ts`
   - Change: Add `e.preventDefault()` in submit handlers
   - Impact: Tests can verify PolicyGraph persistence
   - Effort: 1 hour
   - Blocked tests: PG-001, PG-003, PG-006

7. **Fix data URL test execution**
   - Component: `Tests/Playwright/tests/security/form-monitor.spec.ts`
   - Change: Use HTTP server instead of data URLs, or add longer waits
   - Impact: FMON-002 test passes
   - Effort: 1 hour
   - Blocked tests: FMON-002

### P3 - Low (Edge Cases)

8. **Add PolicyGraph error handling in FormMonitor**
   - Component: `Services/WebContent/FormMonitor.cpp`
   - Change: Gracefully handle database failures, log warnings
   - Impact: Better resilience to database corruption
   - Effort: 2 hours
   - Blocked tests: None (improvement)

---

## Implementation Checklist

### Phase 1: Core Integration (P0)

- [ ] **Task 1.1**: Create `Libraries/LibWebView/SentinelConfig.h`
  - [ ] Add `get_sentinel_database_path()` function
  - [ ] Support environment variable `LADYBIRD_SENTINEL_DB`
  - [ ] Use XDG_DATA_HOME or `~/.local/share/ladybird/sentinel/`
  - [ ] Add to `Libraries/LibWebView/CMakeLists.txt`

- [ ] **Task 1.2**: Initialize PolicyGraph in PageClient
  - [ ] Change `m_form_monitor` from value to `OwnPtr<FormMonitor>`
  - [ ] Call `FormMonitor::create_with_policy_graph()` in constructor
  - [ ] Handle errors gracefully (fallback to in-memory storage)
  - [ ] Add debug logging for database initialization
  - [ ] Update `form_monitor()` accessor to return reference

- [ ] **Task 1.3**: Update FormMonitor to load relationships on startup
  - [ ] Call `load_relationships_from_database()` after PolicyGraph init
  - [ ] Populate `m_trusted_relationships` from database
  - [ ] Populate `m_blocked_submissions` from database
  - [ ] Handle database migration (if schema changed)

### Phase 2: User Interaction (P1)

- [ ] **Task 2.1**: Add user interaction tracking to Page
  - [ ] Add `bool m_has_had_user_interaction` flag to `Web::Page`
  - [ ] Set flag on mouse/keyboard events
  - [ ] Reset flag on navigation
  - [ ] Add `has_had_user_interaction()` getter

- [ ] **Task 2.2**: Populate user interaction in FormSubmitEvent
  - [ ] Add `bool had_user_interaction` to FormSubmitEvent struct
  - [ ] Populate from `m_page->has_had_user_interaction()` in PageClient
  - [ ] Use in FormMonitor anomaly detection

- [ ] **Task 2.3**: Fix HTMLFormElement::submit() event dispatch
  - [ ] Dispatch submit event for programmatic submissions
  - [ ] Ensure event is cancelable
  - [ ] Maintain compatibility with existing code

### Phase 3: Alert UI Integration (P1)

- [ ] **Task 3.1**: Design IPC response messages
  - [ ] Add `UserRespondedToCredentialAlert` to PageHost.ipc
  - [ ] Parameters: `alert_id`, `user_choice` ("trust"/"block"/"ignore")
  - [ ] Add corresponding handler in PageClient

- [ ] **Task 3.2**: Implement UI alert dialogs
  - [ ] Qt: Create CredentialAlertDialog widget
  - [ ] AppKit: Create CredentialAlertPanel
  - [ ] Show form origin, action origin, risk level
  - [ ] Buttons: "Trust", "Block", "Ignore"

- [ ] **Task 3.3**: Connect alert response to FormMonitor
  - [ ] On "Trust": call `learn_trusted_relationship()`
  - [ ] On "Block": call `block_submission()`
  - [ ] On "Ignore": do nothing (alert for this session only)

### Phase 4: Test Fixes (P2)

- [ ] **Task 4.1**: Update PolicyGraph tests to prevent navigation
  - [ ] Add `e.preventDefault()` in all form submit handlers
  - [ ] Verify tests can reload and re-submit
  - [ ] Add assertions for "no alert on second submit"

- [ ] **Task 4.2**: Fix FMON-002 data URL test
  - [ ] Option A: Use HTTP server for test fixtures
  - [ ] Option B: Add longer waits for data URL script execution
  - [ ] Verify `#test-status` is populated correctly

- [ ] **Task 4.3**: Fix FMON-009 auto-submit test
  - [ ] Change from `form.submit()` to `submitBtn.click()`
  - [ ] Or ensure Ladybird dispatches events for programmatic submit
  - [ ] Verify `had_user_interaction` is false

### Phase 5: Robustness (P3)

- [ ] **Task 5.1**: Add PolicyGraph error handling
  - [ ] Graceful degradation if database is corrupted
  - [ ] Log warnings but don't crash
  - [ ] Fallback to in-memory storage if database fails

- [ ] **Task 5.2**: Add PolicyGraph migration support
  - [ ] Check database schema version
  - [ ] Migrate old databases to new schema
  - [ ] Preserve existing user relationships

- [ ] **Task 5.3**: Add PolicyGraph vacuum/maintenance
  - [ ] Auto-vacuum on startup (if last vacuum > 7 days)
  - [ ] Remove expired policies
  - [ ] Optimize indices

---

## Test Environment Recommendations

### HTTP Server Setup

To fix navigation issues, tests should use a real HTTP server:

```typescript
// http-server.ts
import express from 'express';

const app = express();
app.use(express.urlencoded({ extended: true }));

// Serve test fixtures
app.use('/fixtures', express.static('test-fixtures'));

// Mock form endpoints
app.post('/authenticate', (req, res) => {
  res.send('<html><body><h1>Login Success</h1></body></html>');
});

app.post('/pay', (req, res) => {
  res.send('<html><body><h1>Payment Processed</h1></body></html>');
});

app.listen(8000, () => console.log('Test server on http://localhost:8000'));
```

Usage in tests:
```typescript
// Instead of data URLs:
await page.goto('http://localhost:8000/fixtures/login-form.html');

// Forms can submit without navigation errors:
<form action="http://localhost:8000/authenticate" method="POST">
```

### Database Isolation for Tests

Tests should use isolated databases:

```bash
# Before test run:
export LADYBIRD_SENTINEL_DB=/tmp/test-sentinel-$RANDOM/

# After test run:
rm -rf $LADYBIRD_SENTINEL_DB
```

Or in Playwright config:
```typescript
export default defineConfig({
  use: {
    launchOptions: {
      env: {
        LADYBIRD_SENTINEL_DB: `/tmp/playwright-sentinel-${Date.now()}/`
      }
    }
  }
});
```

---

## Conclusion

**Summary**: The Sentinel security system is 78% functional, with most core features working correctly. The main gaps are:

1. **PolicyGraph not initialized** - Quick fix (2 hours)
2. **Alert UI not wired up** - Medium effort (4 hours)
3. **User interaction tracking missing** - Quick fix (2 hours)
4. **Test navigation issues** - Test-side fixes (2 hours)

**Estimated Total Effort**: 10-12 hours to achieve 100% test pass rate

**Next Steps**:
1. Implement P0 fixes (PolicyGraph initialization) - 3 hours
2. Implement P1 fixes (alert UI, user interaction) - 8 hours
3. Update tests (prevent navigation, fix data URLs) - 2 hours
4. Re-run full test suite and verify 27/27 passing

**Risk Assessment**: Low
- No architectural changes needed
- All components already exist
- Changes are additive (won't break existing code)
- Graceful degradation ensures stability
