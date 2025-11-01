// Example: Before/after optimization patterns for Ladybird

// ======================================================================
// OPTIMIZATION 1: Avoid Excessive String Allocations
// ======================================================================

// BEFORE: Creates temporary strings in hot path
String build_css_selector_slow(Vector<String> const& classes)
{
    String selector;
    for (auto const& class_name : classes) {
        selector = selector + "." + class_name;  // Reallocates every iteration
    }
    return selector;
}

// AFTER: Use StringBuilder for efficient concatenation
String build_css_selector_fast(Vector<String> const& classes)
{
    StringBuilder selector;
    for (auto const& class_name : classes) {
        selector.append('.');
        selector.append(class_name);  // Efficient append, no reallocation
    }
    return selector.to_string();  // Single allocation at end
}

// ======================================================================
// OPTIMIZATION 2: Cache Expensive Computations
// ======================================================================

// BEFORE: Recomputes style every time
class ElementSlow {
public:
    CSS::ComputedStyle compute_style() const
    {
        // Expensive: parses CSS, matches selectors, resolves inheritance
        return CSS::StyleComputer::compute_style(*this);
    }
};

// AFTER: Cache computed style with invalidation
class ElementFast {
public:
    CSS::ComputedStyle const& compute_style() const
    {
        if (!m_cached_style.has_value()) {
            m_cached_style = CSS::StyleComputer::compute_style(*this);
        }
        return m_cached_style.value();
    }

    void invalidate_style()
    {
        m_cached_style.clear();
    }

private:
    mutable Optional<CSS::ComputedStyle> m_cached_style;
};

// ======================================================================
// OPTIMIZATION 3: Batch Operations to Reduce Overhead
// ======================================================================

// BEFORE: Individual IPC messages (chatty communication)
void update_cookies_slow(Vector<Cookie> const& cookies)
{
    for (auto const& cookie : cookies) {
        ipc_client().set_cookie(cookie);  // N IPC roundtrips
    }
}

// AFTER: Batch IPC messages (single roundtrip)
void update_cookies_fast(Vector<Cookie> const& cookies)
{
    ipc_client().set_cookies(cookies);  // 1 IPC roundtrip
}

// ======================================================================
// OPTIMIZATION 4: Avoid Layout Thrashing
// ======================================================================

// BEFORE: Interleaved reads/writes cause forced layouts
void resize_elements_slow(Vector<Element*> const& elements)
{
    for (auto* element : elements) {
        auto width = element->offset_width();  // Forced layout (read)
        element->set_width(width + 10);         // Invalidates layout (write)
        // Next iteration forces layout again!
    }
}

// AFTER: Separate read and write phases
void resize_elements_fast(Vector<Element*> const& elements)
{
    // Phase 1: Read all widths (single layout)
    Vector<float> widths;
    widths.ensure_capacity(elements.size());
    for (auto* element : elements) {
        widths.append(element->offset_width());
    }

    // Phase 2: Write all widths (invalidates once)
    for (size_t i = 0; i < elements.size(); ++i) {
        elements[i]->set_width(widths[i] + 10);
    }
    // Single layout at end when needed
}

// ======================================================================
// OPTIMIZATION 5: Reserve Vector Capacity
// ======================================================================

// BEFORE: Vector grows dynamically (multiple reallocations)
Vector<int> parse_numbers_slow(StringView input)
{
    Vector<int> numbers;
    for (auto part : input.split_view(',')) {
        numbers.append(part.to_number<int>().value());  // Reallocates as needed
    }
    return numbers;
}

// AFTER: Reserve capacity upfront (single allocation)
Vector<int> parse_numbers_fast(StringView input)
{
    auto parts = input.split_view(',');

    Vector<int> numbers;
    numbers.ensure_capacity(parts.size());  // Allocate once

    for (auto part : parts) {
        numbers.append(part.to_number<int>().value());  // No reallocation
    }
    return numbers;
}

// ======================================================================
// OPTIMIZATION 6: Use StringView for Temporary Strings
// ======================================================================

// BEFORE: Creates String copies unnecessarily
bool starts_with_protocol_slow(String const& url)
{
    String http = "http://";   // Allocation
    String https = "https://";  // Allocation
    return url.starts_with(http) || url.starts_with(https);
}

// AFTER: Use StringView (zero-cost string slicing)
bool starts_with_protocol_fast(StringView url)
{
    return url.starts_with("http://"sv) || url.starts_with("https://"sv);
}

// ======================================================================
// OPTIMIZATION 7: Avoid Unnecessary HashMap Lookups
// ======================================================================

// BEFORE: Multiple lookups for same key
void process_config_slow(HashMap<String, String> const& config)
{
    if (config.contains("timeout")) {
        auto timeout = config.get("timeout").value();  // Lookup 1
        dbgln("Timeout: {}", timeout);
    }

    if (config.contains("timeout")) {
        auto timeout = config.get("timeout").value();  // Lookup 2 (same key!)
        set_timeout(timeout.to_number<int>());
    }
}

// AFTER: Single lookup, cache result
void process_config_fast(HashMap<String, String> const& config)
{
    if (auto timeout = config.get("timeout")) {  // Single lookup
        dbgln("Timeout: {}", *timeout);
        set_timeout(timeout->to_number<int>());
    }
}

// ======================================================================
// OPTIMIZATION 8: Lazy Evaluation
// ======================================================================

// BEFORE: Eagerly computes expensive data that may not be used
class DocumentSlow {
public:
    DocumentSlow()
    {
        // Expensive computation at construction time
        m_statistics = compute_document_statistics();  // Always computed
    }

    DocumentStatistics const& statistics() const { return m_statistics; }

private:
    DocumentStatistics m_statistics;
};

// AFTER: Lazy computation on first access
class DocumentFast {
public:
    DocumentFast() = default;

    DocumentStatistics const& statistics() const
    {
        if (!m_statistics.has_value()) {
            m_statistics = compute_document_statistics();  // Only when needed
        }
        return m_statistics.value();
    }

private:
    mutable Optional<DocumentStatistics> m_statistics;
};

// ======================================================================
// OPTIMIZATION 9: Reduce Virtual Function Call Overhead
// ======================================================================

// BEFORE: Virtual function in tight loop
void render_shapes_slow(Vector<Shape*> const& shapes)
{
    for (size_t i = 0; i < 1000000; ++i) {
        for (auto* shape : shapes) {
            shape->draw();  // Virtual call overhead * 1M
        }
    }
}

// AFTER: Batch virtual calls
class Shape {
    virtual void draw() { /* ... */ }
    virtual void draw_n_times(size_t n)  // Override in subclasses
    {
        for (size_t i = 0; i < n; ++i) {
            draw_impl();  // Non-virtual implementation
        }
    }
};

void render_shapes_fast(Vector<Shape*> const& shapes)
{
    for (auto* shape : shapes) {
        shape->draw_n_times(1000000);  // Single virtual call per shape
    }
}

// ======================================================================
// OPTIMIZATION 10: Parallelize Independent Work
// ======================================================================

// BEFORE: Sequential CSS parsing (single-threaded)
Vector<CSS::StyleSheet> parse_stylesheets_slow(Vector<String> const& sources)
{
    Vector<CSS::StyleSheet> stylesheets;
    for (auto const& source : sources) {
        stylesheets.append(CSS::parse_stylesheet(source));  // Sequential
    }
    return stylesheets;
}

// AFTER: Parallel CSS parsing (multi-threaded)
Vector<CSS::StyleSheet> parse_stylesheets_fast(Vector<String> const& sources)
{
    Vector<CSS::StyleSheet> stylesheets;
    stylesheets.resize(sources.size());

    // Parse in parallel
    Threading::parallel_for(sources.size(), [&](size_t i) {
        stylesheets[i] = CSS::parse_stylesheet(sources[i]);
    });

    return stylesheets;
}

// ======================================================================
// OPTIMIZATION 11: Use Move Semantics
// ======================================================================

// BEFORE: Unnecessary copies
Vector<Element> create_elements_slow(size_t count)
{
    Vector<Element> elements;
    for (size_t i = 0; i < count; ++i) {
        Element element = Element::create();
        elements.append(element);  // Copy
    }
    return elements;  // Copy
}

// AFTER: Move semantics
Vector<Element> create_elements_fast(size_t count)
{
    Vector<Element> elements;
    elements.ensure_capacity(count);

    for (size_t i = 0; i < count; ++i) {
        elements.append(Element::create());  // Move
    }
    return elements;  // Move (RVO/NRVO)
}

// ======================================================================
// OPTIMIZATION 12: Optimize Hot Loops
// ======================================================================

// BEFORE: Inefficient loop with repeated work
void apply_styles_slow(Vector<Element*> const& elements, HashMap<String, String> const& styles)
{
    for (auto* element : elements) {
        for (auto const& [property, value] : styles) {  // Inner loop
            element->set_style_property(property, value);
        }
    }
}

// AFTER: Hoist invariant computation
void apply_styles_fast(Vector<Element*> const& elements, HashMap<String, String> const& styles)
{
    // Precompute style string once
    StringBuilder style_string;
    for (auto const& [property, value] : styles) {
        style_string.appendff("{}: {}; ", property, value);
    }
    auto style_text = style_string.to_string();

    // Apply to all elements
    for (auto* element : elements) {
        element->set_style_attribute(style_text);  // Single operation
    }
}

// ======================================================================
// OPTIMIZATION 13: Reduce Memory Allocations
// ======================================================================

// BEFORE: Allocates temporary buffer for every call
String encode_url_slow(StringView url)
{
    Vector<u8> buffer;  // Heap allocation
    for (auto ch : url) {
        if (needs_encoding(ch)) {
            buffer.append('%');
            buffer.append(hex_digit(ch >> 4));
            buffer.append(hex_digit(ch & 0xF));
        } else {
            buffer.append(ch);
        }
    }
    return String::copy(buffer);
}

// AFTER: Preallocate buffer based on input size
String encode_url_fast(StringView url)
{
    Vector<u8> buffer;
    buffer.ensure_capacity(url.length() * 3);  // Worst case: all encoded

    for (auto ch : url) {
        if (needs_encoding(ch)) {
            buffer.append('%');
            buffer.append(hex_digit(ch >> 4));
            buffer.append(hex_digit(ch & 0xF));
        } else {
            buffer.append(ch);
        }
    }
    return String::copy(buffer);
}

// ======================================================================
// MEASURING IMPACT
// ======================================================================

/*
Benchmark both implementations:

BENCHMARK_CASE(build_css_selector_comparison) {
    Vector<String> classes;
    for (size_t i = 0; i < 100; ++i) {
        classes.append(String::formatted("class-{}", i));
    }

    // Measure slow version
    auto start = MonotonicTime::now();
    for (size_t i = 0; i < 1000; ++i) {
        auto result = build_css_selector_slow(classes);
    }
    auto slow_time = MonotonicTime::now() - start;

    // Measure fast version
    start = MonotonicTime::now();
    for (size_t i = 0; i < 1000; ++i) {
        auto result = build_css_selector_fast(classes);
    }
    auto fast_time = MonotonicTime::now() - start;

    dbgln("Slow: {}ms, Fast: {}ms ({}x speedup)",
        slow_time.to_milliseconds(),
        fast_time.to_milliseconds(),
        slow_time.to_milliseconds() / fast_time.to_milliseconds());
}

Expected output:
    Slow: 150ms, Fast: 8ms (18.75x speedup)
*/
