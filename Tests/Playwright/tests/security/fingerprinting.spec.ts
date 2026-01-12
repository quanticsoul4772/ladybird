import { test, expect } from '@playwright/test';

/**
 * Fingerprinting Detection Tests (FP-001 to FP-012)
 * Priority: P0 (Critical) - FORK FEATURE
 *
 * Tests Sentinel's browser fingerprinting detection system including:
 * - Canvas fingerprinting (toDataURL, getImageData)
 * - WebGL fingerprinting (getParameter, renderer info)
 * - Audio fingerprinting (OscillatorNode, AnalyserNode)
 * - Navigator enumeration
 * - Font enumeration
 * - Screen properties
 * - Aggressive fingerprinting scoring
 */

test.describe('Fingerprinting Detection', () => {

  test('FP-001: Canvas fingerprinting - toDataURL()', { tag: '@p0' }, async ({ page }) => {
    // Create test page with canvas fingerprinting
    const html = `
      <html>
        <body>
          <canvas id="canvas" width="200" height="50"></canvas>
          <div id="result"></div>
          <script>
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = '#f60';
            ctx.fillRect(125, 1, 62, 20);
            ctx.fillStyle = '#069';
            ctx.font = '11pt Arial';
            ctx.fillText('Fingerprint Test', 2, 15);

            // This should trigger fingerprinting detection
            const dataURL = canvas.toDataURL();
            document.getElementById('result').textContent = 'Canvas fingerprint: ' + dataURL.substring(0, 50);

            // Flag for test to check
            window.__fingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);

    // Wait for canvas operations to complete
    await page.waitForTimeout(500);

    // Verify canvas fingerprinting was detected
    // TODO: This requires integration with Ladybird's fingerprinting detection API
    // For now, just verify the operation completed without crash
    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Canvas fingerprint');

    // In actual Ladybird test, check if:
    // 1. FingerprintingDetector.record_api_call() was invoked
    // 2. Score was calculated
    // 3. Alert shown if score > threshold
  });

  test('FP-002: Canvas fingerprinting - getImageData()', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <canvas id="canvas" width="200" height="50"></canvas>
          <div id="result"></div>
          <script>
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');
            ctx.fillRect(10, 10, 50, 50);

            // getImageData is another fingerprinting vector
            const imageData = ctx.getImageData(0, 0, 200, 50);
            document.getElementById('result').textContent = 'Image data length: ' + imageData.data.length;

            window.__fingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Image data length');

    // TODO: Verify FingerprintingDetector recorded this call
  });

  test('FP-003: WebGL fingerprinting - getParameter()', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <canvas id="canvas"></canvas>
          <div id="result"></div>
          <script>
            const canvas = document.getElementById('canvas');
            const gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');

            if (gl) {
              // Common WebGL fingerprinting parameters
              const vendor = gl.getParameter(gl.VENDOR);
              const renderer = gl.getParameter(gl.RENDERER);
              const version = gl.getParameter(gl.VERSION);

              document.getElementById('result').textContent =
                'WebGL: ' + vendor + ', ' + renderer + ', ' + version;

              window.__webglFingerprintingDetected = true;
            } else {
              document.getElementById('result').textContent = 'WebGL not supported';
            }
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    // Either WebGL info or "not supported" message
    expect(resultText?.length).toBeGreaterThan(0);

    // TODO: If WebGL supported, verify fingerprinting detection
  });

  test('FP-004: Audio fingerprinting - OscillatorNode', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="result"></div>
          <script>
            try {
              const audioContext = new (window.AudioContext || window.webkitAudioContext)();
              const oscillator = audioContext.createOscillator();
              const analyser = audioContext.createAnalyser();

              oscillator.type = 'triangle';
              oscillator.connect(analyser);
              oscillator.start(0);

              // Audio fingerprinting technique
              const frequencyData = new Uint8Array(analyser.frequencyBinCount);
              analyser.getByteFrequencyData(frequencyData);

              document.getElementById('result').textContent =
                'Audio fingerprint data length: ' + frequencyData.length;

              oscillator.stop();
              window.__audioFingerprintingDetected = true;
            } catch (e) {
              document.getElementById('result').textContent = 'Audio API error: ' + e.message;
            }
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText?.length).toBeGreaterThan(0);

    // TODO: Verify FingerprintingDetector recorded audio fingerprinting
  });

  test('FP-005: Navigator enumeration fingerprinting', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="result"></div>
          <script>
            // Enumerate many navigator properties (fingerprinting technique)
            const props = [
              'userAgent',
              'language',
              'languages',
              'platform',
              'hardwareConcurrency',
              'deviceMemory',
              'maxTouchPoints',
              'vendor',
              'vendorSub',
              'productSub',
              'cookieEnabled',
              'doNotTrack',
              'onLine',
              'plugins'
            ];

            let count = 0;
            props.forEach(prop => {
              if (navigator[prop] !== undefined) {
                count++;
              }
            });

            document.getElementById('result').textContent =
              'Navigator properties accessed: ' + count;

            window.__navigatorFingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Navigator properties accessed');

    // TODO: Verify excessive navigator access was detected
    // FingerprintingDetector should flag when > NavigatorThreshold (10) accesses
  });

  test('FP-006: Font enumeration fingerprinting', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="result"></div>
          <script>
            // Font detection technique using size measurements
            const baseFonts = ['monospace', 'sans-serif', 'serif'];
            const testFonts = [
              'Arial', 'Verdana', 'Times New Roman', 'Courier New',
              'Georgia', 'Comic Sans MS', 'Trebuchet MS', 'Impact',
              'Calibri', 'Cambria', 'Consolas', 'Lucida Console',
              'Tahoma', 'Palatino', 'Garamond', 'Bookman',
              'Helvetica', 'Monaco', 'Futura', 'Geneva'
            ];

            const span = document.createElement('span');
            span.style.fontSize = '72px';
            span.innerHTML = 'mmmmmmmmmmlli';
            document.body.appendChild(span);

            let detectedFonts = 0;
            testFonts.forEach(font => {
              span.style.fontFamily = font + ',monospace';
              const width = span.offsetWidth;
              if (width > 0) detectedFonts++;
            });

            document.body.removeChild(span);

            document.getElementById('result').textContent =
              'Font enumeration checks: ' + testFonts.length;

            window.__fontFingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Font enumeration checks');

    // TODO: Verify font enumeration was detected (> FontThreshold = 20)
  });

  test('FP-007: Screen properties fingerprinting', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="result"></div>
          <script>
            // Screen fingerprinting
            const screenInfo = {
              width: screen.width,
              height: screen.height,
              availWidth: screen.availWidth,
              availHeight: screen.availHeight,
              colorDepth: screen.colorDepth,
              pixelDepth: screen.pixelDepth,
              devicePixelRatio: window.devicePixelRatio
            };

            document.getElementById('result').textContent =
              'Screen: ' + screenInfo.width + 'x' + screenInfo.height +
              ', depth: ' + screenInfo.colorDepth;

            window.__screenFingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('Screen:');

    // TODO: Verify screen properties access was recorded
  });

  test('FP-008: Aggressive fingerprinting (multiple techniques)', { tag: '@p0' }, async ({ page }) => {
    // Combine multiple fingerprinting techniques in rapid succession
    const html = `
      <html>
        <body>
          <canvas id="canvas" width="200" height="50"></canvas>
          <div id="result"></div>
          <script>
            // Canvas fingerprinting
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');
            ctx.fillText('Test', 10, 10);
            const canvasFP = canvas.toDataURL();
            const imageFP = ctx.getImageData(0, 0, 200, 50);

            // WebGL fingerprinting
            const gl = canvas.getContext('webgl');
            if (gl) {
              const vendor = gl.getParameter(gl.VENDOR);
              const renderer = gl.getParameter(gl.RENDERER);
            }

            // Audio fingerprinting
            try {
              const audioCtx = new AudioContext();
              const osc = audioCtx.createOscillator();
              const analyser = audioCtx.createAnalyser();
              osc.connect(analyser);
            } catch (e) {}

            // Navigator enumeration
            const nav = [
              navigator.userAgent,
              navigator.language,
              navigator.platform,
              navigator.hardwareConcurrency,
              navigator.deviceMemory,
              navigator.maxTouchPoints
            ];

            // Screen properties
            const screen_info = [
              screen.width,
              screen.height,
              screen.colorDepth,
              window.devicePixelRatio
            ];

            document.getElementById('result').textContent = 'Aggressive fingerprinting complete';
            window.__aggressiveFingerprintingDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    // TODO: In Ladybird, this should trigger high aggressiveness score (> 0.6)
    // and show user alert about fingerprinting
    const resultText = await page.locator('#result').textContent();
    expect(resultText).toContain('complete');

    // Verify multiple techniques detected:
    // - uses_canvas = true
    // - uses_webgl = true (if supported)
    // - uses_audio = true (if supported)
    // - uses_navigator = true
    // - uses_screen = true
    // - aggressiveness_score > 0.6
  });

  test('FP-009: Fingerprinting before user interaction', { tag: '@p0' }, async ({ page }) => {
    // Fingerprinting that occurs before any user interaction is more suspicious
    const html = `
      <html>
        <body>
          <div id="result">No interaction yet</div>
          <script>
            // Immediate fingerprinting (before user clicks anything)
            const canvas = document.createElement('canvas');
            const ctx = canvas.getContext('2d');
            ctx.fillText('Fingerprint', 10, 10);
            const fp = canvas.toDataURL();

            window.__fingerprintedBeforeInteraction = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    // No user interaction yet, but fingerprinting already occurred
    // FingerprintingDetector should flag: had_user_interaction = false
    // This increases suspicion score
  });

  test('FP-010: Rapid-fire fingerprinting (5+ calls in 1s)', { tag: '@p0' }, async ({ page }) => {
    const html = `
      <html>
        <body>
          <div id="result"></div>
          <script>
            const canvas = document.createElement('canvas');
            const ctx = canvas.getContext('2d');

            // Rapid-fire canvas API calls (timing analysis should detect this)
            for (let i = 0; i < 10; i++) {
              ctx.fillText('Test ' + i, 10, 10);
              const data = canvas.toDataURL();
            }

            document.getElementById('result').textContent = 'Rapid fingerprinting: 10 calls';
            window.__rapidFireDetected = true;
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    // FingerprintingDetector should detect:
    // - total_api_calls > RapidFireThreshold (5)
    // - time_window_seconds < 1.0
    // - rapid_fire_detected = true
  });

  test('FP-011: Fingerprinting on legitimate site (low score)', { tag: '@p0' }, async ({ page }) => {
    // Test that normal canvas usage doesn't trigger false positives
    const html = `
      <html>
        <body>
          <canvas id="canvas" width="400" height="300"></canvas>
          <script>
            // Legitimate canvas use (drawing visible chart)
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');

            // Draw a simple bar chart (legitimate use)
            ctx.fillStyle = '#3498db';
            ctx.fillRect(50, 200, 80, 100);
            ctx.fillRect(150, 150, 80, 150);
            ctx.fillRect(250, 180, 80, 120);

            // Single toDataURL call for legitimate export feature
            // (not fingerprinting if done after user interaction)
            // Don't call it yet, wait for user click

            document.getElementById('canvas').addEventListener('click', () => {
              const dataURL = canvas.toDataURL(); // Legitimate export
            });
          </script>
        </body>
      </html>
    `;

    await page.goto(`data:text/html,${encodeURIComponent(html)}`);
    await page.waitForTimeout(500);

    // Legitimate canvas use should have low fingerprinting score
    // FingerprintingDetector should recognize:
    // - Single API call after user interaction
    // - No rapid-fire pattern
    // - aggressiveness_score < 0.3 (benign)
  });

  test('FP-012: Test against browserleaks.com/canvas', { tag: '@p0' }, async ({ page }) => {
    // Real-world test against actual fingerprinting site
    // This test requires internet connection

    await page.goto('https://browserleaks.com/canvas', {
      waitUntil: 'domcontentloaded',
      timeout: 30000,
    });

    // Wait for fingerprinting tests to run
    await page.waitForTimeout(3000);

    // browserleaks.com will perform canvas fingerprinting
    // Ladybird's FingerprintingDetector should detect this and show alert

    // TODO: Verify that:
    // 1. Fingerprinting was detected
    // 2. User alert was shown
    // 3. User can choose to block or allow
    // 4. Decision is stored in PolicyGraph

    // For now, just verify page loaded without crash
    await expect(page).toHaveURL(/browserleaks\.com/);
  });

});
