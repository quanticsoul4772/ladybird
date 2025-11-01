# Playwright MCP Tools Reference

Complete reference for all available Playwright MCP tools.

## Navigation Tools

### mcp__playwright__browser_navigate
Navigate to a URL.

**Parameters:**
- `url` (string, required): The URL to navigate to

**Example:**
```javascript
await mcp__playwright__browser_navigate({
  url: "https://example.com"
});
```

### mcp__playwright__browser_navigate_back
Go back to the previous page in history.

**Parameters:** None

**Example:**
```javascript
await mcp__playwright__browser_navigate_back({});
```

## Page State Tools

### mcp__playwright__browser_snapshot
Capture accessibility snapshot of the current page. **This is the primary way to understand page state.**

**Parameters:** None

**Returns:** Structured accessibility tree with interactive elements

**Example:**
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});
console.log(snapshot);
```

**Best Practice:** Use this instead of screenshots for automation.

### mcp__playwright__browser_take_screenshot
Take a screenshot of the current page or element.

**Parameters:**
- `filename` (string, optional): File name for the screenshot
- `fullPage` (boolean, optional): Capture full scrollable page
- `type` (string, optional): "png" or "jpeg" (default: "png")
- `element` (string, optional): Human-readable element description
- `ref` (string, optional): Element reference from snapshot

**Example:**
```javascript
// Full page screenshot
await mcp__playwright__browser_take_screenshot({
  fullPage: true,
  filename: "page.png"
});

// Element screenshot
await mcp__playwright__browser_take_screenshot({
  element: "Submit button",
  ref: "button[type='submit']",
  filename: "button.png"
});
```

## Interaction Tools

### mcp__playwright__browser_click
Perform click on an element.

**Parameters:**
- `element` (string, required): Human-readable element description
- `ref` (string, required): Element reference from snapshot
- `button` (string, optional): "left", "right", or "middle" (default: "left")
- `doubleClick` (boolean, optional): Perform double-click
- `modifiers` (array, optional): ["Alt", "Control", "Meta", "Shift"]

**Example:**
```javascript
await mcp__playwright__browser_click({
  element: "Login button",
  ref: "button#login",
  button: "left"
});

// Double-click
await mcp__playwright__browser_click({
  element: "File item",
  ref: "div.file-item",
  doubleClick: true
});

// Click with modifier
await mcp__playwright__browser_click({
  element: "Link",
  ref: "a#link",
  modifiers: ["Control"]
});
```

### mcp__playwright__browser_type
Type text into an editable element.

**Parameters:**
- `element` (string, required): Human-readable element description
- `ref` (string, required): Element reference from snapshot
- `text` (string, required): Text to type
- `slowly` (boolean, optional): Type character-by-character
- `submit` (boolean, optional): Press Enter after typing

**Example:**
```javascript
await mcp__playwright__browser_type({
  element: "Username input",
  ref: "input[name='username']",
  text: "test_user"
});

// Type slowly (for autocomplete)
await mcp__playwright__browser_type({
  element: "Search box",
  ref: "input[type='search']",
  text: "query",
  slowly: true
});

// Type and submit
await mcp__playwright__browser_type({
  element: "Search input",
  ref: "input#search",
  text: "ladybird browser",
  submit: true
});
```

### mcp__playwright__browser_fill_form
Fill multiple form fields at once.

**Parameters:**
- `fields` (array, required): Array of field objects
  - `name` (string): Human-readable field name
  - `type` (string): "textbox", "checkbox", "radio", "combobox", "slider"
  - `ref` (string): Element reference
  - `value` (string): Value to fill

**Example:**
```javascript
await mcp__playwright__browser_fill_form({
  fields: [
    {
      name: "Username",
      type: "textbox",
      ref: "input[name='username']",
      value: "test_user"
    },
    {
      name: "Remember me",
      type: "checkbox",
      ref: "input#remember",
      value: "true"
    },
    {
      name: "Country",
      type: "combobox",
      ref: "select[name='country']",
      value: "United States"
    }
  ]
});
```

### mcp__playwright__browser_select_option
Select option(s) in a dropdown.

**Parameters:**
- `element` (string, required): Human-readable element description
- `ref` (string, required): Element reference
- `values` (array of strings, required): Options to select

**Example:**
```javascript
// Single select
await mcp__playwright__browser_select_option({
  element: "Country dropdown",
  ref: "select[name='country']",
  values: ["us"]
});

// Multi-select
await mcp__playwright__browser_select_option({
  element: "Tags selector",
  ref: "select#tags",
  values: ["tag1", "tag2", "tag3"]
});
```

### mcp__playwright__browser_hover
Hover over an element.

**Parameters:**
- `element` (string, required): Human-readable element description
- `ref` (string, required): Element reference

**Example:**
```javascript
await mcp__playwright__browser_hover({
  element: "Tooltip trigger",
  ref: "div.info-icon"
});
```

### mcp__playwright__browser_drag
Perform drag and drop between elements.

**Parameters:**
- `startElement` (string, required): Source element description
- `startRef` (string, required): Source element reference
- `endElement` (string, required): Target element description
- `endRef` (string, required): Target element reference

**Example:**
```javascript
await mcp__playwright__browser_drag({
  startElement: "Draggable item",
  startRef: "div#item1",
  endElement: "Drop zone",
  endRef: "div#dropzone"
});
```

### mcp__playwright__browser_press_key
Press a keyboard key.

**Parameters:**
- `key` (string, required): Key name or character

**Common Keys:**
- `"Enter"`, `"Escape"`, `"Tab"`, `"Backspace"`, `"Delete"`
- `"ArrowLeft"`, `"ArrowRight"`, `"ArrowUp"`, `"ArrowDown"`
- `"Home"`, `"End"`, `"PageUp"`, `"PageDown"`
- `"F1"` through `"F12"`
- Any character: `"a"`, `"A"`, `"1"`, etc.

**Example:**
```javascript
await mcp__playwright__browser_press_key({ key: "Enter" });
await mcp__playwright__browser_press_key({ key: "Escape" });
await mcp__playwright__browser_press_key({ key: "ArrowDown" });
```

## Waiting Tools

### mcp__playwright__browser_wait_for
Wait for text to appear/disappear or time to pass.

**Parameters (one required):**
- `text` (string, optional): Wait for text to appear
- `textGone` (string, optional): Wait for text to disappear
- `time` (number, optional): Wait for seconds

**Example:**
```javascript
// Wait for text
await mcp__playwright__browser_wait_for({
  text: "Loading complete"
});

// Wait for text to disappear
await mcp__playwright__browser_wait_for({
  textGone: "Loading..."
});

// Wait for time
await mcp__playwright__browser_wait_for({
  time: 2
});
```

## Tab Management Tools

### mcp__playwright__browser_tabs
List, create, close, or select browser tabs.

**Parameters:**
- `action` (string, required): "list", "new", "close", "select"
- `index` (number, optional): Tab index for close/select

**Example:**
```javascript
// List tabs
const tabs = await mcp__playwright__browser_tabs({ action: "list" });

// Create new tab
await mcp__playwright__browser_tabs({ action: "new" });

// Switch to tab
await mcp__playwright__browser_tabs({ action: "select", index: 0 });

// Close current tab
await mcp__playwright__browser_tabs({ action: "close" });

// Close specific tab
await mcp__playwright__browser_tabs({ action: "close", index: 1 });
```

## Debugging Tools

### mcp__playwright__browser_console_messages
Get console messages from the page.

**Parameters:**
- `onlyErrors` (boolean, optional): Only return error messages

**Returns:** Array of console message strings

**Example:**
```javascript
// All messages
const messages = await mcp__playwright__browser_console_messages({});

// Only errors
const errors = await mcp__playwright__browser_console_messages({
  onlyErrors: true
});

console.log(`Found ${errors.length} console errors`);
```

### mcp__playwright__browser_network_requests
Get all network requests since page load.

**Parameters:** None

**Returns:** Array of network request objects

**Example:**
```javascript
const requests = await mcp__playwright__browser_network_requests({});

const failed = requests.filter(r => r.status >= 400);
console.log(`Failed requests: ${failed.length}`);
```

### mcp__playwright__browser_evaluate
Execute JavaScript in the page context.

**Parameters:**
- `function` (string, required): JavaScript function to execute
- `element` (string, optional): Element description (for element scope)
- `ref` (string, optional): Element reference (for element scope)

**Example:**
```javascript
// Page-level evaluation
const title = await mcp__playwright__browser_evaluate({
  function: "() => document.title"
});

// Element-level evaluation
const canvasData = await mcp__playwright__browser_evaluate({
  element: "Canvas element",
  ref: "canvas#fingerprint",
  function: "(element) => element.toDataURL()"
});

// More complex evaluation
const metrics = await mcp__playwright__browser_evaluate({
  function: `() => {
    return {
      width: window.innerWidth,
      height: window.innerHeight,
      devicePixelRatio: window.devicePixelRatio
    };
  }`
});
```

### mcp__playwright__browser_handle_dialog
Handle alert/confirm/prompt dialogs.

**Parameters:**
- `accept` (boolean, required): Accept or dismiss dialog
- `promptText` (string, optional): Text for prompt dialogs

**Example:**
```javascript
// Accept alert
await mcp__playwright__browser_handle_dialog({ accept: true });

// Dismiss confirm
await mcp__playwright__browser_handle_dialog({ accept: false });

// Prompt with text
await mcp__playwright__browser_handle_dialog({
  accept: true,
  promptText: "user input"
});
```

## Advanced Tools

### mcp__playwright__browser_file_upload
Upload files through file chooser.

**Parameters:**
- `paths` (array of strings, optional): Absolute paths to files

**Example:**
```javascript
// Upload file
await mcp__playwright__browser_file_upload({
  paths: ["/home/user/document.pdf"]
});

// Upload multiple files
await mcp__playwright__browser_file_upload({
  paths: [
    "/home/user/file1.jpg",
    "/home/user/file2.jpg"
  ]
});

// Cancel file chooser
await mcp__playwright__browser_file_upload({ paths: [] });
```

### mcp__playwright__browser_resize
Resize the browser window.

**Parameters:**
- `width` (number, required): Width in pixels
- `height` (number, required): Height in pixels

**Example:**
```javascript
// Desktop size
await mcp__playwright__browser_resize({
  width: 1920,
  height: 1080
});

// Mobile size
await mcp__playwright__browser_resize({
  width: 375,
  height: 667
});
```

### mcp__playwright__browser_close
Close the browser page.

**Parameters:** None

**Example:**
```javascript
await mcp__playwright__browser_close({});
```

### mcp__playwright__browser_install
Install the browser (if not already installed).

**Parameters:** None

**Example:**
```javascript
await mcp__playwright__browser_install({});
```

## Best Practices

### 1. Always Use Snapshots for State
```javascript
// GOOD: Fast and reliable
const snapshot = await mcp__playwright__browser_snapshot({});
if (snapshot.includes("expected text")) { /* ... */ }

// AVOID: Slower, use only for visual regression
const screenshot = await mcp__playwright__browser_take_screenshot({});
```

### 2. Wait for Conditions, Not Time
```javascript
// GOOD: Deterministic
await mcp__playwright__browser_wait_for({ text: "Loaded" });

// AVOID: Flaky
await mcp__playwright__browser_wait_for({ time: 5 });
```

### 3. Use fill_form for Multiple Fields
```javascript
// GOOD: Single call for entire form
await mcp__playwright__browser_fill_form({ fields: [...] });

// AVOID: Multiple calls
await mcp__playwright__browser_type({ ref: "input1", text: "..." });
await mcp__playwright__browser_type({ ref: "input2", text: "..." });
```

### 4. Capture Evidence on Failure
```javascript
try {
  // Test logic
} catch (error) {
  await mcp__playwright__browser_take_screenshot({
    filename: "failure.png"
  });
  const errors = await mcp__playwright__browser_console_messages({
    onlyErrors: true
  });
  throw error;
}
```

### 5. Clean Up Tabs
```javascript
async function testWithCleanup() {
  await mcp__playwright__browser_tabs({ action: "new" });
  try {
    // Test logic
  } finally {
    await mcp__playwright__browser_tabs({ action: "close" });
  }
}
```

## Common Patterns

### Pattern: Form Fill and Submit
```javascript
await mcp__playwright__browser_fill_form({ fields: [...] });
await mcp__playwright__browser_click({
  element: "Submit",
  ref: "button[type='submit']"
});
await mcp__playwright__browser_wait_for({ text: "Success" });
```

### Pattern: Multi-Tab Workflow
```javascript
await mcp__playwright__browser_tabs({ action: "new" });
await mcp__playwright__browser_navigate({ url: "..." });
// ... work in new tab
await mcp__playwright__browser_tabs({ action: "select", index: 0 });
// ... back to first tab
```

### Pattern: Debugging Failed Test
```javascript
const snapshot = await mcp__playwright__browser_snapshot({});
const errors = await mcp__playwright__browser_console_messages({ onlyErrors: true });
const requests = await mcp__playwright__browser_network_requests({});
await mcp__playwright__browser_take_screenshot({ filename: "debug.png" });
```

### Pattern: Keyboard Navigation Test
```javascript
await mcp__playwright__browser_press_key({ key: "Tab" });
await mcp__playwright__browser_press_key({ key: "Tab" });
await mcp__playwright__browser_press_key({ key: "Enter" });
```
