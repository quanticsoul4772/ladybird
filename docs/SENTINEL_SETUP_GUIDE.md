# Sentinel Security Service - Setup Guide

## Overview

Sentinel is Ladybird's integrated security service that provides real-time download scanning, malware detection, and policy-based threat management. This guide will help you get Sentinel up and running.

## Quick Start

### 1. Starting Sentinel

The Sentinel daemon must be running before downloads are scanned:

```bash
# From the Ladybird build directory
cd /path/to/ladybird
./Build/release/bin/Sentinel
```

The Sentinel service will:
- Create a Unix socket at `/tmp/sentinel.sock`
- Initialize the policy database at `~/.local/share/Ladybird/PolicyGraph/`
- Load YARA rules for malware detection
- Start listening for connections from the browser

**Note:** Keep this terminal window open. Sentinel runs in the foreground and outputs diagnostic information.

### 2. Verify Connection

1. Open Ladybird browser
2. Navigate to `about:security`
3. Check the "System Status" section:
   - **SentinelServer Connection**: Should show "Connected" (green)
   - **Security Scanning**: Should show "Enabled" (green)

### 3. Test Scanning

Download any file in Ladybird. Sentinel will:
- Intercept the download
- Scan the file content with YARA rules
- Apply security policies
- Show an alert if threats are detected

## Command-Line Tool

Sentinel includes a CLI tool for management tasks:

```bash
# Check Sentinel status
./Build/release/bin/sentinel-cli status

# List active security policies
./Build/release/bin/sentinel-cli list-policies

# View a specific policy
./Build/release/bin/sentinel-cli show-policy <policy-id>

# List quarantined files
./Build/release/bin/sentinel-cli list-quarantine

# Restore a quarantined file
./Build/release/bin/sentinel-cli restore <quarantine-id> <destination-path>

# Database maintenance
./Build/release/bin/sentinel-cli vacuum
./Build/release/bin/sentinel-cli verify

# Backup the policy database
./Build/release/bin/sentinel-cli backup
```

## Configuration

### Default Locations

- **Socket**: `/tmp/sentinel.sock`
- **Database**: `~/.local/share/Ladybird/PolicyGraph/policies.db`
- **Quarantine**: `~/.local/share/Ladybird/Quarantine/`
- **YARA Rules**: `./Base/res/ladybird/sentinel/rules/`

### YARA Rules

Sentinel ships with default YARA rulesets:

1. **default.yar**: EICAR test file detection
2. **web-threats.yar**: JavaScript malware, XSS patterns
3. **archives.yar**: Suspicious archive files
4. **user-custom.yar**: Your custom rules (empty by default)

Edit rules in: `./Base/res/ladybird/sentinel/rules/`

After modifying rules, restart Sentinel to reload them.

## Security Policies

### Policy Types

Sentinel supports several policy match types:

1. **File Hash**: Match exact SHA-256 hash
2. **URL Pattern**: SQL LIKE patterns (e.g., `%malicious.com%`)
3. **YARA Rule**: Match by rule name
4. **MIME Type**: Match by content type
5. **Download Origin**: Match by referring page
6. **Form Action Mismatch**: Detect credential exfiltration
7. **Insecure Credential Post**: Detect password sent over HTTP
8. **Third-Party Form Post**: Detect tracking forms

### Policy Actions

- **Allow**: Permit the download
- **Block**: Block the download immediately
- **Quarantine**: Isolate the file for review
- **Warn**: Show warning, let user decide

### Creating Policies

**Via Browser UI:**
1. Go to `about:security`
2. Use "Policy Templates" for common scenarios
3. Or create custom policies in the "Active Security Policies" section

**Via Alert Dialog:**
When a threat is detected, you can:
- Choose an action (Allow/Block/Quarantine)
- Check "Remember this decision" to create a policy
- The policy will apply to future similar threats

**Via CLI:**
```bash
# Policies are managed through the about:security UI
# The CLI tool is read-only for policy viewing
```

## Troubleshooting

### Connection Issues

**Problem**: `about:security` shows "Disconnected"

**Solutions:**
1. Verify Sentinel is running: `ps aux | grep Sentinel`
2. Check socket exists: `ls -l /tmp/sentinel.sock`
3. Check Sentinel logs in the terminal where you started it
4. Try restarting Sentinel
5. Check permissions on socket file

### Scanning Not Working

**Problem**: Files download without being scanned

**Solutions:**
1. Ensure Sentinel is connected (check `about:security`)
2. Verify YARA rules are loaded (check Sentinel startup output)
3. Check that RequestServer has SecurityTap enabled
4. Look for errors in Sentinel's terminal output

### Database Issues

**Problem**: PolicyGraph database errors

**Solutions:**
```bash
# Verify database integrity
./Build/release/bin/sentinel-cli verify

# If corrupted, backup and reset:
mv ~/.local/share/Ladybird/PolicyGraph ~/.local/share/Ladybird/PolicyGraph.backup
# Restart Sentinel to create fresh database
```

### Performance Issues

**Problem**: Downloads are slow

**Causes:**
- Large files take longer to scan
- Many YARA rules increase scan time
- Database lookups on slow storage

**Solutions:**
1. Disable unused YARA rule files
2. Use SSDs for database storage
3. Check metrics: `about:security` → "Security Statistics"
4. Reduce policy count if very high (>1000)

## Production Deployment

### Running as a System Service

#### Linux (systemd)

Create `/etc/systemd/system/sentinel.service`:

```ini
[Unit]
Description=Ladybird Sentinel Security Service
After=network.target

[Service]
Type=simple
User=your-username
ExecStart=/path/to/ladybird/Build/release/bin/Sentinel
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable sentinel
sudo systemctl start sentinel
sudo systemctl status sentinel
```

#### macOS (launchd)

Create `~/Library/LaunchAgents/com.ladybird.sentinel.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.ladybird.sentinel</string>
    <key>ProgramArguments</key>
    <array>
        <string>/path/to/ladybird/Build/release/bin/Sentinel</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
```

Load the service:
```bash
launchctl load ~/Library/LaunchAgents/com.ladybird.sentinel.plist
```

### Security Considerations

1. **Socket Permissions**: The `/tmp/sentinel.sock` file should only be accessible by your user
2. **Database Encryption**: Consider encrypting `~/.local/share/Ladybird/` if it contains sensitive policy data
3. **YARA Rules**: Regularly update rules to detect new threats
4. **Quarantine Cleanup**: Periodically review and delete old quarantined files
5. **Access Control**: In multi-user systems, ensure each user runs their own Sentinel instance

## Advanced Features

### Metrics and Monitoring

Sentinel tracks performance metrics:
- Download scan time
- Policy query time
- Cache hit rate
- Threat detection statistics

View metrics at `about:security` → "Security Statistics"

### Custom YARA Rules

Add your own detection rules in `user-custom.yar`:

```yara
rule Suspicious_Download {
    meta:
        description = "Detects suspicious patterns"
        severity = "medium"

    strings:
        $s1 = "eval(" nocase
        $s2 = "document.write" nocase
        $s3 = "unescape" nocase

    condition:
        2 of them
}
```

### Database Backup

Regular backups recommended:

```bash
# Automated backup
./Build/release/bin/sentinel-cli backup

# Manual backup
cp ~/.local/share/Ladybird/PolicyGraph/policies.db \
   ~/backups/policies-$(date +%Y%m%d).db
```

## Getting Help

### Log Output

Sentinel provides diagnostic output:
- Database initialization status
- YARA rule loading
- Connection events
- Scan results
- Policy matches

### Debug Mode

For verbose logging, set environment variable:
```bash
SENTINEL_DEBUG=1 ./Build/release/bin/Sentinel
```

### Reporting Issues

When reporting issues, include:
1. Sentinel version (git commit hash)
2. Operating system and version
3. Sentinel log output
4. Steps to reproduce the issue
5. Content of `about:security` status page

## Architecture Overview

```
┌─────────────────┐
│ Ladybird Browser│
└────────┬────────┘
         │ IPC
         ▼
┌─────────────────┐         ┌──────────────┐
│ Request Server  │────────▶│ SentinelServer│
│  (SecurityTap)  │  Socket │   (Daemon)    │
└─────────────────┘         └──────┬───────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
                    ▼              ▼              ▼
              ┌──────────┐  ┌──────────┐  ┌──────────┐
              │PolicyGraph│  │YARA Engine│  │Quarantine│
              └──────────┘  └──────────┘  └──────────┘
```

### Components

1. **Sentinel Daemon**: Core service, Unix socket listener
2. **PolicyGraph**: SQLite database for policy/threat storage
3. **YARA Engine**: Pattern-based malware detection
4. **SecurityTap**: IPC bridge in RequestServer
5. **SecurityUI**: Browser UI (`about:security`)
6. **Quarantine**: Isolated storage for suspicious files
7. **FormMonitor**: Credential exfiltration detection
8. **FlowInspector**: Behavioral analysis

## Phase Information

This setup guide covers **Sentinel Phase 5** features:
- ✅ Integration testing infrastructure
- ✅ Performance profiling and benchmarks
- ✅ Security hardening (audit passed)
- ✅ Error handling with graceful degradation
- ✅ Production readiness (metrics, CLI, configuration)
- ✅ Milestone 0.2 foundation (FormMonitor, FlowInspector)

**Coming in Phase 6:**
- Enhanced credential exfiltration detection
- Machine learning-based threat classification
- Cloud-based threat intelligence
- Browser extension for policy sync
- Enterprise management console

## Support

For questions and support:
- GitHub Issues: https://github.com/LadybirdBrowser/ladybird
- Documentation: `/docs/SENTINEL_*.md`
- Status Page: `about:security` in browser

---

**Sentinel Security Service** - Protecting your downloads, one scan at a time. 🛡️
