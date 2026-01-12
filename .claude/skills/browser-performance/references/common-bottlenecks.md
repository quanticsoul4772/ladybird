# Common Performance Bottlenecks in Browser Development

## 1. String Operations

### Excessive String Allocation
**Symptom:** High allocation count, time spent in String::copy()

**Common Causes:**
```cpp
// BAD: O(n²) string concatenation
String result;
for (auto& item : items) {
    result = result + item;  // Allocates new string each time
}

// BAD: Temporary String objects
bool is_http(String const& url) {
    return url.starts_with(String("http://"));  // Unnecessary allocation
}

// BAD: String copies in loops
for (auto class_name : element->class_list()) {  // Copies each string
    process(class_name);
}
```

**Solutions:**
- Use StringBuilder for concatenation
- Use StringView for read-only operations
- Use const references in loops
- Use "..."sv literals

**Detection:**
```bash
# Profile heap allocations
heaptrack ./Ladybird
# Look for: String::copy, String::formatted in flamegraph
```

## 2. Layout Thrashing

### Forced Synchronous Layouts
**Symptom:** Multiple layouts per frame, slow DOM manipulation

**Common Causes:**
```cpp
// BAD: Reading layout properties in loop triggers forced layout
for (auto* element : elements) {
    auto width = element->offset_width();  // Triggers layout
    element->set_width(width + 10);         // Invalidates layout
    // Next iteration forces layout again!
}

// BAD: Interleaved reads/writes
element->set_attribute("class", "new-class");  // Invalidate
auto height = element->offset_height();         // Force layout
element->set_text_content("text");              // Invalidate
auto width = element->offset_width();           // Force layout again
```

**Solutions:**
- Batch all reads, then all writes
- Use DocumentFragment for batch insertions
- Cache layout values when possible
- Minimize style recalculations

**Detection:**
```bash
# Look for: force_layout, layout_tree_builder in flame graph
perf record -g ./Ladybird
perf report | grep layout
```

## 3. Excessive CSS Selector Matching

### Inefficient Selectors
**Symptom:** Time spent in selector matching, style computation

**Common Causes:**
```css
/* BAD: Universal selector with descendant combinator */
div * span { color: red; }  /* Checks every element */

/* BAD: Deep selector chains */
.container .wrapper .inner .content .item { }

/* BAD: Attribute selectors on many elements */
[data-attr] { }
```

**Solutions:**
- Use specific class selectors
- Limit selector depth
- Cache compiled selectors
- Share computed styles for identical elements

**Detection:**
```bash
# Look for: matches_selector, compute_style
perf record -g ./Ladybird
perf report | grep -i "selector\|style"
```

## 4. Parser Performance Issues

### Blocking Parse Operations
**Symptom:** Slow page load, time in parse_html/parse_css

**Common Causes:**
```cpp
// BAD: Blocking on entire document
String load_page(URL const& url) {
    auto content = download(url);  // Block until complete
    return parse_html(content);     // Then parse all at once
}

// BAD: Sequential stylesheet parsing
for (auto& stylesheet_url : stylesheets) {
    auto css = download(stylesheet_url);
    parse_css(css);  // One at a time
}
```

**Solutions:**
- Stream parsing (parse as data arrives)
- Parallel stylesheet parsing
- Defer non-critical resources
- Cache parsed resources

**Detection:**
```bash
# Look for: parse_html, parse_css, tokenize
perf record -g ./Ladybird
perf report | grep parse
```

## 5. IPC Overhead

### Chatty IPC Communication
**Symptom:** High syscall count, time in sendmsg/recvmsg

**Common Causes:**
```cpp
// BAD: Many small IPC messages
for (auto& cookie : cookies) {
    ipc_client.set_cookie(cookie);  // N roundtrips
}

// BAD: Synchronous IPC in hot path
auto result = ipc_client.sync_call();  // Blocks
process(result);
auto result2 = ipc_client.sync_call();  // Blocks again
```

**Solutions:**
- Batch IPC messages
- Use async IPC
- Coalesce frequent updates
- Reduce message size

**Detection:**
```bash
# Count syscalls
perf record -e 'syscalls:sys_enter_*' ./Ladybird
perf report | grep sendmsg
```

## 6. Memory Allocation Overhead

### Excessive Allocations
**Symptom:** Time in malloc/free, high allocation count

**Common Causes:**
```cpp
// BAD: Vector reallocations
Vector<int> numbers;
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // Reallocates multiple times
}

// BAD: Temporary objects in loop
for (auto& item : items) {
    auto temp = process(item);  // Allocates
    use(temp);
}  // Deallocates

// BAD: No object pooling
for (size_t i = 0; i < 1000000; ++i) {
    auto obj = new Object;  // 1M allocations
    process(obj);
    delete obj;
}
```

**Solutions:**
- Reserve vector capacity
- Reuse objects (pooling)
- Minimize temporary objects
- Use stack allocation when possible

**Detection:**
```bash
# Profile allocations
heaptrack ./Ladybird
# Look for: high allocation count, temporary allocations
```

## 7. Virtual Function Call Overhead

### Virtual Calls in Tight Loops
**Symptom:** Time in virtual function dispatch

**Common Causes:**
```cpp
// BAD: Virtual call in inner loop
for (size_t i = 0; i < 1000000; ++i) {
    for (auto* shape : shapes) {
        shape->draw();  // Virtual call
    }
}

// BAD: Virtual call for small operations
virtual int get_value() const { return m_value; }  // Too small
```

**Solutions:**
- Batch virtual calls
- Use templates for type-specific code
- Inline small functions
- Avoid virtual for trivial operations

**Detection:**
```bash
# Look for virtual function names in hot paths
perf record -g ./Ladybird
perf report --stdio | grep "@@"  # Virtual function markers
```

## 8. Synchronous I/O on Main Thread

### Blocking I/O Operations
**Symptom:** UI freezing, long wait times

**Common Causes:**
```cpp
// BAD: Synchronous file read on UI thread
String load_config() {
    auto file = File::open("config.txt");
    return file->read_all();  // Blocks UI
}

// BAD: Synchronous network request
auto response = http_client.get(url);  // Blocks

// BAD: Synchronous database query
auto results = database.query("SELECT ...");  // Blocks
```

**Solutions:**
- Async I/O with callbacks
- Thread pool for blocking operations
- Defer to background threads
- Use event loop properly

**Detection:**
```bash
# Look for: read, write, recv, send in main thread
perf record -g -e syscalls:sys_enter_read ./Ladybird
```

## 9. Cache Misses

### Poor Data Locality
**Symptom:** High cache miss rate, memory stalls

**Common Causes:**
```cpp
// BAD: Poor data layout (lots of pointers)
struct Node {
    int value;
    Node* next;  // Pointer chase
    Node* prev;
};

// BAD: Large objects scattered in memory
Vector<Node*> nodes;  // Random memory access

// BAD: Accessing non-contiguous data
for (auto* node : nodes) {
    process(node->data);  // Cache miss per iteration
}
```

**Solutions:**
- Use contiguous storage (Vector over linked list)
- Group related data (SoA vs AoS)
- Prefetch data when predictable
- Optimize struct layout

**Detection:**
```bash
# Measure cache misses
perf record -e cache-misses,cache-references ./Ladybird
perf report
```

## 10. Regex Performance

### Backtracking Regex
**Symptom:** Exponential time for certain inputs

**Common Causes:**
```cpp
// BAD: Catastrophic backtracking
auto regex = Regex("(a+)+b");  // Exponential time on "aaaa...a"

// BAD: Unbounded quantifiers
auto regex = Regex(".*password.*");  // Slow on long strings
```

**Solutions:**
- Use atomic groups
- Limit quantifiers
- Consider simpler string matching
- Profile regex performance

**Detection:**
```bash
# Look for: regex_match, regex_search taking excessive time
perf record -g ./Ladybird
```

## 11. JavaScript Execution

### Interpreted vs JIT
**Symptom:** Slow JavaScript execution

**Common Causes:**
- No JIT compilation for hot functions
- Excessive property lookups
- Type instability preventing optimization
- Large object graphs

**Solutions:**
- JIT compile hot loops
- Cache property lookups
- Use stable types
- Minimize object creation

**Detection:**
```bash
# Profile JS execution
perf record -g ./Ladybird <js-heavy-page>
# Look for: execute_ast_node, interpret
```

## 12. Memory Leaks

### Growing Memory Usage
**Symptom:** Memory usage increases over time

**Common Causes:**
```cpp
// BAD: Circular references
class Parent {
    Vector<NonnullRefPtr<Child>> m_children;
};
class Child {
    RefPtr<Parent> m_parent;  // Circular reference
};

// BAD: Event listeners not removed
element->add_event_listener("click", callback);
// Element destroyed but callback still referenced

// BAD: Cache without size limit
HashMap<String, Data> m_cache;  // Grows unbounded
```

**Solutions:**
- Use weak pointers for back-references
- Remove event listeners
- Implement cache eviction
- Use RAII for resource management

**Detection:**
```bash
# Detect leaks
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Profile heap growth
heaptrack ./Ladybird
# Look for: growing allocations, leak suspects
```

## Performance Anti-Pattern Checklist

Check your code for these common issues:

**String Operations:**
- [ ] No string concatenation in loops
- [ ] StringView used for temporary strings
- [ ] StringBuilder used for building strings
- [ ] String literals use "..."sv

**Layout/Rendering:**
- [ ] No interleaved layout reads/writes
- [ ] Batch DOM mutations
- [ ] Invalidate minimal regions
- [ ] DocumentFragment for insertions

**CSS:**
- [ ] Specific selectors (no universal)
- [ ] Limited selector depth
- [ ] Cache compiled selectors
- [ ] Share computed styles

**Memory:**
- [ ] Vector capacity reserved
- [ ] No excessive allocations in loops
- [ ] Objects reused (pooling)
- [ ] No memory leaks (ASAN)

**IPC:**
- [ ] Batched messages
- [ ] Async communication
- [ ] Minimal message size
- [ ] Coalesced updates

**I/O:**
- [ ] No synchronous I/O on main thread
- [ ] Async file operations
- [ ] Background threads for blocking ops
- [ ] Proper event loop usage

**Algorithms:**
- [ ] Appropriate complexity (no O(n²))
- [ ] Cache lookups when repeated
- [ ] Minimize virtual calls in loops
- [ ] Good data locality

## Diagnostic Workflow

1. **Identify symptom** (slow load, high CPU, memory growth)
2. **Profile** (perf, heaptrack, callgrind)
3. **Locate hotspot** (flame graph, top functions)
4. **Analyze root cause** (check against common bottlenecks)
5. **Apply fix** (use appropriate pattern)
6. **Verify improvement** (benchmark before/after)
7. **Prevent regression** (add benchmark test)
