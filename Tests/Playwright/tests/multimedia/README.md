# Multimedia Tests - Quick Reference

## Overview
Comprehensive test suite for HTML5 audio and video elements with 24 test cases covering basic playback, API testing, and advanced features.

## Test Suite Structure

```
tests/multimedia/
├── media.spec.ts              # Main test suite (24 tests)
├── media-test-helper.ts       # Helper utilities
└── README.md                  # This file
```

## Test Categories

### Audio Tests (6 tests)
- **MEDIA-001**: Basic playback
- **MEDIA-002**: Volume control
- **MEDIA-003**: Mute/unmute
- **MEDIA-004**: Play/pause
- **MEDIA-005**: Seeking
- **MEDIA-006**: Events

### Video Tests (6 tests)
- **MEDIA-007**: Basic playback
- **MEDIA-008**: Controls
- **MEDIA-009**: Fullscreen
- **MEDIA-010**: Poster image
- **MEDIA-011**: Multiple sources
- **MEDIA-012**: Events

### Media API Tests (4 tests)
- **MEDIA-013**: canPlayType()
- **MEDIA-014**: readyState
- **MEDIA-015**: Duration/currentTime
- **MEDIA-016**: Playback rate

### Advanced Tests (3 tests)
- **MEDIA-017**: Picture-in-Picture
- **MEDIA-018**: Text tracks
- **MEDIA-019**: Error handling

### Additional Tests (5 tests)
- **MEDIA-020**: Supported types
- **MEDIA-021**: Network state
- **MEDIA-022**: Preload attribute
- **MEDIA-023**: Loop attribute
- **MEDIA-024**: Autoplay attribute

## Running Tests

```bash
# Run all multimedia tests
npm test -- tests/multimedia/media.spec.ts

# Run specific test
npm test -- tests/multimedia/media.spec.ts -g "MEDIA-001"

# Run with debug
PWDEBUG=1 npm test -- tests/multimedia/media.spec.ts

# Run with headed browser
npm test -- tests/multimedia/media.spec.ts --headed
```

## Test Fixtures

Media files located in `fixtures/media/`:

**Audio Files** (< 5 KB each):
- `silent-mono.mp3` - Single channel MP3
- `silent-stereo.mp3` - Stereo MP3
- `silent.wav` - Uncompressed WAV
- `silent.ogg` - Ogg Vorbis

**Video Files** (< 10 KB each, 320x240):
- `test-video.mp4` - MP4/H.264
- `test-video.webm` - WebM/VP8
- `test-video.ogv` - Ogg Theora

**Images**:
- `poster.png` - Video poster (320x240)

## Media Test Helper Usage

```typescript
import { MediaTestHelper, MEDIA_FIXTURES } from './media-test-helper';

// Create helper
const helper = new MediaTestHelper(page);

// Create audio element
await helper.createAudioElement({
  id: 'player',
  src: MEDIA_FIXTURES.audio.mono,
  controls: true
});

// Control playback
await helper.play('player');
await helper.pause('player');
await helper.seek('player', 0.5);
await helper.setVolume('player', 0.5);

// Query state
const state = await helper.getPlaybackState('player');
// { paused, currentTime, duration, ended, readyState, networkState }

// Handle events
const events = await helper.getMediaEvents();
await helper.waitForEvent('player', 'playing');
```

## Key Assertions

```typescript
// Playback state
expect(state.paused).toBe(false);
expect(state.currentTime).toBeGreaterThan(0);
expect(state.duration).toBeGreaterThan(0);

// Audio properties
expect(props.volume).toBeCloseTo(0.5, 1);
expect(props.muted).toBe(true);

// Video properties
expect(videoProps.videoWidth).toBe(320);
expect(videoProps.videoHeight).toBe(240);

// Events
expect(events.some(e => e.type === 'playing')).toBe(true);

// Format support
expect(support).toMatch(/probably|maybe|/);

// Ready state
expect(readyState.state).toBeGreaterThanOrEqual(1);
```

## Common Patterns

### Wait for Media Ready
```typescript
await helper.waitForMetadata('player');
// Media duration is now available
```

### Play and Wait for Playing
```typescript
await helper.play('player');
const playing = await helper.waitForEvent('player', 'playing', 2000);
```

### Seek to Position
```typescript
const state = await helper.getPlaybackState('player');
const duration = state.duration;
await helper.seek('player', duration / 2); // Middle
```

### Check Format Support
```typescript
const types = await helper.getSupportedMediaTypes('player');
if (types.includes('audio/mpeg')) {
  // MP3 is supported
}
```

## Browser Compatibility Notes

| Feature | Chrome | Firefox | Safari | Ladybird |
|---------|--------|---------|--------|----------|
| Audio Playback | ✓ | ✓ | ✓ | ✓ |
| Video Playback | ✓ | ✓ | ✓ | ✓ |
| Volume Control | ✓ | ✓ | ✓ | ✓ |
| Seeking | ✓ | ✓ | ✓ | ✓ |
| Fullscreen | ✓ | ✓ | ✓ | ? |
| Picture-in-Picture | ✓ | ✓ | ✓ | ? |
| Text Tracks | ✓ | ✓ | ✓ | ✓ |

## Event Types Reference

**Load Events**: loadstart, progress, suspend, abort, error, emptied, stalled
**Metadata Events**: loadedmetadata, loadeddata, canplay, canplaythrough
**Playback Events**: play, playing, pause, ended
**Seek Events**: seeking, seeked
**Time Events**: timeupdate, durationchange
**State Events**: ratechange, volumechange

## Debugging Tips

1. **Enable Video Recording**:
   ```bash
   npm test -- --headed tests/multimedia/media.spec.ts
   ```

2. **Inspect Events**:
   ```typescript
   const events = await helper.getMediaEvents();
   console.log(JSON.stringify(events, null, 2));
   ```

3. **Check Element State**:
   ```typescript
   const state = await helper.getPlaybackState('player');
   const readyState = await helper.getReadyState('player');
   console.log('State:', state, 'Ready:', readyState);
   ```

4. **Monitor Network State**:
   ```typescript
   const networkState = await helper.getNetworkState('player');
   console.log('Network:', networkState.description);
   ```

## Performance

- All tests use small media files (< 10 KB)
- Average test time: 500ms - 2s
- Can run in parallel
- No external network requests
- Suitable for CI/CD pipelines

## Common Issues

| Issue | Solution |
|-------|----------|
| Fixtures not found | `ls -la fixtures/media/` |
| Timeout on metadata | Check file exists and is valid |
| Volume not changing | Verify element not muted |
| Seeking fails | Wait for metadata before seeking |
| Events not firing | Check event type spelling |

## Test Metrics

- **Total Tests**: 24
- **Total Assertions**: 200+
- **Code Coverage**: Audio, Video, APIs, Advanced
- **Fixture Size**: ~140 KB total
- **Execution Time**: ~60 seconds (full suite)

## Future Work

- [ ] MediaSource API tests
- [ ] Audio Context integration
- [ ] Network error scenarios
- [ ] Buffer state monitoring
- [ ] Codec-specific testing
- [ ] Performance profiling

## See Also

- Full documentation: `MULTIMEDIA_TESTS.md`
- Test helper API: `media-test-helper.ts`
- Main test file: `media.spec.ts`
- Fixtures: `../../fixtures/media/`

---

**Test Status**: Active
**Last Updated**: November 2025
**Framework**: Playwright + TypeScript
**Target**: Ladybird Browser
