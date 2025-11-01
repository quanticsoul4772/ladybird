# Ladybird Browser - Comprehensive E2E Test Matrix

**Version**: 1.0
**Last Updated**: 2025-11-01
**Scope**: Complete user flow coverage for Ladybird browser with Sentinel security features

## Overview

This document defines a comprehensive test matrix for automated end-to-end testing of Ladybird browser using Playwright. Tests are prioritized by criticality and categorized by functional area.

**Priority Levels**:
- **P0 (Critical)**: Core browser functionality and security features - must work
- **P1 (Important)**: Enhanced features and user experience - should work
- **P2 (Nice-to-have)**: Advanced features and edge cases - would be nice

**Target Test Count**: 180-220 automated E2E tests

---

## 1. Core Browser Functionality (P0)

### 1.1 Navigation (P0) - 15 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| NAV-001 | URL bar navigation to HTTP site | P0 | Page loads, title updates, URL bar updates |
| NAV-002 | URL bar navigation to HTTPS site | P0 | TLS connection, secure indicator, page loads |
| NAV-003 | Forward/back button navigation | P0 | History stack, page state preservation |
| NAV-004 | Reload button (F5) | P0 | Page reloads, cache behavior |
| NAV-005 | Hard reload (Ctrl+Shift+R) | P0 | Cache bypassed, fresh resources |
| NAV-006 | External link click (target=_blank) | P0 | New tab opens, opener relationship |
| NAV-007 | External link click (target=_self) | P0 | Same tab navigation |
| NAV-008 | Anchor link navigation (#section) | P0 | Scroll to anchor, no page reload |
| NAV-009 | Fragment navigation with smooth scroll | P0 | Smooth scroll behavior |
| NAV-010 | JavaScript window.location navigation | P0 | Programmatic navigation works |
| NAV-011 | Meta refresh redirect | P0 | Auto-redirect after timeout |
| NAV-012 | 301 permanent redirect | P0 | Follow redirect, update URL |
| NAV-013 | 302 temporary redirect | P0 | Follow redirect, history handling |
| NAV-014 | Invalid URL handling | P0 | Error message, no crash |
| NAV-015 | Navigation to data: URL | P0 | Inline content renders |

### 1.2 Tab Management (P0) - 12 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| TAB-001 | Open new tab (Ctrl+T) | P0 | New tab created, focus switches |
| TAB-002 | Close tab (Ctrl+W) | P0 | Tab closes, focus moves to adjacent |
| TAB-003 | Switch tabs (Ctrl+Tab) | P0 | Tab focus changes, page state preserved |
| TAB-004 | Switch tabs by click | P0 | Tab activation, visual feedback |
| TAB-005 | Close last tab | P0 | Window closes or new blank tab |
| TAB-006 | Reopen closed tab (Ctrl+Shift+T) | P0 | Tab restored, history intact |
| TAB-007 | Drag tab to reorder | P0 | Tab order changes |
| TAB-008 | Drag tab to new window | P0 | Tab detaches, new window created |
| TAB-009 | Duplicate tab | P0 | New tab with same URL, separate history |
| TAB-010 | Pin/unpin tab | P0 | Tab size changes, position updates |
| TAB-011 | Multiple tabs (10+) performance | P0 | No crash, smooth switching |
| TAB-012 | Tab title updates dynamically | P0 | Tab text reflects document.title changes |

### 1.3 History Management (P0) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| HIST-001 | History navigation forward/back | P0 | Correct page restored |
| HIST-002 | History populated on navigation | P0 | URLs added to history |
| HIST-003 | History search/filter | P0 | Search results match query |
| HIST-004 | Clear browsing history | P0 | History deleted, privacy preserved |
| HIST-005 | History date grouping | P0 | Today/Yesterday/Last Week grouping |
| HIST-006 | Click history entry to navigate | P0 | Navigate to historical page |
| HIST-007 | Delete individual history entry | P0 | Entry removed from list |
| HIST-008 | Private browsing mode (no history) | P0 | History not persisted |

### 1.4 Bookmarks (P1) - 10 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| BOOK-001 | Add bookmark (Ctrl+D) | P1 | Bookmark saved, star icon updates |
| BOOK-002 | Remove bookmark | P1 | Bookmark deleted, star icon clears |
| BOOK-003 | Edit bookmark title/URL | P1 | Changes persisted |
| BOOK-004 | Organize bookmarks into folders | P1 | Folder structure maintained |
| BOOK-005 | Bookmark bar visibility toggle | P1 | Bar shows/hides bookmarks |
| BOOK-006 | Click bookmark to navigate | P1 | Navigate to bookmarked URL |
| BOOK-007 | Import bookmarks (HTML format) | P1 | Bookmarks imported correctly |
| BOOK-008 | Export bookmarks (HTML format) | P1 | File contains all bookmarks |
| BOOK-009 | Search bookmarks | P1 | Results match query |
| BOOK-010 | Bookmark duplicate detection | P1 | Warn on duplicate URL |

### 1.5 Settings & Preferences (P1) - 10 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| SET-001 | Open settings page | P1 | Settings UI loads |
| SET-002 | Change default search engine | P1 | Preference persisted, search uses new engine |
| SET-003 | Set custom homepage | P1 | New tabs/startup uses custom URL |
| SET-004 | Toggle JavaScript on/off | P1 | JS execution respects setting |
| SET-005 | Change font size/family | P1 | Page rendering updates |
| SET-006 | Privacy settings (cookies, tracking) | P1 | Settings applied to requests |
| SET-007 | Download location preference | P1 | Files saved to custom path |
| SET-008 | Theme selection (light/dark/auto) | P1 | UI theme updates |
| SET-009 | Clear cache and cookies | P1 | Storage cleared |
| SET-010 | Reset settings to defaults | P1 | All preferences reset |

---

## 2. Page Rendering (P0)

### 2.1 HTML Rendering (P0) - 12 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| HTML-001 | Render semantic HTML5 elements | P0 | Header, nav, article, section, footer visible |
| HTML-002 | Render headings H1-H6 | P0 | Correct hierarchy and styling |
| HTML-003 | Render paragraphs and text formatting | P0 | p, strong, em, mark tags work |
| HTML-004 | Render lists (ul, ol, dl) | P0 | Bullets, numbers, definitions |
| HTML-005 | Render tables with complex structure | P0 | thead, tbody, colspan, rowspan |
| HTML-006 | Render images (inline and block) | P0 | Images load and display |
| HTML-007 | Render SVG inline | P0 | Vector graphics render |
| HTML-008 | Render iframes | P0 | Nested content loads |
| HTML-009 | Render custom data attributes | P0 | Accessible via dataset API |
| HTML-010 | Render malformed HTML gracefully | P0 | Error recovery, no crash |
| HTML-011 | Render HTML5 template element | P0 | Template inert until cloned |
| HTML-012 | Render shadow DOM components | P0 | Encapsulated styling works |

### 2.2 CSS Layout & Styling (P0) - 18 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| CSS-001 | Flexbox layout (row/column) | P0 | Items flex correctly |
| CSS-002 | Flexbox wrapping and alignment | P0 | flex-wrap, justify-content, align-items |
| CSS-003 | CSS Grid layout (basic) | P0 | Grid template areas work |
| CSS-004 | CSS Grid complex (auto-fill, fr units) | P0 | Responsive grid behavior |
| CSS-005 | Positioning (static, relative, absolute, fixed) | P0 | Elements positioned correctly |
| CSS-006 | Z-index stacking context | P0 | Layering order correct |
| CSS-007 | CSS transforms (translate, rotate, scale) | P0 | Transformations apply |
| CSS-008 | CSS transitions | P0 | Smooth property changes |
| CSS-009 | CSS animations (@keyframes) | P0 | Animation plays |
| CSS-010 | CSS custom properties (variables) | P0 | Variables resolve |
| CSS-011 | Media queries (responsive design) | P0 | Layout adapts to viewport |
| CSS-012 | Pseudo-classes (:hover, :active, :focus) | P0 | States trigger styles |
| CSS-013 | Pseudo-elements (::before, ::after) | P0 | Content inserted |
| CSS-014 | CSS calc() function | P0 | Computed values correct |
| CSS-015 | Box model (margin, padding, border) | P0 | Spacing calculated correctly |
| CSS-016 | Display modes (block, inline, inline-block) | P0 | Layout behavior correct |
| CSS-017 | Float and clear | P0 | Text wrapping works |
| CSS-018 | CSS specificity and cascade | P0 | Most specific rule wins |

### 2.3 JavaScript Execution (P0) - 15 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| JS-001 | Execute inline script tags | P0 | Code runs, DOM updates |
| JS-002 | Load external JavaScript file | P0 | Script fetched and executed |
| JS-003 | DOM manipulation (createElement, appendChild) | P0 | DOM tree updates |
| JS-004 | Event listeners (click, input, submit) | P0 | Events fire callbacks |
| JS-005 | Event bubbling and capturing | P0 | Event propagation correct |
| JS-006 | Async/await and Promises | P0 | Asynchronous code executes |
| JS-007 | Fetch API for network requests | P0 | HTTP requests succeed |
| JS-008 | LocalStorage API | P0 | Data persists across sessions |
| JS-009 | SessionStorage API | P0 | Data cleared on tab close |
| JS-010 | Console logging (console.log/error/warn) | P0 | Messages appear in dev console |
| JS-011 | setTimeout and setInterval | P0 | Timers execute callbacks |
| JS-012 | JSON parse and stringify | P0 | Serialization works |
| JS-013 | Array methods (map, filter, reduce) | P0 | Modern JS array APIs |
| JS-014 | Template literals and destructuring | P0 | ES6+ syntax works |
| JS-015 | JavaScript error handling (try/catch) | P0 | Errors caught, no crash |

### 2.4 Web Fonts (P1) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FONT-001 | Load Google Fonts (WOFF2) | P1 | Custom font renders |
| FONT-002 | Font fallback chain | P1 | Fallback to system font if missing |
| FONT-003 | Web font loading states (FOUT/FOIT) | P1 | Text visible during load |
| FONT-004 | Font-weight and font-style variations | P1 | Bold/italic render correctly |
| FONT-005 | Unicode and emoji rendering | P1 | Non-ASCII characters display |

### 2.5 Images & Media (P0) - 10 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| IMG-001 | Load PNG images | P0 | Image displays |
| IMG-002 | Load JPEG images | P0 | Image displays |
| IMG-003 | Load WebP images | P0 | Modern format support |
| IMG-004 | Load SVG images | P0 | Vector graphics scale |
| IMG-005 | Lazy loading (loading="lazy") | P0 | Images load on scroll |
| IMG-006 | Responsive images (srcset) | P0 | Correct size loaded |
| IMG-007 | Image alt text accessibility | P0 | Alt text available to screen readers |
| IMG-008 | Broken image placeholder | P0 | Missing image shows placeholder |
| IMG-009 | Background images (CSS) | P0 | Background renders |
| IMG-010 | Image decode API | P0 | Async decoding works |

---

## 3. Form Handling (P0)

### 3.1 Form Input Types (P0) - 12 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FORM-001 | Text input field | P0 | Text entry and retrieval |
| FORM-002 | Password input field | P0 | Masked input |
| FORM-003 | Email input with validation | P0 | Email format validated |
| FORM-004 | Number input with min/max | P0 | Numeric constraints enforced |
| FORM-005 | Date input picker | P0 | Date selection works |
| FORM-006 | Checkbox input | P0 | Toggle state |
| FORM-007 | Radio button groups | P0 | Exclusive selection |
| FORM-008 | Select dropdown | P0 | Options selectable |
| FORM-009 | Textarea multi-line input | P0 | Line breaks preserved |
| FORM-010 | File upload input | P0 | File selection dialog |
| FORM-011 | Range slider input | P0 | Value changes on drag |
| FORM-012 | Color picker input | P0 | Color selection |

### 3.2 Form Submission (P0) - 10 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FSUB-001 | Form submission via GET method | P0 | Query string parameters |
| FSUB-002 | Form submission via POST method | P0 | Request body contains form data |
| FSUB-003 | Form submission with multipart encoding | P0 | File upload works |
| FSUB-004 | Form submission with JavaScript preventDefault | P0 | Default action blocked |
| FSUB-005 | Form submit button click | P0 | Form submits |
| FSUB-006 | Form submission via Enter key | P0 | Keyboard submission |
| FSUB-007 | Form reset button | P0 | Fields cleared |
| FSUB-008 | FormData API usage | P0 | Programmatic form submission |
| FSUB-009 | Form submission to same-origin action | P0 | No warnings |
| FSUB-010 | Form submission redirect handling | P0 | Follow POST-redirect-GET |

### 3.3 Form Validation (P0) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FVAL-001 | Required field validation | P0 | Error on empty required field |
| FVAL-002 | Email pattern validation | P0 | Invalid email rejected |
| FVAL-003 | URL pattern validation | P0 | Invalid URL rejected |
| FVAL-004 | Min/max length validation | P0 | Length constraints enforced |
| FVAL-005 | Min/max value validation (number) | P0 | Numeric range enforced |
| FVAL-006 | Custom pattern validation (regex) | P0 | Pattern matched |
| FVAL-007 | Custom validation via setCustomValidity | P0 | Custom error message shown |
| FVAL-008 | Form validation messages display | P0 | Error messages visible |

### 3.4 FormMonitor - Credential Protection (P0) - FORK FEATURE

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FMON-001 | Detect cross-origin password submission | P0 | Alert shown before submission |
| FMON-002 | Allow same-origin password submission | P0 | No alert, submission proceeds |
| FMON-003 | User trusts credential relationship | P0 | PolicyGraph stores trust, no future alerts |
| FMON-004 | User blocks credential relationship | P0 | PolicyGraph stores block, submission prevented |
| FMON-005 | Detect insecure (HTTP) credential submission | P0 | Alert warns about unencrypted transmission |
| FMON-006 | Autofill protection - block cross-origin | P0 | Autofill disabled for untrusted cross-origin |
| FMON-007 | Autofill override mechanism | P0 | User grants one-time autofill permission |
| FMON-008 | Load trusted relationships from database | P0 | Previous trust decisions persist |
| FMON-009 | Form anomaly detection - excessive hidden fields | P0 | Alert on suspicious hidden field ratio |
| FMON-010 | Form anomaly detection - rapid submissions | P0 | Alert on unusual submission frequency |
| FMON-011 | Third-party tracking domain detection | P0 | Warn on submission to known trackers |
| FMON-012 | Credential exfiltration simulation test | P0 | Test page triggers all FormMonitor alerts |

---

## 4. Security Features (P0) - FORK SPECIFIC

### 4.1 Sentinel Malware Detection (P0)

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| MAL-001 | Download executable file - clean | P0 | YARA scan passes, download proceeds |
| MAL-002 | Download executable file - malicious | P0 | YARA scan detects, quarantine alert shown |
| MAL-003 | Download PDF - embedded malware | P0 | YARA rule matches, file quarantined |
| MAL-004 | Download ZIP archive - malware inside | P0 | Archive scanned, threat detected |
| MAL-005 | User chooses to quarantine file | P0 | File moved to quarantine directory |
| MAL-006 | User chooses to allow risky download | P0 | PolicyGraph stores exception |
| MAL-007 | ML-based malware detection (TensorFlow Lite) | P0 | ML model scores file, high-risk flagged |
| MAL-008 | YARA rule update and reload | P0 | New rules loaded without restart |
| MAL-009 | Large file download performance | P0 | Scan completes in reasonable time |
| MAL-010 | Multiple concurrent downloads scanning | P0 | Parallel scans don't crash |

### 4.2 Sentinel Phishing Detection (P0)

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| PHISH-001 | Navigate to legitimate domain | P0 | No phishing alert |
| PHISH-002 | Navigate to homograph attack domain | P0 | Unicode lookalike detected, alert shown |
| PHISH-003 | Navigate to typosquatting domain | P0 | Similar to popular domain, alert shown |
| PHISH-004 | Navigate to suspicious TLD (.tk, .ml) | P0 | Free TLD flagged, warning shown |
| PHISH-005 | Navigate to high-entropy random domain | P0 | Gibberish domain detected |
| PHISH-006 | User trusts phishing alert domain | P0 | PolicyGraph stores trust, no future alerts |
| PHISH-007 | User blocks phishing domain | P0 | Navigation blocked, added to blocklist |
| PHISH-008 | Levenshtein distance typo detection | P0 | goggle.com flagged as google.com typo |
| PHISH-009 | Skeleton normalization for homographs | P0 | pаypal.com (Cyrillic 'a') detected |
| PHISH-010 | Combined phishing score (multiple indicators) | P0 | Aggregate score above threshold triggers alert |

### 4.3 Sentinel Fingerprinting Detection (P0)

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FP-001 | Canvas fingerprinting - toDataURL() | P0 | API call recorded, score updated |
| FP-002 | Canvas fingerprinting - getImageData() | P0 | API call recorded, score updated |
| FP-003 | WebGL fingerprinting - getParameter() | P0 | API call recorded, score updated |
| FP-004 | Audio fingerprinting - OscillatorNode | P0 | API call recorded, score updated |
| FP-005 | Navigator enumeration fingerprinting | P0 | Excessive property access detected |
| FP-006 | Font enumeration fingerprinting | P0 | Font probing detected |
| FP-007 | Screen properties fingerprinting | P0 | Screen API access recorded |
| FP-008 | Aggressive fingerprinting (multiple techniques) | P0 | Combined score > 0.6, alert shown |
| FP-009 | Fingerprinting before user interaction | P0 | Flag set, increases suspicion score |
| FP-010 | Rapid-fire fingerprinting (5+ calls in 1s) | P0 | Timing analysis detects aggressive behavior |
| FP-011 | Fingerprinting on legitimate site (low score) | P0 | Normal canvas use doesn't trigger false positive |
| FP-012 | Test against browserleaks.com/canvas | P0 | Real-world fingerprinting site detected |

### 4.4 PolicyGraph Integration (P0)

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| PG-001 | Create security policy (trust decision) | P0 | Policy persisted to SQLite database |
| PG-002 | Retrieve existing policy | P0 | Cached decision used, no re-prompt |
| PG-003 | Update existing policy | P0 | Policy modified, changes saved |
| PG-004 | Delete security policy | P0 | Policy removed from database |
| PG-005 | Export policies (JSON/SQLite) | P0 | File contains all policies |
| PG-006 | Import policies | P0 | Policies loaded into database |
| PG-007 | Policy template application | P0 | Template creates multiple policies |
| PG-008 | SQL injection prevention | P0 | Prepared statements prevent injection |
| PG-009 | Concurrent access handling | P0 | Database locks prevent corruption |
| PG-010 | Policy audit log | P0 | All policy changes logged |

---

## 5. Multimedia (P1)

### 5.1 HTML5 Video (P1) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| VID-001 | Load and play MP4 video | P1 | Video plays, controls work |
| VID-002 | Load and play WebM video | P1 | Modern codec support |
| VID-003 | Video playback controls (play/pause/seek) | P1 | User controls functional |
| VID-004 | Video autoplay attribute | P1 | Autoplay policy respected |
| VID-005 | Video poster frame | P1 | Thumbnail shows before play |
| VID-006 | Video fullscreen mode | P1 | Fullscreen API works |
| VID-007 | Video playback speed control | P1 | Playback rate changes |
| VID-008 | Video subtitles/captions (WebVTT) | P1 | Text tracks display |

### 5.2 HTML5 Audio (P1) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| AUD-001 | Load and play MP3 audio | P1 | Audio plays |
| AUD-002 | Load and play Ogg Vorbis audio | P1 | Alternative codec support |
| AUD-003 | Audio playback controls | P1 | Play/pause/volume work |
| AUD-004 | Audio autoplay restrictions | P1 | User interaction required |
| AUD-005 | Web Audio API - AudioContext | P1 | Programmatic audio generation |

### 5.3 Canvas 2D (P1) - 6 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| CVS-001 | Draw shapes (rectangles, circles, paths) | P1 | Shapes render correctly |
| CVS-002 | Fill and stroke styles | P1 | Colors and patterns apply |
| CVS-003 | Draw text on canvas | P1 | Text renders with font styling |
| CVS-004 | Draw images on canvas | P1 | Image compositing works |
| CVS-005 | Canvas transformations (translate, rotate, scale) | P1 | Transform matrix applies |
| CVS-006 | Canvas export to image (toDataURL) | P1 | Image data URL generated |

---

## 6. Network & Downloads (P1)

### 6.1 HTTP/HTTPS Requests (P1) - 10 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| NET-001 | HTTP GET request | P1 | Resource fetched |
| NET-002 | HTTP POST request | P1 | Request body sent |
| NET-003 | HTTPS request with valid certificate | P1 | TLS handshake succeeds |
| NET-004 | HTTPS request with self-signed certificate | P1 | Certificate warning shown |
| NET-005 | HTTPS request with expired certificate | P1 | Error page displayed |
| NET-006 | Request with custom headers | P1 | Headers sent to server |
| NET-007 | Request with cookies | P1 | Cookies sent and received |
| NET-008 | CORS preflight request | P1 | OPTIONS request sent, CORS headers checked |
| NET-009 | Request compression (gzip, brotli) | P1 | Content decompressed |
| NET-010 | HTTP/2 request | P1 | Protocol upgrade if supported |

### 6.2 Resource Loading (P1) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| RES-001 | Load CSS from external file | P1 | Stylesheet fetched and applied |
| RES-002 | Load JavaScript from external file | P1 | Script executed |
| RES-003 | Load images from CDN | P1 | Cross-origin images load |
| RES-004 | Parallel resource loading | P1 | Multiple requests in flight |
| RES-005 | Resource caching (Cache-Control) | P1 | Cached resources reused |
| RES-006 | Cache validation (If-Modified-Since) | P1 | Conditional request sent |
| RES-007 | Resource preloading (link rel=preload) | P1 | Resources fetched early |
| RES-008 | Lazy loading with Intersection Observer | P1 | Resources load when visible |

### 6.3 Download Handling (P1) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| DL-001 | Download file via link click | P1 | Download starts, file saved |
| DL-002 | Download file via JavaScript (blob URL) | P1 | Programmatic download works |
| DL-003 | Download with Content-Disposition header | P1 | Filename from header used |
| DL-004 | Download pause and resume | P1 | Partial download resumed |
| DL-005 | Download progress tracking | P1 | Progress bar updates |
| DL-006 | Multiple simultaneous downloads | P1 | All downloads proceed |
| DL-007 | Download to custom location | P1 | User-selected path used |
| DL-008 | Download cancellation | P1 | Download stopped, partial file deleted |

### 6.4 Error Handling (P1) - 6 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| ERR-001 | 404 Not Found error page | P1 | Custom error page or browser default |
| ERR-002 | 500 Internal Server Error page | P1 | Error displayed |
| ERR-003 | Network timeout (slow server) | P1 | Timeout error after threshold |
| ERR-004 | DNS resolution failure | P1 | DNS error page shown |
| ERR-005 | Connection refused | P1 | Connection error message |
| ERR-006 | Mixed content blocking (HTTPS page loads HTTP) | P1 | Insecure content blocked or warned |

---

## 7. Accessibility (P1)

### 7.1 ARIA Attributes (P1) - 8 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| A11Y-001 | ARIA roles (button, navigation, main) | P1 | Roles exposed to assistive tech |
| A11Y-002 | ARIA labels (aria-label, aria-labelledby) | P1 | Labels accessible |
| A11Y-003 | ARIA descriptions (aria-describedby) | P1 | Descriptions exposed |
| A11Y-004 | ARIA live regions (polite, assertive) | P1 | Dynamic content announced |
| A11Y-005 | ARIA states (aria-expanded, aria-checked) | P1 | State changes announced |
| A11Y-006 | ARIA properties (aria-hidden) | P1 | Hidden elements excluded |
| A11Y-007 | Complex widgets (combobox, tree, tab panel) | P1 | Widget patterns accessible |
| A11Y-008 | Form field ARIA validation messages | P1 | Error messages associated |

### 7.2 Keyboard Navigation (P1) - 7 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| KBD-001 | Tab key navigation through links | P1 | Focus moves sequentially |
| KBD-002 | Shift+Tab reverse navigation | P1 | Focus moves backward |
| KBD-003 | Enter key activates links/buttons | P1 | Keyboard activation works |
| KBD-004 | Space key activates buttons | P1 | Button pressed via keyboard |
| KBD-005 | Arrow key navigation in select/radio | P1 | Options selectable via arrow keys |
| KBD-006 | Escape key closes modals/dialogs | P1 | Dialog dismissed |
| KBD-007 | Skip to main content link | P1 | Bypass navigation landmarks |

### 7.3 Focus Management (P1) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| FOC-001 | Visible focus indicator on all interactive elements | P1 | Outline or custom focus style visible |
| FOC-002 | Focus trap in modal dialogs | P1 | Focus contained within modal |
| FOC-003 | Focus restoration after modal close | P1 | Focus returns to trigger element |
| FOC-004 | Focus order matches DOM order | P1 | Logical tab sequence |
| FOC-005 | Programmatic focus (focus() API) | P1 | JavaScript can set focus |

---

## 8. Performance (P2)

### 8.1 Page Load Metrics (P2) - 6 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| PERF-001 | Time to First Byte (TTFB) | P2 | Server response < 500ms |
| PERF-002 | First Contentful Paint (FCP) | P2 | Content visible < 1.5s |
| PERF-003 | Largest Contentful Paint (LCP) | P2 | Main content visible < 2.5s |
| PERF-004 | Time to Interactive (TTI) | P2 | Page interactive < 3.5s |
| PERF-005 | Cumulative Layout Shift (CLS) | P2 | Layout stable (CLS < 0.1) |
| PERF-006 | First Input Delay (FID) | P2 | Input response < 100ms |

### 8.2 Resource Loading Performance (P2) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| RPERF-001 | Parallel CSS/JS loading | P2 | Resources fetched concurrently |
| RPERF-002 | Image lazy loading reduces initial load | P2 | Below-fold images deferred |
| RPERF-003 | Cache hit rate for repeat visits | P2 | Cached resources reused |
| RPERF-004 | Resource prioritization (critical CSS) | P2 | Critical resources loaded first |
| RPERF-005 | Service worker caching strategy | P2 | Offline-first caching works |

### 8.3 Memory & Rendering Performance (P2) - 4 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| MEM-001 | Memory leak detection (heap growth) | P2 | Memory stable over time |
| MEM-002 | Large DOM tree handling (10k+ nodes) | P2 | Rendering doesn't freeze |
| MEM-003 | Animation frame rate (60fps) | P2 | Smooth animations |
| MEM-004 | Layout thrashing prevention | P2 | Read/write batching optimizes reflow |

---

## 9. Edge Cases & Error Conditions (P2)

### 9.1 Large Pages (P2) - 4 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| EDGE-001 | Page with 10,000+ DOM elements | P2 | Page loads, scrolling smooth |
| EDGE-002 | Page with 1MB+ of inline JavaScript | P2 | Script parses and executes |
| EDGE-003 | Page with 100+ images | P2 | All images load, lazy loading works |
| EDGE-004 | Deeply nested DOM tree (50+ levels) | P2 | Rendering doesn't stack overflow |

### 9.2 Concurrent Operations (P2) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| CONC-001 | 20+ tabs open simultaneously | P2 | All tabs responsive |
| CONC-002 | Rapid tab switching (stress test) | P2 | No crashes, UI responsive |
| CONC-003 | Multiple forms submitting concurrently | P2 | All submissions processed |
| CONC-004 | Parallel XHR/fetch requests (50+) | P2 | Requests complete, connection pooling |
| CONC-005 | Multiple videos playing simultaneously | P2 | Playback smooth across tabs |

### 9.3 Network Failures (P2) - 4 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| NETF-001 | Request timeout (server hangs) | P2 | Timeout error after threshold |
| NETF-002 | Connection drop mid-download | P2 | Error message or resume capability |
| NETF-003 | Offline mode (no network) | P2 | Offline error page shown |
| NETF-004 | Network reconnection handling | P2 | Retry logic works |

### 9.4 Invalid Input (P2) - 5 tests

| Test ID | Test Scenario | Priority | Validation Points |
|---------|--------------|----------|-------------------|
| INV-001 | Malformed HTML (unclosed tags) | P2 | Error recovery, page renders |
| INV-002 | Broken CSS (syntax errors) | P2 | Parser ignores invalid rules |
| INV-003 | JavaScript syntax errors | P2 | Error logged, script doesn't execute |
| INV-004 | Invalid UTF-8 encoding | P2 | Replacement characters shown |
| INV-005 | Circular redirects (redirect loop) | P2 | Loop detected, error shown |

---

## 10. Test Matrix Summary Table

| Feature Category | P0 Tests | P1 Tests | P2 Tests | Total | Estimated Hours |
|------------------|----------|----------|----------|-------|----------------|
| **Core Browser Functionality** | 45 | 20 | 0 | **65** | 50 |
| Navigation | 15 | 0 | 0 | 15 | 10 |
| Tab Management | 12 | 0 | 0 | 12 | 8 |
| History | 8 | 0 | 0 | 8 | 5 |
| Bookmarks | 0 | 10 | 0 | 10 | 7 |
| Settings | 0 | 10 | 0 | 10 | 8 |
| **Page Rendering** | 55 | 5 | 0 | **60** | 45 |
| HTML | 12 | 0 | 0 | 12 | 8 |
| CSS | 18 | 0 | 0 | 18 | 12 |
| JavaScript | 15 | 0 | 0 | 15 | 10 |
| Fonts | 0 | 5 | 0 | 5 | 3 |
| Images | 10 | 0 | 0 | 10 | 7 |
| **Form Handling** | 42 | 0 | 0 | **42** | 40 |
| Input Types | 12 | 0 | 0 | 12 | 8 |
| Submission | 10 | 0 | 0 | 10 | 7 |
| Validation | 8 | 0 | 0 | 8 | 5 |
| FormMonitor (FORK) | 12 | 0 | 0 | 12 | 20 |
| **Security Features (FORK)** | 40 | 0 | 0 | **40** | 60 |
| Malware Detection | 10 | 0 | 0 | 10 | 15 |
| Phishing Detection | 10 | 0 | 0 | 10 | 15 |
| Fingerprinting Detection | 12 | 0 | 0 | 12 | 20 |
| PolicyGraph | 10 | 0 | 0 | 10 | 10 |
| **Multimedia** | 0 | 19 | 0 | **19** | 15 |
| Video | 0 | 8 | 0 | 8 | 6 |
| Audio | 0 | 5 | 0 | 5 | 3 |
| Canvas | 0 | 6 | 0 | 6 | 5 |
| **Network & Downloads** | 0 | 32 | 0 | **32** | 25 |
| HTTP/HTTPS | 0 | 10 | 0 | 10 | 8 |
| Resource Loading | 0 | 8 | 0 | 8 | 6 |
| Downloads | 0 | 8 | 0 | 8 | 6 |
| Error Handling | 0 | 6 | 0 | 6 | 4 |
| **Accessibility** | 0 | 20 | 0 | **20** | 18 |
| ARIA | 0 | 8 | 0 | 8 | 6 |
| Keyboard Navigation | 0 | 7 | 0 | 7 | 5 |
| Focus Management | 0 | 5 | 0 | 5 | 4 |
| **Performance** | 0 | 0 | 15 | **15** | 12 |
| Page Load Metrics | 0 | 0 | 6 | 6 | 4 |
| Resource Performance | 0 | 0 | 5 | 5 | 3 |
| Memory & Rendering | 0 | 0 | 4 | 4 | 3 |
| **Edge Cases** | 0 | 0 | 18 | **18** | 15 |
| Large Pages | 0 | 0 | 4 | 4 | 3 |
| Concurrent Operations | 0 | 0 | 5 | 5 | 4 |
| Network Failures | 0 | 0 | 4 | 4 | 3 |
| Invalid Input | 0 | 0 | 5 | 5 | 3 |
| **TOTAL** | **182** | **96** | **33** | **311** | **280** |

**Priority Breakdown**:
- **P0 Critical**: 182 tests (58.5%) - Core browser + Security features
- **P1 Important**: 96 tests (30.9%) - Enhanced features + Multimedia
- **P2 Nice-to-have**: 33 tests (10.6%) - Performance + Edge cases

---

## 11. Implementation Roadmap

### Phase 1: P0 Critical Tests (182 tests) - Weeks 1-8

**Week 1-2: Core Browser (45 tests)**
- Navigation (NAV-001 to NAV-015): 15 tests
- Tab Management (TAB-001 to TAB-012): 12 tests
- History (HIST-001 to HIST-008): 8 tests

**Week 3-4: Page Rendering (55 tests)**
- HTML Rendering (HTML-001 to HTML-012): 12 tests
- CSS Layout (CSS-001 to CSS-018): 18 tests
- JavaScript Execution (JS-001 to JS-015): 15 tests
- Images (IMG-001 to IMG-010): 10 tests

**Week 5-6: Form Handling (42 tests)**
- Input Types (FORM-001 to FORM-012): 12 tests
- Submission (FSUB-001 to FSUB-010): 10 tests
- Validation (FVAL-001 to FVAL-008): 8 tests
- FormMonitor (FMON-001 to FMON-012): 12 tests

**Week 7-8: Security Features - FORK (40 tests)**
- Malware Detection (MAL-001 to MAL-010): 10 tests
- Phishing Detection (PHISH-001 to PHISH-010): 10 tests
- Fingerprinting Detection (FP-001 to FP-012): 12 tests
- PolicyGraph (PG-001 to PG-010): 10 tests

**Phase 1 Deliverable**: 182 critical tests covering core browser and all Sentinel security features

---

### Phase 2: P1 Important Tests (96 tests) - Weeks 9-13

**Week 9-10: Enhanced Browser Features (30 tests)**
- Bookmarks (BOOK-001 to BOOK-010): 10 tests
- Settings (SET-001 to SET-010): 10 tests
- Fonts (FONT-001 to FONT-005): 5 tests

**Week 11: Multimedia (19 tests)**
- Video (VID-001 to VID-008): 8 tests
- Audio (AUD-001 to AUD-005): 5 tests
- Canvas (CVS-001 to CVS-006): 6 tests

**Week 12: Network & Downloads (32 tests)**
- HTTP/HTTPS (NET-001 to NET-010): 10 tests
- Resource Loading (RES-001 to RES-008): 8 tests
- Downloads (DL-001 to DL-008): 8 tests
- Error Handling (ERR-001 to ERR-006): 6 tests

**Week 13: Accessibility (20 tests)**
- ARIA (A11Y-001 to A11Y-008): 8 tests
- Keyboard Navigation (KBD-001 to KBD-007): 7 tests
- Focus Management (FOC-001 to FOC-005): 5 tests

**Phase 2 Deliverable**: 96 important tests covering enhanced features and accessibility

---

### Phase 3: P2 Nice-to-have Tests (33 tests) - Weeks 14-16

**Week 14: Performance (15 tests)**
- Page Load Metrics (PERF-001 to PERF-006): 6 tests
- Resource Performance (RPERF-001 to RPERF-005): 5 tests
- Memory & Rendering (MEM-001 to MEM-004): 4 tests

**Week 15-16: Edge Cases (18 tests)**
- Large Pages (EDGE-001 to EDGE-004): 4 tests
- Concurrent Operations (CONC-001 to CONC-005): 5 tests
- Network Failures (NETF-001 to NETF-004): 4 tests
- Invalid Input (INV-001 to INV-005): 5 tests

**Phase 3 Deliverable**: 33 edge case and performance tests

---

### Total Implementation Timeline

- **Phase 1 (P0)**: 8 weeks - 182 tests
- **Phase 2 (P1)**: 5 weeks - 96 tests
- **Phase 3 (P2)**: 3 weeks - 33 tests
- **Total**: 16 weeks - 311 tests

**Estimated Effort**: 280 hours total (~17.5 hours per week over 16 weeks)

---

## 12. Test Implementation Strategy

### 12.1 Playwright Test Structure

```javascript
// tests/core-browser/navigation.spec.ts
import { test, expect } from '@playwright/test';

test.describe('Navigation', () => {
  test('NAV-001: URL bar navigation to HTTP site', async ({ page }) => {
    await page.goto('http://example.com');
    await expect(page).toHaveTitle(/Example Domain/);
    await expect(page).toHaveURL('http://example.com/');
  });

  test('NAV-002: URL bar navigation to HTTPS site', async ({ page }) => {
    await page.goto('https://example.com');
    // Verify secure connection indicator
    await expect(page).toHaveURL(/^https:\/\//);
    await expect(page).toHaveTitle(/Example Domain/);
  });
});
```

### 12.2 Sentinel Security Test Structure

```javascript
// tests/security/fingerprinting.spec.ts
import { test, expect } from '@playwright/test';

test.describe('Fingerprinting Detection', () => {
  test('FP-001: Canvas fingerprinting - toDataURL()', async ({ page }) => {
    // Navigate to test page with canvas fingerprinting
    await page.goto('file:///home/rbsmith4/ladybird/test_canvas_fingerprinting.html');

    // Execute canvas fingerprinting
    await page.evaluate(() => {
      const canvas = document.createElement('canvas');
      canvas.width = 200;
      canvas.height = 50;
      const ctx = canvas.getContext('2d');
      ctx.fillText('Fingerprint test', 10, 25);
      const dataURL = canvas.toDataURL(); // Triggers fingerprinting detection
    });

    // Check if fingerprinting was detected (via console logs or alert)
    const logs = await page.evaluate(() => {
      return window.__fingerprinting_detected; // Test page sets this flag
    });

    expect(logs).toBeTruthy();
  });
});
```

### 12.3 FormMonitor Test Structure

```javascript
// tests/security/form-monitor.spec.ts
import { test, expect } from '@playwright/test';

test.describe('FormMonitor - Credential Protection', () => {
  test('FMON-001: Detect cross-origin password submission', async ({ page }) => {
    // Listen for security alerts
    page.on('dialog', async dialog => {
      expect(dialog.message()).toContain('cross-origin');
      await dialog.accept();
    });

    // Navigate to test page with cross-origin form
    await page.goto('http://example.com/login-form.html');

    // Fill in password field
    await page.fill('input[type="password"]', 'test-password');
    await page.fill('input[type="email"]', 'test@example.com');

    // Submit form to different origin
    await page.click('button[type="submit"]');

    // Alert should have been shown
    // Dialog handler above verifies this
  });
});
```

### 12.4 Test Fixtures and Utilities

```javascript
// tests/fixtures/ladybird-fixtures.ts
import { test as base } from '@playwright/test';
import { PolicyGraph } from './policy-graph-helper';
import { SentinelClient } from './sentinel-client';

export const test = base.extend({
  // Custom fixture for PolicyGraph interaction
  policyGraph: async ({}, use) => {
    const pg = new PolicyGraph('/tmp/test-policy.db');
    await pg.initialize();
    await use(pg);
    await pg.cleanup();
  },

  // Custom fixture for Sentinel service
  sentinel: async ({}, use) => {
    const client = new SentinelClient('/tmp/sentinel.sock');
    await client.connect();
    await use(client);
    await client.disconnect();
  },
});
```

### 12.5 Test Data Management

**Test Pages**: Create test HTML pages in `/Tests/Playwright/test-pages/`

```html
<!-- test-pages/fingerprinting/canvas-aggressive.html -->
<!DOCTYPE html>
<html>
<head><title>Canvas Fingerprinting Test</title></head>
<body>
  <script>
    // Aggressive canvas fingerprinting
    const canvas = document.createElement('canvas');
    canvas.width = 200;
    canvas.height = 50;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#f60';
    ctx.fillRect(125, 1, 62, 20);
    ctx.fillStyle = '#069';
    ctx.font = '11pt Arial';
    ctx.fillText('Test String', 2, 15);
    const dataURL = canvas.toDataURL();
    const imageData = ctx.getImageData(0, 0, 200, 50);

    // Flag for test verification
    window.__fingerprinting_detected = true;
  </script>
</body>
</html>
```

### 12.6 CI/CD Integration

```yaml
# .github/workflows/playwright-tests.yml
name: Playwright E2E Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: 18

      # Build Ladybird
      - name: Build Ladybird
        run: |
          cmake --preset Release
          cmake --build Build/release

      # Start Sentinel service
      - name: Start Sentinel
        run: |
          ./Build/release/bin/Sentinel &
          sleep 2

      # Install Playwright
      - name: Install dependencies
        run: npm ci
        working-directory: Tests/Playwright

      # Run tests by priority
      - name: Run P0 tests
        run: npm run test:p0
        working-directory: Tests/Playwright

      - name: Run P1 tests
        run: npm run test:p1
        working-directory: Tests/Playwright

      # Upload results
      - uses: actions/upload-artifact@v3
        if: always()
        with:
          name: playwright-report
          path: Tests/Playwright/playwright-report/
```

---

## 13. Success Metrics

### 13.1 Coverage Targets

- **P0 Critical Tests**: 100% pass rate required for release
- **P1 Important Tests**: 95% pass rate target
- **P2 Nice-to-have Tests**: 80% pass rate acceptable

### 13.2 Test Stability

- **Flaky Test Threshold**: < 2% of tests may be flaky
- **Test Execution Time**: Full suite completes in < 30 minutes
- **Parallelization**: Tests run in parallel across 4+ workers

### 13.3 Maintenance

- **Test Review Cycle**: Monthly review of test failures
- **Test Updates**: Tests updated when features change
- **Deprecation**: Remove obsolete tests after feature removal

---

## 14. Test Prioritization for Initial MVP

**Absolute Must-Have for First Release** (MVP - 80 tests):

1. **Core Navigation** (15 tests): NAV-001 to NAV-015
2. **Tab Management** (12 tests): TAB-001 to TAB-012
3. **Basic HTML/CSS** (20 tests): HTML-001 to HTML-012, CSS-001 to CSS-008
4. **JavaScript Execution** (10 tests): JS-001 to JS-010
5. **Form Handling** (15 tests): FORM-001 to FORM-012, FSUB-001 to FSUB-003
6. **Sentinel Security** (20 tests):
   - FormMonitor: FMON-001 to FMON-006 (6 tests)
   - Malware: MAL-001 to MAL-005 (5 tests)
   - Phishing: PHISH-001 to PHISH-005 (5 tests)
   - Fingerprinting: FP-001 to FP-008 (4 tests)

---

## 15. Notes and Assumptions

1. **Multi-Process Architecture**: Tests must account for Ladybird's multi-process design (UI, WebContent, RequestServer)
2. **Sentinel IPC**: Security feature tests require Sentinel service running and Unix socket communication
3. **PolicyGraph Database**: Tests must manage test database lifecycle (create, populate, cleanup)
4. **Real-World Sites**: Some tests (FP-012, phishing tests) reference real fingerprinting/phishing test sites
5. **Test Isolation**: Each test should be independent and not rely on previous test state
6. **Performance Tests**: P2 performance tests may require dedicated hardware or longer timeouts
7. **Accessibility Tests**: May require screen reader simulation or axe-core integration

---

## 16. Related Documentation

- **Playwright Documentation**: https://playwright.dev/
- **Ladybird Architecture**: `/home/rbsmith4/ladybird/Documentation/ProcessArchitecture.md`
- **Sentinel Architecture**: `/home/rbsmith4/ladybird/docs/SENTINEL_ARCHITECTURE.md`
- **FormMonitor Guide**: `/home/rbsmith4/ladybird/docs/USER_GUIDE_CREDENTIAL_PROTECTION.md`
- **Fingerprinting Detection**: `/home/rbsmith4/ladybird/docs/FINGERPRINTING_DETECTION_ARCHITECTURE.md`
- **Phishing Detection**: `/home/rbsmith4/ladybird/docs/PHISHING_DETECTION_ARCHITECTURE.md`

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-01 | rbsmith4 | Initial test matrix creation |

---

**End of Test Matrix**
