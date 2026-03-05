# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About This Fork

This is a **personal learning fork** of Ladybird Browser with experimental privacy and security features. Not production-ready or intended for upstream contribution.

Fork-specific features:
- **Sentinel Service** (`Services/Sentinel/`): Multi-layered security system (YARA malware detection, ML via TensorFlow Lite, credential exfiltration detection, fingerprinting detection, phishing URL analysis, PolicyGraph database)
- **Network Privacy**: Tor integration, IPFS/IPNS support, ENS resolution
- **Enhanced IPC Security**: Rate limiting, SafeMath, ValidatedDecoder (experimental)

## Commands

### Build and Run
```bash
./Meta/ladybird.py build              # Build all targets
./Meta/ladybird.py run                # Build and run Ladybird
./Meta/ladybird.py test               # Run all tests
./Meta/ladybird.py test LibWeb        # Run LibWeb tests only
./Meta/ladybird.py test ".*CSS.*"     # Run tests matching regex

# Direct CMake alternative
cmake --preset Release && cmake --build Build/release
BUILD_PRESET=Debug ./Meta/ladybird.py run   # Use Debug preset
```

### Testing
```bash
# LibWeb test runner (Text, Layout, Ref, Screenshot tests)
./Meta/ladybird.py run test-web
./Meta/ladybird.py run test-web -- -f Text/input/your-test.html        # Single test
./Meta/ladybird.py run test-web -- --rebaseline -f Text/input/test.html # Regenerate expectations

# Create new LibWeb tests
./Tests/LibWeb/add_libweb_test.py my-test Text      # Text|Layout|Ref|Screenshot

# CTest directly
cd Build/release && ctest -R LibWeb
CTEST_OUTPUT_ON_FAILURE=1 ctest   # Show output on failure

# Sanitizer build (reproduces CI failures)
cmake --preset Sanitizer && cmake --build --preset Sanitizer && ctest --preset Sanitizer

# Web Platform Tests
./Meta/WPT.sh run --log results.log css
./Meta/WPT.sh compare --log results.log expectations.log css
./Meta/WPT.sh import html/dom/aria-attribute-reflection.html  # Import WPT test
```

### Code Quality
```bash
ninja -C Build/release check-style        # clang-format check
ninja -C Build/release lint-shell-scripts  # Shell script linting
```

### Debugging
```bash
./Meta/ladybird.py debug ladybird                    # GDB/LLDB
./Meta/ladybird.py debug ladybird --debugger=lldb    # Specific debugger
./Meta/ladybird.py profile ladybird                  # callgrind profiling
WEBCONTENT_DEBUG=1 LIBWEB_DEBUG=1 ./Build/release/bin/Ladybird  # Debug logging
```

## Architecture

### Multi-Process Design

```
UI Process (Browser chrome: UI/Qt/ or UI/AppKit/)
├── WebContent Process (per tab, sandboxed) — LibWeb + LibJS rendering
├── RequestServer Process (per WebContent) — network isolation
├── ImageDecoder Process (per image, sandboxed)
├── WebDriver Process (browser automation)
├── WebWorker Process (Web Workers)
└── Sentinel Service (standalone daemon) [Fork]
```

All entry points use `ladybird_main(Main::Arguments)` instead of standard `main()`.

### IPC Communication

`.ipc` files define message interfaces; the IPC compiler generates endpoint code. Message types: synchronous (`=>`) and asynchronous (`=|`).

```cpp
endpoint RequestServer {
    start_request(i32 request_id, ByteString method, URL::URL url, ...) =|
    stop_request(i32 request_id) => (bool success)
}
```

Key IPC files: `Services/*/{}Client,Server}.ipc`, `Libraries/LibWebView/UIProcess{Client,Server}.ipc`

### Core Libraries (`Libraries/`)

- **LibWeb/** — HTML/CSS/DOM rendering engine. Subdirectories map to specs (e.g., `XHR/` → `Web::XHR` namespace). 75+ subdirectories.
- **LibJS/** — JavaScript engine (ECMAScript). Uses completions for error propagation.
- **LibWasm/** — WebAssembly support
- **AK/** (root dir) — Application Kit: containers (Vector, String, HashMap), error handling (ErrorOr/TRY/MUST), smart pointers
- **LibCore/** — Event loop, file I/O, OS abstraction
- **LibIPC/** — Inter-process communication framework
- **LibGfx/** — 2D graphics, image decoding, font rendering
- **LibWebView/** — Bridge between UI process and WebContent

### GC-Managed Objects

LibWeb objects participating in garbage collection must:
1. Use `GC_CELL(ClassName, BaseClass)` macro in the class declaration
2. Override `visit_edges(Cell::Visitor&)` to trace GC references
3. Use `GC::Ref<T>` / `GC::Ptr<T>` for pointers to other GC objects
4. Use static factory methods returning `GC::Ref<T>` instead of public constructors

### LibWeb Error Handling Hierarchy

- `AK::ErrorOr<T>` — Only for OOM propagation. Use `TRY_OR_THROW_OOM` to convert to JS error.
- `WebIDL::ExceptionOr<T>` — Primary error type for anything touching JS bindings. Wraps SimpleException, DOMException, or JS::Completion.
- `JS::ThrowCompletionOr<T>` — Only when overriding LibJS virtual methods. Wrap in ExceptionOr ASAP.

## Coding Style

Follow `Documentation/CodingStyle.md` and `.clang-format`. C++23 required (gcc-14 or clang-20).

### Naming
- **Classes/Structs/Namespaces**: CamelCase (`FileDescriptor`, `HTMLElement`)
- **Functions/Variables**: snake_case (`buffer_size`, `absolute_path()`)
- **Constants**: SCREAMING_CASE
- **Members**: `m_` prefix, **Static**: `s_` prefix, **Global**: `g_` prefix
- **Getters**: bare word (`count()`), **Setters**: `set_` prefix (`set_count()`)
- **Singletons**: static `the()` method

### Key Patterns
```cpp
// Error propagation (like Rust's ?)
ErrorOr<void> do_something() {
    auto result = TRY(fallible_operation());
    return {};
}

// MUST for infallible operations
MUST(vector.try_append(item));

// Zero-cost StringView literals
auto str = "Hello"sv;

// East const style (const on the right)
Salt const& m_salt;   // Right
const Salt& m_salt;   // Wrong

// Virtual overrides: always use BOTH 'virtual' and 'override'/'final'
virtual String description() override { ... }

// Fallible constructors
static ErrorOr<NonnullOwnPtr<MyClass>> create() { ... }
```

### Spec Implementation Rules
- **Match spec naming exactly** (e.g., `suffering_from_being_missing()` not `has_missing_constraint()`)
- **Add spec URL** above every function implementing a spec algorithm
- **Comment every numbered step** from the spec
- **Use `FIXME:`** for unimplemented steps, **`NOTE:`** for spec notes, **`NB:`** for our own notes
- **`OPTIMIZATION:`** prefix for non-spec fast paths
- Copy IDL verbatim (4-space indent), don't reorder functions or rename params

### Code Rules
- No C-style casts (exception: `(void)param;`), use `static_cast`/`reinterpret_cast`/`bit_cast`
- No `using namespace std;` in .cpp files — fully qualify `std::` names
- `#pragma once` for header guards
- `class` for types with methods (private members with `m_`), `struct` for POD (public, no prefix)
- Prefer enums over bool parameters at call sites: `AllowFooBar::Yes` not `true`
- Curly braces may be omitted only for single-line bodies; if any branch needs braces, all branches get them

### Adding New Web IDL Interfaces
1. Create `.idl` file in appropriate `LibWeb/` subdirectory
2. Add `#import` for referenced IDL types
3. Add `libweb_js_bindings()` call in `Libraries/LibWeb/idl_files.cmake`
4. Forward declare in `Libraries/LibWeb/Forward.h`
5. Non-Event/Element types: add to `is_platform_object()` in `Meta/Lagom/Tools/CodeGenerators/LibWeb/BindingsGenerator/IDLGenerators.cpp`

## Testing

### Test Types
1. **Text** — Test Web APIs via `println()` output comparison. Sync or async (use `done` callback).
2. **Layout** — Compare layout tree dumps. No JS needed.
3. **Ref** — Screenshot comparison against reference HTML via `<link rel="match" href="...">`.
4. **Screenshot** — Screenshot comparison against reference image. Avoid if Ref test suffices.
5. **C++ Unit Tests** — `Tests/Lib*/Test*.cpp` using LibTest framework.

Every LibWeb feature/bugfix should have a corresponding test.

### LADYBIRD_SOURCE_DIR
Some tests require `export LADYBIRD_SOURCE_DIR=${PWD}` pointing to the repo root.

## Commit Message Format

```
Category: Brief imperative description

Detailed explanation if needed.
```

Examples: `LibWeb: Add CSS grid support`, `RequestServer: Fix memory leak in connection handling`

Use imperative mood: "Add feature" not "Added feature".

## Fork-Specific Patterns

### Security Detection Pattern
1. Core detection logic in `Services/Sentinel/` (no LibWeb dependencies)
2. Integration in `Services/WebContent/PageClient` (owns detector instance)
3. Minimal hooks in `LibWeb/` (just call into PageClient)
4. All features must gracefully degrade if initialization fails
5. Unit tests in `Services/Sentinel/Test*.cpp`, browser tests in `Tests/LibWeb/Text/input/`

### PolicyGraph Integration
```cpp
// 1. Check cached policy → 2. Run detection → 3. Alert user via IPC → 4. Store decision in PolicyGraph
auto result = m_policy_graph->evaluate_policy(domain, resource);
```

## Key Documentation

- `Documentation/CodingStyle.md` — Complete style rules
- `Documentation/Testing.md` — Testing guide
- `Documentation/ProcessArchitecture.md` — Multi-process design
- `Documentation/LibWebPatterns.md` — LibWeb code patterns and error handling
- `Documentation/LibWebFromLoadingToPainting.md` — Rendering pipeline
- `Documentation/AddNewIDLFile.md` — Adding Web IDL files
- `Documentation/CSSProperties.md` — Adding new CSS properties
- `Documentation/BuildInstructionsLadybird.md` — Platform-specific build setup
