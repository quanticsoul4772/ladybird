/*
 * Copyright (c) 2025, Your Name <your.email@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/ARIA/AccessibleNameCalculation.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/DOM/Node.h>

namespace Web::ARIA {

// https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_name
// Compute the accessible name for an element using the AccName 1.2 algorithm
ErrorOr<String> compute_accessible_name(DOM::Element const& element, bool allow_name_from_content)
{
    // NOTE: The full implementation of the Accessible Name and Description Computation
    // algorithm is currently in DOM::Node::accessible_name() and DOM::Node::name_or_description().
    // This wrapper function delegates to that implementation for now.
    //
    // The algorithm follows these steps per AccName 1.2:
    // 1. Check aria-labelledby - resolve IDs and concatenate text
    // 2. Check aria-label attribute
    // 3. Check native attributes (alt, value, placeholder, etc.)
    // 4. Check name from content (if role allows it)
    // 5. Check title attribute as fallback
    //
    // For full details, see:
    // https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_te

    // Delegate to the existing implementation in DOM::Node
    // The allow_name_from_content parameter is not currently used by the Node implementation,
    // as it determines this based on the element's role using ARIA::allows_name_from_content()
    (void)allow_name_from_content;

    return element.accessible_name(element.document());
}

// https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_description
// Compute the accessible description for an element using the AccName 1.2 algorithm
ErrorOr<String> compute_accessible_description(DOM::Element const& element)
{
    // NOTE: The full implementation of the Accessible Description Computation
    // algorithm is currently in DOM::Node::accessible_description() and DOM::Node::name_or_description().
    // This wrapper function delegates to that implementation for now.
    //
    // The algorithm follows these steps per AccName 1.2:
    // 1. Check aria-describedby - resolve IDs and concatenate text
    // 2. Check aria-description attribute
    // 3. Check title attribute (if not used for name)
    //
    // For full details, see:
    // https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_description

    // Delegate to the existing implementation in DOM::Node
    return element.accessible_description(element.document());
}

}
