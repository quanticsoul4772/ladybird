# ECMAScript Specification Compliance Guide

## Core Principle

**Match the specification exactly** - Function names, variable names, and algorithm structure must mirror the ECMAScript specification.

## Specification Structure

ECMAScript algorithms follow this format:

```
22.1.3.15 String.prototype.repeat ( count )

This method performs the following steps when called:

1. Let O be ? RequireObjectCoercible(this value).
2. Let S be ? ToString(O).
3. Let n be ? ToIntegerOrInfinity(count).
4. If n < 0 or n = +∞, throw a RangeError exception.
5. If n = 0 or the length of S is 0, return the empty String.
6. Return the String value that is made from n copies of S appended together.
```

## Implementation Mapping

### 1. Naming Convention

**Rule**: Match spec names exactly, even if they seem unusual.

```cpp
// ✅ CORRECT - Matches spec terminology
bool HTMLInputElement::suffering_from_being_missing() const
{
    // Spec: https://html.spec.whatwg.org/#suffering-from-being-missing
}

// ❌ WRONG - Deviates from spec
bool HTMLInputElement::has_missing_value_constraint() const
{
    // Doesn't match spec!
}

// ✅ CORRECT - Abstract operation names
ThrowCompletionOr<Value> require_object_coercible(VM&, Value);
ThrowCompletionOr<size_t> length_of_array_like(VM&, Object const&);

// ❌ WRONG - Custom names
ThrowCompletionOr<Value> ensure_object(VM&, Value);
ThrowCompletionOr<size_t> get_array_length(VM&, Object const&);
```

### 2. Step-by-Step Implementation

**Rule**: Follow spec steps exactly, with comments matching step numbers.

```cpp
// 22.1.3.1 Array.prototype.at ( index )
JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::at)
{
    // 1. Let O be ? ToObject(this value).
    auto object = TRY(vm.this_value().to_object(vm));

    // 2. Let len be ? LengthOfArrayLike(O).
    auto length = TRY(length_of_array_like(vm, *object));

    // 3. Let relativeIndex be ? ToIntegerOrInfinity(index).
    auto relative_index = TRY(vm.argument(0).to_integer_or_infinity(vm));

    // 4. If relativeIndex ≥ 0, then
    //    a. Let k be relativeIndex.
    // 5. Else,
    //    a. Let k be len + relativeIndex.
    double k;
    if (relative_index >= 0)
        k = relative_index;
    else
        k = length + relative_index;

    // 6. If k < 0 or k ≥ len, return undefined.
    if (k < 0 || k >= length)
        return js_undefined();

    // 7. Return ? Get(O, ! ToString(𝔽(k))).
    return TRY(object->get(k));
}
```

### 3. Abstract Operations

**Abstract Operations** are reusable algorithms defined in the spec.

**Common Abstract Operations**:

| Spec Name | LibJS Implementation | Usage |
|-----------|---------------------|-------|
| `RequireObjectCoercible(argument)` | `TRY(require_object_coercible(vm, value))` | Reject null/undefined |
| `ToString(argument)` | `TRY(value.to_string(vm))` | Convert to string |
| `ToNumber(argument)` | `TRY(value.to_number(vm))` | Convert to number |
| `ToBoolean(argument)` | `value.to_boolean()` | Convert to boolean |
| `ToObject(argument)` | `TRY(value.to_object(vm))` | Convert to object |
| `ToIntegerOrInfinity(argument)` | `TRY(value.to_integer_or_infinity(vm))` | Convert to integer |
| `ToLength(argument)` | `TRY(value.to_length(vm))` | Clamp to valid length |
| `LengthOfArrayLike(obj)` | `TRY(length_of_array_like(vm, obj))` | Get .length property |
| `Call(F, V, argumentsList)` | `TRY(call_impl(vm, function, this_value, args))` | Invoke function |
| `Construct(F, argumentsList)` | `TRY(construct_impl(vm, constructor, args))` | Construct object |
| `Get(O, P)` | `TRY(object.get(property_key))` | Get property |
| `Set(O, P, V, Throw)` | `TRY(object.set(key, value, throw_flag))` | Set property |

### 4. Completion Records

The spec uses **Completion Records** to represent exceptional control flow.

```
Completion Record {
    [[Type]]: normal, break, continue, return, throw
    [[Value]]: any value
    [[Target]]: label (for break/continue)
}
```

**In LibJS**:

```cpp
// ThrowCompletionOr<T> represents operations that can throw
ThrowCompletionOr<Value> my_operation(VM& vm)
{
    // Normal completion
    return Value(42);

    // Throw completion
    return vm.throw_completion<TypeError>(ErrorType::NotAnObject);
}

// ? (question mark) means "ReturnIfAbrupt"
// Implemented with TRY() macro
auto result = TRY(fallible_operation(vm));

// ! (exclamation mark) means "never throws"
// Implemented with MUST() macro
auto result = MUST(infallible_operation());
```

### 5. Type Conversions

**Spec Conversions → LibJS Methods**:

```cpp
// ToString(value)
auto string = TRY(value.to_string(vm));

// ToNumber(value)
auto number = TRY(value.to_number(vm));

// ToNumeric(value)
auto numeric = TRY(value.to_numeric(vm));

// ToBigInt(value)
auto bigint = TRY(value.to_bigint(vm));

// ToBoolean(value)
bool boolean = value.to_boolean();  // Cannot throw

// ToObject(value)
auto object = TRY(value.to_object(vm));

// ToPropertyKey(value)
auto property_key = TRY(value.to_property_key(vm));

// ToLength(value)
auto length = TRY(value.to_length(vm));

// ToIndex(value)
auto index = TRY(value.to_index(vm));

// ToInt32(value)
auto int32 = TRY(value.to_i32(vm));

// ToUint32(value)
auto uint32 = TRY(value.to_u32(vm));

// ToIntegerOrInfinity(value)
auto integer = TRY(value.to_integer_or_infinity(vm));
```

### 6. Property Descriptors

```cpp
// PropertyDescriptor matches spec structure
PropertyDescriptor {
    Optional<Value> value;
    Optional<GC::Ptr<FunctionObject>> get;
    Optional<GC::Ptr<FunctionObject>> set;
    Optional<bool> writable;
    Optional<bool> enumerable;
    Optional<bool> configurable;
};

// Define property per spec
// Object.defineProperty(O, P, Desc)
TRY(object->define_property_or_throw(property_key, descriptor));

// Get property descriptor
// Object.getOwnPropertyDescriptor(O, P)
auto descriptor = TRY(object->internal_get_own_property(property_key));
```

### 7. Internal Methods

Objects have **internal methods** (double brackets in spec):

| Spec | LibJS Method |
|------|-------------|
| `[[Get]](P, Receiver)` | `internal_get(PropertyKey, Value receiver)` |
| `[[Set]](P, V, Receiver)` | `internal_set(PropertyKey, Value, Value receiver)` |
| `[[GetOwnProperty]](P)` | `internal_get_own_property(PropertyKey)` |
| `[[DefineOwnProperty]](P, Desc)` | `internal_define_own_property(PropertyKey, PropertyDescriptor)` |
| `[[HasProperty]](P)` | `internal_has_property(PropertyKey)` |
| `[[Delete]](P)` | `internal_delete(PropertyKey)` |
| `[[OwnPropertyKeys]]()` | `internal_own_property_keys()` |
| `[[Call]](thisArgument, argumentsList)` | `internal_call(Value, ReadonlySpan<Value>)` |
| `[[Construct]](argumentsList, newTarget)` | `internal_construct(ReadonlySpan<Value>, FunctionObject&)` |

### 8. Error Types

**Match spec error types**:

```cpp
// TypeError - type errors
return vm.throw_completion<TypeError>(ErrorType::NotAnObject, value);

// RangeError - numeric range errors
return vm.throw_completion<RangeError>(ErrorType::InvalidLength);

// ReferenceError - undeclared variables
return vm.throw_completion<ReferenceError>(ErrorType::UnknownIdentifier);

// SyntaxError - syntax errors
return vm.throw_completion<SyntaxError>(ErrorType::UnexpectedToken);

// URIError - URI encoding/decoding
return vm.throw_completion<URIError>(ErrorType::URIMalformed);

// AggregateError - multiple errors
return vm.throw_completion<AggregateError>(errors);
```

## Common Spec Patterns

### Pattern 1: Iterating Array-like Objects

```cpp
// Spec pattern:
// 1. Let O be ? ToObject(this value).
// 2. Let len be ? LengthOfArrayLike(O).
// 3. For each integer k in the inclusive interval from 0 to len - 1, do:
//    a. Let Pk be ! ToString(𝔽(k)).
//    b. Let kValue be ? Get(O, Pk).
//    c. ... process kValue

JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::example)
{
    auto object = TRY(vm.this_value().to_object(vm));
    auto length = TRY(length_of_array_like(vm, *object));

    for (size_t k = 0; k < length; ++k) {
        auto value = TRY(object->get(k));
        // Process value
    }

    return js_undefined();
}
```

### Pattern 2: Callback Functions

```cpp
// Spec pattern for Array.prototype.map(callbackfn, thisArg):
// 1. Let O be ? ToObject(this value).
// 2. Let len be ? LengthOfArrayLike(O).
// 3. If IsCallable(callbackfn) is false, throw a TypeError exception.
// 4. Let A be ? ArraySpeciesCreate(O, len).
// 5. For each integer k from 0 to len - 1:
//    a. Let kValue be ? Get(O, k).
//    b. Let mappedValue be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).
//    c. Perform ? CreateDataPropertyOrThrow(A, k, mappedValue).
// 6. Return A.

JS_DEFINE_NATIVE_FUNCTION(ArrayPrototype::map)
{
    auto object = TRY(vm.this_value().to_object(vm));
    auto length = TRY(length_of_array_like(vm, *object));

    auto callback = vm.argument(0);
    if (!callback.is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, callback);

    auto this_arg = vm.argument(1);

    auto array = TRY(Array::create(realm, length));

    for (size_t k = 0; k < length; ++k) {
        auto value = TRY(object->get(k));

        auto mapped = TRY(call_impl(vm, callback.as_function(), this_arg,
            value, Value(k), object));

        TRY(array->create_data_property_or_throw(k, mapped));
    }

    return array;
}
```

### Pattern 3: Optional Arguments

```cpp
// Spec: String.prototype.slice(start, end)
// If end is undefined, use length

JS_DEFINE_NATIVE_FUNCTION(StringPrototype::slice)
{
    auto string = TRY(utf8_string_from(vm));
    auto length = string.code_points().length();

    auto start_value = vm.argument(0);
    auto start = TRY(start_value.to_integer_or_infinity(vm));

    // Handle optional 'end' parameter
    auto end_value = vm.argument(1);
    double end;
    if (end_value.is_undefined())
        end = length;
    else
        end = TRY(end_value.to_integer_or_infinity(vm));

    // ... rest of implementation
}
```

### Pattern 4: Species Constructor

```cpp
// Array.prototype methods use species constructor
// Spec: ArraySpeciesCreate(originalArray, length)

static ThrowCompletionOr<Object*> array_species_create(VM& vm,
    Object& original_array, size_t length)
{
    auto& realm = *vm.current_realm();

    // Check if array
    auto is_array = TRY(Value(&original_array).is_array(vm));
    if (!is_array)
        return TRY(Array::create(realm, length)).ptr();

    // Get constructor
    auto constructor = TRY(original_array.get(vm.names.constructor));

    // Get @@species
    if (constructor.is_object()) {
        constructor = TRY(constructor.as_object().get(
            vm.well_known_symbol_species()));
        if (constructor.is_null())
            constructor = js_undefined();
    }

    // Use default or species constructor
    if (constructor.is_undefined())
        return TRY(Array::create(realm, length)).ptr();

    if (!constructor.is_constructor())
        return vm.throw_completion<TypeError>(ErrorType::NotAConstructor);

    return TRY(construct(vm, constructor.as_function(), Value(length))).ptr();
}
```

## Testing Compliance

### Test262 Conformance

LibJS tracks ECMAScript compliance with Test262:

```bash
# Run Test262 tests
./Meta/WPT.sh run --test262

# Compare against expectations
./Meta/WPT.sh compare --test262
```

### Writing Spec-Compliant Tests

```javascript
// test-string-repeat.js
describe("String.prototype.repeat", () => {
    test("basic functionality", () => {
        expect("abc".repeat(3)).toBe("abcabcabc");
        expect("".repeat(5)).toBe("");
        expect("x".repeat(0)).toBe("");
    });

    test("negative count throws RangeError", () => {
        expect(() => "x".repeat(-1)).toThrowWithMessage(
            RangeError,
            "repeat count must be a positive number"
        );
    });

    test("infinity count throws RangeError", () => {
        expect(() => "x".repeat(Infinity)).toThrowWithMessage(
            RangeError,
            "repeat count must be a positive number"
        );
    });

    test("converts argument to integer", () => {
        expect("x".repeat(2.9)).toBe("xx");
        expect("x".repeat("3")).toBe("xxx");
    });
});
```

## Spec References

### Where to Find Specifications

- **ECMAScript**: https://tc39.es/ecma262/
- **WebIDL**: https://webidl.spec.whatwg.org/
- **HTML**: https://html.spec.whatwg.org/
- **DOM**: https://dom.spec.whatwg.org/

### Reading Spec Algorithms

**Example from spec**:
```
7.3.23 LengthOfArrayLike ( obj )

The abstract operation LengthOfArrayLike takes argument obj
(an Object) and returns either a normal completion containing a
non-negative integer or a throw completion.

It returns the value of the "length" property of an array-like object.
It performs the following steps when called:

1. Return ℝ(? ToLength(? Get(obj, "length"))).
```

**Implementation**:
```cpp
// 7.3.23 LengthOfArrayLike ( obj )
ThrowCompletionOr<size_t> length_of_array_like(VM& vm, Object const& object)
{
    // 1. Return ℝ(? ToLength(? Get(obj, "length"))).
    auto length_value = TRY(object.get(vm.names.length));
    return TRY(length_value.to_length(vm));
}
```

## Checklist for Spec Compliance

When implementing a spec algorithm:

- [ ] Find the spec section (include URL in comment)
- [ ] Match function name exactly to spec
- [ ] Match parameter names to spec
- [ ] Comment each step with step number
- [ ] Use abstract operations from spec
- [ ] Handle optional parameters per spec
- [ ] Use correct error types
- [ ] Return correct completion type
- [ ] Add test cases for edge cases
- [ ] Verify against Test262 if available

## Common Deviations (Allowed)

Some deviations are acceptable for implementation:

1. **Performance optimizations** that don't affect observable behavior
2. **Fast paths** for common cases
3. **Internal helper functions** not exposed to JavaScript
4. **Implementation-specific debugging** (not observable)

```cpp
// ✅ ALLOWED: Fast path optimization
if (lhs.is_int32() && rhs.is_int32()) {
    // Fast integer addition
    return Value(lhs.as_i32() + rhs.as_i32());
}
// Fallback to spec algorithm
return TRY(add_impl(vm, lhs, rhs));

// ❌ NOT ALLOWED: Different observable behavior
// Don't skip type conversion steps!
```
