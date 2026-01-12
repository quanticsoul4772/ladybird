# LibWeb Architecture Overview

## Directory Structure

LibWeb is organized by web specification, with each spec getting its own namespace and directory:

```
Libraries/LibWeb/
├── ARIA/                  # Accessible Rich Internet Applications
├── Animations/            # Web Animations API
├── Bindings/              # WebIDL → C++ binding infrastructure
├── CSS/                   # All CSS specifications
│   ├── Parser/            # CSS parsing (tokenizer, parser)
│   ├── StyleValues/       # CSS value types
│   └── Properties.json    # CSS property definitions
├── Clipboard/             # Clipboard API
├── Crypto/                # Web Crypto API
├── DOM/                   # DOM Core specification
│   ├── Node.h/cpp         # Base DOM node
│   ├── Element.h/cpp      # Base element
│   ├── Document.h/cpp     # Document node
│   └── Event*.h/cpp       # Event system
├── Encoding/              # Encoding specification
├── Fetch/                 # Fetch API
│   ├── Infrastructure/    # Fetch infrastructure
│   └── Fetching/          # Fetching algorithms
├── FileAPI/               # File API
├── Geometry/              # Geometry interfaces (DOMRect, etc.)
├── HTML/                  # HTML specification
│   ├── HTMLElement.h/cpp  # Base HTML element
│   ├── HTML*Element.*     # Specific HTML elements
│   ├── Parser/            # HTML parser
│   └── Scripting/         # Script execution
├── IntersectionObserver/  # Intersection Observer API
├── Layout/                # Layout algorithms
│   ├── Node.h/cpp         # Base layout node
│   ├── Box.h/cpp          # Layout box
│   ├── BlockFormattingContext.*
│   ├── InlineFormattingContext.*
│   └── FlexFormattingContext.*
├── Painting/              # Rendering
│   ├── Paintable.h/cpp    # Base paintable
│   ├── PaintableBox.*     # Box paintable
│   └── StackingContext.*  # Stacking contexts
├── PerformanceTimeline/   # Performance APIs
├── RequestIdleCallback/   # requestIdleCallback
├── ResizeObserver/        # Resize Observer API
├── SVG/                   # SVG specification
├── Selection/             # Selection API
├── Streams/               # Streams API
├── URL/                   # URL specification
├── WebAudio/              # Web Audio API
├── WebGL/                 # WebGL specification
├── WebIDL/                # WebIDL infrastructure
│   ├── ExceptionOr.h      # Exception handling
│   └── Types.h            # WebIDL types
├── WebSockets/            # WebSocket API
├── XHR/                   # XMLHttpRequest
└── Forward.h              # Forward declarations for all types
```

## Key Subsystems

### 1. HTML Parsing Pipeline

```
Raw bytes
    ↓
[Encoding Detection]
    ↓
Character stream
    ↓
[HTML Tokenizer] (HTML/Parser/HTMLTokenizer.*)
    ↓
Token stream
    ↓
[HTML Parser] (HTML/Parser/HTMLParser.*)
    ↓
DOM tree
```

Key files:
- `HTML/Parser/HTMLTokenizer.h/cpp` - Tokenizes HTML
- `HTML/Parser/HTMLParser.h/cpp` - Builds DOM tree from tokens
- `DOM/Document.h/cpp` - Document node
- `DOM/Node.h/cpp` - Base node class

### 2. CSS Processing Pipeline

```
CSS source
    ↓
[CSS Tokenizer] (CSS/Parser/Tokenizer.*)
    ↓
Token stream
    ↓
[CSS Parser] (CSS/Parser/Parser.*)
    ↓
CSSOM (CSS::StyleSheet)
    ↓
[Style Computer] (CSS/StyleComputer.*)
    ↓
Computed styles (CSS::ComputedValues)
```

Key files:
- `CSS/Parser/Tokenizer.h/cpp` - Tokenizes CSS
- `CSS/Parser/Parser.h/cpp` - Parses CSS into CSSOM
- `CSS/StyleComputer.h/cpp` - Computes styles for elements
- `CSS/ComputedValues.h` - Final computed CSS properties
- `CSS/Selector.h/cpp` - Selector matching

### 3. Layout Pipeline

```
DOM tree + Computed styles
    ↓
[Layout Tree Builder] (Layout/TreeBuilder.*)
    ↓
Layout tree (Layout::Node hierarchy)
    ↓
[Formatting Contexts] (Layout/*FormattingContext.*)
    ↓
LayoutState (final metrics)
    ↓
[Paintable Tree] (Painting/Paintable.*)
    ↓
Paint tree
```

Key files:
- `Layout/TreeBuilder.h/cpp` - Builds layout tree from DOM
- `Layout/Node.h/cpp` - Base layout node
- `Layout/Box.h/cpp` - Layout box with metrics
- `Layout/BlockFormattingContext.*` - Block layout
- `Layout/InlineFormattingContext.*` - Inline/text layout
- `Layout/FlexFormattingContext.*` - Flexbox layout
- `Painting/Paintable.h/cpp` - Base paintable class
- `Painting/PaintableBox.h/cpp` - Box painting

### 4. Rendering Pipeline

```
Paint tree
    ↓
[Stacking Context Tree] (Painting/StackingContext.*)
    ↓
Stacking contexts (z-order)
    ↓
[Painting] (Painting/PaintableBox::paint)
    ↓
Display commands
    ↓
[DisplayListRecorder] (Painting/DisplayListRecorder.*)
    ↓
Display list
    ↓
GPU/Canvas
```

Key files:
- `Painting/StackingContext.h/cpp` - Z-order management
- `Painting/PaintableBox.cpp::paint()` - Main paint method
- `Painting/DisplayListRecorder.h/cpp` - Records paint commands

### 5. JavaScript Integration

```
JS source
    ↓
[JS Parser] (LibJS/Parser/Parser.*)
    ↓
AST
    ↓
[Bytecode Compiler] (LibJS/Bytecode/Compiler.*)
    ↓
Bytecode
    ↓
[Bytecode Interpreter] (LibJS/Bytecode/Interpreter.*)
    ↓
Execution
```

WebIDL bindings:
- `.idl` files define web APIs
- `Meta/Lagom/Tools/CodeGenerators/LibWeb/BindingsGenerator/` generates C++ bindings
- `Bindings/PlatformObject.h` - Base class for all web objects
- `Bindings/Intrinsics.h` - JS intrinsic objects

### 6. Event System

```
Event trigger
    ↓
[Event Creation] (DOM/Event::create)
    ↓
Event object
    ↓
[Event Dispatch] (DOM/EventTarget::dispatch_event)
    ↓
Capture phase → Target → Bubble phase
    ↓
Event listeners
```

Key files:
- `DOM/Event.h/cpp` - Base event class
- `DOM/EventTarget.h/cpp` - Event dispatch
- `HTML/EventHandler.h/cpp` - Event handler attributes
- `UIEvents/` - Mouse, keyboard, touch events

## Memory Management

### Garbage Collection

LibWeb uses a precise garbage collector integrated with LibJS:

1. **All web objects inherit from `GC::Cell`**
   - `Bindings::PlatformObject` extends `GC::Cell`
   - `DOM::Node` extends `EventTarget` extends `PlatformObject`

2. **GC Pointer Types**
   - `GC::Ptr<T>` - Nullable GC pointer
   - `GC::Ref<T>` - Non-null GC pointer
   - `GC::Root<T>` - GC root (prevents collection)

3. **visit_edges Pattern**
   ```cpp
   virtual void visit_edges(Cell::Visitor& visitor) override
   {
       Base::visit_edges(visitor);
       visitor.visit(m_gc_pointer);
       // Visit all GC pointers!
   }
   ```

4. **Allocation**
   ```cpp
   auto obj = realm.create<MyObject>(realm, args...);
   ```

### Reference Counting (Limited Use)

Used only for non-GC types:
- `RefPtr<T>` / `NonnullRefPtr<T>` for `RefCounted<T>` types
- `StyleValue` and subclasses (CSS values)
- `Resource` (network resources)
- Some utility classes

## Web Standards Compliance

### Spec Comments Pattern

Every function implementing a spec algorithm must:

1. Link to spec section above function
2. Number each step
3. Use spec terminology exactly

Example:
```cpp
// https://dom.spec.whatwg.org/#dom-node-appendchild
WebIDL::ExceptionOr<GC::Ref<Node>> Node::append_child(GC::Ref<Node> node)
{
    // 1. Return the result of pre-inserting node into this before null.
    return pre_insert(node, nullptr);
}
```

### Spec Naming

Match spec names exactly:
- Function names: `suffering_from_being_missing()` not `has_missing_constraint()`
- Algorithms: `reset_the_insertion_mode_appropriately()` not `reset_insertion_mode()`
- Variables: `document` not `doc`, `element` not `elem`

## Performance Considerations

### Style Computation Optimization

1. **Selector Bucketing**: StyleComputer divides selectors by rightmost selector type (ID, class, tag)
2. **Bloom Filters**: Fast ancestor checking during selector matching
3. **Invalidation Sets**: Targeted style recomputation on changes

### Layout Optimization

1. **Immutable Layout**: LayoutState is created fresh, old state discarded
2. **Intrinsic Size Caching**: Cache min-content, max-content widths
3. **Partial Relayout**: Only relayout affected subtrees

### Rendering Optimization

1. **Display List Recording**: Record paint commands once, replay multiple times
2. **Damage Tracking**: Only repaint damaged areas
3. **Layer Composition**: Separate layers for transformed/animated content

## Testing Strategy

### Test Types

1. **Text Tests** (`Tests/LibWeb/Text/`)
   - Test JS APIs
   - Compare `println()` output to expectations
   - Fast, no rendering needed

2. **Layout Tests** (`Tests/LibWeb/Layout/`)
   - Compare layout tree structure
   - Verify box metrics
   - Test formatting contexts

3. **Ref Tests** (`Tests/LibWeb/Ref/`)
   - Screenshot comparison against reference HTML
   - Test visual rendering
   - Use `<link rel="match" href="reference.html">`

4. **Screenshot Tests** (`Tests/LibWeb/Screenshot/`)
   - Compare against reference image
   - Pixel-perfect rendering tests

5. **WPT Tests** (`Tests/LibWeb/WPT/`)
   - Web Platform Tests
   - Shared test suite across browsers

### Running Tests

```bash
# All LibWeb tests
./Meta/ladybird.py test LibWeb

# Specific test
./Meta/ladybird.py run test-web -- -f Text/input/my-test.html

# Rebaseline (update expectations)
./Meta/ladybird.py run test-web -- --rebaseline -f Text/input/my-test.html
```

## Key Design Principles

1. **Spec Compliance**: Match web standards exactly
2. **Type Safety**: Use strong types, avoid raw pointers
3. **GC Integration**: All web objects are GC'd
4. **Immutability**: Layout is immutable, create new state
5. **Error Handling**: Use ExceptionOr for web APIs
6. **Performance**: Profile-guided optimization
7. **Testing**: Comprehensive test coverage
8. **Documentation**: Spec links for all algorithms
