# Sentinel Phase 5 - Testing, Hardening, and Milestone 0.2 Preparation

**Status**: Ready to Start
**Timeline**: Days 29-35 (1 week)
**Focus**: Production readiness and Milestone 0.2 foundation

---

## Overview

Phase 5 completes Milestone 0.1 by hardening the existing implementation, adding comprehensive testing, and laying groundwork for Milestone 0.2 (Credential Exfiltration Detection).

**Goals**:
1. Comprehensive testing of Phases 1-4 functionality
2. Performance profiling and optimization
3. Security hardening and error handling
4. Production readiness improvements
5. Foundation for Milestone 0.2

---

## Day 29-30: Integration Testing and Bug Fixes

### Objectives
- End-to-end testing of complete system
- Identify and fix critical bugs
- Validate all user flows

### Tasks

#### Test Suite 1: Download Vetting Flow
1. Download EICAR test file
   - Verify SecurityTap intercepts
   - Verify YARA detection triggers
   - Verify SecurityAlertDialog appears
   - Create policy and verify storage
   - Re-download and verify auto-enforcement

2. Download clean file
   - Verify no alert triggered
   - Verify normal download flow
   - Verify no performance degradation

3. Policy enforcement
   - Test block action
   - Test quarantine action
   - Test allow action
   - Verify rate limiting works

#### Test Suite 2: UI Components
1. SecurityNotificationBanner
   - Test all notification types
   - Verify auto-dismiss timing
   - Test queue behavior
   - Test "View Policy" navigation

2. QuarantineManagerDialog
   - List quarantined files
   - Restore file functionality
   - Delete file functionality
   - Export to CSV
   - Search and filter

3. about:security page
   - Real-time status updates
   - Policy template creation
   - Policy management (edit/delete)
   - Statistics display
   - Network audit log

#### Test Suite 3: Backend Components
1. PolicyGraph
   - Policy CRUD operations
   - Policy matching accuracy
   - Cache effectiveness
   - Database maintenance
   - Cleanup old threats

2. SecurityTap
   - Download detection
   - Metadata extraction
   - Hash computation
   - IPC communication
   - Error handling

3. Quarantine system
   - File isolation
   - Metadata storage
   - Directory permissions
   - File restoration
   - Cleanup on delete

### Acceptance Criteria
- All test suites pass without errors
- No memory leaks (run with ASAN)
- No crashes under normal operation
- Critical bugs documented and fixed

---

## Day 31: Performance Profiling and Optimization

### Objectives
- Measure system overhead
- Identify bottlenecks
- Optimize hot paths

### Profiling Targets

#### Download Path
```bash
# Test scenarios
1. Small file (100 KB) - 100 downloads
2. Medium file (10 MB) - 20 downloads
3. Large file (100 MB) - 5 downloads

# Metrics to capture
- Download time (with/without Sentinel)
- YARA scan time
- PolicyGraph query time
- IPC overhead
- Memory usage
```

#### Database Performance
```sql
-- Measure query times
1. Policy match query (< 5ms target)
2. Threat history query (< 50ms target)
3. Statistics aggregation (< 100ms target)
4. Policy creation (< 10ms target)
```

#### UI Responsiveness
- SecurityAlertDialog load time (< 100ms)
- about:security page load (< 200ms)
- Notification display (< 50ms)
- Template preview generation (< 100ms)

### Optimization Tasks

1. **PolicyGraph Cache**
   - Measure cache hit rate
   - Tune cache size if needed
   - Add cache warming on startup

2. **YARA Scanning**
   - Profile rule compilation
   - Test stream scanning for large files
   - Consider rule optimization

3. **IPC Communication**
   - Measure message serialization time
   - Test async IPC for large payloads
   - Add IPC message batching if needed

4. **UI Rendering**
   - Profile JavaScript execution in about:security
   - Optimize table rendering in QuarantineManager
   - Add pagination if needed

### Deliverables
- Performance report with benchmarks
- Optimization recommendations
- Implemented critical optimizations

---

## Day 32: Security Hardening

### Objectives
- Audit security-critical code paths
- Fix potential vulnerabilities
- Add defensive checks

### Security Audit Areas

#### 1. Input Validation
**Files to audit**:
- `Services/RequestServer/ConnectionFromClient.cpp`
- `Services/Sentinel/PolicyGraph.cpp`
- `Libraries/LibWebView/WebUI/SecurityUI.cpp`

**Checks**:
- All IPC message parameters validated
- URL lengths checked against limits
- File paths sanitized (no directory traversal)
- Hash strings validated (hex format, correct length)
- JSON parsing has error handling

#### 2. File System Operations
**Files to audit**:
- `Services/RequestServer/Quarantine.cpp`
- `Services/Sentinel/PolicyGraph.cpp`

**Checks**:
- Quarantine directory permissions (0700)
- No symlink following in quarantine
- Atomic file operations
- Secure temp file creation
- Proper cleanup on errors

#### 3. Database Security
**Files to audit**:
- `Services/Sentinel/PolicyGraph.cpp`

**Checks**:
- SQL injection prevention (parameterized queries)
- Database file permissions (0600)
- Transaction handling
- Corruption detection
- Backup integrity

#### 4. IPC Security
**Files to audit**:
- `Services/RequestServer/ConnectionFromClient.cpp`
- `Services/WebContent/ConnectionFromClient.cpp`

**Checks**:
- Rate limiting enforced
- Message size limits
- Request ID validation
- Client authentication
- Error message sanitization

### Hardening Tasks

1. Add input validation functions
```cpp
// In IPC::Limits or similar
bool validate_hash_string(StringView hash);
bool validate_policy_name(StringView name);
bool validate_file_extension(StringView ext);
```

2. Add filesystem helpers
```cpp
// In Core::FileSystem or similar
ErrorOr<void> ensure_secure_directory(StringView path);
ErrorOr<void> secure_file_copy(StringView src, StringView dest);
bool is_safe_path(StringView path, StringView base_dir);
```

3. Add database integrity checks
```cpp
// In PolicyGraph
ErrorOr<void> verify_database_integrity();
ErrorOr<void> backup_database();
ErrorOr<void> restore_from_backup();
```

### Deliverables
- Security audit report
- Fixed vulnerabilities
- Added defensive checks
- Updated security documentation

---

## Day 33: Error Handling and Resilience

### Objectives
- Graceful degradation on failures
- User-friendly error messages
- Recovery mechanisms

### Error Scenarios to Handle

#### 1. Sentinel Daemon Failures
**Scenarios**:
- Sentinel process crashes
- Sentinel not started
- IPC connection lost
- YARA rules compilation fails

**Handling**:
```cpp
// In SecurityTap
if (!sentinel_connected()) {
    // Fail-open: Allow download without scanning
    dbgln("Sentinel unavailable, allowing download");
    return DownloadDecision::Allow;
}
```

#### 2. Database Failures
**Scenarios**:
- Database corruption
- Disk full
- Permission denied
- Lock timeout

**Handling**:
```cpp
// In PolicyGraph
auto result = query_policy();
if (result.is_error()) {
    // Fall back to in-memory policies
    return check_memory_cache();
}
```

#### 3. Quarantine Failures
**Scenarios**:
- Quarantine directory missing
- Disk full
- Permission denied
- File already exists

**Handling**:
```cpp
// In Quarantine
auto result = quarantine_file(path);
if (result.is_error()) {
    // Show error to user, offer alternatives
    show_error_dialog("Cannot quarantine file: {}", result.error());
    return offer_block_or_allow();
}
```

#### 4. UI Failures
**Scenarios**:
- about:security page fails to load
- SecurityAlertDialog crashes
- Template parsing error

**Handling**:
- Show error page with fallback UI
- Log error details for debugging
- Offer manual policy management

### Tasks

1. Add error recovery mechanisms
2. Improve error messages (user-friendly)
3. Add logging for debugging
4. Test failure scenarios
5. Document known issues and workarounds

### Deliverables
- Comprehensive error handling
- User-friendly error messages
- Recovery procedures documented
- Failure test suite

---

## Day 34: Production Readiness

### Objectives
- Monitoring and observability
- Configuration management
- Deployment preparation

### Monitoring

#### 1. System Health Metrics
**Metrics to track**:
```cpp
struct SentinelMetrics {
    size_t total_downloads_scanned;
    size_t threats_detected;
    size_t policies_enforced;
    size_t cache_hits;
    size_t cache_misses;
    Duration average_scan_time;
    Duration average_query_time;
    size_t database_size_bytes;
    size_t quarantine_size_bytes;
};
```

**Implementation**:
- Add metrics collection in SecurityTap
- Add metrics endpoint in about:security
- Add periodic metrics logging

#### 2. Diagnostic Tools
**Tools to create**:
```bash
# Sentinel status check
./sentinel-cli status

# Policy database inspection
./sentinel-cli list-policies
./sentinel-cli show-policy <id>

# Quarantine management
./sentinel-cli list-quarantine
./sentinel-cli restore <id> <path>

# Database maintenance
./sentinel-cli vacuum
./sentinel-cli verify
./sentinel-cli backup
```

### Configuration

#### 1. User Configuration File
**Location**: `~/.config/ladybird/sentinel/config.json`

**Options**:
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

#### 2. YARA Rules Management
**Default rules location**: `~/.config/ladybird/sentinel/rules/`

**Rules to include**:
- `default.yar` - Basic malware signatures
- `web-threats.yar` - JavaScript malware
- `archives.yar` - Suspicious archives
- `user-custom.yar` - User-defined rules

### Deployment

#### 1. Installation
- Add Sentinel to install targets
- Create default directories on first run
- Copy default YARA rules
- Initialize database schema

#### 2. Upgrade Path
- Database migration system
- Config file versioning
- Backward compatibility checks

### Deliverables
- Metrics collection system
- Diagnostic CLI tool
- Configuration file support
- Installation procedures
- Upgrade migration system

---

## Day 35: Milestone 0.2 Foundation

### Objectives
- Design Credential Exfiltration Detection
- Implement foundation components
- Prepare for Phase 6

### Milestone 0.2 Overview

**Goal**: Detect credential exfiltration attempts

**Detection scenarios**:
1. Login form posts to unexpected domain
2. Password field value sent to third-party
3. Cookie theft attempts
4. Form data sent over HTTP (not HTTPS)
5. Autofill to suspicious forms

### Foundation Components

#### 1. Form Monitor
**File**: `Services/WebContent/FormMonitor.h/cpp`

**Responsibilities**:
- Track form submissions
- Detect password fields
- Monitor autofill events
- Extract form action URLs

**Integration point**:
```cpp
// In WebContent when form submitted
void FormMonitor::on_form_submit(
    DOM::HTMLFormElement& form,
    String const& action_url,
    Vector<FormField> const& fields
) {
    // Check for password fields
    bool has_password = fields.any_of([](auto& f) {
        return f.type == "password";
    });

    if (has_password) {
        // Extract origin of form vs action URL
        auto form_origin = extract_origin(form.document().url());
        auto action_origin = extract_origin(action_url);

        if (form_origin != action_origin) {
            // Potential credential exfiltration
            send_alert_to_sentinel({
                .type = "credential_exfil",
                .form_origin = form_origin,
                .action_origin = action_origin,
                .uses_https = action_url.starts_with("https://")
            });
        }
    }
}
```

#### 2. Flow Inspector Protocol
**File**: `Services/Sentinel/FlowInspector.h/cpp`

**Responsibilities**:
- Receive form submission events
- Analyze behavioral patterns
- Generate credential exfil alerts
- Learn trusted form relationships

**Alert format**:
```json
{
  "alert_type": "credential_exfiltration",
  "severity": "high",
  "form_origin": "https://example.com",
  "action_origin": "https://malicious-site.ru",
  "uses_https": false,
  "has_password_field": true,
  "timestamp": "2025-10-30T12:00:00Z"
}
```

#### 3. Policy Extensions
**New policy types**:
```cpp
enum class PolicyMatchType {
    DownloadOriginFileType,  // Existing
    FormActionMismatch,      // NEW: Form posts to different origin
    InsecureCredentialPost,  // NEW: Password sent over HTTP
    ThirdPartyFormPost       // NEW: Form posts to third-party
};
```

**New enforcement actions**:
```cpp
enum class PolicyAction {
    Block,                   // Existing
    Quarantine,             // Existing
    Allow,                  // Existing
    BlockAutofill,          // NEW: Prevent autofill
    WarnUser                // NEW: Show warning, allow if confirmed
};
```

### Implementation Tasks

1. Create FormMonitor in WebContent
2. Add IPC messages for form events
3. Create FlowInspector in Sentinel
4. Extend PolicyGraph for new policy types
5. Add UI for credential exfil alerts
6. Write basic flow detection rules

### Testing

**Test scenarios**:
1. Login to legitimate site (no alert)
2. Form posts to different domain (alert)
3. Password sent over HTTP (alert)
4. Create policy to block suspicious form
5. Verify autofill prevention works

### Deliverables
- FormMonitor component
- FlowInspector foundation
- Extended policy types
- Form event IPC messages
- Initial test suite
- Phase 6 implementation plan

---

## Phase 5 Summary

### Goals Achieved
1. Comprehensive testing of Milestone 0.1
2. Performance optimized
3. Security hardened
4. Production ready
5. Milestone 0.2 foundation laid

### Metrics
- Test coverage: 80%+ for critical paths
- Performance overhead: < 5% for downloads
- Security audit: No critical vulnerabilities
- Error handling: Graceful degradation implemented
- Documentation: Complete

### Next Phase
**Phase 6: Credential Exfiltration Detection (Milestone 0.2)**
- Full FormMonitor implementation
- FlowInspector rules engine
- Credential exfil policy enforcement
- Autofill protection
- User education UI

---

## Task Breakdown

### Day 29: Testing - Download Flow
- [ ] Write download vetting test suite
- [ ] Test EICAR detection
- [ ] Test policy creation flow
- [ ] Test auto-enforcement
- [ ] Fix identified bugs

### Day 30: Testing - UI and Backend
- [ ] Test all UI components
- [ ] Test PolicyGraph operations
- [ ] Test Quarantine system
- [ ] Run ASAN/Valgrind tests
- [ ] Document known issues

### Day 31: Performance
- [ ] Profile download path
- [ ] Profile database queries
- [ ] Profile UI rendering
- [ ] Implement optimizations
- [ ] Benchmark results

### Day 32: Security
- [ ] Audit input validation
- [ ] Audit file operations
- [ ] Audit database security
- [ ] Audit IPC security
- [ ] Fix vulnerabilities

### Day 33: Error Handling
- [ ] Handle Sentinel failures
- [ ] Handle database failures
- [ ] Handle quarantine failures
- [ ] Improve error messages
- [ ] Test failure scenarios

### Day 34: Production Readiness
- [ ] Add metrics collection
- [ ] Create diagnostic tools
- [ ] Add configuration support
- [ ] Document installation
- [ ] Create upgrade procedures

### Day 35: Milestone 0.2 Foundation
- [ ] Design FormMonitor
- [ ] Design FlowInspector
- [ ] Extend PolicyGraph schema
- [ ] Add form event IPC
- [ ] Write Phase 6 plan

---

## Dependencies

**Required from Phase 4**:
- SecurityNotificationBanner (working)
- QuarantineManagerDialog (working)
- PolicyGraph cache (implemented)
- SecurityTap (stable)

**External dependencies**:
- YARA library (installed)
- SQLite (available)
- Qt framework (available)

**New dependencies**:
None for Phase 5

---

## Risk Assessment

### Low Risk
- Testing and bug fixes
- Performance profiling
- Documentation updates

### Medium Risk
- Security hardening (requires careful code review)
- Error handling (must not break existing functionality)
- Configuration system (backward compatibility)

### High Risk
- Milestone 0.2 foundation (new architecture)
- FormMonitor integration (WebContent changes)

**Mitigation**:
- Incremental implementation
- Feature flags for new code
- Comprehensive testing
- Code review for security changes

---

## Success Criteria

Phase 5 is complete when:
1. All test suites pass
2. Performance meets targets (< 5% overhead)
3. Security audit complete (no critical issues)
4. Error handling tested and working
5. Production deployment ready
6. Milestone 0.2 foundation implemented
7. Phase 6 plan approved

---

**Plan Version**: 1.0
**Created**: 2025-10-30
**Author**: Development Team
