# Sentinel Changelog

## Milestone 0.2 - Credential Protection (2025-10-31)

Added real-time credential exfiltration detection and protection.

### Features

- FormMonitor component for detecting cross-origin credential submissions
- Security alerts for suspicious form submissions
- Trusted form relationship management
- Password autofill protection for untrusted forms
- One-time autofill override mechanism
- Integration with about:security page
- User education modal and security tips
- Comprehensive end-to-end tests

### Components

- Services/WebContent/FormMonitor: Core credential protection engine
- Services/WebContent/PageClient: Form submission interception
- UI/Qt/SecurityAlertDialog: Extended for credential alerts
- Base/res/ladybird/about-pages/security.html: Dashboard integration
- Tests/LibWeb/Text/input/credential-protection-*.html: Test suite

### Documentation

- docs/USER_GUIDE_CREDENTIAL_PROTECTION.md: User guide
- docs/SENTINEL_ARCHITECTURE.md: Updated with Milestone 0.2 details

## Milestone 0.1 - Malware Scanning (2025-10-29)

Initial release of Sentinel malware protection system.

### Features

- YARA-based malware detection for downloads
- SecurityTap integration with RequestServer
- PolicyGraph SQLite database for security policies
- Quarantine system for suspicious files
- Security alert dialogs
- about:security management interface
- Real-time threat detection and blocking

### Components

- Services/Sentinel/SentinelServer: Standalone YARA scanning daemon
- Services/RequestServer/SecurityTap: Integration layer
- Services/Sentinel/PolicyGraph: Policy database
- Services/RequestServer/Quarantine: File isolation
- Libraries/LibWebView/WebUI/SecurityUI: Management interface

### Documentation

- docs/SENTINEL_ARCHITECTURE.md: System architecture
- docs/SENTINEL_SETUP_GUIDE.md: Installation guide
- docs/SENTINEL_USER_GUIDE.md: End-user documentation
- docs/SENTINEL_POLICY_GUIDE.md: Policy management
- docs/SENTINEL_YARA_RULES.md: Rule documentation

## Development Phases

### Phase 1: Foundation
- Basic YARA integration
- SentinelServer daemon
- Unix socket IPC

### Phase 2: Download Protection
- SecurityTap integration
- Request interception
- Alert system

### Phase 3: Policy Management
- PolicyGraph database
- Policy matching and enforcement
- about:security interface

### Phase 4: User Experience
- Security alert dialogs
- Quarantine management
- Notification system

### Phase 5: Production Hardening
- Error handling and graceful degradation
- Performance optimizations
- Security audit and hardening
- Circuit breaker pattern
- Health checks and monitoring

### Phase 6: Credential Protection
- FormMonitor implementation
- Cross-origin detection
- Trusted relationships
- Autofill protection
- User education
- Comprehensive testing

## Milestone 0.3 - Enhanced Credential Protection (In Progress)

Phases 1-4 complete. Database schema, PolicyGraph API, FormMonitor integration, and import/export UI implemented.

### Completed Features (Phases 1-4)

- Database schema for credential relationships and alerts
- PolicyGraph API extensions (21 new methods)
- Persistent trusted/blocked form relationships
- Credential alert history tracking
- FormMonitor-PolicyGraph integration with hybrid cache strategy
- Relationship persistence across browser restarts
- Import/export UI for backing up and restoring trusted relationships
- JSON-based credential relationship export with browser download
- File picker-based import with validation

### Implementation Documentation

- Services/Sentinel/PolicyGraph.h: API extensions for Milestone 0.3
- Services/Sentinel/DatabaseMigrations.cpp: Schema v2 to v3 migration
- Services/WebContent/FormMonitor.h/cpp: PolicyGraph integration
- Base/res/ladybird/about-pages/security.html: Import/export UI
- Libraries/LibWebView/WebUI/SecurityUI.h/cpp: Import/export handlers
- docs/MILESTONE_0.3_PLAN.md: Complete implementation plan

### Remaining Features (Phases 5-6)

- Policy templates for credential protection (Phase 5)
- Basic form anomaly detection (Phase 6, optional)
