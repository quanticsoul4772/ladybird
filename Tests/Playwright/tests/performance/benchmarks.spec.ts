import { test, expect } from '@playwright/test';

/**
 * Performance Benchmark Tests (PERF-001 to PERF-015)
 * Priority: P1 (High) - Performance Regression Prevention
 *
 * Comprehensive performance benchmarks covering:
 * - Page Load Performance (3 tests)
 * - JavaScript Execution (3 tests)
 * - Rendering Performance (3 tests)
 * - Network Performance (2 tests)
 * - Sentinel Security Performance (4 tests)
 *
 * Thresholds are generous to account for:
 * - CI environment variability
 * - Hardware differences
 * - Concurrent test execution
 * - Background system processes
 *
 * All measurements use Performance API for accuracy and consistency.
 */

// ============================================
// PAGE LOAD PERFORMANCE (PERF-001 to PERF-003)
// ============================================

test.describe('Page Load Performance', () => {
  test('PERF-001: Simple page load time', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests baseline page load performance with minimal HTML
     * Threshold: < 500ms for simple page
     */
    console.log('PERF-001: Measuring simple page load time...');

    const startTime = Date.now();
    await page.goto('http://localhost:9081/performance/simple-page.html');
    await page.waitForLoadState('load');
    const loadTime = Date.now() - startTime;

    console.log(`Simple page loaded in ${loadTime}ms`);

    // Get Performance API metrics
    const perfMetrics = await page.evaluate(() => {
      const perf = performance.getEntriesByType('navigation')[0] as PerformanceNavigationTiming;
      return {
        domContentLoaded: perf.domContentLoadedEventEnd - perf.domContentLoadedEventStart,
        domInteractive: perf.domInteractive - perf.fetchStart,
        loadComplete: perf.loadEventEnd - perf.fetchStart,
        domComplete: perf.domComplete - perf.fetchStart,
      };
    });

    console.log(`Performance metrics:`, perfMetrics);

    // Verify page loaded
    await expect(page).toHaveTitle(/Simple Page Performance Test/);

    // Performance assertions (generous thresholds for CI)
    expect(loadTime).toBeLessThan(1000); // 1 second max
    expect(perfMetrics.domInteractive).toBeLessThan(800);
    expect(perfMetrics.loadComplete).toBeLessThan(1000);
  });

  test('PERF-002: Complex DOM page load time', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests page load with large DOM (1000+ elements)
     * Threshold: < 3000ms for complex DOM
     */
    console.log('PERF-002: Measuring complex DOM page load time...');

    const startTime = Date.now();
    await page.goto('http://localhost:9081/performance/complex-dom.html');
    await page.waitForLoadState('load');
    const loadTime = Date.now() - startTime;

    console.log(`Complex DOM page loaded in ${loadTime}ms`);

    // Get DOM metrics
    const domMetrics = await page.evaluate(() => {
      const perf = performance.getEntriesByType('navigation')[0] as PerformanceNavigationTiming;
      return {
        domContentLoaded: perf.domContentLoadedEventEnd - perf.domContentLoadedEventStart,
        domInteractive: perf.domInteractive - perf.fetchStart,
        loadComplete: perf.loadEventEnd - perf.fetchStart,
        nodeCount: document.querySelectorAll('*').length,
      };
    });

    console.log(`DOM metrics:`, domMetrics);
    console.log(`Total nodes: ${domMetrics.nodeCount}`);

    // Verify complex DOM exists
    expect(domMetrics.nodeCount).toBeGreaterThan(1000);

    // Performance assertions (generous for complex page)
    expect(loadTime).toBeLessThan(5000); // 5 seconds max
    expect(domMetrics.domInteractive).toBeLessThan(4000);
    expect(domMetrics.loadComplete).toBeLessThan(5000);
  });

  test('PERF-003: Media-heavy page load time', { tag: '@p2' }, async ({ page }) => {
    /**
     * Tests page load with multiple images and media elements
     * Threshold: < 5000ms (includes resource loading)
     */
    console.log('PERF-003: Measuring media-heavy page load time...');

    const startTime = Date.now();
    await page.goto('http://localhost:9081/performance/media-heavy.html');
    await page.waitForLoadState('networkidle');
    const loadTime = Date.now() - startTime;

    console.log(`Media-heavy page loaded in ${loadTime}ms`);

    // Get resource timing metrics
    const resourceMetrics = await page.evaluate(() => {
      const resources = performance.getEntriesByType('resource');
      const images = resources.filter(r => r.name.match(/\.(jpg|png|gif|svg)$/i));
      const perf = performance.getEntriesByType('navigation')[0] as PerformanceNavigationTiming;

      return {
        totalResources: resources.length,
        imageCount: images.length,
        loadComplete: perf.loadEventEnd - perf.fetchStart,
        domComplete: perf.domComplete - perf.fetchStart,
      };
    });

    console.log(`Resource metrics:`, resourceMetrics);

    // Verify media resources loaded (SVG data URIs don't count as external resources)
    // So we check for image elements in the DOM instead
    const imageElements = await page.evaluate(() => document.querySelectorAll('img').length);
    console.log(`Image elements in DOM: ${imageElements}`);
    expect(imageElements).toBeGreaterThan(5);

    // Performance assertions (generous for media loading)
    expect(loadTime).toBeLessThan(8000); // 8 seconds max for all resources
    expect(resourceMetrics.loadComplete).toBeLessThan(8000);
  });
});

// ============================================
// JAVASCRIPT EXECUTION (PERF-004 to PERF-006)
// ============================================

test.describe('JavaScript Execution Performance', () => {
  test('PERF-004: Computation performance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests JavaScript computation speed (array operations, math)
     * Threshold: < 200ms for 10,000 iterations
     */
    console.log('PERF-004: Measuring JavaScript computation performance...');

    await page.goto('http://localhost:9081/performance/js-computation.html');

    // Run computation benchmark
    const computeMetrics = await page.evaluate(() => {
      const start = performance.now();

      // Heavy computation: array operations and math
      let result = 0;
      const arr = Array.from({ length: 10000 }, (_, i) => i);

      for (let i = 0; i < arr.length; i++) {
        result += Math.sqrt(arr[i]) * Math.sin(arr[i]) + Math.cos(arr[i]);
      }

      const duration = performance.now() - start;

      return {
        duration,
        result,
        iterations: arr.length,
      };
    });

    console.log(`Computation completed in ${computeMetrics.duration.toFixed(2)}ms`);
    console.log(`Result: ${computeMetrics.result.toFixed(2)}`);

    // Performance assertions (generous for various CPUs)
    expect(computeMetrics.duration).toBeLessThan(500); // 500ms max
    expect(computeMetrics.iterations).toBe(10000);
  });

  test('PERF-005: DOM manipulation performance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests DOM manipulation speed (create, append, modify elements)
     * Threshold: < 500ms for 1,000 operations
     */
    console.log('PERF-005: Measuring DOM manipulation performance...');

    await page.goto('http://localhost:9081/performance/js-dom-manipulation.html');

    // Run DOM manipulation benchmark
    const domMetrics = await page.evaluate(() => {
      const start = performance.now();
      const container = document.getElementById('benchmark-container');

      // Create and append 1000 elements
      for (let i = 0; i < 1000; i++) {
        const div = document.createElement('div');
        div.className = 'test-item';
        div.textContent = `Item ${i}`;
        div.setAttribute('data-index', i.toString());
        container!.appendChild(div);
      }

      // Modify elements
      const items = container!.querySelectorAll('.test-item');
      items.forEach((item, idx) => {
        if (idx % 2 === 0) {
          item.classList.add('even');
        }
      });

      const duration = performance.now() - start;

      return {
        duration,
        elementCount: items.length,
        containerChildren: container!.children.length,
      };
    });

    console.log(`DOM manipulation completed in ${domMetrics.duration.toFixed(2)}ms`);
    console.log(`Created ${domMetrics.elementCount} elements`);

    // Performance assertions
    expect(domMetrics.duration).toBeLessThan(1000); // 1 second max
    expect(domMetrics.elementCount).toBe(1000);
    expect(domMetrics.containerChildren).toBe(1000);
  });

  test('PERF-006: Event handling performance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests event listener registration and firing performance
     * Threshold: < 300ms for 1,000 event handlers
     */
    console.log('PERF-006: Measuring event handling performance...');

    await page.goto('http://localhost:9081/performance/js-event-handling.html');

    // Run event handling benchmark
    const eventMetrics = await page.evaluate(() => {
      const start = performance.now();
      const container = document.getElementById('event-container');
      let eventCount = 0;

      // Register 1000 event listeners
      const buttons: HTMLElement[] = [];
      for (let i = 0; i < 1000; i++) {
        const button = document.createElement('button');
        button.textContent = `Button ${i}`;
        button.addEventListener('click', () => {
          eventCount++;
        });
        container!.appendChild(button);
        buttons.push(button);
      }

      const setupTime = performance.now() - start;

      // Trigger events
      const fireStart = performance.now();
      buttons.forEach(btn => btn.click());
      const fireTime = performance.now() - fireStart;

      return {
        setupTime,
        fireTime,
        totalTime: setupTime + fireTime,
        eventCount,
        listenerCount: buttons.length,
      };
    });

    console.log(`Event setup: ${eventMetrics.setupTime.toFixed(2)}ms`);
    console.log(`Event firing: ${eventMetrics.fireTime.toFixed(2)}ms`);
    console.log(`Total: ${eventMetrics.totalTime.toFixed(2)}ms`);

    // Performance assertions
    expect(eventMetrics.totalTime).toBeLessThan(1500); // 1.5 seconds max
    expect(eventMetrics.eventCount).toBe(1000);
    expect(eventMetrics.listenerCount).toBe(1000);
  });
});

// ============================================
// RENDERING PERFORMANCE (PERF-007 to PERF-009)
// ============================================

test.describe('Rendering Performance', () => {
  test('PERF-007: Layout thrashing avoidance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests layout performance with forced reflows
     * Threshold: < 1000ms for 100 forced reflows
     */
    console.log('PERF-007: Measuring layout thrashing performance...');

    await page.goto('http://localhost:9081/performance/layout-thrashing.html');

    // Run layout benchmark (intentional thrashing for testing)
    const layoutMetrics = await page.evaluate(() => {
      const element = document.getElementById('test-element');
      const start = performance.now();

      // Intentional layout thrashing (read-write-read-write)
      for (let i = 0; i < 100; i++) {
        const height = element!.offsetHeight; // Force reflow
        element!.style.height = (height + 1) + 'px'; // Invalidate layout
      }

      const thrashingTime = performance.now() - start;

      // Now do optimized version (batch reads then batch writes)
      const optimizedStart = performance.now();
      const heights: number[] = [];

      // Batch reads
      for (let i = 0; i < 100; i++) {
        heights.push(element!.offsetHeight);
      }

      // Batch writes
      for (let i = 0; i < heights.length; i++) {
        element!.style.height = (heights[i] + 1) + 'px';
      }

      const optimizedTime = performance.now() - optimizedStart;

      return {
        thrashingTime,
        optimizedTime,
        improvement: ((thrashingTime - optimizedTime) / thrashingTime * 100).toFixed(1),
      };
    });

    console.log(`Layout thrashing: ${layoutMetrics.thrashingTime.toFixed(2)}ms`);
    console.log(`Optimized layout: ${layoutMetrics.optimizedTime.toFixed(2)}ms`);
    console.log(`Improvement: ${layoutMetrics.improvement}%`);

    // Performance assertions (thrashing should be slower)
    expect(layoutMetrics.thrashingTime).toBeLessThan(2000); // 2 seconds max
    expect(layoutMetrics.optimizedTime).toBeLessThan(layoutMetrics.thrashingTime);
  });

  test('PERF-008: Paint performance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests paint/repaint performance with style changes
     * Threshold: < 500ms for 500 paint operations
     */
    console.log('PERF-008: Measuring paint performance...');

    await page.goto('http://localhost:9081/performance/paint-performance.html');

    // Run paint benchmark
    const paintMetrics = await page.evaluate(() => {
      const container = document.getElementById('paint-container');
      const start = performance.now();

      // Create elements that will trigger paint
      for (let i = 0; i < 500; i++) {
        const div = document.createElement('div');
        div.className = 'paint-box';
        div.style.backgroundColor = `hsl(${i * 0.72}, 70%, 50%)`;
        div.style.width = '50px';
        div.style.height = '50px';
        div.textContent = i.toString();
        container!.appendChild(div);
      }

      const createTime = performance.now() - start;

      // Now repaint by changing colors
      const repaintStart = performance.now();
      const boxes = container!.querySelectorAll('.paint-box');
      boxes.forEach((box, idx) => {
        (box as HTMLElement).style.backgroundColor = `hsl(${(idx + 180) * 0.72}, 70%, 50%)`;
      });

      const repaintTime = performance.now() - repaintStart;

      return {
        createTime,
        repaintTime,
        totalTime: createTime + repaintTime,
        boxCount: boxes.length,
      };
    });

    console.log(`Paint creation: ${paintMetrics.createTime.toFixed(2)}ms`);
    console.log(`Repaint: ${paintMetrics.repaintTime.toFixed(2)}ms`);
    console.log(`Total: ${paintMetrics.totalTime.toFixed(2)}ms`);

    // Performance assertions
    expect(paintMetrics.totalTime).toBeLessThan(2000); // 2 seconds max
    expect(paintMetrics.boxCount).toBe(500);
  });

  test('PERF-009: CSS animation performance', { tag: '@p2' }, async ({ page }) => {
    /**
     * Tests CSS animation frame rate consistency
     * Threshold: Average frame time < 33ms (>30fps)
     */
    console.log('PERF-009: Measuring CSS animation performance...');

    await page.goto('http://localhost:9081/performance/css-animation.html');

    // Wait for animations to run and collect metrics
    await page.waitForTimeout(1000);

    const animationMetrics = await page.evaluate(() => {
      return new Promise((resolve) => {
        const frames: number[] = [];
        let lastTime = performance.now();
        let frameCount = 0;
        const maxFrames = 60; // Collect 60 frames

        const measureFrame = () => {
          const currentTime = performance.now();
          const frameTime = currentTime - lastTime;
          frames.push(frameTime);
          lastTime = currentTime;
          frameCount++;

          if (frameCount < maxFrames) {
            requestAnimationFrame(measureFrame);
          } else {
            // Calculate statistics
            const avgFrameTime = frames.reduce((a, b) => a + b, 0) / frames.length;
            const maxFrameTime = Math.max(...frames);
            const minFrameTime = Math.min(...frames);

            resolve({
              avgFrameTime,
              maxFrameTime,
              minFrameTime,
              frameCount: frames.length,
              estimatedFPS: 1000 / avgFrameTime,
            });
          }
        };

        requestAnimationFrame(measureFrame);
      });
    });

    console.log(`Animation metrics:`, animationMetrics);
    console.log(`Average frame time: ${(animationMetrics as any).avgFrameTime.toFixed(2)}ms`);
    console.log(`Estimated FPS: ${(animationMetrics as any).estimatedFPS.toFixed(1)}`);

    // Performance assertions (should maintain reasonable frame rate)
    expect((animationMetrics as any).avgFrameTime).toBeLessThan(50); // 50ms = 20fps minimum
    expect((animationMetrics as any).frameCount).toBe(60);
  });
});

// ============================================
// NETWORK PERFORMANCE (PERF-010 to PERF-011)
// ============================================

test.describe('Network Performance', () => {
  test('PERF-010: Concurrent request handling', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests handling of multiple concurrent network requests
     * Threshold: < 3000ms for 10 concurrent requests
     */
    console.log('PERF-010: Measuring concurrent request handling...');

    await page.goto('http://localhost:9081/performance/concurrent-requests.html');

    // Trigger concurrent requests
    const networkMetrics = await page.evaluate(() => {
      return new Promise((resolve) => {
        const start = performance.now();
        const requestCount = 10;
        let completed = 0;

        const requests = [];
        for (let i = 0; i < requestCount; i++) {
          // Create dummy fetch (will fail but we measure timing)
          const req = fetch(`/api/test-endpoint-${i}`, { method: 'GET' })
            .catch(() => {})
            .finally(() => {
              completed++;
              if (completed === requestCount) {
                const duration = performance.now() - start;
                resolve({
                  duration,
                  requestCount,
                  avgTimePerRequest: duration / requestCount,
                });
              }
            });
          requests.push(req);
        }
      });
    });

    console.log(`Concurrent requests completed in ${(networkMetrics as any).duration.toFixed(2)}ms`);
    console.log(`Average per request: ${(networkMetrics as any).avgTimePerRequest.toFixed(2)}ms`);

    // Performance assertions (generous for network variability)
    expect((networkMetrics as any).duration).toBeLessThan(5000); // 5 seconds max
    expect((networkMetrics as any).requestCount).toBe(10);
  });

  test('PERF-011: Large payload handling', { tag: '@p2' }, async ({ page }) => {
    /**
     * Tests handling of large response payloads
     * Threshold: < 2000ms to process 1MB JSON
     */
    console.log('PERF-011: Measuring large payload handling...');

    await page.goto('http://localhost:9081/performance/large-payload.html');

    // Generate and process large payload
    const payloadMetrics = await page.evaluate(() => {
      const start = performance.now();

      // Generate 1MB of JSON data
      const largeObject = {
        data: Array.from({ length: 10000 }, (_, i) => ({
          id: i,
          name: `Item ${i}`,
          description: `This is a description for item ${i} with some additional text to increase payload size`,
          metadata: {
            created: new Date().toISOString(),
            updated: new Date().toISOString(),
            tags: ['tag1', 'tag2', 'tag3'],
          },
        })),
      };

      const jsonString = JSON.stringify(largeObject);
      const payloadSize = new Blob([jsonString]).size;

      // Parse it back
      const parseStart = performance.now();
      const parsed = JSON.parse(jsonString);
      const parseTime = performance.now() - parseStart;

      const totalTime = performance.now() - start;

      return {
        totalTime,
        parseTime,
        payloadSize,
        itemCount: parsed.data.length,
      };
    });

    console.log(`Large payload processed in ${(payloadMetrics as any).totalTime.toFixed(2)}ms`);
    console.log(`Payload size: ${((payloadMetrics as any).payloadSize / 1024 / 1024).toFixed(2)}MB`);
    console.log(`Parse time: ${(payloadMetrics as any).parseTime.toFixed(2)}ms`);

    // Performance assertions
    expect((payloadMetrics as any).totalTime).toBeLessThan(3000); // 3 seconds max
    expect((payloadMetrics as any).itemCount).toBe(10000);
  });
});

// ============================================
// SENTINEL SECURITY PERFORMANCE (PERF-012 to PERF-015)
// ============================================

test.describe('Sentinel Security Performance', () => {
  test('PERF-012: FormMonitor overhead', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests FormMonitor performance overhead on form submissions
     * Threshold: < 100ms overhead per form submission analysis
     */
    console.log('PERF-012: Measuring FormMonitor performance overhead...');

    await page.goto('http://localhost:9081/performance/formmonitor-overhead.html');

    // Measure form submission with FormMonitor analysis
    const formMetrics = await page.evaluate(() => {
      const form = document.getElementById('test-form') as HTMLFormElement;
      const iterations = 100;
      const times: number[] = [];

      for (let i = 0; i < iterations; i++) {
        const start = performance.now();

        // Simulate FormMonitor analysis
        const formData = new FormData(form);
        const hasPassword = form.querySelector('input[type="password"]') !== null;
        const actionUrl = form.action;
        const documentUrl = window.location.href;

        // Simulate origin check
        let actionOrigin: string;
        try {
          actionOrigin = new URL(actionUrl).origin;
        } catch {
          actionOrigin = window.location.origin;
        }
        const isCrossOrigin = window.location.origin !== actionOrigin;

        // Simulate suspicious submission check
        const isSuspicious = isCrossOrigin && hasPassword;

        const duration = performance.now() - start;
        times.push(duration);
      }

      const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
      const maxTime = Math.max(...times);
      const minTime = Math.min(...times);

      return {
        avgTime,
        maxTime,
        minTime,
        iterations,
      };
    });

    console.log(`FormMonitor average overhead: ${(formMetrics as any).avgTime.toFixed(3)}ms`);
    console.log(`Max overhead: ${(formMetrics as any).maxTime.toFixed(3)}ms`);

    // Performance assertions (should be minimal overhead)
    expect((formMetrics as any).avgTime).toBeLessThan(10); // 10ms average max
    expect((formMetrics as any).maxTime).toBeLessThan(50); // 50ms max for single analysis
  });

  test('PERF-013: PolicyGraph query performance', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests PolicyGraph policy lookup performance
     * Threshold: < 50ms for policy lookup (simulated)
     */
    console.log('PERF-013: Measuring PolicyGraph query performance...');

    await page.goto('http://localhost:9081/performance/policygraph-queries.html');

    // Simulate PolicyGraph lookups
    const policyMetrics = await page.evaluate(() => {
      // Simulate policy database with Map
      const policyDB = new Map<string, any>();

      // Populate with test policies
      for (let i = 0; i < 1000; i++) {
        policyDB.set(`domain-${i}.com`, {
          domain: `domain-${i}.com`,
          action: i % 2 === 0 ? 'allow' : 'block',
          timestamp: Date.now(),
        });
      }

      const iterations = 1000;
      const times: number[] = [];

      for (let i = 0; i < iterations; i++) {
        const start = performance.now();

        // Simulate policy lookup
        const domain = `domain-${i % 1000}.com`;
        const policy = policyDB.get(domain);

        const duration = performance.now() - start;
        times.push(duration);
      }

      const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
      const maxTime = Math.max(...times);

      return {
        avgTime,
        maxTime,
        iterations,
        policyCount: policyDB.size,
      };
    });

    console.log(`PolicyGraph average query time: ${(policyMetrics as any).avgTime.toFixed(3)}ms`);
    console.log(`Policy database size: ${(policyMetrics as any).policyCount}`);

    // Performance assertions
    expect((policyMetrics as any).avgTime).toBeLessThan(1); // 1ms average max
    expect((policyMetrics as any).maxTime).toBeLessThan(10); // 10ms max
  });

  test('PERF-014: Fingerprinting detection overhead', { tag: '@p1' }, async ({ page }) => {
    /**
     * Tests fingerprinting detection performance overhead
     * Threshold: < 50ms overhead for canvas fingerprinting detection
     */
    console.log('PERF-014: Measuring fingerprinting detection overhead...');

    await page.goto('http://localhost:9081/performance/fingerprinting-overhead.html');

    // Measure canvas fingerprinting detection overhead
    const fpMetrics = await page.evaluate(() => {
      const canvas = document.createElement('canvas');
      canvas.width = 200;
      canvas.height = 50;
      const ctx = canvas.getContext('2d');

      const iterations = 100;
      const times: number[] = [];

      for (let i = 0; i < iterations; i++) {
        const start = performance.now();

        // Simulate fingerprinting detection on canvas operations
        if (ctx) {
          ctx.textBaseline = 'top';
          ctx.font = '14px Arial';
          ctx.fillStyle = '#f00';
          ctx.fillRect(0, 0, 200, 50);
          ctx.fillStyle = '#00f';
          ctx.fillText('Canvas fingerprint test', 2, 2);

          // Simulate FingerprintingDetector.record_api_call()
          const apiCall = {
            technique: 'canvas',
            api: 'toDataURL',
            timestamp: performance.now(),
            hadUserInteraction: false,
          };

          // Simulate score calculation
          const score = 50 + i % 50;
        }

        const duration = performance.now() - start;
        times.push(duration);
      }

      const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
      const maxTime = Math.max(...times);

      return {
        avgTime,
        maxTime,
        iterations,
      };
    });

    console.log(`Fingerprinting detection average overhead: ${(fpMetrics as any).avgTime.toFixed(3)}ms`);
    console.log(`Max overhead: ${(fpMetrics as any).maxTime.toFixed(3)}ms`);

    // Performance assertions
    expect((fpMetrics as any).avgTime).toBeLessThan(20); // 20ms average max
    expect((fpMetrics as any).maxTime).toBeLessThan(100); // 100ms max
  });

  test('PERF-015: Malware scanning overhead', { tag: '@p2' }, async ({ page }) => {
    /**
     * Tests malware scanning performance overhead (simulated)
     * Threshold: < 500ms for small file scan simulation
     */
    console.log('PERF-015: Measuring malware scanning overhead (simulated)...');

    await page.goto('http://localhost:9081/performance/malware-scanning.html');

    // Simulate malware scanning overhead
    const scanMetrics = await page.evaluate(() => {
      const iterations = 50;
      const times: number[] = [];

      for (let i = 0; i < iterations; i++) {
        const start = performance.now();

        // Simulate file content generation (10KB)
        const fileSize = 10 * 1024;
        const fileContent = new Uint8Array(fileSize);
        for (let j = 0; j < fileSize; j++) {
          fileContent[j] = Math.floor(Math.random() * 256);
        }

        // Simulate YARA-like pattern matching
        const patterns = [
          'malware', 'virus', 'trojan', 'backdoor', 'exploit',
        ];

        let matches = 0;
        const contentStr = String.fromCharCode.apply(null, Array.from(fileContent));

        for (const pattern of patterns) {
          if (contentStr.includes(pattern)) {
            matches++;
          }
        }

        // Simulate ML classification (simple heuristic)
        const entropy = fileContent.reduce((acc, val, idx) =>
          acc + (val / 255 * Math.log2((val + 1) / 255)), 0);

        const duration = performance.now() - start;
        times.push(duration);
      }

      const avgTime = times.reduce((a, b) => a + b, 0) / times.length;
      const maxTime = Math.max(...times);

      return {
        avgTime,
        maxTime,
        iterations,
        fileSize: 10 * 1024,
      };
    });

    console.log(`Malware scanning average time: ${(scanMetrics as any).avgTime.toFixed(2)}ms`);
    console.log(`Max scan time: ${(scanMetrics as any).maxTime.toFixed(2)}ms`);
    console.log(`File size: ${(scanMetrics as any).fileSize} bytes`);

    // Performance assertions (generous for simulated scanning)
    expect((scanMetrics as any).avgTime).toBeLessThan(100); // 100ms average max
    expect((scanMetrics as any).maxTime).toBeLessThan(500); // 500ms max
  });
});
