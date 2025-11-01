---
name: libweb-patterns
description: LibWeb patterns for HTML/CSS/DOM rendering engine implementation, including IDL bindings, layout algorithms, and spec compliance
use-when: Implementing or modifying LibWeb features, DOM nodes, CSS properties, layout algorithms, or WebIDL interfaces
allowed-tools:
  - Read
  - Write
  - Edit
  - Grep
  - Glob
  - Bash
---

# LibWeb Implementation Patterns

## Overview

LibWeb is Ladybird's HTML/CSS/DOM rendering engine, located in `Libraries/LibWeb/`. It implements web standards from WHATWG and W3C specifications.

### Architecture

```
Libraries/LibWeb/
├── DOM/              # DOM tree, nodes, elements, events
├── HTML/             # HTML elements, parsing, scripting
├── CSS/              # CSS parsing, selectors, cascade, computed styles
├── Layout/           # Layout tree, formatting contexts (BFC, IFC, FFC)
├── Painting/         # Paint tree, rendering, stacking contexts
├── Bindings/         # WebIDL → C++ bindings, GC integration
├── Fetch/            # Fetch API, HTTP requests
├── WebIDL/           # WebIDL infrastructure (ExceptionOr, etc.)
├── PerformanceTimeline/  # Performance APIs
└── [75+ other specs] # Each spec gets its own namespace
```

### Key Principles

1. **Spec Compliance First**: Match spec naming and algorithm steps exactly
2. **GC-Based Memory**: Everything extends `GC::Cell` hierarchy, not `RefCounted`
3. **Immutability**: Layout is immutable - create new `LayoutState`, don't mutate
4. **Error Handling**: Use `WebIDL::ExceptionOr<T>` for web APIs, `ErrorOr<T>` for OOM
5. **IDL-Driven**: Web APIs defined in `.idl` files, C++ generated automatically

## Pattern 1: DOM Node Implementation

### File Structure

For a new HTML element `HTMLFooElement`:

```
Libraries/LibWeb/HTML/
├── HTMLFooElement.idl    # WebIDL interface (must match spec)
├── HTMLFooElement.h      # C++ header
└── HTMLFooElement.cpp    # C++ implementation
```

### Basic DOM Element Pattern

```cpp
// HTMLFooElement.h
#pragma once

#include <LibWeb/HTML/HTMLElement.h>

namespace Web::HTML {

class HTMLFooElement : public HTMLElement {
    WEB_PLATFORM_OBJECT(HTMLFooElement, HTMLElement);
    GC_DECLARE_ALLOCATOR(HTMLFooElement);

public:
    virtual ~HTMLFooElement() override;

    // WebIDL reflected attributes
    String bar() const { return get_attribute(HTML::AttributeNames::bar).value_or(String {}); }
    void set_bar(String const& value) { MUST(set_attribute(HTML::AttributeNames::bar, value)); }

    // Spec algorithms (always link to spec)
    // https://example.spec.whatwg.org/#foo-activation
    virtual bool has_activation_behavior() const override { return true; }
    virtual void activation_behavior(DOM::Event const&) override;

protected:
    HTMLFooElement(DOM::Document&, DOM::QualifiedName);

    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Cell::Visitor&) override;

    // Lifecycle hooks
    virtual void attribute_changed(FlyString const& name, Optional<String> const& old_value,
                                   Optional<String> const& value, Optional<FlyString> const& namespace_) override;
    virtual void inserted() override;
    virtual void removed_from(Node* old_parent, Node& old_root) override;

private:
    // Member variables with m_ prefix
    GC::Ptr<SomeObject> m_internal_data;
};

}
```

```cpp
// HTMLFooElement.cpp
#include <LibWeb/Bindings/HTMLFooElementPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/HTMLFooElement.h>

namespace Web::HTML {

GC_DEFINE_ALLOCATOR(HTMLFooElement);

HTMLFooElement::HTMLFooElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
}

HTMLFooElement::~HTMLFooElement() = default;

void HTMLFooElement::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(HTMLFooElement);
}

void HTMLFooElement::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_internal_data);
}

// https://example.spec.whatwg.org/#foo-activation
void HTMLFooElement::activation_behavior(DOM::Event const& event)
{
    // 1. If this's disabled state is true, return.
    if (has_attribute(HTML::AttributeNames::disabled))
        return;

    // 2. Do the thing.
    // ... implementation ...
}

void HTMLFooElement::attribute_changed(FlyString const& name, Optional<String> const& old_value,
                                       Optional<String> const& value, Optional<FlyString> const& namespace_)
{
    Base::attribute_changed(name, old_value, value, namespace_);

    if (name == HTML::AttributeNames::bar) {
        // React to attribute change
        invalidate_style(DOM::StyleInvalidationReason::ElementAttributeChange);
    }
}

}
```

### Key Patterns

1. **GC_DECLARE_ALLOCATOR / GC_DEFINE_ALLOCATOR**: Required for GC objects
2. **WEB_PLATFORM_OBJECT macro**: Defines `realm()` and type checking
3. **WEB_SET_PROTOTYPE_FOR_INTERFACE**: Sets up JS prototype in `initialize()`
4. **visit_edges()**: Must visit all GC pointers for garbage collection
5. **Spec comments**: Every algorithm must link to spec section

## Pattern 2: WebIDL Interface Definition

### IDL File Structure

```webidl
// HTMLFooElement.idl
#import <HTML/HTMLElement.idl>

// https://example.spec.whatwg.org/#htmlfooelement
[Exposed=Window]
interface HTMLFooElement : HTMLElement {
    [HTMLConstructor] constructor();

    // Reflected attributes (automatically implemented)
    [Reflect, CEReactions] attribute DOMString bar;
    [Reflect=maxlength] attribute unsigned long maxLength;

    // Custom properties (need manual implementation)
    readonly attribute boolean isActive;

    // Methods
    undefined activate();
    Promise<undefined> doAsyncThing();
};
```

### IDL Attribute Patterns

```webidl
// Reflected attribute (maps to HTML attribute)
[Reflect] attribute DOMString foo;

// Reflected with different attribute name
[Reflect=maxlength] attribute unsigned long maxLength;

// Custom Element Reactions (triggers lifecycle callbacks)
[CEReactions] attribute DOMString bar;

// Nullable
attribute DOMString? optionalThing;

// Readonly
readonly attribute boolean computed;

// Enumerated attribute
[Enumerated=FooEnum] attribute DOMString type;

enum FooEnum {
    "option1",
    "option2",
    "option3"
};
```

### Registering IDL Files

1. Add to `Libraries/LibWeb/idl_files.cmake`:
```cmake
libweb_js_bindings(HTML/HTMLFooElement)
```

2. Forward declare in `Libraries/LibWeb/Forward.h`:
```cpp
namespace Web::HTML {
    class HTMLFooElement;
}
```

## Pattern 3: CSS Property Implementation

### Step 1: Define in Properties.json

```json
{
  "my-property": {
    "inherited": false,
    "initial": "auto",
    "animation-type": "discrete",
    "percentages": "relative to containing block width"
  }
}
```

### Step 2: Add Parsing (if custom)

```cpp
// CSS/Parser/PropertyParsing.cpp

RefPtr<CSS::StyleValue> Parser::parse_css_value(CSS::PropertyID property_id)
{
    switch (property_id) {
    case PropertyID::MyProperty:
        return parse_my_property_value(tokens);
    // ...
    }
}

RefPtr<CSS::StyleValue> Parser::parse_my_property_value(TokenStream<ComponentValue>& tokens)
{
    // https://example.spec.org/#my-property-parsing

    // 1. Try keyword values
    if (auto keyword = parse_keyword_value(tokens))
        return keyword;

    // 2. Try length/percentage
    if (auto length_percentage = parse_length_percentage(tokens))
        return length_percentage;

    return nullptr;
}
```

### Step 3: Add Computed Value Getter

```cpp
// CSS/ComputedValues.h

struct ComputedValues {
    // ...

    LengthPercentage const& my_property() const { return m_noninherited.my_property; }

private:
    struct Noninherited {
        LengthPercentage my_property { Length::make_auto() };
    };
};

struct MutableComputedValues {
    void set_my_property(LengthPercentage value) { m_noninherited.my_property = move(value); }
};

struct InitialValues {
    static LengthPercentage my_property() { return Length::make_auto(); }
};
```

### Step 4: Apply in Style Computation

```cpp
// NodeWithStyle::apply_style()

// Copy from StyleProperties to ComputedValues
computed_values.set_my_property(properties.my_property());
```

### Step 5: Use in Layout/Painting

```cpp
// Layout/BlockFormattingContext.cpp

void BlockFormattingContext::compute_width(Box const& box)
{
    auto const& computed_values = box.computed_values();
    auto my_property = computed_values.my_property();

    // Use the computed value
    if (my_property.is_auto()) {
        // ...
    }
}
```

## Pattern 4: Layout Node Implementation

### Layout Tree Structure

```
DOM Tree            Layout Tree           Paint Tree
────────            ───────────           ──────────
Document            InitialContainingBlock  PaintableBox
└── <div>           └── BlockContainer      └── PaintableBox
    ├── text            ├── TextNode            ├── PaintableWithLines
    └── <span>          └── InlineNode          └── PaintableBox
```

### Layout Node Pattern

```cpp
// Layout/MyLayoutNode.h

namespace Web::Layout {

class MyLayoutNode : public Box {
    GC_CELL(MyLayoutNode, Box);

public:
    MyLayoutNode(DOM::Document&, DOM::Node*, GC::Ref<CSS::ComputedProperties>);
    virtual ~MyLayoutNode() override;

    virtual GC::Ptr<Painting::Paintable> create_paintable() const override;

private:
    virtual bool is_my_layout_node() const final { return true; }
};

// Type checking helper
template<>
inline bool Node::fast_is<MyLayoutNode>() const { return is_my_layout_node(); }

}
```

### Formatting Context Pattern

```cpp
// Layout/MyFormattingContext.h

class MyFormattingContext {
public:
    explicit MyFormattingContext(LayoutState&, Box const& root);

    // Main entry point
    void run(Box const&, LayoutMode, AvailableSpace const&);

private:
    void layout_children(Box const&);
    void compute_width(Box const&);
    void compute_height(Box const&);

    LayoutState& m_state;
    Box const& m_root;
};
```

## Pattern 5: Event Handling

### Dispatching Events

```cpp
// https://dom.spec.whatwg.org/#dispatching-events

void HTMLFooElement::do_something()
{
    // 1. Create the event
    auto event = DOM::Event::create(realm(), HTML::EventNames::foo);

    // 2. Set event properties
    event->set_bubbles(true);
    event->set_cancelable(true);

    // 3. Dispatch the event
    dispatch_event(event);
}
```

### Event Handler Attributes

```cpp
// HTML element with onclick, etc.
class HTMLElement : public DOM::Element, public GlobalEventHandlers {
    // ...
};

// GlobalEventHandlers provides:
//   set_onclick(), set_onload(), etc.
```

### Adding Event Listeners

```cpp
// In C++ code
auto callback = GC::create_function_with_prototype(
    realm(),
    [](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        // Handle event
        return JS::js_undefined();
    },
    realm().intrinsics().function_prototype()
);

element->add_event_listener_without_options(
    HTML::EventNames::click,
    GC::Ref(*callback)
);
```

## Pattern 6: Spec Algorithm Implementation

### Golden Rule: Match Spec Exactly

```cpp
// ❌ WRONG - Arbitrary naming
bool HTMLInputElement::has_missing_constraint()
{
    return m_value.is_empty();
}

// ✅ CORRECT - Matches spec naming
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#suffering-from-being-missing
bool HTMLInputElement::suffering_from_being_missing()
{
    // 1. If this is not a control that can have a required attribute, return false.
    if (!can_have_required_attribute())
        return false;

    // 2. If this's required state is false, return false.
    if (!required())
        return false;

    // 3. If this's value is not empty, return false.
    if (!m_value.is_empty())
        return false;

    // 4. Return true.
    return true;
}
```

### Comment Style for Spec Steps

```cpp
// https://example.spec.whatwg.org/#algorithm-name
void do_spec_algorithm()
{
    // 1. Let result be null.
    GC::Ptr<Object> result;

    // 2. For each item in list:
    for (auto& item : list) {
        // 2.1. Do something with item.
        process(item);

        // 2.2. If condition holds, then:
        if (condition) {
            // 2.2.1. Set result to item.
            result = item;

            // 2.2.2. Break.
            break;
        }
    }

    // 3. Return result.
    return result;
}
```

### FIXME for Unimplemented Steps

```cpp
// https://example.spec.whatwg.org/#complex-algorithm
void complex_algorithm()
{
    // 1. Do step one.
    do_step_one();

    // FIXME: 2. Do step two (requires feature X).

    // 3. Do step three.
    do_step_three();
}
```

## Pattern 7: Error Handling

### WebIDL::ExceptionOr<T>

Used for all web API functions that can throw JS exceptions:

```cpp
// https://dom.spec.whatwg.org/#dom-node-appendchild
WebIDL::ExceptionOr<GC::Ref<Node>> Node::append_child(GC::Ref<Node> node)
{
    // 1. Return the result of pre-inserting node into this before null.
    return pre_insert(node, nullptr);
}
```

### Converting ErrorOr to ExceptionOr

```cpp
WebIDL::ExceptionOr<void> process_data()
{
    // TRY_OR_THROW_OOM converts ErrorOr<T> to ExceptionOr<T>
    auto buffer = TRY_OR_THROW_OOM(realm(), ByteBuffer::create_uninitialized(1024));

    // Use buffer...
    return {};
}
```

### SimpleException vs DOMException

```cpp
// Use SimpleException for built-in JS errors
if (index >= length)
    return WebIDL::SimpleException { WebIDL::SimpleExceptionType::RangeError, "Index out of range"sv };

// Use DOMException for web-specific errors
if (!is_valid_state())
    return WebIDL::DOMException::create(realm(), WebIDL::ExceptionCode::InvalidStateError, "Invalid state"_string);
```

## Pattern 8: GC and Memory Management

### Visit Edges Pattern

```cpp
class MyObject : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(MyObject, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(MyObject);

public:
    virtual void visit_edges(Cell::Visitor& visitor) override
    {
        Base::visit_edges(visitor);

        // Visit all GC pointers
        visitor.visit(m_element);
        visitor.visit(m_callback);

        // Visit GC pointers in containers
        for (auto& item : m_items)
            visitor.visit(item);
    }

private:
    GC::Ptr<DOM::Element> m_element;
    GC::Ptr<WebIDL::CallbackType> m_callback;
    Vector<GC::Ref<Node>> m_items;
};
```

### GC Types

```cpp
// Nullable reference
GC::Ptr<Node> m_parent;

// Non-null reference
GC::Ref<Document> m_document;

// Use ref() to convert Ptr to Ref when you know it's non-null
GC::Ref<Node> parent = *m_parent;

// Returning from factory
static GC::Ref<MyObject> create(JS::Realm& realm)
{
    return realm.create<MyObject>(realm);
}
```

## Common Pitfalls

### ❌ Pitfall 1: Forgetting visit_edges

```cpp
// ❌ MEMORY LEAK - GC pointer not visited
class BadClass : public Bindings::PlatformObject {
    GC::Ptr<Node> m_node;
    // Missing visit_edges override!
};

// ✅ CORRECT
class GoodClass : public Bindings::PlatformObject {
    virtual void visit_edges(Cell::Visitor& visitor) override {
        Base::visit_edges(visitor);
        visitor.visit(m_node);
    }
    GC::Ptr<Node> m_node;
};
```

### ❌ Pitfall 2: Using RefCounted Instead of GC

```cpp
// ❌ WRONG - RefCounted not compatible with LibWeb GC
class MyValue : public RefCounted<MyValue> {
};

// ✅ CORRECT - Use GC::Cell
class MyValue : public JS::Cell {
    GC_CELL(MyValue, JS::Cell);
};
```

### ❌ Pitfall 3: Mutating Layout Tree

```cpp
// ❌ WRONG - Don't mutate layout nodes
void compute_layout(Box& box)
{
    box.set_width(100);  // Mutation!
}

// ✅ CORRECT - Use immutable LayoutState
void compute_layout(Box const& box, LayoutState& state)
{
    state.get_mutable(box).set_content_width(100);
}
```

### ❌ Pitfall 4: Not Matching Spec Names

```cpp
// ❌ WRONG - Arbitrary naming
void updateStyleData() { }

// ✅ CORRECT - Match spec exactly
// https://dom.spec.whatwg.org/#update-style
void update_style() { }
```

### ❌ Pitfall 5: Missing Spec Links

```cpp
// ❌ WRONG - No spec link
void do_complex_thing() {
    // Implementation...
}

// ✅ CORRECT - Always link to spec
// https://html.spec.whatwg.org/multipage/parsing.html#reset-the-insertion-mode-appropriately
void reset_the_insertion_mode_appropriately() {
    // 1. Let last be false.
    // ...
}
```

## Testing Patterns

### LibWeb Test Types

1. **Text Tests**: Test JS APIs, compare output
   ```bash
   ./Meta/ladybird.py run test-web -- -f Text/input/my-test.html
   ```

2. **Layout Tests**: Compare layout tree structure
   ```bash
   ./Meta/ladybird.py run test-web -- -f Layout/my-test.html
   ```

3. **Ref Tests**: Compare screenshot against reference
   ```html
   <!DOCTYPE html>
   <link rel="match" href="my-test-ref.html">
   <div>Test content</div>
   ```

4. **Unit Tests**: C++ tests in `Services/*/Test*.cpp`
   ```cpp
   TEST_CASE(my_feature)
   {
       auto document = create_document();
       auto element = document->create_element(HTML::TagNames::div);
       EXPECT_EQ(element->tag_name(), "div");
   }
   ```

### Creating New Tests

```bash
# Add a text test
./Tests/LibWeb/add_libweb_test.py my-feature Text

# Add a layout test
./Tests/LibWeb/add_libweb_test.py my-feature Layout

# Add a ref test
./Tests/LibWeb/add_libweb_test.py my-feature Ref
```

## Checklist for LibWeb Implementation

When implementing a new LibWeb feature:

- [ ] Read the spec thoroughly
- [ ] Create `.idl` file matching spec exactly
- [ ] Add to `idl_files.cmake`
- [ ] Forward declare in `Forward.h`
- [ ] Create C++ header with `WEB_PLATFORM_OBJECT` macro
- [ ] Create C++ implementation with `GC_DEFINE_ALLOCATOR`
- [ ] Implement `initialize()` with `WEB_SET_PROTOTYPE_FOR_INTERFACE`
- [ ] Implement `visit_edges()` for all GC pointers
- [ ] Add spec comments for every algorithm
- [ ] Number spec steps in comments
- [ ] Use `WebIDL::ExceptionOr<T>` for web APIs
- [ ] Use `TRY_OR_THROW_OOM` for OOM conversion
- [ ] Match spec naming exactly
- [ ] Add tests (Text/Layout/Ref/Unit)
- [ ] Test with real web pages
- [ ] Check for memory leaks with sanitizers

## References

- **Spec Compliance**: Documentation/LibWebPatterns.md
- **CSS Properties**: Documentation/CSSProperties.md
- **IDL Files**: Documentation/AddNewIDLFile.md
- **Architecture**: Documentation/LibWebFromLoadingToPainting.md
- **Testing**: Documentation/Testing.md

## Key File Locations

- IDL files: `Libraries/LibWeb/[Module]/*.idl`
- CSS properties: `Libraries/LibWeb/CSS/Properties.json`
- Property parsing: `Libraries/LibWeb/CSS/Parser/PropertyParsing.cpp`
- Computed values: `Libraries/LibWeb/CSS/ComputedValues.h`
- Layout: `Libraries/LibWeb/Layout/`
- Painting: `Libraries/LibWeb/Painting/`
- Forward declarations: `Libraries/LibWeb/Forward.h`
- IDL registration: `Libraries/LibWeb/idl_files.cmake`

## Related Skills

### Prerequisite Skills
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Master C++ patterns before LibWeb development. Use ErrorOr<T>, smart pointers, and spec-matching function names.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Test LibWeb implementations against spec algorithms. Create Text, Layout, and Ref tests for new features.

### Complementary Skills
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Integrate LibWeb DOM with LibJS execution. Understand GC integration, wrapper objects, and JavaScript bindings for HTML/DOM APIs.
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: LibWeb runs in WebContent process. Understand process isolation and IPC communication with UI process.

### Build and Testing
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Add new LibWeb source files to CMake. Use ladybird_lib() for new components and compile IDL files.
- **[documentation-generation](../documentation-generation/SKILL.md)**: Document LibWeb APIs with spec links. Follow Doxygen patterns for Web API documentation.

### Debugging and Optimization
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug LibWeb crashes using GDB/LLDB. Attach to WebContent process and inspect DOM tree state.
- **[browser-performance](../browser-performance/SKILL.md)**: Optimize rendering pipeline performance. Profile layout, paint, and style computation hotspots.

### Quality Assurance
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Fuzz HTML parser, CSS parser, and DOM manipulation. Create fuzzer harnesses for LibWeb components.
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Run LibWeb tests in CI with sanitizers. Test across platforms and build configurations.
