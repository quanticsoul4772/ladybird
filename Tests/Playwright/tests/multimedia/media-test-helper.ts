/**
 * Media Test Helper for Multimedia Tests
 *
 * Provides utilities for testing HTML5 audio/video elements:
 * - Media element creation and configuration
 * - Event listeners and tracking
 * - State verification (readyState, networkState)
 * - API testing (play, pause, seek, volume)
 * - Event sequence validation
 */

import { Page, Locator, expect } from '@playwright/test';

export interface MediaTestConfig {
  id: string;
  src?: string;
  sources?: Array<{ src: string; type: string }>;
  controls?: boolean;
  autoplay?: boolean;
  loop?: boolean;
  muted?: boolean;
  preload?: 'none' | 'metadata' | 'auto';
  width?: number;
  height?: number;
  poster?: string;
}

export interface MediaEventLog {
  timestamp: number;
  event: string;
  detail?: any;
}

export class MediaTestHelper {
  private page: Page;
  private eventLog: MediaEventLog[] = [];

  constructor(page: Page) {
    this.page = page;
  }

  /**
   * Create an audio element with specified configuration
   */
  async createAudioElement(config: MediaTestConfig): Promise<void> {
    const sources = config.sources
      ? config.sources.map(s => `<source src="${s.src}" type="${s.type}">`).join('')
      : `<source src="${config.src}" type="audio/mpeg">`;

    const controls = config.controls ? 'controls' : '';
    const autoplay = config.autoplay ? 'autoplay' : '';
    const loop = config.loop ? 'loop' : '';
    const muted = config.muted ? 'muted' : '';
    const preload = config.preload ? `preload="${config.preload}"` : '';

    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <audio
            id="${config.id}"
            ${controls}
            ${autoplay}
            ${loop}
            ${muted}
            ${preload}
          >
            ${sources}
          </audio>
          <script>
            const audio = document.getElementById('${config.id}');
            const events = window.mediaEvents = [];

            const eventTypes = [
              'loadstart', 'progress', 'suspend', 'abort', 'error', 'emptied', 'stalled',
              'loadedmetadata', 'loadeddata', 'canplay', 'canplaythrough', 'playing',
              'pause', 'ended', 'seeking', 'seeked', 'timeupdate', 'durationchange',
              'ratechange', 'volumechange'
            ];

            eventTypes.forEach(eventType => {
              audio.addEventListener(eventType, () => {
                events.push({
                  type: eventType,
                  currentTime: audio.currentTime,
                  duration: audio.duration,
                  readyState: audio.readyState,
                  networkState: audio.networkState
                });
              });
            });
          </script>
        </body>
      </html>
    `;

    await this.page.goto(`data:text/html,${encodeURIComponent(html)}`);
  }

  /**
   * Create a video element with specified configuration
   */
  async createVideoElement(config: MediaTestConfig): Promise<void> {
    const sources = config.sources
      ? config.sources.map(s => `<source src="${s.src}" type="${s.type}">`).join('')
      : `<source src="${config.src}" type="video/mp4">`;

    const controls = config.controls ? 'controls' : '';
    const autoplay = config.autoplay ? 'autoplay' : '';
    const loop = config.loop ? 'loop' : '';
    const muted = config.muted ? 'muted' : '';
    const preload = config.preload ? `preload="${config.preload}"` : '';
    const width = config.width ? `width="${config.width}"` : '';
    const height = config.height ? `height="${config.height}"` : '';
    const poster = config.poster ? `poster="${config.poster}"` : '';

    const html = `
      <!DOCTYPE html>
      <html>
        <body>
          <video
            id="${config.id}"
            ${controls}
            ${autoplay}
            ${loop}
            ${muted}
            ${preload}
            ${width}
            ${height}
            ${poster}
          >
            ${sources}
          </video>
          <script>
            const video = document.getElementById('${config.id}');
            const events = window.mediaEvents = [];

            const eventTypes = [
              'loadstart', 'progress', 'suspend', 'abort', 'error', 'emptied', 'stalled',
              'loadedmetadata', 'loadeddata', 'canplay', 'canplaythrough', 'playing',
              'pause', 'ended', 'seeking', 'seeked', 'timeupdate', 'durationchange',
              'ratechange', 'volumechange'
            ];

            eventTypes.forEach(eventType => {
              video.addEventListener(eventType, () => {
                events.push({
                  type: eventType,
                  currentTime: video.currentTime,
                  duration: video.duration,
                  readyState: video.readyState,
                  networkState: video.networkState
                });
              });
            });
          </script>
        </body>
      </html>
    `;

    await this.page.goto(`data:text/html,${encodeURIComponent(html)}`);
  }

  /**
   * Get media element locator
   */
  getMediaElement(id: string): Locator {
    return this.page.locator(`#${id}`);
  }

  /**
   * Get current playback state
   */
  async getPlaybackState(
    id: string
  ): Promise<{
    paused: boolean;
    currentTime: number;
    duration: number;
    ended: boolean;
    readyState: number;
    networkState: number;
  }> {
    return this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      return {
        paused: element.paused,
        currentTime: element.currentTime,
        duration: element.duration,
        ended: element.ended,
        readyState: element.readyState,
        networkState: element.networkState
      };
    }, id);
  }

  /**
   * Get audio properties
   */
  async getAudioProperties(id: string): Promise<{
    volume: number;
    muted: boolean;
    channels: number;
  }> {
    return this.page.evaluate((elementId) => {
      const audio = document.getElementById(elementId) as any;
      return {
        volume: audio.volume,
        muted: audio.muted,
        channels: audio.mozChannels || audio.webkitAudioContext?.destination.maxChannelCount || 2
      };
    }, id);
  }

  /**
   * Get video properties
   */
  async getVideoProperties(id: string): Promise<{
    videoWidth: number;
    videoHeight: number;
    width: number;
    height: number;
  }> {
    return this.page.evaluate((elementId) => {
      const video = document.getElementById(elementId) as any;
      return {
        videoWidth: video.videoWidth,
        videoHeight: video.videoHeight,
        width: video.width,
        height: video.height
      };
    }, id);
  }

  /**
   * Play media element
   */
  async play(id: string): Promise<void> {
    await this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      return element.play().catch(() => {
        // Handle promise rejection gracefully
      });
    }, id);
  }

  /**
   * Pause media element
   */
  async pause(id: string): Promise<void> {
    await this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      element.pause();
    }, id);
  }

  /**
   * Seek to specified time
   */
  async seek(id: string, time: number): Promise<void> {
    await this.page.evaluate(
      ({ elementId, seekTime }) => {
        const element = document.getElementById(elementId) as any;
        element.currentTime = seekTime;
      },
      { elementId: id, seekTime: time }
    );
  }

  /**
   * Set volume
   */
  async setVolume(id: string, volume: number): Promise<void> {
    await this.page.evaluate(
      ({ elementId, vol }) => {
        const audio = document.getElementById(elementId) as any;
        audio.volume = Math.max(0, Math.min(1, vol));
      },
      { elementId: id, vol: volume }
    );
  }

  /**
   * Set muted state
   */
  async setMuted(id: string, muted: boolean): Promise<void> {
    await this.page.evaluate(
      ({ elementId, isMuted }) => {
        const element = document.getElementById(elementId) as any;
        element.muted = isMuted;
      },
      { elementId: id, isMuted: muted }
    );
  }

  /**
   * Set playback rate
   */
  async setPlaybackRate(id: string, rate: number): Promise<void> {
    await this.page.evaluate(
      ({ elementId, playRate }) => {
        const element = document.getElementById(elementId) as any;
        element.playbackRate = playRate;
      },
      { elementId: id, playRate: rate }
    );
  }

  /**
   * Get recorded events
   */
  async getMediaEvents(): Promise<MediaEventLog[]> {
    return this.page.evaluate(() => {
      return (window as any).mediaEvents || [];
    });
  }

  /**
   * Clear event log
   */
  async clearEventLog(): Promise<void> {
    await this.page.evaluate(() => {
      (window as any).mediaEvents = [];
    });
  }

  /**
   * Wait for specific event
   */
  async waitForEvent(id: string, eventType: string, timeoutMs: number = 5000): Promise<boolean> {
    try {
      await this.page.waitForFunction(
        ({ elementId, event }) => {
          const events = (window as any).mediaEvents || [];
          return events.some((e: any) => e.type === event);
        },
        { elementId: id, event: eventType },
        { timeout: timeoutMs }
      );
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Check if canPlayType is supported
   */
  async canPlayType(id: string, mimeType: string): Promise<string> {
    return this.page.evaluate(
      ({ elementId, mime }) => {
        const element = document.getElementById(elementId) as any;
        return element.canPlayType(mime);
      },
      { elementId: id, mime: mimeType }
    );
  }

  /**
   * Load media resource
   */
  async load(id: string): Promise<void> {
    await this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      element.load();
    }, id);
  }

  /**
   * Get network state
   */
  async getNetworkState(id: string): Promise<{
    state: number;
    description: string;
  }> {
    return this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      const states: { [key: number]: string } = {
        0: 'NETWORK_EMPTY',
        1: 'NETWORK_IDLE',
        2: 'NETWORK_LOADING',
        3: 'NETWORK_NO_SOURCE'
      };
      return {
        state: element.networkState,
        description: states[element.networkState] || 'UNKNOWN'
      };
    }, id);
  }

  /**
   * Get ready state
   */
  async getReadyState(id: string): Promise<{
    state: number;
    description: string;
  }> {
    return this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      const states: { [key: number]: string } = {
        0: 'HAVE_NOTHING',
        1: 'HAVE_METADATA',
        2: 'HAVE_CURRENT_DATA',
        3: 'HAVE_FUTURE_DATA',
        4: 'HAVE_ENOUGH_DATA'
      };
      return {
        state: element.readyState,
        description: states[element.readyState] || 'UNKNOWN'
      };
    }, id);
  }

  /**
   * Check if fullscreen is supported
   */
  async isFullscreenSupported(id: string): Promise<boolean> {
    return this.page.evaluate((elementId) => {
      const video = document.getElementById(elementId) as any;
      return (
        document.fullscreenEnabled &&
        (video.requestFullscreen !== undefined ||
         (video as any).webkitRequestFullscreen !== undefined)
      );
    }, id);
  }

  /**
   * Request fullscreen
   */
  async requestFullscreen(id: string): Promise<void> {
    await this.page.evaluate((elementId) => {
      const video = document.getElementById(elementId) as any;
      if (video.requestFullscreen) {
        return video.requestFullscreen();
      } else if ((video as any).webkitRequestFullscreen) {
        return (video as any).webkitRequestFullscreen();
      }
    }, id);
  }

  /**
   * Check Picture-in-Picture support
   */
  async isPictureInPictureSupported(): Promise<boolean> {
    return this.page.evaluate(() => {
      return document.pictureInPictureEnabled === true;
    });
  }

  /**
   * Request Picture-in-Picture
   */
  async requestPictureInPicture(id: string): Promise<boolean> {
    try {
      return await this.page.evaluate((elementId) => {
        const video = document.getElementById(elementId) as any;
        if (document.pictureInPictureEnabled && video.requestPictureInPicture) {
          return video.requestPictureInPicture()
            .then(() => true)
            .catch(() => false);
        }
        return false;
      }, id);
    } catch {
      return false;
    }
  }

  /**
   * Get text tracks
   */
  async getTextTracks(id: string): Promise<
    Array<{
      kind: string;
      label: string;
      language: string;
    }>
  > {
    return this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      const tracks = [];
      for (let i = 0; i < element.textTracks.length; i++) {
        const track = element.textTracks[i];
        tracks.push({
          kind: track.kind,
          label: track.label,
          language: track.language
        });
      }
      return tracks;
    }, id);
  }

  /**
   * Add text track
   */
  async addTextTrack(
    id: string,
    kind: 'subtitles' | 'captions' | 'descriptions' | 'chapters' | 'metadata',
    label: string,
    srclang: string
  ): Promise<void> {
    await this.page.evaluate(
      ({ elementId, trackKind, trackLabel, trackSrclang }) => {
        const element = document.getElementById(elementId) as any;
        const track = element.addTextTrack(trackKind, trackLabel, trackSrclang);
        track.addCue(new VTTCue(0, 10, 'Sample subtitle'));
      },
      { elementId: id, trackKind: kind, trackLabel: label, trackSrclang: srclang }
    );
  }

  /**
   * Verify event sequence
   */
  async verifyEventSequence(expectedEvents: string[]): Promise<boolean> {
    const events = await this.getMediaEvents();
    const eventTypes = events.map(e => e.type);

    // Check if expectedEvents occur in order (not necessarily consecutive)
    let eventIndex = 0;
    for (const expected of expectedEvents) {
      const foundIndex = eventTypes.indexOf(expected, eventIndex);
      if (foundIndex === -1) {
        return false;
      }
      eventIndex = foundIndex + 1;
    }
    return true;
  }

  /**
   * Wait for media to be ready to play
   */
  async waitForCanPlay(id: string, timeoutMs: number = 10000): Promise<boolean> {
    try {
      await this.page.waitForFunction(
        (elementId) => {
          const element = document.getElementById(elementId) as any;
          return element.readyState >= 2; // HAVE_CURRENT_DATA or better
        },
        id,
        { timeout: timeoutMs }
      );
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Wait for metadata to load
   */
  async waitForMetadata(id: string, timeoutMs: number = 10000): Promise<boolean> {
    try {
      await this.page.waitForFunction(
        (elementId) => {
          const element = document.getElementById(elementId) as any;
          return element.duration > 0;
        },
        id,
        { timeout: timeoutMs }
      );
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Get supported media types
   */
  async getSupportedMediaTypes(id: string): Promise<string[]> {
    return this.page.evaluate((elementId) => {
      const element = document.getElementById(elementId) as any;
      const types = [
        'audio/mpeg',
        'audio/wav',
        'audio/ogg',
        'audio/webm',
        'video/mp4',
        'video/webm',
        'video/ogg'
      ];

      return types.filter(type => {
        const support = element.canPlayType(type);
        return support === 'probably' || support === 'maybe';
      });
    }, id);
  }
}

/**
 * Media file paths for testing
 * Note: Test server (test-server.js) serves 'fixtures/' directory as root,
 * so paths should be absolute URLs pointing to the test server (http://localhost:9080)
 * We use port 9080 as the primary test server (configured in playwright.config.ts)
 */
export const MEDIA_FIXTURES = {
  audio: {
    mono: 'http://localhost:9080/media/silent-mono.mp3',
    stereo: 'http://localhost:9080/media/silent-stereo.mp3',
    wav: 'http://localhost:9080/media/silent.wav',
    ogg: 'http://localhost:9080/media/silent.ogg'
  },
  video: {
    mp4: 'http://localhost:9080/media/test-video.mp4',
    webm: 'http://localhost:9080/media/test-video.webm',
    ogv: 'http://localhost:9080/media/test-video.ogv'
  },
  poster: 'http://localhost:9080/media/poster.png'
};

/**
 * Common media MIME types
 */
export const MEDIA_TYPES = {
  audio: {
    mp3: 'audio/mpeg',
    wav: 'audio/wav',
    ogg: 'audio/ogg',
    webm: 'audio/webm',
    aac: 'audio/aac',
    flac: 'audio/flac'
  },
  video: {
    mp4: 'video/mp4',
    webm: 'video/webm',
    ogg: 'video/ogg',
    mpeg: 'video/mpeg'
  }
};
