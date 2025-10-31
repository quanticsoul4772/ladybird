# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About This Fork

This is a **personal learning fork** of Ladybird Browser with experimental privacy and security features. Not production-ready or intended for upstream contribution.

Fork-specific features:
- **Sentinel Service** (`Services/Sentinel/`): YARA-based malware detection for downloads
- **Network Privacy**: Tor integration, IPFS/IPNS support, ENS resolution
- **Enhanced IPC Security**: Rate limiting, SafeMath, ValidatedDecoder (experimental)

## Commands

### Build and Run
```bash
# Primary build script (recommended)
./Meta/ladybird.py build              # Build all targets
./Meta/ladybird.py run                # Build and run Ladybird
./Meta/ladybird.py run test-web       # Run test-web runner
./Meta/ladybird.py test               # Run all tests
./Meta/ladybird.py test LibWeb        # Run LibWeb tests only

# Direct CMake (alternative)
cmake --preset Release                # Configure (or Debug, Sanitizers)
cmake --build Build/release           # Build
./Build/release/bin/Ladybird          # Run

# Environment variable for preset
BUILD_PRESET=Debug ./Meta/ladybird.py run
```

### Testing
```bash
# Run all tests
./Meta/ladybird.py test

# Run specific test suite
./Meta/ladybird.py test LibWeb
./Meta/ladybird.py test AK

# Run specific test pattern (regex)
./Meta/ladybird.py test ".*CSS.*"

# LibWeb test types (Text, Layout, Ref, Screenshot)
./Meta/ladybird.py run test-web
./Meta/ladybird.py run test-web -- -f Text/input/your-test.html

# Rebaseline tests (regenerate expectations)
./Meta/ladybird.py run test-web -- --rebaseline -f Text/input/your-test.html

# Web Platform Tests
./Meta/WPT.sh run --log results.log
./Meta/WPT.sh compare --log results.log expectations.log css

# Using CTest directly
cd Build/release && ctest
cd Build/release && ctest -R LibWeb  # Run tests matching pattern
CTEST_OUTPUT_ON_FAILURE=1 ctest      # Show output on failure
```

### Debugging
```bash
# GDB/LLDB debugging
./Meta/ladybird.py debug ladybird                    # Launch with default debugger
./Meta/ladybird.py debug ladybird --debugger=lldb    # Specify debugger
./Meta/ladybird.py debug ladybird -cmd "break main"  # Pass debugger commands

# Attach to running process (WebContent, RequestServer, etc.)
ps aux | grep WebContent   # Find PID
gdb -p <PID>

# Performance profiling
./Meta/ladybird.py profile ladybird  # Launch with callgrind
```

### Code Quality
```bash
# Format check (requires clang-format)
ninja -C Build/release check-style

# Shell script linting
ninja -C Build/release lint-shell-scripts
```

### Adding LibWeb Tests
```bash
# Helper script to create new tests
./Tests/LibWeb/add_libweb_test.py my-test Text      # Text test
./Tests/LibWeb/add_libweb_test.py my-test Layout    # Layout test
./Tests/LibWeb/add_libweb_test.py my-test Ref        # Ref test
./Tests/LibWeb/add_libweb_test.py my-test Screenshot # Screenshot test
```

## Code Architecture

### Multi-Process Design

Ladybird uses a **sandboxed multi-process architecture**:

```
UI Process (Browser)
├── WebContent Process (per tab, sandboxed)
│   ├── LibWeb (HTML/CSS rendering)
│   └── LibJS (JavaScript execution)
├── RequestServer Process (per WebContent)
│   ├── HTTP/HTTPS requests
│   ├── Fork: Tor/IPFS/VPN support
│   └── Fork: SecurityTap integration
├── ImageDecoder Process (per image, sandboxed)
├── Sentinel Service (standalone daemon) [Fork]
│   ├── YARA rule engine
│   ├── PolicyGraph SQLite database
│   └── Unix socket IPC (/tmp/sentinel.sock)
├── WebDriver Process (browser automation)
└── WebWorker Process (Web Workers)
```

### IPC Communication

- **Definition Files**: `.ipc` files define message interfaces (e.g., `Services/RequestServer/RequestServer.ipc`)
- **Code Generation**: IPC compiler generates endpoint code from `.ipc` files
- **Message Types**: Synchronous (`=>`) and asynchronous (`=|`)
- **Transport**: Unix sockets and platform-specific mechanisms

Example IPC definition:
```cpp
endpoint RequestServer {
    start_request(i32 request_id, ByteString method, URL::URL url, ...) =|
    stop_request(i32 request_id) => (bool success)
}
```

### Core Libraries

Located in `Libraries/`:

**Web Engine**:
- `LibWeb/` - HTML/CSS/DOM rendering engine (75+ subdirectories)
- `LibJS/` - JavaScript engine (ECMAScript implementation)
- `LibWasm/` - WebAssembly support

**Infrastructure**:
- `AK/` (root directory) - Application Kit: data structures (Vector, String, HashMap), error handling (ErrorOr, Try/Must)
- `LibCore/` - Event loop, file I/O, OS abstraction
- `LibIPC/` - Inter-process communication
- `LibGfx/` - 2D graphics, image decoding, font rendering
- `LibHTTP/` - HTTP/1.1 client
- `LibCrypto/LibTLS/` - Cryptography and TLS/SSL
- `LibWebView/` - Bridge between UI and WebContent

**Fork Enhancements**:
- `LibWebView/SentinelConfig.h` - Sentinel configuration
- `Services/RequestServer/SecurityTap.h` - YARA integration
- `Services/Sentinel/PolicyGraph.h` - Security policy database

## Coding Style

Follow the style in `Documentation/CodingStyle.md` and `.clang-format`:

### Naming Conventions
- **Classes/Structs/Namespaces**: CamelCase (e.g., `FileDescriptor`, `HTMLElement`)
- **Functions/Variables**: snake_case (e.g., `buffer_size`, `absolute_path()`)
- **Constants**: SCREAMING_CASE (e.g., `MAX_BUFFER_SIZE`)
- **Member Variables**: Prefix with `m_` (e.g., `m_document`, `m_length`)
- **Static Members**: Prefix with `s_` (e.g., `s_instance_count`)
- **Global Variables**: Prefix with `g_` (e.g., `g_config`)

### Key Patterns
```cpp
// Error handling - use TRY for error propagation (like Rust's ?)
ErrorOr<void> do_something() {
    auto result = TRY(fallible_operation());
    return {};
}

// MUST for operations that should never fail
MUST(vector.try_append(item));

// String literals - use "text"sv for zero-cost StringView
auto str = "Hello"sv;

// Fallible constructors - use static create() methods
class MyClass {
public:
    static ErrorOr<NonnullOwnPtr<MyClass>> create() {
        return adopt_nonnull_own(new MyClass());
    }
private:
    MyClass() = default;
};

// Entry points - use ladybird_main, not main
ErrorOr<int> ladybird_main(Main::Arguments arguments) {
    return 0;
}

// East const style
Salt const& m_salt;  // Right
const Salt& m_salt;  // Wrong

// Virtual methods - use both 'virtual' and 'override'/'final'
class Base {
    virtual String description() { ... }
};
class Derived : public Base {
    virtual String description() override { ... }  // Right
};
```

### Code Organization
- **Classes**: Use `class` for types with methods, keep members private with `m_` prefix
- **Structs**: Use `struct` for POD types, public members without `m_` prefix
- **Constructors**: Initialize members with C++ initializer syntax, prefer member initialization at definition
- **Getters/Setters**: Bare words for getters (e.g., `count()`), prefix setters with `set_` (e.g., `set_count()`)
- **Singletons**: Use static `the()` method to access instance

### Comments
- Use `//` for comments (not `/* */` except for copyright)
- Write as proper sentences with capitalization and punctuation
- Use `FIXME:` or `TODO:` without attribution for items to address
- Explain **why**, not **what** (the code should be self-explanatory)

## Testing

### Test Organization

```
Tests/
├── LibWeb/
│   ├── Text/            # JavaScript API tests with text output
│   ├── Layout/          # Layout tree comparison tests
│   ├── Ref/             # Screenshot comparison against reference HTML
│   └── Screenshot/      # Screenshot comparison against reference image
├── LibJS/               # JavaScript engine tests
├── AK/                  # AK container tests
├── Sentinel/            # Sentinel security tests [Fork]
└── Lib*/Test*.cpp       # C++ unit tests
```

### Test Types

1. **Text Tests** - Test Web APIs without visual representation, compare `println()` output
2. **Layout Tests** - Compare layout tree structure
3. **Ref Tests** - Compare screenshot against reference HTML page using `<link rel="match" href="...">`
4. **Screenshot Tests** - Compare against reference screenshot image
5. **Unit Tests** - C++ unit tests using LibTest framework

### Sanitizer Testing

CI runs with ASAN and UBSAN:
```bash
cmake --preset Sanitizers
cmake --build Build/sanitizers
ctest --preset Sanitizers
```

## Commit Message Format

Use strict imperative format:
```
Category: Brief imperative description

Detailed explanation if needed.
```

Examples:
- `LibWeb: Add CSS grid support`
- `RequestServer: Fix memory leak in connection handling`
- `Sentinel: Improve YARA rule matching performance`

Write in imperative mood: "Add feature" not "Added feature" or "Adds feature".

## Important Development Notes

### Web Standards Implementation

When implementing spec algorithms, **match spec naming exactly**:

```cpp
// Right - matches https://html.spec.whatwg.org/
bool HTMLInputElement::suffering_from_being_missing();

// Wrong - arbitrarily differs from spec
bool HTMLInputElement::has_missing_constraint();
```

### C++23 Features

This codebase uses **C++23** (requires gcc-14 or clang-20). Modern features are encouraged.

### No C-Style Casts

Use appropriate C++ cast operators: `static_cast`, `reinterpret_cast`, `bit_cast`, `dynamic_cast`.
Exception: `(void)parameter;` to mark parameters as used.

### Build Presets

Available CMake presets (see `CMakePresets.json`):
- **Release**: Default optimized build
- **Debug**: Debug build with symbols
- **Sanitizers**: ASAN/UBSAN for memory safety testing
- **Windows_Experimental**: Windows build with Clang-CL
- **All_Debug**: Debug with all debug macros enabled

## Key Documentation

Located in `Documentation/`:
- `BuildInstructionsLadybird.md` - Platform-specific build setup
- `ProcessArchitecture.md` - Multi-process architecture details
- `Testing.md` - Comprehensive testing guide
- `CodingStyle.md` - Complete coding style rules
- `LibWebFromLoadingToPainting.md` - LibWeb rendering pipeline
- `LibWebPatterns.md` - LibWeb code patterns
- `CSSProperties.md` - Adding new CSS properties
- `AddNewIDLFile.md` - Adding Web IDL files
- `GettingStartedContributing.md` - Contributing to upstream

Fork-specific docs in `docs/`:
- `FORK.md` - Fork overview
- `FEATURES.md` - Detailed feature documentation
- `SENTINEL_*.md` - Sentinel system documentation

## Entry Points

Key entry point files:
- UI Process: `UI/Qt/main.cpp`, `UI/AppKit/main.mm`
- WebContent: `Services/WebContent/main.cpp`
- RequestServer: `Services/RequestServer/main.cpp`
- ImageDecoder: `Services/ImageDecoder/main.cpp`
- Sentinel: `Services/Sentinel/main.cpp` [Fork]
- WebDriver: `Services/WebDriver/main.cpp`

All use `ladybird_main(Main::Arguments arguments)` instead of standard `main()`.
