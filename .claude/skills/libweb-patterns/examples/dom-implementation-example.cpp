// Example: Implementing a new HTML element
// This demonstrates the complete pattern for adding HTMLDetailsElement

// ==================== HTMLDetailsElement.idl ====================
/*
#import <HTML/HTMLElement.idl>

// https://html.spec.whatwg.org/multipage/interactive-elements.html#htmldetailselement
[Exposed=Window]
interface HTMLDetailsElement : HTMLElement {
    [HTMLConstructor] constructor();

    [CEReactions, Reflect] attribute boolean open;
};
*/

// ==================== HTMLDetailsElement.h ====================
#pragma once

#include <LibWeb/HTML/HTMLElement.h>

namespace Web::HTML {

class HTMLDetailsElement : public HTMLElement {
    WEB_PLATFORM_OBJECT(HTMLDetailsElement, HTMLElement);
    GC_DECLARE_ALLOCATOR(HTMLDetailsElement);

public:
    virtual ~HTMLDetailsElement() override;

    // IDL attributes
    bool open() const { return has_attribute(AttributeNames::open); }
    void set_open(bool value);

protected:
    HTMLDetailsElement(DOM::Document&, DOM::QualifiedName);

    virtual void initialize(JS::Realm&) override;
    virtual void visit_edges(Cell::Visitor&) override;

    virtual void attribute_changed(FlyString const& name, Optional<String> const& old_value,
                                   Optional<String> const& value, Optional<FlyString> const& namespace_) override;

private:
    void queue_details_toggle_event_task(String old_state, String new_state);
};

}

// ==================== HTMLDetailsElement.cpp ====================
#include <LibWeb/Bindings/HTMLDetailsElementPrototype.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/HTMLDetailsElement.h>

namespace Web::HTML {

GC_DEFINE_ALLOCATOR(HTMLDetailsElement);

HTMLDetailsElement::HTMLDetailsElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : HTMLElement(document, move(qualified_name))
{
}

HTMLDetailsElement::~HTMLDetailsElement() = default;

void HTMLDetailsElement::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    WEB_SET_PROTOTYPE_FOR_INTERFACE(HTMLDetailsElement);
}

void HTMLDetailsElement::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    // Visit any GC pointers here
}

// https://html.spec.whatwg.org/multipage/interactive-elements.html#dom-details-open
void HTMLDetailsElement::set_open(bool value)
{
    if (value)
        MUST(set_attribute(AttributeNames::open, String {}));
    else
        remove_attribute(AttributeNames::open);
}

void HTMLDetailsElement::attribute_changed(FlyString const& name, Optional<String> const& old_value,
                                          Optional<String> const& value, Optional<FlyString> const& namespace_)
{
    Base::attribute_changed(name, old_value, value, namespace_);

    if (name == AttributeNames::open) {
        // Trigger layout tree update when open state changes
        set_needs_layout_tree_update(DOM::SetNeedsLayoutTreeUpdateReason::DetailsElementOpenedOrClosed);

        // Queue toggle event
        String old_state = old_value.has_value() ? "open"_string : "closed"_string;
        String new_state = value.has_value() ? "open"_string : "closed"_string;
        queue_details_toggle_event_task(old_state, new_state);
    }
}

void HTMLDetailsElement::queue_details_toggle_event_task(String old_state, String new_state)
{
    // https://html.spec.whatwg.org/multipage/interactive-elements.html#queue-a-details-toggle-event-task

    // 1. If element's details toggle task tracker is not null, then:
    //    (Implementation would go here)

    // 2. Queue an element task on the DOM manipulation task source given element and the following steps:
    queue_an_element_task(HTML::Task::Source::DOMManipulation, [this, old_state, new_state] {
        // 2.1. Fire an event named toggle at element, using ToggleEvent
        auto event_init = DOM::ToggleEventInit {};
        event_init.old_state = old_state;
        event_init.new_state = new_state;

        auto event = DOM::ToggleEvent::create(realm(), HTML::EventNames::toggle, event_init);
        dispatch_event(event);
    });
}

}

// ==================== Registration Steps ====================

// 1. Add to Libraries/LibWeb/idl_files.cmake:
//    libweb_js_bindings(HTML/HTMLDetailsElement)

// 2. Add to Libraries/LibWeb/Forward.h:
//    namespace Web::HTML {
//        class HTMLDetailsElement;
//    }

// 3. Build and test:
//    ./Meta/ladybird.py build
//    ./Meta/ladybird.py run
