# LibWeb Test Types - When to Use Each

## Decision Tree

```
Does the feature have visual output?
├─ NO → Use Text test
│         (Tests JavaScript APIs, DOM manipulation)
│
└─ YES → Is it testing layout structure?
          ├─ YES → Use Layout test
          │         (Tests box tree, positioning, sizing)
          │
          └─ NO → Can you replicate visuals in HTML/CSS?
                    ├─ YES → Use Ref test
                    │         (Compare against reference HTML)
                    │
                    └─ NO → Use Screenshot test
                              (Compare against reference image)
```

## Test Type Comparison

| Type | Purpose | Output | Rebaseline | Use When |
|------|---------|--------|------------|----------|
| **Text** | API behavior | Text output | ✅ Easy | Testing Web APIs, DOM manipulation, event handling |
| **Layout** | Layout tree | Layout dump | ✅ Easy | Testing CSS layout, box positioning, sizing |
| **Ref** | Visual rendering | Screenshot | ❌ Manual | Visual effects with HTML reference |
| **Screenshot** | Visual rendering | Screenshot | ❌ Manual | Complex visuals (Canvas, WebGL, SVG) |

## Text Tests

### When to Use

✅ **Use Text tests for**:
- JavaScript API testing
- DOM manipulation (createElement, appendChild, etc.)
- Event handling (addEventListener, dispatchEvent)
- Attribute manipulation (getAttribute, setAttribute)
- Computed style queries (getComputedStyle)
- Form element behavior (input values, validation)
- API behavior without visual impact
- Spec algorithm verification

❌ **Don't use Text tests for**:
- Visual rendering
- Layout positioning
- CSS effects (shadows, gradients)
- Canvas/WebGL rendering

### Examples

**Good Text test candidates**:
```javascript
// DOM API
test(() => {
    const div = document.createElement("div");
    div.setAttribute("id", "test");
    println(`ID: ${div.id}`); // Testing IDL attribute reflection
});

// Events
test(() => {
    let fired = false;
    document.addEventListener("click", () => { fired = true; });
    document.dispatchEvent(new Event("click"));
    println(`Event fired: ${fired}`);
});

// Computed styles (values, not rendering)
test(() => {
    const div = document.querySelector(".test");
    const style = getComputedStyle(div);
    println(`Color: ${style.color}`); // "rgb(255, 0, 0)"
});
```

### Rebaselining

```bash
# Easy - just rerun with --rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-test.html
```

## Layout Tests

### When to Use

✅ **Use Layout tests for**:
- CSS box model (margin, border, padding)
- Flexbox layout
- Grid layout
- Absolute/relative positioning
- Float behavior
- Block formatting contexts
- Table layout
- Scrollable overflow
- Layout tree structure

❌ **Don't use Layout tests for**:
- Visual effects (colors, shadows, gradients)
- Exact pixel rendering
- Canvas/SVG content
- Font rendering details

### Examples

**Good Layout test candidates**:

```html
<!-- Flexbox layout -->
<style>
    .container {
        display: flex;
        width: 400px;
    }
    .item { flex: 1; }
</style>
<div class="container">
    <div class="item">A</div>
    <div class="item">B</div>
</div>
```

**Output shows**:
- Box sizes (width, height)
- Positions (x, y coordinates)
- Layout properties (flex-container, flex-item)
- Box tree structure

**What Layout tests DON'T show**:
- Actual colors/backgrounds
- Text rendering
- Border styles (only sizes)
- Visual effects

### Rebaselining

```bash
# Easy - just rerun with --rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Layout/input/my-test.html
```

## Ref Tests

### When to Use

✅ **Use Ref tests for**:
- Visual effects achievable with simpler CSS
- Box shadows
- Border radius
- Gradients (if reference can match)
- CSS filters
- Opacity/blending
- Z-index stacking
- Pseudo-elements
- CSS transforms (visual)

❌ **Don't use Ref tests for**:
- Canvas rendering (hard to replicate)
- WebGL output (impossible to replicate)
- Complex SVG (hard to replicate)
- Text rendering edge cases (font-dependent)

### Examples

**Good Ref test candidates**:

```html
<!-- Test: box-shadow -->
<link rel="match" href="../expected/box-shadow-ref.html" />
<style>
    .box {
        width: 100px;
        height: 100px;
        box-shadow: 10px 10px 20px rgba(0,0,0,0.5);
    }
</style>
<div class="box"></div>
```

**Reference page** (simpler technique):
```html
<!-- Reference: simulate shadow with positioned div -->
<style>
    .box {
        width: 100px;
        height: 100px;
        position: relative;
    }
    .box::after {
        content: "";
        position: absolute;
        width: 100px;
        height: 100px;
        background: rgba(0,0,0,0.3);
        filter: blur(10px);
        z-index: -1;
    }
</style>
<div class="box"></div>
```

### Creating Ref Tests

```bash
# 1. Create test
./Tests/LibWeb/add_libweb_test.py box-shadow Ref

# 2. Edit test page
vim Tests/LibWeb/Ref/input/box-shadow.html

# 3. Edit reference page (manually!)
vim Tests/LibWeb/Ref/expected/box-shadow-ref.html

# 4. Run test
./Meta/ladybird.py run test-web -f Ref/input/box-shadow.html
```

**Note**: Cannot auto-rebaseline Ref tests - must manually create reference.

## Screenshot Tests

### When to Use

✅ **Use Screenshot tests for**:
- Canvas 2D rendering
- WebGL output
- Complex SVG that's hard to replicate
- Image rendering edge cases
- Pixel-perfect rendering verification

❌ **Don't use Screenshot tests for**:
- Anything a Ref test can handle
- Platform-dependent rendering (fonts, anti-aliasing)
- Simple visual effects

⚠️ **Screenshot test caveats**:
- Fragile to small rendering changes
- Platform-dependent (fonts, anti-aliasing)
- Harder to maintain than Ref tests
- Last resort option

### Examples

**Good Screenshot test candidates**:

```html
<!-- Canvas rendering -->
<link rel="match" href="../expected/canvas-rect-ref.html" />
<canvas id="c" width="200" height="200"></canvas>
<script>
    const ctx = c.getContext("2d");
    ctx.fillStyle = "red";
    ctx.fillRect(50, 50, 100, 100);
</script>
```

**Reference page**:
```html
<style>
    * { margin: 0; }
    body { background: white; }
</style>
<img src="../images/canvas-rect-ref.png">
```

### Creating Screenshot Tests

```bash
# 1. Create test
./Tests/LibWeb/add_libweb_test.py canvas-rendering Screenshot

# 2. Edit test page
vim Tests/LibWeb/Screenshot/input/canvas-rendering.html

# 3. Generate reference screenshot (headless mode)
./Meta/ladybird.py run ladybird --headless --layout-test-mode \
    Tests/LibWeb/Screenshot/input/canvas-rendering.html

# 4. Move screenshot to images directory
mv ~/Downloads/screenshot-*.png \
    Tests/LibWeb/Screenshot/images/canvas-rendering-ref.png

# 5. Create reference HTML page
vim Tests/LibWeb/Screenshot/expected/canvas-rendering-ref.html

# 6. Run test
./Meta/ladybird.py run test-web -f Screenshot/input/canvas-rendering.html
```

## Detailed Comparison

### Text vs Layout

**When in doubt between Text and Layout**:

```html
<!-- Testing computed style VALUE → Text test -->
<script src="include.js"></script>
<style> .box { color: red; } </style>
<script>
test(() => {
    const style = getComputedStyle(document.querySelector(".box"));
    println(`Color: ${style.color}`); // Testing the VALUE
});
</script>

<!-- Testing layout STRUCTURE → Layout test -->
<style> .box { width: 100px; height: 100px; } </style>
<div class="box"></div>
<!-- Output shows box in layout tree with size/position -->
```

### Layout vs Ref

**When in doubt between Layout and Ref**:

```html
<!-- Testing box POSITIONS → Layout test -->
<style>
    .container { position: relative; }
    .child { position: absolute; top: 50px; left: 50px; }
</style>
<div class="container">
    <div class="child"></div>
</div>
<!-- Layout shows exact positions: child at (50, 50) -->

<!-- Testing VISUAL appearance → Ref test -->
<style>
    .box { box-shadow: 5px 5px 10px black; }
</style>
<div class="box"></div>
<!-- Ref test compares screenshot against reference -->
```

### Ref vs Screenshot

**When in doubt between Ref and Screenshot**:

```html
<!-- Can replicate in HTML → Ref test -->
<style>
    .gradient {
        background: linear-gradient(red, blue);
    }
</style>
<!-- Reference: use solid color approximation or simpler gradient -->

<!-- Cannot replicate in HTML → Screenshot test -->
<canvas id="c"></canvas>
<script>
    const ctx = c.getContext("2d");
    // Complex canvas drawing
</script>
<!-- Reference: PNG screenshot of expected output -->
```

## Test Type Selection Examples

### Example 1: Testing Element.setAttribute()

```
Feature: Element.setAttribute(name, value)
Visual output? NO
→ Use Text test

test(() => {
    const div = document.createElement("div");
    div.setAttribute("id", "test");
    println(`ID: ${div.id}`);
});
```

### Example 2: Testing Flexbox Layout

```
Feature: CSS Flexbox
Visual output? YES
Testing layout structure? YES
→ Use Layout test

<style>
    .container {
        display: flex;
        width: 400px;
    }
    .item { flex: 1; }
</style>
<div class="container">
    <div class="item">A</div>
    <div class="item">B</div>
</div>
```

### Example 3: Testing Box Shadow

```
Feature: box-shadow
Visual output? YES
Testing layout structure? NO
Can replicate in HTML? YES (with positioned div + blur)
→ Use Ref test

<!-- Test -->
<link rel="match" href="../expected/box-shadow-ref.html" />
<style> .box { box-shadow: 5px 5px 10px black; } </style>
<div class="box"></div>

<!-- Reference -->
<style> .box { /* simpler technique */ } </style>
<div class="box"></div>
```

### Example 4: Testing Canvas Rendering

```
Feature: Canvas 2D API
Visual output? YES
Testing layout structure? NO
Can replicate in HTML? NO (complex canvas drawing)
→ Use Screenshot test

<canvas id="c"></canvas>
<script>
    const ctx = c.getContext("2d");
    ctx.fillRect(0, 0, 100, 100);
    // Complex drawing operations
</script>

<!-- Reference: screenshot PNG -->
```

## Quick Decision Chart

| What are you testing? | Test Type |
|----------------------|-----------|
| DOM API | Text |
| Event handling | Text |
| JavaScript behavior | Text |
| IDL attribute reflection | Text |
| Computed style **values** | Text |
| Box model (size, position) | Layout |
| Flexbox/Grid layout | Layout |
| Positioning (absolute/relative) | Layout |
| Float behavior | Layout |
| Box shadow | Ref |
| Border radius | Ref |
| Gradients (simple) | Ref |
| CSS transforms (visual) | Ref |
| Opacity/blending | Ref |
| Canvas rendering | Screenshot |
| WebGL output | Screenshot |
| Complex SVG | Screenshot |

## Common Mistakes

### ❌ Using Text test for visual rendering

```javascript
// WRONG: Can't test visual shadow with Text test
test(() => {
    const div = document.querySelector(".shadowed");
    // How do you verify the shadow looks right?
    println("Shadow exists?"); // This doesn't test the visual!
});
```

Use Ref or Screenshot test instead.

### ❌ Using Layout test for visual effects

```html
<!-- WRONG: Layout test won't show gradient rendering -->
<style>
    .gradient {
        background: linear-gradient(red, blue);
    }
</style>
<div class="gradient"></div>
<!-- Layout dump won't show the gradient! -->
```

Use Ref test instead.

### ❌ Using Screenshot when Ref would work

```html
<!-- WRONG: Overusing Screenshot test -->
<style>
    .box { border-radius: 10px; }
</style>
<div class="box"></div>
<!-- This can be a Ref test with HTML reference! -->
```

Use Ref test with simple HTML reference instead.

### ✅ Correct approach

```html
<!-- RIGHT: Ref test with HTML reference -->
<link rel="match" href="../expected/border-radius-ref.html" />
<style>
    .box {
        width: 100px;
        height: 100px;
        border-radius: 10px;
        background: red;
    }
</style>
<div class="box"></div>

<!-- Reference: Same styling, ensures consistent rendering -->
```

## Summary

1. **Text tests** - API behavior, no visual output
2. **Layout tests** - Box positions and sizes, layout tree structure
3. **Ref tests** - Visual rendering with HTML reference
4. **Screenshot tests** - Complex visuals (Canvas, WebGL)

**Rule of thumb**: Use the simplest test type that verifies what you need.

**Priority**: Text > Layout > Ref > Screenshot
