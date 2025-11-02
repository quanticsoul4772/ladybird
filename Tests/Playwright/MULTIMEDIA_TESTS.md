# Multimedia Tests for Ladybird Browser

## Overview

This document provides comprehensive documentation for the Ladybird multimedia test suite covering HTML5 audio and video elements, Media APIs, and advanced media features.

**Total Tests**: 24 comprehensive test cases
**Coverage**: Audio elements, Video elements, Media APIs, Advanced features
**Location**: `/Tests/Playwright/tests/multimedia/`

## Test Categories

### 1. Audio Element Tests (MEDIA-001 to MEDIA-006)

#### MEDIA-001: Audio Playback Basic Functionality
- **Priority**: P1 (High)
- **Description**: Verifies basic audio element creation, loading, and playback control
- **Test Coverage**:
  - Audio element visibility and attributes
  - Initial playback state (paused)
  - Metadata loading (duration detection)
  - Play/pause state transitions
- **Expected Behavior**:
  - Audio loads without errors
  - Duration is correctly loaded
  - Play/pause transitions work correctly
- **Files Used**: `silent-mono.mp3`

```typescript
// Example: Verify audio playback
const state = await mediaHelper.getPlaybackState('audio-player');
expect(state.paused).toBe(true);  // Initially paused
await mediaHelper.play('audio-player');
expect(state.paused).toBe(false); // After play
```

#### MEDIA-002: Audio Volume Control
- **Priority**: P1
- **Description**: Tests audio volume adjustment from 0% to 100%
- **Test Coverage**:
  - Default volume level (1.0)
  - Volume adjustment (0.0 - 1.0)
  - Volume clamping (>1.0 clamped to 1.0)
  - Volume change events
- **Expected Behavior**:
  - Volume property reflects set values
  - Values outside 0-1 range are clamped
  - volumechange event fires on volume change
- **Files Used**: `silent-mono.mp3`

#### MEDIA-003: Audio Mute/Unmute Functionality
- **Priority**: P1
- **Description**: Tests audio mute/unmute state and muted attribute
- **Test Coverage**:
  - Muted state toggling
  - Muted attribute initialization
  - Mute/unmute events
  - Interaction with volume control
- **Expected Behavior**:
  - Muted property reflects mute state
  - Muted attribute works in HTML
  - Events fire on mute state change
- **Files Used**: `silent-stereo.mp3`, `silent-mono.mp3`

#### MEDIA-004: Audio Play/Pause Controls
- **Priority**: P1
- **Description**: Tests play/pause functionality and state transitions
- **Test Coverage**:
  - Initial paused state
  - Play command execution
  - Pause command execution
  - Multiple play/pause cycles
  - Event tracking
- **Expected Behavior**:
  - Paused property reflects current state
  - Play/pause commands work repeatedly
  - Events indicate state changes
- **Files Used**: `silent-mono.mp3`

#### MEDIA-005: Audio Seeking/Scrubbing
- **Priority**: P1
- **Description**: Tests audio seeking to different timeline positions
- **Test Coverage**:
  - Seek to beginning
  - Seek to middle
  - Seek to end
  - Seek with negative/invalid times
  - Seeking events
- **Expected Behavior**:
  - currentTime reflects seek position
  - Seeking stays within duration bounds
  - Seeking events (seeking, seeked) fire
- **Files Used**: `silent-mono.mp3`

#### MEDIA-006: Audio Events and Event Sequences
- **Priority**: P1
- **Description**: Tests media event firing and proper event sequencing
- **Test Coverage**:
  - Event detection (loadstart, loadedmetadata, etc.)
  - Event ordering
  - State information in events
  - Multiple event cycles
- **Expected Behavior**:
  - Events fire in proper order
  - Event objects contain valid state
  - Events correlate with user actions
- **Files Used**: `silent-mono.mp3`

### 2. Video Element Tests (MEDIA-007 to MEDIA-012)

#### MEDIA-007: Video Playback Basic Functionality
- **Priority**: P1
- **Description**: Verifies basic video element creation and playback
- **Test Coverage**:
  - Video element visibility
  - Video dimension detection (videoWidth, videoHeight)
  - Metadata loading
  - Play/pause control
- **Expected Behavior**:
  - Video loads and renders
  - Dimensions are correctly detected
  - Playback control works
- **Files Used**: `test-video.mp4` (320x240px)

#### MEDIA-008: Video Controls Visibility
- **Priority**: P1
- **Description**: Tests controls attribute and control UI availability
- **Test Coverage**:
  - Controls attribute presence
  - Controls visibility with attribute
  - No controls without attribute
  - Controls attribute modification
- **Expected Behavior**:
  - Controls attribute correctly set
  - Controls UI appears when attribute present
  - Controls UI absent when attribute absent
- **Files Used**: `test-video.mp4`

#### MEDIA-009: Fullscreen Support
- **Priority**: P1
- **Description**: Tests fullscreen API availability and requests
- **Test Coverage**:
  - Fullscreen support detection
  - Fullscreen API availability
  - Fullscreen request handling
  - Browser permission handling
- **Expected Behavior**:
  - Fullscreen support is detectable
  - Fullscreen API methods exist
  - Requests don't cause errors
- **Files Used**: `test-video.mp4`

#### MEDIA-010: Poster Image Display
- **Priority**: P1
- **Description**: Tests video poster image display before playback
- **Test Coverage**:
  - Poster attribute setting
  - Poster image loading
  - Poster property access
  - Poster display timing
- **Expected Behavior**:
  - Poster attribute is preserved
  - Poster property is accessible
  - Poster image loads before playback
- **Files Used**: `test-video.mp4`, `poster.png`

#### MEDIA-011: Multiple Video Source Formats
- **Priority**: P1
- **Description**: Tests video source fallback when multiple formats provided
- **Test Coverage**:
  - Multiple source elements
  - Format selection by browser
  - Fallback to next source
  - Metadata loading from selected source
- **Expected Behavior**:
  - Browser selects compatible source
  - Video loads from selected source
  - Fallback occurs if first source unsupported
- **Files Used**: `test-video.mp4`, `test-video.webm`

#### MEDIA-012: Video Events and State Tracking
- **Priority**: P1
- **Description**: Tests video-specific events and state tracking
- **Test Coverage**:
  - Video event firing
  - State information in events
  - Event sequences during playback
  - Duration and time tracking
- **Expected Behavior**:
  - Events fire during video lifecycle
  - Events contain valid state data
  - Event sequences match expected flow
- **Files Used**: `test-video.mp4`

### 3. Media API Tests (MEDIA-013 to MEDIA-016)

#### MEDIA-013: canPlayType() Format Detection
- **Priority**: P1
- **Description**: Tests MIME type support detection
- **Test Coverage**:
  - canPlayType() for supported formats
  - canPlayType() for unsupported formats
  - Return values: "probably", "maybe", ""
  - Audio and video types
- **Expected Behavior**:
  - Returns appropriate support level
  - Supports common formats
  - Returns empty string for unsupported types
- **Files Used**: `silent-mono.mp3`, `test-video.mp4`

**Common MIME Types Tested**:
```
Audio:
  - audio/mpeg (MP3)
  - audio/wav
  - audio/ogg (Vorbis)
  - audio/webm
  - audio/aac

Video:
  - video/mp4
  - video/webm
  - video/ogg (Theora)
  - video/mpeg
```

#### MEDIA-014: readyState Property Values
- **Priority**: P1
- **Description**: Tests readyState transitions during media loading
- **Test Coverage**:
  - HAVE_NOTHING (0): No data loaded
  - HAVE_METADATA (1): Metadata available
  - HAVE_CURRENT_DATA (2): Current frame available
  - HAVE_FUTURE_DATA (3): Future frame available
  - HAVE_ENOUGH_DATA (4): Enough data to play through
- **Expected Behavior**:
  - readyState increases as media loads
  - State descriptions match numeric values
  - Preload attribute affects initial state
- **Files Used**: `silent-mono.mp3`

**readyState Reference**:
```
0: HAVE_NOTHING      - No data loaded
1: HAVE_METADATA     - Duration/dimensions available
2: HAVE_CURRENT_DATA - Current frame available
3: HAVE_FUTURE_DATA  - Next frame available
4: HAVE_ENOUGH_DATA  - Can play through
```

#### MEDIA-015: Duration and currentTime Tracking
- **Priority**: P1
- **Description**: Tests duration detection and playback position tracking
- **Test Coverage**:
  - Duration detection from metadata
  - Initial currentTime (0)
  - currentTime updates during playback
  - currentTime clamping to duration
  - Seeking updates currentTime
- **Expected Behavior**:
  - Duration is positive number
  - currentTime starts at 0
  - currentTime advances during playback
  - currentTime ≤ duration always
- **Files Used**: `silent-mono.mp3`

#### MEDIA-016: Playback Rate Control
- **Priority**: P1
- **Description**: Tests playback speed adjustment
- **Test Coverage**:
  - Standard rates: 0.25, 0.5, 1.0, 1.5, 2.0
  - Rate 0 (pause/slow-mo)
  - Rate > 2.0
  - Rate change events
- **Expected Behavior**:
  - playbackRate property reflects set value
  - Various rates are supported
  - Rate changes affect playback speed
- **Files Used**: `silent-mono.mp3`

### 4. Advanced Features (MEDIA-017 to MEDIA-019)

#### MEDIA-017: Picture-in-Picture Mode
- **Priority**: P1
- **Description**: Tests Picture-in-Picture (PiP) API support
- **Test Coverage**:
  - PiP support detection
  - PiP request
  - PiP window creation
  - PiP state tracking
- **Expected Behavior**:
  - Browser reports PiP support correctly
  - PiP requests don't cause errors
  - PiP mode works when supported
- **Browser Support**: Not all browsers support PiP
- **Files Used**: `test-video.mp4`

#### MEDIA-018: Media Tracks (Captions/Subtitles)
- **Priority**: P1
- **Description**: Tests text track management for subtitles and captions
- **Test Coverage**:
  - Add subtitle tracks
  - Add caption tracks
  - Track properties (kind, label, language)
  - Multiple track management
  - Track cue handling
- **Expected Behavior**:
  - Tracks can be added dynamically
  - Track properties are accessible
  - Multiple tracks coexist
  - Cues are properly stored
- **Files Used**: `test-video.mp4`

#### MEDIA-019: Error Handling and Fallbacks
- **Priority**: P1
- **Description**: Tests error handling for missing/unsupported media
- **Test Coverage**:
  - Missing file error handling
  - Multiple source fallback
  - Error events (error, abort)
  - Network state during errors
  - Fallback text content
- **Expected Behavior**:
  - Error events fire for missing files
  - Multiple sources provide fallback
  - Error state is detectable
  - Fallback text displays appropriately
- **Files Used**: None (intentionally broken)

### 5. Additional Tests (MEDIA-020 to MEDIA-024)

#### MEDIA-020: Supported Media Types Detection
- **Priority**: P1
- Tests detection of browser's supported audio/video formats
- Useful for feature detection and graceful degradation

#### MEDIA-021: Network State Tracking
- **Priority**: P1
- Tests networkState property transitions:
  - 0: NETWORK_EMPTY
  - 1: NETWORK_IDLE
  - 2: NETWORK_LOADING
  - 3: NETWORK_NO_SOURCE

#### MEDIA-022: Preload Attribute Behavior
- **Priority**: P1
- Tests preload attribute effects:
  - "none": Don't preload
  - "metadata": Load metadata only
  - "auto": Preload as much as possible

#### MEDIA-023: Loop Attribute Behavior
- **Priority**: P1
- Tests loop attribute causing media to restart at end

#### MEDIA-024: Autoplay Attribute Behavior
- **Priority**: P1
- Tests autoplay attribute initiating playback on load

## Test Fixtures

### Audio Files
Located in `fixtures/media/`:

| File | Format | Duration | Size | Description |
|------|--------|----------|------|-------------|
| silent-mono.mp3 | MP3 | ~1 sec | 4.3 KB | Silent mono audio for quick tests |
| silent-stereo.mp3 | MP3 | ~1 sec | 4.4 KB | Silent stereo audio for channel tests |
| silent.wav | WAV | ~1 sec | 87 KB | Uncompressed silent audio |
| silent.ogg | Ogg Vorbis | ~1 sec | 3.4 KB | Compressed Vorbis audio |

### Video Files
| File | Format | Resolution | Duration | Size | Description |
|------|--------|-----------|----------|------|-------------|
| test-video.mp4 | H.264/MP4 | 320x240 | 1 sec | 3.5 KB | Minimal H.264 video |
| test-video.webm | VP8/WebM | 320x240 | 1 sec | 2.2 KB | Minimal VP8 video |
| test-video.ogv | Theora/Ogg | 320x240 | 1 sec | 7.2 KB | Minimal Theora video |

### Images
| File | Format | Resolution | Size | Description |
|------|--------|-----------|------|-------------|
| poster.png | PNG | 320x240 | 886 B | Red poster image |

## Media Test Helper API

The `MediaTestHelper` class provides utilities for media testing:

### Element Creation
```typescript
await mediaHelper.createAudioElement(config);
await mediaHelper.createVideoElement(config);
```

### Playback Control
```typescript
await mediaHelper.play(id);
await mediaHelper.pause(id);
await mediaHelper.seek(id, time);
await mediaHelper.setVolume(id, volume);
await mediaHelper.setMuted(id, muted);
await mediaHelper.setPlaybackRate(id, rate);
```

### State Queries
```typescript
const state = await mediaHelper.getPlaybackState(id);
const props = await mediaHelper.getAudioProperties(id);
const props = await mediaHelper.getVideoProperties(id);
const readyState = await mediaHelper.getReadyState(id);
const networkState = await mediaHelper.getNetworkState(id);
```

### Event Handling
```typescript
const events = await mediaHelper.getMediaEvents();
const occurred = await mediaHelper.waitForEvent(id, eventType);
await mediaHelper.clearEventLog();
const verified = await mediaHelper.verifyEventSequence(eventList);
```

### Format Support
```typescript
const support = await mediaHelper.canPlayType(id, mimeType);
const types = await mediaHelper.getSupportedMediaTypes(id);
```

### Tracks
```typescript
const tracks = await mediaHelper.getTextTracks(id);
await mediaHelper.addTextTrack(id, kind, label, srclang);
```

### Advanced Features
```typescript
const supported = await mediaHelper.isFullscreenSupported(id);
await mediaHelper.requestFullscreen(id);

const pipSupported = await mediaHelper.isPictureInPictureSupported();
const pipRequested = await mediaHelper.requestPictureInPicture(id);
```

## Event Types

Media elements fire various events during their lifecycle:

### Loading Events
- `loadstart`: Media resource selection started
- `progress`: Browser downloading media
- `suspend`: Media download suspended
- `abort`: Media loading aborted
- `error`: Error during media loading
- `emptied`: Media buffer emptied
- `stalled`: Media download stalled

### Metadata Events
- `loadedmetadata`: Duration and dimensions loaded
- `loadeddata`: Current frame data loaded
- `canplay`: Media can start playing
- `canplaythrough`: Enough data to play through
- `playing`: Playback started

### Playback Events
- `play`: Play called or autoplay started
- `pause`: Pause called
- `playing`: Actually started playing
- `ended`: Playback finished
- `seeking`: User seeking
- `seeked`: Seeking completed
- `timeupdate`: Current time changed
- `durationchange`: Duration updated
- `ratechange`: Playback rate changed
- `volumechange`: Volume/mute changed

## Running Tests

### Run All Multimedia Tests
```bash
npm test -- tests/multimedia/media.spec.ts
```

### Run Specific Test
```bash
npm test -- tests/multimedia/media.spec.ts -g "MEDIA-001"
```

### Run With Debug Output
```bash
PWDEBUG=1 npm test -- tests/multimedia/media.spec.ts
```

### Run With Video Recording
```bash
npm test -- tests/multimedia/media.spec.ts --headed
```

## Expected Browser Support

### Audio Formats
- **MP3 (MPEG)**: Widely supported (probably/maybe)
- **WAV**: Commonly supported (probably/maybe)
- **Ogg Vorbis**: Supported in Firefox, Chrome, Edge
- **WebM**: Chrome, Firefox, Edge support

### Video Formats
- **MP4 (H.264/AVC)**: Widely supported (probably)
- **WebM (VP8/VP9)**: Chrome, Firefox, Edge support
- **Ogg Theora**: Firefox and some others support

### HTML5 APIs
- **Basic Playback**: All modern browsers
- **Fullscreen**: Most modern browsers
- **Picture-in-Picture**: Chrome, Firefox, Edge 79+
- **Text Tracks**: All modern browsers

## Troubleshooting

### Tests Timing Out
**Issue**: Media elements not loading
**Solution**: Ensure fixtures directory exists and files are accessible
```bash
ls -la Tests/Playwright/fixtures/media/
```

### Volume/Mute Tests Failing
**Issue**: Volume changes not detected
**Solution**: Verify volumechange event listeners are registered
Check browser console for errors

### Seeking Tests Failing
**Issue**: currentTime not updating
**Solution**: Ensure media can be seeked (loading must complete first)
Use `waitForMetadata()` before seeking

### Format Detection Failing
**Issue**: canPlayType() returns empty string
**Solution**: Browser may not support that format
Test with multiple formats and accept browser capabilities

### PiP Tests Failing
**Issue**: Picture-in-Picture not working
**Solution**: PiP requires user interaction or specific permissions
May not work in headless/sandbox environments

## Coverage Analysis

| Category | Tests | Status |
|----------|-------|--------|
| Audio Playback | 6 | Complete |
| Video Playback | 6 | Complete |
| Media API | 4 | Complete |
| Advanced Features | 3 | Complete |
| Additional Features | 5 | Complete |
| **Total** | **24** | **Complete** |

## Limitations and Notes

### Headless Browser Limitations
- Fullscreen may not work in headless mode
- Picture-in-Picture may be restricted
- Autoplay may require user interaction
- Some audio output is not available

### Media File Considerations
- All test files are minimal (< 100KB)
- Files use silent/blank content for fast testing
- Avoid network delays with local file testing
- Use data:// URLs for HTML-based tests

### Event Testing Notes
- Events fire asynchronously
- Event timing varies by browser
- Multiple events may fire for single action
- Always clear event log between tests

### Performance Considerations
- Tests use small media files for speed
- Metadata loading is fastest operation
- Full playback tests add ~100-200ms overhead
- Parallel test execution recommended

## Future Enhancements

Potential additions to test coverage:
1. MediaSource API for streaming
2. Audio context integration
3. WebRTC media elements
4. Media capture (getUserMedia)
5. Spatial audio features
6. Codec-specific testing
7. Network error simulation
8. Buffer state monitoring
9. Frame-by-frame analysis
10. Audio visualization

## References

- [HTML5 Media Elements - W3C](https://html.spec.whatwg.org/multipage/media.html)
- [HTMLMediaElement API](https://developer.mozilla.org/en-US/docs/Web/API/HTMLMediaElement)
- [HTMLAudioElement](https://developer.mozilla.org/en-US/docs/Web/API/HTMLAudioElement)
- [HTMLVideoElement](https://developer.mozilla.org/en-US/docs/Web/API/HTMLVideoElement)
- [Picture-in-Picture API](https://developer.mozilla.org/en-US/docs/Web/API/Picture-in-Picture_API)
- [TextTrack API](https://developer.mozilla.org/en-US/docs/Web/API/TextTrack)

## Contributing

When adding new multimedia tests:

1. Use `MediaTestHelper` for consistent APIs
2. Test both audio and video where applicable
3. Include error cases and edge conditions
4. Document MIME types and fixtures used
5. Ensure tests run in < 5 seconds
6. Add to this documentation

## Contact & Support

For issues or questions about multimedia tests:
1. Check test fixture files exist
2. Verify browser media codec support
3. Check Playwright browser compatibility
4. Review Ladybird media implementation
5. Open an issue with test name and error

---

**Last Updated**: November 2025
**Test Framework**: Playwright
**Target Browser**: Ladybird
**Status**: Active Development
