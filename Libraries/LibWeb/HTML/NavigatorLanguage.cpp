/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 * Copyright (c) 2024, Jamie Mansfield <jmansfield@cadixdev.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/HTML/NavigatorLanguage.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Loader/ResourceLoader.h>
#include <LibWeb/Page/Page.h>

namespace Web::HTML {

// https://html.spec.whatwg.org/multipage/system-state.html#dom-navigator-language
String const& NavigatorLanguageMixin::language() const
{
    // Track potential fingerprinting (Milestone 0.4 Phase 4)
    auto const& window = as<HTML::Window>(HTML::current_principal_global_object());
    window.page().client().page_did_call_fingerprinting_api("navigator"sv, "language"sv);

    return ResourceLoader::the().preferred_languages()[0];
}

}
