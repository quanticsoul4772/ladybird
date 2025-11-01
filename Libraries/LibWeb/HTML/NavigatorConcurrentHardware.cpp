/*
 * Copyright (c) 2022, Andrew Kaster <akaster@serenityos.org>
 * Copyright (c) 2024, Shannon Booth <shannon@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/System.h>
#include <LibWeb/HTML/NavigatorConcurrentHardware.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>

namespace Web::HTML {

// https://html.spec.whatwg.org/multipage/workers.html#dom-navigator-hardwareconcurrency
WebIDL::UnsignedLongLong NavigatorConcurrentHardwareMixin::hardware_concurrency() const
{
    // Track potential fingerprinting (Milestone 0.4 Phase 4)
    auto const& window = as<HTML::Window>(HTML::current_principal_global_object());
    window.page().client().page_did_call_fingerprinting_api("navigator"sv, "hardwareConcurrency"sv);

    return Core::System::hardware_concurrency();
}

}
