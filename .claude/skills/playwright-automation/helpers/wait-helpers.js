/**
 * Wait Helpers
 *
 * Smart waiting strategies for browser automation
 */

class WaitHelpers {
  /**
   * Wait for text to appear in page
   */
  static async waitForText(text, timeout = 10000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      const snapshot = await mcp__playwright__browser_snapshot({});

      if (snapshot.includes(text)) {
        return { found: true, elapsed: Date.now() - startTime };
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for text: "${text}" (${timeout}ms)`);
  }

  /**
   * Wait for text to disappear
   */
  static async waitForTextGone(text, timeout = 10000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      const snapshot = await mcp__playwright__browser_snapshot({});

      if (!snapshot.includes(text)) {
        return { gone: true, elapsed: Date.now() - startTime };
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for text to disappear: "${text}" (${timeout}ms)`);
  }

  /**
   * Wait for element to appear
   */
  static async waitForElement(selector, timeout = 10000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      const snapshot = await mcp__playwright__browser_snapshot({});

      if (snapshot.includes(selector)) {
        return { found: true, elapsed: Date.now() - startTime };
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for element: "${selector}" (${timeout}ms)`);
  }

  /**
   * Wait for page load complete
   */
  static async waitForPageLoad(expectedContent, timeout = 30000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      try {
        const snapshot = await mcp__playwright__browser_snapshot({});

        // Check for loading indicators
        const isLoading = snapshot.includes('loading') ||
                         snapshot.includes('spinner') ||
                         snapshot.includes('Loading...');

        // Check for expected content
        const hasContent = expectedContent ?
                          snapshot.includes(expectedContent) :
                          snapshot.length > 100;

        if (!isLoading && hasContent) {
          return { loaded: true, elapsed: Date.now() - startTime };
        }

      } catch (error) {
        // Page might still be loading
      }

      await mcp__playwright__browser_wait_for({ time: 1 });
    }

    throw new Error(`Timeout waiting for page load (${timeout}ms)`);
  }

  /**
   * Wait for network idle (no new requests)
   */
  static async waitForNetworkIdle(idleTime = 2000, timeout = 30000) {
    const startTime = Date.now();
    let lastRequestCount = 0;
    let idleStartTime = null;

    while (Date.now() - startTime < timeout) {
      const requests = await mcp__playwright__browser_network_requests({});
      const currentCount = requests.length;

      if (currentCount === lastRequestCount) {
        // No new requests
        if (!idleStartTime) {
          idleStartTime = Date.now();
        } else if (Date.now() - idleStartTime >= idleTime) {
          return {
            idle: true,
            elapsed: Date.now() - startTime,
            totalRequests: currentCount
          };
        }
      } else {
        // New request detected
        idleStartTime = null;
        lastRequestCount = currentCount;
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for network idle (${timeout}ms)`);
  }

  /**
   * Wait for console message
   */
  static async waitForConsoleMessage(messagePattern, timeout = 10000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      const messages = await mcp__playwright__browser_console_messages({});

      const found = messages.some(msg =>
        typeof messagePattern === 'string' ?
          msg.includes(messagePattern) :
          messagePattern.test(msg)
      );

      if (found) {
        return { found: true, elapsed: Date.now() - startTime };
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for console message (${timeout}ms)`);
  }

  /**
   * Retry function until it succeeds or times out
   */
  static async retry(fn, options = {}) {
    const maxAttempts = options.maxAttempts || 3;
    const delay = options.delay || 1000;
    const timeout = options.timeout || 30000;
    const startTime = Date.now();

    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        const result = await fn();
        return {
          success: true,
          result: result,
          attempts: attempt,
          elapsed: Date.now() - startTime
        };
      } catch (error) {
        if (attempt === maxAttempts || Date.now() - startTime >= timeout) {
          throw new Error(
            `Failed after ${attempt} attempts (${Date.now() - startTime}ms): ${error.message}`
          );
        }

        console.log(`  Attempt ${attempt} failed, retrying in ${delay}ms...`);
        await mcp__playwright__browser_wait_for({ time: delay / 1000 });
      }
    }
  }

  /**
   * Wait with exponential backoff
   */
  static async waitWithBackoff(checkFn, options = {}) {
    const maxAttempts = options.maxAttempts || 10;
    const initialDelay = options.initialDelay || 100;
    const maxDelay = options.maxDelay || 5000;
    const multiplier = options.multiplier || 2;

    let delay = initialDelay;

    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      const result = await checkFn();

      if (result) {
        return { success: true, attempts: attempt };
      }

      await mcp__playwright__browser_wait_for({ time: delay / 1000 });

      delay = Math.min(delay * multiplier, maxDelay);
    }

    throw new Error(`Condition not met after ${maxAttempts} attempts with backoff`);
  }

  /**
   * Wait for multiple conditions (all)
   */
  static async waitForAll(conditions, timeout = 30000) {
    const startTime = Date.now();
    const results = conditions.map(() => false);

    while (Date.now() - startTime < timeout) {
      for (let i = 0; i < conditions.length; i++) {
        if (!results[i]) {
          try {
            results[i] = await conditions[i]();
          } catch (error) {
            // Condition not met yet
          }
        }
      }

      if (results.every(r => r)) {
        return {
          success: true,
          elapsed: Date.now() - startTime
        };
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(
      `Timeout waiting for all conditions. ` +
      `Met: ${results.filter(r => r).length}/${results.length} (${timeout}ms)`
    );
  }

  /**
   * Wait for any condition (race)
   */
  static async waitForAny(conditions, timeout = 30000) {
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      for (let i = 0; i < conditions.length; i++) {
        try {
          const result = await conditions[i]();
          if (result) {
            return {
              success: true,
              conditionIndex: i,
              elapsed: Date.now() - startTime
            };
          }
        } catch (error) {
          // Continue to next condition
        }
      }

      await mcp__playwright__browser_wait_for({ time: 0.5 });
    }

    throw new Error(`Timeout waiting for any condition (${timeout}ms)`);
  }
}

module.exports = WaitHelpers;

// Example usage
if (require.main === module) {
  async function examples() {
    console.log('Wait Helpers Examples:\n');

    // Example 1: Wait for text
    console.log('1. Waiting for text to appear');
    // await WaitHelpers.waitForText('Welcome');

    // Example 2: Retry with backoff
    console.log('2. Retry with exponential backoff');
    // await WaitHelpers.waitWithBackoff(async () => {
    //   const snapshot = await mcp__playwright__browser_snapshot({});
    //   return snapshot.includes('loaded');
    // });

    // Example 3: Wait for multiple conditions
    console.log('3. Wait for all conditions');
    // await WaitHelpers.waitForAll([
    //   async () => { /* check condition 1 */ return true; },
    //   async () => { /* check condition 2 */ return true; }
    // ]);

    console.log('\nAll examples documented');
  }

  examples();
}
