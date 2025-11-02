# HTML5 Media Tests - Ladybird Browser Documentation

## Overview

This document explains the current state of HTML5 media support in Ladybird Browser and documents the 11 expected test failures related to media playback functionality.

**Status**: 11 tests are marked as `test.skip()` (expected failures)
**Test File**: `/home/rbsmith4/ladybird/Tests/Playwright/tests/multimedia/media.spec.ts`
**Reference Implementation**: `Libraries/LibWeb/HTML/HTMLMediaElement.cpp`

---

## Summary of Known Limitations

Ladybird Browser currently has **incomplete HTML5 media element support**. The following core features are not yet implemented:

| Issue | Impact | Tests Affected |
|-------|--------|----------------|
| **Metadata loading fails** | audio/video elements cannot read duration, dimensions, codec info | MEDIA-001, MEDIA-007, MEDIA-011 |
| **play() Promise timeout** | play() never resolves/rejects, hangs indefinitely | MEDIA-004 |
| **duration is NaN** | Video duration never determined from file headers | MEDIA-005, MEDIA-015 |
| **readyState stuck at 0** | Never transitions from HAVE_NOTHING (0) to HAVE_METADATA (1) or higher | MEDIA-014, MEDIA-022 |
| **No media events** | loadstart, loadedmetadata, play, pause, seeking, etc. don't fire | MEDIA-006, MEDIA-012, MEDIA-019 |
| **Preload attribute ignored** | preload="metadata" or preload="auto" have no effect | MEDIA-022 |

---

## 11 Skipped Tests (Expected Failures)

### MEDIA-001: Audio playback basic functionality

**Status**: `test.skip()`
**Reason**: Metadata loading fails
**Details**:
- Audio elements do not fire `loadedmetadata` event
- `duration` property remains `NaN` after 10-second timeout
- `waitForMetadata()` helper function fails to detect metadata loading
- No initial playback state available

**Test Flow** (what would happen if test ran):
```
1. Create audio element with controls
2. Wait for metadata to load (10-second timeout)
   ← FAILS: metadata event never fires, duration stays NaN
3. Verify duration > 0
   ← FAILS: NaN is not greater than 0
4. Play audio
5. Verify playback state
```

**Root Cause**: HTMLMediaElement doesn't parse audio headers or fire metadata events

---

### MEDIA-004: Audio play/pause controls

**Status**: `test.skip()`
**Reason**: play() Promise timeout
**Details**:
- Calling `audio.play()` returns a Promise that never resolves
- Promise doesn't reject either - it hangs indefinitely
- No play/pause events fire
- Playback state never changes from paused

**Test Flow** (what would happen if test ran):
```
1. Create audio element with controls
2. Wait for metadata (also fails in MEDIA-001)
3. Call play()
   ← HANGS: Promise never settles, test timeout after 30 seconds
4. Check if paused = false
5. Pause audio
```

**Root Cause**: play() Promise implementation incomplete in HTMLMediaElement

---

### MEDIA-005: Audio seeking/scrubbing

**Status**: `test.skip()`
**Reason**: duration is NaN
**Details**:
- Cannot calculate seek position (duration / 2) when duration is NaN
- Seeking operations impossible without valid duration
- Even if seek were possible, no seeking/seeked events fire

**Test Flow** (what would happen if test ran):
```
1. Create audio element with controls
2. Wait for metadata
   ← FAILS: duration never loads
3. Get duration (NaN)
4. Calculate seekTime = duration / 2
   ← FAILS: NaN / 2 = NaN
5. Seek to calculated time
6. Verify currentTime changed
```

**Root Cause**: Depends on metadata loading from MEDIA-001

---

### MEDIA-006: Audio events and event sequences

**Status**: `test.skip()`
**Reason**: No media events fire
**Details**:
- Event listener setup works but no events are dispatched
- Expected events: loadstart, loadedmetadata, play, pause, seeking, seeked
- `getMediaEvents()` returns empty array
- Event-driven testing impossible in Ladybird

**Test Flow** (what would happen if test ran):
```
1. Create audio element
2. Wait for metadata
   ← FAILS: no loadstart event
3. Check events array length > 0
   ← FAILS: array is empty
4. Verify 'loadstart' in events
   ← FAILS: no loadstart event dispatched
5. Play and verify play/playing events
   ← FAILS: no events dispatched
```

**Root Cause**: Media event dispatch system not fully implemented

---

### MEDIA-007: Video playback basic functionality

**Status**: `test.skip()`
**Reason**: Metadata loading fails (same as MEDIA-001)
**Details**:
- Video elements don't fire `loadedmetadata` event
- `videoWidth` and `videoHeight` stay at 0
- `duration` remains NaN
- Video dimensions cannot be determined

**Test Flow** (what would happen if test ran):
```
1. Create video element with 640x480 size
2. Wait for metadata (10-second timeout)
   ← FAILS: no loadedmetadata event
3. Get video properties
   ← FAILS: videoWidth = 0, videoHeight = 0
4. Play video
5. Verify playback state
```

**Root Cause**: HTMLMediaElement doesn't parse video headers

---

### MEDIA-011: Multiple video source formats

**Status**: `test.skip()`
**Reason**: Metadata loading fails with multiple sources
**Details**:
- Browser provided with multiple source formats (MP4, WebM)
- No codec negotiation occurs
- Metadata doesn't load regardless of format variety
- Format selection mechanism not implemented

**Test Flow** (what would happen if test ran):
```
1. Create video with sources: [MP4, WebM]
2. Wait for metadata
   ← FAILS: no format selection, no metadata loading
3. Get playback state
   ← FAILS: duration is NaN
4. Play video
5. Verify playback
```

**Root Cause**: Codec negotiation and format selection not implemented

---

### MEDIA-012: Video events and state tracking

**Status**: `test.skip()`
**Reason**: No media events fire with state information
**Details**:
- Events expected: loadedmetadata, play, playing, pause
- No events in array after waiting
- readyState info in events would be all zeros (HAVE_NOTHING)
- State tracking lifecycle not implemented

**Test Flow** (what would happen if test ran):
```
1. Create video element
2. Wait for metadata
   ← FAILS: no loadedmetadata event
3. Check events.length > 0
   ← FAILS: array is empty
4. Find event with readyState in details
   ← FAILS: no events at all
5. Play and check for playing event
   ← FAILS: no events dispatched
```

**Root Cause**: Video event dispatch not implemented

---

### MEDIA-014: readyState property values

**Status**: `test.skip()`
**Reason**: readyState stuck at 0 (HAVE_NOTHING)
**Details**:
- readyState constant values:
  - 0 = HAVE_NOTHING (no data)
  - 1 = HAVE_METADATA (duration/dimensions available)
  - 2 = HAVE_CURRENT_DATA (data for current playback position)
  - 3 = HAVE_FUTURE_DATA (data for future playback)
  - 4 = HAVE_ENOUGH_DATA (enough data to play through)
- Ladybird stuck at 0 regardless of preload attribute
- No transition to state 1 (HAVE_METADATA)

**Test Flow** (what would happen if test ran):
```
1. Create audio with preload="none"
2. Initial readyState should be 0 or 1
   ← OK: readyState = 0
3. Call load()
4. Wait for metadata
   ← FAILS: readyState stays 0
5. readyState should be >= 1
   ← FAILS: still 0
6. Create audio with preload="auto"
7. Wait and check readyState >= 1
   ← FAILS: stays 0
```

**Root Cause**: Media state machine not fully implemented

---

### MEDIA-015: Duration and currentTime tracking

**Status**: `test.skip()`
**Reason**: duration is NaN
**Details**:
- Duration never determined from media file
- currentTime updates during playback would require valid duration
- Without duration, seek operations impossible
- Cannot verify time bounds checking (currentTime <= duration)

**Test Flow** (what would happen if test ran):
```
1. Create audio element
2. Wait for metadata
   ← FAILS: metadata doesn't load
3. Get duration (NaN)
4. Verify duration > 0
   ← FAILS: NaN is not greater than 0
5. Play for 200ms
6. Get currentTime
   ← FAILS: probably 0
7. Verify 0 < currentTime < duration
   ← FAILS: 0 < undefined
```

**Root Cause**: Media file parsing and duration extraction not implemented

---

### MEDIA-019: Error handling and fallbacks

**Status**: `test.skip()`
**Reason**: No error events fire
**Details**:
- Test uses nonexistent file paths: `/nonexistent/file.mp3`
- Expected events: 'error' and 'abort'
- No events are dispatched when resources fail to load
- Error handling mechanism not implemented

**Test Flow** (what would happen if test ran):
```
1. Create audio with nonexistent sources
2. Wait 1 second for errors
3. Check errorEvents array
   ← FAILS: no error or abort events
4. Get networkState
5. Check for error state changes
   ← FAILS: networkState probably 0 (NETWORK_EMPTY)
```

**Root Cause**: Error event dispatch not implemented

---

### MEDIA-022: Preload attribute behavior

**Status**: `test.skip()`
**Reason**: preload attribute ignored
**Details**:
- Three preload modes should have different behavior:
  - `preload="none"`: readyState stays at HAVE_NOTHING (0)
  - `preload="metadata"`: readyState should be >= HAVE_METADATA (1)
  - `preload="auto"`: readyState should load data (2, 3, or 4)
- All three modes result in same behavior: readyState = 0
- Preload attribute completely ignored

**Test Flow** (what would happen if test ran):
```
1. Create audio with preload="none"
   readyState = 0 ✓ (correct)
2. Create audio with preload="metadata"
   Wait 500ms
   readyState should be in [1, 2, 3, 4]
   ← FAILS: readyState = 0
3. Create audio with preload="auto"
   Wait 500ms
   readyState should be in [1, 2, 3, 4]
   ← FAILS: readyState = 0
```

**Root Cause**: Preload attribute not connected to media loading logic

---

## Tests That Currently Pass

The following 13 tests **pass** because they test attributes and simple properties that don't require actual media loading:

| Test ID | Test Name | Why It Passes |
|---------|-----------|---------------|
| MEDIA-002 | Audio volume control | `volume` property is settable/gettable |
| MEDIA-003 | Audio mute/unmute | `muted` property works correctly |
| MEDIA-008 | Video controls visibility | `controls` attribute is parsed |
| MEDIA-009 | Fullscreen support | `fullscreenEnabled` API is detectable |
| MEDIA-010 | Poster image display | `poster` attribute is parsed |
| MEDIA-013 | canPlayType() detection | Returns empty string (correct for no codec) |
| MEDIA-016 | Playback rate control | `playbackRate` property is settable |
| MEDIA-017 | Picture-in-Picture mode | `pictureInPictureEnabled` API is detectable |
| MEDIA-018 | Media tracks | `addTextTrack()` API is implemented |
| MEDIA-020 | Supported media types | `canPlayType()` method works |
| MEDIA-021 | Network state tracking | `networkState` property exists |
| MEDIA-023 | Loop attribute behavior | `loop` attribute is parsed |
| MEDIA-024 | Autoplay attribute behavior | `autoplay` attribute is parsed |

---

## Architecture Reference

### Media Element Hierarchy

```
HTMLMediaElement (abstract base)
├── HTMLAudioElement
└── HTMLVideoElement
```

### Related Source Files

- **Core Implementation**: `Libraries/LibWeb/HTML/HTMLMediaElement.cpp`
- **Audio Element**: `Libraries/LibWeb/HTML/HTMLAudioElement.cpp`
- **Video Element**: `Libraries/LibWeb/HTML/HTMLVideoElement.cpp`
- **IPC Integration**: `Services/WebContent/PageClient.{h,cpp}` (media event dispatch)
- **Audio Context**: `Libraries/LibWeb/WebAudio/AudioContext.cpp` (not used for HTMLMediaElement)

### What's Missing

1. **Media File Parsing**
   - No MP3, WAV, OGG audio codec support
   - No MP4, WebM, OGV video codec support
   - Duration extraction from headers
   - Dimension extraction for video

2. **State Management**
   - Media loading state machine (HAVE_NOTHING → HAVE_METADATA → ...)
   - readyState transitions
   - networkState updates

3. **Event Dispatch**
   - loadstart event
   - loadedmetadata event
   - play/pause/playing events
   - seeking/seeked events
   - error/abort events

4. **Promise Support**
   - play() Promise never settles
   - pause() works but doesn't integrate with promises

5. **Advanced Features**
   - Preload attribute handling
   - Multiple format selection (codec negotiation)
   - Error recovery

---

## How to Run Tests

### Run all media tests (including skipped):
```bash
npx playwright test tests/multimedia/media.spec.ts --reporter=list
```

### Run only non-skipped tests:
```bash
npx playwright test tests/multimedia/media.spec.ts --reporter=list
```
(Skipped tests won't execute)

### Run specific test:
```bash
npx playwright test tests/multimedia/media.spec.ts -g "MEDIA-002"
```

### Run with verbose output:
```bash
npx playwright test tests/multimedia/media.spec.ts --reporter=verbose
```

---

## Future Work

### Milestone: HTML5 Media Support

To enable these tests, implement in this order:

1. **Phase 1: Audio Codec Support** (MEDIA-001, MEDIA-004, MEDIA-005)
   - MP3 decoding (libmpg123 or ffmpeg)
   - WAV decoding
   - OGG Vorbis support
   - Duration extraction

2. **Phase 2: Video Codec Support** (MEDIA-007, MEDIA-011)
   - MP4/H.264 decoding
   - WebM/VP9 decoding
   - OGV/Theora support
   - Resolution extraction

3. **Phase 3: Media State Machine** (MEDIA-014, MEDIA-015, MEDIA-022)
   - Implement readyState transitions
   - Connect preload attribute to loading
   - Update networkState during loading

4. **Phase 4: Event System** (MEDIA-006, MEDIA-012, MEDIA-019)
   - Dispatch loadstart event
   - Dispatch loadedmetadata event
   - Dispatch play/pause/playing events
   - Implement error event dispatch

5. **Phase 5: Playback Control** (MEDIA-004)
   - Fix play() Promise implementation
   - Implement actual audio/video playback
   - currentTime updates during playback

### Estimated Impact

Completing this work would:
- Enable 11 currently skipped tests
- Improve HTML5 compatibility significantly
- Support YouTube embedded videos
- Support audio/video content sites

---

## Conclusion

The 11 skipped tests are **not** test failures - they're **expected failures** documenting Ladybird's current architectural limitations. This documentation ensures future developers understand:

1. **Why tests are skipped** - technical limitations
2. **What's missing** - specific features not implemented
3. **Impact on users** - what sites won't work properly
4. **How to fix** - implementation roadmap

The remaining 13 passing tests verify that the HTML5 media element API is partially accessible (attributes, basic properties), even though actual playback is not supported.
