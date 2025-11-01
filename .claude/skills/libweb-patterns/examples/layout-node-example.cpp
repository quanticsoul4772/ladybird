// Example: Custom Layout Node Implementation
// Demonstrates layout tree node pattern and formatting context

// ==================== Layout/FlexContainer.h ====================
#pragma once

#include <LibWeb/Layout/Box.h>

namespace Web::Layout {

// Flex container implements CSS Flexible Box Layout
// https://www.w3.org/TR/css-flexbox-1/#flex-containers
class FlexContainer : public Box {
    GC_CELL(FlexContainer, Box);

public:
    FlexContainer(DOM::Document&, DOM::Node*, GC::Ref<CSS::ComputedProperties>);
    FlexContainer(DOM::Document&, DOM::Node*, NonnullOwnPtr<CSS::ComputedValues>);
    virtual ~FlexContainer() override;

    // Create corresponding paintable
    virtual GC::Ptr<Painting::Paintable> create_paintable() const override;

private:
    // Type checking helper
    virtual bool is_flex_container() const final { return true; }
};

// Fast type checking helper
template<>
inline bool Node::fast_is<FlexContainer>() const { return is_flex_container(); }

}

// ==================== Layout/FlexContainer.cpp ====================
#include <LibWeb/Layout/FlexContainer.h>
#include <LibWeb/Painting/FlexContainerPaintable.h>

namespace Web::Layout {

FlexContainer::FlexContainer(DOM::Document& document, DOM::Node* node, GC::Ref<CSS::ComputedProperties> properties)
    : Box(document, node, move(properties))
{
}

FlexContainer::FlexContainer(DOM::Document& document, DOM::Node* node, NonnullOwnPtr<CSS::ComputedValues> computed_values)
    : Box(document, node, move(computed_values))
{
}

FlexContainer::~FlexContainer() = default;

GC::Ptr<Painting::Paintable> FlexContainer::create_paintable() const
{
    return Painting::FlexContainerPaintable::create(*this);
}

}

// ==================== Layout/FlexFormattingContext.h ====================
#pragma once

#include <LibWeb/CSS/FlexibleLength.h>
#include <LibWeb/Layout/FormattingContext.h>

namespace Web::Layout {

// https://www.w3.org/TR/css-flexbox-1/#flex-formatting-context
class FlexFormattingContext final : public FormattingContext {
public:
    explicit FlexFormattingContext(LayoutState&, Box const& flex_container, FormattingContext* parent);
    ~FlexFormattingContext();

    virtual void run(Box const&, LayoutMode, AvailableSpace const&) override;
    virtual CSSPixels automatic_content_width() const override;
    virtual CSSPixels automatic_content_height() const override;

private:
    // Flex container properties
    bool is_row_layout() const;
    bool is_wrap_reverse() const;
    CSS::FlexDirection flex_direction() const;

    // Main algorithm steps
    void determine_flex_base_size_and_hypothetical_main_size();
    void determine_main_size_of_flex_container();
    void collect_flex_items_into_flex_lines();
    void resolve_flexible_lengths();
    void determine_hypothetical_cross_size_of_item(FlexItem&);
    void calculate_cross_size_of_flex_items();
    void determine_cross_size_of_flex_container();
    void main_axis_alignment();
    void cross_axis_alignment();

    struct FlexItem {
        Box const& box;
        CSSPixels flex_base_size { 0 };
        CSSPixels hypothetical_main_size { 0 };
        CSSPixels hypothetical_cross_size { 0 };
        CSSPixels main_size { 0 };
        CSSPixels cross_size { 0 };
        CSSPixels main_offset { 0 };
        CSSPixels cross_offset { 0 };
        bool is_min_violation { false };
        bool is_max_violation { false };
    };

    struct FlexLine {
        Vector<FlexItem*> items;
        CSSPixels cross_size { 0 };
    };

    Vector<FlexItem> m_flex_items;
    Vector<FlexLine> m_flex_lines;

    Box const& m_flex_container;
};

}

// ==================== Layout/FlexFormattingContext.cpp ====================
#include <LibWeb/Layout/FlexFormattingContext.h>
#include <LibWeb/Layout/FlexContainer.h>

namespace Web::Layout {

FlexFormattingContext::FlexFormattingContext(LayoutState& state, Box const& flex_container, FormattingContext* parent)
    : FormattingContext(Type::Flex, state, flex_container, parent)
    , m_flex_container(flex_container)
{
}

FlexFormattingContext::~FlexFormattingContext() = default;

// https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
void FlexFormattingContext::run(Box const& box, LayoutMode layout_mode, AvailableSpace const& available_space)
{
    // 1. Generate anonymous flex items
    // (Done during layout tree construction)

    // 2. Determine the available main and cross space for the flex items
    determine_main_size_of_flex_container();

    // 3. Determine the flex base size and hypothetical main size of each item
    determine_flex_base_size_and_hypothetical_main_size();

    // 4. Collect flex items into flex lines
    collect_flex_items_into_flex_lines();

    // 5. Resolve the flexible lengths
    resolve_flexible_lengths();

    // 6. Determine the hypothetical cross size of each item
    for (auto& item : m_flex_items)
        determine_hypothetical_cross_size_of_item(item);

    // 7. Calculate the cross size of each flex line
    calculate_cross_size_of_flex_items();

    // 8. Determine the used cross size of each flex item
    determine_cross_size_of_flex_container();

    // 9. Main-Axis Alignment
    main_axis_alignment();

    // 10. Cross-Axis Alignment
    cross_axis_alignment();

    // 11. Determine the flex container's used cross size
    // (Handled in determine_cross_size_of_flex_container)
}

bool FlexFormattingContext::is_row_layout() const
{
    auto direction = flex_direction();
    return direction == CSS::FlexDirection::Row || direction == CSS::FlexDirection::RowReverse;
}

CSS::FlexDirection FlexFormattingContext::flex_direction() const
{
    return m_flex_container.computed_values().flex_direction();
}

// https://www.w3.org/TR/css-flexbox-1/#algo-main-container
void FlexFormattingContext::determine_main_size_of_flex_container()
{
    // If the flex container has a definite main size, that's it.
    auto const& computed_values = m_flex_container.computed_values();

    if (is_row_layout()) {
        // For row: main size is width
        if (!computed_values.width().is_auto()) {
            auto main_size = computed_values.width().to_px(m_flex_container);
            m_state.get_mutable(m_flex_container).set_content_width(main_size);
        }
    } else {
        // For column: main size is height
        if (!computed_values.height().is_auto()) {
            auto main_size = computed_values.height().to_px(m_flex_container);
            m_state.get_mutable(m_flex_container).set_content_height(main_size);
        }
    }
}

// https://www.w3.org/TR/css-flexbox-1/#algo-main-item
void FlexFormattingContext::determine_flex_base_size_and_hypothetical_main_size()
{
    for (auto& item : m_flex_items) {
        // A. Determine the flex base size
        auto const& computed_values = item.box.computed_values();
        auto flex_basis = computed_values.flex_basis();

        if (flex_basis.is_content()) {
            // Use automatic sizing
            item.flex_base_size = automatic_content_width();
        } else if (flex_basis.is_length()) {
            item.flex_base_size = flex_basis.length().to_px(item.box);
        }

        // B. The hypothetical main size is the item's flex base size
        //    clamped by its min and max main size properties
        auto min_main = computed_values.min_width().to_px(item.box);
        auto max_main = computed_values.max_width().to_px(item.box);

        item.hypothetical_main_size = clamp(item.flex_base_size, min_main, max_main);
    }
}

void FlexFormattingContext::collect_flex_items_into_flex_lines()
{
    // https://www.w3.org/TR/css-flexbox-1/#algo-line-break

    // If flex-wrap is nowrap, all items go in single line
    if (m_flex_container.computed_values().flex_wrap() == CSS::FlexWrap::Nowrap) {
        FlexLine line;
        for (auto& item : m_flex_items)
            line.items.append(&item);
        m_flex_lines.append(move(line));
        return;
    }

    // Otherwise, break into multiple lines
    // (Implementation for wrapping...)
}

void FlexFormattingContext::resolve_flexible_lengths()
{
    // https://www.w3.org/TR/css-flexbox-1/#resolve-flexible-lengths

    for (auto& line : m_flex_lines) {
        // 1. Determine the used flex factor
        CSSPixels free_space = calculate_free_space(line);

        if (free_space > 0) {
            // Use flex-grow
            distribute_positive_free_space(line, free_space);
        } else if (free_space < 0) {
            // Use flex-shrink
            distribute_negative_free_space(line, -free_space);
        }
    }
}

void FlexFormattingContext::main_axis_alignment()
{
    // https://www.w3.org/TR/css-flexbox-1/#main-alignment

    auto justify_content = m_flex_container.computed_values().justify_content();

    for (auto& line : m_flex_lines) {
        CSSPixels free_space = calculate_free_space(line);
        CSSPixels offset = 0;

        switch (justify_content) {
        case CSS::JustifyContent::FlexStart:
            offset = 0;
            break;
        case CSS::JustifyContent::FlexEnd:
            offset = free_space;
            break;
        case CSS::JustifyContent::Center:
            offset = free_space / 2;
            break;
        case CSS::JustifyContent::SpaceBetween:
            // Distribute between items
            break;
        case CSS::JustifyContent::SpaceAround:
            // Distribute around items
            break;
        }

        // Position items
        for (auto* item : line.items) {
            item->main_offset = offset;
            offset += item->main_size;
        }
    }
}

}

// ==================== Usage in Layout Tree Builder ====================

// In LayoutTreeBuilder.cpp
GC::Ref<Layout::Node> create_layout_node(DOM::Node& node, CSS::ComputedValues const& computed)
{
    auto display = computed.display();

    // Check if this creates a flex container
    if (display.is_flex_inside()) {
        return realm.create<Layout::FlexContainer>(document, &node, computed);
    }

    // ... other layout node types
}
