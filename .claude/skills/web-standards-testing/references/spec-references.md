# Web Specifications Reference

Quick reference for web specifications relevant to Ladybird development.

## WHATWG Living Standards

### HTML Living Standard
- **URL**: https://html.spec.whatwg.org/
- **Topics**: HTML elements, parsing, scripting, webappapis
- **Key Sections**:
  - [DOM](https://html.spec.whatwg.org/multipage/dom.html)
  - [Semantics](https://html.spec.whatwg.org/multipage/semantics.html)
  - [Forms](https://html.spec.whatwg.org/multipage/forms.html)
  - [Canvas](https://html.spec.whatwg.org/multipage/canvas.html)
  - [Web Storage](https://html.spec.whatwg.org/multipage/webstorage.html)

### DOM Living Standard
- **URL**: https://dom.spec.whatwg.org/
- **Topics**: DOM nodes, events, ranges, selections
- **Key Sections**:
  - [Nodes](https://dom.spec.whatwg.org/#nodes)
  - [Events](https://dom.spec.whatwg.org/#events)
  - [Ranges](https://dom.spec.whatwg.org/#ranges)
  - [Mutation Observers](https://dom.spec.whatwg.org/#mutation-observers)

### Fetch Living Standard
- **URL**: https://fetch.spec.whatwg.org/
- **Topics**: HTTP requests, CORS, service workers
- **Key Sections**:
  - [Fetch API](https://fetch.spec.whatwg.org/#fetch-api)
  - [Request](https://fetch.spec.whatwg.org/#requests)
  - [Response](https://fetch.spec.whatwg.org/#responses)
  - [CORS](https://fetch.spec.whatwg.org/#http-cors-protocol)

### URL Living Standard
- **URL**: https://url.spec.whatwg.org/
- **Topics**: URL parsing, URLSearchParams
- **Key Sections**:
  - [URL class](https://url.spec.whatwg.org/#url-class)
  - [URLSearchParams](https://url.spec.whatwg.org/#urlsearchparams)

### Console Living Standard
- **URL**: https://console.spec.whatwg.org/
- **Topics**: console.log, console.error, etc.

### Streams Living Standard
- **URL**: https://streams.spec.whatwg.org/
- **Topics**: ReadableStream, WritableStream

## W3C CSS Specifications

### CSS Specifications Index
- **URL**: https://www.w3.org/Style/CSS/specs.en.html
- **All CSS specs**: https://www.w3.org/TR/?tag=css

### Core CSS

**CSS Syntax Module Level 3**
- **URL**: https://www.w3.org/TR/css-syntax-3/
- **Topics**: CSS parsing, tokenization

**CSS Selectors Level 4**
- **URL**: https://www.w3.org/TR/selectors-4/
- **Topics**: Element selectors, pseudo-classes, pseudo-elements

**CSS Cascading and Inheritance Level 4**
- **URL**: https://www.w3.org/TR/css-cascade-4/
- **Topics**: Cascade, specificity, inheritance

**CSS Values and Units Level 4**
- **URL**: https://www.w3.org/TR/css-values-4/
- **Topics**: px, em, rem, %, calc(), var()

### Layout

**CSS Box Model Module Level 3**
- **URL**: https://www.w3.org/TR/css-box-3/
- **Topics**: Content box, padding, border, margin

**CSS Flexbox Layout Module Level 1**
- **URL**: https://www.w3.org/TR/css-flexbox-1/
- **Topics**: display: flex, flex-direction, justify-content

**CSS Grid Layout Module Level 2**
- **URL**: https://www.w3.org/TR/css-grid-2/
- **Topics**: display: grid, grid-template, grid areas

**CSS Positioned Layout Module Level 3**
- **URL**: https://www.w3.org/TR/css-position-3/
- **Topics**: position: absolute/relative/fixed/sticky

**CSS Display Module Level 3**
- **URL**: https://www.w3.org/TR/css-display-3/
- **Topics**: display property values

### Visual Effects

**CSS Color Module Level 4**
- **URL**: https://www.w3.org/TR/css-color-4/
- **Topics**: rgb(), hsl(), hwb(), color spaces

**CSS Backgrounds and Borders Module Level 3**
- **URL**: https://www.w3.org/TR/css-backgrounds-3/
- **Topics**: background, border, border-radius, box-shadow

**CSS Transforms Module Level 1**
- **URL**: https://www.w3.org/TR/css-transforms-1/
- **Topics**: transform, rotate, scale, translate

**CSS Transitions**
- **URL**: https://www.w3.org/TR/css-transitions-1/
- **Topics**: transition property

**CSS Animations Level 1**
- **URL**: https://www.w3.org/TR/css-animations-1/
- **Topics**: @keyframes, animation property

**Filter Effects Module Level 1**
- **URL**: https://www.w3.org/TR/filter-effects-1/
- **Topics**: filter, backdrop-filter

### Text and Fonts

**CSS Fonts Module Level 4**
- **URL**: https://www.w3.org/TR/css-fonts-4/
- **Topics**: @font-face, font-family, font-weight

**CSS Text Module Level 3**
- **URL**: https://www.w3.org/TR/css-text-3/
- **Topics**: text-align, white-space, word-break

**CSS Writing Modes Level 4**
- **URL**: https://www.w3.org/TR/css-writing-modes-4/
- **Topics**: writing-mode, direction

### Other

**CSS Custom Properties for Cascading Variables**
- **URL**: https://www.w3.org/TR/css-variables-1/
- **Topics**: --custom-property, var()

**CSS Containment Module Level 2**
- **URL**: https://www.w3.org/TR/css-contain-2/
- **Topics**: contain, content-visibility

## ECMAScript (JavaScript)

### ECMAScript Language Specification
- **URL**: https://tc39.es/ecma262/
- **Topics**: JavaScript language, built-in objects
- **Key Sections**:
  - [Types](https://tc39.es/ecma262/#sec-ecmascript-data-types-and-values)
  - [Objects](https://tc39.es/ecma262/#sec-objects)
  - [Functions](https://tc39.es/ecma262/#sec-ecmascript-function-objects)
  - [Promises](https://tc39.es/ecma262/#sec-promise-objects)

### TC39 Proposals
- **URL**: https://github.com/tc39/proposals
- **Finished proposals** (Stage 4): Merged into spec
- **Active proposals**: Stage 0-3

## Web APIs

### WebIDL
- **URL**: https://webidl.spec.whatwg.org/
- **Topics**: IDL syntax, types, extended attributes

### Web Workers
- **URL**: https://html.spec.whatwg.org/multipage/workers.html
- **Topics**: Worker, SharedWorker, MessagePort

### WebSocket
- **URL**: https://websockets.spec.whatwg.org/
- **Topics**: WebSocket API

### WebGL
- **URL**: https://www.khronos.org/registry/webgl/specs/latest/
- **Topics**: WebGL 1.0, WebGL 2.0 APIs

### Web Audio API
- **URL**: https://www.w3.org/TR/webaudio/
- **Topics**: AudioContext, audio processing

### Canvas 2D API
- **URL**: https://html.spec.whatwg.org/multipage/canvas.html
- **Topics**: CanvasRenderingContext2D

## ARIA (Accessibility)

### WAI-ARIA
- **URL**: https://www.w3.org/TR/wai-aria-1.2/
- **Topics**: ARIA roles, states, properties

### ARIA in HTML
- **URL**: https://www.w3.org/TR/html-aria/
- **Topics**: How to use ARIA in HTML

## Finding Specs

### By Feature Name
1. Google: "feature_name specification"
2. MDN docs often link to specs
3. Can I Use has spec links: https://caniuse.com/

### By Test
WPT tests include `<link rel="help">` with spec URLs

### By Browser Behavior
1. Check Chrome DevTools source links
2. Firefox has "Learn more" links in console
3. Safari Web Inspector shows spec sections

## Reading Specs Effectively

### Algorithm Notation

Specs use imperative language:

```
To fire an event named e at target, given an optional boolean flag:
1. If flag is not given, let flag be false.
2. Let event be a new event.
3. Initialize event's type attribute to e.
4. Dispatch event at target.
5. Return event.
```

Implement as:

```cpp
JS::NonnullGCPtr<Event> fire_an_event(
    EventTarget& target,
    FlyString const& event_name,
    bool flag = false)
{
    // Step 1: (handled by default parameter)

    // Step 2: Let event be a new event.
    auto event = Event::create(event_name);

    // Step 3: Initialize event's type attribute to e
    // (handled in Event::create)

    // Step 4: Dispatch event at target
    target.dispatch_event(event);

    // Step 5: Return event
    return event;
}
```

### Key Phrases

- **"User agents must"** - Required behavior
- **"User agents should"** - Recommended behavior
- **"User agents may"** - Optional behavior
- **"For the purposes of"** - Definition follows
- **"Let x be"** - Variable assignment
- **"If x, then"** - Conditional
- **"For each"** - Loop
- **"Return"** - Function return value

### IDL Syntax

```webidl
interface Element : Node {
    // Attributes
    readonly attribute DOMString tagName;
    attribute DOMString id;

    // Methods
    void setAttribute(DOMString name, DOMString value);
    DOMString? getAttribute(DOMString name);

    // Extended attributes
    [CEReactions] void removeAttribute(DOMString name);
};
```

- `readonly` - No setter
- `?` - Nullable type
- `[CEReactions]` - Triggers custom element reactions

## Spec Testing Strategy

1. **Find spec section** for feature being implemented
2. **Read algorithm** carefully, noting all steps
3. **Implement algorithm** matching spec naming
4. **Write test** covering each step
5. **Test edge cases** mentioned in spec
6. **Compare** with other browsers if behavior unclear

## Keeping Up to Date

### Spec Changes

- **WHATWG specs** update continuously (Living Standards)
- **W3C specs** publish periodic snapshots
- **TC39 proposals** advance through stages

### Monitoring Updates

- **WHATWG GitHub**: https://github.com/whatwg
- **W3C Working Drafts**: https://www.w3.org/TR/
- **TC39 meetings**: https://github.com/tc39/notes

### When Specs Conflict

1. **WHATWG HTML** supersedes W3C HTML
2. **Latest spec** wins for same organization
3. **Browser consensus** matters for ambiguities
4. **Ask on Discord** if unsure for Ladybird

## Quick Reference Table

| Feature | Spec | URL |
|---------|------|-----|
| HTML Elements | WHATWG HTML | https://html.spec.whatwg.org/ |
| DOM APIs | WHATWG DOM | https://dom.spec.whatwg.org/ |
| CSS Layout | W3C CSS | https://www.w3.org/TR/?tag=css |
| JavaScript | TC39 ECMAScript | https://tc39.es/ecma262/ |
| Fetch API | WHATWG Fetch | https://fetch.spec.whatwg.org/ |
| Canvas | WHATWG HTML (Canvas section) | https://html.spec.whatwg.org/multipage/canvas.html |
| WebGL | Khronos | https://www.khronos.org/webgl/ |
| Events | WHATWG DOM (Events section) | https://dom.spec.whatwg.org/#events |
