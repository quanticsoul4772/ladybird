# Sentinel Phase 5 Day 34: Production Readiness

**Date**: 2025-10-30
**Status**: Complete
**Focus**: Production monitoring, configuration, diagnostic tools, and deployment

---

## Overview

Day 34 completes the production readiness milestone by adding comprehensive monitoring, configuration management, diagnostic tools, and deployment infrastructure. This prepares Sentinel for production deployment and ongoing maintenance.

**Key Achievements**:
1. ✅ Metrics collection system for observability
2. ✅ CLI diagnostic tool for system management
3. ✅ Configuration management system
4. ✅ Default YARA rules for threat detection
5. ✅ Database migration system for upgrades
6. ✅ Installation targets and procedures

---

## 1. Metrics Collection System

### Overview

The metrics system provides comprehensive observability into Sentinel's operation, tracking scans, threats, policies, cache performance, and resource usage.

### Implementation

**Files Created**:
- `/home/rbsmith4/ladybird/Services/Sentinel/SentinelMetrics.h`
- `/home/rbsmith4/ladybird/Services/Sentinel/SentinelMetrics.cpp`

### Metrics Structure

```cpp
struct SentinelMetrics {
    // Scan statistics
    size_t total_downloads_scanned;
    size_t threats_detected;
    size_t threats_blocked;
    size_t threats_quarantined;
    size_t threats_allowed;

    // Policy statistics
    size_t policies_enforced;
    size_t policies_created;
    size_t total_policies;

    // Cache statistics
    size_t cache_hits;
    size_t cache_misses;

    // Performance metrics
    Duration total_scan_time;
    Duration total_query_time;
    size_t scan_count;
    size_t query_count;

    // Storage statistics
    size_t database_size_bytes;
    size_t quarantine_size_bytes;
    size_t quarantine_file_count;

    // System health
    UnixDateTime started_at;
    UnixDateTime last_scan;
    UnixDateTime last_threat;
};
```

### Usage

#### Singleton Access

```cpp
#include <Services/Sentinel/SentinelMetrics.h>

auto& metrics = Sentinel::MetricsCollector::the();
```

#### Recording Events

```cpp
// Record a download scan
metrics.record_download_scan(scan_duration);

// Record a threat detection
metrics.record_threat_detected("block"); // or "quarantine", "allow"

// Record a policy query
metrics.record_policy_query(query_duration, cache_hit);

// Record policy creation
metrics.record_policy_created();

// Record policy enforcement
metrics.record_policy_enforced();
```

#### Updating Storage Stats

```cpp
// Update database size
metrics.update_database_size(file_size_bytes);

// Update quarantine statistics
metrics.update_quarantine_stats(total_bytes, file_count);

// Update total policies
metrics.update_total_policies(count);
```

#### Retrieving Metrics

```cpp
// Get current metrics snapshot
auto snapshot = metrics.snapshot();

// Get as JSON
auto json = snapshot.to_json();

// Get as human-readable text
auto text = snapshot.to_human_readable();
```

#### Computed Metrics

```cpp
auto snapshot = metrics.snapshot();

// Average scan time
auto avg_scan = snapshot.average_scan_time();

// Average query time
auto avg_query = snapshot.average_query_time();

// Cache hit rate (0.0 to 1.0)
auto hit_rate = snapshot.cache_hit_rate();

// System uptime
auto uptime = snapshot.uptime();
```

#### Persistence

```cpp
// Save metrics to file
TRY(metrics.save_to_file("/path/to/metrics.json"));

// Load metrics from file
TRY(metrics.load_from_file("/path/to/metrics.json"));

// Reset session metrics (preserves cumulative counts)
metrics.reset_session_metrics();

// Reset all metrics
metrics.reset_all_metrics();
```

### Integration Points

#### In SecurityTap

```cpp
auto scan_start = MonotonicTime::now();
auto result = TRY(scan_file(content));
auto scan_duration = MonotonicTime::now() - scan_start;

MetricsCollector::the().record_download_scan(scan_duration);

if (result.is_threat) {
    MetricsCollector::the().record_threat_detected(action);
}
```

#### In PolicyGraph

```cpp
auto query_start = MonotonicTime::now();
auto policy = TRY(match_policy(threat));
auto query_duration = MonotonicTime::now() - query_start;

bool cache_hit = m_cache.get_cached(key).has_value();
MetricsCollector::the().record_policy_query(query_duration, cache_hit);
```

#### In Quarantine

```cpp
// After quarantining files
auto entries = TRY(list_all_entries());
size_t total_size = 0;
for (auto const& entry : entries) {
    total_size += entry.file_size;
}
MetricsCollector::the().update_quarantine_stats(total_size, entries.size());
```

### Metrics Endpoint in about:security

The metrics are exposed via the SecurityUI interface:

```javascript
// In about:security page
window.chrome.getMetrics().then(metrics => {
    console.log(`Total policies: ${metrics.totalPolicies}`);
    console.log(`Total threats: ${metrics.totalThreats}`);
    console.log(`Threats blocked: ${metrics.threatsBlocked}`);
    console.log(`Threats quarantined: ${metrics.threatsQuarantined}`);
});
```

---

## 2. Diagnostic CLI Tool

### Overview

`sentinel-cli` is a command-line tool for managing and diagnosing the Sentinel system. It provides inspection, maintenance, and recovery operations.

**File**: `/home/rbsmith4/ladybird/Tools/sentinel-cli.cpp`

### Commands

#### `sentinel-cli status`

Shows comprehensive system status including daemon state, database health, quarantine status, and YARA rules availability.

**Example Output**:
```
=== Sentinel Status ===

Sentinel Daemon:      RUNNING
Policy Database:      EXISTS
  Path: /home/user/.local/share/Ladybird/PolicyGraph/policies.db
  Size: 32768 bytes
  Policies: 5
  Threats: 12
Quarantine Directory: EXISTS
  Path: /home/user/.local/share/Ladybird/Quarantine
  Files: 3
YARA Rules:           FOUND
  Path: /home/user/.config/ladybird/sentinel/rules
```

#### `sentinel-cli list-policies`

Lists all policies with details.

**Example Output**:
```
=== Policies (5) ===

Policy ID: 1
  Rule: EICAR_Test_File
  URL Pattern: (any)
  File Hash: 275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f
  MIME Type: (any)
  Action: Block
  Created: 1730000000 by user
  Hit Count: 3
  Last Hit: 1730100000

Policy ID: 2
  Rule: Malicious_JavaScript_Redirector
  URL Pattern: https://malicious-site.ru/*
  File Hash: (any)
  MIME Type: application/javascript
  Action: Quarantine
  Created: 1730050000 by user
  Hit Count: 1
  Last Hit: 1730150000
```

#### `sentinel-cli show-policy <id>`

Shows detailed information for a specific policy.

**Usage**:
```bash
sentinel-cli show-policy 1
```

**Example Output**:
```
=== Policy 1 ===

Rule Name:     EICAR_Test_File
URL Pattern:   (any)
File Hash:     275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f
MIME Type:     (any)
Action:        Block
Created At:    1730000000
Created By:    user
Expires At:    Never
Hit Count:     3
Last Hit:      1730100000
```

#### `sentinel-cli list-quarantine`

Lists all quarantined files.

**Example Output**:
```
=== Quarantined Files (3) ===

ID: 20251030_120000_a3f5e1
  Filename:      malware.js
  URL:           https://malicious-site.ru/malware.js
  SHA256:        a3f5e1b2c9d8...
  Size:          4096 bytes
  Detected:      2025-10-30T12:00:00Z
  Rules:         Malicious_JavaScript_Redirector, Obfuscated_JavaScript

ID: 20251030_130000_b4g6f2
  Filename:      suspicious.zip
  URL:           https://suspicious-site.com/file.zip
  SHA256:        b4g6f2c3d0e9...
  Size:          102400 bytes
  Detected:      2025-10-30T13:00:00Z
  Rules:         Suspicious_ZIP_with_Executable
```

#### `sentinel-cli restore <id> <path>`

Restores a quarantined file to the specified directory.

**Usage**:
```bash
sentinel-cli restore 20251030_120000_a3f5e1 /home/user/Downloads
```

**Output**:
```
Restoring quarantine ID 20251030_120000_a3f5e1 to /home/user/Downloads...
Successfully restored file.
```

**Notes**:
- File is moved from quarantine to destination
- If destination file exists, a unique name is generated (e.g., `malware_(1).js`)
- Metadata is deleted after successful restoration
- File permissions are restored to `rw-------` (0600)

#### `sentinel-cli vacuum`

Optimizes the database by reclaiming unused space.

**Usage**:
```bash
sentinel-cli vacuum
```

**Output**:
```
Running database vacuum...
Vacuum completed successfully.
```

**When to Use**:
- After deleting many policies or threats
- To reclaim disk space
- As part of regular maintenance

#### `sentinel-cli verify`

Verifies database integrity and reports statistics.

**Usage**:
```bash
sentinel-cli verify
```

**Example Output**:
```
Verifying database integrity...
  Policies: 5
  Threats: 12
Database integrity verified.
```

**Error Handling**:
```
Verifying database integrity...
ERROR: Cannot open database: Database file is corrupted
```

#### `sentinel-cli backup`

Creates a timestamped backup of the database.

**Usage**:
```bash
sentinel-cli backup
```

**Example Output**:
```
Backing up database...
  Source: /home/user/.local/share/Ladybird/PolicyGraph/policies.db
  Destination: /home/user/.local/share/Ladybird/PolicyGraph/policies.db.backup.1730000000
Backup created successfully.
```

**Backup Naming**:
- Format: `policies.db.backup.<unix_timestamp>`
- Example: `policies.db.backup.1730000000`

### Installation

The tool is built and installed automatically with Sentinel:

```bash
cmake --build Build/release
cmake --install Build/release
```

After installation, `sentinel-cli` is available in `$PATH`.

---

## 3. Configuration Management

### Overview

The configuration system allows users to customize Sentinel's behavior through a JSON configuration file.

**Files Created**:
- `/home/rbsmith4/ladybird/Libraries/LibWebView/SentinelConfig.h`
- `/home/rbsmith4/ladybird/Libraries/LibWebView/SentinelConfig.cpp`

### Configuration File

**Location**: `~/.config/ladybird/sentinel/config.json`

**Default Configuration**:
```json
{
  "enabled": true,
  "yara_rules_path": "~/.config/ladybird/sentinel/rules",
  "quarantine_path": "~/.local/share/Ladybird/Quarantine",
  "database_path": "~/.local/share/Ladybird/PolicyGraph/policies.db",
  "policy_cache_size": 1000,
  "threat_retention_days": 30,
  "rate_limit": {
    "policies_per_minute": 5,
    "rate_window_seconds": 60
  },
  "notifications": {
    "enabled": true,
    "auto_dismiss_seconds": 5,
    "max_queue_size": 10
  }
}
```

### Configuration Options

#### Core Settings

- **enabled** (`bool`): Master switch for Sentinel functionality
  - Default: `true`
  - Set to `false` to disable all scanning

- **yara_rules_path** (`string`): Directory containing YARA rule files
  - Default: `~/.config/ladybird/sentinel/rules`
  - Must be an absolute path or use `~` for home directory

- **quarantine_path** (`string`): Directory for quarantined files
  - Default: `~/.local/share/Ladybird/Quarantine`
  - Created automatically if missing

- **database_path** (`string`): Path to PolicyGraph database
  - Default: `~/.local/share/Ladybird/PolicyGraph/policies.db`
  - Parent directory created automatically

- **policy_cache_size** (`number`): Maximum policies in LRU cache
  - Default: `1000`
  - Larger values = better performance, more memory usage

- **threat_retention_days** (`number`): Days to keep threat history
  - Default: `30`
  - Older threats are automatically cleaned up

#### Rate Limiting

- **rate_limit.policies_per_minute** (`number`): Max policy creations per minute
  - Default: `5`
  - Prevents abuse of policy creation API

- **rate_limit.rate_window_seconds** (`number`): Time window for rate limiting
  - Default: `60`
  - Must match `policies_per_minute` unit

#### Notifications

- **notifications.enabled** (`bool`): Enable security notifications
  - Default: `true`
  - Set to `false` to suppress all notifications

- **notifications.auto_dismiss_seconds** (`number`): Auto-dismiss timeout
  - Default: `5`
  - Set to `0` to require manual dismissal

- **notifications.max_queue_size** (`number`): Maximum queued notifications
  - Default: `10`
  - Prevents notification spam

### Usage in Code

```cpp
#include <LibWebView/SentinelConfig.h>

// Load default configuration
auto config = TRY(WebView::SentinelConfig::load_default());

// Load from specific path
auto config = TRY(WebView::SentinelConfig::load_from_file("/path/to/config.json"));

// Access settings
if (config.enabled) {
    dbgln("Sentinel is enabled");
    dbgln("Cache size: {}", config.policy_cache_size);
}

// Create default config
auto config = WebView::SentinelConfig::create_default();

// Save config
TRY(config.save_to_file("~/.config/ladybird/sentinel/config.json"));

// Convert to JSON
auto json = config.to_json();

// Parse from JSON
auto config = TRY(WebView::SentinelConfig::from_json(json_string));
```

### First-Run Initialization

On first run, Sentinel creates:

1. **Config directory**: `~/.config/ladybird/sentinel/`
2. **Default config**: `config.json` with default settings
3. **Rules directory**: `~/.config/ladybird/sentinel/rules/`
4. **Default YARA rules**: Copied from installation
5. **Data directory**: `~/.local/share/Ladybird/PolicyGraph/`
6. **Quarantine directory**: `~/.local/share/Ladybird/Quarantine/`

---

## 4. YARA Rules Management

### Overview

Sentinel includes default YARA rules for common threats and supports user-defined custom rules.

**Rules Directory**: `~/.config/ladybird/sentinel/rules/`

### Default Rules

#### `default.yar`

Basic malware detection patterns.

**Rules**:
- `EICAR_Test_File`: EICAR anti-virus test file
- `Suspicious_EXE_in_Archive`: Executable within compressed archive
- `Obfuscated_JavaScript`: JavaScript with suspicious obfuscation
- `Embedded_PE_File`: PE executable embedded in non-executable file

#### `web-threats.yar`

Web-specific threat detection.

**Rules**:
- `Malicious_JavaScript_Redirector`: JavaScript performing suspicious redirects
- `Drive_By_Download`: JavaScript attempting drive-by download
- `Malicious_WebAssembly`: Suspicious WebAssembly binary
- `XSS_Payload`: Potential XSS attack payload
- `Credential_Stealer_JS`: JavaScript attempting to steal credentials

#### `archives.yar`

Archive-specific threat detection.

**Rules**:
- `Suspicious_ZIP_with_Executable`: ZIP containing Windows executable
- `Archive_Bomb`: Potential ZIP bomb (high compression ratio)
- `Nested_Archives`: Archive containing another archive (evasion)
- `Password_Protected_Archive`: Password-protected archive
- `Double_Extension_Archive`: File with double extension (e.g., `.pdf.exe`)

#### `user-custom.yar`

User-defined custom rules (empty template).

### Creating Custom Rules

Add your own rules to `user-custom.yar`:

```yara
rule My_Custom_Threat {
    meta:
        description = "Detects my specific threat pattern"
        severity = "high"
        category = "malicious"
    strings:
        $pattern1 = "suspicious string"
        $pattern2 = /regex_pattern/
        $hex_pattern = { 4D 5A 90 00 }
    condition:
        any of them
}
```

**Metadata Fields**:
- `description`: Human-readable description
- `severity`: `low`, `medium`, `high`, `critical`
- `category`: `test`, `suspicious`, `malicious`, `exploit`

### Rule Management

#### Adding New Rules

1. Create `.yar` file in `~/.config/ladybird/sentinel/rules/`
2. Restart Sentinel daemon to load new rules
3. Test with `sentinel-cli status` to verify compilation

#### Disabling Rules

To temporarily disable a rule:
1. Rename file extension (e.g., `.yar.disabled`)
2. Restart Sentinel daemon

#### Updating Rules

1. Edit `.yar` file
2. Save changes
3. Restart Sentinel daemon

#### Rule Compilation Errors

If YARA rules fail to compile:
- Check syntax with `yara -c rule.yar`
- Review logs in Sentinel output
- Sentinel fails-open (allows downloads) on rule errors

---

## 5. Database Migration System

### Overview

The migration system manages database schema versioning and upgrades, ensuring backward compatibility across Sentinel versions.

**Files Created**:
- `/home/rbsmith4/ladybird/Services/Sentinel/DatabaseMigrations.h`
- `/home/rbsmith4/ladybird/Services/Sentinel/DatabaseMigrations.cpp`

### Schema Versioning

**Current Version**: 1

The schema version is tracked in the `schema_version` table:

```sql
CREATE TABLE schema_version (
    version INTEGER PRIMARY KEY
);
```

### Usage

#### Check if Migration Needed

```cpp
#include <Services/Sentinel/DatabaseMigrations.h>

auto db = TRY(Database::Database::create("/path/to/db"));

if (TRY(DatabaseMigrations::needs_migration(db))) {
    dbgln("Database needs migration");
}
```

#### Get Current Schema Version

```cpp
auto version = TRY(DatabaseMigrations::get_schema_version(db));
dbgln("Current schema version: {}", version);
```

#### Perform Migration

```cpp
// Migrates from current version to latest
TRY(DatabaseMigrations::migrate(db));
```

#### Initialize Fresh Database

```cpp
// Creates schema_version table and sets version
TRY(DatabaseMigrations::initialize_schema(db));
```

#### Verify Schema Compatibility

```cpp
// Checks version and required tables exist
TRY(DatabaseMigrations::verify_schema(db));
```

### Migration Process

#### Automatic Migration

When PolicyGraph opens a database:

1. Check schema version
2. If version < current, run migrations
3. Apply migrations sequentially (v0→v1, v1→v2, etc.)
4. Update schema_version table
5. Verify schema

#### Adding New Migrations

To add a migration for version 2:

```cpp
// In DatabaseMigrations.cpp

static ErrorOr<void> migrate_v1_to_v2(Database::Database& db)
{
    dbgln("DatabaseMigrations: Migrating from v1 to v2");

    // Add new column
    auto sql = "ALTER TABLE policies ADD COLUMN new_field TEXT"_string;
    auto statement = TRY(db.prepare_statement(sql));
    TRY(statement->execute());

    return {};
}

ErrorOr<void> DatabaseMigrations::migrate(Database::Database& db)
{
    auto current_version = TRY(get_schema_version(db));

    // Existing migrations...

    if (current_version < 2) {
        TRY(migrate_v1_to_v2(db));
        TRY(set_schema_version(db, 2));
    }

    return {};
}
```

### Backward Compatibility

#### Version Checking

```cpp
if (current_version > CURRENT_SCHEMA_VERSION) {
    return Error::from_string_literal(
        "Database schema version is newer than supported"
    );
}
```

#### Safe Upgrades

- Migrations never delete data
- New columns are nullable or have defaults
- Indexes are rebuilt after schema changes

#### Rollback

To rollback a migration:

1. Restore from backup: `sentinel-cli backup` (before upgrade)
2. Replace database file with backup
3. Downgrade Sentinel binary

---

## 6. Installation and Deployment

### Build Configuration

The build system is configured to install Sentinel components:

```cmake
# Install targets
install(TARGETS Sentinel sentinel-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/Base/res/ladybird/sentinel/rules
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ladybird/sentinel)
```

### Installation Steps

#### Build Sentinel

```bash
cd /path/to/ladybird
cmake -B Build/release -DCMAKE_BUILD_TYPE=Release
cmake --build Build/release
```

#### Install System-Wide

```bash
sudo cmake --install Build/release
```

This installs:
- `/usr/local/bin/Sentinel` - Sentinel daemon
- `/usr/local/bin/sentinel-cli` - CLI tool
- `/usr/local/share/ladybird/sentinel/rules/` - Default YARA rules

#### User Installation

For user-only installation (no sudo required):

```bash
cmake --install Build/release --prefix ~/.local
```

This installs:
- `~/.local/bin/Sentinel`
- `~/.local/bin/sentinel-cli`
- `~/.local/share/ladybird/sentinel/rules/`

Add `~/.local/bin` to `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

### First-Run Setup

#### Automatic Initialization

On first run, Sentinel automatically creates:

```
~/.config/ladybird/sentinel/
├── config.json                    # Configuration file
└── rules/                         # YARA rules
    ├── default.yar
    ├── web-threats.yar
    ├── archives.yar
    └── user-custom.yar

~/.local/share/Ladybird/
├── PolicyGraph/
│   └── policies.db                # Policy database
└── Quarantine/                     # Quarantined files
```

#### Manual Initialization

To initialize manually:

```bash
# Create directories
mkdir -p ~/.config/ladybird/sentinel/rules
mkdir -p ~/.local/share/Ladybird/PolicyGraph
mkdir -p ~/.local/share/Ladybird/Quarantine

# Copy default rules
cp -r /usr/local/share/ladybird/sentinel/rules/* ~/.config/ladybird/sentinel/rules/

# Create default config
cat > ~/.config/ladybird/sentinel/config.json << 'EOF'
{
  "enabled": true,
  "yara_rules_path": "~/.config/ladybird/sentinel/rules",
  "quarantine_path": "~/.local/share/Ladybird/Quarantine",
  "database_path": "~/.local/share/Ladybird/PolicyGraph/policies.db",
  "policy_cache_size": 1000,
  "threat_retention_days": 30,
  "rate_limit": {
    "policies_per_minute": 5,
    "rate_window_seconds": 60
  },
  "notifications": {
    "enabled": true,
    "auto_dismiss_seconds": 5,
    "max_queue_size": 10
  }
}
EOF
```

### Running Sentinel

#### Start Daemon

```bash
Sentinel
```

Output:
```
Sentinel: Starting daemon...
Sentinel: YARA rules loaded from ~/.config/ladybird/sentinel/rules
Sentinel: PolicyGraph initialized
Sentinel: Listening on /tmp/sentinel.sock
```

#### Verify Status

```bash
sentinel-cli status
```

#### Integration with Ladybird

Ladybird automatically connects to Sentinel when:
1. Sentinel daemon is running
2. Socket exists at `/tmp/sentinel.sock`
3. Sentinel is enabled in config

### Upgrading Sentinel

#### Pre-Upgrade Steps

1. **Backup database**:
   ```bash
   sentinel-cli backup
   ```

2. **Note configuration**:
   ```bash
   cat ~/.config/ladybird/sentinel/config.json
   ```

3. **Export policies** (optional):
   ```bash
   sentinel-cli list-policies > policies.txt
   ```

#### Upgrade Process

1. **Stop Sentinel**:
   ```bash
   killall Sentinel
   ```

2. **Build new version**:
   ```bash
   cd /path/to/ladybird
   git pull
   cmake --build Build/release
   ```

3. **Install new version**:
   ```bash
   sudo cmake --install Build/release
   ```

4. **Start Sentinel**:
   ```bash
   Sentinel
   ```

5. **Verify migration**:
   ```bash
   sentinel-cli verify
   ```

#### Post-Upgrade Steps

1. **Check schema version**:
   - Migration happens automatically on first run
   - Check logs for migration messages

2. **Verify functionality**:
   ```bash
   sentinel-cli status
   sentinel-cli list-policies
   ```

3. **Test scanning**:
   - Download EICAR test file
   - Verify detection and policy enforcement

### Deployment Checklist

- [ ] Build Sentinel with release configuration
- [ ] Install Sentinel and sentinel-cli
- [ ] Copy default YARA rules
- [ ] Create configuration directories
- [ ] Generate default config file
- [ ] Start Sentinel daemon
- [ ] Verify daemon status
- [ ] Test EICAR detection
- [ ] Create initial policies
- [ ] Configure automatic startup (systemd/launchd)
- [ ] Set up log rotation
- [ ] Schedule periodic backups
- [ ] Document custom YARA rules
- [ ] Train users on security alerts
- [ ] Establish incident response procedures

### System Service Configuration

#### systemd (Linux)

Create `/etc/systemd/system/sentinel.service`:

```ini
[Unit]
Description=Sentinel Security Daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/Sentinel
Restart=on-failure
User=sentinel
Group=sentinel

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable sentinel
sudo systemctl start sentinel
```

#### launchd (macOS)

Create `~/Library/LaunchAgents/com.ladybird.sentinel.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.ladybird.sentinel</string>
    <key>ProgramArguments</key>
    <array>
        <string>/usr/local/bin/Sentinel</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
```

Load:
```bash
launchctl load ~/Library/LaunchAgents/com.ladybird.sentinel.plist
```

---

## 7. Maintenance Procedures

### Regular Maintenance

#### Daily

- Monitor Sentinel logs for errors
- Check daemon status: `sentinel-cli status`

#### Weekly

- Review quarantined files: `sentinel-cli list-quarantine`
- Check threat statistics in `about:security`
- Clean up old threats: `PolicyGraph::cleanup_old_threats(7)` (7 days)

#### Monthly

- Backup database: `sentinel-cli backup`
- Vacuum database: `sentinel-cli vacuum`
- Review and update YARA rules
- Audit policies: `sentinel-cli list-policies`
- Check disk usage of quarantine directory

### Database Maintenance

#### Vacuum Database

Reclaims space after deletions:

```bash
sentinel-cli vacuum
```

#### Cleanup Old Threats

Remove threat history older than N days:

```cpp
auto pg = TRY(PolicyGraph::create("/path/to/db"));
TRY(pg.cleanup_old_threats(30)); // Keep 30 days
```

#### Cleanup Expired Policies

Remove policies past their expiration date:

```cpp
TRY(pg.cleanup_expired_policies());
```

### Quarantine Management

#### List Quarantined Files

```bash
sentinel-cli list-quarantine
```

#### Restore False Positive

```bash
sentinel-cli restore <quarantine_id> ~/Downloads
```

#### Clear Old Quarantine Files

Manual cleanup (not in CLI):

```cpp
auto entries = TRY(Quarantine::list_all_entries());
auto cutoff = UnixDateTime::now().seconds_since_epoch() - (90 * 86400); // 90 days

for (auto const& entry : entries) {
    auto detected_time = parse_iso8601(entry.detection_time);
    if (detected_time < cutoff) {
        TRY(Quarantine::delete_file(String::from_byte_string(entry.quarantine_id)));
    }
}
```

### Troubleshooting

#### Sentinel Not Running

**Symptoms**: `sentinel-cli status` shows "NOT RUNNING"

**Solutions**:
1. Start daemon: `Sentinel`
2. Check for port conflicts on `/tmp/sentinel.sock`
3. Review logs for startup errors
4. Verify YARA rules compile: `yara -c ~/.config/ladybird/sentinel/rules/*.yar`

#### Database Corruption

**Symptoms**: Errors accessing PolicyGraph

**Solutions**:
1. Verify integrity: `sentinel-cli verify`
2. Restore from backup:
   ```bash
   cp ~/.local/share/Ladybird/PolicyGraph/policies.db.backup.* \
      ~/.local/share/Ladybird/PolicyGraph/policies.db
   ```
3. Rebuild database (loses history):
   ```bash
   rm ~/.local/share/Ladybird/PolicyGraph/policies.db
   Sentinel  # Will recreate on startup
   ```

#### High Memory Usage

**Symptoms**: Sentinel process using excessive memory

**Solutions**:
1. Reduce cache size in config:
   ```json
   "policy_cache_size": 500
   ```
2. Restart Sentinel daemon
3. Vacuum database: `sentinel-cli vacuum`

#### Quarantine Directory Full

**Symptoms**: Disk space warnings

**Solutions**:
1. Review quarantined files: `sentinel-cli list-quarantine`
2. Delete unnecessary files through Quarantine Manager
3. Restore false positives: `sentinel-cli restore <id> <path>`

---

## 8. Security Considerations

### File Permissions

Sentinel enforces strict permissions:

- **Quarantine directory**: `0700` (owner only)
- **Quarantined files**: `0400` (read-only)
- **Database**: `0600` (owner read/write)
- **Config file**: `0600` (owner read/write)

### Threat Isolation

Quarantined files are:
- Moved (not copied) to quarantine directory
- Permissions changed to read-only
- Isolated from execution paths
- Tracked with metadata for restoration

### Configuration Security

- Config files should not contain secrets
- YARA rules are not encrypted
- Database contains file hashes (not content)
- Threat metadata includes URLs (review before sharing)

### Network Communication

- Sentinel uses local UNIX socket (`/tmp/sentinel.sock`)
- No network communication by default
- IPC messages are not encrypted (local-only)

---

## 9. Performance Impact

### Overhead Measurements

Based on Phase 5 Day 31 profiling:

- **Download path overhead**: < 5% for typical files
- **YARA scan time**: 5-50ms for small files (< 1MB)
- **PolicyGraph query**: < 5ms with cache hit
- **Database insert**: < 10ms per threat record

### Cache Effectiveness

With default cache size (1000):
- **Expected hit rate**: 80-95% for typical usage
- **Memory usage**: ~50KB per 1000 cached entries

### Scaling Recommendations

For high-volume environments:

1. **Increase cache size**:
   ```json
   "policy_cache_size": 5000
   ```

2. **Increase rate limits**:
   ```json
   "rate_limit": {
     "policies_per_minute": 20,
     "rate_window_seconds": 60
   }
   ```

3. **Reduce threat retention**:
   ```json
   "threat_retention_days": 7
   ```

---

## 10. Future Enhancements

### Planned Features

1. **Remote Metrics Export**
   - Prometheus endpoint for metrics
   - Grafana dashboard templates
   - Alerting on anomalies

2. **Policy Synchronization**
   - Share policies across installations
   - Import/export policy packs
   - Community policy repository

3. **Advanced YARA Features**
   - Rule auto-update from repository
   - Rule performance profiling
   - Dynamic rule loading (no restart)

4. **Enhanced CLI**
   - Interactive policy editor
   - Threat analysis reports
   - Performance profiling tools

5. **Web Dashboard**
   - Real-time monitoring UI
   - Policy management interface
   - Quarantine management

---

## Summary

Day 34 successfully implemented production readiness features:

1. **Metrics Collection**: Comprehensive observability
2. **CLI Tool**: Powerful diagnostic and management capabilities
3. **Configuration**: Flexible, user-configurable settings
4. **YARA Rules**: Production-ready threat detection
5. **Migrations**: Safe schema upgrades
6. **Installation**: Complete deployment infrastructure

Sentinel is now production-ready with monitoring, configuration, diagnostic tools, and deployment procedures in place.

**Files Created**:
- `Services/Sentinel/SentinelMetrics.h` - Metrics data structures
- `Services/Sentinel/SentinelMetrics.cpp` - Metrics collection
- `Tools/sentinel-cli.cpp` - CLI diagnostic tool
- `Libraries/LibWebView/SentinelConfig.h` - Configuration management
- `Libraries/LibWebView/SentinelConfig.cpp` - Config parsing
- `Services/Sentinel/DatabaseMigrations.h` - Schema versioning
- `Services/Sentinel/DatabaseMigrations.cpp` - Migration system
- `Base/res/ladybird/sentinel/rules/*.yar` - Default YARA rules

**Files Modified**:
- `Services/Sentinel/CMakeLists.txt` - Added metrics, migrations, CLI
- `Libraries/LibWebView/CMakeLists.txt` - Added SentinelConfig
- `Libraries/LibWebView/WebUI/SecurityUI.h` - Added metrics endpoint
- `Libraries/LibWebView/WebUI/SecurityUI.cpp` - Implemented metrics

---

**Next Steps**: Phase 5 Day 35 - Milestone 0.2 Foundation (Credential Exfiltration Detection)
