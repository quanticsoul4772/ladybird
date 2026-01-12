# Phase 6 Manual Testing Checklist

**Milestone:** 0.4 - Advanced Detection
**Phase:** 6 - Network Behavioral Analysis
**Purpose:** Manual functional testing procedures

---

## Pre-Testing Setup

### Prerequisites
- [ ] Ladybird browser built: `/home/rbsmith4/ladybird/Build/release/bin/Ladybird`
- [ ] Test HTML pages available: `/home/rbsmith4/ladybird/Tests/Integration/html/`
- [ ] PolicyGraph database initialized: `~/.config/ladybird/policy_graph.db`
- [ ] Automated tests passed: Run `./Tests/Integration/test_network_monitoring.sh`

### Environment Setup
```bash
# Optional: Enable debug logging for detailed output
export WEBCONTENT_DEBUG=1
export REQUESTSERVER_DEBUG=1
export SENTINEL_DEBUG=1

# Start Ladybird
./Build/release/bin/Ladybird
```

---

## Test 1: DGA Detection

### Objective
Verify that Domain Generation Algorithm (DGA) detection works correctly.

### Test Procedure
1. [ ] Start Ladybird browser
2. [ ] Navigate to test page:
   ```
   file:///home/rbsmith4/ladybird/Tests/Integration/html/test_dga_detection.html
   ```
3. [ ] Page loads and attempts to fetch from DGA-like domains:
   - `xk3j9f2lm8n.com`
   - `q5w7r9t2y4u.net`
   - `a1b2c3d4e5f.org`
4. [ ] Wait 5-10 seconds for pattern analysis
5. [ ] Navigate to `about:security`

### Expected Results
- [ ] **RequestServer logs** (if debug enabled):
  ```
  TrafficMonitor: DGA-like domain detected: xk3j9f2lm8n.com
  TrafficMonitor: High entropy: 3.46, Consonant ratio: 1.00
  TrafficMonitor: Generating network behavior alert
  ```
- [ ] **about:security Dashboard:**
  - [ ] Alert appears in "Network Behavior Alerts" section
  - [ ] Alert type: "Suspicious Domain (DGA)"
  - [ ] Domain listed: `xk3j9f2lm8n.com`
  - [ ] Confidence: ~0.66-0.85 (medium-high)
  - [ ] Explanation: "Suspicious characteristics: Excessive consonants, Unusual character patterns"
- [ ] **Policy Options:**
  - [ ] "Block Domain" button available
  - [ ] "Monitor Domain" button available
  - [ ] "Allow Domain" button available

### Test Actions
- [ ] Click "Block Domain"
- [ ] Verify policy created in PolicyGraph
- [ ] Reload test page
- [ ] Verify blocked domain no longer sends requests

### Pass/Fail Criteria
**PASS:** Alert generated with reasonable confidence (>0.6), policy creation works
**FAIL:** No alert, crash, or policy creation fails

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 2: C2 Beaconing Detection

### Objective
Verify that Command & Control beaconing pattern detection works correctly.

### Test Procedure
1. [ ] Start Ladybird browser
2. [ ] Navigate to test page:
   ```
   file:///home/rbsmith4/ladybird/Tests/Integration/html/test_beaconing_detection.html
   ```
3. [ ] Observe page status showing beacon count incrementing every 5 seconds
4. [ ] Let page run for **at least 2 minutes** (12+ requests)
5. [ ] Wait additional 5 minutes for pattern analysis (analysis runs every 5 min)
6. [ ] Navigate to `about:security`

### Expected Results
- [ ] **Page Display:**
  ```
  Sent 12 beacons to suspicious-c2-server.example.com (every 5s)
  ```
- [ ] **RequestServer logs** (if debug enabled):
  ```
  TrafficMonitor: Regular intervals detected for suspicious-c2-server.example.com
  TrafficMonitor: CV=0.02, Period=5.0s, Count=12
  TrafficMonitor: C2 beaconing detected
  ```
- [ ] **about:security Dashboard:**
  - [ ] Alert type: "C2 Beaconing"
  - [ ] Domain: `suspicious-c2-server.example.com`
  - [ ] Confidence: ~0.95 (very high)
  - [ ] Explanation: "Highly regular intervals (CV=0.02), 12 requests with 5.0s period"
  - [ ] Details: Shows request count, interval, CV value

### Test Actions
- [ ] Click "Monitor Domain" (allows traffic but logs)
- [ ] Verify policy set to "monitor"
- [ ] Reload page and let run again
- [ ] Verify requests still go through but are logged

### Edge Cases to Test
- [ ] **Irregular intervals:** Manually trigger requests at random times (should NOT alert)
- [ ] **Insufficient data:** Stop after 4 requests (should NOT alert, need 5+)
- [ ] **Legitimate beaconing:** Navigate to legitimate site with polling (e.g., Gmail) - should whitelist

### Pass/Fail Criteria
**PASS:** Beaconing detected after 12+ regular requests, CV calculated correctly
**FAIL:** No alert, false positive on irregular traffic, or crash

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 3: Data Exfiltration Detection

### Objective
Verify that large data upload (exfiltration) detection works correctly.

### Test Procedure
1. [ ] Start Ladybird browser
2. [ ] Navigate to test page:
   ```
   file:///home/rbsmith4/ladybird/Tests/Integration/html/test_exfiltration_detection.html
   ```
3. [ ] Click "Start Exfiltration Test" button
4. [ ] Wait for upload to complete (or fail as expected)
5. [ ] Navigate to `about:security`

### Expected Results
- [ ] **Page Display:**
  ```
  Uploading 10.00 MB...
  Upload failed (expected): [error message]
  ```
- [ ] **RequestServer logs** (if debug enabled):
  ```
  TrafficMonitor: High upload ratio detected for suspicious-exfil-server.example.com
  TrafficMonitor: Sent=10485760 bytes, Received=1024 bytes, Ratio=91%
  TrafficMonitor: Data exfiltration detected
  ```
- [ ] **about:security Dashboard:**
  - [ ] Alert type: "Data Exfiltration"
  - [ ] Domain: `suspicious-exfil-server.example.com`
  - [ ] Confidence: ~0.90 (high)
  - [ ] Explanation: "High upload ratio (91%), 10 MB sent vs 1 KB received"
  - [ ] Details: Shows bytes sent, bytes received, ratio

### Test Actions
- [ ] Click "Block Domain"
- [ ] Reload page
- [ ] Click button again
- [ ] Verify upload blocked immediately

### Edge Cases to Test
- [ ] **Legitimate upload service:**
  - Modify test page domain to `drive.google.com`
  - Upload 10MB
  - Should NOT alert (whitelisted)
- [ ] **Small volume:**
  - Modify data size to 1MB
  - High ratio but small volume
  - Should NOT alert (<5MB threshold)
- [ ] **Balanced traffic:**
  - Modify to upload 5MB, download 5MB
  - Ratio ~50%
  - Should NOT alert (<80% threshold)

### Pass/Fail Criteria
**PASS:** Exfiltration detected for 10MB upload with high ratio, whitelist works
**FAIL:** No alert, false positive on balanced traffic, or whitelist failure

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 4: about:security Dashboard

### Objective
Verify that the about:security dashboard displays network behavior data correctly.

### Test Procedure
1. [ ] After running Tests 1-3, navigate to `about:security`
2. [ ] Verify all sections present:
   - [ ] Overview section
   - [ ] Statistics section
   - [ ] Network Behavior Alerts section
   - [ ] Policy Management section

### Expected Results

#### Overview Section
- [ ] **Network Monitoring Status:** "Enabled"
- [ ] **Last Analysis:** Timestamp within last 5 minutes
- [ ] **Alerts Generated:** Count ≥ 3 (from tests 1-3)

#### Statistics Section
- [ ] **Requests Monitored:** Shows total request count
- [ ] **Domains Analyzed:** Shows unique domain count
- [ ] **Alerts Generated:** Matches overview count
- [ ] **Policies Active:** Shows count of created policies

#### Network Behavior Alerts Section
Table with columns:
- [ ] **Timestamp** (sortable)
- [ ] **Domain** (e.g., `xk3j9f2lm8n.com`)
- [ ] **Alert Type** (DGA, C2 Beaconing, Data Exfiltration)
- [ ] **Confidence** (0.0-1.0)
- [ ] **Status** (New, Acknowledged, Resolved)
- [ ] **Actions** (View Details, Create Policy, Dismiss)

#### Policy Management Section
- [ ] **Create Policy** button
- [ ] **Import/Export** buttons
- [ ] Policy list showing:
  - Domain
  - Policy action (Allow, Block, Monitor)
  - Threat type
  - Created date
  - Actions (Edit, Delete)

### Test Actions
- [ ] Sort alerts by timestamp (ascending/descending)
- [ ] Click "View Details" on an alert
  - [ ] Modal/panel opens with full details
  - [ ] Explanation text displayed
  - [ ] Technical indicators shown (CV, entropy, etc.)
  - [ ] Recommendation provided
- [ ] Click "Create Policy" from alert
  - [ ] Policy creation form appears
  - [ ] Domain pre-filled
  - [ ] Choose action (Block/Monitor/Allow)
  - [ ] Save policy
  - [ ] Verify policy appears in Policy Management section
- [ ] Edit existing policy
  - [ ] Change action from Block to Monitor
  - [ ] Save changes
  - [ ] Verify update reflected
- [ ] Delete policy
  - [ ] Confirmation prompt appears
  - [ ] Confirm deletion
  - [ ] Policy removed from list
- [ ] Export policies
  - [ ] Click "Export" button
  - [ ] JSON file downloaded
  - [ ] Verify file contains policy data
- [ ] Import policies
  - [ ] Delete all policies
  - [ ] Click "Import" button
  - [ ] Select exported JSON file
  - [ ] Verify policies restored

### Pass/Fail Criteria
**PASS:** Dashboard displays all data correctly, actions work, no crashes
**FAIL:** Missing sections, incorrect data, actions fail, or crash

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 5: Policy Enforcement

### Objective
Verify that created policies are enforced correctly.

### Test Procedure

#### Block Policy
1. [ ] Create "Block" policy for `xk3j9f2lm8n.com`
2. [ ] Navigate to test page: `test_dga_detection.html`
3. [ ] **Expected:** Requests to blocked domain rejected
4. [ ] **Verify:** Developer console shows blocked requests
5. [ ] **Verify:** RequestServer logs show policy enforcement:
   ```
   TrafficMonitor: Request to xk3j9f2lm8n.com blocked by policy
   ```

#### Monitor Policy
1. [ ] Change policy to "Monitor" for `suspicious-c2-server.example.com`
2. [ ] Navigate to test page: `test_beaconing_detection.html`
3. [ ] **Expected:** Requests allowed to proceed
4. [ ] **Verify:** Requests successful (may fail due to invalid domain, but not blocked by policy)
5. [ ] **Verify:** RequestServer logs show monitoring:
   ```
   TrafficMonitor: Request to suspicious-c2-server.example.com monitored
   ```

#### Allow Policy
1. [ ] Create "Allow" policy for `google.com`
2. [ ] Navigate to `https://google.com`
3. [ ] **Expected:** Requests allowed, no alerts generated
4. [ ] **Verify:** No analysis performed (skipped due to whitelist)

### Pass/Fail Criteria
**PASS:** Block prevents requests, Monitor allows but logs, Allow skips analysis
**FAIL:** Policies not enforced, requests blocked when should allow, or crash

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 6: Settings UI (if implemented)

### Objective
Verify that network monitoring can be enabled/disabled via Settings.

### Test Procedure
1. [ ] Open Ladybird Settings dialog
2. [ ] Navigate to "Security" or "Privacy" section
3. [ ] Locate "Network Monitoring" toggle

### Expected Results
- [ ] **Toggle present:** "Enable Network Behavioral Analysis"
- [ ] **Default state:** ON (enabled)
- [ ] **Description:** Brief explanation of feature

### Test Actions

#### Disable Monitoring
1. [ ] Toggle to OFF
2. [ ] Click "Save" or "Apply"
3. [ ] **Option A:** Restart required
   - [ ] Prompt: "Restart required to apply changes"
   - [ ] Restart Ladybird
4. [ ] **Option B:** Hot reload
   - [ ] Settings applied immediately
5. [ ] Navigate to test page
6. [ ] **Verify:** No alerts generated
7. [ ] **Verify:** RequestServer logs show:
   ```
   TrafficMonitor: Network monitoring disabled by user
   ```
8. [ ] Navigate to `about:security`
9. [ ] **Verify:** Status shows "Disabled"

#### Re-enable Monitoring
1. [ ] Toggle to ON
2. [ ] Apply/Save
3. [ ] Restart (if required)
4. [ ] Navigate to test page
5. [ ] **Verify:** Alerts generated again
6. [ ] **Verify:** Status shows "Enabled"

### Pass/Fail Criteria
**PASS:** Toggle works, monitoring can be disabled/enabled, settings persist
**FAIL:** Toggle has no effect, crashes on toggle, or settings not saved

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED  [ ] NOT IMPLEMENTED

**Notes:**
```


```

---

## Test 7: Performance and Stability

### Objective
Verify that network monitoring does not significantly impact browsing performance.

### Test Procedure

#### Memory Usage Test
1. [ ] Start Ladybird with fresh profile
2. [ ] Record baseline memory usage (RequestServer process)
3. [ ] Visit 50 different websites (mix of popular and test sites)
4. [ ] Record memory usage after
5. [ ] **Expected:** Memory increase ≤ 100KB (2KB × 50 domains)

#### CPU Usage Test
1. [ ] Monitor CPU usage with `top` or `htop`
2. [ ] Load test page that generates 100+ requests
3. [ ] Observe RequestServer CPU usage
4. [ ] **Expected:** CPU spike <5%, returns to baseline quickly

#### Latency Test
1. [ ] Use browser developer tools Network tab
2. [ ] Visit `https://google.com`
3. [ ] Record request latency
4. [ ] **Expected:** No noticeable increase compared to baseline (±1ms)

#### Heavy Load Test
1. [ ] Open 10 tabs
2. [ ] Navigate each tab to a different high-traffic site
3. [ ] Let run for 5 minutes
4. [ ] **Verify:** No crashes, memory leaks, or performance degradation
5. [ ] Check TrafficMonitor stats:
   ```
   TrafficMonitor: Tracking 500 patterns (max reached, LRU eviction active)
   ```

### Pass/Fail Criteria
**PASS:** Memory usage reasonable, CPU <5%, no latency increase, stable under load
**FAIL:** Memory leak, high CPU usage, crashes, or noticeable latency

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 8: Real-World Validation

### Objective
Test with real websites to validate detection and false positive rate.

### Test Procedure

#### Legitimate High-Traffic Sites (should NOT alert)
- [ ] `https://google.com` - Search engine
- [ ] `https://facebook.com` - Social media
- [ ] `https://github.com` - Code hosting
- [ ] `https://amazon.com` - E-commerce
- [ ] `https://youtube.com` - Video streaming
- [ ] `https://wikipedia.org` - Encyclopedia

**Expected:** No alerts (whitelisted popular domains)

#### Known Upload Services (should NOT alert)
- [ ] `https://drive.google.com` - Upload test file
- [ ] `https://dropbox.com` - Upload test file
- [ ] `https://onedrive.live.com` - Upload test file

**Expected:** No exfiltration alerts (whitelisted upload services)

#### Borderline Cases
- [ ] Sites with WebSockets (e.g., `https://slack.com`) - regular keep-alive
  - **Expected:** May trigger beaconing, but should whitelist or adjust threshold
- [ ] Sites with lots of subdomains (e.g., CDN domains)
  - **Expected:** Should NOT trigger DGA (legitimate subdomains)

#### Known Malicious Domains (CAUTION: Test in isolated environment)
- [ ] Visit known phishing URL (from PhishTank)
  - **Expected:** Phishing detection AND network behavior alert
- [ ] Visit DGA domain (from malware feed)
  - **Expected:** DGA detection alert

**WARNING:** Only test with known malicious domains in a safe, isolated environment (VM, sandbox).

### Pass/Fail Criteria
**PASS:** No false positives on legitimate sites, detects real threats
**FAIL:** False positives on Google/Facebook/etc., or misses known threats

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 9: Error Handling and Edge Cases

### Objective
Verify that the system handles edge cases and errors gracefully.

### Test Scenarios

#### Empty/Invalid Domains
- [ ] Request with empty domain
  - **Expected:** Rejected, no crash
- [ ] Request with invalid domain (e.g., `invalid..domain.com`)
  - **Expected:** Handled gracefully, no crash

#### Database Errors
- [ ] Corrupt PolicyGraph database
  - **Expected:** Fallback to in-memory policies, warning logged
- [ ] Read-only database file
  - **Expected:** Graceful degradation, continue with read-only

#### Resource Exhaustion
- [ ] Trigger 1000+ unique domain requests (exceed 500 pattern limit)
  - **Expected:** LRU eviction, oldest patterns removed, no crash
- [ ] Generate 200+ alerts (exceed 100 alert limit)
  - **Expected:** FIFO eviction, oldest alerts removed, no crash

#### Concurrent Access
- [ ] Open multiple browser tabs simultaneously requesting different domains
  - **Expected:** TrafficMonitor handles concurrent access, no race conditions

#### Network Failures
- [ ] Request to domain that times out
  - **Expected:** Request recorded, analysis proceeds, no hang
- [ ] Request to domain with DNS failure
  - **Expected:** Recorded as failed, no crash

### Pass/Fail Criteria
**PASS:** All edge cases handled gracefully, no crashes or data corruption
**FAIL:** Crashes, hangs, data corruption, or unhandled exceptions

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Test 10: User Experience

### Objective
Evaluate the user-friendliness of the network monitoring feature.

### Evaluation Criteria

#### Alert Clarity
- [ ] Alert explanations are clear and understandable by non-technical users
- [ ] Technical details hidden by default (expandable for power users)
- [ ] Recommendations actionable (e.g., "Block this domain to prevent further requests")

#### Dashboard Usability
- [ ] about:security layout is clean and organized
- [ ] Navigation intuitive (no hunting for features)
- [ ] Actions clearly labeled (buttons, links)
- [ ] Responsive design (works at different window sizes)

#### Performance Perception
- [ ] No noticeable lag when monitoring active
- [ ] Alerts appear promptly (within 1 second of detection)
- [ ] Dashboard loads quickly (<500ms)

#### Error Messages
- [ ] Error messages helpful (not just "Error occurred")
- [ ] Suggest corrective actions when possible

#### Documentation
- [ ] Help button/link present in dashboard
- [ ] User guide accessible and comprehensive
- [ ] Tooltips/hints for advanced features

### Pass/Fail Criteria
**PASS:** Feature easy to understand and use, good UX design
**FAIL:** Confusing UI, poor performance perception, or unclear messaging

**Result:** [ ] PASS  [ ] FAIL  [ ] BLOCKED

**Notes:**
```


```

---

## Overall Test Summary

### Test Results

| Test | Name                       | Status | Notes |
|------|---------------------------|--------|-------|
| 1    | DGA Detection             | [ ]    |       |
| 2    | C2 Beaconing Detection    | [ ]    |       |
| 3    | Data Exfiltration         | [ ]    |       |
| 4    | about:security Dashboard  | [ ]    |       |
| 5    | Policy Enforcement        | [ ]    |       |
| 6    | Settings UI               | [ ]    |       |
| 7    | Performance & Stability   | [ ]    |       |
| 8    | Real-World Validation     | [ ]    |       |
| 9    | Error Handling            | [ ]    |       |
| 10   | User Experience           | [ ]    |       |

### Overall Status
- [ ] **ALL TESTS PASSED** - Ready for production
- [ ] **MOST TESTS PASSED** - Minor issues to fix
- [ ] **SIGNIFICANT FAILURES** - Major rework needed
- [ ] **BLOCKED** - Cannot proceed with testing

### Blocking Issues
```
Issue 1:

Issue 2:

Issue 3:
```

### Recommendations
```
Recommendation 1:

Recommendation 2:

Recommendation 3:
```

---

## Sign-Off

**Tester Name:** _______________________
**Date:** _______________________
**Signature:** _______________________

**Approved for Release:**
- [ ] Yes - All critical tests passed
- [ ] No - Issues must be resolved
- [ ] Conditional - With noted limitations

**Comments:**
```




```

---

## Appendix: Debugging Tips

### Enable Verbose Logging
```bash
export WEBCONTENT_DEBUG=1
export REQUESTSERVER_DEBUG=1
export SENTINEL_DEBUG=1
./Build/release/bin/Ladybird
```

### Check Logs
- **RequestServer:** Look for "TrafficMonitor:" prefix
- **WebContent:** Look for "SecurityAlertHandler:" prefix
- **PolicyGraph:** Check `~/.config/ladybird/policy_graph.db` with sqlite3

### Verify Database
```bash
sqlite3 ~/.config/ladybird/policy_graph.db
> SELECT * FROM network_behavior_policies;
> SELECT * FROM threat_history;
> .exit
```

### Monitor Resource Usage
```bash
# Memory usage
ps aux | grep -E '(Ladybird|RequestServer|WebContent)'

# CPU usage
top -p $(pgrep -f RequestServer)

# Network traffic
sudo tcpdump -i any -n host example.com
```

### Reset Test Environment
```bash
# Clear PolicyGraph database
rm ~/.config/ladybird/policy_graph.db

# Clear browser cache
rm -rf ~/.config/ladybird/cache/

# Restart with clean slate
./Build/release/bin/Ladybird
```

---

**Document Version:** 1.0
**Last Updated:** 2025-11-01
**Milestone:** 0.4 Phase 6
