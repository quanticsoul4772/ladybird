# PolicyGraph Database Test Guide

## Overview

PolicyGraph is the **foundational SQLite database** for all Sentinel security features in Ladybird. This guide documents the Playwright test suite for PolicyGraph (tests PG-001 to PG-015).

## What is PolicyGraph?

PolicyGraph is a security policy database that stores:

1. **Security Policies** - Rules for handling threats (malware, fingerprinting, phishing)
2. **Credential Relationships** - Trusted form submission patterns
3. **Threat History** - Log of detected security events
4. **User Decisions** - Allow/block/quarantine choices
5. **Policy Templates** - Reusable policy configurations

### Database Location

```
~/.local/share/Ladybird/sentinel/policy_graph.db
```

### Database Schema

#### Table: `policies`
Stores security policies (what actions to take for detected threats)

```sql
CREATE TABLE policies (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  rule_name TEXT NOT NULL,              -- YARA rule or policy name
  url_pattern TEXT,                     -- URL pattern to match (SQL LIKE)
  file_hash TEXT,                       -- SHA256 hash of file
  mime_type TEXT,                       -- MIME type filter
  action TEXT NOT NULL,                 -- 'allow', 'block', 'quarantine', 'block_autofill', 'warn_user'
  match_type TEXT DEFAULT 'download',   -- 'download', 'form_mismatch', 'insecure_cred', 'third_party_form'
  enforcement_action TEXT DEFAULT '',   -- Additional enforcement details
  created_at INTEGER NOT NULL,          -- Unix timestamp (milliseconds)
  created_by TEXT NOT NULL,             -- User or system that created policy
  expires_at INTEGER,                   -- Expiration timestamp (-1 = never)
  hit_count INTEGER DEFAULT 0,          -- Number of times policy matched
  last_hit INTEGER                      -- Last match timestamp
);
```

**Indexes:**
- `idx_policies_rule_name` on `rule_name`
- `idx_policies_file_hash` on `file_hash`
- `idx_policies_url_pattern` on `url_pattern`

#### Table: `credential_relationships`
Stores trusted credential submission patterns (FormMonitor)

```sql
CREATE TABLE credential_relationships (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  form_origin TEXT NOT NULL,            -- Origin of the form page
  action_origin TEXT NOT NULL,          -- Origin where form submits
  relationship_type TEXT NOT NULL,      -- 'trusted_credential_post', 'sso_flow', etc.
  created_at INTEGER NOT NULL,
  created_by TEXT NOT NULL,
  last_used INTEGER,                    -- Last time relationship was used
  use_count INTEGER DEFAULT 0,          -- Number of times used
  expires_at INTEGER,
  notes TEXT,
  UNIQUE(form_origin, action_origin, relationship_type)
);
```

**Indexes:**
- `idx_relationships_origins` on `(form_origin, action_origin)`
- `idx_relationships_type` on `relationship_type`

#### Table: `threat_history`
Logs all detected security threats

```sql
CREATE TABLE threat_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  detected_at INTEGER NOT NULL,
  url TEXT NOT NULL,
  filename TEXT NOT NULL,
  file_hash TEXT NOT NULL,
  mime_type TEXT,
  file_size INTEGER NOT NULL,
  rule_name TEXT NOT NULL,              -- YARA rule that matched
  severity TEXT NOT NULL,               -- 'low', 'medium', 'high', 'critical'
  action_taken TEXT NOT NULL,           -- 'allowed', 'blocked', 'quarantined'
  policy_id INTEGER,                    -- FK to policies table
  alert_json TEXT NOT NULL,             -- Full alert details as JSON
  FOREIGN KEY (policy_id) REFERENCES policies(id)
);
```

**Indexes:**
- `idx_threat_history_detected_at` on `detected_at`
- `idx_threat_history_rule_name` on `rule_name`
- `idx_threat_history_file_hash` on `file_hash`

#### Table: `credential_alerts`
Logs credential protection alerts

```sql
CREATE TABLE credential_alerts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  detected_at INTEGER NOT NULL,
  form_origin TEXT NOT NULL,
  action_origin TEXT NOT NULL,
  alert_type TEXT NOT NULL,             -- 'cross_origin', 'insecure', 'third_party'
  severity TEXT NOT NULL,
  has_password_field INTEGER NOT NULL,  -- Boolean (0/1)
  has_email_field INTEGER NOT NULL,
  uses_https INTEGER NOT NULL,
  is_cross_origin INTEGER NOT NULL,
  user_action TEXT,                     -- 'trust', 'block', 'allow_once', null
  policy_id INTEGER,
  alert_json TEXT,
  anomaly_score INTEGER DEFAULT 0,      -- 0-1000 (scaled from 0.0-1.0)
  anomaly_indicators TEXT DEFAULT '[]', -- JSON array of anomaly flags
  FOREIGN KEY(policy_id) REFERENCES policies(id)
);
```

**Indexes:**
- `idx_alerts_time` on `detected_at`
- `idx_alerts_origins` on `(form_origin, action_origin)`
- `idx_alerts_type` on `alert_type`

#### Table: `policy_templates`
Stores reusable policy templates

```sql
CREATE TABLE policy_templates (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT UNIQUE NOT NULL,
  description TEXT NOT NULL,
  category TEXT NOT NULL,               -- 'malware', 'credential', 'fingerprinting'
  template_json TEXT NOT NULL,          -- Template with {{VARIABLE}} placeholders
  is_builtin INTEGER DEFAULT 0,         -- Boolean (0/1)
  created_at INTEGER NOT NULL,
  updated_at INTEGER
);
```

**Index:**
- `idx_templates_category` on `category`

## Policy Matching Priority

When PolicyGraph searches for a matching policy, it uses this priority order:

1. **File Hash** (highest priority) - Most specific match
2. **URL Pattern** (medium priority) - Medium specificity
3. **Rule Name** (lowest priority) - Least specific match

```cpp
// Example: Three policies exist for same threat
Policy A: file_hash="abc123", action="allow"
Policy B: url_pattern="%malicious.com%", action="block"
Policy C: rule_name="Generic_Malware", action="quarantine"

// When matching:
// 1. Check hash first → Returns Policy A (action=allow)
// 2. If no hash match → Check URL → Returns Policy B (action=block)
// 3. If no URL match → Check rule name → Returns Policy C (action=quarantine)
```

## Test Suite Structure

### Test Files

```
Tests/Playwright/tests/security/
├── policy-graph.spec.ts           # Main test suite (PG-001 to PG-015)
└── POLICYGRAPH_TEST_GUIDE.md      # This document
```

### Test Categories

#### CRUD Operations (PG-001 to PG-005)
- **PG-001**: Create policy - Credential relationship persistence
- **PG-002**: Read policy - Verify stored relationships
- **PG-003**: Update policy - Modify existing relationship
- **PG-004**: Delete policy - Remove trusted relationship
- **PG-005**: Policy matching - Priority order

#### Policy Evaluation (PG-006 to PG-007)
- **PG-006**: Evaluate policy - Positive match (allow)
- **PG-007**: Evaluate policy - Negative match (block)

#### Import/Export (PG-008)
- **PG-008**: Import/Export - Backup and restore policies

#### Security Tests (PG-009 to PG-010)
- **PG-009**: SQL injection prevention
- **PG-010**: Concurrent access - Multiple tabs/processes

#### Advanced Features (PG-011 to PG-015)
- **PG-011**: Policy expiration - Automatic cleanup
- **PG-012**: Database health checks and recovery
- **PG-013**: Transaction atomicity - Rollback on error
- **PG-014**: Cache invalidation - Policies update correctly
- **PG-015**: Policy templates - Instantiate and apply

## Running the Tests

### Run All PolicyGraph Tests

```bash
cd /home/rbsmith4/ladybird/Tests/Playwright
npx playwright test tests/security/policy-graph.spec.ts --project=ladybird
```

### Run Specific Test

```bash
# Run single test by name
npx playwright test tests/security/policy-graph.spec.ts -g "PG-001" --project=ladybird

# Run CRUD tests only
npx playwright test tests/security/policy-graph.spec.ts -g "PG-00[1-5]" --project=ladybird

# Run security tests only
npx playwright test tests/security/policy-graph.spec.ts -g "PG-00[9-10]" --project=ladybird
```

### Run with Debug Mode

```bash
# Show browser window and pause on failures
npx playwright test tests/security/policy-graph.spec.ts --project=ladybird --debug

# Run in headed mode (see browser)
npx playwright test tests/security/policy-graph.spec.ts --project=ladybird --headed
```

### View Test Report

```bash
# Generate HTML report
npx playwright show-report

# Open in browser at http://localhost:9323
```

## Testing Strategy

Since PolicyGraph is a C++ SQLite database, these Playwright tests use **indirect testing** by triggering browser events that interact with PolicyGraph:

### Method 1: FormMonitor Integration
Trigger cross-origin form submissions to create credential relationships:

```javascript
// Submit credentials to different domain
await page.fill('input[type="password"]', 'test');
await page.click('button[type="submit"]');

// This triggers:
// 1. FormMonitor detects cross-origin submission
// 2. PolicyGraph.has_relationship() checks for existing trust
// 3. If not trusted, alert shown
// 4. User clicks "Trust" → PolicyGraph.create_relationship()
```

### Method 2: Download Scanning
Trigger file downloads to test malware policies:

```javascript
// Download file (triggers YARA scan)
await page.click('a[download]');

// This triggers:
// 1. SecurityTap scans file with YARA
// 2. If threat detected → PolicyGraph.match_policy()
// 3. Policy action enforced (allow/block/quarantine)
// 4. PolicyGraph.record_threat() logs the event
```

### Method 3: Fingerprinting Detection
Trigger fingerprinting APIs to test fingerprinting policies:

```javascript
// Canvas fingerprinting
const canvas = document.createElement('canvas');
const dataURL = canvas.toDataURL(); // Triggers detection

// This triggers:
// 1. FingerprintingDetector records API call
// 2. If aggressive → PolicyGraph checked for domain policy
// 3. User decision stored in PolicyGraph
```

## Expected Test Results

### Passing Tests (Green)
```
✅ PG-001: Create policy - Credential relationship persistence
✅ PG-002: Read policy - Verify stored relationships
✅ PG-003: Update policy - Modify existing relationship
✅ PG-004: Delete policy - Remove trusted relationship
✅ PG-005: Policy matching - Priority order
✅ PG-006: Evaluate policy - Positive match (allow)
✅ PG-007: Evaluate policy - Negative match (block)
✅ PG-008: Import/Export - Backup and restore policies
✅ PG-009: SQL injection prevention
✅ PG-010: Concurrent access - Multiple tabs/processes
✅ PG-011: Policy expiration - Automatic cleanup
✅ PG-012: Database health checks and recovery
✅ PG-013: Transaction atomicity - Rollback on error
✅ PG-014: Cache invalidation - Policies update correctly
✅ PG-015: Policy templates - Instantiate and apply

15 passed (15 tests)
```

### Common Failures and Fixes

#### Test Timeout
```
Error: Test timeout of 60000ms exceeded.
```

**Cause**: Database operation taking too long
**Fix**: Increase timeout in `playwright.config.ts` or specific test

```javascript
test('PG-001: Create policy', {
  tag: '@p0',
  timeout: 120000  // Increase to 2 minutes
}, async ({ page }) => {
  // Test code
});
```

#### Database Locked
```
Error: SQLite error 5: database is locked
```

**Cause**: Another process has exclusive lock on database
**Fix**: Ensure no other Ladybird instances running, check WAL mode enabled

#### Policy Not Found
```
Error: Policy not found after creation
```

**Cause**: Policy not persisted, cache issue, or timing problem
**Fix**: Add wait/retry logic, check database write permissions

## Direct Database Testing (Alternative)

If you need to test PolicyGraph directly (not through browser events):

### Option 1: C++ Unit Tests
See `/home/rbsmith4/ladybird/Services/Sentinel/TestPolicyGraph.cpp`

```bash
# Build and run C++ tests
./Meta/ladybird.py build
./Build/release/bin/TestPolicyGraph
```

### Option 2: SQLite CLI
Directly query the database:

```bash
# Open database
sqlite3 ~/.local/share/Ladybird/sentinel/policy_graph.db

# List all policies
SELECT * FROM policies;

# List all credential relationships
SELECT * FROM credential_relationships;

# Check threat history
SELECT * FROM threat_history ORDER BY detected_at DESC LIMIT 10;

# Verify schema
.schema
```

### Option 3: Python Script
Use Python's sqlite3 module to inspect database:

```python
import sqlite3
import json

# Connect to database
conn = sqlite3.connect('/home/rbsmith4/.local/share/Ladybird/sentinel/policy_graph.db')
cursor = conn.cursor()

# Query policies
cursor.execute('SELECT * FROM policies')
policies = cursor.fetchall()

for policy in policies:
    print(f"Policy {policy[0]}: {policy[1]} → {policy[5]}")

# Export to JSON
cursor.execute('SELECT * FROM credential_relationships')
relationships = cursor.fetchall()
print(json.dumps(relationships, indent=2))

conn.close()
```

## Integration with Ladybird Features

PolicyGraph is used by these Sentinel components:

### 1. SecurityTap (Malware Scanning)
```cpp
// Services/RequestServer/SecurityTap.cpp
auto policy = m_policy_graph->match_policy(threat_metadata);
if (policy.has_value()) {
    if (policy->action == PolicyGraph::PolicyAction::Block) {
        // Block download
    } else if (policy->action == PolicyGraph::PolicyAction::Quarantine) {
        // Move to quarantine
    }
}
```

### 2. FormMonitor (Credential Protection)
```cpp
// Services/WebContent/FormMonitor.cpp
auto has_trust = m_policy_graph->has_relationship(
    form_origin,
    action_origin,
    "trusted_credential_post"
);

if (!has_trust) {
    // Show alert to user
    // User decision → create_relationship()
}
```

### 3. FingerprintingDetector
```cpp
// Services/Sentinel/FingerprintingDetector.cpp
auto score = calculate_score();
if (score.is_aggressive()) {
    // Check if domain has policy
    auto policy = m_policy_graph->match_policy(/* ... */);
    // Apply policy or prompt user
}
```

### 4. URLSecurityAnalyzer (Phishing Detection)
```cpp
// Services/Sentinel/PhishingURLAnalyzer.cpp
auto analysis = analyze_url(url);
if (analysis.is_suspicious) {
    // Check PolicyGraph for whitelisted domain
    // Record threat if no policy
}
```

## API Reference

### C++ API (PolicyGraph Methods)

#### CRUD Operations
```cpp
// Create policy
ErrorOr<i64> create_policy(Policy const& policy);

// Read policy
ErrorOr<Policy> get_policy(i64 policy_id);
ErrorOr<Vector<Policy>> list_policies();

// Update policy
ErrorOr<void> update_policy(i64 policy_id, Policy const& policy);

// Delete policy
ErrorOr<void> delete_policy(i64 policy_id);
```

#### Policy Matching
```cpp
// Match by priority: hash > URL pattern > rule name
ErrorOr<Optional<Policy>> match_policy(ThreatMetadata const& threat);
```

#### Credential Relationships
```cpp
// Create relationship
ErrorOr<i64> create_relationship(CredentialRelationship const& relationship);

// Check if relationship exists
ErrorOr<bool> has_relationship(
    String const& form_origin,
    String const& action_origin,
    String const& type
);

// Update usage statistics
ErrorOr<void> update_relationship_usage(i64 relationship_id);

// Delete relationship
ErrorOr<void> delete_relationship(i64 relationship_id);
```

#### Threat History
```cpp
// Record threat
ErrorOr<void> record_threat(
    ThreatMetadata const& threat,
    String action_taken,
    Optional<i64> policy_id,
    String alert_json
);

// Query threats
ErrorOr<Vector<ThreatRecord>> get_threat_history(Optional<UnixDateTime> since);
ErrorOr<Vector<ThreatRecord>> get_threats_by_rule(String const& rule_name);
```

#### Import/Export
```cpp
// Export to JSON
ErrorOr<String> export_relationships_json();
ErrorOr<String> export_templates_json();

// Import from JSON
ErrorOr<void> import_relationships_json(String const& json);
ErrorOr<void> import_templates_json(String const& json);
```

#### Database Maintenance
```cpp
// Cleanup
ErrorOr<void> cleanup_expired_policies();
ErrorOr<void> cleanup_old_threats(u64 days_to_keep = 30);
ErrorOr<void> vacuum_database();

// Health checks
ErrorOr<void> verify_database_integrity();
bool is_database_healthy();

// Transactions
ErrorOr<void> begin_transaction();
ErrorOr<void> commit_transaction();
ErrorOr<void> rollback_transaction();
```

## Performance Considerations

### LRU Cache
PolicyGraph uses an LRU cache to avoid repeated database queries:

```cpp
// Cache size: 1000 entries (configurable)
PolicyGraphCache m_cache { 1000 };

// Cache metrics
auto metrics = policy_graph->get_cache_metrics();
dbgln("Cache hits: {}, misses: {}", metrics.hits, metrics.misses);
```

### Circuit Breaker
Prevents cascade failures when database unavailable:

```cpp
// Circuit breaker trips after 3 consecutive failures
// Prevents overwhelming database with failed queries
Core::CircuitBreaker m_circuit_breaker;

if (m_circuit_breaker.is_open()) {
    // Database unhealthy, fail fast
    return Error::from_string_literal("Database unavailable");
}
```

### Database Optimizations
- WAL (Write-Ahead Logging) mode for concurrent access
- Prepared statements (compiled once, executed many times)
- Indexes on frequently queried columns
- VACUUM to reclaim space from deleted records

## Security Considerations

### SQL Injection Prevention
PolicyGraph uses **parameterized queries** exclusively:

```cpp
// SAFE: Parameterized query
statements.match_by_url_pattern = TRY(database->prepare_statement(R"#(
    SELECT * FROM policies
    WHERE url_pattern != ''
      AND ? LIKE url_pattern ESCAPE '\'
    LIMIT 1;
)#"sv));

// Parameters passed separately (not concatenated)
database->execute_statement(statement, url_parameter);
```

**Never concatenates user input** into SQL strings.

### Input Validation
Uses `InputValidator` framework to validate all inputs:

```cpp
// Validate rule name
auto result = InputValidator::validate_rule_name(policy.rule_name);
if (!result.is_valid)
    return Error::from_string_literal(result.error_message);

// Validate URL pattern
auto url_result = InputValidator::validate_url_pattern(pattern);

// Validate SHA256 hash
auto hash_result = InputValidator::validate_sha256(hash);
```

### Constant-Time Comparison
Hash comparisons use constant-time algorithms to prevent timing attacks:

```cpp
bool file_hash_match = Crypto::ConstantTimeComparison::compare_hashes(
    existing.file_hash,
    policy.file_hash
);
```

## Troubleshooting

### Database Corruption

**Symptoms:**
- `PRAGMA integrity_check` fails
- `PolicyGraph.verify_database_integrity()` returns error
- Queries return unexpected results

**Recovery:**
1. Export policies: `PolicyGraph.export_relationships_json()`
2. Backup corrupted database: `cp policy_graph.db policy_graph.db.corrupt`
3. Delete database: `rm policy_graph.db`
4. Restart Ladybird (recreates database)
5. Import policies: `PolicyGraph.import_relationships_json(json)`

### Slow Queries

**Symptoms:**
- Policy matching takes > 100ms
- Cache hit rate < 50%

**Solutions:**
1. Run ANALYZE to update query planner statistics
2. VACUUM database to optimize storage
3. Check index usage with EXPLAIN QUERY PLAN
4. Increase cache size in `SentinelConfig`

### Lock Timeout

**Symptoms:**
- `SQLite error 5: database is locked`
- Queries timeout waiting for lock

**Solutions:**
1. Verify WAL mode enabled: `PRAGMA journal_mode=WAL`
2. Reduce transaction duration (commit more frequently)
3. Check for long-running transactions blocking database
4. Increase SQLite busy timeout

## Future Enhancements

Planned improvements to PolicyGraph testing:

1. **Direct Test API** - Expose PolicyGraph operations via CDP or test:// URL scheme
2. **Visual Policy Editor** - UI for creating/editing policies (testable with Playwright)
3. **Policy Sync** - Test multi-device policy synchronization
4. **Backup/Restore UI** - Test import/export through settings page
5. **Performance Benchmarks** - Automated performance regression tests
6. **Fuzzing** - Automated fuzzing of PolicyGraph inputs

## Related Documentation

- **C++ Implementation**: `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.cpp`
- **Header File**: `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.h`
- **C++ Unit Tests**: `/home/rbsmith4/ladybird/Services/Sentinel/TestPolicyGraph.cpp`
- **FormMonitor Integration**: `/home/rbsmith4/ladybird/docs/USER_GUIDE_CREDENTIAL_PROTECTION.md`
- **YARA Integration**: `/home/rbsmith4/ladybird/docs/SENTINEL_YARA_RULES.md`
- **Architecture**: `/home/rbsmith4/ladybird/docs/SENTINEL_ARCHITECTURE.md`

## Contact & Support

For questions about PolicyGraph testing:
- Review this guide first
- Check C++ unit tests for examples
- See Sentinel documentation in `/docs/`
- Examine existing Playwright tests in `/Tests/Playwright/tests/security/`
