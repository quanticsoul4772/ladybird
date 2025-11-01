// Example: Microbenchmark structure for Ladybird performance testing
// File: Tests/Benchmarks/DOMBenchmark.cpp

#include <LibTest/Benchmark.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/HTML/HTMLDivElement.h>

// ===== DOM Query Benchmarks =====

BENCHMARK_CASE(dom_query_selector_by_id)
{
    // Setup: Create document with 1000 elements
    auto document = create_test_document();
    auto parent = document->body();

    for (size_t i = 0; i < 1000; ++i) {
        auto div = document->create_element("div");
        div->set_attribute("id", String::formatted("element-{}", i));
        parent->append_child(div);
    }

    // Benchmark: Query by ID (should be fast with hash table)
    BENCHMARK_LOOP {
        auto element = document->query_selector("#element-500");
        VERIFY(element);  // Prevent optimization
    }
}

BENCHMARK_CASE(dom_query_selector_by_class)
{
    auto document = create_test_document();
    auto parent = document->body();

    for (size_t i = 0; i < 1000; ++i) {
        auto div = document->create_element("div");
        div->set_attribute("class", "test-class");
        parent->append_child(div);
    }

    // Benchmark: Query by class (should scan efficiently)
    BENCHMARK_LOOP {
        auto elements = document->query_selector_all(".test-class");
        VERIFY(elements->length() == 1000);
    }
}

BENCHMARK_CASE(dom_query_complex_selector)
{
    auto document = create_test_document();
    create_complex_dom_tree(document);  // Nested structure

    // Benchmark: Complex selector matching
    BENCHMARK_LOOP {
        auto elements = document->query_selector_all("div.container > ul li.active");
        VERIFY(elements);
    }
}

// ===== DOM Manipulation Benchmarks =====

BENCHMARK_CASE(dom_single_insertions)
{
    BENCHMARK_LOOP {
        auto document = create_test_document();
        auto parent = document->body();

        // Insert elements one by one (triggers multiple layouts)
        for (size_t i = 0; i < 100; ++i) {
            auto div = document->create_element("div");
            div->set_text_content(String::number(i));
            parent->append_child(div);
        }
    }
}

BENCHMARK_CASE(dom_batch_insertions)
{
    BENCHMARK_LOOP {
        auto document = create_test_document();
        auto parent = document->body();

        // Use DocumentFragment for batch insertion (single layout)
        auto fragment = document->create_document_fragment();
        for (size_t i = 0; i < 100; ++i) {
            auto div = document->create_element("div");
            div->set_text_content(String::number(i));
            fragment->append_child(div);
        }
        parent->append_child(fragment);
    }
}

BENCHMARK_CASE(dom_element_removal)
{
    auto document = create_test_document();
    auto parent = document->body();

    // Pre-create elements
    for (size_t i = 0; i < 100; ++i) {
        auto div = document->create_element("div");
        parent->append_child(div);
    }

    BENCHMARK_LOOP {
        // Remove all children
        while (auto child = parent->first_child()) {
            parent->remove_child(child);
        }

        // Re-add for next iteration
        for (size_t i = 0; i < 100; ++i) {
            auto div = document->create_element("div");
            parent->append_child(div);
        }
    }
}

// ===== Style Computation Benchmarks =====

BENCHMARK_CASE(style_computation_simple)
{
    auto document = create_test_document();

    // Add simple stylesheet
    auto style = document->create_element("style");
    style->set_text_content("div { color: red; }");
    document->head()->append_child(style);

    auto parent = document->body();
    for (size_t i = 0; i < 100; ++i) {
        auto div = document->create_element("div");
        parent->append_child(div);
    }

    BENCHMARK_LOOP {
        // Force style recomputation
        document->invalidate_style();
        document->update_style();
    }
}

BENCHMARK_CASE(style_computation_complex)
{
    auto document = create_test_document();

    // Add complex stylesheet with many selectors
    auto style = document->create_element("style");
    StringBuilder css;
    for (size_t i = 0; i < 50; ++i) {
        css.appendff("div.class-{} {{ color: red; }}\n", i);
    }
    style->set_text_content(css.to_string());
    document->head()->append_child(style);

    auto parent = document->body();
    for (size_t i = 0; i < 100; ++i) {
        auto div = document->create_element("div");
        div->set_attribute("class", String::formatted("class-{}", i % 50));
        parent->append_child(div);
    }

    BENCHMARK_LOOP {
        document->invalidate_style();
        document->update_style();
    }
}

// ===== Layout Benchmarks =====

BENCHMARK_CASE(layout_simple_flow)
{
    auto document = create_test_document();
    auto parent = document->body();

    for (size_t i = 0; i < 100; ++i) {
        auto div = document->create_element("div");
        div->set_text_content("Test content");
        parent->append_child(div);
    }

    BENCHMARK_LOOP {
        // Force layout
        document->force_layout();
    }
}

BENCHMARK_CASE(layout_flexbox)
{
    auto document = create_test_document();

    auto style = document->create_element("style");
    style->set_text_content(".container { display: flex; } .item { flex: 1; }");
    document->head()->append_child(style);

    auto container = document->create_element("div");
    container->set_attribute("class", "container");
    document->body()->append_child(container);

    for (size_t i = 0; i < 50; ++i) {
        auto div = document->create_element("div");
        div->set_attribute("class", "item");
        div->set_text_content("Flex item");
        container->append_child(div);
    }

    BENCHMARK_LOOP {
        document->force_layout();
    }
}

// ===== String Building Benchmarks =====

BENCHMARK_CASE(string_concatenation_naive)
{
    BENCHMARK_LOOP {
        String result;
        for (size_t i = 0; i < 1000; ++i) {
            result = String::formatted("{}{}", result, i);  // Slow: reallocates
        }
        VERIFY(!result.is_empty());
    }
}

BENCHMARK_CASE(string_concatenation_builder)
{
    BENCHMARK_LOOP {
        StringBuilder builder;
        for (size_t i = 0; i < 1000; ++i) {
            builder.append(String::number(i));  // Fast: efficient append
        }
        String result = builder.to_string();
        VERIFY(!result.is_empty());
    }
}

// ===== Vector Operations Benchmarks =====

BENCHMARK_CASE(vector_append_without_reserve)
{
    BENCHMARK_LOOP {
        Vector<int> numbers;
        for (size_t i = 0; i < 10000; ++i) {
            numbers.append(i);  // Multiple reallocations
        }
        VERIFY(numbers.size() == 10000);
    }
}

BENCHMARK_CASE(vector_append_with_reserve)
{
    BENCHMARK_LOOP {
        Vector<int> numbers;
        numbers.ensure_capacity(10000);  // Reserve upfront
        for (size_t i = 0; i < 10000; ++i) {
            numbers.append(i);  // No reallocations
        }
        VERIFY(numbers.size() == 10000);
    }
}

// ===== Helper Functions =====

static NonnullRefPtr<Web::DOM::Document> create_test_document()
{
    auto document = Web::DOM::Document::create();
    auto html = document->create_element("html");
    auto head = document->create_element("head");
    auto body = document->create_element("body");

    html->append_child(head);
    html->append_child(body);
    document->append_child(html);

    return document;
}

static void create_complex_dom_tree(NonnullRefPtr<Web::DOM::Document> document)
{
    auto body = document->body();

    for (size_t i = 0; i < 10; ++i) {
        auto container = document->create_element("div");
        container->set_attribute("class", "container");

        auto ul = document->create_element("ul");
        for (size_t j = 0; j < 10; ++j) {
            auto li = document->create_element("li");
            if (j % 2 == 0) {
                li->set_attribute("class", "active");
            }
            li->set_text_content(String::formatted("Item {}", j));
            ul->append_child(li);
        }

        container->append_child(ul);
        body->append_child(container);
    }
}

// ===== Running Benchmarks =====

/*
Build and run:
    cmake --preset Release
    cmake --build Build/release --target DOMBenchmark
    ./Build/release/bin/DOMBenchmark

Run specific benchmark:
    ./Build/release/bin/DOMBenchmark --filter=dom_query

Compare before/after:
    ./Build/release/bin/DOMBenchmark --json > before.json
    # Make changes
    ./Build/release/bin/DOMBenchmark --json > after.json
    ./scripts/compare_benchmarks.py before.json after.json
*/
