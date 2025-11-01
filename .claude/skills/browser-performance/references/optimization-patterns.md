# Browser Performance Optimization Patterns

## General Principles

1. **Profile First**: Never optimize without profiling
2. **Measure Impact**: Always benchmark before/after
3. **Focus on Hot Paths**: Optimize what matters (80/20 rule)
4. **Avoid Premature Optimization**: Write clear code first
5. **Maintain Correctness**: Don't sacrifice correctness for speed

## Pattern Categories

### 1. String Operations

#### Use StringBuilder for Concatenation
```cpp
// BAD: O(n²) reallocations
String result;
for (size_t i = 0; i < 1000; ++i) {
    result = result + String::number(i);
}

// GOOD: O(n) with single allocation
StringBuilder builder;
for (size_t i = 0; i < 1000; ++i) {
    builder.append(String::number(i));
}
String result = builder.to_string();
```

#### Use StringView for Temporary Strings
```cpp
// BAD: Allocates temporary String
bool is_http_url(String const& url) {
    return url.starts_with(String("http://"));  // Allocation!
}

// GOOD: Zero-cost StringView
bool is_http_url(StringView url) {
    return url.starts_with("http://"sv);
}
```

#### Avoid String Copies in Loops
```cpp
// BAD: Copies string every iteration
for (auto css_class : element->class_list()) {  // Copy
    process_class(css_class);
}

// GOOD: Use const reference
for (auto const& css_class : element->class_list()) {
    process_class(css_class);
}
```

### 2. Container Operations

#### Reserve Vector Capacity
```cpp
// BAD: Multiple reallocations
Vector<int> numbers;
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);
}

// GOOD: Single allocation
Vector<int> numbers;
numbers.ensure_capacity(10000);
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);
}
```

#### Avoid Unnecessary HashMap Lookups
```cpp
// BAD: Two lookups for same key
if (map.contains(key)) {
    auto value = map.get(key);  // Second lookup
    process(value.value());
}

// GOOD: Single lookup
if (auto value = map.get(key)) {
    process(value.value());
}
```

#### Use Move Semantics
```cpp
// BAD: Copies vector
Vector<Element> elements = create_elements();  // Copy
process_elements(elements);                     // Copy

// GOOD: Moves vector
Vector<Element> elements = create_elements();  // Move
process_elements(move(elements));               // Move
```

### 3. Caching Patterns

#### Cache Expensive Computations
```cpp
class Element {
    // BAD: Recomputes every time
    CSS::ComputedStyle compute_style() const {
        return CSS::StyleComputer::compute_style(*this);  // Expensive!
    }

    // GOOD: Cache with invalidation
    CSS::ComputedStyle const& compute_style() const {
        if (!m_cached_style.has_value()) {
            m_cached_style = CSS::StyleComputer::compute_style(*this);
        }
        return m_cached_style.value();
    }

    void invalidate_style() { m_cached_style.clear(); }

private:
    mutable Optional<CSS::ComputedStyle> m_cached_style;
};
```

#### Share Immutable Data
```cpp
// BAD: Each element stores full font
class Element {
    Gfx::Font m_font;  // 100s of bytes per element
};

// GOOD: Share font via RefPtr
class Element {
    RefPtr<Gfx::Font> m_font;  // 8 bytes per element
};
```

### 4. Layout/Rendering Optimization

#### Avoid Layout Thrashing
```cpp
// BAD: Interleaved reads/writes force multiple layouts
for (auto* element : elements) {
    auto width = element->offset_width();  // Forced layout
    element->set_width(width + 10);         // Invalidate
}

// GOOD: Batch reads, then batch writes
Vector<float> widths;
for (auto* element : elements) {
    widths.append(element->offset_width());  // Single layout
}
for (size_t i = 0; i < elements.size(); ++i) {
    elements[i]->set_width(widths[i] + 10);  // Batch invalidation
}
```

#### Minimize Repaints
```cpp
// BAD: Repaints entire page
void update_element(Element* element) {
    element->set_property("color", "red");
    document->invalidate();  // Entire page
}

// GOOD: Invalidate only affected region
void update_element(Element* element) {
    element->set_property("color", "red");
    element->invalidate_rect(element->bounding_box());
}
```

#### Use DocumentFragment for Batch Insertions
```cpp
// BAD: N layouts
for (size_t i = 0; i < 1000; ++i) {
    auto div = document->create_element("div");
    parent->append_child(div);  // Triggers layout
}

// GOOD: 1 layout
auto fragment = document->create_document_fragment();
for (size_t i = 0; i < 1000; ++i) {
    auto div = document->create_element("div");
    fragment->append_child(div);
}
parent->append_child(fragment);  // Single layout
```

### 5. CSS Selector Optimization

#### Prefer Specific Selectors
```cpp
// SLOW: Checks every element
// div * span { ... }

// FAST: Direct class selector
// .specific-class { ... }
```

#### Cache Selector Compilation
```cpp
class SelectorMatcher {
    HashMap<String, CompiledSelector> m_cache;

    bool matches(Element const& element, StringView selector) {
        auto compiled = m_cache.get(selector);
        if (!compiled.has_value()) {
            compiled = compile_selector(selector);
            m_cache.set(selector, compiled.value());
        }
        return compiled.value().matches(element);
    }
};
```

#### Share Computed Styles
```cpp
// Elements with identical styles share computed style
class StyleComputer {
    HashMap<StyleHash, RefPtr<ComputedStyle>> m_cache;

    RefPtr<ComputedStyle> compute_style(Element const& element) {
        auto hash = compute_style_hash(element);
        if (auto cached = m_cache.get(hash)) {
            return cached.value();
        }
        auto style = compute_style_uncached(element);
        m_cache.set(hash, style);
        return style;
    }
};
```

### 6. Parser Optimization

#### Streaming Parsing
```cpp
// BAD: Wait for entire document
void parse_html(String const& html) {
    auto document = parse(html);  // Blocks until complete
    return document;
}

// GOOD: Parse incrementally
class StreamingParser {
    void append_chunk(ByteBuffer const& chunk) {
        m_buffer.append(chunk);
        while (auto token = try_extract_token()) {
            process_token(token.value());
        }
    }
};
```

#### Parallel Parsing
```cpp
// Parse multiple CSS files in parallel
Vector<StyleSheet> parse_stylesheets(Vector<String> const& sources) {
    Vector<StyleSheet> results;
    results.resize(sources.size());

    Threading::parallel_for(sources.size(), [&](size_t i) {
        results[i] = parse_css(sources[i]);
    });

    return results;
}
```

### 7. IPC Optimization

#### Batch Messages
```cpp
// BAD: N IPC roundtrips
for (auto const& cookie : cookies) {
    ipc_client.set_cookie(cookie);
}

// GOOD: 1 IPC roundtrip
ipc_client.set_cookies(cookies);
```

#### Async with Coalescing
```cpp
class IPCCoalescer {
    void request_update(String property, String value) {
        m_pending[property] = value;

        if (!m_timer.is_active()) {
            m_timer.start(16, [this] {  // 60fps
                flush_updates();
            });
        }
    }

    void flush_updates() {
        ipc_client.batch_update(m_pending);
        m_pending.clear();
    }

    HashMap<String, String> m_pending;
    Timer m_timer;
};
```

### 8. Memory Optimization

#### Lazy Initialization
```cpp
// BAD: Always computes, even if not used
class Document {
    Document() {
        m_statistics = compute_statistics();  // Expensive
    }
    DocumentStatistics m_statistics;
};

// GOOD: Compute on first access
class Document {
    DocumentStatistics const& statistics() const {
        if (!m_statistics.has_value()) {
            m_statistics = compute_statistics();
        }
        return m_statistics.value();
    }
    mutable Optional<DocumentStatistics> m_statistics;
};
```

#### Object Pooling
```cpp
// Reuse objects instead of allocating
class ObjectPool<T> {
    T* acquire() {
        if (m_pool.is_empty()) {
            return new T;
        }
        return m_pool.take_last();
    }

    void release(T* object) {
        object->reset();
        m_pool.append(object);
    }

    Vector<T*> m_pool;
};
```

### 9. JavaScript Optimization

#### JIT Compilation
```cpp
// Hot loops are JIT-compiled
class JITCompiler {
    void execute_function(Function const& fn) {
        m_execution_count[fn.name()]++;

        if (m_execution_count[fn.name()] > JIT_THRESHOLD) {
            // Compile to native code
            auto native_code = compile_to_native(fn);
            m_compiled_cache.set(fn.name(), native_code);
        }

        if (auto compiled = m_compiled_cache.get(fn.name())) {
            execute_native(compiled.value());
        } else {
            interpret(fn);
        }
    }

    static constexpr size_t JIT_THRESHOLD = 100;
    HashMap<String, NativeCode> m_compiled_cache;
    HashMap<String, size_t> m_execution_count;
};
```

#### Bytecode Caching
```cpp
// Cache compiled bytecode
class BytecodeCache {
    Bytecode::Executable const& get_or_compile(String const& source) {
        auto hash = compute_hash(source);

        if (auto cached = m_cache.get(hash)) {
            return cached.value();
        }

        auto bytecode = compile_to_bytecode(source);
        m_cache.set(hash, bytecode);
        return bytecode;
    }

    HashMap<u64, Bytecode::Executable> m_cache;
};
```

### 10. Virtual Function Call Reduction

#### Batch Virtual Calls
```cpp
// BAD: Virtual call in tight loop
for (size_t i = 0; i < 1000000; ++i) {
    shape->draw();  // 1M virtual calls
}

// GOOD: Batch in virtual function
class Shape {
    virtual void draw_n_times(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            draw_impl();  // Non-virtual
        }
    }
};

shape->draw_n_times(1000000);  // 1 virtual call
```

#### Template Specialization
```cpp
// Avoid virtual calls via templates
template<typename Renderer>
void render_page(Renderer& renderer) {
    renderer.render_background();  // Non-virtual
    renderer.render_content();     // Non-virtual
}
```

## Performance Anti-Patterns

### 1. Repeated Work in Loops
```cpp
// BAD
for (auto& element : elements) {
    auto parent_bounds = element.parent()->bounding_box();  // Repeated
    element.position_relative_to(parent_bounds);
}

// GOOD
auto parent_bounds = parent->bounding_box();  // Once
for (auto& element : elements) {
    element.position_relative_to(parent_bounds);
}
```

### 2. Premature Allocation
```cpp
// BAD
Vector<int> results;
for (auto& item : items) {
    if (filter(item)) {
        results.append(item);  // Unknown size, multiple reallocations
    }
}

// GOOD
Vector<int> results;
results.ensure_capacity(items.size());  // Over-allocate
for (auto& item : items) {
    if (filter(item)) {
        results.append(item);
    }
}
```

### 3. Synchronous I/O on Main Thread
```cpp
// BAD: Blocks UI
String load_file(String const& path) {
    auto file = Core::File::open(path);  // Blocks!
    return file->read_all();
}

// GOOD: Async I/O
void load_file_async(String const& path, Function<void(String)> callback) {
    Thread::create([path, callback] {
        auto content = /* read file */;
        Core::EventLoop::current().deferred_invoke([content, callback] {
            callback(content);
        });
    });
}
```

## Optimization Checklist

- [ ] Profile to identify hotspots
- [ ] Focus on hot paths (top 10% of CPU time)
- [ ] Avoid premature optimization
- [ ] Use StringBuilder for string concatenation
- [ ] Reserve vector capacity when size known
- [ ] Cache expensive computations
- [ ] Avoid layout thrashing
- [ ] Batch IPC messages
- [ ] Use StringView for temporary strings
- [ ] Minimize virtual function calls in loops
- [ ] Avoid synchronous I/O on main thread
- [ ] Share immutable data
- [ ] Use move semantics
- [ ] Measure impact with benchmarks
- [ ] Verify correctness not compromised
