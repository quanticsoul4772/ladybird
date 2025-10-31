# Sentinel Milestone 0.3 - Persistent Credential Protection

**Status**: Planning
**Target**: Milestone 0.3
**Estimated Duration**: 2-3 weeks
**Prerequisites**: Milestone 0.2 Complete

## Overview

Milestone 0.3 extends credential protection with persistent storage, policy management, and advanced detection capabilities.

## Goals

1. Persistent trusted form relationships (PolicyGraph storage)
2. Credential alert history tracking
3. Import/export functionality for trust lists
4. Policy templates for credential protection
5. Form anomaly detection

## Current State Analysis

### FormMonitor (Milestone 0.2)
- Location: Services/WebContent/FormMonitor.{h,cpp}
- Storage: In-memory HashMap (lost on restart)
- Relationships: trusted_relationships, blocked_submissions, autofill_overrides
- Detection: Cross-origin, insecure transport, third-party tracking

### PolicyGraph (Milestone 0.1)
- Location: Services/Sentinel/PolicyGraph.{h,cpp}
- Database: SQLite with policies and threat_history tables
- Actions: Allow, Block, Quarantine, BlockAutofill, WarnUser
- Match Types: DownloadOriginFileType, FormActionMismatch, InsecureCredentialPost, ThirdPartyFormPost

### Gap Analysis
- FormMonitor relationships not persisted across restarts
- No credential alert history in database
- No import/export for trusted relationships
- No policy template system
- No anomaly detection beyond rule-based checks

## Implementation Plan

### Phase 1: Database Schema (Week 1, Days 1-2)

**Add credential_relationships table:**
```sql
CREATE TABLE IF NOT EXISTS credential_relationships (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    form_origin TEXT NOT NULL,
    action_origin TEXT NOT NULL,
    relationship_type TEXT NOT NULL,  -- 'trusted', 'blocked'
    created_at INTEGER NOT NULL,
    created_by TEXT NOT NULL,  -- 'user' or 'policy'
    last_used INTEGER,
    use_count INTEGER DEFAULT 0,
    expires_at INTEGER,
    notes TEXT,
    UNIQUE(form_origin, action_origin, relationship_type)
);

CREATE INDEX idx_relationships_origins ON credential_relationships(form_origin, action_origin);
CREATE INDEX idx_relationships_type ON credential_relationships(relationship_type);
```

**Add credential_alerts table:**
```sql
CREATE TABLE IF NOT EXISTS credential_alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    detected_at INTEGER NOT NULL,
    form_origin TEXT NOT NULL,
    action_origin TEXT NOT NULL,
    alert_type TEXT NOT NULL,  -- 'credential_exfiltration', 'insecure_post', 'third_party_post'
    severity TEXT NOT NULL,  -- 'critical', 'high', 'medium', 'low'
    has_password_field INTEGER NOT NULL,
    has_email_field INTEGER NOT NULL,
    uses_https INTEGER NOT NULL,
    is_cross_origin INTEGER NOT NULL,
    user_action TEXT,  -- 'blocked', 'trusted', 'allowed_once', 'ignored'
    policy_id INTEGER,
    alert_json TEXT,
    FOREIGN KEY(policy_id) REFERENCES policies(id)
);

CREATE INDEX idx_alerts_time ON credential_alerts(detected_at);
CREATE INDEX idx_alerts_origins ON credential_alerts(form_origin, action_origin);
CREATE INDEX idx_alerts_type ON credential_alerts(alert_type);
```

**Add policy_templates table:**
```sql
CREATE TABLE IF NOT EXISTS policy_templates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    description TEXT NOT NULL,
    category TEXT NOT NULL,  -- 'credential_protection', 'download_security', 'custom'
    template_json TEXT NOT NULL,  -- JSON template with variables
    is_builtin INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL,
    updated_at INTEGER
);

CREATE INDEX idx_templates_category ON policy_templates(category);
```

**Files to modify:**
- Services/Sentinel/PolicyGraph.cpp (add schema creation)
- Services/Sentinel/DatabaseMigrations.cpp (add migration logic)

**Testing:**
- Tests/Sentinel/TestPolicyGraphSchema.cpp (verify schema creation)
- Test migration from 0.2 to 0.3 schema

### Phase 2: PolicyGraph API Extensions (Week 1, Days 3-4)

**Add credential relationship methods:**
```cpp
// Services/Sentinel/PolicyGraph.h additions

struct CredentialRelationship {
    i64 id { -1 };
    String form_origin;
    String action_origin;
    String relationship_type;  // "trusted" or "blocked"
    UnixDateTime created_at;
    String created_by;
    Optional<UnixDateTime> last_used;
    i64 use_count { 0 };
    Optional<UnixDateTime> expires_at;
    String notes;
};

struct CredentialAlert {
    i64 id { -1 };
    UnixDateTime detected_at;
    String form_origin;
    String action_origin;
    String alert_type;
    String severity;
    bool has_password_field;
    bool has_email_field;
    bool uses_https;
    bool is_cross_origin;
    Optional<String> user_action;
    Optional<i64> policy_id;
    String alert_json;
};

struct PolicyTemplate {
    i64 id { -1 };
    String name;
    String description;
    String category;
    String template_json;
    bool is_builtin;
    UnixDateTime created_at;
    Optional<UnixDateTime> updated_at;
};

// Credential relationship management
ErrorOr<i64> create_relationship(CredentialRelationship const&);
ErrorOr<CredentialRelationship> get_relationship(String const& form_origin, String const& action_origin, String const& type);
ErrorOr<Vector<CredentialRelationship>> list_relationships(Optional<String> type_filter);
ErrorOr<void> update_relationship_usage(i64 relationship_id);
ErrorOr<void> delete_relationship(i64 relationship_id);
ErrorOr<bool> has_relationship(String const& form_origin, String const& action_origin, String const& type);

// Credential alert history
ErrorOr<i64> record_credential_alert(CredentialAlert const&);
ErrorOr<Vector<CredentialAlert>> get_credential_alerts(Optional<UnixDateTime> since);
ErrorOr<Vector<CredentialAlert>> get_alerts_by_origin(String const& origin);
ErrorOr<void> update_alert_action(i64 alert_id, String const& user_action);

// Policy templates
ErrorOr<i64> create_template(PolicyTemplate const&);
ErrorOr<PolicyTemplate> get_template(i64 template_id);
ErrorOr<PolicyTemplate> get_template_by_name(String const& name);
ErrorOr<Vector<PolicyTemplate>> list_templates(Optional<String> category_filter);
ErrorOr<void> update_template(i64 template_id, PolicyTemplate const&);
ErrorOr<void> delete_template(i64 template_id);
ErrorOr<Policy> instantiate_template(i64 template_id, HashMap<String, String> const& variables);

// Import/Export
ErrorOr<String> export_relationships_json();
ErrorOr<void> import_relationships_json(String const& json);
ErrorOr<String> export_templates_json();
ErrorOr<void> import_templates_json(String const& json);
```

**Files to modify:**
- Services/Sentinel/PolicyGraph.h (add struct definitions and method declarations)
- Services/Sentinel/PolicyGraph.cpp (implement methods with prepared statements)

**Testing:**
- Tests/Sentinel/TestCredentialRelationships.cpp (CRUD operations)
- Tests/Sentinel/TestCredentialAlerts.cpp (alert recording and retrieval)
- Tests/Sentinel/TestPolicyTemplates.cpp (template management)

### Phase 3: FormMonitor Integration (Week 1, Days 5-6)

**Extend FormMonitor with PolicyGraph persistence:**

```cpp
// Services/WebContent/FormMonitor.h additions

class FormMonitor {
public:
    // Add PolicyGraph integration
    void set_policy_graph(PolicyGraph* graph);

    // Persist relationships to PolicyGraph
    ErrorOr<void> persist_trusted_relationship(String const& form_origin, String const& action_origin);
    ErrorOr<void> persist_blocked_relationship(String const& form_origin, String const& action_origin);

    // Load relationships from PolicyGraph on startup
    ErrorOr<void> load_relationships_from_database();

    // Record alert to database
    ErrorOr<void> persist_credential_alert(CredentialAlert const& alert, Optional<String> user_action);

private:
    PolicyGraph* m_policy_graph { nullptr };
};
```

**IPC Changes:**

Add PolicyGraph reference passing to FormMonitor:
- Services/WebContent/PageClient.cpp: Pass PolicyGraph to FormMonitor
- Services/WebContent/ConnectionFromClient.cpp: Create/access PolicyGraph instance

**Files to modify:**
- Services/WebContent/FormMonitor.h (add PolicyGraph integration)
- Services/WebContent/FormMonitor.cpp (implement persistence methods)
- Services/WebContent/PageClient.cpp (pass PolicyGraph to FormMonitor)
- Services/WebContent/ConnectionFromClient.cpp (manage PolicyGraph lifecycle)

**Testing:**
- Tests/Sentinel/TestFormMonitorPersistence.cpp (verify persistence)
- Tests/Sentinel/TestFormMonitorLoadRestore.cpp (verify load on restart)

### Phase 4: Import/Export UI (Week 2, Days 1-2)

**Extend about:security page:**

Add import/export UI section:
```html
<section id="credential-trust-management">
    <h2>Trusted Form Relationships</h2>
    <button onclick="exportTrustedRelationships()">Export Trust List</button>
    <button onclick="importTrustedRelationships()">Import Trust List</button>
    <table id="trusted-relationships-table">
        <thead>
            <tr>
                <th>Form Origin</th>
                <th>Action Origin</th>
                <th>Created</th>
                <th>Last Used</th>
                <th>Use Count</th>
                <th>Actions</th>
            </tr>
        </thead>
        <tbody id="trusted-relationships-tbody"></tbody>
    </table>
</section>

<section id="credential-alert-history">
    <h2>Credential Alert History</h2>
    <div id="alert-filters">
        <select id="alert-time-filter">
            <option value="24h">Last 24 Hours</option>
            <option value="7d">Last 7 Days</option>
            <option value="30d">Last 30 Days</option>
            <option value="all">All Time</option>
        </select>
        <select id="alert-severity-filter">
            <option value="all">All Severities</option>
            <option value="critical">Critical</option>
            <option value="high">High</option>
            <option value="medium">Medium</option>
            <option value="low">Low</option>
        </select>
    </div>
    <table id="credential-alerts-table">
        <thead>
            <tr>
                <th>Time</th>
                <th>Form Origin</th>
                <th>Action Origin</th>
                <th>Alert Type</th>
                <th>Severity</th>
                <th>User Action</th>
            </tr>
        </thead>
        <tbody id="credential-alerts-tbody"></tbody>
    </table>
</section>
```

**JavaScript functions:**
```javascript
async function exportTrustedRelationships() {
    const json = await window.securityUI.exportRelationshipsJson();
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `trusted-relationships-${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
}

async function importTrustedRelationships() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e) => {
        const file = e.target.files[0];
        const text = await file.text();
        await window.securityUI.importRelationshipsJson(text);
        loadTrustedRelationships();
    };
    input.click();
}

async function loadTrustedRelationships() {
    const relationships = await window.securityUI.getCredentialRelationships();
    const tbody = document.getElementById('trusted-relationships-tbody');
    tbody.innerHTML = '';

    for (const rel of relationships) {
        const row = tbody.insertRow();
        row.insertCell().textContent = rel.form_origin;
        row.insertCell().textContent = rel.action_origin;
        row.insertCell().textContent = new Date(rel.created_at * 1000).toLocaleString();
        row.insertCell().textContent = rel.last_used ? new Date(rel.last_used * 1000).toLocaleString() : 'Never';
        row.insertCell().textContent = rel.use_count;

        const actionsCell = row.insertCell();
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = 'Revoke';
        deleteBtn.onclick = () => revokeRelationship(rel.id);
        actionsCell.appendChild(deleteBtn);
    }
}

async function loadCredentialAlerts() {
    const timeFilter = document.getElementById('alert-time-filter').value;
    const severityFilter = document.getElementById('alert-severity-filter').value;

    const alerts = await window.securityUI.getCredentialAlerts(timeFilter);
    const tbody = document.getElementById('credential-alerts-tbody');
    tbody.innerHTML = '';

    for (const alert of alerts) {
        if (severityFilter !== 'all' && alert.severity !== severityFilter) {
            continue;
        }

        const row = tbody.insertRow();
        row.insertCell().textContent = new Date(alert.detected_at * 1000).toLocaleString();
        row.insertCell().textContent = alert.form_origin;
        row.insertCell().textContent = alert.action_origin;
        row.insertCell().textContent = alert.alert_type;
        row.insertCell().textContent = alert.severity;
        row.insertCell().textContent = alert.user_action || 'None';
    }
}
```

**SecurityUI Bridge Extensions:**

```cpp
// Libraries/LibWebView/WebUI/SecurityUI.h additions

void get_credential_relationships();
void revoke_relationship(JsonValue const&);
void export_relationships_json();
void import_relationships_json(JsonValue const&);
void get_credential_alerts(JsonValue const&);
void export_templates_json();
void import_templates_json(JsonValue const&);
```

**Files to modify:**
- Base/res/ladybird/about-pages/security.html (add UI sections)
- Libraries/LibWebView/WebUI/SecurityUI.h (add method declarations)
- Libraries/LibWebView/WebUI/SecurityUI.cpp (implement bridge methods)

**Testing:**
- Tests/LibWeb/Text/input/credential-import-export.html (import/export flow)
- Manual testing: Export relationships, clear database, import, verify

### Phase 5: Policy Templates (Week 2, Days 3-4)

**Create builtin templates:**

```json
// Template: Block All Cross-Origin Credential Submissions
{
    "name": "Block All Cross-Origin Credentials",
    "description": "Blocks all form submissions that send credentials to a different origin",
    "category": "credential_protection",
    "policies": [
        {
            "rule_name": "Cross-Origin Credential Block",
            "match_type": "FormActionMismatch",
            "action": "Block",
            "enforcement_action": "block_submission"
        }
    ]
}

// Template: Warn on Insecure Credential Submissions
{
    "name": "Warn on HTTP Password Posts",
    "description": "Shows warning when credentials are submitted over unencrypted HTTP",
    "category": "credential_protection",
    "policies": [
        {
            "rule_name": "Insecure Credential Warning",
            "match_type": "InsecureCredentialPost",
            "action": "WarnUser",
            "enforcement_action": "show_warning"
        }
    ]
}

// Template: Block Third-Party Tracking Forms
{
    "name": "Block Third-Party Tracking",
    "description": "Blocks form submissions to known tracking/analytics domains",
    "category": "credential_protection",
    "policies": [
        {
            "rule_name": "Third-Party Form Block",
            "match_type": "ThirdPartyFormPost",
            "url_pattern": "%(tracking_domain)s",
            "action": "Block",
            "enforcement_action": "block_submission"
        }
    ],
    "variables": {
        "tracking_domain": "*.analytics.com|*.tracking.net|*.ads.example.com"
    }
}
```

**Template Management UI:**

Add templates section to about:security:
```html
<section id="policy-templates">
    <h2>Policy Templates</h2>
    <p>Pre-configured policy templates for common security scenarios.</p>

    <div id="template-list"></div>

    <button onclick="showTemplateDialog()">Create Custom Template</button>
    <button onclick="exportTemplates()">Export Templates</button>
    <button onclick="importTemplates()">Import Templates</button>
</section>

<dialog id="template-dialog">
    <h3>Apply Policy Template</h3>
    <div id="template-variables"></div>
    <button onclick="applyTemplate()">Apply</button>
    <button onclick="closeTemplateDialog()">Cancel</button>
</dialog>
```

**Files to modify:**
- Services/Sentinel/PolicyTemplates.cpp (builtin template definitions)
- Services/Sentinel/PolicyGraph.cpp (template instantiation logic)
- Base/res/ladybird/about-pages/security.html (template UI)
- Libraries/LibWebView/WebUI/SecurityUI.cpp (template management)

**Testing:**
- Tests/Sentinel/TestPolicyTemplates.cpp (template instantiation)
- Tests/Sentinel/TestTemplateVariables.cpp (variable substitution)

### Phase 6: Form Anomaly Detection (Week 2, Days 5-6, Optional)

**Basic anomaly detection features:**

1. Hidden field detection (phishing indicator)
2. Excessive field count (data harvesting)
3. Unusual action domain patterns
4. Frequency analysis (repeated submissions)

**Implementation:**

```cpp
// Services/WebContent/FormMonitor.h additions

struct FormAnomalyScore {
    float score;  // 0.0 (normal) to 1.0 (highly suspicious)
    Vector<String> indicators;  // List of detected anomalies
};

class FormMonitor {
public:
    FormAnomalyScore calculate_anomaly_score(FormSubmitEvent const& event) const;

private:
    // Anomaly detection helpers
    float check_hidden_field_ratio(FormSubmitEvent const& event) const;
    float check_field_count(FormSubmitEvent const& event) const;
    float check_action_domain_reputation(URL::URL const& action_url) const;
    float check_submission_frequency(String const& form_origin) const;

    // Submission frequency tracking
    HashMap<String, Vector<UnixDateTime>> m_submission_timestamps;
};
```

**Scoring Algorithm:**

```cpp
FormAnomalyScore FormMonitor::calculate_anomaly_score(FormSubmitEvent const& event) const {
    float score = 0.0;
    Vector<String> indicators;

    // 1. Hidden field ratio (weight: 0.3)
    float hidden_ratio = check_hidden_field_ratio(event);
    if (hidden_ratio > 0.5) {
        score += 0.3 * hidden_ratio;
        indicators.append("High hidden field ratio");
    }

    // 2. Excessive fields (weight: 0.2)
    float field_score = check_field_count(event);
    if (field_score > 0.7) {
        score += 0.2 * field_score;
        indicators.append("Excessive field count");
    }

    // 3. Domain reputation (weight: 0.3)
    float domain_score = check_action_domain_reputation(event.action_url);
    if (domain_score > 0.5) {
        score += 0.3 * domain_score;
        indicators.append("Suspicious action domain");
    }

    // 4. Submission frequency (weight: 0.2)
    float freq_score = check_submission_frequency(extract_origin(event.document_url));
    if (freq_score > 0.8) {
        score += 0.2 * freq_score;
        indicators.append("Unusual submission frequency");
    }

    return FormAnomalyScore { score, indicators };
}
```

**Integration with alerts:**

Anomaly score added to CredentialAlert:
```cpp
struct CredentialAlert {
    // ... existing fields ...
    float anomaly_score { 0.0 };
    Vector<String> anomaly_indicators;
};
```

Alert severity adjusted based on anomaly score:
- score < 0.3: No adjustment
- score 0.3-0.6: Increase severity by one level
- score > 0.6: Mark as critical

**Files to modify:**
- Services/WebContent/FormMonitor.h (add anomaly detection)
- Services/WebContent/FormMonitor.cpp (implement scoring)
- Services/Sentinel/PolicyGraph.h (add anomaly fields to CredentialAlert)

**Testing:**
- Tests/Sentinel/TestFormAnomalyDetection.cpp (scoring algorithm)
- Tests/LibWeb/Text/input/credential-anomaly-*.html (end-to-end tests)

## Testing Strategy

### Unit Tests
- Tests/Sentinel/TestCredentialRelationships.cpp
- Tests/Sentinel/TestCredentialAlerts.cpp
- Tests/Sentinel/TestPolicyTemplates.cpp
- Tests/Sentinel/TestFormMonitorPersistence.cpp
- Tests/Sentinel/TestFormAnomalyDetection.cpp

### Integration Tests
- Tests/Sentinel/TestCredentialProtectionIntegration.cpp (full flow)
- Tests/LibWeb/Text/input/credential-persistence.html (restart test)
- Tests/LibWeb/Text/input/credential-import-export.html (import/export)

### Manual Testing
1. Create trusted relationship, restart browser, verify persistence
2. Export trust list, import on clean profile, verify relationships
3. Apply policy template, verify policies created correctly
4. Trigger anomaly detection, verify alert severity escalation
5. View alert history in about:security, verify filtering

## Documentation Updates

### User Documentation
- docs/USER_GUIDE_CREDENTIAL_PROTECTION.md
  - Add section on persistent relationships
  - Document import/export workflow
  - Explain policy templates
  - Document anomaly detection indicators

### Technical Documentation
- docs/SENTINEL_ARCHITECTURE.md
  - Add Milestone 0.3 architecture diagrams
  - Document database schema changes
  - Explain anomaly detection algorithm

- docs/SENTINEL_POLICY_GUIDE.md
  - Document credential protection policies
  - Explain policy templates
  - Provide template customization examples

### Changelog
- docs/CHANGELOG.md
  - Add Milestone 0.3 entry with features and components

## Success Criteria

1. Trusted relationships persist across browser restarts
2. All credential alerts recorded in database
3. Import/export functionality works with valid JSON
4. At least 3 builtin policy templates available
5. Anomaly detection scores correlate with suspicious behavior
6. about:security displays relationship and alert tables
7. All tests passing (unit, integration, end-to-end)
8. Documentation complete and accurate

## Risks and Mitigations

### Risk: Database Schema Migration Failures
**Mitigation**: Comprehensive migration tests, backup before upgrade, rollback capability

### Risk: Performance Impact of Anomaly Detection
**Mitigation**: Benchmark scoring algorithm, implement caching, make anomaly detection optional

### Risk: False Positives in Anomaly Detection
**Mitigation**: Tune thresholds based on testing, allow users to adjust sensitivity, log false positive feedback

### Risk: Import/Export Security Issues
**Mitigation**: Validate JSON schema strictly, sanitize imported data, warn on untrusted imports

## Timeline

### Week 1
- Day 1-2: Database schema and migrations
- Day 3-4: PolicyGraph API extensions
- Day 5-6: FormMonitor integration
- Day 7: Week 1 testing and fixes

### Week 2
- Day 1-2: Import/export UI
- Day 3-4: Policy templates
- Day 5-6: Anomaly detection (optional)
- Day 7: Week 2 testing and fixes

### Week 3 (Buffer)
- Day 1-2: Documentation
- Day 3-4: End-to-end testing
- Day 5-7: Bug fixes and polish

## Dependencies

- Milestone 0.2 must be complete
- PolicyGraph database functional
- FormMonitor operational
- about:security page established

## Future Considerations (Milestone 0.4+)

- Machine learning models for anomaly detection
- Federated learning for threat intelligence
- Browser fingerprinting detection
- Advanced phishing detection heuristics
- Integration with external threat feeds
