# Common LibJS APIs Reference

Quick reference for frequently used LibJS APIs and patterns.

## VM and Execution Context

### Accessing VM and Realm

```cpp
// Get VM from function context
JS_DEFINE_NATIVE_FUNCTION(MyClass::my_method)
{
    // VM& vm is implicit parameter
    auto& realm = *vm.current_realm();
}

// Get VM from object
class MyObject : public Object {
    void some_method()
    {
        auto& vm = this->vm();
        auto& realm = *vm.current_realm();
    }
};
```

### Execution Context

```cpp
// Current execution context
auto& context = vm.running_execution_context();

// Push new context
ExecutionContext new_context;
TRY(vm.push_execution_context(new_context, VM::CheckStackSpaceLimitTag {}));

// Pop context
vm.pop_execution_context();
```

## Value Operations

### Type Checking

```cpp
// Primitive types
value.is_undefined()
value.is_null()
value.is_nullish()              // undefined or null
value.is_boolean()
value.is_number()
value.is_string()
value.is_bigint()
value.is_symbol()

// Object types
value.is_object()
value.is_function()
value.is_constructor()

// Special values
value.is_nan()
value.is_infinity()
value.is_positive_infinity()
value.is_negative_infinity()
value.is_positive_zero()
value.is_negative_zero()

// Specific object types
if (value.is_object()) {
    auto& object = value.as_object();
    is<Array>(object)
    is<String>(object)
    is<Error>(object)
    is<TypedArrayBase>(object)
    // ... any custom type
}
```

### Type Extraction

```cpp
// Always check type first!
if (value.is_boolean())
    bool b = value.as_bool();

if (value.is_number())
    double d = value.as_double();

if (value.is_int32())
    i32 i = value.as_i32();

if (value.is_string())
    PrimitiveString& str = value.as_string();

if (value.is_object())
    Object& obj = value.as_object();

if (value.is_bigint())
    BigInt& bigint = value.as_bigint();

if (value.is_symbol())
    Symbol& symbol = value.as_symbol();
```

### Type Conversion

```cpp
// ToString(value)
auto string = TRY(value.to_string(vm));              // String (UTF-8)
auto prim_string = TRY(value.to_primitive_string(vm)); // PrimitiveString

// ToNumber(value)
auto number = TRY(value.to_number(vm));              // Value (number)
auto double_val = TRY(value.to_double(vm));          // double

// ToBoolean(value)
bool boolean = value.to_boolean();                   // Never throws

// ToObject(value)
auto object = TRY(value.to_object(vm));             // GC::Ref<Object>

// ToPropertyKey(value)
auto key = TRY(value.to_property_key(vm));          // PropertyKey

// Integer conversions
auto int_or_inf = TRY(value.to_integer_or_infinity(vm)); // double
auto length = TRY(value.to_length(vm));                  // size_t
auto index = TRY(value.to_index(vm));                    // size_t
auto i32_val = TRY(value.to_i32(vm));                    // i32
auto u32_val = TRY(value.to_u32(vm));                    // u32
auto i16_val = TRY(value.to_i16(vm));                    // i16
auto u16_val = TRY(value.to_u16(vm));                    // u16
auto i8_val = TRY(value.to_i8(vm));                      // i8
auto u8_val = TRY(value.to_u8(vm));                      // u8

// BigInt conversion
auto bigint = TRY(value.to_bigint(vm));                  // GC::Ref<BigInt>
```

### Creating Values

```cpp
// Primitives
Value js_undefined();
Value js_null();
Value js_true();
Value js_false();
Value js_nan();
Value js_infinity();
Value js_negative_infinity();

// From literals
Value(true);
Value(false);
Value(42);                              // i32
Value(42u);                             // u32
Value(3.14);                            // double

// Strings
auto str = PrimitiveString::create(vm, "Hello"sv);
Value(str.ptr());

// Objects
auto obj = Object::create(realm, nullptr);
Value(obj.ptr());
```

## Object Operations

### Creating Objects

```cpp
// Plain object
auto obj = Object::create(realm, realm.intrinsics().object_prototype());

// Array
auto array = TRY(Array::create(realm, 10));  // length = 10
auto array2 = Array::create_from(realm, {Value(1), Value(2), Value(3)});

// Object with specific prototype
auto obj = Object::create(realm, custom_prototype);

// Custom object
auto my_obj = MyObject::create(realm, args...);
```

### Property Access

```cpp
// Get property
auto value = TRY(object.get(vm.names.propertyName));
auto value = TRY(object.get(PropertyKey { "name" }));
auto value = TRY(object.get(0));  // Indexed property

// Set property
TRY(object.set(vm.names.propertyName, value,
    Object::ShouldThrowExceptions::Yes));

// Check if property exists
bool has = TRY(object.has_property(vm.names.propertyName));

// Delete property
bool deleted = TRY(object.internal_delete(vm.names.propertyName));

// Get own property
auto descriptor = TRY(object.internal_get_own_property(vm.names.name));
if (descriptor.has_value()) {
    // Property exists
}
```

### Defining Properties

```cpp
// Direct property (fast, bypasses setters)
object.define_direct_property(vm.names.name, value, Attribute::Enumerable);

// With descriptor
PropertyDescriptor descriptor {
    .value = value,
    .writable = true,
    .enumerable = true,
    .configurable = true,
};
TRY(object.define_property_or_throw(vm.names.name, descriptor));

// Accessor property
object.define_native_accessor(realm, vm.names.name,
    getter_function, setter_function, Attribute::Configurable);

// Native function
object.define_native_function(realm, vm.names.methodName,
    method_impl, 2,  // length = 2
    Attribute::Writable | Attribute::Configurable);
```

### Property Attributes

```cpp
// Attribute flags
Attribute::None                // 0 (non-writable, non-enumerable, non-configurable)
Attribute::Writable            // Can be modified
Attribute::Enumerable          // Shows in for-in, Object.keys()
Attribute::Configurable        // Can be deleted, attributes can change

// Common combinations
auto attr = Attribute::Writable | Attribute::Configurable;
auto readonly = Attribute::Enumerable;
```

### Iteration

```cpp
// Iterate own properties
auto keys = TRY(object.internal_own_property_keys());
for (auto& key : keys) {
    auto value = TRY(object.get(key));
    // Process property
}

// Iterate array-like
auto length = TRY(length_of_array_like(vm, object));
for (size_t i = 0; i < length; ++i) {
    auto value = TRY(object.get(i));
    // Process element
}

// Iterator protocol
auto iterator = TRY(get_iterator(vm, iterable));
while (true) {
    auto next = TRY(iterator_step(vm, iterator));
    if (!next.has_value())
        break;
    auto value = TRY(iterator_value(vm, *next));
    // Process value
}
```

## String Operations

### Creating Strings

```cpp
// From string view
auto str = PrimitiveString::create(vm, "Hello"sv);

// From String
auto str = PrimitiveString::create(vm, MUST(String::from_utf8("Hello"sv)));

// Empty string
vm.empty_string()

// Single character
vm.single_ascii_character_string('x')  // Only ASCII (0-127)

// Formatted string
auto str = PrimitiveString::create(vm,
    MUST(String::formatted("Value: {}", value)));
```

### String Operations

```cpp
// Get UTF-8 string
String utf8_string = primitive_string.utf8_string();

// Get code points
auto code_points = primitive_string.code_points();
auto length = code_points.length();

// Concatenation
auto result = TRY(PrimitiveString::create(vm, str1, str2));

// Substring
auto sub = primitive_string.substring(vm, start, length);
```

## Array Operations

### Creating Arrays

```cpp
// Empty array
auto array = TRY(Array::create(realm, 0));

// With length
auto array = TRY(Array::create(realm, 100));

// From values
auto array = Array::create_from(realm, {
    Value(1), Value(2), Value(3)
});
```

### Array Operations

```cpp
// Set element
TRY(array->set(0, value, Object::ShouldThrowExceptions::Yes));

// Get element
auto value = TRY(array->get(0));

// Push (using create_data_property_or_throw)
auto length = TRY(length_of_array_like(vm, *array));
TRY(array->create_data_property_or_throw(length, value));

// Iterate
auto length = TRY(length_of_array_like(vm, *array));
for (size_t i = 0; i < length; ++i) {
    auto value = TRY(array->get(i));
    // Process value
}
```

## Function Calls

### Calling Functions

```cpp
// Call function
auto result = TRY(call_impl(vm, function, this_value));

// Call with arguments
auto result = TRY(call_impl(vm, function, this_value,
    arg0, arg1, arg2));

// Call with argument array
Vector<Value> args { Value(1), Value(2) };
auto result = TRY(call_impl(vm, function, this_value, args.span()));

// Construct (new Foo())
auto object = TRY(construct_impl(vm, constructor));

// Construct with arguments
auto object = TRY(construct_impl(vm, constructor, arg0, arg1));
```

### Checking Callability

```cpp
// Check if value is callable
if (value.is_function()) {
    auto& function = value.as_function();
    auto result = TRY(call_impl(vm, function, js_undefined()));
}

// Check if value is constructor
if (value.is_constructor()) {
    auto& constructor = value.as_function();
    auto object = TRY(construct_impl(vm, constructor));
}
```

## Error Handling

### Throwing Errors

```cpp
// TypeError
return vm.throw_completion<TypeError>(ErrorType::NotAnObject, value);

// RangeError
return vm.throw_completion<RangeError>(ErrorType::InvalidLength);

// ReferenceError
return vm.throw_completion<ReferenceError>(ErrorType::UnknownIdentifier, name);

// SyntaxError
return vm.throw_completion<SyntaxError>(ErrorType::UnexpectedToken);

// Custom message
return vm.throw_completion<TypeError>("Custom error message"sv);

// AggregateError (multiple errors)
auto errors = Array::create_from(realm, {error1, error2});
return vm.throw_completion<AggregateError>(errors);
```

### Error Types Enum

Common `ErrorType` values (see `Runtime/ErrorTypes.h`):

```cpp
ErrorType::NotAnObject
ErrorType::NotAnObjectOfType
ErrorType::NotAFunction
ErrorType::NotAConstructor
ErrorType::IsImmutable
ErrorType::InvalidLength
ErrorType::InvalidIndex
ErrorType::OutOfBounds
ErrorType::UnknownIdentifier
ErrorType::CallStackSizeExceeded
ErrorType::NumberIsNaN
ErrorType::NumberIsNegative
// ... many more
```

### Checking for Exceptions

```cpp
// TRY() automatically propagates
auto result = TRY(operation_that_can_throw(vm));

// Check manually
auto completion = operation_that_can_throw(vm);
if (completion.is_error()) {
    // Handle error
    return completion.release_error();
}
auto result = completion.release_value();

// MUST() for operations that should never fail
auto result = MUST(infallible_operation());
```

## Realm and Intrinsics

### Accessing Intrinsics

```cpp
auto& realm = *vm.current_realm();

// Prototypes
realm.intrinsics().object_prototype()
realm.intrinsics().function_prototype()
realm.intrinsics().array_prototype()
realm.intrinsics().string_prototype()
realm.intrinsics().number_prototype()
realm.intrinsics().boolean_prototype()
realm.intrinsics().error_prototype()
// ... all built-in prototypes

// Constructors
realm.intrinsics().object_constructor()
realm.intrinsics().function_constructor()
realm.intrinsics().array_constructor()
realm.intrinsics().string_constructor()
// ... all built-in constructors

// Global object
auto& global = realm.global_object();
```

### Common Property Names

```cpp
auto& vm = /* ... */;

// Well-known property names
vm.names.length
vm.names.name
vm.names.prototype
vm.names.constructor
vm.names.toString
vm.names.valueOf
vm.names.message
vm.names.arguments
vm.names.caller
// ... many more

// Well-known symbols
vm.well_known_symbol_iterator()
vm.well_known_symbol_to_string_tag()
vm.well_known_symbol_species()
vm.well_known_symbol_has_instance()
vm.well_known_symbol_async_iterator()
// ... all well-known symbols
```

## Abstract Operations

### Common Operations

```cpp
// RequireObjectCoercible - reject null/undefined
TRY(require_object_coercible(vm, value));

// Call function
TRY(call_impl(vm, function, this_value, args...));

// Construct object
TRY(construct_impl(vm, constructor, args...));

// Get length of array-like
auto length = TRY(length_of_array_like(vm, object));

// Check if value can be held weakly
bool weak = can_be_held_weakly(value);

// Get function realm
auto* function_realm = TRY(get_function_realm(vm, function));

// Create list from array-like
auto list = TRY(create_list_from_array_like(vm, value));

// Species constructor
auto* constructor = TRY(species_constructor(vm, object, default_constructor));
```

## GC Operations

### Allocating Objects

```cpp
// Allocate on GC heap
auto obj = realm.heap().allocate<MyObject>(realm, args...);

// With OOM handling
auto obj = TRY_OR_THROW_OOM(vm, realm.heap().allocate<MyObject>(realm, args...));
```

### GC References

```cpp
// Non-null reference
GC::Ref<Object> obj_ref = object;

// Nullable pointer
GC::Ptr<Object> obj_ptr = object;
obj_ptr = nullptr;

// Check null
if (obj_ptr) {
    // Use *obj_ptr
}

// Convert between types
GC::Ptr<Object> ptr = obj_ref;  // Ref to Ptr
GC::Ref<Object> ref = *ptr;     // Ptr to Ref (check first!)
```

### Visiting Edges

```cpp
void MyObject::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);

    // Visit single references
    visitor.visit(m_object);
    visitor.visit(m_string);

    // Visit collections
    for (auto& item : m_items)
        visitor.visit(item);

    // Visit values (may contain objects)
    visitor.visit(m_value);
}
```

## Helper Macros

### Object Definition

```cpp
// In .h file
class MyObject : public Object {
    JS_OBJECT(MyObject, Object);
    GC_DECLARE_ALLOCATOR(MyObject);
};

// In .cpp file
GC_DEFINE_ALLOCATOR(MyObject);
```

### Native Functions

```cpp
// In .h file
JS_DECLARE_NATIVE_FUNCTION(my_function);

// In .cpp file
JS_DEFINE_NATIVE_FUNCTION(MyClass::my_function)
{
    // Implementation
}
```

### Completion Handling

```cpp
// Propagate exceptions (like Rust's ?)
auto result = TRY(fallible_operation(vm));

// Must succeed
auto result = MUST(infallible_operation());

// TRY with OOM handling
auto result = TRY_OR_THROW_OOM(vm, allocation_operation());

// Must succeed or internal error
auto result = MUST_OR_THROW_INTERNAL_ERROR(operation());
```

## Performance Tips

### Fast Paths

```cpp
// Check for fast integer path
if (lhs.is_int32() && rhs.is_int32()) {
    // Fast path: integer arithmetic
    return Value(lhs.as_i32() + rhs.as_i32());
}

// Fallback to slow path
return TRY(add_impl(vm, lhs, rhs));
```

### Caching

```cpp
// Use static caches for property lookup
static Bytecode::PropertyLookupCache cache;
auto value = TRY(object.get(vm.names.property, cache));
```

### String Interning

```cpp
// VM automatically interns strings
auto str1 = PrimitiveString::create(vm, "hello"sv);
auto str2 = PrimitiveString::create(vm, "hello"sv);
// str1.ptr() == str2.ptr() (same object)
```

## Debugging Helpers

```cpp
// Print value without side effects
dbgln("Value: {}", value.to_string_without_side_effects());

// Verify invariants
VERIFY(value.is_object());
VERIFY(!stack.is_empty());

// Dump backtrace
vm.dump_backtrace();

// Enable bytecode dump
extern bool g_dump_bytecode;
g_dump_bytecode = true;
```
