/*
 * Copyright (c) 2025, Your Name <your.email@example.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <LibWeb/Export.h>
#include <LibWeb/Forward.h>

namespace Web::ARIA {

// https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_name
// Main entry point - compute accessible name for an element
WEB_API ErrorOr<String> compute_accessible_name(
    DOM::Element const& element,
    bool allow_name_from_content = true);

// https://www.w3.org/TR/accname-1.2/#mapping_additional_nd_description
// Main entry point - compute accessible description for an element
WEB_API ErrorOr<String> compute_accessible_description(
    DOM::Element const& element);

}
