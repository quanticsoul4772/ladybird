/*
 * Copyright (c) 2025, Your Name <your.email@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/FlyString.h>
#include <AK/StringBuilder.h>
#include <LibWeb/ARIA/ReferenceResolution.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/DOM/Text.h>
#include <LibWeb/Infra/CharacterTypes.h>

namespace Web::ARIA {

// https://www.w3.org/TR/wai-aria-1.2/#valuetype_idref_list
ErrorOr<Vector<GC::Ref<DOM::Element>>> resolve_id_references(
    StringView id_list,
    DOM::Document const& document)
{
    // 1. Split the ID list on ASCII whitespace
    auto id_tokens = id_list.split_view_if(Infra::is_ascii_whitespace);

    Vector<GC::Ref<DOM::Element>> elements;

    // 2. For each ID, resolve to an element
    for (auto const& id_token : id_tokens) {
        if (id_token.is_empty())
            continue;

        // 3. Get element by ID from document
        auto fly_string_id = TRY(FlyString::from_utf8(id_token));
        auto element = document.get_element_by_id(fly_string_id);

        // 4. If any ID is not found, return empty vector
        if (!element)
            return Vector<GC::Ref<DOM::Element>> {};

        TRY(elements.try_append(*element));
    }

    return elements;
}

// https://www.w3.org/TR/accname-1.2/#step2B
String get_text_from_id_references(
    StringView id_list,
    DOM::Document const& document)
{
    // 1. Split the ID list on ASCII whitespace
    auto id_tokens = id_list.split_view_if(Infra::is_ascii_whitespace);

    StringBuilder text_builder;
    bool first = true;

    // 2. For each ID, get the text content
    for (auto const& id_token : id_tokens) {
        if (id_token.is_empty())
            continue;

        // 3. Get element by ID from document
        auto fly_string_id = MUST(FlyString::from_utf8(id_token));
        auto element = document.get_element_by_id(fly_string_id);

        if (!element)
            continue;

        // 4. Get text content from the element
        // Note: textContent recursively gets all text from descendant nodes
        auto text_content = element->text_content();
        if (!text_content.has_value() || text_content->is_empty())
            continue;

        // 5. Join multiple text contents with spaces
        if (!first)
            text_builder.append(' ');
        text_builder.append(text_content.value());
        first = false;
    }

    return MUST(text_builder.to_string());
}

}
