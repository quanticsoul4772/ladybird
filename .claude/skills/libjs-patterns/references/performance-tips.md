# LibJS Performance Optimization Guide

## Core Performance Principles

1. **Fast paths for common cases** - Optimize the 90% case
2. **Minimize allocations** - GC pressure is expensive
3. **Cache lookups** - Property access can be cached
4. **Inline hot paths** - Small, frequently-called functions
5. **Lazy initialization** - Don't compute until needed

## Value Operations

### Fast Integer Arithmetic

```cpp
// ✅ GOOD: Fast path for integers
ThrowCompletionOr<Value> add(VM& vm, Value lhs, Value rhs)
{
    // Fast path: both int32
    if (lhs.is_int32() && rhs.is_int32()) {
        // Check for overflow
        Checked<i32> result = lhs.as_i32();
        result += rhs.as_i32();
        if (!result.has_overflow())
            return Value(result.value());
    }

    // Slow path: generic addition
    auto lhs_num = TRY(lhs.to_number(vm));
    auto rhs_num = TRY(rhs.to_number(vm));
    return Value(lhs_num.as_double() + rhs_num.as_double());
}

// Performance: ~10x faster for integer arithmetic
```

### Avoid Unnecessary Conversions

```cpp
// ❌ BAD: Unnecessary string conversion
auto str = TRY(value.to_string(vm));
auto len = str.code_points().length();

// ✅ GOOD: Direct length check for strings
if (value.is_string()) {
    auto len = value.as_string().code_points().length();
} else {
    auto str = TRY(value.to_string(vm));
    auto len = str.code_points().length();
}
```

### Value Type Checks

```cpp
// ✅ GOOD: Type checks are very fast (bit operations)
if (value.is_int32()) { }      // Fast
if (value.is_number()) { }     // Fast
if (value.is_object()) { }     // Fast
if (value.is_string()) { }     // Fast

// Type extraction is O(1)
auto i = value.as_i32();       // Fast
auto d = value.as_double();    // Fast
auto& obj = value.as_object(); // Fast
```

## Property Access

### Inline Caches

```cpp
// ✅ GOOD: Use inline cache for repeated property access
ThrowCompletionOr<Value> optimized_get(Object& object, PropertyKey const& key)
{
    static Bytecode::PropertyLookupCache cache;

    // First lookup: cache miss, stores shape info
    // Subsequent lookups: cache hit, O(1) instead of O(n)
    return TRY(object.get(key, cache));
}

// Performance: ~5x faster on cache hit
```

### Shape-Based Optimization

```cpp
// Objects with same properties share a "shape"
// Adding properties in same order = same shape = faster access

// ✅ GOOD: Consistent property order
function createPoint(x, y) {
    return { x: x, y: y };  // Always same shape
}

// ❌ BAD: Inconsistent property order
function createPoint(x, y) {
    if (x > 0)
        return { x: x, y: y };
    else
        return { y: y, x: x };  // Different shape!
}
```

### Direct Property Access

```cpp
// ✅ GOOD: Direct property (bypasses getters/setters)
object.define_direct_property(vm.names.name, value, Attribute::Enumerable);

// ❌ SLOWER: Full property set (checks getters, prototype chain)
TRY(object.set(vm.names.name, value, Object::ShouldThrowExceptions::Yes));

// Use direct property for:
// - Object initialization
// - Internal properties
// - Known no setters exist
```

## String Operations

### String Interning

```cpp
// ✅ GOOD: Reuse interned strings
auto str = vm.names.length;  // Already interned
auto str = vm.names.prototype;

// ✅ GOOD: VM automatically interns common strings
auto str1 = PrimitiveString::create(vm, "hello"sv);
auto str2 = PrimitiveString::create(vm, "hello"sv);
// str1.ptr() == str2.ptr() (same object in memory)

// Performance: String comparison becomes pointer comparison
```

### Avoid String Concatenation in Loops

```cpp
// ❌ BAD: Repeated concatenation (O(n²) allocations)
String result;
for (size_t i = 0; i < 1000; ++i) {
    result = MUST(String::formatted("{}{}", result, i));
}

// ✅ GOOD: StringBuilder (O(n) allocations)
StringBuilder builder;
for (size_t i = 0; i < 1000; ++i) {
    builder.appendff("{}", i);
}
auto result = MUST(builder.to_string());

// Performance: ~100x faster for large strings
```

### String Views

```cpp
// ✅ GOOD: Use StringView for non-owning references
void process_string(StringView str)  // No allocation
{
    // ... use str
}

// ❌ BAD: Unnecessary copy
void process_string(String str)  // Copies string
{
    // ... use str
}
```

## Array Operations

### Packed Arrays

```cpp
// ✅ GOOD: Packed array (dense, fast)
auto array = Array::create_from(realm, {
    Value(0), Value(1), Value(2), Value(3)
});

// ❌ BAD: Sparse array (slow, uses hash map)
auto array = TRY(Array::create(realm, 0));
TRY(array->set(0, Value(1), Object::ShouldThrowExceptions::Yes));
TRY(array->set(1000000, Value(2), Object::ShouldThrowExceptions::Yes));
// Creates sparse storage, much slower access

// Performance: Packed arrays are ~10x faster
```

### Array Pre-allocation

```cpp
// ✅ GOOD: Pre-allocate known size
auto array = TRY(Array::create(realm, 1000));
for (size_t i = 0; i < 1000; ++i) {
    TRY(array->set(i, Value(i), Object::ShouldThrowExceptions::Yes));
}

// ❌ BAD: Growing array (repeated reallocations)
auto array = TRY(Array::create(realm, 0));
for (size_t i = 0; i < 1000; ++i) {
    auto length = TRY(length_of_array_like(vm, *array));
    TRY(array->create_data_property_or_throw(length, Value(i)));
}
```

### Indexed Access

```cpp
// ✅ GOOD: Direct indexed access
auto value = TRY(array->get(0));
auto value = TRY(array->get(42));

// ❌ SLOWER: PropertyKey construction
auto value = TRY(array->get(PropertyKey { 0 }));
```

## Function Calls

### Native Functions vs. Bytecode

```cpp
// Native functions (C++) are faster than bytecode-interpreted functions
// But crossing the native/JS boundary has overhead

// ✅ GOOD: Batch operations in native code
JS_DEFINE_NATIVE_FUNCTION(Array::prototype_fast_process)
{
    auto* array = TRY(typed_this_object(vm));
    auto length = TRY(length_of_array_like(vm, *array));

    // Process entire array in native code
    for (size_t i = 0; i < length; ++i) {
        auto value = TRY(array->get(i));
        // ... process value
    }
}

// ❌ BAD: Many JS->Native->JS transitions
// JavaScript:
// for (let i = 0; i < array.length; i++) {
//     nativeProcessOne(array[i]);  // Expensive transition per item
// }
```

### Avoid Repeated Lookups

```cpp
// ❌ BAD: Repeated property lookup
for (size_t i = 0; i < 1000; ++i) {
    auto method = TRY(object.get(vm.names.method));
    TRY(call_impl(vm, method.as_function(), &object));
}

// ✅ GOOD: Cache function reference
auto method = TRY(object.get(vm.names.method));
if (!method.is_function())
    return vm.throw_completion<TypeError>(ErrorType::NotAFunction);

for (size_t i = 0; i < 1000; ++i) {
    TRY(call_impl(vm, method.as_function(), &object));
}
```

## GC and Memory

### Minimize Allocations

```cpp
// ❌ BAD: Allocates on every iteration
for (size_t i = 0; i < 1000; ++i) {
    auto temp = PrimitiveString::create(vm, "temp"sv);
    // ... use temp
}

// ✅ GOOD: Allocate once, reuse
auto temp = PrimitiveString::create(vm, "temp"sv);
for (size_t i = 0; i < 1000; ++i) {
    // ... use temp
}
```

### Batch Allocations

```cpp
// ✅ GOOD: Allocate array elements in batch
auto array = TRY(Array::create(realm, 100));
for (size_t i = 0; i < 100; ++i) {
    TRY(array->set(i, Value(i), Object::ShouldThrowExceptions::Yes));
}

// GC only needs to track 1 object (the array)
// Internal slots are not individually tracked
```

### Object Pooling (Advanced)

```cpp
// For very hot paths, consider object pooling
// (Advanced technique, use sparingly)

class ObjectPool {
    Vector<GC::Ref<Object>> m_free_objects;

public:
    GC::Ref<Object> acquire(Realm& realm)
    {
        if (!m_free_objects.is_empty())
            return m_free_objects.take_last();
        return Object::create(realm, realm.intrinsics().object_prototype());
    }

    void release(GC::Ref<Object> obj)
    {
        // Clear object properties
        // ...
        m_free_objects.append(obj);
    }
};
```

## Bytecode Optimization

### Bytecode Builtins

```cpp
// Mark functions as bytecode builtins for inlining
void MathObject::initialize(Realm& realm)
{
    auto& vm = this->vm();
    u8 attr = Attribute::Writable | Attribute::Configurable;

    // Builtin hint allows bytecode inlining
    define_native_function(realm, vm.names.abs, abs, 1, attr,
        Bytecode::Builtin::MathAbs);  // ← Builtin hint

    define_native_function(realm, vm.names.floor, floor, 1, attr,
        Bytecode::Builtin::MathFloor);

    // Performance: ~3x faster when inlined
}
```

### Register Allocation

```cpp
// Minimize register pressure in bytecode generation

// ✅ GOOD: Reuse registers
void generate_bytecode(Generator& generator)
{
    auto temp = generator.allocate_register();
    generator.emit<Op::LoadImmediate>(temp, Value(1));
    // ... use temp
    generator.free_register(temp);  // Reuse for next operation
}

// ❌ BAD: Allocate many registers
void generate_bytecode(Generator& generator)
{
    auto temp1 = generator.allocate_register();
    auto temp2 = generator.allocate_register();
    auto temp3 = generator.allocate_register();
    // ... uses more stack space
}
```

## Common Patterns

### Early Return

```cpp
// ✅ GOOD: Early return on fast path
ThrowCompletionOr<Value> optimized_function(VM& vm, Value value)
{
    // Fast path: handle common case immediately
    if (value.is_int32() && value.as_i32() >= 0)
        return Value(value.as_i32() * 2);

    // Slow path: full validation
    auto num = TRY(value.to_number(vm));
    if (num.is_nan())
        return js_nan();
    // ... more logic
}
```

### Lazy Initialization

```cpp
// ✅ GOOD: Initialize only when needed
class LazyObject : public Object {
    mutable GC::Ptr<Object> m_expensive_property;

public:
    GC::Ref<Object> expensive_property() const
    {
        if (!m_expensive_property) {
            // Initialize on first access
            m_expensive_property = create_expensive_object();
        }
        return *m_expensive_property;
    }
};
```

### Compile-Time Constants

```cpp
// ✅ GOOD: Use compile-time constants
constexpr size_t MAX_ARRAY_LENGTH = 4294967295;  // 2^32 - 1
constexpr double MAX_SAFE_INTEGER = 9007199254740991.0;  // 2^53 - 1

// Fast comparison
if (length > MAX_ARRAY_LENGTH)
    return vm.throw_completion<RangeError>(ErrorType::InvalidLength);
```

## Profiling and Measurement

### Microbenchmarks

```cpp
// Measure performance of critical sections
#include <AK/Time.h>

auto start = MonotonicTime::now();

// ... code to benchmark

auto end = MonotonicTime::now();
auto duration = end - start;
dbgln("Operation took {}ms", duration.to_milliseconds());
```

### Hotspot Identification

```bash
# Use perf to find hotspots
perf record -g ./Build/release/bin/Ladybird
perf report

# Common hotspots:
# - Property lookup (optimize with inline caches)
# - Type conversions (add fast paths)
# - String operations (use StringBuilder)
# - GC marking (reduce allocation rate)
```

### Test262 Performance

```bash
# Run Test262 with timing
./Meta/WPT.sh run --test262 --time

# Compare performance between changes
./Meta/WPT.sh compare baseline.log new.log --perf
```

## Anti-Patterns

### ❌ Premature Optimization

```cpp
// Don't optimize until you've measured!

// ❌ BAD: Complicated code for unclear benefit
inline __attribute__((always_inline)) __attribute__((hot))
Value ultra_optimized(Value v) {
    // Micro-optimization without measurement
}

// ✅ GOOD: Clear code, measure, then optimize if needed
Value clear_code(Value v) {
    // Straightforward implementation
}
```

### ❌ Over-Caching

```cpp
// ❌ BAD: Caching everything (memory overhead)
class OverCached {
    HashMap<String, Value> m_cache;  // Grows forever

    Value get_value(String const& key) {
        if (auto cached = m_cache.get(key); cached.has_value())
            return cached.value();
        auto value = compute_value(key);
        m_cache.set(key, value);
        return value;
    }
};

// ✅ GOOD: Cache with size limit or LRU eviction
class SmartCache {
    static constexpr size_t MAX_CACHE_SIZE = 100;
    // ... LRU implementation
};
```

### ❌ Ignoring GC Pressure

```cpp
// ❌ BAD: Allocating in hot loop
for (size_t i = 0; i < 1000000; ++i) {
    auto obj = Object::create(realm, nullptr);
    // Short-lived object, triggers frequent GC
}

// ✅ GOOD: Reuse or batch allocate
auto obj = Object::create(realm, nullptr);
for (size_t i = 0; i < 1000000; ++i) {
    // Reuse obj or use stack values
}
```

## Performance Checklist

When optimizing LibJS code:

- [ ] Profile first - identify actual bottlenecks
- [ ] Add fast paths for common cases (int32, packed arrays)
- [ ] Minimize allocations in hot paths
- [ ] Use inline caches for property access
- [ ] Cache repeated lookups (functions, properties)
- [ ] Use StringBuilder for string concatenation
- [ ] Pre-allocate arrays when size is known
- [ ] Mark hot native functions as bytecode builtins
- [ ] Avoid unnecessary type conversions
- [ ] Use StringView for non-owning string references
- [ ] Early return on fast paths
- [ ] Lazy initialize expensive operations
- [ ] Verify with benchmarks before and after

## Further Reading

- ECMAScript Performance: https://v8.dev/docs/turbofan
- GC Performance: `Libraries/LibGC/README.md`
- Bytecode Optimization: `Libraries/LibJS/Bytecode/README.md`
- Inline Caching: https://mathiasbynens.be/notes/shapes-ics
