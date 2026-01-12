/*
 * Copyright (c) 2025, Your Name <your.email@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Vector.h>
#include <LibGC/Ptr.h>
#include <LibWeb/Forward.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::ARIA {

// https://www.w3.org/TR/wai-aria-1.2/#valuetype_idref_list
// Resolve space-separated list of IDs to elements.
// Returns empty vector if any ID is not found.
ErrorOr<Vector<GC::Ref<DOM::Element>>> resolve_id_references(
    StringView id_list,
    DOM::Document const& document);

// Get concatenated text content from elements referenced by ID list.
// Used for aria-labelledby and aria-describedby computation.
// https://www.w3.org/TR/accname-1.2/#step2B
String get_text_from_id_references(
    StringView id_list,
    DOM::Document const& document);

}
