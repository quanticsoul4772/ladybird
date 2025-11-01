// Complete WebIDL binding example demonstrating all major patterns
// This shows how IDL translates to C++ implementation

// ==================== Basic Interface ====================

// Simple interface with attributes and methods
[Exposed=Window]
interface SimpleExample {
    // Constructor
    constructor(DOMString name);

    // Readonly attribute (getter only)
    readonly attribute DOMString name;

    // Read-write attribute (getter and setter)
    attribute unsigned long count;

    // Method with return value
    DOMString greet();

    // Void method
    undefined reset();
};

// C++ Implementation Pattern:
/*
class SimpleExample : public Bindings::PlatformObject {
    WEB_PLATFORM_OBJECT(SimpleExample, Bindings::PlatformObject);
    GC_DECLARE_ALLOCATOR(SimpleExample);

public:
    static WebIDL::ExceptionOr<GC::Ref<SimpleExample>> construct_impl(JS::Realm&, String name);

    String name() const { return m_name; }
    u32 count() const { return m_count; }
    void set_count(u32 value) { m_count = value; }

    String greet();
    void reset();

private:
    SimpleExample(JS::Realm&, String name);
    String m_name;
    u32 m_count { 0 };
};
*/

// ==================== HTML Element Patterns ====================

#import <HTML/HTMLElement.idl>

// Typical HTML element with reflected attributes
[Exposed=Window]
interface HTMLExampleElement : HTMLElement {
    [HTMLConstructor] constructor();

    // Reflected DOMString attribute (maps to HTML attribute)
    [Reflect, CEReactions] attribute DOMString title;

    // Reflected boolean attribute
    [Reflect, CEReactions] attribute boolean disabled;

    // Reflected with different attribute name
    [Reflect=maxlength, CEReactions] attribute unsigned long maxLength;

    // Enumerated attribute with specific values
    [Reflect, Enumerated=ExampleType, CEReactions] attribute DOMString type;

    // Non-reflected computed property
    readonly attribute boolean isValid;

    // Nullable attribute
    attribute DOMString? optionalValue;
};

enum ExampleType {
    "text",
    "number",
    "email"
};

// C++ Pattern for Reflected Attributes:
/*
// Reflected attributes use base class helpers
String title() const { return get_attribute(HTML::AttributeNames::title).value_or(String {}); }
void set_title(String const& value) { MUST(set_attribute(HTML::AttributeNames::title, value)); }

bool disabled() const { return has_attribute(HTML::AttributeNames::disabled); }
void set_disabled(bool value) {
    if (value)
        MUST(set_attribute(HTML::AttributeNames::disabled, String {}));
    else
        remove_attribute(HTML::AttributeNames::disabled);
}
*/

// ==================== Event Target Patterns ====================

[Exposed=Window]
interface EventfulInterface : EventTarget {
    // Event handler attributes (onfoo pattern)
    attribute EventHandler onchange;
    attribute EventHandler oninput;

    // Method that dispatches event
    undefined triggerChange();
};

// C++ Pattern:
/*
void EventfulInterface::triggerChange()
{
    // Create and dispatch event
    auto event = DOM::Event::create(realm(), HTML::EventNames::change);
    event->set_bubbles(true);
    event->set_cancelable(false);
    dispatch_event(event);
}
*/

// ==================== Promise-Returning Methods ====================

[Exposed=Window]
interface AsyncExample {
    // Promise-returning method
    Promise<DOMString> fetchData();

    // Promise that may reject
    Promise<undefined> doAsyncOperation();
};

// C++ Pattern:
/*
GC::Ref<JS::Promise> AsyncExample::fetch_data()
{
    auto& realm = this->realm();
    auto promise = JS::Promise::create(realm);

    // Queue a task to resolve the promise later
    queue_a_task([promise, this] {
        auto result = perform_operation();
        promise->fulfill(JS::PrimitiveString::create(vm(), result));
    });

    return promise;
}
*/

// ==================== Callback Patterns ====================

// Callback function type
callback EventCallback = undefined (Event event);

[Exposed=Window]
interface CallbackExample {
    // Method accepting callback
    undefined addEventListener(DOMString type, EventCallback? callback);

    // Method with optional callback
    undefined processData(optional EventCallback callback);
};

// C++ Pattern:
/*
void CallbackExample::add_event_listener(String const& type, GC::Ptr<WebIDL::CallbackType> callback)
{
    if (!callback)
        return;

    // Store callback (must visit in visit_edges!)
    m_callbacks.append(callback);
}
*/

// ==================== Dictionary Patterns ====================

dictionary ExampleOptions {
    // Optional members with defaults
    boolean bubbles = false;
    boolean cancelable = true;

    // Required member
    required DOMString type;

    // Member with specific type
    unsigned long timeout = 1000;

    // Nullable member
    DOMString? message = null;
};

[Exposed=Window]
interface DictionaryExample {
    undefined configure(ExampleOptions options);
};

// C++ Pattern:
/*
struct ExampleOptions {
    bool bubbles { false };
    bool cancelable { true };
    String type;  // Required, no default
    u32 timeout { 1000 };
    Optional<String> message;
};

void DictionaryExample::configure(ExampleOptions const& options)
{
    dbgln("Type: {}, Bubbles: {}, Timeout: {}",
          options.type, options.bubbles, options.timeout);
}
*/

// ==================== Sequence and Record Patterns ====================

[Exposed=Window]
interface CollectionExample {
    // Sequence (JavaScript array)
    sequence<DOMString> getTags();
    undefined setTags(sequence<DOMString> tags);

    // Record (JavaScript object as map)
    record<DOMString, any> getMetadata();
    undefined setMetadata(record<DOMString, any> data);

    // FrozenArray (immutable array)
    [SameObject] readonly attribute FrozenArray<DOMString> supportedTypes;
};

// C++ Pattern:
/*
Vector<String> CollectionExample::get_tags()
{
    return m_tags;
}

void CollectionExample::set_tags(Vector<String> tags)
{
    m_tags = move(tags);
}

OrderedHashMap<String, JS::Value> CollectionExample::get_metadata()
{
    return m_metadata;
}
*/

// ==================== Mixin Patterns ====================

// Mixins add capabilities to interfaces
interface mixin ExampleMixin {
    readonly attribute DOMString mixinProperty;
    undefined mixinMethod();
};

[Exposed=Window]
interface MixinUser {
    attribute DOMString name;
};

// Include mixin in interface
MixinUser includes ExampleMixin;

// C++ Pattern:
/*
// Define mixin as a base class or CRTP
template<typename Derived>
class ExampleMixin {
public:
    String mixin_property() const;
    void mixin_method();
};

class MixinUser
    : public Bindings::PlatformObject
    , public ExampleMixin<MixinUser> {
    // ...
};
*/

// ==================== Iterator Patterns ====================

[Exposed=Window]
interface IterableExample {
    // Iterable with key-value pairs
    iterable<DOMString, unsigned long>;

    // Methods to support iteration
    readonly attribute unsigned long length;
    getter unsigned long item(unsigned long index);
};

// C++ Pattern:
/*
// Iterable interfaces need to implement:
// - Iterator create_value_iterator()
// - length property
// - item(index) getter
*/

// ==================== Constructor Patterns ====================

// Standard constructor
[Exposed=Window]
interface StandardConstructor {
    constructor(DOMString arg);
};

// HTML constructor (for custom elements)
[Exposed=Window]
interface CustomElement : HTMLElement {
    [HTMLConstructor] constructor();
};

// Constructor with overloads
[Exposed=Window]
interface OverloadedConstructor {
    constructor();
    constructor(DOMString name);
    constructor(DOMString name, unsigned long count);
};

// ==================== Special Extended Attributes ====================

[Exposed=Window]
interface ExtendedAttributeExamples {
    // SameObject - always returns same object
    [SameObject] readonly attribute DOMStringList classList;

    // NewObject - creates new object each time
    [NewObject] DOMStringList getList();

    // Unscopable - not accessible via 'with' statement
    [Unscopable] undefined legacyMethod();

    // PutForwards - setting delegates to another object
    [PutForwards=value] readonly attribute DOMTokenList tokens;

    // Replaceable - can be replaced by assignment
    [Replaceable] readonly attribute any customData;

    // LegacyNullToEmptyString - null becomes ""
    [LegacyNullToEmptyString] attribute DOMString text;
};

// ==================== Registration Steps ====================

// To add a new IDL interface:
// 1. Create the .idl file in appropriate directory (e.g., LibWeb/DOM/)
// 2. Add imports for any types used
// 3. Add to Libraries/LibWeb/idl_files.cmake:
//    libweb_js_bindings(DOM/YourInterface)
// 4. Forward declare in Libraries/LibWeb/Forward.h
// 5. Create .h and .cpp files with implementation
// 6. Build: ./Meta/ladybird.py build
// 7. Test with LibWeb tests
