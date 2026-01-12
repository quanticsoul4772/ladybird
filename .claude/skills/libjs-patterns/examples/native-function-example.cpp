/*
 * Example: Implementing a Native Function in LibJS
 *
 * This example shows how to implement String.prototype.repeat(count)
 * following ECMAScript spec patterns.
 *
 * Spec: https://tc39.es/ecma262/#sec-string.prototype.repeat
 */

#include <LibJS/Runtime/StringPrototype.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/GlobalObject.h>

namespace JS {

// Step 1: Declare in StringPrototype.h
class StringPrototype : public StringObject {
    // ... other members ...

private:
    JS_DECLARE_NATIVE_FUNCTION(repeat);
};

// Step 2: Register in initialize() method
void StringPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    u8 attr = Attribute::Writable | Attribute::Configurable;

    // Register the function: name, C++ function, length, attributes
    define_native_function(realm, vm.names.repeat, repeat, 1, attr);
}

// Step 3: Implement the native function
// 22.1.3.15 String.prototype.repeat ( count )
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::repeat)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    auto object_coercible = TRY(require_object_coercible(vm, vm.this_value()));

    // 2. Let S be ? ToString(O).
    auto string = TRY(object_coercible.to_string(vm));

    // 3. Let n be ? ToIntegerOrInfinity(count).
    auto count_value = vm.argument(0);
    auto count = TRY(count_value.to_integer_or_infinity(vm));

    // 4. If n < 0 or n = +∞, throw a RangeError exception.
    if (count < 0 || Value(count).is_positive_infinity())
        return vm.throw_completion<RangeError>(ErrorType::StringRepeatCountMustBe);

    // 5. If n = 0 or the length of S is 0, return the empty String.
    if (count == 0 || string.is_empty())
        return PrimitiveString::create(vm, String {});

    // 6. If the length of S multiplied by n overflows the maximum string length,
    //    throw a RangeError exception.
    auto n = static_cast<size_t>(count);
    if (string.bytes().size() > (String::max_bytes / n))
        return vm.throw_completion<RangeError>(ErrorType::StringRepeatCountMustBe);

    // 7. Return the String value that is made from n copies of S appended together.
    StringBuilder builder;
    for (size_t i = 0; i < n; ++i)
        builder.append(string);

    return PrimitiveString::create(vm, MUST(builder.to_string()));
}

/*
 * Key Patterns Demonstrated:
 *
 * 1. Function Signature:
 *    JS_DEFINE_NATIVE_FUNCTION(ClassName::function_name)
 *    - Returns ThrowCompletionOr<Value>
 *    - Implicit VM& vm parameter
 *    - Access arguments via vm.argument(index)
 *    - Access 'this' via vm.this_value()
 *
 * 2. Error Handling:
 *    - Use TRY() for operations that can throw
 *    - Throw errors with vm.throw_completion<ErrorType>(...)
 *    - Return early on error conditions
 *
 * 3. Type Conversions:
 *    - TRY(value.to_string(vm))
 *    - TRY(value.to_integer_or_infinity(vm))
 *    - TRY(require_object_coercible(vm, value))
 *
 * 4. Return Values:
 *    - Primitive strings: PrimitiveString::create(vm, string)
 *    - Numbers: Value(42) or Value(3.14)
 *    - Booleans: Value(true) or js_true()
 *    - Undefined: js_undefined()
 *    - Objects: GC::Ref<Object>
 *
 * 5. Spec Compliance:
 *    - Match spec step numbers in comments
 *    - Use spec terminology exactly
 *    - Follow spec algorithm order
 */

// Example Usage in JavaScript:
// "abc".repeat(3)      // "abcabcabc"
// "x".repeat(0)        // ""
// "hi".repeat(2.5)     // "hihi" (2.5 becomes 2)
// "".repeat(100)       // ""
// "x".repeat(-1)       // RangeError
// "x".repeat(Infinity) // RangeError

}
