# Sentinel Integration - Priority Fix List

**Status**: 21/27 tests passing (78%)
**Goal**: Achieve 27/27 tests passing (100%)
**Estimated Effort**: 10-12 hours

---

## P0 - Critical Fixes (3 hours)

### 1. Initialize PolicyGraph in PageClient Constructor

**File**: `/home/rbsmith4/ladybird/Services/WebContent/PageClient.cpp`

**Current Problem**:
```cpp
// FormMonitor is default-constructed without database
FormMonitor m_form_monitor;  // No persistence!
```

**Fix**:
```cpp
// PageClient.h - Change member to pointer
class PageClient {
    OwnPtr<FormMonitor> m_form_monitor;  // Changed from value to pointer
};

// PageClient.cpp - Initialize with database
PageClient::PageClient(PageHost& owner, u64 id)
{
    // ... existing code ...

    // Initialize FormMonitor with PolicyGraph
    auto db_path = Sentinel::get_sentinel_database_path();
    auto monitor_result = FormMonitor::create_with_policy_graph(db_path);
    if (monitor_result.is_error()) {
        dbgln("Warning: Failed to create FormMonitor with PolicyGraph: {}", monitor_result.error());
        m_form_monitor = make<FormMonitor>();  // Fallback to in-memory
    } else {
        m_form_monitor = monitor_result.release_value();
    }
}
```

**Impact**: Fixes PG-001, PG-003 persistence tests
**Effort**: 2 hours

---

### 2. Create SentinelConfig.h with Database Path Helper

**File**: `/home/rbsmith4/ladybird/Libraries/LibWebView/SentinelConfig.h` (NEW)

```cpp
/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <stdlib.h>

namespace Sentinel {

inline ByteString get_sentinel_database_path()
{
    // Priority 1: Environment variable (for testing)
    auto env_path = getenv("LADYBIRD_SENTINEL_DB");
    if (env_path && env_path[0] != '\0')
        return ByteString(env_path);

    // Priority 2: XDG_DATA_HOME
    auto data_home = getenv("XDG_DATA_HOME");
    if (data_home && data_home[0] != '\0')
        return ByteString::formatted("{}/ladybird/sentinel", data_home);

    // Priority 3: Home directory
    auto home = getenv("HOME");
    if (home && home[0] != '\0')
        return ByteString::formatted("{}/.local/share/ladybird/sentinel", home);

    // Fallback: /tmp (tests only)
    return "/tmp/sentinel"sv;
}

}
```

**Add to CMakeLists.txt**:
```cmake
# Libraries/LibWebView/CMakeLists.txt
set(SOURCES
    # ... existing files ...
    SentinelConfig.h
)
```

**Impact**: Standardizes database location
**Effort**: 1 hour

---

## P1 - High Priority (8 hours)

### 3. Add User Interaction Tracking

**Files**:
- `/home/rbsmith4/ladybird/Libraries/LibWeb/Page/Page.h`
- `/home/rbsmith4/ladybird/Libraries/LibWeb/Page/Page.cpp`
- `/home/rbsmith4/ladybird/Services/WebContent/PageClient.cpp`

**Changes**:

```cpp
// Page.h - Add tracking flag
class Page {
public:
    bool has_had_user_interaction() const { return m_has_had_user_interaction; }
    void set_user_interaction_flag() { m_has_had_user_interaction = true; }
    void reset_user_interaction_flag() { m_has_had_user_interaction = false; }

private:
    bool m_has_had_user_interaction { false };
};

// Page.cpp - Set flag on events
void Page::handle_mousedown(/* ... */)
{
    m_has_had_user_interaction = true;
    // ... existing code ...
}

void Page::handle_keydown(/* ... */)
{
    m_has_had_user_interaction = true;
    // ... existing code ...
}

void Page::did_navigate(/* ... */)
{
    m_has_had_user_interaction = false;  // Reset on navigation
    // ... existing code ...
}

// PageClient.cpp - Use in FormSubmitEvent
FormMonitor::FormSubmitEvent event;
event.had_user_interaction = m_page->has_had_user_interaction();
```

**FormMonitor.h - Add field**:
```cpp
struct FormSubmitEvent {
    // ... existing fields ...
    bool had_user_interaction { false };
};
```

**Impact**: Fixes FMON-009, enables auto-submit detection
**Effort**: 2 hours

---

### 4. Fix HTMLFormElement::submit() Event Dispatch

**File**: `/home/rbsmith4/ladybird/Libraries/LibWeb/HTML/HTMLFormElement.cpp`

**Current Problem**: Programmatic `form.submit()` doesn't dispatch submit event

**Fix**:
```cpp
void HTMLFormElement::submit()
{
    // Create and dispatch submit event
    auto event = DOM::Event::create(realm(), HTML::EventNames::submit);
    event->set_cancelable(true);

    bool should_continue = dispatch_event(event);

    // Only submit if event not cancelled
    if (should_continue && !event->cancelled())
        submit_form(/* ... existing params ... */);
}
```

**Impact**: FormMonitor sees all submissions
**Effort**: 2 hours

---

### 5. Implement Alert UI Response Callbacks

**Files**:
- `/home/rbsmith4/ladybird/Services/WebContent/PageHost.ipc`
- `/home/rbsmith4/ladybird/Services/WebContent/PageClient.cpp`
- `/home/rbsmith4/ladybird/UI/Qt/WebContentView.cpp` (or AppKit equivalent)

**IPC Changes**:
```cpp
// PageHost.ipc - Add response message
endpoint PageHost
{
    // Existing alert message
    DidRequestCredentialExfiltrationAlert(
        i64 alert_id,
        String alert_type,
        String form_origin,
        String action_origin,
        String severity
    ) =|

    // NEW: User response
    messages PageClient {
        UserRespondedToCredentialAlert(
            i64 alert_id,
            String user_choice  // "trust", "block", or "ignore"
        ) => ()
    }
}
```

**PageClient Handler**:
```cpp
Messages::PageClient::UserRespondedToCredentialAlertResponse
PageClient::user_responded_to_credential_alert(i64 alert_id, String user_choice)
{
    auto form_origin = m_pending_alerts.get(alert_id)->form_origin;
    auto action_origin = m_pending_alerts.get(alert_id)->action_origin;

    if (user_choice == "trust") {
        m_form_monitor->learn_trusted_relationship(form_origin, action_origin);
    } else if (user_choice == "block") {
        m_form_monitor->block_submission(form_origin, action_origin);
    }
    // "ignore" = do nothing (one-time allow)

    m_pending_alerts.remove(alert_id);
    return {};
}
```

**Qt UI**:
```cpp
// UI/Qt/WebContentView.cpp
void WebContentView::did_request_credential_exfiltration_alert(
    i64 alert_id, String alert_type, String form_origin,
    String action_origin, String severity)
{
    auto dialog = new CredentialAlertDialog(this);
    dialog->set_form_origin(form_origin);
    dialog->set_action_origin(action_origin);
    dialog->set_severity(severity);

    connect(dialog, &CredentialAlertDialog::userChoseTrust, [=]() {
        client().async_user_responded_to_credential_alert(alert_id, "trust");
    });

    connect(dialog, &CredentialAlertDialog::userChoseBlock, [=]() {
        client().async_user_responded_to_credential_alert(alert_id, "block");
    });

    dialog->show();
}
```

**Impact**: User decisions persisted to PolicyGraph
**Effort**: 4 hours

---

## P2 - Medium Priority (2 hours)

### 6. Update Tests to Prevent Navigation

**File**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/security/policy-graph.spec.ts`

**Fix for PG-001, PG-003, PG-006**:
```typescript
const formHTML = `
  <html>
    <body>
      <form id="loginForm" action="https://different-domain.com/auth" method="POST">
        <input type="password" name="password" />
        <button type="submit" id="submitBtn">Login</button>
      </form>
      <script>
        // Prevent navigation but still trigger FormMonitor
        document.getElementById('loginForm').addEventListener('submit', (e) => {
          e.preventDefault();
          console.log('Form submitted, prevented navigation');
        });
      </script>
    </body>
  </html>
`;

await page.goto(`data:text/html,${encodeURIComponent(formHTML)}`);
await page.fill('input[type="password"]', 'test123');
await page.click('#submitBtn');

// FormMonitor still detects submission
// Alert still triggers
// User can click "Trust"

// Reload and verify persistence
await page.reload();
await page.fill('input[type="password"]', 'test123');
await page.click('#submitBtn');

// Should NOT see alert (relationship trusted)
```

**Impact**: Fixes PG-001, PG-003, PG-006
**Effort**: 1 hour

---

### 7. Fix FMON-002 Data URL Test

**File**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/security/form-monitor.spec.ts`

**Option 1: Use HTTP Server** (Recommended)
```typescript
// Start HTTP server in playwright.config.ts
import { defineConfig } from '@playwright/test';

export default defineConfig({
  webServer: {
    command: 'node http-server.js',
    port: 8000,
    reuseExistingServer: true,
  },
});

// In test:
await page.goto('http://localhost:8000/fixtures/same-origin-form.html');
```

**Option 2: Fix Data URL Timing**
```typescript
await page.goto(`data:text/html,${encodeURIComponent(html)}`);
await page.waitForLoadState('networkidle');
await page.waitForTimeout(500);  // Give script time to execute

const status = await page.locator('#test-status').textContent();
```

**Impact**: Fixes FMON-002
**Effort**: 1 hour

---

## Implementation Order

### Day 1 (4 hours)
1. Create `SentinelConfig.h` (1 hour)
2. Initialize PolicyGraph in PageClient (2 hours)
3. Test persistence with manual verification (1 hour)

### Day 2 (4 hours)
4. Add user interaction tracking (2 hours)
5. Fix HTMLFormElement::submit() (2 hours)

### Day 3 (4 hours)
6. Implement alert UI callbacks (4 hours)

### Day 4 (2 hours)
7. Update tests (prevent navigation, fix data URLs) (2 hours)
8. Run full test suite and verify 27/27 passing

---

## Testing Checklist

After each fix, verify:

- [ ] FormMonitor tests: `npx playwright test tests/security/form-monitor.spec.ts --project=ladybird`
- [ ] PolicyGraph tests: `npx playwright test tests/security/policy-graph.spec.ts --project=ladybird`
- [ ] Manual test: Submit cross-origin form, click "Trust", reload, verify no alert

---

## Code Review Checklist

Before committing:

- [ ] All debug logging uses `dbgln()` (not `printf`)
- [ ] Error handling follows "graceful degradation" pattern
- [ ] Database failures don't crash browser
- [ ] Memory safety: no raw pointers, use `OwnPtr`/`NonnullOwnPtr`
- [ ] Follows Ladybird coding style (east const, snake_case)
- [ ] IPC messages are non-blocking (async)
- [ ] Comments explain "why", not "what"
- [ ] Tests pass on both Qt and AppKit builds
- [ ] No hardcoded paths (use SentinelConfig.h)

---

## Success Criteria

- [ ] 27/27 Playwright tests passing
- [ ] FormMonitor detects cross-origin submissions
- [ ] User clicks "Trust" → relationship persisted to database
- [ ] User clicks "Block" → form blocked permanently
- [ ] Auto-submit detection works (FMON-009)
- [ ] Same-origin submissions don't alert (FMON-002)
- [ ] PolicyGraph survives browser restart
- [ ] Database corruption doesn't crash browser
