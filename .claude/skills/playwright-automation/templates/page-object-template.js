/**
 * [Page Name] Page Object
 *
 * Encapsulates interactions with the [Page Name] page
 *
 * Usage:
 *   const page = new [PageName]Page();
 *   await page.navigate();
 *   await page.[action]();
 */

class [PageName]Page {
  constructor() {
    // Page URL
    this.url = "[page-url]";

    // Element selectors
    this.selectors = {
      // Add your selectors here
      // Example:
      // submitButton: "button[type='submit']",
      // usernameInput: "input[name='username']",
      // errorMessage: "div.error"
    };

    // Page state
    this.state = {
      loaded: false
    };
  }

  /**
   * Navigate to this page
   */
  async navigate() {
    await mcp__playwright__browser_navigate({ url: this.url });
    await this.waitForLoad();
  }

  /**
   * Wait for page to load
   */
  async waitForLoad() {
    await mcp__playwright__browser_wait_for({
      text: "[expected page content]"
    });
    this.state.loaded = true;
  }

  /**
   * Check if page is loaded
   */
  isLoaded() {
    return this.state.loaded;
  }

  /**
   * Click element by selector
   */
  async click(selectorKey, description) {
    if (!this.selectors[selectorKey]) {
      throw new Error(`Unknown selector: ${selectorKey}`);
    }

    await mcp__playwright__browser_click({
      element: description || selectorKey,
      ref: this.selectors[selectorKey]
    });
  }

  /**
   * Type into element
   */
  async type(selectorKey, text, description) {
    if (!this.selectors[selectorKey]) {
      throw new Error(`Unknown selector: ${selectorKey}`);
    }

    await mcp__playwright__browser_type({
      element: description || selectorKey,
      ref: this.selectors[selectorKey],
      text: text
    });
  }

  /**
   * Get text content from element (via snapshot)
   */
  async getText(selectorKey) {
    const snapshot = await mcp__playwright__browser_snapshot({});

    // Parse snapshot for element content
    // This is a simplified example
    const pattern = new RegExp(`${this.selectors[selectorKey]}[^>]*>([^<]+)<`);
    const match = snapshot.match(pattern);

    return match ? match[1] : null;
  }

  /**
   * Check if element exists
   */
  async hasElement(selectorKey) {
    const snapshot = await mcp__playwright__browser_snapshot({});
    return snapshot.includes(this.selectors[selectorKey]);
  }

  /**
   * Take screenshot of page
   */
  async takeScreenshot(filename) {
    await mcp__playwright__browser_take_screenshot({
      filename: filename || `${this.constructor.name}-${Date.now()}.png`
    });
  }

  /**
   * Example: Fill a form on this page
   */
  async fillForm(data) {
    const fields = [];

    // Build fields array from data
    for (const [key, value] of Object.entries(data)) {
      if (this.selectors[key]) {
        fields.push({
          name: key,
          type: "textbox",  // Adjust based on field type
          ref: this.selectors[key],
          value: value
        });
      }
    }

    await mcp__playwright__browser_fill_form({ fields });
  }

  /**
   * Example: Submit a form
   */
  async submit() {
    await this.click('submitButton', 'Submit button');
    await mcp__playwright__browser_wait_for({ time: 1 });
  }

  /**
   * Get page state snapshot
   */
  async getSnapshot() {
    return await mcp__playwright__browser_snapshot({});
  }
}

module.exports = [PageName]Page;
