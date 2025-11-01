# Spec Compliance Guidelines

This document explains how to implement web specifications in LibWeb with maximum fidelity to the original specs.

## Core Principle: Match the Spec Exactly

The golden rule: **When implementing a spec algorithm, match the spec naming and structure exactly.**

### Why Exact Matching Matters

1. **Maintainability**: Future developers can correlate code with spec
2. **Correctness**: Specs are carefully worded to avoid edge case bugs
3. **Auditability**: Easy to verify implementation against spec
4. **Compatibility**: Ensures behavior matches other browsers

## Naming Conventions

### Function Names

Use the exact function/algorithm name from the spec:

```cpp
// ✅ CORRECT - Matches spec exactly
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#suffering-from-being-missing
bool HTMLInputElement::suffering_from_being_missing()
{
    // Implementation
}

// ❌ WRONG - Arbitrary name
bool HTMLInputElement::has_missing_constraint()
{
    // Implementation
}
```

### Variable Names

Use variable names from spec algorithms:

```cpp
// Spec says: "Let oldDocument be node's node document"
// ✅ CORRECT
auto old_document = node->document();

// ❌ WRONG
auto prev_doc = node->document();
```

### Converting Spec Names to C++

Spec naming → C++ naming rules:

1. **Spaces to underscores**: "reset the insertion mode" → `reset_the_insertion_mode()`
2. **Lowercase**: "HTMLElement" stays CamelCase (type name), but "html element" → `html_element`
3. **Hyphens to underscores**: "pre-insert" → `pre_insert()`
4. **Remove articles**: Usually keep them for exact match ("get the parent" → `get_the_parent()`)

## Comment Structure

### Spec Link

Every function implementing a spec algorithm must start with a spec link:

```cpp
// https://dom.spec.whatwg.org/#dom-node-appendchild
WebIDL::ExceptionOr<GC::Ref<Node>> Node::append_child(GC::Ref<Node> node)
{
    // Implementation
}
```

Spec link format:
- Use the canonical URL (prefer `.spec.whatwg.org` for WHATWG specs)
- Link to the specific algorithm section using `#fragment-id`
- Put link on the line immediately before function definition

### Algorithm Steps

Number each step exactly as in the spec:

```cpp
// https://dom.spec.whatwg.org/#concept-node-pre-insert
WebIDL::ExceptionOr<GC::Ref<Node>> Node::pre_insert(GC::Ref<Node> node, GC::Ptr<Node> child)
{
    // 1. Ensure pre-insertion validity of node into parent before child.
    TRY(ensure_pre_insertion_validity(node, child));

    // 2. Let referenceChild be child.
    auto reference_child = child;

    // 3. If referenceChild is node, then set referenceChild to node's next sibling.
    if (reference_child == node)
        reference_child = node->next_sibling();

    // 4. Insert node into parent before referenceChild.
    insert(node, reference_child);

    // 5. Return node.
    return node;
}
```

### Nested Steps

For nested steps, indent and number appropriately:

```cpp
// https://example.spec/#complex-algorithm
void complex_algorithm()
{
    // 1. Let result be null.
    GC::Ptr<Object> result;

    // 2. For each item in items:
    for (auto& item : items) {
        // 2.1. If item is null, continue.
        if (!item)
            continue;

        // 2.2. Otherwise:
        // 2.2.1. Process item.
        process(item);

        // 2.2.2. If item is valid, then:
        if (item->is_valid()) {
            // 2.2.2.1. Set result to item.
            result = item;

            // 2.2.2.2. Break.
            break;
        }
    }

    // 3. Return result.
    return result;
}
```

### FIXME Comments

When a step can't be implemented yet:

```cpp
// https://fetch.spec.whatwg.org/#main-fetch
void main_fetch()
{
    // 1. Let response be null.
    GC::Ptr<Response> response;

    // 2. Let fetchParams be null.
    GC::Ptr<FetchParams> fetch_params;

    // FIXME: 3. Perform a connection pool check for fetchParams.
    //        (Requires connection pool implementation)

    // 4. Run the remaining steps in parallel.
    // ...
}
```

### Non-Standard Code

Mark non-standard optimizations or workarounds:

```cpp
void algorithm()
{
    // 1. Spec step one.
    do_step_one();

    // OPTIMIZATION: Fast path for common case.
    if (is_common_case())
        return fast_path();

    // 2. Spec step two.
    do_step_two();
}
```

## Type Mapping

### WebIDL Types → C++ Types

| WebIDL Type | C++ Type | Notes |
|-------------|----------|-------|
| `undefined` | `void` | For return type only |
| `any` | `JS::Value` | Untyped JS value |
| `boolean` | `bool` | |
| `byte` | `i8` | |
| `octet` | `u8` | |
| `short` | `i16` | |
| `unsigned short` | `u16` | |
| `long` | `i32` | |
| `unsigned long` | `u32` | |
| `long long` | `i64` | |
| `unsigned long long` | `u64` | |
| `float` | `float` | |
| `unrestricted float` | `float` | Allows NaN/Infinity |
| `double` | `double` | |
| `unrestricted double` | `double` | Allows NaN/Infinity |
| `DOMString` | `String` | AK::String |
| `ByteString` | `ByteString` | |
| `USVString` | `String` | Scalar values only |
| `sequence<T>` | `Vector<T>` | |
| `record<K, V>` | `OrderedHashMap<K, V>` | |
| `Promise<T>` | `GC::Ref<JS::Promise>` | |
| `FrozenArray<T>` | `Vector<T>` | Immutable |
| Interface | `GC::Ptr<Interface>` or `GC::Ref<Interface>` | |
| Nullable type `T?` | `GC::Ptr<T>` or `Optional<T>` | Depends on type |
| `[EnforceRange] long` | `i32` | Range checking in bindings |
| `[Clamp] octet` | `u8` | Clamping in bindings |

### Algorithm Patterns → C++ Patterns

**Assertions**:
```cpp
// Spec: "Assert: node is not null."
VERIFY(node);
```

**Conditionals**:
```cpp
// Spec: "If condition holds, then:"
if (condition) {
    // Steps
}

// Spec: "Otherwise:"
else {
    // Steps
}
```

**Loops**:
```cpp
// Spec: "For each item in list:"
for (auto& item : list) {
    // Steps
}

// Spec: "While condition holds:"
while (condition) {
    // Steps
}
```

**Early Returns**:
```cpp
// Spec: "Return value."
return value;

// Spec: "Return the result of running algorithm."
return run_algorithm();
```

**Let Statements**:
```cpp
// Spec: "Let value be 42."
auto value = 42;

// Spec: "Let element be null."
GC::Ptr<Element> element;

// Spec: "Let result be the result of calling foo."
auto result = foo();
```

**Set Statements**:
```cpp
// Spec: "Set value to 100."
value = 100;

// Spec: "Set element to document's body element."
element = document->body();
```

## Error Handling

### Throwing Exceptions

Follow spec exception throwing exactly:

```cpp
// Spec: "Throw a TypeError."
return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, "Error message"sv };

// Spec: "Throw an 'InvalidStateError' DOMException."
return WebIDL::DOMException::create(realm(), WebIDL::ExceptionCode::InvalidStateError, "Invalid state"_string);

// Spec: "If node is not an element, throw a TypeError."
if (!node->is_element())
    return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, "Node must be an element"sv };
```

### Propagating Exceptions

Use `TRY()` for error propagation:

```cpp
// Spec: "Let result be the result of running algorithm (may throw)."
auto result = TRY(algorithm());

// Spec: "If calling foo throws, rethrow."
TRY(foo());
```

### OOM Handling

Convert OOM to JavaScript exceptions:

```cpp
// Creating buffers that may fail
auto buffer = TRY_OR_THROW_OOM(realm(), ByteBuffer::create_uninitialized(size));

// Appending to vectors
TRY_OR_THROW_OOM(realm(), vector.try_append(item));
```

## Algorithm Fidelity

### Preserve Spec Order

Even if steps seem redundant, preserve the order:

```cpp
// ❌ WRONG - Reordered for "efficiency"
void algorithm()
{
    // Check condition first (optimization)
    if (!condition)
        return;

    // Then do step 1
    do_step_one();

    // Then do step 2
    do_step_two();
}

// ✅ CORRECT - Matches spec order
void algorithm()
{
    // 1. Do step one.
    do_step_one();

    // 2. Do step two.
    do_step_two();

    // 3. If condition is false, return.
    if (!condition)
        return;
}
```

Only deviate with clear optimization comments:

```cpp
void algorithm()
{
    // OPTIMIZATION: Early return for common case (spec would return at step 5).
    if (common_case)
        return;

    // 1. Spec step one.
    // 2. Spec step two.
    // ...
}
```

### Preserve Spec Assertions

Keep spec assertions as `VERIFY()`:

```cpp
// Spec: "Assert: element is not null."
VERIFY(element);

// Spec: "Assert: index is less than length."
VERIFY(index < length);
```

These help catch implementation bugs and document preconditions.

### Spec Notes

Preserve important spec notes as comments:

```cpp
// NOTE: This algorithm is not observable from JavaScript.
void internal_algorithm()
{
    // ...
}

// NOTE: The spec says this step is for compatibility with legacy content.
void legacy_behavior()
{
    // ...
}
```

## Testing Spec Compliance

### Text Tests for Algorithms

Create tests that verify algorithm behavior:

```html
<!DOCTYPE html>
<script src="../include.js"></script>
<script>
    test(() => {
        // Test spec algorithm: "append child"
        const parent = document.createElement('div');
        const child = document.createElement('span');

        parent.appendChild(child);

        // Verify spec postconditions
        assert_equals(child.parentNode, parent);
        assert_equals(parent.firstChild, child);
    });
</script>
```

### Web Platform Tests

Run WPT (Web Platform Tests) to verify spec compliance:

```bash
./Meta/WPT.sh run --log results.log
./Meta/WPT.sh compare results.log
```

### Manual Testing

Test with real websites to verify behavior matches other browsers.

## Spec Version Tracking

### Spec Status

Different spec statuses affect implementation priorities:

1. **Living Standard (WHATWG)**: Implement as-is
2. **W3C Recommendation**: Stable, implement fully
3. **W3C Candidate Recommendation**: May change, mark as experimental
4. **W3C Working Draft**: Unstable, low priority

### Spec Changes

When specs change:

1. Update implementation to match new spec
2. Update spec links in comments
3. Update or add tests
4. Add changelog entry if user-visible

## Common Spec Patterns

### Abstract Operations

Specs define abstract operations (not exposed to JS):

```cpp
// https://infra.spec.whatwg.org/#list-contain
template<typename T>
bool list_contains(Vector<T> const& list, T const& item)
{
    return list.contains_slow(item);
}
```

### Feature Detection

Implement feature detection as specified:

```cpp
// https://example.spec/#feature-detection
bool supports_feature(StringView feature)
{
    // 1. If feature is "example", return true.
    if (feature == "example"sv)
        return true;

    // 2. Otherwise, return false.
    return false;
}
```

### Parallel Steps

Specs say "in parallel" for async operations:

```cpp
// Spec: "In parallel, fetch the resource."
void fetch_in_parallel()
{
    // Queue a task to the network task source
    queue_a_task([this] {
        auto resource = fetch_resource();
        // Process result
    });
}
```

## Resources

- WHATWG Standards: https://spec.whatwg.org/
- W3C Standards: https://www.w3.org/TR/
- Web Platform Tests: https://github.com/web-platform-tests/wpt
- Can I Use: https://caniuse.com/
- MDN Web Docs: https://developer.mozilla.org/
