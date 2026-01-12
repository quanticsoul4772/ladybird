# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About This Fork

This is a **personal learning fork** of Ladybird Browser with experimental privacy and security features. Not production-ready or intended for upstream contribution.

Fork-specific features:
- **Sentinel Service** (`Services/Sentinel/`): Multi-layered security system
  - YARA-based malware detection with ML enhancement (TensorFlow Lite)
  - Real-time credential exfiltration detection (FormMonitor)
  - Browser fingerprinting detection and scoring
  - Phishing URL analysis (URLSecurityAnalyzer)
  - PolicyGraph database for security policies and trusted relationships
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

### Testing Fork Features
```bash
# Sentinel component tests
./Build/release/bin/TestFingerprintingDetector  # Fingerprinting detection
./Build/release/bin/TestPolicyGraph              # Policy database

# Test fingerprinting detection in browser
# 1. Run Ladybird
./Build/release/bin/Ladybird
# 2. Load test page
file:///home/rbsmith4/ladybird/test_canvas_fingerprinting.html

# Test with real fingerprinting sites
# - https://browserleaks.com/canvas
# - https://amiunique.org/fingerprint
# - https://coveryourtracks.eff.org/

# Credential protection tests
./Meta/ladybird.py run test-web -- -f Text/input/credential-protection-*.html

# Enable debug logging
WEBCONTENT_DEBUG=1 LIBWEB_DEBUG=1 ./Build/release/bin/Ladybird
```

## Code Architecture

### Multi-Process Design

Ladybird uses a **sandboxed multi-process architecture**:

```
UI Process (Browser)
├── WebContent Process (per tab, sandboxed)
│   ├── LibWeb (HTML/CSS rendering)
│   ├── LibJS (JavaScript execution)
│   ├── FormMonitor (credential exfiltration detection) [Fork]
│   └── FingerprintingDetector (browser fingerprinting detection) [Fork]
├── RequestServer Process (per WebContent)
│   ├── HTTP/HTTPS requests
│   ├── Fork: Tor/IPFS/VPN support
│   ├── Fork: SecurityTap (YARA malware scanning)
│   └── Fork: URLSecurityAnalyzer (phishing detection)
├── ImageDecoder Process (per image, sandboxed)
├── Sentinel Service (standalone daemon) [Fork]
│   ├── YARA rule engine + ML (TensorFlow Lite)
│   ├── PolicyGraph SQLite database
│   ├── PhishingURLAnalyzer
│   ├── FormAnomalyDetector
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
- `Services/Sentinel/` - Security detection components:
  - `PolicyGraph.{h,cpp}` - Security policy database (SQLite)
  - `FingerprintingDetector.{h,cpp}` - Browser fingerprinting detection
  - `PhishingURLAnalyzer.{h,cpp}` - Phishing URL analysis
  - `FormAnomalyDetector.{h,cpp}` - Anomalous form behavior detection
  - `MalwareML.{h,cpp}` - ML-based malware detection (TensorFlow Lite)
- `Services/RequestServer/SecurityTap.{h,cpp}` - YARA integration for downloads
- `Services/RequestServer/URLSecurityAnalyzer.{h,cpp}` - Real-time phishing detection
- `Services/WebContent/FormMonitor.{h,cpp}` - Credential exfiltration detection
- `Services/WebContent/PageClient.{h,cpp}` - Fingerprinting detector integration
- `Libraries/LibWeb/HTML/HTMLCanvasElement.cpp` - Canvas fingerprinting hooks
- `Libraries/LibWeb/HTML/CanvasRenderingContext2D.cpp` - Canvas API monitoring
- `LibWebView/SentinelConfig.h` - Sentinel configuration

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

## Fork-Specific Architecture Patterns

### Security Detection Pattern

All fork security features follow a consistent pattern:

```cpp
// 1. Detector in Services/Sentinel/ (core logic, no LibWeb dependencies)
class FingerprintingDetector {
    static ErrorOr<NonnullOwnPtr<FingerprintingDetector>> create();
    void record_api_call(FingerprintingTechnique, StringView api_name, bool had_user_interaction);
    FingerprintingScore calculate_score() const;
};

// 2. Integration in WebContent/PageClient (owns detector instance)
class PageClient {
    OwnPtr<Sentinel::FingerprintingDetector> m_fingerprinting_detector;
    void notify_fingerprinting_api_call(/* params */);
};

// 3. Hooks in LibWeb (call into detector)
// Libraries/LibWeb/HTML/HTMLCanvasElement.cpp
ErrorOr<String> HTMLCanvasElement::to_data_url(/* params */) {
    // ... existing implementation ...

    // Hook: Record fingerprinting API call
    if (auto* page = document().page())
        page->notify_fingerprinting_api_call(/* params */);

    return result;
}
```

### PolicyGraph Integration Pattern

Security decisions use PolicyGraph for persistent storage:

```cpp
// 1. Check existing policy
auto result = m_policy_graph->evaluate_policy(domain, resource);
if (result.has_value()) {
    // Use cached decision
    return result.value();
}

// 2. Detect threat
auto score = detector->calculate_score();
if (score.is_aggressive()) {
    // 3. Alert user (via IPC to UI)
    send_security_alert(/* params */);

    // 4. User decision stored in PolicyGraph
    m_policy_graph->create_policy(domain, resource, user_choice);
}
```

### Graceful Degradation

All fork features implement graceful degradation:

```cpp
// Always use TRY and handle failures
auto detector = TRY(FingerprintingDetector::create());

// Check for null before use
if (m_fingerprinting_detector) {
    m_fingerprinting_detector->record_api_call(/* params */);
}

// Never fail core browser functionality
// If security feature fails, log and continue
if (auto result = detector->analyze(); result.is_error()) {
    dbgln("Warning: Fingerprinting detection failed: {}", result.error());
    // Continue normal page operation
}
```

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
- `CHANGELOG.md` - Sentinel development changelog
- **Sentinel System**:
  - `SENTINEL_ARCHITECTURE.md` - System architecture
  - `SENTINEL_SETUP_GUIDE.md` - Installation guide
  - `SENTINEL_USER_GUIDE.md` - End-user documentation
  - `SENTINEL_POLICY_GUIDE.md` - Policy management
  - `SENTINEL_YARA_RULES.md` - Rule documentation
- **Security Features**:
  - `USER_GUIDE_CREDENTIAL_PROTECTION.md` - Credential protection guide
  - `FINGERPRINTING_DETECTION_ARCHITECTURE.md` - Fingerprinting detection docs
  - `PHISHING_DETECTION_ARCHITECTURE.md` - Phishing detection docs
  - `TENSORFLOW_LITE_INTEGRATION.md` - ML malware detection docs
- **Development**:
  - `MILESTONE_0.3_PLAN.md` - Credential protection milestone
  - `MILESTONE_0.4_PLAN.md` - Advanced detection milestone
  - `MILESTONE_0.4_TECHNICAL_SPECS.md` - Technical specifications

## Entry Points

Key entry point files:
- UI Process: `UI/Qt/main.cpp`, `UI/AppKit/main.mm`
- WebContent: `Services/WebContent/main.cpp`
- RequestServer: `Services/RequestServer/main.cpp`
- ImageDecoder: `Services/ImageDecoder/main.cpp`
- Sentinel: `Services/Sentinel/main.cpp` [Fork]
- WebDriver: `Services/WebDriver/main.cpp`

All use `ladybird_main(Main::Arguments arguments)` instead of standard `main()`.

## Sentinel Development Milestones

This fork follows a structured development plan:

### Milestone 0.1 - Malware Scanning (Complete)
- YARA-based malware detection for downloads
- SecurityTap integration with RequestServer
- PolicyGraph SQLite database
- Quarantine system and security alerts

### Milestone 0.2 - Credential Protection (Complete)
- FormMonitor for cross-origin credential submissions
- Trusted form relationship management
- Password autofill protection
- User education and security tips

### Milestone 0.3 - Enhanced Credential Protection (Complete)
- Phase 1-6: Database schema, API, FormMonitor integration
- Import/export UI for credential relationships
- Policy template system
- Form anomaly detection

### Milestone 0.4 - Advanced Detection (In Progress)
- **Phase 1**: ML-based malware detection (TensorFlow Lite) ✅
- **Phase 4**: Browser fingerprinting detection ✅
- **Phase 5**: Phishing URL analysis ✅
- Pending: WebGL/Audio/Navigator fingerprinting hooks
- Pending: User alerts and policy UI integration

When working on Sentinel features:
1. Core detection logic goes in `Services/Sentinel/` (no LibWeb dependencies)
2. Integration hooks go in `Services/WebContent/` or `Services/RequestServer/`
3. LibWeb hooks are minimal - just call into PageClient
4. All features must gracefully degrade if initialization fails
5. Unit tests in `Services/Sentinel/Test*.cpp`
6. Browser tests in `Tests/LibWeb/Text/input/`
