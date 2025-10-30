# Sentinel UI Components Test Checklist
## Phase 5 Days 29-30 - Manual Testing Guide

**Note**: This document provides comprehensive manual testing procedures for Sentinel UI components. Automated UI testing requires Qt Test framework integration which is beyond the scope of Phase 5 Day 29-30.

---

## Test Suite 2: UI Components Testing

### Prerequisites
- Ladybird browser running
- Sentinel daemon running
- Test EICAR file available: `wget https://secure.eicar.org/eicar.com` (or create manually)

---

## 2.1 SecurityNotificationBanner Tests

### Test 2.1.1: Threat Blocked Notification
**Steps**:
1. Download EICAR file from `https://secure.eicar.org/eicar.com`
2. Create policy to block it
3. Download EICAR again from different URL

**Expected Results**:
- ✓ Red notification banner appears at top of window
- ✓ Message: "Threat blocked: EICAR_Test_File"
- ✓ "View Policy" link present
- ✓ Auto-dismisses after 5 seconds
- ✓ Can manually close with X button

**Pass/Fail**: _____

---

### Test 2.1.2: File Quarantined Notification
**Steps**:
1. Download suspicious file
2. Create policy to quarantine it
3. Download same file again

**Expected Results**:
- ✓ Yellow/orange notification banner appears
- ✓ Message: "File quarantined: [filename]"
- ✓ "View Quarantine" link present
- ✓ Auto-dismisses after 5 seconds

**Pass/Fail**: _____

---

### Test 2.1.3: File Allowed Notification
**Steps**:
1. Download file that triggers YARA rule
2. Create policy to allow it
3. Download same file again

**Expected Results**:
- ✓ Green notification banner appears
- ✓ Message: "Download allowed: [filename]"
- ✓ Auto-dismisses after 5 seconds

**Pass/Fail**: _____

---

### Test 2.1.4: Notification Queue Behavior
**Steps**:
1. Rapidly download 5 different threat files
2. Create policies for all of them
3. Download all 5 again quickly

**Expected Results**:
- ✓ Notifications appear one at a time (not stacked)
- ✓ Each notification displays for 5 seconds
- ✓ Queue processes in order (FIFO)
- ✓ Maximum 10 notifications queued (older ones dropped)

**Pass/Fail**: _____

---

### Test 2.1.5: View Policy Navigation
**Steps**:
1. Trigger threat notification with "View Policy" link
2. Click "View Policy" link

**Expected Results**:
- ✓ Browser navigates to `about:security`
- ✓ Security page loads successfully
- ✓ Relevant policy is highlighted/scrolled to

**Pass/Fail**: _____

---

## 2.2 QuarantineManagerDialog Tests

### Test 2.2.1: List Quarantined Files
**Steps**:
1. Quarantine 3 different files
2. Open Quarantine Manager (from menu or about:security)

**Expected Results**:
- ✓ Dialog opens with title "Quarantine Manager"
- ✓ All 3 quarantined files listed
- ✓ Each entry shows: filename, original URL, detection time, rules matched
- ✓ File hashes displayed
- ✓ File sizes shown correctly

**Pass/Fail**: _____

---

### Test 2.2.2: Restore File Functionality
**Steps**:
1. Select quarantined file in list
2. Click "Restore" button
3. Choose destination directory

**Expected Results**:
- ✓ File restore confirmation dialog appears
- ✓ Warning about restoring potentially malicious file
- ✓ After confirm, file copied to destination
- ✓ Original quarantine file remains (safe restore)
- ✓ Success notification shown

**Pass/Fail**: _____

---

### Test 2.2.3: Delete File Functionality
**Steps**:
1. Select quarantined file in list
2. Click "Delete" button
3. Confirm deletion

**Expected Results**:
- ✓ Deletion confirmation dialog appears
- ✓ Warning: "This action cannot be undone"
- ✓ After confirm, file removed from quarantine
- ✓ Entry removed from list
- ✓ Metadata JSON also deleted

**Pass/Fail**: _____

---

### Test 2.2.4: Export to CSV
**Steps**:
1. Quarantine multiple files (at least 5)
2. Click "Export to CSV" button
3. Choose save location

**Expected Results**:
- ✓ CSV file save dialog appears
- ✓ CSV contains all quarantined files
- ✓ Columns: Filename, Original URL, Detection Time, Rules, Hash, Size
- ✓ CSV properly formatted (comma-separated, quoted strings)
- ✓ Can open in spreadsheet software

**Pass/Fail**: _____

---

### Test 2.2.5: Search and Filter
**Steps**:
1. Quarantine files with different filenames and rules
2. Enter search term in search box
3. Test filter by severity dropdown

**Expected Results**:
- ✓ Search filters list as you type
- ✓ Case-insensitive search
- ✓ Searches filename, URL, and rule name
- ✓ Severity filter (All, Critical, High, Medium, Low)
- ✓ Can combine search and filter

**Pass/Fail**: _____

---

## 2.3 about:security Page Tests

### Test 2.3.1: Page Load and Statistics
**Steps**:
1. Navigate to `about:security` in browser
2. Observe initial page load

**Expected Results**:
- ✓ Page loads within 200ms
- ✓ Header shows "Security Center" with icon
- ✓ Statistics card displays:
  - Total threats detected
  - Policies active
  - Files quarantined
  - Last scan time
- ✓ Numbers accurate and match database

**Pass/Fail**: _____

---

### Test 2.3.2: Real-Time Status Updates
**Steps**:
1. Keep about:security page open
2. Download and quarantine a file in another tab
3. Observe statistics card

**Expected Results**:
- ✓ Statistics update within 2 seconds
- ✓ "Threats detected" count increments
- ✓ "Files quarantined" count increments
- ✓ "Last scan time" updates to current time
- ✓ No page reload required

**Pass/Fail**: _____

---

### Test 2.3.3: Policy Template Creation
**Steps**:
1. Scroll to "Policy Templates" section
2. Click "Create Template" button
3. Fill in template details:
   - Template name: "Block Executables from Malicious Domain"
   - URL pattern: "%malicious-site.com%"
   - File type: "application/x-executable"
   - Action: Block
4. Click "Save Template"

**Expected Results**:
- ✓ Template creation form appears
- ✓ All fields editable with validation
- ✓ URL pattern supports SQL LIKE syntax (% wildcard)
- ✓ Action dropdown: Block, Quarantine, Allow
- ✓ Template saved successfully
- ✓ Appears in templates list

**Pass/Fail**: _____

---

### Test 2.3.4: Apply Policy Template
**Steps**:
1. Create or select existing template
2. Click "Apply Template" button
3. Confirm application

**Expected Results**:
- ✓ Confirmation dialog shows template details
- ✓ After confirm, policy created in database
- ✓ Policy appears in "Active Policies" section
- ✓ Template preview shows before/after

**Pass/Fail**: _____

---

### Test 2.3.5: Policy Management (Edit)
**Steps**:
1. Scroll to "Active Policies" section
2. Click "Edit" on existing policy
3. Change action from Block to Quarantine
4. Save changes

**Expected Results**:
- ✓ Edit form pre-populated with current values
- ✓ Can modify all fields except rule name and hash
- ✓ Changes saved to database
- ✓ Policy list updates immediately
- ✓ Success notification shown

**Pass/Fail**: _____

---

### Test 2.3.6: Policy Management (Delete)
**Steps**:
1. Select policy to delete
2. Click "Delete" button
3. Confirm deletion

**Expected Results**:
- ✓ Deletion confirmation dialog
- ✓ Warning: "Future threats will show alert dialog"
- ✓ After confirm, policy removed from database
- ✓ Removed from list immediately
- ✓ Threat history preserved

**Pass/Fail**: _____

---

### Test 2.3.7: Threat History Display
**Steps**:
1. Scroll to "Recent Threats" section
2. Observe threat list

**Expected Results**:
- ✓ Shows last 50 threats (most recent first)
- ✓ Each entry shows:
  - Timestamp
  - Filename
  - Original URL
  - Rule(s) matched
  - Action taken (blocked/quarantined/allowed)
  - Severity badge (color-coded)
- ✓ "View Details" link for each threat
- ✓ "Export History" button present

**Pass/Fail**: _____

---

### Test 2.3.8: Network Audit Log
**Steps**:
1. Scroll to "Network Audit Log" section
2. Download several files
3. Observe log updates

**Expected Results**:
- ✓ Shows all download attempts (threats and clean)
- ✓ Real-time updates (no refresh needed)
- ✓ Columns: Time, URL, Filename, Scan Result, Action
- ✓ Color coding: Red (blocked), Yellow (quarantined), Green (allowed)
- ✓ Pagination for large logs (50 entries per page)

**Pass/Fail**: _____

---

## Performance Tests

### Test 2.4.1: Page Load Performance
**Steps**:
1. Clear browser cache
2. Open Chrome DevTools (F12)
3. Navigate to `about:security`
4. Measure page load time

**Expected Results**:
- ✓ Initial HTML load: < 50ms
- ✓ JavaScript execution: < 100ms
- ✓ Total page load: < 200ms
- ✓ No JavaScript errors in console

**Measured Time**: _____ ms
**Pass/Fail**: _____

---

### Test 2.4.2: Table Rendering Performance
**Steps**:
1. Create 100+ threat history records
2. Navigate to about:security
3. Scroll through threat history table

**Expected Results**:
- ✓ Table renders without lag
- ✓ Smooth scrolling
- ✓ No frame drops (maintain 60fps)
- ✓ Pagination loads quickly (< 100ms)

**Pass/Fail**: _____

---

### Test 2.4.3: Real-Time Update Performance
**Steps**:
1. Keep about:security open
2. Download 10 files in quick succession
3. Observe update lag

**Expected Results**:
- ✓ Statistics update within 2 seconds
- ✓ UI remains responsive
- ✓ No flickering or visual glitches
- ✓ Smooth animations

**Pass/Fail**: _____

---

## Edge Cases and Error Handling

### Test 2.5.1: Empty States
**Steps**:
1. Fresh Sentinel installation (no data)
2. Navigate to about:security

**Expected Results**:
- ✓ "No threats detected" message in history
- ✓ "No policies active" message in policy list
- ✓ "No quarantined files" in quarantine section
- ✓ Statistics show zeros
- ✓ Empty state graphics/illustrations shown

**Pass/Fail**: _____

---

### Test 2.5.2: Database Unavailable
**Steps**:
1. Stop Sentinel daemon
2. Navigate to about:security

**Expected Results**:
- ✓ Error message: "Cannot connect to Security Service"
- ✓ Fallback UI with limited functionality
- ✓ Option to retry connection
- ✓ No crashes or hangs

**Pass/Fail**: _____

---

### Test 2.5.3: Invalid Policy Template
**Steps**:
1. Create template with invalid URL pattern (e.g., "[invalid")
2. Try to save

**Expected Results**:
- ✓ Validation error shown
- ✓ Highlights invalid field
- ✓ Helpful error message
- ✓ Cannot save until fixed

**Pass/Fail**: _____

---

### Test 2.5.4: Large Threat History
**Steps**:
1. Generate 1000+ threat records
2. Navigate to about:security
3. Test pagination and search

**Expected Results**:
- ✓ Page still loads quickly (< 500ms)
- ✓ Pagination works correctly
- ✓ Search performs well (< 100ms)
- ✓ No memory leaks

**Pass/Fail**: _____

---

## Accessibility Tests

### Test 2.6.1: Keyboard Navigation
**Steps**:
1. Navigate about:security using only keyboard
2. Tab through all interactive elements

**Expected Results**:
- ✓ Can reach all buttons and links with Tab
- ✓ Focus indicators visible
- ✓ Enter activates buttons
- ✓ Escape closes dialogs

**Pass/Fail**: _____

---

### Test 2.6.2: Screen Reader Compatibility
**Steps**:
1. Enable screen reader (NVDA/JAWS)
2. Navigate about:security page

**Expected Results**:
- ✓ All text content read correctly
- ✓ Button labels announced
- ✓ Table data accessible
- ✓ ARIA labels present where needed

**Pass/Fail**: _____

---

## Visual/Styling Tests

### Test 2.7.1: Light Mode
**Steps**:
1. Set system theme to light mode
2. View all Sentinel UI components

**Expected Results**:
- ✓ All text readable (sufficient contrast)
- ✓ Colors appropriate for light background
- ✓ No visual glitches

**Pass/Fail**: _____

---

### Test 2.7.2: Dark Mode
**Steps**:
1. Set system theme to dark mode
2. View all Sentinel UI components

**Expected Results**:
- ✓ All text readable (sufficient contrast)
- ✓ Colors appropriate for dark background
- ✓ Consistent with Ladybird dark theme

**Pass/Fail**: _____

---

### Test 2.7.3: Responsive Design
**Steps**:
1. Resize browser window to various widths
2. Test at: 1920px, 1280px, 800px

**Expected Results**:
- ✓ Layout adapts to window size
- ✓ No horizontal scrolling
- ✓ Cards stack vertically on narrow windows
- ✓ All content accessible

**Pass/Fail**: _____

---

## Integration Tests

### Test 2.8.1: Notification to Security Page Flow
**Steps**:
1. Trigger threat notification
2. Click "View Policy" in notification
3. Interact with security page
4. Make policy changes

**Expected Results**:
- ✓ Smooth transition to about:security
- ✓ Relevant section scrolled into view
- ✓ Policy edits reflected in real-time
- ✓ Can navigate back to previous page

**Pass/Fail**: _____

---

### Test 2.8.2: Quarantine Manager Integration
**Steps**:
1. Quarantine file from download
2. Open Quarantine Manager from security page
3. Restore file
4. Verify file restored to correct location

**Expected Results**:
- ✓ File appears in quarantine list immediately
- ✓ Restore operation succeeds
- ✓ File hash verified after restore
- ✓ Audit log updated

**Pass/Fail**: _____

---

## Summary

**Total Tests**: 38
**Tests Passed**: _____
**Tests Failed**: _____
**Tests Skipped**: _____

**Critical Issues Found**: _____
**Major Issues Found**: _____
**Minor Issues Found**: _____

**Overall Status**: ☐ PASS  ☐ FAIL  ☐ PARTIAL

---

## Notes

_[Add any additional observations, bugs found, or recommendations here]_

---

**Tester**: _____________________
**Date**: _____________________
**Ladybird Version**: _____________________
**Sentinel Version**: Phase 5 Day 29-30
