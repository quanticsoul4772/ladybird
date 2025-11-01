# Common LibWeb APIs Reference

Quick reference for frequently used APIs when implementing LibWeb features.

## DOM Manipulation

### Creating Elements

```cpp
// Create element with tag name
auto element = document->create_element(HTML::TagNames::div).release_value();

// Create element with qualified name
auto svg_element = document->create_element_ns(
    Namespace::SVG,
    DOM::QualifiedName { "svg"_fly_string, "rect"_fly_string, {}
}).release_value();

// Create text node
auto text = document->create_text_node("Hello, world!"_string);

// Create document fragment
auto fragment = document->create_document_fragment();
```

### Tree Manipulation

```cpp
// Append child
parent->append_child(child).release_value();

// Insert before
parent->insert_before(new_child, reference_child).release_value();

// Remove child
parent->remove_child(child).release_value();

// Replace child
parent->replace_child(new_child, old_child).release_value();

// Remove node from parent
node->remove();

// Clone node
auto clone = node->clone_node(document, /* clone_children = */ true);
```

### Querying DOM

```cpp
// Get element by ID
auto element = document->get_element_by_id("my-id"_fly_string);

// Query selector
auto element = document->query_selector(".class > div"_string).release_value();

// Query selector all
auto elements = document->query_selector_all(".class"_string).release_value();

// Get elements by tag name
auto divs = document->get_elements_by_tag_name(HTML::TagNames::div);

// Get elements by class name
auto elements = document->get_elements_by_class_name("my-class"_fly_string);
```

### Traversal

```cpp
// Parent/child relationships
auto parent = node->parent();
auto first_child = node->first_child();
auto last_child = node->last_child();
auto next_sibling = node->next_sibling();
auto previous_sibling = node->previous_sibling();

// Element traversal (skips text nodes)
auto parent_element = node->parent_element();
auto first_element_child = element->first_element_child();
auto next_element_sibling = element->next_element_sibling();

// Iterate children
node->for_each_child([](Node& child) {
    dbgln("Child: {}", child.node_name());
    return IterationDecision::Continue;
});

// Iterate in tree order
node->for_each_in_inclusive_subtree([](Node& node) {
    dbgln("Node: {}", node.node_name());
    return TraversalDecision::Continue;
});
```

## Attributes

### Getting/Setting Attributes

```cpp
// Get attribute
auto value = element->get_attribute(HTML::AttributeNames::href);
if (value.has_value())
    dbgln("href: {}", value.value());

// Set attribute
element->set_attribute(HTML::AttributeNames::id, "my-id"_string).release_value();

// Remove attribute
element->remove_attribute(HTML::AttributeNames::disabled);

// Check if has attribute
if (element->has_attribute(HTML::AttributeNames::checked))
    dbgln("Checkbox is checked");

// Get attribute names
auto names = element->get_attribute_names();
```

### Reflected Attributes

```cpp
// Boolean attributes
bool disabled() const { return has_attribute(HTML::AttributeNames::disabled); }
void set_disabled(bool value) {
    if (value)
        MUST(set_attribute(HTML::AttributeNames::disabled, String {}));
    else
        remove_attribute(HTML::AttributeNames::disabled);
}

// String attributes
String title() const {
    return get_attribute(HTML::AttributeNames::title).value_or(String {});
}
void set_title(String const& value) {
    MUST(set_attribute(HTML::AttributeNames::title, value));
}

// Numeric attributes
u32 max_length() const {
    auto value = get_attribute(HTML::AttributeNames::maxlength);
    return value.has_value() ? value->to_number<u32>().value_or(0) : 0;
}
```

## CSS Computed Styles

### Accessing Computed Values

```cpp
// Get computed values for element
auto const& computed_values = element->computed_values();

// Common properties
auto display = computed_values.display();
auto position = computed_values.position();
auto width = computed_values.width();
auto height = computed_values.height();
auto color = computed_values.color();
auto background_color = computed_values.background_color();

// Layout properties
auto flex_direction = computed_values.flex_direction();
auto justify_content = computed_values.justify_content();
auto align_items = computed_values.align_items();

// Box model
auto padding = computed_values.padding();
auto margin = computed_values.margin();
auto border = computed_values.border();
```

### Triggering Style Recalculation

```cpp
// Invalidate style for element
element->invalidate_style(DOM::StyleInvalidationReason::ElementAttributeChange);

// Invalidate style for entire document
document->invalidate_style();

// Invalidate specific property
element->invalidate_style_for_property(CSS::PropertyID::Color);
```

## Events

### Creating Events

```cpp
// Create basic event
auto event = DOM::Event::create(realm(), HTML::EventNames::click);
event->set_bubbles(true);
event->set_cancelable(true);

// Create UI event
auto ui_event = UIEvents::UIEvent::create(realm(), HTML::EventNames::click);

// Create mouse event
auto mouse_event = UIEvents::MouseEvent::create(realm(), HTML::EventNames::click, {
    .client_x = 100,
    .client_y = 200,
    .button = UIEvents::MouseButton::Primary
});

// Create custom event
auto custom = DOM::CustomEvent::create(realm(), "my-event"_fly_string);
custom->set_detail(JS::Value { 42 });
```

### Dispatching Events

```cpp
// Dispatch event
element->dispatch_event(event);

// Check if event was prevented
if (event->default_prevented())
    dbgln("Default action prevented");

// Fire a simple event
element->fire_simple_event(HTML::EventNames::load);
```

### Event Listeners

```cpp
// Add event listener (C++ callback)
auto callback = GC::create_function_with_prototype(
    realm(),
    [](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        dbgln("Event fired!");
        return JS::js_undefined();
    },
    realm().intrinsics().function_prototype()
);

element->add_event_listener_without_options(
    HTML::EventNames::click,
    GC::Ref(*callback)
);

// Event handler attribute
element->set_event_handler_attribute(
    HTML::EventNames::onclick,
    callback
);
```

## Layout and Painting

### Layout Tree

```cpp
// Get layout node for DOM node
auto layout_node = dom_node->layout_node();
if (layout_node) {
    dbgln("Has layout node");
}

// Check layout node type
if (layout_node->is_box()) {
    auto& box = static_cast<Layout::Box&>(*layout_node);
    dbgln("Box size: {}x{}", box.content_width(), box.content_height());
}

// Get paintable
auto paintable = layout_node->paintable();
if (paintable) {
    paintable->paint(context, paint_phase);
}
```

### Layout State

```cpp
// Get layout state for node
auto const& state = layout_state.get(layout_node);

// Get mutable state
auto& mutable_state = layout_state.get_mutable(layout_node);
mutable_state.set_content_width(100);
mutable_state.set_content_height(200);

// Set position
mutable_state.set_content_x(10);
mutable_state.set_content_y(20);
```

### Triggering Layout

```cpp
// Mark needs layout
element->set_needs_layout(DOM::SetNeedsLayoutReason::StyleChange);

// Mark needs layout tree update
element->set_needs_layout_tree_update(
    DOM::SetNeedsLayoutTreeUpdateReason::StyleChange
);
```

## Task Queuing

### Queuing Tasks

```cpp
// Queue a task on DOM manipulation task source
element->queue_an_element_task(HTML::Task::Source::DOMManipulation, [this] {
    // Task code
    dbgln("Task executed");
});

// Queue a microtask
HTML::queue_a_microtask(document, [this] {
    // Microtask code
});

// Queue on specific task source
queue_a_task(HTML::Task::Source::Networking, [this] {
    // Network task
});
```

## Document and Window

### Document Access

```cpp
// Get document from node
auto& document = node->document();

// Check document state
if (document->is_fully_active())
    dbgln("Document is fully active");

// Get browsing context
auto browsing_context = document->browsing_context();

// Get window
auto window = document->window();

// Get realm
auto& realm = document->realm();
```

### Document Operations

```cpp
// Create URL relative to document
auto url = document->parse_url("path/to/resource"_string);

// Get base URL
auto base_url = document->base_url();

// Get document element
auto document_element = document->document_element();

// Get body element
auto body = document->body();
```

## JavaScript Integration

### Realm and VM

```cpp
// Get realm from node
auto& realm = node->realm();

// Get VM
auto& vm = realm.vm();

// Get global object
auto& global = realm.global_object();
```

### Creating JS Values

```cpp
// Primitives
auto undefined = JS::js_undefined();
auto null = JS::js_null();
auto boolean = JS::Value { true };
auto number = JS::Value { 42 };
auto string = JS::PrimitiveString::create(vm, "Hello"_string);

// Objects
auto object = JS::Object::create(realm, nullptr);

// Arrays
auto array = JS::Array::create(realm, 0);
array->indexed_properties().append(JS::Value { 1 });
array->indexed_properties().append(JS::Value { 2 });
```

### Calling JavaScript

```cpp
// Call function
auto result = TRY(JS::call(vm, function, this_value, arg1, arg2));

// Construct object
auto instance = TRY(JS::construct(vm, constructor, arg1, arg2));

// Get property
auto value = TRY(object->get(vm, "propertyName"_fly_string));

// Set property
TRY(object->set(vm, "propertyName"_fly_string, JS::Value { 42 }, JS::Object::ShouldThrowExceptions::Yes));
```

## Utilities

### String Operations

```cpp
// Create string
auto str = "Hello, world!"_string;
auto fly = "tagname"_fly_string;

// String view (non-owning)
auto view = "Hello"sv;

// Convert to AK::String
auto string = MUST(String::from_utf8(view));

// Format string
auto formatted = MUST(String::formatted("Value: {}", 42));
```

### Error Handling

```cpp
// Return error
if (error_condition)
    return WebIDL::SimpleException {
        WebIDL::SimpleExceptionType::TypeError,
        "Error message"sv
    };

// DOMException
return WebIDL::DOMException::create(
    realm(),
    WebIDL::ExceptionCode::InvalidStateError,
    "Invalid state"_string
);

// Propagate errors
auto result = TRY(fallible_operation());

// Convert OOM
auto buffer = TRY_OR_THROW_OOM(realm(), ByteBuffer::create_uninitialized(1024));
```

### Debugging

```cpp
// Debug logging
dbgln("Debug message: {}", value);
dbgln_if(LIBWEB_DEBUG, "Conditional debug message");

// Assertions
VERIFY(condition);
VERIFY_NOT_REACHED();

// TODO/FIXME markers
// FIXME: Implement this feature
// TODO: Optimize this code path
```

## Memory Management

### GC Pointers

```cpp
// Nullable pointer
GC::Ptr<Node> nullable_node;

// Non-null pointer
GC::Ref<Document> document = /* ... */;

// Convert Ptr to Ref (when you know it's non-null)
GC::Ref<Node> node = *nullable_node;

// Create GC object
auto object = realm.create<MyObject>(realm, args...);

// GC root (prevents collection)
GC::Root<Node> root { node };
```

### Visiting Edges

```cpp
virtual void visit_edges(Cell::Visitor& visitor) override
{
    Base::visit_edges(visitor);

    // Visit single pointer
    visitor.visit(m_node);

    // Visit container of pointers
    for (auto& item : m_items)
        visitor.visit(item);

    // Visit optional
    if (m_optional_node)
        visitor.visit(*m_optional_node);
}
```

## CSS Values

### Creating Style Values

```cpp
// Keyword value
auto value = CSS::KeywordStyleValue::create(CSS::Keyword::Auto);

// Length value
auto length = CSS::LengthStyleValue::create(CSS::Length::make_px(100));

// Color value
auto color = CSS::ColorStyleValue::create(Gfx::Color::from_rgb(0xFF0000));

// Identifier value
auto ident = CSS::IdentifierStyleValue::create(CSS::ValueID::Block);
```

### Resolving Values

```cpp
// Resolve length to pixels
CSSPixels pixels = length.to_px(layout_node);

// Get color
Gfx::Color color = color_value.to_color();

// Check value type
if (value->is_keyword() && value->as_keyword() == CSS::Keyword::Auto) {
    // Handle auto
}
```
