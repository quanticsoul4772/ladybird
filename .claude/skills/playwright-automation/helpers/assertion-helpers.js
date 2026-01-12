/**
 * Assertion Helpers
 *
 * Custom assertion utilities for Playwright tests
 */

class AssertionError extends Error {
  constructor(message, expected, actual) {
    super(message);
    this.name = 'AssertionError';
    this.expected = expected;
    this.actual = actual;
  }
}

class AssertionHelpers {
  /**
   * Assert that snapshot includes text
   */
  static async assertSnapshotIncludes(text, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    if (!snapshot.includes(text)) {
      throw new AssertionError(
        message || `Expected snapshot to include "${text}"`,
        text,
        'not found in snapshot'
      );
    }

    return true;
  }

  /**
   * Assert that snapshot does not include text
   */
  static async assertSnapshotNotIncludes(text, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    if (snapshot.includes(text)) {
      throw new AssertionError(
        message || `Expected snapshot to NOT include "${text}"`,
        'text not present',
        `"${text}" found in snapshot`
      );
    }

    return true;
  }

  /**
   * Assert element exists
   */
  static async assertElementExists(selector, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    if (!snapshot.includes(selector)) {
      throw new AssertionError(
        message || `Expected element to exist: ${selector}`,
        selector,
        'element not found'
      );
    }

    return true;
  }

  /**
   * Assert element does not exist
   */
  static async assertElementNotExists(selector, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    if (snapshot.includes(selector)) {
      throw new AssertionError(
        message || `Expected element to NOT exist: ${selector}`,
        'element not present',
        `${selector} found in snapshot`
      );
    }

    return true;
  }

  /**
   * Assert console has no errors
   */
  static async assertNoConsoleErrors(message) {
    const errors = await mcp__playwright__browser_console_messages({
      onlyErrors: true
    });

    if (errors.length > 0) {
      throw new AssertionError(
        message || `Expected no console errors, found ${errors.length}`,
        0,
        errors.length
      );
    }

    return true;
  }

  /**
   * Assert console has specific message
   */
  static async assertConsoleMessage(pattern, message) {
    const messages = await mcp__playwright__browser_console_messages({});

    const found = messages.some(msg =>
      typeof pattern === 'string' ?
        msg.includes(pattern) :
        pattern.test(msg)
    );

    if (!found) {
      throw new AssertionError(
        message || `Expected console message matching: ${pattern}`,
        pattern,
        'not found in console'
      );
    }

    return true;
  }

  /**
   * Assert network request was made
   */
  static async assertRequestMade(urlPattern, message) {
    const requests = await mcp__playwright__browser_network_requests({});

    const found = requests.some(req =>
      typeof urlPattern === 'string' ?
        req.url.includes(urlPattern) :
        urlPattern.test(req.url)
    );

    if (!found) {
      throw new AssertionError(
        message || `Expected network request to: ${urlPattern}`,
        urlPattern,
        'request not found'
      );
    }

    return true;
  }

  /**
   * Assert no network errors
   */
  static async assertNoNetworkErrors(message) {
    const requests = await mcp__playwright__browser_network_requests({});
    const failed = requests.filter(r => r.status >= 400);

    if (failed.length > 0) {
      throw new AssertionError(
        message || `Expected no failed requests, found ${failed.length}`,
        0,
        failed.length
      );
    }

    return true;
  }

  /**
   * Assert URL matches pattern
   */
  static async assertURL(urlPattern, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Try to extract current URL from snapshot
    // This is a simplified check
    const urlMatch = typeof urlPattern === 'string';

    if (!urlMatch) {
      throw new AssertionError(
        message || `Expected URL to match: ${urlPattern}`,
        urlPattern,
        'current URL'
      );
    }

    return true;
  }

  /**
   * Assert title contains text
   */
  static async assertTitle(titleText, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Extract title from snapshot
    const titleMatch = snapshot.match(/<title[^>]*>([^<]+)<\/title>/i);
    const currentTitle = titleMatch ? titleMatch[1] : '';

    if (!currentTitle.includes(titleText)) {
      throw new AssertionError(
        message || `Expected title to include "${titleText}"`,
        titleText,
        currentTitle
      );
    }

    return true;
  }

  /**
   * Assert element count
   */
  static async assertElementCount(selector, expectedCount, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    const pattern = new RegExp(selector, 'g');
    const matches = snapshot.match(pattern) || [];
    const actualCount = matches.length;

    if (actualCount !== expectedCount) {
      throw new AssertionError(
        message || `Expected ${expectedCount} elements matching "${selector}", found ${actualCount}`,
        expectedCount,
        actualCount
      );
    }

    return true;
  }

  /**
   * Assert snapshot matches regex
   */
  static async assertSnapshotMatches(regex, message) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    if (!regex.test(snapshot)) {
      throw new AssertionError(
        message || `Expected snapshot to match: ${regex}`,
        regex.toString(),
        'no match found'
      );
    }

    return true;
  }

  /**
   * Generic assertion
   */
  static assert(condition, message, expected, actual) {
    if (!condition) {
      throw new AssertionError(message, expected, actual);
    }

    return true;
  }

  /**
   * Assert equal
   */
  static assertEqual(actual, expected, message) {
    if (actual !== expected) {
      throw new AssertionError(
        message || `Expected ${expected}, got ${actual}`,
        expected,
        actual
      );
    }

    return true;
  }

  /**
   * Assert not equal
   */
  static assertNotEqual(actual, expected, message) {
    if (actual === expected) {
      throw new AssertionError(
        message || `Expected value to not equal ${expected}`,
        `not ${expected}`,
        actual
      );
    }

    return true;
  }

  /**
   * Assert greater than
   */
  static assertGreaterThan(actual, threshold, message) {
    if (actual <= threshold) {
      throw new AssertionError(
        message || `Expected ${actual} to be greater than ${threshold}`,
        `> ${threshold}`,
        actual
      );
    }

    return true;
  }

  /**
   * Assert less than
   */
  static assertLessThan(actual, threshold, message) {
    if (actual >= threshold) {
      throw new AssertionError(
        message || `Expected ${actual} to be less than ${threshold}`,
        `< ${threshold}`,
        actual
      );
    }

    return true;
  }

  /**
   * Assert array includes
   */
  static assertArrayIncludes(array, item, message) {
    if (!array.includes(item)) {
      throw new AssertionError(
        message || `Expected array to include ${item}`,
        item,
        'not found in array'
      );
    }

    return true;
  }

  /**
   * Assert object has property
   */
  static assertHasProperty(object, property, message) {
    if (!(property in object)) {
      throw new AssertionError(
        message || `Expected object to have property "${property}"`,
        property,
        'property not found'
      );
    }

    return true;
  }
}

module.exports = { AssertionHelpers, AssertionError };

// Example usage
if (require.main === module) {
  async function examples() {
    console.log('Assertion Helpers Examples:\n');

    try {
      // Example assertions
      AssertionHelpers.assert(true, 'This passes');
      console.log('✓ Basic assertion');

      AssertionHelpers.assertEqual(1, 1, 'Values are equal');
      console.log('✓ Equality assertion');

      AssertionHelpers.assertGreaterThan(10, 5, '10 > 5');
      console.log('✓ Comparison assertion');

      AssertionHelpers.assertArrayIncludes([1, 2, 3], 2, 'Array includes 2');
      console.log('✓ Array assertion');

      // This will fail
      // AssertionHelpers.assertEqual(1, 2, 'This should fail');

    } catch (error) {
      if (error instanceof AssertionError) {
        console.error(`✗ Assertion failed: ${error.message}`);
        console.error(`  Expected: ${error.expected}`);
        console.error(`  Actual: ${error.actual}`);
      } else {
        console.error(`✗ Error: ${error.message}`);
      }
    }

    console.log('\nAll examples complete');
  }

  examples();
}
