---
name: memory-safety-debugging
description: Comprehensive guide for memory safety debugging in Ladybird using sanitizers (ASAN, UBSAN, MSAN, TSAN), Valgrind, and memory profiling tools
use-when: Debugging crashes, memory leaks, undefined behavior, race conditions, or performance issues. Interpreting sanitizer reports and fixing memory bugs
allowed-tools:
  - Bash
  - Read
  - Write
  - Grep
scripts:
  - run_with_asan.sh
  - run_with_valgrind.sh
  - analyze_sanitizer_report.sh
  - symbolicate_stack.sh
---

# Memory Safety Debugging for Ladybird

## Overview

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│   Detect    │ → │   Diagnose   │ → │     Fix     │
│  (Sanitize) │    │  (Analyze)   │    │  (Prevent)  │
└─────────────┘    └──────────────┘    └─────────────┘
        ↓                  ↓                   ↓
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│ ASAN/UBSAN/ │    │ Stack Trace  │    │    RAII/    │
│ MSAN/TSAN   │    │  Analysis    │    │ Smart Ptrs  │
└─────────────┘    └──────────────┘    └─────────────┘
```

Memory safety is critical in browser development. Ladybird's multi-process architecture involves:
- **IPC message handling** (buffer overflows, integer overflows)
- **DOM tree management** (use-after-free, reference cycles)
- **Parser state machines** (uninitialized memory, buffer overruns)
- **Multi-threaded rendering** (data races, deadlocks)

This skill covers detection, diagnosis, and prevention of memory safety issues.

## 1. Sanitizer Types and Usage

### 1.1 AddressSanitizer (ASAN) - Memory Errors

**Detects:**
- Heap/stack/global buffer overflows
- Use-after-free
- Use-after-return
- Use-after-scope
- Double-free
- Memory leaks (with LeakSanitizer)

**Build:**
```bash
# Use Sanitizers preset (includes ASAN + UBSAN)
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Or manual configuration
cmake -B Build/asan \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
```

**Run:**
```bash
# Basic run
./Build/sanitizers/bin/Ladybird

# With options
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 \
./Build/sanitizers/bin/Ladybird

# Suppress known issues
ASAN_OPTIONS=suppressions=/home/rbsmith4/ladybird/asan_suppressions.txt \
./Build/sanitizers/bin/Ladybird
```

**Example Output:**
```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60700000eff8 at pc 0x7f8b2c3d4e56 bp 0x7ffc8d9e1a40 sp 0x7ffc8d9e1a38
READ of size 8 at 0x60700000eff8 thread T0
    #0 0x7f8b2c3d4e55 in Web::DOM::Node::parent() LibWeb/DOM/Node.cpp:142
    #1 0x7f8b2c3d5123 in Web::DOM::Element::remove() LibWeb/DOM/Element.cpp:89
    #2 0x7f8b2c3d6789 in Web::HTML::HTMLElement::click() LibWeb/HTML/HTMLElement.cpp:45

0x60700000eff8 is located 8 bytes inside of 64-byte region [0x60700000eff0,0x60700000f030)
freed by thread T0 here:
    #0 0x7f8b2d123456 in operator delete(void*) (Ladybird+0x123456)
    #1 0x7f8b2c3d4abc in Web::DOM::Node::~Node() LibWeb/DOM/Node.cpp:67
    #2 0x7f8b2c3d5def in Web::DOM::Document::remove_from_tree() LibWeb/DOM/Document.cpp:234
```

### 1.2 UndefinedBehaviorSanitizer (UBSAN) - Undefined Behavior

**Detects:**
- Signed integer overflow
- Null pointer dereference
- Misaligned pointer use
- Division by zero
- Array bounds violations
- Type mismatches
- Invalid enum values

**Build:**
```bash
# Included in Sanitizers preset
cmake --preset Sanitizers

# Or standalone
cmake -B Build/ubsan \
    -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g"
```

**Run:**
```bash
# With error printing
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 \
./Build/sanitizers/bin/Ladybird

# Stop on first error
UBSAN_OPTIONS=halt_on_error=1 \
./Build/sanitizers/bin/Ladybird
```

**Example Output:**
```
LibIPC/Decoder.cpp:45:18: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
    #0 0x7f8b2c123456 in IPC::Decoder::decode<int>() LibIPC/Decoder.cpp:45
    #1 0x7f8b2c234567 in RequestServer::ConnectionFromClient::handle_start_request() Services/RequestServer/ConnectionFromClient.cpp:89
```

### 1.3 MemorySanitizer (MSAN) - Uninitialized Memory

**Detects:**
- Use of uninitialized memory
- Uninitialized variables
- Uninitialized heap allocations

**Build:**
```bash
# MSAN requires full rebuild of all dependencies
cmake -B Build/msan \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=memory -fno-omit-frame-pointer -g -fPIE" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=memory -pie"
```

**Run:**
```bash
MSAN_OPTIONS=poison_in_dtor=1 \
./Build/msan/bin/Ladybird
```

**Example Output:**
```
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x7f8b2c123456 in Web::CSS::Parser::parse_color() LibWeb/CSS/Parser/Parser.cpp:567
    #1 0x7f8b2c234567 in Web::CSS::StyleProperties::set_property() LibWeb/CSS/StyleProperties.cpp:89

  Uninitialized value was created by an allocation of 'rgba' in the stack frame
    #0 0x7f8b2c123000 in Web::CSS::Parser::parse_color() LibWeb/CSS/Parser/Parser.cpp:560
```

### 1.4 ThreadSanitizer (TSAN) - Race Conditions

**Detects:**
- Data races
- Thread safety violations
- Deadlocks (limited)

**Build:**
```bash
cmake -B Build/tsan \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

**Run:**
```bash
TSAN_OPTIONS=second_deadlock_stack=1 \
./Build/tsan/bin/Ladybird
```

**Example Output:**
```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7b0400001234 by thread T1:
    #0 0x7f8b2c123456 in Web::ResourceLoader::did_load_resource() LibWeb/Loader/ResourceLoader.cpp:89
    #1 0x7f8b2c234567 in RequestServer::ConnectionFromClient::did_finish_request() Services/RequestServer/ConnectionFromClient.cpp:156

  Previous read of size 8 at 0x7b0400001234 by main thread:
    #0 0x7f8b2c345678 in Web::ResourceLoader::is_loading() LibWeb/Loader/ResourceLoader.cpp:45
    #1 0x7f8b2c456789 in Web::Document::update() LibWeb/DOM/Document.cpp:234
```

## 2. Building with Sanitizers

### 2.1 Sanitizers Preset (Recommended)

Ladybird provides a pre-configured `Sanitizers` preset:

```bash
# Configure
cmake --preset Sanitizers

# Build
cmake --build Build/sanitizers

# Run browser
./Build/sanitizers/bin/Ladybird

# Run WebContent directly
./Build/sanitizers/bin/WebContent

# Run tests
ctest --preset Sanitizers

# Run specific test
./Build/sanitizers/bin/TestIPC
```

**What's Included:**
- AddressSanitizer (ASAN)
- UndefinedBehaviorSanitizer (UBSAN)
- LeakSanitizer (part of ASAN)
- Debug symbols (`-g`)
- Optimized for debugging (`-O1`)
- Frame pointers (`-fno-omit-frame-pointer`)

### 2.2 Custom Sanitizer Builds

```bash
# ASAN + UBSAN + extra checks
cmake -B Build/asan-strict \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 -fno-optimize-sibling-calls" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

# MSAN (requires clean build)
cmake -B Build/msan \
    -DCMAKE_CXX_FLAGS="-fsanitize=memory -fno-omit-frame-pointer -g -O1 -fPIE" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=memory -pie"

# TSAN
cmake -B Build/tsan \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g -O1" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"

# Combine ASAN + UBSAN + fuzzing
cmake -B Build/fuzz-sanitize \
    -DENABLE_FUZZING=ON \
    -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer,address,undefined -g -O1"
```

### 2.3 Sanitizer Environment Variables

**ASAN Options:**
```bash
export ASAN_OPTIONS="\
detect_leaks=1:\
strict_string_checks=1:\
detect_stack_use_after_return=1:\
check_initialization_order=1:\
strict_init_order=1:\
detect_invalid_pointer_pairs=2:\
print_stacktrace=1:\
symbolize=1:\
halt_on_error=0:\
abort_on_error=0:\
log_path=asan.log"
```

**UBSAN Options:**
```bash
export UBSAN_OPTIONS="\
print_stacktrace=1:\
halt_on_error=0:\
suppressions=/home/rbsmith4/ladybird/ubsan_suppressions.txt"
```

**LSAN Options (Leak Sanitizer):**
```bash
export LSAN_OPTIONS="\
suppressions=/home/rbsmith4/ladybird/lsan_suppressions.txt:\
print_suppressions=0"
```

**TSAN Options:**
```bash
export TSAN_OPTIONS="\
second_deadlock_stack=1:\
history_size=7:\
suppressions=/home/rbsmith4/ladybird/tsan_suppressions.txt"
```

## 3. Interpreting Sanitizer Reports

### 3.1 ASAN: Heap Use-After-Free

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60700000eff8 at pc 0x7f8b2c3d4e56 bp 0x7ffc8d9e1a40 sp 0x7ffc8d9e1a38
READ of size 8 at 0x60700000eff8 thread T0
    #0 0x7f8b2c3d4e55 in Web::DOM::Node::parent() LibWeb/DOM/Node.cpp:142
    #1 0x7f8b2c3d5123 in Web::DOM::Element::remove() LibWeb/DOM/Element.cpp:89

0x60700000eff8 is located 8 bytes inside of 64-byte region [0x60700000eff0,0x60700000f030)
freed by thread T0 here:
    #0 0x7f8b2d123456 in operator delete(void*) (Ladybird+0x123456)
    #1 0x7f8b2c3d4abc in Web::DOM::Node::~Node() LibWeb/DOM/Node.cpp:67
```

**Analysis:**
1. **Error Type:** `heap-use-after-free` - accessing freed memory
2. **Location:** `0x60700000eff8` - address of freed memory
3. **Access:** `READ of size 8` - 8-byte read (likely pointer dereference)
4. **Use Site:** `Web::DOM::Node::parent()` at line 142 - where we accessed freed memory
5. **Free Site:** `Web::DOM::Node::~Node()` at line 67 - where memory was freed
6. **Root Cause:** Dangling pointer - holding reference to deleted Node

**Fix Pattern:**
```cpp
// Before (WRONG):
void HTMLElement::click() {
    Node* parent = m_node->parent();  // Raw pointer
    m_node->remove();                 // Deletes m_node
    parent->update();                 // USE-AFTER-FREE! parent might be deleted
}

// After (CORRECT):
void HTMLElement::click() {
    NonnullRefPtr<Node> parent = *m_node->parent();  // RefPtr keeps alive
    m_node->remove();
    parent->update();  // Safe - RefPtr prevents deletion
}
```

### 3.2 ASAN: Stack Buffer Overflow

```
=================================================================
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ffc8d9e1a50 at pc 0x7f8b2c123456 bp 0x7ffc8d9e1a00 sp 0x7ffc8d9e19f8
WRITE of size 4 at 0x7ffc8d9e1a50 thread T0
    #0 0x7f8b2c123455 in Web::CSS::Parser::parse_selector() LibWeb/CSS/Parser/Parser.cpp:234

0x7ffc8d9e1a50 is located in stack of thread T0 at offset 80 in frame
    #0 0x7f8b2c123000 in Web::CSS::Parser::parse_selector() LibWeb/CSS/Parser/Parser.cpp:220

  This frame has 1 object(s):
    [32, 80) 'buffer' (line 225) <== Memory access at offset 80 overflows this variable
```

**Analysis:**
1. **Error Type:** `stack-buffer-overflow` - writing beyond stack buffer
2. **Buffer Size:** 48 bytes `[32, 80)` - from offset 32 to 80
3. **Access:** Writing at offset 80 - exactly at end (off-by-one error)
4. **Variable:** `buffer` at line 225

**Fix Pattern:**
```cpp
// Before (WRONG):
void parse_selector() {
    char buffer[48];
    for (size_t i = 0; i <= 48; ++i) {  // Off-by-one: should be i < 48
        buffer[i] = input[i];
    }
}

// After (CORRECT):
void parse_selector() {
    Vector<char> buffer;
    buffer.ensure_capacity(48);
    for (size_t i = 0; i < input.size() && i < 48; ++i) {
        buffer.append(input[i]);
    }
}
```

### 3.3 ASAN: Memory Leak

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 128 bytes in 2 object(s) allocated from:
    #0 0x7f8b2d123456 in operator new(unsigned long) (Ladybird+0x123456)
    #1 0x7f8b2c234567 in Web::ResourceLoader::create() LibWeb/Loader/ResourceLoader.cpp:45
    #2 0x7f8b2c345678 in Web::Document::load_resource() LibWeb/DOM/Document.cpp:156

SUMMARY: AddressSanitizer: 128 byte(s) leaked in 2 allocation(s).
```

**Analysis:**
1. **Leak Size:** 128 bytes in 2 objects
2. **Allocation Site:** `Web::ResourceLoader::create()` line 45
3. **Call Chain:** Document::load_resource() → ResourceLoader::create()
4. **Root Cause:** Objects allocated but never deleted

**Fix Pattern:**
```cpp
// Before (WRONG):
void Document::load_resource(URL const& url) {
    auto* loader = new ResourceLoader(url);  // Leaked!
    loader->start();
}

// After (CORRECT - OwnPtr):
void Document::load_resource(URL const& url) {
    auto loader = make<ResourceLoader>(url);  // OwnPtr auto-deletes
    loader->start();
    // loader deleted here automatically
}

// Or (CORRECT - member variable):
class Document {
    OwnPtr<ResourceLoader> m_loader;
};

void Document::load_resource(URL const& url) {
    m_loader = make<ResourceLoader>(url);
    m_loader->start();
}
```

### 3.4 UBSAN: Signed Integer Overflow

```
LibIPC/Decoder.cpp:45:18: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
    #0 0x7f8b2c123456 in IPC::Decoder::decode<int>() LibIPC/Decoder.cpp:45
    #1 0x7f8b2c234567 in RequestServer::ConnectionFromClient::handle_start_request() Services/RequestServer/ConnectionFromClient.cpp:89
```

**Analysis:**
1. **Error Type:** Signed integer overflow
2. **Values:** `2147483647 + 1` (INT_MAX + 1)
3. **Location:** IPC::Decoder::decode<int>() line 45
4. **Context:** IPC message handling - **SECURITY CRITICAL**

**Fix Pattern:**
```cpp
// Before (WRONG):
template<>
ErrorOr<int> Decoder::decode() {
    int value;
    TRY(m_stream.read_value(value));
    return value + 1;  // OVERFLOW!
}

// After (CORRECT - Checked arithmetic):
template<>
ErrorOr<int> Decoder::decode() {
    int value;
    TRY(m_stream.read_value(value));

    // Use AK::checked_add
    auto result = AK::checked_add(value, 1);
    if (result.has_overflow())
        return Error::from_string_literal("Integer overflow in IPC decode");

    return result.value();
}
```

### 3.5 UBSAN: Null Pointer Dereference

```
LibWeb/DOM/Element.cpp:89:5: runtime error: member call on null pointer of type 'Web::DOM::Node'
    #0 0x7f8b2c123456 in Web::DOM::Element::remove() LibWeb/DOM/Element.cpp:89
    #1 0x7f8b2c234567 in Web::HTML::HTMLElement::click() LibWeb/HTML/HTMLElement.cpp:45
```

**Fix Pattern:**
```cpp
// Before (WRONG):
void Element::remove() {
    m_parent->remove_child(this);  // m_parent might be null
}

// After (CORRECT):
void Element::remove() {
    if (!m_parent)
        return;
    m_parent->remove_child(this);
}

// Or (BETTER - use Optional):
Optional<NonnullRefPtr<Node>> parent() const { return m_parent; }

void Element::remove() {
    if (auto parent = this->parent())
        parent.value()->remove_child(this);
}
```

### 3.6 TSAN: Data Race

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7b0400001234 by thread T1:
    #0 0x7f8b2c123456 in Web::ResourceLoader::did_load_resource() LibWeb/Loader/ResourceLoader.cpp:89
  Previous read of size 8 at 0x7b0400001234 by main thread:
    #0 0x7f8b2c345678 in Web::ResourceLoader::is_loading() LibWeb/Loader/ResourceLoader.cpp:45
```

**Fix Pattern:**
```cpp
// Before (WRONG - race condition):
class ResourceLoader {
    bool m_is_loading { false };  // Accessed from multiple threads!
};

void did_load_resource() {  // Thread T1
    m_is_loading = true;
}

bool is_loading() const {   // Main thread
    return m_is_loading;
}

// After (CORRECT - mutex protection):
class ResourceLoader {
    mutable Threading::Mutex m_mutex;
    bool m_is_loading { false };
};

void did_load_resource() {
    Threading::MutexLocker locker(m_mutex);
    m_is_loading = true;
}

bool is_loading() const {
    Threading::MutexLocker locker(m_mutex);
    return m_is_loading;
}

// Or (BETTER - atomic):
class ResourceLoader {
    Atomic<bool> m_is_loading { false };
};

void did_load_resource() {
    m_is_loading.store(true, AK::MemoryOrder::memory_order_release);
}

bool is_loading() const {
    return m_is_loading.load(AK::MemoryOrder::memory_order_acquire);
}
```

## 4. Common Memory Safety Issues in Ladybird

### 4.1 Use-After-Free with RefPtr/NonnullRefPtr

**Problem:** DOM nodes reference each other. When nodes are removed, dangling pointers can remain.

**Pattern:**
```cpp
// WRONG:
void process_nodes(Node* parent, Node* child) {
    parent->remove_child(child);  // May delete child
    child->update();              // USE-AFTER-FREE
}

// CORRECT:
void process_nodes(Node* parent, NonnullRefPtr<Node> child) {
    parent->remove_child(child);  // child RefPtr keeps it alive
    child->update();              // Safe
}
// child deleted here when RefPtr goes out of scope
```

**Detection:**
- ASAN will catch at runtime
- Static analysis tools (clang-tidy)

**Prevention:**
- Use `RefPtr<T>` and `NonnullRefPtr<T>` for shared ownership
- Use `OwnPtr<T>` and `NonnullOwnPtr<T>` for unique ownership
- Never store raw pointers to ref-counted objects

### 4.2 Memory Leaks with OwnPtr/NonnullOwnPtr

**Problem:** Allocated memory is never freed, accumulates over time.

**Pattern:**
```cpp
// WRONG:
class Document {
    ResourceLoader* m_loader { nullptr };

    void load() {
        m_loader = new ResourceLoader();  // LEAK when load() called multiple times
    }
};

// CORRECT:
class Document {
    OwnPtr<ResourceLoader> m_loader;

    void load() {
        m_loader = make<ResourceLoader>();  // Old loader auto-deleted
    }
};
```

**Detection:**
- LSAN (part of ASAN) with `detect_leaks=1`
- Valgrind memcheck
- Manual inspection with `valgrind --leak-check=full`

**Prevention:**
- Always use `OwnPtr<T>` for unique ownership
- Never use raw `new`/`delete` - use `make<T>()`
- Check destruction order for circular references

### 4.3 Buffer Overflows in Parsers

**Problem:** Parsers process untrusted input, bounds checking is critical.

**Pattern:**
```cpp
// WRONG:
void parse_html(char const* input, size_t length) {
    char buffer[256];
    memcpy(buffer, input, length);  // OVERFLOW if length > 256
}

// CORRECT (use Vector):
void parse_html(char const* input, size_t length) {
    Vector<char> buffer;
    buffer.ensure_capacity(length);
    for (size_t i = 0; i < length; ++i)
        buffer.append(input[i]);
}

// CORRECT (bounds check):
void parse_html(char const* input, size_t length) {
    char buffer[256];
    size_t copy_length = min(length, sizeof(buffer));
    memcpy(buffer, input, copy_length);
}
```

**Detection:**
- ASAN detects all buffer overflows
- UBSAN detects array bounds violations
- Fuzzing with ASAN (see fuzzing-workflow skill)

**Prevention:**
- Use `Vector<T>` instead of fixed-size arrays
- Always bounds-check before array access
- Use `StringView` instead of `char*` for string parsing

### 4.4 Integer Overflows in IPC

**Problem:** IPC messages contain attacker-controlled integers. Overflow can bypass validation.

**Pattern:**
```cpp
// WRONG:
ErrorOr<void> handle_allocate(u32 size) {
    u32 total_size = size + sizeof(Header);  // OVERFLOW: size = 0xFFFFFFFF
    auto* buffer = new u8[total_size];       // Tiny allocation!
    // ... write beyond buffer bounds
}

// CORRECT (checked arithmetic):
ErrorOr<void> handle_allocate(u32 size) {
    auto total_size = AK::checked_add(size, sizeof(Header));
    if (total_size.has_overflow())
        return Error::from_string_literal("Size overflow");

    auto buffer = TRY(ByteBuffer::create_uninitialized(total_size.value()));
    // ...
}
```

**Detection:**
- UBSAN with `signed-integer-overflow` and `unsigned-integer-overflow`
- Fuzzing IPC messages (see fuzzing-workflow skill)

**Prevention:**
- Use `AK::checked_add`, `AK::checked_mul`, etc.
- Validate all IPC message sizes and counts
- Set maximum limits on allocations

### 4.5 Reference Cycles in DOM

**Problem:** DOM nodes reference each other, creating cycles that prevent deletion.

**Pattern:**
```cpp
// WRONG:
class Node : public RefCounted<Node> {
    RefPtr<Node> m_parent;  // Parent refs child, child refs parent = cycle
    Vector<NonnullRefPtr<Node>> m_children;
};

// CORRECT (weak reference):
class Node : public RefCounted<Node> {
    WeakPtr<Node> m_parent;  // Weak reference breaks cycle
    Vector<NonnullRefPtr<Node>> m_children;
};
```

**Detection:**
- LSAN may detect (but cycles can hide from detection)
- Manual inspection with debugger
- Heap profiling (see section 6)

**Prevention:**
- Use `WeakPtr<T>` for parent pointers
- Document ownership relationships
- Review reference graphs during design

## 5. Valgrind Workflow

### 5.1 Valgrind Memcheck - Memory Error Detection

**Run:**
```bash
# Build without ASAN (conflict with Valgrind)
cmake --preset Release
cmake --build Build/release

# Run memcheck
valgrind --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --verbose \
    --log-file=valgrind.log \
    ./Build/release/bin/Ladybird

# Or use helper script
./scripts/run_with_valgrind.sh
```

**Options:**
- `--leak-check=full` - Detailed leak information
- `--show-leak-kinds=all` - Show all leak types (definite, possible, reachable)
- `--track-origins=yes` - Track uninitialized values to allocation
- `--suppressions=valgrind.supp` - Suppress known issues

**Output:**
```
==12345== Invalid read of size 8
==12345==    at 0x4E89AB: Web::DOM::Node::parent() (Node.cpp:142)
==12345==    by 0x4E8BCD: Web::DOM::Element::remove() (Element.cpp:89)
==12345==  Address 0x1234abcd is 8 bytes inside a block of size 64 free'd
==12345==    at 0x4C2EDEB: operator delete(void*) (vg_replace_malloc.c:595)
==12345==    by 0x4E89EF: Web::DOM::Node::~Node() (Node.cpp:67)
```

### 5.2 Valgrind Callgrind - Performance Profiling

**Run:**
```bash
# Generate callgrind profile
valgrind --tool=callgrind \
    --dump-instr=yes \
    --collect-jumps=yes \
    --callgrind-out-file=callgrind.out \
    ./Build/release/bin/Ladybird

# Visualize with kcachegrind
kcachegrind callgrind.out
```

### 5.3 Valgrind Massif - Heap Profiling

**Run:**
```bash
# Profile heap usage
valgrind --tool=massif \
    --massif-out-file=massif.out \
    ./Build/release/bin/Ladybird

# Visualize
ms_print massif.out > massif.txt
```

**Output:**
```
--------------------------------------------------------------------------------
  n        time(i)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
  0              0                0                0             0            0
  1         10,240          1,024,000          1,020,000         4,000            0
  2         20,480          2,048,000          2,040,000         8,000            0
```

## 6. Debugging Techniques

### 6.1 Symbolication

Sanitizers need debug symbols to show useful stack traces:

```bash
# Ensure debug symbols
cmake --preset Sanitizers  # Includes -g

# Manual symbolication
llvm-symbolizer --obj=./Build/sanitizers/bin/Ladybird < addresses.txt

# Set symbolizer path
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
```

### 6.2 GDB Integration

```bash
# Run with GDB
gdb --args ./Build/sanitizers/bin/Ladybird

# Break on sanitizer error
(gdb) break __asan_report_error
(gdb) run

# When ASAN fires
(gdb) bt        # Backtrace
(gdb) info locals
(gdb) print *ptr
```

### 6.3 Core Dumps

```bash
# Enable core dumps
ulimit -c unlimited

# Run until crash
./Build/sanitizers/bin/Ladybird

# Analyze core dump
gdb ./Build/sanitizers/bin/Ladybird core

(gdb) bt
(gdb) frame 0
(gdb) info locals
```

### 6.4 Reproduction

Always create minimal reproducers:

```cpp
// Tests/LibWeb/TestUseAfterFree.cpp
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibTest/TestCase.h>

TEST_CASE(use_after_free_in_remove)
{
    auto document = Web::DOM::Document::create();
    auto parent = document->create_element("div");
    auto child = document->create_element("span");

    parent->append_child(child);

    // This should not crash
    parent->remove_child(child);
    child->update();  // Should be safe with RefPtr
}
```

## 7. Prevention Patterns

### 7.1 RAII (Resource Acquisition Is Initialization)

**Always use RAII for resource management:**

```cpp
// WRONG:
void process_file(String const& path) {
    int fd = open(path.characters(), O_RDONLY);
    // ... might return early or throw
    close(fd);  // May not be reached!
}

// CORRECT:
void process_file(String const& path) {
    auto fd = TRY(Core::File::open(path, Core::File::OpenMode::Read));
    // ... fd auto-closed when going out of scope
}
```

### 7.2 Smart Pointers

**Smart Pointer Selection Guide:**

```cpp
// OwnPtr<T> - Unique ownership, single owner
class Document {
    OwnPtr<StyleComputer> m_style_computer;  // Document owns exclusively
};

// NonnullOwnPtr<T> - Unique ownership, never null
ErrorOr<NonnullOwnPtr<Document>> Document::create() {
    auto doc = TRY(adopt_nonnull_own(new Document()));
    return doc;
}

// RefPtr<T> - Shared ownership, can be null
class Element {
    RefPtr<Node> m_parent;  // May be null for root
    Vector<NonnullRefPtr<Node>> m_children;  // Children never null
};

// WeakPtr<T> - Non-owning reference, breaks cycles
class Node {
    WeakPtr<Document> m_document;  // Don't keep document alive
};
```

### 7.3 Bounds Checking

**Always check bounds before access:**

```cpp
// WRONG:
char process_input(Vector<char> const& input, size_t index) {
    return input[index];  // Unchecked access
}

// CORRECT:
ErrorOr<char> process_input(Vector<char> const& input, size_t index) {
    if (index >= input.size())
        return Error::from_string_literal("Index out of bounds");
    return input[index];
}

// Or use at():
ErrorOr<char> process_input(Vector<char> const& input, size_t index) {
    return input.at(index);  // Throws on out of bounds
}
```

### 7.4 Checked Arithmetic

**Use AK checked arithmetic for safety-critical code:**

```cpp
// IPC message handling
ErrorOr<void> handle_allocate(u32 count, u32 size) {
    // Check multiplication overflow
    auto total_size = AK::checked_mul(count, size);
    if (total_size.has_overflow())
        return Error::from_string_literal("Allocation size overflow");

    // Check addition overflow
    auto with_header = AK::checked_add(total_size.value(), sizeof(Header));
    if (with_header.has_overflow())
        return Error::from_string_literal("Size overflow");

    auto buffer = TRY(ByteBuffer::create_uninitialized(with_header.value()));
    // ...
}
```

### 7.5 Error Propagation

**Use ErrorOr and TRY for error handling:**

```cpp
// WRONG:
Node* parse_node(StringView input) {
    if (input.is_empty())
        return nullptr;  // Lost error context
    // ...
}

// CORRECT:
ErrorOr<NonnullRefPtr<Node>> parse_node(StringView input) {
    if (input.is_empty())
        return Error::from_string_literal("Empty input");

    auto name = TRY(parse_name(input));
    auto node = TRY(Node::create(name));
    return node;
}
```

## 8. CI Sanitizer Integration

### 8.1 CI Sanitizer Workflow

Ladybird CI runs sanitizers on all tests:

```yaml
# .github/workflows/sanitizers.yml (conceptual)
name: Sanitizers

on: [push, pull_request]

jobs:
  asan-ubsan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build with sanitizers
        run: |
          cmake --preset Sanitizers
          cmake --build Build/sanitizers

      - name: Run tests
        run: |
          export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
          ctest --preset Sanitizers

      - name: Upload sanitizer logs
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: sanitizer-logs
          path: |
            asan.log.*
            ubsan.log.*
```

### 8.2 Suppression Files

Create suppression files for known issues:

**asan_suppressions.txt:**
```
# Third-party library issue
leak:libfontconfig.so

# Known issue in Qt
leak:QApplication::exec
```

**ubsan_suppressions.txt:**
```
# Third-party header
unsigned-integer-overflow:qhash.h
```

**lsan_suppressions.txt:**
```
# Qt event loop
leak:QEventLoop::exec
```

## 9. Quick Reference

### Sanitizer Command Quick Reference

```bash
# Build with sanitizers
cmake --preset Sanitizers && cmake --build Build/sanitizers

# Run browser
./Build/sanitizers/bin/Ladybird

# Run specific test
./Build/sanitizers/bin/TestIPC

# Run all tests
ctest --preset Sanitizers

# With custom options
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./Build/sanitizers/bin/Ladybird

# Valgrind memcheck
valgrind --leak-check=full --track-origins=yes ./Build/release/bin/Ladybird

# Generate core dump
ulimit -c unlimited && ./Build/sanitizers/bin/Ladybird

# Analyze with GDB
gdb ./Build/sanitizers/bin/Ladybird core
```

### Common Sanitizer Options

| Sanitizer | Key Options | Purpose |
|-----------|-------------|---------|
| ASAN | `detect_leaks=1` | Enable leak detection |
| ASAN | `halt_on_error=1` | Stop on first error |
| ASAN | `detect_stack_use_after_return=1` | Detect stack UAF |
| UBSAN | `print_stacktrace=1` | Show stack traces |
| UBSAN | `halt_on_error=0` | Continue after error |
| LSAN | `suppressions=file.txt` | Suppress known leaks |
| TSAN | `second_deadlock_stack=1` | Show deadlock info |

## Checklist

Memory safety debugging workflow:

- [ ] Build with Sanitizers preset
- [ ] Run affected code path
- [ ] Capture sanitizer output
- [ ] Symbolicate stack traces
- [ ] Identify error type (UAF, overflow, leak, etc.)
- [ ] Locate allocation/free sites
- [ ] Understand ownership model
- [ ] Create minimal reproducer
- [ ] Fix with appropriate pattern (RefPtr, OwnPtr, bounds check, etc.)
- [ ] Verify fix with sanitizers
- [ ] Add regression test
- [ ] Check for similar issues in codebase

## Common Pitfalls

**ASAN false negatives:** ASAN only detects errors that execute. Use fuzzing for coverage.

**MSAN requires clean build:** All dependencies must be MSAN-instrumented.

**TSAN incompatible with ASAN:** Run separately.

**Performance overhead:** ASAN ~2x slowdown, TSAN ~5-10x, MSAN ~3x, UBSAN ~1.2x.

**Symbolication failures:** Ensure debug symbols with `-g` flag.

**Suppression abuse:** Fix root causes, don't just suppress.

## Related Skills

### Debugging Foundation for Security Skills
- **[ipc-security](../ipc-security/SKILL.md)**: Debug IPC handler crashes and memory corruption. Use ASAN to catch buffer overflows in IPC message processing.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Triage and debug fuzzer-discovered crashes. Analyze ASAN reports, reproduce crashes, and create regression tests.

### Build Configuration
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build with sanitizers using Sanitizer preset or ENABLE_ADDRESS_SANITIZER flags.
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Run sanitizer builds in CI. Configure ASAN/UBSAN/MSAN options for continuous testing.

### Architecture Debugging
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Debug crashes in multi-process context. Attach to WebContent, RequestServer, or ImageDecoder processes.
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Debug LibWeb DOM tree corruption and layout crashes. Inspect node relationships and style computation.
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Debug LibJS GC issues and heap corruption. Inspect GC heap state and reference counting.

### Performance and Quality
- **[browser-performance](../browser-performance/SKILL.md)**: Use profiling tools (perf, heaptrack) to identify memory leaks and allocation hotspots.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Debug test failures with sanitizers. Run test suite with ASAN to catch memory issues.

### Development Patterns
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Debug smart pointer misuse and ErrorOr violations. Understand RefPtr cycles and use-after-free bugs.
