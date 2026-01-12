/**
 * [Fixture Name] Fixture
 *
 * Provides reusable setup and teardown for [purpose]
 *
 * Usage:
 *   const fixture = new [FixtureName]();
 *   await fixture.setup();
 *   // ... run tests
 *   await fixture.teardown();
 */

class [FixtureName]Fixture {
  constructor(options = {}) {
    this.options = {
      // Default options
      ...options
    };

    this.state = {
      // Track fixture state
      initialized: false
    };
  }

  /**
   * Setup fixture before tests
   */
  async setup() {
    console.log(`[${this.constructor.name}] Setting up...`);

    // Navigate to starting point
    await mcp__playwright__browser_navigate({
      url: this.options.baseURL || "https://example.com"
    });

    // Wait for page ready
    await mcp__playwright__browser_wait_for({ time: 1 });

    // Perform setup actions
    // ... add your setup logic here

    this.state.initialized = true;
    console.log(`[${this.constructor.name}] Setup complete`);
  }

  /**
   * Teardown fixture after tests
   */
  async teardown() {
    console.log(`[${this.constructor.name}] Tearing down...`);

    // Cleanup actions
    // ... add your cleanup logic here

    this.state.initialized = false;
    console.log(`[${this.constructor.name}] Teardown complete`);
  }

  /**
   * Helper method example
   */
  async helperMethod() {
    if (!this.state.initialized) {
      throw new Error("Fixture not initialized. Call setup() first.");
    }

    // ... helper logic
  }

  /**
   * Get fixture state
   */
  getState() {
    return { ...this.state };
  }
}

module.exports = [FixtureName]Fixture;
