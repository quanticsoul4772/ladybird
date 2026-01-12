# Testing Canvas Fingerprinting Detection

**Date**: 2025-10-31
**Milestone**: 0.4 Phase 4 - Browser Fingerprinting Detection
**Feature**: Canvas API Fingerprinting Detection

---

## Quick Start

### 1. Build Complete (Already Done!)
```bash
# Ladybird browser has been built successfully
✅ Build/release/bin/Ladybird
✅ Build/release/libexec/WebContent
```

### 2. Run Ladybird Browser

**Option A: Run from terminal (see console output)**
```bash
cd /home/rbsmith4/ladybird
./Build/release/bin/Ladybird
```

**Option B: Run with debug output**
```bash
cd /home/rbsmith4/ladybird
WEBCONTENT_DEBUG=1 ./Build/release/bin/Ladybird
```

**Option C: Run from build script**
```bash
./Meta/ladybird.py run
```

### 3. Load Test Page

Once Ladybird opens:
1. Navigate to: `file:///home/rbsmith4/ladybird/test_canvas_fingerprinting.html`
2. Or type in address bar: `/home/rbsmith4/ladybird/test_canvas_fingerprinting.html`

---

## Test Scenarios

### Test 1: Single Canvas Call (Should NOT Alert)
**Purpose**: Verify that normal Canvas usage doesn't trigger alerts

**Steps**:
1. Click "Run Test 1" button
2. Check terminal running Ladybird/WebContent

**Expected Behavior**:
- ✅ Canvas renders successfully
- ✅ Data URL displayed
- ❌ **NO aggressive fingerprinting alert** (score ~0.7, below 0.75 threshold)

**Why**: Single `toDataURL()` call has base score of 0.7, but we need score > 0.75 to avoid false positives on legitimate canvas usage.

---

### Test 2: Multiple toDataURL() Calls (Should Alert)
**Purpose**: Detect repeated canvas data extraction (fingerprinting pattern)

**Steps**:
1. Click "Run Test 2" button
2. Watch terminal for alert

**Expected Console Output**:
```
⚠️ Aggressive fingerprinting detected! Score: 0.80, Confidence: 0.15
    Techniques: 1 (canvas=true, webgl=false, audio=false, navigator=false, fonts=false)
    Canvas calls: 3
    Explanation: Canvas fingerprinting (3 calls); No user interaction before fingerprinting
```

**Why**: 3 `toDataURL()` calls = base score 0.7 + 0.2 (additional calls) = 0.9, with 1.2x multiplier for no user interaction = **score > 0.75** (aggressive threshold)

---

### Test 3: Advanced Fingerprinting (Should DEFINITELY Alert)
**Purpose**: Detect aggressive fingerprinting combining toDataURL + getImageData

**Steps**:
1. Click "Run Test 3" button
2. Watch terminal for alert

**Expected Console Output**:
```
⚠️ Aggressive fingerprinting detected! Score: 1.00, Confidence: 0.20
    Techniques: 1 (canvas=true, webgl=false, audio=false, navigator=false, fonts=false)
    Canvas calls: 5
    Explanation: Canvas fingerprinting (5 calls); No user interaction before fingerprinting
```

**Why**: 2 `toDataURL()` + 1 `getImageData()` = multiple data extraction methods = **maximum aggressiveness score**

---

## How to Verify Detection

### 1. Terminal Output
The WebContent process will print alerts to the terminal:

```bash
# When you run Ladybird, watch for:
Fingerprinting detector initialized successfully

# After clicking test buttons:
⚠️ Aggressive fingerprinting detected! Score: X.XX, Confidence: X.XX
    Techniques: 1 (canvas=true, webgl=false, audio=false, navigator=false, fonts=false)
    Explanation: Canvas fingerprinting (N calls); No user interaction before fingerprinting
```

### 2. No Alert for Test 1
Test 1 should NOT show the alert because:
- Single call = score 0.7
- No multipliers active (only 1 call)
- 0.7 > 0.6 threshold? **NO** (we need additional indicators)

### 3. Alerts for Tests 2 & 3
Tests 2 and 3 should show alerts because:
- Multiple calls = higher base score
- No user interaction multiplier = 1.2x
- Final score > 0.6 = **ALERT!**

---

## Testing with Real Fingerprinting Sites

You can also test with actual fingerprinting detection sites:

### Option 1: BrowserLeaks Canvas Test
```bash
# In Ladybird address bar:
https://browserleaks.com/canvas
```

**Expected**: Should detect canvas fingerprinting when the site reads canvas data.

### Option 2: AmIUnique Fingerprinting
```bash
# In Ladybird address bar:
https://amiunique.org/fingerprint
```

**Expected**: Should detect multiple fingerprinting techniques (canvas, fonts, navigator).

### Option 3: EFF Cover Your Tracks
```bash
# In Ladybird address bar:
https://coveryourtracks.eff.org/
```

**Expected**: Should detect extensive fingerprinting attempts.

---

## Understanding the Scoring System

### Base Scores (per technique):
- **Canvas**: 0.7 for first call, +0.1 per additional call (max 1.0)
- **WebGL**: 0.6 for first call, +0.1 per additional call (max 1.0)
- **Audio**: 0.8 for first call, +0.05 per additional call (max 1.0)

### Multipliers:
- **Multiple Techniques** (≥3): 1.5x
- **Rapid Fire** (5+ calls in <1s): 1.3x
- **No User Interaction** (>5 calls before input): 1.2x

### Aggressive Threshold:
- Score > **0.75** triggers alert (raised from 0.6 to avoid false positives on single canvas calls)
- Higher confidence = more API calls detected

---

## Debugging

### Enable Verbose Logging
```bash
# Run with debug flags
WEBCONTENT_DEBUG=1 LIBWEB_DEBUG=1 ./Build/release/bin/Ladybird

# Or modify PageClient.cpp line 528 to always log:
// Remove the is_aggressive_fingerprinting() check to see all scores
auto score = m_fingerprinting_detector->calculate_score();
dbgln("Fingerprinting score: {:.2f} (confidence: {:.2f})",
    score.aggressiveness_score, score.confidence);
```

### Check Detector Initialization
Look for this line when Ladybird starts:
```
Fingerprinting detector initialized successfully
```

If you see:
```
Warning: Failed to create FingerprintingDetector: [error]
Fingerprinting detection will be disabled for this page
```

Then check that FingerprintingDetector.cpp compiled correctly.

---

## Troubleshooting

### Problem: No alerts appearing

**Solution 1**: Check that WebContent is using the updated build
```bash
# Kill any old WebContent processes
pkill -f WebContent

# Run Ladybird again
./Build/release/bin/Ladybird
```

**Solution 2**: Verify detection is enabled
```bash
# Check if detector was initialized
grep "Fingerprinting detector" /tmp/webcontent.log
```

**Solution 3**: Lower the aggressive threshold (for testing)
```cpp
// In FingerprintingDetector.cpp:166, temporarily change:
return score.aggressiveness_score > 0.75f;
// To:
return score.aggressiveness_score > 0.3f;  // Lower threshold for testing
```

### Problem: Ladybird crashes

**Solution**: Check for null pointer issues
```bash
# Run with gdb
gdb ./Build/release/bin/Ladybird
run
# When it crashes: bt (backtrace)
```

---

## Expected Test Results Summary

| Test | Canvas Calls | Expected Score | Alert? | Reason |
|------|-------------|----------------|--------|--------|
| Test 1 | 1 toDataURL | ~0.70 | ❌ NO | Below 0.75 threshold (normal usage) |
| Test 2 | 3 toDataURL | ~0.90 | ✅ YES | Above 0.75 threshold (multiple calls) |
| Test 3 | 2 toDataURL + 1 getImageData | ~1.00 | ✅ YES | Maximum score (aggressive pattern) |

---

## Next Steps After Testing

Once Canvas detection is verified:

1. **Add WebGL Hooks** - Libraries/LibWeb/WebGL/WebGLRenderingContext.cpp
2. **Add Audio Hooks** - Libraries/LibWeb/WebAudio/AudioContext.cpp
3. **Add Navigator Hooks** - Libraries/LibWeb/HTML/Navigator.cpp
4. **Create IPC Alerts** - Send to UI for user notifications
5. **Add User Controls** - Allow/block/whitelist options

---

## Files Involved in This Test

```
Detection Logic:
- Services/Sentinel/FingerprintingDetector.{h,cpp}
- Services/Sentinel/TestFingerprintingDetector.cpp (unit tests)

Integration:
- Services/WebContent/PageClient.{h,cpp} (detector instance)
- Libraries/LibWeb/Page/Page.h (interface)
- Libraries/LibWeb/HTML/HTMLCanvasElement.cpp (toDataURL, toBlob hooks)
- Libraries/LibWeb/HTML/CanvasRenderingContext2D.cpp (getImageData hook)

Testing:
- test_canvas_fingerprinting.html (this test page)
- TESTING_FINGERPRINTING_DETECTION.md (this guide)
```

---

## Success Criteria

✅ **Test passes if**:
1. Test 1 completes without alert
2. Test 2 triggers alert with score > 0.6
3. Test 3 triggers alert with higher score
4. Console output shows technique breakdown
5. No crashes or errors

---

*Happy testing! If you see the alerts, Canvas fingerprinting detection is working! 🎉*
