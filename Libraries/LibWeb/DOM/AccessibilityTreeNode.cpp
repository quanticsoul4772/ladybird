/*
 * Copyright (c) 2022, Jonah Shafran <jonahshafran@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/DOM/AccessibilityTreeNode.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/DOM/Node.h>
#include <LibWeb/DOM/Text.h>

namespace Web::DOM {

GC_DEFINE_ALLOCATOR(AccessibilityTreeNode);

GC::Ref<AccessibilityTreeNode> AccessibilityTreeNode::create(Document* document, DOM::Node const* value)
{
    return document->realm().create<AccessibilityTreeNode>(value);
}

AccessibilityTreeNode::AccessibilityTreeNode(GC::Ptr<DOM::Node const> value)
    : m_value(value)
{
    m_children = {};

    // Compute ARIA properties at node creation time
    // https://www.w3.org/TR/wai-aria-1.2/
    if (!value)
        return;

    auto const* element = dynamic_cast<DOM::Element const*>(value.ptr());
    if (!element)
        return;

    // Compute role: explicit role from 'role' attribute, or implicit role from element
    // https://www.w3.org/TR/wai-aria-1.2/#introroles
    m_computed_role = element->role_or_default();

    // Compute accessible name
    // https://www.w3.org/TR/accname-1.2/
    // NOTE: The full accessible name calculation algorithm is implemented in Element::accessible_name()
    // TODO: Cache the computed name here once the algorithm is fully implemented by another agent
    // For now, we leave this empty and let serialize_tree_as_json() call the algorithm directly
    // m_computed_name = element->accessible_name(element->document()).release_value_but_fixme_should_propagate_errors();

    // Compute accessible description
    // https://www.w3.org/TR/accname-1.2/
    // NOTE: The full accessible description calculation algorithm is implemented in Element::accessible_description()
    // TODO: Cache the computed description here once the algorithm is fully implemented by another agent
    // For now, we leave this empty and let serialize_tree_as_json() call the algorithm directly
    // m_computed_description = element->accessible_description(element->document()).release_value_but_fixme_should_propagate_errors();
}

void AccessibilityTreeNode::serialize_tree_as_json(JsonObjectSerializer<StringBuilder>& object, Document const& document) const
{
    if (value()->is_document()) {
        VERIFY_NOT_REACHED();
    } else if (value()->is_element()) {
        auto const* element = static_cast<DOM::Element const*>(value().ptr());

        if (element->include_in_accessibility_tree()) {
            MUST(object.add("type"sv, "element"sv));

            // Use computed role if available, otherwise compute on-the-fly
            // The computed role is cached at node creation for performance
            Optional<ARIA::Role> role;
            if (m_computed_role.has_value()) {
                role = m_computed_role;
            } else {
                role = element->role_or_default();
            }
            bool has_role = role.has_value() && !ARIA::is_abstract_role(*role);

            // Compute name and description on-the-fly for now
            // TODO: Use m_computed_name and m_computed_description once they are populated
            auto name = MUST(element->accessible_name(document));
            MUST(object.add("name"sv, name));
            auto description = MUST(element->accessible_description(document));
            MUST(object.add("description"sv, description));

            if (has_role)
                MUST(object.add("role"sv, ARIA::role_name(*role)));
            else
                MUST(object.add("role"sv, ""sv));
        } else {
            VERIFY_NOT_REACHED();
        }
    } else if (value()->is_text()) {
        MUST(object.add("type"sv, "text"sv));

        auto const* text_node = static_cast<DOM::Text const*>(value().ptr());
        MUST(object.add("name"sv, text_node->data().to_utf8()));
        MUST(object.add("role"sv, "text leaf"sv));
    }

    MUST(object.add("id"sv, value()->unique_id().value()));

    if (value()->has_child_nodes()) {
        auto node_children = MUST(object.add_array("children"sv));
        for (auto& child : children()) {
            if (child->value()->is_uninteresting_whitespace_node())
                continue;
            JsonObjectSerializer<StringBuilder> child_object = MUST(node_children.add_object());
            child->serialize_tree_as_json(child_object, document);
            MUST(child_object.finish());
        }
        MUST(node_children.finish());
    }
}

void AccessibilityTreeNode::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_value);
    visitor.visit(m_children);
}

}
