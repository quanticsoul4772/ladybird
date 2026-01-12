// Example: Adding a new CSS property "line-clamp"
// This demonstrates the complete pattern for CSS property implementation

// ==================== Step 1: Properties.json ====================
/*
{
  "line-clamp": {
    "inherited": false,
    "initial": "none",
    "animation-type": "by computed value",
    "percentages": "N/A"
  }
}
*/

// ==================== Step 2: Add Parsing (if needed) ====================
// In CSS/Parser/PropertyParsing.cpp

RefPtr<CSS::StyleValue> Parser::parse_css_value(CSS::PropertyID property_id)
{
    switch (property_id) {
    case PropertyID::LineClamp:
        return parse_line_clamp_value(tokens);
    // ... other properties
    }
}

// https://drafts.csswg.org/css-overflow-4/#propdef-line-clamp
RefPtr<CSS::StyleValue> Parser::parse_line_clamp_value(TokenStream<ComponentValue>& tokens)
{
    // line-clamp: none | <integer [1,∞]>

    // 1. Try 'none' keyword
    if (auto none = parse_keyword_value(tokens)) {
        if (none->to_keyword() == Keyword::None)
            return none;
    }

    // 2. Try integer >= 1
    if (auto integer = parse_integer_value(tokens)) {
        if (integer->as_integer() >= 1)
            return integer;
    }

    return nullptr;
}

// ==================== Step 3: ComputedValues.h ====================
// In CSS/ComputedValues.h

struct ComputedValues {
    // Getter
    IntegerOrNone const& line_clamp() const { return m_noninherited.line_clamp; }

private:
    struct Noninherited {
        IntegerOrNone line_clamp { IntegerOrNone::make_none() };
        // ... other non-inherited properties
    };
};

struct MutableComputedValues {
    void set_line_clamp(IntegerOrNone value) { m_noninherited.line_clamp = move(value); }
};

struct InitialValues {
    static IntegerOrNone line_clamp() { return IntegerOrNone::make_none(); }
};

// ==================== Step 4: Apply Style ====================
// In NodeWithStyle::apply_style()

void NodeWithStyle::apply_style(CSS::StyleProperties const& properties)
{
    // ... existing code ...

    // Copy line-clamp from StyleProperties to ComputedValues
    computed_values.set_line_clamp(properties.line_clamp());

    // ... more properties ...
}

// ==================== Step 5: Use in Layout ====================
// In Layout/BlockFormattingContext.cpp or similar

void InlineFormattingContext::run(Box const& box, LayoutMode layout_mode, AvailableSpace const& available_space)
{
    auto const& computed_values = box.computed_values();
    auto line_clamp = computed_values.line_clamp();

    // Use the property
    if (!line_clamp.is_none()) {
        auto max_lines = line_clamp.as_integer();

        // Implement line clamping logic
        size_t line_count = 0;
        for (auto& line : m_lines) {
            if (line_count >= max_lines) {
                // Clamp this line (add ellipsis, etc.)
                clamp_line(line);
                break;
            }
            line_count++;
        }
    }
}

// ==================== Step 6: JavaScript Access ====================
// In CSS/CSSStyleProperties.cpp

// For computed style access from JS (if special serialization needed)
RefPtr<CSSStyleValue const> CSSStyleProperties::style_value_for_computed_property(
    PropertyID property_id,
    ComputedValues const& computed_values) const
{
    switch (property_id) {
    case PropertyID::LineClamp: {
        auto line_clamp = computed_values.line_clamp();
        if (line_clamp.is_none())
            return CSSKeywordValue::create(Keyword::None);
        return CSSIntegerValue::create(line_clamp.as_integer());
    }
    // ... other properties
    }
}

// ==================== Usage Example ====================

void example_usage()
{
    // CSS:
    // .truncate {
    //     display: -webkit-box;
    //     -webkit-line-clamp: 3;
    //     -webkit-box-orient: vertical;
    //     overflow: hidden;
    // }

    // In layout code:
    auto const& computed = box.computed_values();
    if (!computed.line_clamp().is_none()) {
        dbgln("Clamping to {} lines", computed.line_clamp().as_integer());
    }
}

// ==================== Testing ====================

// Create test file: Tests/LibWeb/Text/input/line-clamp.html
/*
<!DOCTYPE html>
<style>
    .clamp-2 {
        line-clamp: 2;
    }
    .clamp-none {
        line-clamp: none;
    }
</style>
<script src="../include.js"></script>
<script>
    test(() => {
        const div = document.createElement('div');
        div.className = 'clamp-2';
        document.body.appendChild(div);

        const style = getComputedStyle(div);
        println(style.lineClamp); // Should print "2"
    });

    test(() => {
        const div = document.createElement('div');
        div.className = 'clamp-none';
        document.body.appendChild(div);

        const style = getComputedStyle(div);
        println(style.lineClamp); // Should print "none"
    });
</script>
*/

// Run test:
// ./Meta/ladybird.py run test-web -- -f Text/input/line-clamp.html
