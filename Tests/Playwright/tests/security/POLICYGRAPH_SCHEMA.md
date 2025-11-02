# PolicyGraph Database Schema

## Visual Schema Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         policies                            │
├─────────────────────────────────────────────────────────────┤
│ id                    INTEGER PRIMARY KEY AUTOINCREMENT     │
│ rule_name             TEXT NOT NULL                         │
│ url_pattern           TEXT                                  │
│ file_hash             TEXT                                  │
│ mime_type             TEXT                                  │
│ action                TEXT NOT NULL                         │
│ match_type            TEXT DEFAULT 'download'               │
│ enforcement_action    TEXT DEFAULT ''                       │
│ created_at            INTEGER NOT NULL                      │
│ created_by            TEXT NOT NULL                         │
│ expires_at            INTEGER                               │
│ hit_count             INTEGER DEFAULT 0                     │
│ last_hit              INTEGER                               │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ (referenced by)
                              │ policy_id (FK)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      threat_history                         │
├─────────────────────────────────────────────────────────────┤
│ id                    INTEGER PRIMARY KEY AUTOINCREMENT     │
│ detected_at           INTEGER NOT NULL                      │
│ url                   TEXT NOT NULL                         │
│ filename              TEXT NOT NULL                         │
│ file_hash             TEXT NOT NULL                         │
│ mime_type             TEXT                                  │
│ file_size             INTEGER NOT NULL                      │
│ rule_name             TEXT NOT NULL                         │
│ severity              TEXT NOT NULL                         │
│ action_taken          TEXT NOT NULL                         │
│ policy_id             INTEGER (FK → policies.id)            │
│ alert_json            TEXT NOT NULL                         │
└─────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────┐
│                 credential_relationships                    │
├─────────────────────────────────────────────────────────────┤
│ id                    INTEGER PRIMARY KEY AUTOINCREMENT     │
│ form_origin           TEXT NOT NULL                         │
│ action_origin         TEXT NOT NULL                         │
│ relationship_type     TEXT NOT NULL                         │
│ created_at            INTEGER NOT NULL                      │
│ created_by            TEXT NOT NULL                         │
│ last_used             INTEGER                               │
│ use_count             INTEGER DEFAULT 0                     │
│ expires_at            INTEGER                               │
│ notes                 TEXT                                  │
│ UNIQUE(form_origin, action_origin, relationship_type)       │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ (related to)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    credential_alerts                        │
├─────────────────────────────────────────────────────────────┤
│ id                    INTEGER PRIMARY KEY AUTOINCREMENT     │
│ detected_at           INTEGER NOT NULL                      │
│ form_origin           TEXT NOT NULL                         │
│ action_origin         TEXT NOT NULL                         │
│ alert_type            TEXT NOT NULL                         │
│ severity              TEXT NOT NULL                         │
│ has_password_field    INTEGER NOT NULL                      │
│ has_email_field       INTEGER NOT NULL                      │
│ uses_https            INTEGER NOT NULL                      │
│ is_cross_origin       INTEGER NOT NULL                      │
│ user_action           TEXT                                  │
│ policy_id             INTEGER (FK → policies.id)            │
│ alert_json            TEXT                                  │
│ anomaly_score         INTEGER DEFAULT 0                     │
│ anomaly_indicators    TEXT DEFAULT '[]'                     │
└─────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────┐
│                     policy_templates                        │
├─────────────────────────────────────────────────────────────┤
│ id                    INTEGER PRIMARY KEY AUTOINCREMENT     │
│ name                  TEXT UNIQUE NOT NULL                  │
│ description           TEXT NOT NULL                         │
│ category              TEXT NOT NULL                         │
│ template_json         TEXT NOT NULL                         │
│ is_builtin            INTEGER DEFAULT 0                     │
│ created_at            INTEGER NOT NULL                      │
│ updated_at            INTEGER                               │
└─────────────────────────────────────────────────────────────┘
```

## Indexes

### policies Table
```sql
CREATE INDEX idx_policies_rule_name ON policies(rule_name);
CREATE INDEX idx_policies_file_hash ON policies(file_hash);
CREATE INDEX idx_policies_url_pattern ON policies(url_pattern);
```

### threat_history Table
```sql
CREATE INDEX idx_threat_history_detected_at ON threat_history(detected_at);
CREATE INDEX idx_threat_history_rule_name ON threat_history(rule_name);
CREATE INDEX idx_threat_history_file_hash ON threat_history(file_hash);
```

### credential_relationships Table
```sql
CREATE INDEX idx_relationships_origins ON credential_relationships(form_origin, action_origin);
CREATE INDEX idx_relationships_type ON credential_relationships(relationship_type);
```

### credential_alerts Table
```sql
CREATE INDEX idx_alerts_time ON credential_alerts(detected_at);
CREATE INDEX idx_alerts_origins ON credential_alerts(form_origin, action_origin);
CREATE INDEX idx_alerts_type ON credential_alerts(alert_type);
```

### policy_templates Table
```sql
CREATE INDEX idx_templates_category ON policy_templates(category);
```

## Data Flow Diagram

```
┌─────────────────┐
│  SecurityTap    │ ──┐
│ (Malware Scan)  │   │
└─────────────────┘   │
                      │
┌─────────────────┐   │    ┌──────────────────────┐
│  FormMonitor    │ ──┼───→│   PolicyGraph DB     │
│ (Credentials)   │   │    │  (SQLite Database)   │
└─────────────────┘   │    └──────────────────────┘
                      │              │
┌─────────────────┐   │              │
│Fingerprinting   │ ──┤              │
│   Detector      │   │              ▼
└─────────────────┘   │    ┌──────────────────────┐
                      │    │  Browser UI Process  │
┌─────────────────┐   │    │  - Security alerts   │
│ URL Security    │ ──┘    │  - User decisions    │
│   Analyzer      │        │  - Policy UI         │
└─────────────────┘        └──────────────────────┘
```

## Field Descriptions

### policies Table

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `id` | INTEGER | Unique policy identifier | 42 |
| `rule_name` | TEXT | YARA rule or policy name | "EICAR_Test_File" |
| `url_pattern` | TEXT | URL pattern (SQL LIKE syntax) | "%malicious.com%" |
| `file_hash` | TEXT | SHA256 hash of file | "abc123...xyz" |
| `mime_type` | TEXT | MIME type filter | "application/x-msdos-program" |
| `action` | TEXT | Action to take | "allow", "block", "quarantine", "block_autofill", "warn_user" |
| `match_type` | TEXT | Type of match | "download", "form_mismatch", "insecure_cred", "third_party_form" |
| `enforcement_action` | TEXT | Additional enforcement details | "notify_user" |
| `created_at` | INTEGER | Unix timestamp (ms) | 1730482923000 |
| `created_by` | TEXT | Creator (user or system) | "user@example.com" |
| `expires_at` | INTEGER | Expiration timestamp (-1 = never) | 1730569323000 or -1 |
| `hit_count` | INTEGER | Times policy matched | 15 |
| `last_hit` | INTEGER | Last match timestamp | 1730482923000 |

### credential_relationships Table

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `id` | INTEGER | Unique relationship ID | 7 |
| `form_origin` | TEXT | Origin hosting the form | "https://example.com" |
| `action_origin` | TEXT | Origin where form submits | "https://auth.example.com" |
| `relationship_type` | TEXT | Type of relationship | "trusted_credential_post", "sso_flow" |
| `created_at` | INTEGER | Creation timestamp (ms) | 1730482923000 |
| `created_by` | TEXT | Creator | "user@example.com" |
| `last_used` | INTEGER | Last usage timestamp | 1730569323000 |
| `use_count` | INTEGER | Number of uses | 42 |
| `expires_at` | INTEGER | Expiration timestamp | 1730655723000 |
| `notes` | TEXT | User notes | "SSO login flow" |

### threat_history Table

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `id` | INTEGER | Unique threat record ID | 123 |
| `detected_at` | INTEGER | Detection timestamp (ms) | 1730482923000 |
| `url` | TEXT | Download URL | "http://malware.com/virus.exe" |
| `filename` | TEXT | Filename | "virus.exe" |
| `file_hash` | TEXT | SHA256 hash | "abc123...xyz" |
| `mime_type` | TEXT | MIME type | "application/x-msdos-program" |
| `file_size` | INTEGER | File size in bytes | 1048576 |
| `rule_name` | TEXT | YARA rule that matched | "Win32_Trojan_Generic" |
| `severity` | TEXT | Threat severity | "low", "medium", "high", "critical" |
| `action_taken` | TEXT | Action performed | "blocked", "quarantined", "allowed" |
| `policy_id` | INTEGER | Associated policy ID (FK) | 42 |
| `alert_json` | TEXT | Full alert details (JSON) | "{\"threat\":\"malware\",...}" |

### credential_alerts Table

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `id` | INTEGER | Unique alert ID | 88 |
| `detected_at` | INTEGER | Detection timestamp (ms) | 1730482923000 |
| `form_origin` | TEXT | Form page origin | "https://example.com" |
| `action_origin` | TEXT | Submission destination | "https://evil.com" |
| `alert_type` | TEXT | Type of alert | "cross_origin", "insecure", "third_party" |
| `severity` | TEXT | Alert severity | "low", "medium", "high", "critical" |
| `has_password_field` | INTEGER | Password present (0/1) | 1 |
| `has_email_field` | INTEGER | Email present (0/1) | 1 |
| `uses_https` | INTEGER | Uses HTTPS (0/1) | 0 |
| `is_cross_origin` | INTEGER | Cross-origin submission (0/1) | 1 |
| `user_action` | TEXT | User decision | "trust", "block", "allow_once", null |
| `policy_id` | INTEGER | Associated policy (FK) | 42 |
| `alert_json` | TEXT | Full alert details (JSON) | "{\"form\":\"login\",...}" |
| `anomaly_score` | INTEGER | Anomaly score (0-1000) | 750 |
| `anomaly_indicators` | TEXT | Anomaly flags (JSON array) | "[\"rapid_submission\",...]" |

### policy_templates Table

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `id` | INTEGER | Unique template ID | 5 |
| `name` | TEXT | Template name (unique) | "Block Suspicious Domain" |
| `description` | TEXT | Template description | "Blocks downloads from suspicious domains" |
| `category` | TEXT | Template category | "malware", "credential", "fingerprinting" |
| `template_json` | TEXT | Template with placeholders | "{\"rule_name\":\"{{DOMAIN}}_Block\",...}" |
| `is_builtin` | INTEGER | Builtin template (0/1) | 1 |
| `created_at` | INTEGER | Creation timestamp (ms) | 1730482923000 |
| `updated_at` | INTEGER | Last update timestamp | 1730569323000 |

## Policy Matching Logic

### Priority Order
```
1. File Hash (Highest Priority)
   └─ Most specific: Exact SHA256 match

2. URL Pattern (Medium Priority)
   └─ Medium specificity: SQL LIKE pattern match

3. Rule Name (Lowest Priority)
   └─ Least specific: Generic YARA rule name
```

### Example Match Flow
```sql
-- Given threat metadata:
{
  url: "http://malware.com/virus.exe",
  filename: "virus.exe",
  file_hash: "abc123...xyz",
  rule_name: "Win32_Trojan"
}

-- Step 1: Try hash match
SELECT * FROM policies
WHERE file_hash = 'abc123...xyz'
  AND (expires_at = -1 OR expires_at > NOW())
LIMIT 1;

-- If no match, Step 2: Try URL pattern
SELECT * FROM policies
WHERE url_pattern != ''
  AND 'http://malware.com/virus.exe' LIKE url_pattern ESCAPE '\'
  AND (expires_at = -1 OR expires_at > NOW())
LIMIT 1;

-- If no match, Step 3: Try rule name
SELECT * FROM policies
WHERE rule_name = 'Win32_Trojan'
  AND (file_hash = '' OR file_hash IS NULL)
  AND (url_pattern = '' OR url_pattern IS NULL)
  AND (expires_at = -1 OR expires_at > NOW())
LIMIT 1;
```

## Query Examples

### Create Policy
```sql
INSERT INTO policies (
  rule_name, url_pattern, file_hash, mime_type,
  action, match_type, enforcement_action,
  created_at, created_by, expires_at, hit_count, last_hit
) VALUES (
  'EICAR_Test_File',
  NULL,
  'abc123...xyz',
  'application/x-msdos-program',
  'quarantine',
  'download',
  '',
  1730482923000,
  'user@example.com',
  -1,
  0,
  NULL
);
```

### Create Credential Relationship
```sql
INSERT INTO credential_relationships (
  form_origin, action_origin, relationship_type,
  created_at, created_by, last_used, use_count, expires_at, notes
) VALUES (
  'https://example.com',
  'https://auth.example.com',
  'trusted_credential_post',
  1730482923000,
  'user@example.com',
  NULL,
  0,
  -1,
  'SSO login flow'
);
```

### Record Threat
```sql
INSERT INTO threat_history (
  detected_at, url, filename, file_hash, mime_type, file_size,
  rule_name, severity, action_taken, policy_id, alert_json
) VALUES (
  1730482923000,
  'http://malware.com/virus.exe',
  'virus.exe',
  'abc123...xyz',
  'application/x-msdos-program',
  1048576,
  'Win32_Trojan',
  'high',
  'quarantined',
  42,
  '{"threat":"malware","source":"download"}'
);
```

### Check Relationship Exists
```sql
SELECT COUNT(*) FROM credential_relationships
WHERE form_origin = 'https://example.com'
  AND action_origin = 'https://auth.example.com'
  AND relationship_type = 'trusted_credential_post';
```

### Get Recent Threats
```sql
SELECT * FROM threat_history
WHERE detected_at >= (strftime('%s','now') - 86400) * 1000
ORDER BY detected_at DESC
LIMIT 10;
```

### Export Relationships (JSON)
```sql
SELECT json_group_array(
  json_object(
    'form_origin', form_origin,
    'action_origin', action_origin,
    'relationship_type', relationship_type,
    'created_at', created_at,
    'use_count', use_count
  )
) FROM credential_relationships;
```

## Database File Location

```
~/.local/share/Ladybird/sentinel/policy_graph.db
```

### SQLite Files
```
policy_graph.db         # Main database file
policy_graph.db-wal     # Write-Ahead Log (WAL mode)
policy_graph.db-shm     # Shared memory file
```

## Performance Characteristics

### Table Sizes (Estimated)

| Table | Typical Row Size | Max Rows | Estimated Size |
|-------|-----------------|----------|----------------|
| policies | 500 bytes | 1,000 | 500 KB |
| credential_relationships | 300 bytes | 5,000 | 1.5 MB |
| threat_history | 1 KB | 10,000 | 10 MB |
| credential_alerts | 800 bytes | 10,000 | 8 MB |
| policy_templates | 2 KB | 100 | 200 KB |

**Total Database Size**: ~20 MB (typical usage)

### Query Performance

| Operation | Indexed | Avg Time | Notes |
|-----------|---------|----------|-------|
| Match by hash | ✅ | < 1 ms | B-tree index on file_hash |
| Match by URL | ✅ | < 5 ms | Index + LIKE pattern matching |
| Match by rule | ✅ | < 1 ms | B-tree index on rule_name |
| List all policies | ❌ | < 10 ms | Full table scan (small table) |
| Check relationship | ✅ | < 1 ms | Composite index on origins |
| Record threat | ✅ | < 2 ms | Insert operation |

## Database Maintenance

### Cleanup Old Threats
```sql
-- Delete threats older than 30 days
DELETE FROM threat_history
WHERE detected_at < (strftime('%s','now') - 2592000) * 1000;
```

### Cleanup Expired Policies
```sql
-- Delete expired policies
DELETE FROM policies
WHERE expires_at IS NOT NULL
  AND expires_at != -1
  AND expires_at <= strftime('%s','now') * 1000;
```

### Vacuum Database
```sql
-- Reclaim unused space
VACUUM;
```

### Integrity Check
```sql
-- Verify database integrity
PRAGMA integrity_check;
```

## Related Documentation

- **Test Suite**: `policy-graph.spec.ts` - Playwright tests
- **Test Guide**: `POLICYGRAPH_TEST_GUIDE.md` - Comprehensive testing documentation
- **Summary**: `POLICYGRAPH_TEST_SUMMARY.md` - High-level overview
- **C++ Implementation**: `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.cpp`
- **C++ Header**: `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.h`
