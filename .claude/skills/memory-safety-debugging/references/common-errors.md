# Common Memory Safety Errors in Ladybird

Catalog of frequent memory bugs in browser development with detection and prevention.

## 1. Use-After-Free Errors

### 1.1 DOM Node Lifetime Issues

**Description:** DOM nodes are reference-counted, but raw pointers can outlive the object.

**Detection:** ASAN `heap-use-after-free`

**Example:**
```cpp
// WRONG:
void process_element(Element* element) {
    auto* parent = element->parent();
    element->remove();  // May delete element
    parent->update();   // May delete parent
    element->update();  // USE-AFTER-FREE
}
```

**Fix:**
```cpp
// CORRECT:
void process_element(Element& element) {
    auto protected_element = NonnullRefPtr(element);
    auto parent = element.parent();
    if (!parent)
        return;
    auto protected_parent = NonnullRefPtr(*parent);

    element.remove();
    protected_parent->update();
    protected_element->update();  // Safe
}
```

**Prevention:**
- Use `RefPtr<T>` and `NonnullRefPtr<T>` for DOM objects
- Never store raw pointers to ref-counted objects
- Keep `NonnullRefPtr` protector when calling methods that may delete

---

### 1.2 Event Callback After Object Destruction

**Description:** Async callbacks fire after object is destroyed.

**Detection:** ASAN `heap-use-after-free`

**Example:**
```cpp
// WRONG:
class RequestHandler {
    void send_request() {
        auto callback = [this]() {
            this->handle_response();  // 'this' may be deleted
        };
        m_connection->async_send(callback);
    }
};
```

**Fix:**
```cpp
// CORRECT:
class RequestHandler : public RefCounted<RequestHandler> {
    void send_request() {
        auto callback = [strong_this = NonnullRefPtr(*this)]() {
            strong_this->handle_response();  // Safe
        };
        m_connection->async_send(callback);
    }
};
```

**Prevention:**
- Capture `NonnullRefPtr` in async lambdas
- Inherit from `RefCounted<T>` for async objects
- Use `WeakPtr` if object lifetime is uncertain

---

### 1.3 Iterator Invalidation

**Description:** Modifying container while iterating invalidates iterators.

**Detection:** ASAN `heap-use-after-free`, segfault

**Example:**
```cpp
// WRONG:
for (auto& child : m_children) {
    if (should_remove(child))
        m_children.remove(child);  // Iterator invalidated
}
```

**Fix:**
```cpp
// CORRECT - collect indices:
Vector<size_t> to_remove;
for (size_t i = 0; i < m_children.size(); ++i) {
    if (should_remove(m_children[i]))
        to_remove.append(i);
}
for (auto i : to_remove.in_reverse())
    m_children.remove(i);

// Or use remove_all_matching:
m_children.remove_all_matching([](auto& child) {
    return should_remove(child);
});
```

**Prevention:**
- Don't modify containers while iterating
- Use `remove_all_matching()` for conditional removal
- Collect items to remove, then remove in separate loop

---

## 2. Buffer Overflow Errors

### 2.1 Fixed-Size Buffer Overflow

**Description:** Writing beyond fixed-size buffer bounds.

**Detection:** ASAN `stack-buffer-overflow` or `heap-buffer-overflow`

**Example:**
```cpp
// WRONG:
void parse_input(char const* input, size_t length) {
    char buffer[256];
    memcpy(buffer, input, length);  // Overflow if length > 256
}
```

**Fix:**
```cpp
// CORRECT - use Vector:
void parse_input(char const* input, size_t length) {
    Vector<char> buffer;
    buffer.ensure_capacity(length);
    for (size_t i = 0; i < length; ++i)
        buffer.append(input[i]);
}

// Or with bounds check:
void parse_input(char const* input, size_t length) {
    char buffer[256];
    size_t copy_length = min(length, sizeof(buffer));
    memcpy(buffer, input, copy_length);
}
```

**Prevention:**
- Use `Vector<T>` instead of fixed arrays
- Always bounds-check before array access
- Use `StringView` for string processing

---

### 2.2 Off-By-One Errors

**Description:** Loop or index calculation off by one.

**Detection:** ASAN `*-buffer-overflow`, UBSAN `index-out-of-bounds`

**Example:**
```cpp
// WRONG:
for (size_t i = 0; i <= buffer.size(); ++i) {  // <= instead of <
    process(buffer[i]);  // Overflow on last iteration
}
```

**Fix:**
```cpp
// CORRECT:
for (size_t i = 0; i < buffer.size(); ++i) {
    process(buffer[i]);
}

// Or use range-based for:
for (auto& item : buffer) {
    process(item);
}
```

**Prevention:**
- Use `<` not `<=` for loop bounds
- Prefer range-based for loops
- Use `.at()` instead of `[]` for bounds-checked access

---

### 2.3 String Buffer Overflow

**Description:** String operations exceed buffer size.

**Detection:** ASAN `heap-buffer-overflow`

**Example:**
```cpp
// WRONG:
char* create_message(char const* name) {
    char* msg = new char[100];
    sprintf(msg, "Hello, %s!", name);  // Overflow if name is long
    return msg;
}
```

**Fix:**
```cpp
// CORRECT:
String create_message(StringView name) {
    return String::formatted("Hello, {}!", name);
}
```

**Prevention:**
- Use `String` and `StringBuilder` instead of `char*`
- Never use `sprintf`, use `snprintf` or `String::formatted`
- Use `StringView` for string parameters

---

## 3. Integer Overflow Errors

### 3.1 IPC Size Calculation Overflow

**Description:** Integer overflow in IPC message size calculation.

**Detection:** UBSAN `signed-integer-overflow`, ASAN (if causes allocation)

**Example:**
```cpp
// WRONG:
ErrorOr<void> handle_allocate(u32 count, u32 size) {
    u32 total = count * size;  // Overflow: 0x10000 * 0x10000 = 0
    auto buffer = TRY(ByteBuffer::create_uninitialized(total));
    // Small allocation, later buffer overflow
}
```

**Fix:**
```cpp
// CORRECT:
ErrorOr<void> handle_allocate(u32 count, u32 size) {
    auto total = AK::checked_mul(count, size);
    if (total.has_overflow())
        return Error::from_string_literal("Size overflow");

    auto buffer = TRY(ByteBuffer::create_uninitialized(total.value()));
}
```

**Prevention:**
- Use `AK::checked_add`, `checked_mul`, `checked_sub`
- Validate IPC message sizes and counts
- Set reasonable maximum limits

---

### 3.2 Array Index Calculation Overflow

**Description:** Index calculation overflows, causes out-of-bounds access.

**Detection:** UBSAN `signed-integer-overflow`, ASAN `*-buffer-overflow`

**Example:**
```cpp
// WRONG:
u8 get_pixel(u32 x, u32 y, u32 width) {
    u32 index = y * width + x;  // May overflow
    return m_pixels[index];
}
```

**Fix:**
```cpp
// CORRECT:
ErrorOr<u8> get_pixel(u32 x, u32 y, u32 width) {
    auto index_y = AK::checked_mul(y, width);
    if (index_y.has_overflow())
        return Error::from_string_literal("Y overflow");

    auto index = AK::checked_add(index_y.value(), x);
    if (index.has_overflow())
        return Error::from_string_literal("Index overflow");

    if (index.value() >= m_pixels.size())
        return Error::from_string_literal("Out of bounds");

    return m_pixels[index.value()];
}
```

**Prevention:**
- Use checked arithmetic for index calculations
- Validate coordinates before access
- Use 2D data structures instead of flattened arrays

---

## 4. Memory Leak Errors

### 4.1 Missing Smart Pointer

**Description:** Allocated memory never freed.

**Detection:** LSAN `definitely lost`

**Example:**
```cpp
// WRONG:
void load_resource(URL const& url) {
    auto* loader = new ResourceLoader(url);  // Leaked
    loader->start();
}
```

**Fix:**
```cpp
// CORRECT:
void load_resource(URL const& url) {
    auto loader = make<ResourceLoader>(url);
    loader->start();
}  // Auto-deleted here
```

**Prevention:**
- Always use `OwnPtr<T>` or `RefPtr<T>`
- Never use raw `new`/`delete`
- Use `make<T>()` for construction

---

### 4.2 Reference Cycles

**Description:** Objects reference each other, preventing deletion.

**Detection:** LSAN (may not detect), manual heap profiling

**Example:**
```cpp
// WRONG:
class Node : public RefCounted<Node> {
    RefPtr<Node> m_parent;
    Vector<NonnullRefPtr<Node>> m_children;
    // Cycle: parent refs child, child refs parent
};
```

**Fix:**
```cpp
// CORRECT:
class Node : public RefCounted<Node> {
    WeakPtr<Node> m_parent;  // Weak breaks cycle
    Vector<NonnullRefPtr<Node>> m_children;
};
```

**Prevention:**
- Use `WeakPtr<T>` for back-references
- Document ownership relationships
- Review object graphs for cycles

---

### 4.3 Exception/Error Path Leak

**Description:** Resource leaked when error occurs.

**Detection:** LSAN `definitely lost`

**Example:**
```cpp
// WRONG:
ErrorOr<void> process() {
    int fd = TRY(open_file());
    TRY(process_data());  // If this fails, fd leaked
    close(fd);
    return {};
}
```

**Fix:**
```cpp
// CORRECT:
ErrorOr<void> process() {
    auto file = TRY(Core::File::open(...));
    TRY(process_data());
    // file auto-closed on error or success
    return {};
}
```

**Prevention:**
- Use RAII for all resources
- Never manual cleanup in error paths
- Use `ScopeGuard` for non-RAII cleanup

---

## 5. Null Pointer Errors

### 5.1 Unchecked Optional Access

**Description:** Accessing Optional without checking has_value().

**Detection:** UBSAN `null-pointer-dereference`, segfault

**Example:**
```cpp
// WRONG:
auto parent = element->parent();  // Returns Optional<RefPtr<Node>>
parent.value()->update();  // Crash if no parent
```

**Fix:**
```cpp
// CORRECT:
auto parent = element->parent();
if (parent.has_value())
    parent.value()->update();

// Or more idiomatic:
if (auto parent = element->parent())
    parent.value()->update();
```

**Prevention:**
- Always check `has_value()` before `value()`
- Use `value_or()` for default values
- Use `if (auto x = optional())` pattern

---

### 5.2 Unchecked Pointer Dereference

**Description:** Dereferencing pointer without null check.

**Detection:** UBSAN `null-pointer-dereference`, segfault

**Example:**
```cpp
// WRONG:
void update_parent(Node* node) {
    node->parent()->update();  // Crash if node or parent is null
}
```

**Fix:**
```cpp
// CORRECT:
void update_parent(Node* node) {
    if (!node)
        return;

    if (auto parent = node->parent())
        parent->update();
}

// Or use NonnullRefPtr if null is invalid:
void update_parent(NonnullRefPtr<Node> node) {
    if (auto parent = node->parent())
        parent->update();
}
```

**Prevention:**
- Check for null before dereference
- Use `NonnullRefPtr<T>` if null is invalid
- Use `Optional<NonnullRefPtr<T>>` for nullable references

---

## 6. Race Condition Errors

### 6.1 Unsynchronized Shared Data

**Description:** Multiple threads accessing shared data without synchronization.

**Detection:** TSAN `data race`

**Example:**
```cpp
// WRONG:
class Cache {
    HashMap<String, Value> m_cache;  // Accessed from multiple threads

    void insert(String key, Value value) {
        m_cache.set(move(key), move(value));  // RACE
    }

    Optional<Value> get(String const& key) {
        return m_cache.get(key);  // RACE
    }
};
```

**Fix:**
```cpp
// CORRECT:
class Cache {
    HashMap<String, Value> m_cache;
    mutable Threading::Mutex m_mutex;

    void insert(String key, Value value) {
        Threading::MutexLocker lock(m_mutex);
        m_cache.set(move(key), move(value));
    }

    Optional<Value> get(String const& key) {
        Threading::MutexLocker lock(m_mutex);
        return m_cache.get(key);
    }
};
```

**Prevention:**
- Protect shared data with `Threading::Mutex`
- Use `Atomic<T>` for simple types
- Document thread safety requirements

---

### 6.2 Lock-Free Race Conditions

**Description:** Non-atomic operations on shared variables.

**Detection:** TSAN `data race`

**Example:**
```cpp
// WRONG:
class Counter {
    int m_count { 0 };

    void increment() {
        m_count++;  // Not atomic!
    }

    int count() const {
        return m_count;  // RACE
    }
};
```

**Fix:**
```cpp
// CORRECT:
class Counter {
    Atomic<int> m_count { 0 };

    void increment() {
        m_count.fetch_add(1, AK::MemoryOrder::memory_order_relaxed);
    }

    int count() const {
        return m_count.load(AK::MemoryOrder::memory_order_relaxed);
    }
};
```

**Prevention:**
- Use `Atomic<T>` for lock-free counters/flags
- Specify memory order explicitly
- Use mutex for complex operations

---

## Detection Summary

| Error Type | ASAN | UBSAN | TSAN | MSAN | Valgrind |
|------------|------|-------|------|------|----------|
| Use-after-free | ✅ | ❌ | ❌ | ❌ | ✅ |
| Buffer overflow | ✅ | ✅ (bounds) | ❌ | ❌ | ✅ |
| Integer overflow | ❌ | ✅ | ❌ | ❌ | ❌ |
| Memory leak | ✅ (LSan) | ❌ | ❌ | ❌ | ✅ |
| Null pointer | ❌ | ✅ | ❌ | ❌ | ✅ |
| Race condition | ❌ | ❌ | ✅ | ❌ | ✅ (helgrind) |
| Uninitialized read | ❌ | ❌ | ❌ | ✅ | ✅ |

## Prevention Checklist

- [ ] Use `RefPtr<T>`/`NonnullRefPtr<T>` for shared ownership
- [ ] Use `OwnPtr<T>`/`NonnullOwnPtr<T>` for unique ownership
- [ ] Use `WeakPtr<T>` to break reference cycles
- [ ] Use `Vector<T>` instead of fixed arrays
- [ ] Use checked arithmetic for IPC and calculations
- [ ] Always check Optional before accessing
- [ ] Use `Threading::Mutex` or `Atomic<T>` for shared data
- [ ] Capture `NonnullRefPtr` in async callbacks
- [ ] Use RAII for all resource management
- [ ] Run sanitizers regularly during development
