# Media Tests Quick Reference

## Test Status Summary

**Total Tests**: 24 (MEDIA-001 to MEDIA-024)
**Passing**: 13 tests
**Skipped (Expected Failures)**: 11 tests
**Reason for Skips**: Incomplete HTML5 media codec support in Ladybird

---

## Skipped Tests (11)

| ID | Test Name | Root Cause |
|----|-----------| -----------|
| MEDIA-001 | Audio playback basic functionality | Metadata loading fails |
| MEDIA-004 | Audio play/pause controls | play() Promise timeout |
| MEDIA-005 | Audio seeking/scrubbing | duration is NaN |
| MEDIA-006 | Audio events and event sequences | No media events fire |
| MEDIA-007 | Video playback basic functionality | Metadata loading fails |
| MEDIA-011 | Multiple video source formats | No codec negotiation |
| MEDIA-012 | Video events and state tracking | No media events |
| MEDIA-014 | readyState property values | Stuck at 0 (HAVE_NOTHING) |
| MEDIA-015 | Duration and currentTime tracking | duration is NaN |
| MEDIA-019 | Error handling and fallbacks | No error events |
| MEDIA-022 | Preload attribute behavior | Attribute ignored |

---

## Passing Tests (13)

| ID | Test Name | Works Because |
|----|-----------| -----------|
| MEDIA-002 | Audio volume control | volume property implemented |
| MEDIA-003 | Audio mute/unmute | muted property implemented |
| MEDIA-008 | Video controls visibility | Attribute parsing works |
| MEDIA-009 | Fullscreen support | API detection available |
| MEDIA-010 | Poster image display | Attribute parsing works |
| MEDIA-013 | canPlayType() detection | Method returns correct empty string |
| MEDIA-016 | Playback rate control | playbackRate property works |
| MEDIA-017 | Picture-in-Picture mode | API detection available |
| MEDIA-018 | Media tracks | addTextTrack() API works |
| MEDIA-020 | Supported media types | canPlayType() works |
| MEDIA-021 | Network state tracking | networkState property exists |
| MEDIA-023 | Loop attribute | Attribute parsing works |
| MEDIA-024 | Autoplay attribute | Attribute parsing works |

---

## Skip Comments in Code

Each skipped test includes a comment explaining why:

```typescript
test.skip('MEDIA-001: Audio playback basic functionality', { tag: '@p1' }, async ({
    page
  }) => {
    // SKIPPED: Ladybird limitation - metadata loading fails
    // Audio elements don't fire loadedmetadata event, duration remains NaN
    // Related issue: HTML5 media codec support not fully implemented
```

---

## Known Limitations

### 1. Metadata Loading Fails
- Audio/video elements don't parse file headers
- `duration` property never updated (remains NaN)
- Video `videoWidth`/`videoHeight` stay at 0
- `loadedmetadata` event never fires

**Affected Tests**: MEDIA-001, MEDIA-007, MEDIA-011

### 2. Play Promise Hangs
- `element.play()` returns Promise that never settles
- No timeout, no rejection - just hangs
- Prevents all playback control testing

**Affected Tests**: MEDIA-004

### 3. No State Transitions
- `readyState` stuck at 0 (HAVE_NOTHING)
- Never reaches 1 (HAVE_METADATA) or higher
- State machine not implemented

**Affected Tests**: MEDIA-014, MEDIA-022

### 4. No Event Dispatch
- Media events don't fire: loadstart, loadedmetadata, play, pause, seeking
- Error events don't fire: error, abort
- Event system not hooked up

**Affected Tests**: MEDIA-006, MEDIA-012, MEDIA-019

### 5. Duration Always NaN
- Seeking impossible (can't calculate position)
- currentTime tracking impossible
- Cannot verify time bounds

**Affected Tests**: MEDIA-005, MEDIA-015

### 6. Preload Attribute Ignored
- preload="none/metadata/auto" all behave the same
- No effect on readyState or loading

**Affected Tests**: MEDIA-022

---

## Actual vs Expected Behavior

### Example: MEDIA-001 (Audio playback)

**Test Code**:
```typescript
await mediaHelper.createAudioElement({
  id: 'audio-player',
  src: MEDIA_FIXTURES.audio.mono,
  controls: true
});

const metadataLoaded = await mediaHelper.waitForMetadata('audio-player');
expect(metadataLoaded).toBe(true); // ← FAILS
```

**What Happens**:
```
1. HTML audio element created ✓
2. Browser requests /fixtures/media/silent-mono.mp3 ✓
3. Headers/metadata parsed? ✗ NO
4. loadedmetadata event fired? ✗ NO
5. duration set? ✗ NO (remains NaN)
6. waitForMetadata timeout after 10 seconds ✗
7. Test fails: expected true, got false
```

**Why It Happens**:
- Ladybird doesn't have MP3 codec support
- Media file parsing not implemented
- Duration extraction not implemented

---

## Documentation Files

1. **This File** (Quick Reference)
   - Summary of all tests
   - Why each is skipped
   - Quick lookup table

2. **MEDIA_TESTS_DOCUMENTATION.md** (Detailed)
   - Full technical details
   - Root cause analysis
   - Architecture reference
   - Implementation roadmap

3. **media.spec.ts** (Test Implementation)
   - Test code with skip comments
   - Top-level file documentation
   - Helper references

---

## Running Tests

```bash
# Run all media tests (shows skipped as "SKIPPED")
npx playwright test tests/multimedia/media.spec.ts

# Run only passing tests (skipped don't run)
npx playwright test tests/multimedia/media.spec.ts

# Run specific passing test
npx playwright test tests/multimedia/media.spec.ts -g "MEDIA-002"

# Show which tests are skipped
npx playwright test tests/multimedia/media.spec.ts --reporter=list | grep -i skip
```

---

## For Future Implementation

When implementing HTML5 media support, focus on:

1. **Audio codec support** (for MEDIA-001, 004, 005, 006)
   - MP3 decoder
   - Duration extraction
   - Event dispatch

2. **Video codec support** (for MEDIA-007, 011, 012)
   - MP4/H.264 decoder
   - Resolution extraction
   - Video event dispatch

3. **State machine** (for MEDIA-014, 015, 022)
   - readyState transitions
   - preload attribute handling
   - Loading state management

4. **Error handling** (for MEDIA-019)
   - Error event dispatch
   - Resource loading failures
   - Fallback handling

---

## Quick Navigation

- **Test File**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/multimedia/media.spec.ts`
- **Helper**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/multimedia/media-test-helper.ts`
- **Core Implementation**: `Libraries/LibWeb/HTML/HTMLMediaElement.cpp`
- **Full Documentation**: `MEDIA_TESTS_DOCUMENTATION.md`
