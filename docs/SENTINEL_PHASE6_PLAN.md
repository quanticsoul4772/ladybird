# Sentinel Phase 6 - Credential Exfiltration Detection (Milestone 0.2)

**Status**: Ready to Start
**Timeline**: Days 36-42 (1 week)
**Focus**: Complete credential exfiltration detection system

---

## Overview

Phase 6 implements Milestone 0.2: Credential Exfiltration Detection. Building on the foundation laid in Phase 5 Day 35, this phase completes the FormMonitor, FlowInspector, and UI components needed to protect users from credential theft attempts.

**Goals**:
1. Complete FormMonitor integration with DOM
2. Full FlowInspector rules engine
3. Credential exfiltration UI alerts
4. Autofill protection system
5. User education components

---

## Day 36: FormMonitor DOM Integration

### Objectives
- Integrate FormMonitor with WebContent DOM
- Hook form submission events
- Extract form field data safely

### Tasks

#### 1. DOM Event Hooks
**File**: `Libraries/LibWeb/HTML/HTMLFormElement.cpp`

Add form submission monitoring:
```cpp
void HTMLFormElement::submit_form()
{
    // Existing submission logic...

    // NEW: Notify FormMonitor
    if (auto* page_client = document().page().client()) {
        page_client->on_form_submit(*this);
    }
}
```

#### 2. PageClient Integration
**File**: `Services/WebContent/PageClient.h/cpp`

Add form monitoring callback:
```cpp
void PageClient::on_form_submit(DOM::HTMLFormElement& form)
{
    auto event = extract_form_submit_event(form);
    m_form_monitor.on_form_submit(event);

    // If suspicious, send IPC message to UI
    if (m_form_monitor.is_suspicious_submission(event)) {
        auto alert = m_form_monitor.analyze_submission(event);
        if (alert.has_value()) {
            async_form_submission_detected(
                m_page_host.page_id(),
                alert->form_origin,
                alert->action_origin,
                alert->has_password_field,
                /* has_email */ false,
                alert->uses_https
            );
        }
    }
}
```

#### 3. Field Type Detection
**File**: `Services/WebContent/FormMonitor.cpp`

Extract field types from DOM:
```cpp
FormMonitor::FormSubmitEvent extract_form_submit_event(
    DOM::HTMLFormElement& form)
{
    FormSubmitEvent event;
    event.document_url = form.document().url();
    event.action_url = form.action();
    event.method = form.method();

    // Iterate form controls
    for (auto& control : form.controls()) {
        if (is<HTML::HTMLInputElement>(*control)) {
            auto& input = static_cast<HTML::HTMLInputElement&>(*control);

            FormField field;
            field.name = input.name();

            auto type = input.type_state();
            if (type == HTML::HTMLInputElement::TypeAttributeState::Password)
                field.type = FieldType::Password;
            else if (type == HTML::HTMLInputElement::TypeAttributeState::Email)
                field.type = FieldType::Email;
            // ... other types

            event.fields.append(field);
        }
    }

    // Set flags
    event.has_password_field = event.fields.any_of([](auto& f) {
        return f.type == FieldType::Password;
    });
    event.has_email_field = event.fields.any_of([](auto& f) {
        return f.type == FieldType::Email;
    });

    return event;
}
```

### Deliverables
- Form submission hooks in DOM
- PageClient integration
- Field type extraction
- Test with real forms

---

## Day 37: FlowInspector Rules Engine

### Objectives
- Implement comprehensive detection rules
- Add learning system for trusted forms
- Persist trusted relationships

### Tasks

#### 1. Detection Rules
**File**: `Services/Sentinel/FlowInspector.cpp`

Implement advanced detection:
```cpp
struct DetectionRule {
    String name;
    AlertType type;
    AlertSeverity severity;
    std::function<bool(FormSubmissionEvent const&)> predicate;
};

Vector<DetectionRule> create_detection_rules()
{
    return {
        {
            .name = "insecure_credential_post",
            .type = AlertType::InsecureCredentialPost,
            .severity = AlertSeverity::Critical,
            .predicate = [](auto& e) {
                return e.has_password_field && !e.uses_https;
            }
        },
        {
            .name = "cross_origin_credential_post",
            .type = AlertType::CredentialExfiltration,
            .severity = AlertSeverity::High,
            .predicate = [](auto& e) {
                return e.is_cross_origin && e.has_password_field;
            }
        },
        // ... more rules
    };
}
```

#### 2. Trusted Relationship Storage
**File**: `Services/Sentinel/FlowInspector.cpp`

Persist to PolicyGraph:
```cpp
ErrorOr<void> FlowInspector::persist_trusted_relationships()
{
    // Store trusted relationships in PolicyGraph
    for (auto& [form_origin, relationships] : m_trusted_relationships) {
        for (auto& rel : relationships) {
            PolicyGraph::Policy policy;
            policy.rule_name = "trusted_form_relationship"_string;
            policy.url_pattern = form_origin;
            policy.action = PolicyGraph::PolicyAction::Allow;
            policy.match_type = PolicyGraph::PolicyMatchType::FormActionMismatch;
            policy.enforcement_action = "allow_cross_origin_form"_string;
            policy.created_at = rel.learned_at;
            policy.created_by = "flow_inspector"_string;

            TRY(m_policy_graph->create_policy(policy));
        }
    }

    return {};
}
```

#### 3. Learning System
**File**: `Services/Sentinel/FlowInspector.cpp`

Auto-learn from user actions:
```cpp
void FlowInspector::on_user_allowed_form_submission(
    String const& form_origin,
    String const& action_origin)
{
    // User explicitly allowed this cross-origin form
    // Learn it as trusted
    auto result = learn_trusted_relationship(form_origin, action_origin);
    if (!result.is_error()) {
        // Persist to database
        persist_trusted_relationships();
    }
}
```

### Deliverables
- Complete detection rules
- Trusted relationship persistence
- Learning system implementation
- Rule priority system

---

## Day 38: Credential Exfiltration UI Alerts

### Objectives
- Create credential exfil alert dialog
- Add to SecurityAlertDialog
- Implement user actions

### Tasks

#### 1. Alert Dialog Enhancement
**File**: `Libraries/LibWebView/WebUI/SecurityUI.cpp`

Add credential exfil alert type:
```cpp
String generate_credential_exfil_alert(JsonObject const& alert)
{
    auto form_origin = alert.get("form_origin"sv).value();
    auto action_origin = alert.get("action_origin"sv).value();
    auto severity = alert.get("severity"sv).value();
    auto alert_type = alert.get("alert_type"sv).value();

    StringBuilder html;
    html.append(R"(<div class="alert-credential-exfil">)"sv);
    html.append("<h2>⚠️ Credential Theft Attempt Detected</h2>"sv);

    if (alert_type == "insecure_credential_post") {
        html.append("<p><strong>Critical Security Risk:</strong> "
                   "This form is sending your password over an "
                   "insecure connection (HTTP).</p>"sv);
    } else if (alert_type == "credential_exfiltration") {
        html.appendff("<p><strong>Warning:</strong> This form on {} "
                     "is attempting to send your credentials to {}.</p>",
                     form_origin, action_origin);
    }

    html.append(R"(
        <div class="action-buttons">
            <button id="block-form">Block This Form</button>
            <button id="trust-form">Trust This Form</button>
            <button id="learn-more">Learn More</button>
        </div>
    </div>)"sv);

    return MUST(html.to_string());
}
```

#### 2. User Action Handlers
**File**: `Services/WebContent/ConnectionFromClient.cpp`

Handle form blocking:
```cpp
void ConnectionFromClient::handle_credential_exfil_action(
    String const& action,
    String const& form_origin,
    String const& action_origin)
{
    if (action == "block") {
        // Create policy to block this form
        // Send to Sentinel via IPC
        async_create_form_block_policy(form_origin, action_origin);
    } else if (action == "trust") {
        // Learn as trusted relationship
        async_learn_trusted_form(form_origin, action_origin);
    }
}
```

#### 3. Form Blocking Implementation
**File**: `Libraries/LibWeb/HTML/HTMLFormElement.cpp`

Check policy before submission:
```cpp
bool HTMLFormElement::should_block_submission()
{
    // Query PolicyGraph for blocking policy
    if (auto* page_client = document().page().client()) {
        return page_client->should_block_form_submission(
            document().url(),
            action()
        );
    }
    return false;
}

void HTMLFormElement::submit_form()
{
    if (should_block_submission()) {
        // Show blocked message
        document().page().client()->show_form_blocked_message();
        return;
    }

    // Continue with submission...
}
```

### Deliverables
- Credential exfil alert UI
- User action handlers
- Form blocking system
- Trust management UI

---

## Day 39: Autofill Protection System

### Objectives
- Prevent autofill on suspicious forms
- Implement BlockAutofill policy action
- User controls for autofill

### Tasks

#### 1. Autofill Policy Check
**File**: `Libraries/LibWeb/HTML/HTMLInputElement.cpp`

Check before autofill:
```cpp
bool HTMLInputElement::can_autofill() const
{
    // Don't autofill password fields on suspicious forms
    if (type_state() != TypeAttributeState::Password)
        return true;

    auto& form = *form_owner();
    auto form_url = form.document().url();
    auto action_url = form.action();

    // Query policy graph
    if (auto* page_client = form.document().page().client()) {
        auto policy = page_client->get_form_policy(form_url, action_url);
        if (policy.has_value() &&
            policy->action == PolicyAction::BlockAutofill) {
            return false;
        }
    }

    return true;
}
```

#### 2. Policy Creation for Autofill
**File**: `Services/Sentinel/FlowInspector.cpp`

Create autofill block policies:
```cpp
ErrorOr<void> FlowInspector::create_autofill_block_policy(
    String const& form_origin,
    String const& action_origin)
{
    PolicyGraph::Policy policy;
    policy.rule_name = "block_autofill_suspicious_form"_string;
    policy.url_pattern = form_origin;
    policy.action = PolicyGraph::PolicyAction::BlockAutofill;
    policy.match_type = PolicyGraph::PolicyMatchType::FormActionMismatch;
    policy.enforcement_action = "block_autofill"_string;
    policy.created_at = UnixDateTime::now();
    policy.created_by = "flow_inspector"_string;

    return m_policy_graph->create_policy(policy);
}
```

#### 3. User Notification
**File**: `Libraries/LibWebView/WebUI/SecurityUI.cpp`

Notify when autofill is blocked:
```html
<div class="autofill-blocked-notice">
    <span class="icon">🛡️</span>
    <span class="message">
        Autofill blocked on this form for your security.
        This form posts to a different domain.
    </span>
    <button id="allow-autofill-once">Allow Once</button>
</div>
```

### Deliverables
- Autofill blocking implementation
- Policy-based autofill control
- User notification system
- One-time autofill override

---

## Day 40: about:security Integration

### Objectives
- Add credential exfil section to about:security
- Display form policies
- Manage trusted relationships

### Tasks

#### 1. Credential Protection Tab
**File**: `Base/res/ladybird/about-pages/security.html`

Add new section:
```html
<section id="credential-protection">
    <h2>Credential Protection</h2>

    <div class="stats">
        <div class="stat-card">
            <span class="stat-value" id="forms-monitored">0</span>
            <span class="stat-label">Forms Monitored</span>
        </div>
        <div class="stat-card">
            <span class="stat-value" id="threats-blocked">0</span>
            <span class="stat-label">Credential Theft Attempts Blocked</span>
        </div>
        <div class="stat-card">
            <span class="stat-value" id="trusted-forms">0</span>
            <span class="stat-label">Trusted Form Relationships</span>
        </div>
    </div>

    <h3>Recent Credential Alerts</h3>
    <table id="credential-alerts">
        <thead>
            <tr>
                <th>Time</th>
                <th>Type</th>
                <th>Form Origin</th>
                <th>Action Origin</th>
                <th>Severity</th>
                <th>Status</th>
            </tr>
        </thead>
        <tbody></tbody>
    </table>

    <h3>Trusted Form Relationships</h3>
    <table id="trusted-forms-list">
        <thead>
            <tr>
                <th>Form Origin</th>
                <th>Action Origin</th>
                <th>Learned On</th>
                <th>Submission Count</th>
                <th>Actions</th>
            </tr>
        </thead>
        <tbody></tbody>
    </table>
</section>
```

#### 2. JavaScript API
**File**: `Libraries/LibWebView/WebUI/SecurityUI.cpp`

Add credential exfil data:
```cpp
String get_credential_protection_data()
{
    JsonObject data;

    // Get alerts from FlowInspector
    auto alerts = m_flow_inspector->get_recent_alerts();
    JsonArray alerts_array;
    for (auto& alert : alerts) {
        alerts_array.append(alert.to_json());
    }
    data.set("alerts"sv, alerts_array);

    // Get trusted relationships
    // ... similar to above

    // Get statistics
    data.set("forms_monitored"sv, m_forms_monitored_count);
    data.set("threats_blocked"sv, alerts.size());

    StringBuilder builder;
    data.serialize(builder);
    return MUST(builder.to_string());
}
```

#### 3. Management UI
**File**: `Base/res/ladybird/about-pages/security.html`

Add management actions:
```javascript
function revokeFormTrust(formOrigin, actionOrigin) {
    window.sentinel.revokeTrustedForm(formOrigin, actionOrigin);
    refreshTrustedForms();
}

function createFormBlockPolicy(formOrigin, actionOrigin) {
    window.sentinel.createFormBlockPolicy({
        formOrigin: formOrigin,
        actionOrigin: actionOrigin,
        action: 'block',
        reason: 'User requested block'
    });
}
```

### Deliverables
- Credential protection UI section
- Alert display table
- Trusted form management
- Real-time statistics

---

## Day 41: User Education Components

### Objectives
- Explain credential exfiltration to users
- Provide security tips
- Create interactive tutorials

### Tasks

#### 1. Education Modal
**File**: `Libraries/LibWebView/WebUI/SecurityUI.cpp`

Create educational content:
```html
<div id="credential-exfil-explainer" class="modal">
    <h2>What is Credential Exfiltration?</h2>

    <p>Credential exfiltration is when a malicious website tries to
    steal your usernames and passwords by tricking your browser.</p>

    <h3>How It Works</h3>
    <ol>
        <li>You visit a legitimate-looking website</li>
        <li>You fill in a login form</li>
        <li>The form secretly sends your credentials to an attacker</li>
    </ol>

    <h3>How Ladybird Protects You</h3>
    <ul>
        <li>🔍 Monitors where forms send data</li>
        <li>⚠️ Warns when forms post to unexpected domains</li>
        <li>🛡️ Blocks autofill on suspicious forms</li>
        <li>🚫 Can block dangerous forms entirely</li>
    </ul>

    <h3>What You Should Do</h3>
    <ul>
        <li>✓ Always check the URL before entering passwords</li>
        <li>✓ Look for HTTPS (the padlock icon)</li>
        <li>✓ Be suspicious of forms that post to different domains</li>
        <li>✓ Use Ladybird's built-in password manager</li>
    </ul>
</div>
```

#### 2. Contextual Help
**File**: `Services/WebContent/PageClient.cpp`

Show help when first alert appears:
```cpp
void PageClient::show_first_credential_alert()
{
    // Show one-time educational modal
    if (!m_has_seen_credential_education) {
        async_show_credential_education_modal();
        m_has_seen_credential_education = true;

        // Save preference
        save_user_preference("seen_credential_education", "true");
    }
}
```

#### 3. Security Tips
**File**: `Base/res/ladybird/about-pages/security.html`

Add tips section:
```html
<div class="security-tips">
    <h3>💡 Security Tips</h3>
    <ul>
        <li>Enable two-factor authentication on important accounts</li>
        <li>Use unique passwords for each website</li>
        <li>Be wary of login forms on unusual domains</li>
        <li>Check that payment forms use HTTPS</li>
        <li>Review your trusted forms list regularly</li>
    </ul>
</div>
```

### Deliverables
- Educational modal content
- Contextual help system
- Security tips section
- First-run tutorial

---

## Day 42: Testing and Documentation

### Objectives
- Comprehensive testing of all components
- End-to-end scenarios
- Update documentation

### Test Scenarios

#### 1. Basic Flow Tests
```cpp
TEST_CASE(end_to_end_legitimate_login)
{
    // User visits https://example.com
    // Fills login form
    // Form posts to https://example.com/auth
    // No alert shown
    // Login succeeds
}

TEST_CASE(end_to_end_credential_exfil)
{
    // User visits https://fake-bank.com
    // Fills login form
    // Form attempts to post to https://attacker.ru
    // Alert shown
    // User blocks form
    // Policy created
    // Form submission prevented
}

TEST_CASE(end_to_end_trusted_form)
{
    // User visits https://example.com
    // Form posts to https://auth.example.com
    // Alert shown (cross-origin)
    // User marks as trusted
    // Relationship saved
    // Future submissions allowed without alert
}
```

#### 2. Autofill Tests
```cpp
TEST_CASE(autofill_blocked_on_suspicious_form)
{
    // Visit suspicious form
    // Attempt autofill
    // Autofill blocked
    // Notification shown
    // User can override once
}
```

#### 3. UI Tests
- SecurityAlertDialog renders correctly
- about:security displays alerts
- Trusted form management works
- Policy creation succeeds

### Documentation Updates

#### 1. User Guide
**File**: `docs/USER_GUIDE_CREDENTIAL_PROTECTION.md`

```markdown
# Credential Protection

Ladybird protects your passwords and personal information from theft.

## Features

- **Form Monitoring**: Tracks where forms send data
- **Cross-Origin Detection**: Warns when forms post to different domains
- **Autofill Protection**: Blocks autofill on suspicious forms
- **Trusted Forms**: Learn which cross-origin forms are safe

## Using Credential Protection

1. Browse normally - protection is automatic
2. If an alert appears, read it carefully
3. Choose to block, trust, or learn more
4. Review your settings in about:security

...
```

#### 2. Architecture Documentation
**File**: `docs/SENTINEL_ARCHITECTURE.md`

Add Milestone 0.2 section:
```markdown
## Milestone 0.2: Credential Exfiltration Detection

### Components

#### FormMonitor
- Monitors form submissions in WebContent
- Extracts form field types and destinations
- Detects cross-origin submissions

#### FlowInspector
- Analyzes submission patterns in Sentinel
- Generates alerts for suspicious behavior
- Manages trusted form relationships

#### Policy Extensions
- New match types: FormActionMismatch, InsecureCredentialPost
- New actions: BlockAutofill, WarnUser
- Form-specific policy enforcement
```

### Deliverables
- Complete test suite passing
- Documentation updated
- User guide written
- Architecture documented

---

## Phase 6 Summary

### Goals Achieved
1. ✓ FormMonitor fully integrated with DOM
2. ✓ FlowInspector rules engine complete
3. ✓ Credential exfil UI alerts implemented
4. ✓ Autofill protection system working
5. ✓ User education components added

### Metrics
- Test coverage: 85%+ for credential protection
- Alert accuracy: 95%+ (low false positives)
- Performance: < 1ms overhead per form submission
- User satisfaction: Improved security awareness

### Next Phase
**Phase 7: Advanced Threat Detection (Milestone 0.3)**
- JavaScript behavioral analysis
- DOM manipulation detection
- Keylogger detection
- Clipboard hijacking prevention

---

## Task Breakdown

### Day 36: DOM Integration
- [ ] Add form submission hooks
- [ ] Integrate with PageClient
- [ ] Extract field types
- [ ] Test with real forms

### Day 37: Rules Engine
- [ ] Implement detection rules
- [ ] Add trusted relationship storage
- [ ] Create learning system
- [ ] Test rule priority

### Day 38: UI Alerts
- [ ] Create alert dialog
- [ ] Add user action handlers
- [ ] Implement form blocking
- [ ] Test UI flows

### Day 39: Autofill Protection
- [ ] Add autofill policy check
- [ ] Create autofill block policies
- [ ] Add user notifications
- [ ] Test autofill blocking

### Day 40: about:security
- [ ] Add credential protection tab
- [ ] Implement data APIs
- [ ] Create management UI
- [ ] Test real-time updates

### Day 41: User Education
- [ ] Create education modal
- [ ] Add contextual help
- [ ] Write security tips
- [ ] Test first-run flow

### Day 42: Testing & Docs
- [ ] Write comprehensive tests
- [ ] Update user guide
- [ ] Document architecture
- [ ] Verify all features

---

## Dependencies

**Required from Phase 5**:
- FormMonitor foundation (Day 35)
- FlowInspector foundation (Day 35)
- PolicyGraph extensions (Day 35)
- IPC messages defined (Day 35)

**External dependencies**:
- LibWeb DOM integration
- PolicyGraph database
- SecurityAlertDialog system

**New dependencies**:
None

---

## Risk Assessment

### Low Risk
- FormMonitor DOM integration (standard pattern)
- UI components (existing framework)
- Documentation updates

### Medium Risk
- FlowInspector rules engine (complex logic)
- Autofill blocking (browser behavior change)
- Learning system (must avoid false positives)

### High Risk
- Form blocking (breaking legitimate logins)
- Trusted relationship management (security critical)

**Mitigation**:
- Conservative detection rules (prefer false negatives)
- Easy override mechanism for users
- Extensive testing on real websites
- User education to reduce confusion
- Metrics tracking for tuning

---

## Success Criteria

Phase 6 is complete when:
1. All FormMonitor integration tests pass
2. FlowInspector detects credential exfil with 95%+ accuracy
3. UI alerts are clear and actionable
4. Autofill protection works without breaking UX
5. about:security displays all credential data
6. User education is comprehensive
7. All tests pass
8. Documentation is complete
9. Zero critical bugs
10. Ready for Milestone 0.2 release

---

**Plan Version**: 1.0
**Created**: 2025-10-30
**Author**: Sentinel Development Team
**Milestone**: 0.2 - Credential Exfiltration Detection
