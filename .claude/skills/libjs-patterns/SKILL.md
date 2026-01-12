---
name: libjs-patterns
description: JavaScript engine development patterns for LibJS including bytecode, VM, runtime, heap, ECMAScript spec implementation, and built-in objects
use-when: Implementing JavaScript features, built-in objects, or engine internals in LibJS
allowed-tools:
  - Read
  - Write
  - Grep
  - Glob
  - Bash
---

# LibJS Patterns

Comprehensive patterns for JavaScript engine development in Ladybird's LibJS.

## Architecture Overview

LibJS is structured around these core components:

```
Libraries/LibJS/
├── Runtime/           # ECMAScript runtime (Object, Value, VM, built-ins)
├── Bytecode/          # Bytecode interpreter and operations
├── Heap/              # Garbage collection (uses LibGC)
├── Parser.cpp         # JavaScript parser (produces AST)
├── Lexer.cpp          # Tokenizer
└── Tests/             # JavaScript test suite
```

### Key Components

1. **VM (Virtual Machine)**: Central execution environment, manages execution contexts and heap
2. **Bytecode Interpreter**: Executes bytecode generated from AST
3. **Runtime**: ECMAScript built-in objects (Object, Array, String, etc.)
4. **Heap**: Garbage-collected memory (via LibGC with NanBoxed values)
5. **Value**: NanBoxed tagged union representing any JavaScript value

## 1. Value Types and Conversion

### Understanding JS::Value

LibJS uses **NanBoxing** to store all JavaScript values in a 64-bit representation:

```cpp
// Value is a NanBoxed tagged union
class Value : public GC::NanBoxedValue {
    // Primitive types
    bool is_undefined() const;
    bool is_null() const;
    bool is_boolean() const;
    bool is_number() const;
    bool is_string() const;
    bool is_bigint() const;
    bool is_symbol() const;

    // Object types
    bool is_object() const;
    bool is_accessor() const;

    // Extractors (must check type first!)
    bool as_bool() const;
    double as_double() const;
    i32 as_i32() const;
    Object& as_object();
    PrimitiveString& as_string();
};
```

### Pattern: Type Checking and Conversion

```cpp
// ✅ CORRECT - Always check type before extraction
ThrowCompletionOr<Value> my_function(VM& vm)
{
    auto value = vm.argument(0);

    // Type check before extraction
    if (!value.is_object())
        return vm.throw_completion<TypeError>(ErrorType::NotAnObject, value);

    auto& object = value.as_object();
    // ... use object

    return js_undefined();
}

// ✅ CORRECT - Use conversion helpers
ThrowCompletionOr<Value> to_number_example(VM& vm)
{
    auto value = vm.argument(0);

    // Converts value to number per ECMAScript spec
    auto number = TRY(value.to_number(vm));

    return number;
}

// Common conversions:
TRY(value.to_string(vm))           // ToString(value)
TRY(value.to_number(vm))           // ToNumber(value)
TRY(value.to_boolean())            // ToBoolean(value)
TRY(value.to_object(vm))           // ToObject(value)
TRY(value.to_primitive_string(vm)) // Returns PrimitiveString
```

### Creating Values

```cpp
// Primitive values
Value js_undefined();
Value js_null();
Value js_true();
Value js_false();
Value js_nan();
Value js_infinity();

// From C++ types
Value(bool);              // Boolean
Value(i32);               // Int32 (fast path)
Value(u32);               // Uint32
Value(double);            // Number
Value(Object*);           // Object
Value(PrimitiveString*);  // String

// From strings
auto str = PrimitiveString::create(vm, "Hello"sv);
Value(str.ptr());
```

## 2. Native Function Implementation

### Pattern: Implementing Built-in Functions

Every built-in function follows this pattern:

```cpp
// In the .h file
class StringPrototype : public StringObject {
    JS_OBJECT(StringPrototype, StringObject);
    GC_DECLARE_ALLOCATOR(StringPrototype);

public:
    virtual void initialize(Realm&) override;

private:
    // Declare native function
    JS_DECLARE_NATIVE_FUNCTION(char_at);
    JS_DECLARE_NATIVE_FUNCTION(substring);
};

// In the .cpp file
GC_DEFINE_ALLOCATOR(StringPrototype);

void StringPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    u8 attr = Attribute::Writable | Attribute::Configurable;

    // Register native functions
    define_native_function(realm, vm.names.charAt, char_at, 1, attr);
    define_native_function(realm, vm.names.substring, substring, 2, attr);
}

// Implement the function - signature ALWAYS matches this pattern
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::char_at)
{
    // 1. Get 'this' value and validate
    auto string = TRY(utf8_string_from(vm));

    // 2. Get and validate arguments
    auto position = TRY(vm.argument(0).to_integer_or_infinity(vm));

    // 3. Implement spec algorithm
    if (position < 0 || position >= static_cast<double>(string.code_points().length()))
        return PrimitiveString::create(vm, String {});

    // 4. Return result
    auto code_point = string.code_points().at(position);
    return PrimitiveString::create(vm, String::from_code_point(code_point));
}
```

### Pattern: ThrowCompletionOr and TRY

LibJS uses `ThrowCompletionOr<T>` for functions that can throw JavaScript exceptions:

```cpp
// Return type for all fallible operations
ThrowCompletionOr<Value> my_function(VM& vm)
{
    // TRY propagates exceptions (like Rust's ?)
    auto string = TRY(vm.argument(0).to_string(vm));
    auto number = TRY(vm.argument(1).to_number(vm));

    // Throw TypeError
    if (number.is_nan())
        return vm.throw_completion<TypeError>(ErrorType::NumberIsNaN);

    // Throw RangeError
    if (number.as_double() < 0)
        return vm.throw_completion<RangeError>(ErrorType::NumberIsNegative);

    // Normal return
    return PrimitiveString::create(vm, string);
}

// Helper functions that can't fail return Value directly
Value simple_function(VM& vm)
{
    return Value(42);
}
```

### Pattern: Accessing Arguments

```cpp
JS_DEFINE_NATIVE_FUNCTION(MyObject::my_method)
{
    // Get argument count
    auto arg_count = vm.argument_count();

    // Get specific argument (returns js_undefined() if not provided)
    auto arg0 = vm.argument(0);
    auto arg1 = vm.argument(1);

    // Get this_value
    auto this_value = vm.this_value();

    // Require this_value to be specific type
    auto* this_object = TRY(typed_this_object(vm));

    // Example: Math.max(...values)
    if (arg_count == 0)
        return js_negative_infinity();

    double max = -INFINITY;
    for (size_t i = 0; i < arg_count; ++i) {
        auto number = TRY(vm.argument(i).to_number(vm));
        if (number.is_nan())
            return js_nan();
        max = AK::max(max, number.as_double());
    }

    return Value(max);
}
```

## 3. Object Model Patterns

### Creating Custom Object Types

```cpp
// Pattern: Custom object with constructor
// In MyObject.h
class MyObject : public Object {
    JS_OBJECT(MyObject, Object);
    GC_DECLARE_ALLOCATOR(MyObject);

public:
    static GC::Ref<MyObject> create(Realm&, String data);

    virtual ~MyObject() override = default;

    String const& data() const { return m_data; }

protected:
    MyObject(Realm&, String data);

    virtual void initialize(Realm&) override;

private:
    String m_data;
};

// In MyObject.cpp
GC_DEFINE_ALLOCATOR(MyObject);

GC::Ref<MyObject> MyObject::create(Realm& realm, String data)
{
    return realm.heap().allocate<MyObject>(realm, move(data));
}

MyObject::MyObject(Realm& realm, String data)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().object_prototype())
    , m_data(move(data))
{
}

void MyObject::initialize(Realm& realm)
{
    Base::initialize(realm);

    // Define properties
    define_direct_property(vm().names.data,
        PrimitiveString::create(vm(), m_data),
        Attribute::Enumerable);
}
```

### Pattern: Constructor Object

```cpp
// Constructor is a separate class
class MyObjectConstructor : public NativeFunction {
    JS_OBJECT(MyObjectConstructor, NativeFunction);
    GC_DECLARE_ALLOCATOR(MyObjectConstructor);

public:
    virtual void initialize(Realm&) override;

    virtual ThrowCompletionOr<Value> call() override;
    virtual ThrowCompletionOr<GC::Ref<Object>> construct(FunctionObject& new_target) override;

private:
    explicit MyObjectConstructor(Realm&);

    virtual bool has_constructor() const override { return true; }
};

GC_DEFINE_ALLOCATOR(MyObjectConstructor);

MyObjectConstructor::MyObjectConstructor(Realm& realm)
    : NativeFunction(realm.vm().names.MyObject.as_string(),
                     realm.intrinsics().function_prototype())
{
}

void MyObjectConstructor::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    // Set .length property
    define_direct_property(vm.names.length, Value(1), Attribute::Configurable);

    // Set .prototype property
    define_direct_property(vm.names.prototype,
        realm.intrinsics().my_object_prototype(),
        Attribute::None);
}

// new MyObject(data)
ThrowCompletionOr<GC::Ref<Object>> MyObjectConstructor::construct(FunctionObject&)
{
    auto& vm = this->vm();
    auto& realm = *vm.current_realm();

    auto data = TRY(vm.argument(0).to_string(vm));
    return MyObject::create(realm, move(data));
}

// MyObject(data) - called without 'new'
ThrowCompletionOr<Value> MyObjectConstructor::call()
{
    auto& vm = this->vm();
    return vm.throw_completion<TypeError>(ErrorType::ConstructorWithoutNew, "MyObject");
}
```

### Pattern: Prototype Object

```cpp
class MyObjectPrototype : public Object {
    JS_OBJECT(MyObjectPrototype, Object);
    GC_DECLARE_ALLOCATOR(MyObjectPrototype);

public:
    virtual void initialize(Realm&) override;

private:
    explicit MyObjectPrototype(Realm&);

    JS_DECLARE_NATIVE_FUNCTION(to_string);
    JS_DECLARE_NATIVE_FUNCTION(get_data);
};

GC_DEFINE_ALLOCATOR(MyObjectPrototype);

MyObjectPrototype::MyObjectPrototype(Realm& realm)
    : Object(ConstructWithPrototypeTag::Tag, realm.intrinsics().object_prototype())
{
}

void MyObjectPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(realm, vm.names.toString, to_string, 0, attr);
    define_native_function(realm, vm.names.getData, get_data, 0, attr);
}

JS_DEFINE_NATIVE_FUNCTION(MyObjectPrototype::to_string)
{
    auto* this_object = TRY(typed_this_object(vm));
    return PrimitiveString::create(vm,
        MUST(String::formatted("[object MyObject: {}]", this_object->data())));
}

JS_DEFINE_NATIVE_FUNCTION(MyObjectPrototype::get_data)
{
    auto* this_object = TRY(typed_this_object(vm));
    return PrimitiveString::create(vm, this_object->data());
}
```

## 4. Garbage Collection Patterns

### Pattern: GC-Safe Object Management

All JavaScript objects use LibGC's garbage collector:

```cpp
// ✅ CORRECT - Use GC::Ref and GC::Ptr
class MyObject : public Object {
    JS_OBJECT(MyObject, Object);
    GC_DECLARE_ALLOCATOR(MyObject);  // Required macro

private:
    GC::Ptr<Object> m_other_object;      // Nullable GC pointer
    GC::Ref<PrimitiveString> m_string;   // Non-null GC reference

    // Non-GC types are fine
    String m_data;
    Vector<u32> m_numbers;
};

// MUST define allocator in .cpp file
GC_DEFINE_ALLOCATOR(MyObject);

// ✅ CORRECT - Visit GC edges for marking
void MyObject::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);

    // Visit all GC-managed members
    visitor.visit(m_other_object);
    visitor.visit(m_string);
}

// ❌ WRONG - Don't use raw pointers to GC objects
class BadObject : public Object {
    Object* m_other;  // Will crash! Not tracked by GC
};
```

### Pattern: Creating GC Objects

```cpp
// Use heap().allocate<T>() or static create() methods
GC::Ref<MyObject> MyObject::create(Realm& realm, String data)
{
    // Allocate on GC heap
    return realm.heap().allocate<MyObject>(realm, move(data));
}

// Alternative: Use TRY if allocation can fail
ThrowCompletionOr<GC::Ref<MyObject>> MyObject::try_create(VM& vm, String data)
{
    auto& realm = *vm.current_realm();
    // TRY_OR_THROW_OOM handles allocation failures
    return TRY_OR_THROW_OOM(vm, realm.heap().allocate<MyObject>(realm, move(data)));
}
```

## 5. ECMAScript Spec Implementation

### Pattern: Matching Spec Algorithms

**Critical**: Function and variable names must match the ECMAScript specification exactly.

```cpp
// Example: Array.prototype.includes(searchElement, fromIndex)
// Spec: https://tc39.es/ecma262/#sec-array.prototype.includes
JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::includes)
{
    // 1. Let O be ? ToObject(this value).
    auto object = TRY(vm.this_value().to_object(vm));

    // 2. Let len be ? LengthOfArrayLike(O).
    auto length = TRY(length_of_array_like(vm, object));

    // 3. If len = 0, return false.
    if (length == 0)
        return Value(false);

    // 4. Let n be ? ToIntegerOrInfinity(fromIndex).
    auto from_index = TRY(vm.argument(1).to_integer_or_infinity(vm));

    // 5. If n = +∞, return false.
    if (Value(from_index).is_positive_infinity())
        return Value(false);

    // ... continue matching spec steps exactly

    return Value(true);
}
```

### Pattern: Abstract Operations

Use functions from `Runtime/AbstractOperations.h`:

```cpp
#include <LibJS/Runtime/AbstractOperations.h>

// Common abstract operations:
TRY(require_object_coercible(vm, value))  // RequireObjectCoercible
TRY(call_impl(vm, function, this_value))  // Call
TRY(construct_impl(vm, constructor))      // Construct
TRY(length_of_array_like(vm, object))     // LengthOfArrayLike
TRY(get_function_realm(vm, function))     // GetFunctionRealm
can_be_held_weakly(value)                 // CanBeHeldWeakly
```

### Pattern: Error Types

Match spec error types precisely:

```cpp
// TypeError - type errors
return vm.throw_completion<TypeError>(ErrorType::NotAnObject, value);
return vm.throw_completion<TypeError>(ErrorType::NotAFunction, value);

// RangeError - value out of range
return vm.throw_completion<RangeError>(ErrorType::InvalidLength);

// ReferenceError - undefined variable
return vm.throw_completion<ReferenceError>(ErrorType::UnknownIdentifier, name);

// SyntaxError - parsing errors
return vm.throw_completion<SyntaxError>(ErrorType::InvalidRegularExpression);

// URIError - URI encoding/decoding
return vm.throw_completion<URIError>(ErrorType::URIMalformed);

// Custom error messages
return vm.throw_completion<TypeError>("Custom error message"sv);
```

## 6. Property Access Patterns

### Pattern: Getting Properties

```cpp
// Simple property get
auto value = TRY(object.get(vm.names.length));

// Property get with PropertyKey
PropertyKey key { "propertyName" };
auto value = TRY(object.get(key));

// Indexed property
auto value = TRY(object.get(0));

// Internal [[Get]]
auto value = TRY(object.internal_get(key, &object));
```

### Pattern: Setting Properties

```cpp
// Simple property set
TRY(object.set(vm.names.length, Value(42), Object::ShouldThrowExceptions::Yes));

// Define property with attributes
PropertyDescriptor descriptor {
    .value = Value(42),
    .writable = true,
    .enumerable = true,
    .configurable = true,
};
TRY(object.define_property_or_throw(vm.names.count, descriptor));

// Direct property (bypass setters)
object.define_direct_property(vm.names.data, value, Attribute::Enumerable);
```

### Pattern: Property Attributes

```cpp
// Attribute flags
u8 attr = Attribute::Writable | Attribute::Configurable;
u8 readonly = Attribute::Enumerable;  // Omit Writable and Configurable
u8 none = Attribute::None;             // Like Math.prototype

// Common patterns:
Attribute::Enumerable                                      // Writable omitted
Attribute::Writable | Attribute::Configurable             // Most prototype methods
Attribute::Writable | Attribute::Enumerable | Attribute::Configurable  // Data properties
```

## 7. Common Patterns and Idioms

### Pattern: Type Checking Helpers

```cpp
// Use typed_this_object for methods
JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::push)
{
    // Validates 'this' is Array or Array-like
    auto* array = TRY(typed_this_object(vm));
    // ... use array
}

// Custom type checks
auto* my_obj = TRY(vm.this_value().to_object(vm));
if (!is<MyObject>(my_obj))
    return vm.throw_completion<TypeError>(ErrorType::NotAnObjectOfType, "MyObject");

auto& typed_obj = static_cast<MyObject&>(*my_obj);
```

### Pattern: Variadic Arguments

```cpp
// Example: Math.max(...values)
JS_DEFINE_NATIVE_FUNCTION(MathObject::max)
{
    if (vm.argument_count() == 0)
        return js_negative_infinity();

    double max_value = -INFINITY;
    for (size_t i = 0; i < vm.argument_count(); ++i) {
        auto number = TRY(vm.argument(i).to_number(vm));
        if (number.is_nan())
            return js_nan();
        max_value = AK::max(max_value, number.as_double());
    }

    return Value(max_value);
}
```

### Pattern: Iterator Protocol

```cpp
// Get iterator
auto iterator = TRY(get_iterator(vm, iterable));

// Iterate
while (true) {
    auto next = TRY(iterator_step(vm, iterator));
    if (!next.has_value())
        break;

    auto value = TRY(iterator_value(vm, *next));
    // ... process value
}

// Close iterator on error
auto iterator = TRY(get_iterator(vm, iterable));
auto result = [&]() -> ThrowCompletionOr<Value> {
    // ... iteration code
    if (error_condition)
        return vm.throw_completion<Error>(...);
    return value;
}();

if (result.is_error()) {
    TRY(iterator_close(vm, iterator, result.release_error()));
}
```

### Pattern: Callback Functions

```cpp
// Array.prototype.map(callbackFn, thisArg)
JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::map)
{
    auto callback = vm.argument(0);
    auto this_arg = vm.argument(1);

    // 1. Validate callback is callable
    if (!callback.is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, callback);

    auto& callback_fn = callback.as_function();

    // 2. Call callback for each element
    auto length = TRY(length_of_array_like(vm, object));
    auto new_array = TRY(Array::create(realm, length));

    for (size_t i = 0; i < length; ++i) {
        auto value = TRY(object.get(i));

        // Call: callbackFn.call(thisArg, value, index, object)
        auto result = TRY(call_impl(vm, callback_fn, this_arg,
            value, Value(i), &object));

        TRY(new_array->create_data_property_or_throw(i, result));
    }

    return new_array;
}
```

## 8. Bytecode Integration

### Understanding Bytecode Flow

```
JavaScript Source → Parser → AST → Bytecode Generator → Bytecode → Interpreter → Execution
```

LibJS compiles JavaScript to bytecode for efficient execution.

### Pattern: Bytecode Builtins

Some functions can be optimized as bytecode builtins:

```cpp
// In MathObject::initialize
define_native_function(realm, vm.names.abs, abs, 1, attr,
    Bytecode::Builtin::MathAbs);  // Optional builtin hint

// The bytecode interpreter can inline fast paths for these
```

### Pattern: Bytecode Operations

Bytecode operations are defined in `Bytecode/Op.h`:

```cpp
// Example operation structure
class GetById final : public Instruction {
public:
    GetById(Operand dst, Operand base, IdentifierTableIndex property)
        : Instruction(Type::GetById)
        , m_dst(dst)
        , m_base(base)
        , m_property(property)
    {
    }

    void execute_impl(Bytecode::Interpreter&) const;

private:
    Operand m_dst;
    Operand m_base;
    IdentifierTableIndex m_property;
};
```

## 9. Testing Patterns

### Pattern: LibJS Test Structure

```javascript
// Tests/LibJS/test-my-feature.js
describe("MyObject", () => {
    test("constructor", () => {
        expect(new MyObject("data")).toBeInstanceOf(MyObject);
    });

    test("getData method", () => {
        const obj = new MyObject("hello");
        expect(obj.getData()).toBe("hello");
    });

    test("throws on invalid arguments", () => {
        expect(() => {
            new MyObject(Symbol());
        }).toThrowWithMessage(TypeError, "Cannot convert symbol to string");
    });
});
```

### Pattern: C++ Unit Tests

```cpp
// Tests/LibJS/TestMyObject.cpp
#include <LibJS/Runtime/MyObject.h>
#include <LibTest/TestCase.h>

TEST_CASE(my_object_creation)
{
    auto vm = JS::VM::create();
    auto realm = JS::Realm::create(*vm);

    auto obj = JS::MyObject::create(*realm, "test"_string);
    EXPECT_EQ(obj->data(), "test"_string);
}
```

## 10. Common Pitfalls

### ❌ Don't: Return raw pointers

```cpp
// ❌ WRONG
Object* my_function(VM& vm) {
    return new MyObject();  // Memory leak + GC won't track
}

// ✅ CORRECT
GC::Ref<Object> my_function(VM& vm) {
    auto& realm = *vm.current_realm();
    return MyObject::create(realm, "data"_string);
}
```

### ❌ Don't: Store non-GC pointers to GC objects

```cpp
// ❌ WRONG
class MyClass {
    Object* m_object;  // Dangling pointer after GC!
};

// ✅ CORRECT
class MyClass : public Cell {
    GC::Ptr<Object> m_object;

    void visit_edges(Visitor& visitor) {
        visitor.visit(m_object);
    }
};
```

### ❌ Don't: Forget to check types

```cpp
// ❌ WRONG
auto& obj = vm.argument(0).as_object();  // Crashes if not object!

// ✅ CORRECT
if (!vm.argument(0).is_object())
    return vm.throw_completion<TypeError>(ErrorType::NotAnObject);
auto& obj = vm.argument(0).as_object();
```

### ❌ Don't: Use C++ exceptions

```cpp
// ❌ WRONG - Exceptions disabled in Ladybird
try {
    something();
} catch (...) {
}

// ✅ CORRECT - Use ThrowCompletionOr and TRY
auto result = TRY(something_fallible(vm));
```

## Checklist for Implementing JavaScript Features

When implementing a new JavaScript feature:

- [ ] Match ECMAScript spec algorithm names exactly
- [ ] Use `ThrowCompletionOr<T>` for all fallible operations
- [ ] Use `TRY()` macro for error propagation
- [ ] Implement proper type conversions (ToNumber, ToString, etc.)
- [ ] Use GC::Ref/GC::Ptr for all JavaScript objects
- [ ] Implement `visit_edges()` for GC-managed members
- [ ] Add `GC_DECLARE_ALLOCATOR` in .h and `GC_DEFINE_ALLOCATOR` in .cpp
- [ ] Follow naming conventions (m_ prefix for members)
- [ ] Validate arguments before use
- [ ] Add JavaScript tests in Tests/LibJS/
- [ ] Check for edge cases (undefined, null, NaN, infinity)
- [ ] Handle iterator protocol correctly
- [ ] Use abstract operations from AbstractOperations.h

## Reference Documentation

See the `references/` directory for:
- `libjs-architecture.md` - Detailed VM and runtime architecture
- `ecmascript-compliance.md` - Spec compliance guidelines
- `common-apis.md` - Frequently used LibJS APIs
- `performance-tips.md` - Optimization patterns

See the `examples/` directory for:
- `native-function-example.cpp` - Complete native function implementation
- `object-implementation-example.cpp` - Custom object type with constructor/prototype
- `bytecode-operation-example.cpp` - Adding a bytecode operation
- `gc-integration-example.cpp` - GC-safe object management

## Related Skills

### Prerequisite Skills
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Master C++ patterns before LibJS development. Understand GC::Ptr, NonnullGCPtr, and error handling with ThrowCompletionOr.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Test JavaScript implementations against ECMAScript spec and Test262 conformance suite.

### Complementary Skills
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Integrate LibJS with LibWeb DOM APIs. Implement JavaScript bindings for HTML elements and Web APIs using WebIDL.
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: LibJS runs in WebContent process. Understand process isolation for JavaScript execution.

### Build and Testing
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Add new LibJS source files to CMake. Build bytecode compiler, runtime, and built-in objects.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Fuzz JavaScript parser, bytecode compiler, and runtime. Create fuzzer harnesses for JS engine components.

### Debugging and Optimization
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug LibJS crashes and GC issues. Inspect heap state, stack traces, and bytecode execution.
- **[browser-performance](../browser-performance/SKILL.md)**: Optimize JavaScript execution performance. Profile bytecode generation, JIT compilation, and GC overhead.

### Quality Assurance
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Run Test262 and LibJS tests in CI. Test JIT compiler across platforms.
- **[documentation-generation](../documentation-generation/SKILL.md)**: Document LibJS APIs and ECMAScript spec compliance. Link to TC39 spec sections.
