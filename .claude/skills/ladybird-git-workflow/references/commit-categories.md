# Commit Categories Reference

Complete list of commit category prefixes used in Ladybird development.

## Core Libraries

### LibWeb - HTML/CSS/DOM Rendering Engine
**Path**: `Libraries/LibWeb/`
**Usage**: Any changes to web rendering, HTML, CSS, DOM
**Examples**:
```
LibWeb: Add CSS grid support
LibWeb: Implement HTMLInputElement constraint validation
LibWeb: Fix crash when parsing invalid CSS
```

### LibJS - JavaScript Engine
**Path**: `Libraries/LibJS/`
**Usage**: JavaScript runtime, ECMAScript implementation
**Examples**:
```
LibJS: Fix memory leak in running_execution_context()
LibJS: Add support for async generators
LibJS: Implement WeakRef and FinalizationRegistry
```

### LibGfx - 2D Graphics
**Path**: `Libraries/LibGfx/`
**Usage**: Graphics primitives, image codecs, fonts
**Examples**:
```
LibGfx: Add WebP image decoder
LibGfx: Optimize path rendering
LibGfx: Fix font metrics calculation
```

### AK - Application Kit
**Path**: Root directory `AK/`
**Usage**: Data structures, error handling, utilities
**Examples**:
```
AK: Add try_append() method to Vector
AK: Fix HashMap hash collision handling
AK: Optimize String copy-on-write
```

### LibCore - Core Functionality
**Path**: `Libraries/LibCore/`
**Usage**: Event loop, file I/O, system abstraction
**Examples**:
```
LibCore: Add EventLoop::quit_with_code()
LibCore: Fix race condition in timer handling
LibCore: Implement file watching API
```

### LibHTTP - HTTP Client
**Path**: `Libraries/LibHTTP/`
**Usage**: HTTP/1.1 client implementation
**Examples**:
```
LibHTTP: Add support for chunked transfer encoding
LibHTTP: Fix header parsing edge case
LibHTTP: Implement connection pooling
```

### LibTLS - TLS/SSL
**Path**: `Libraries/LibTLS/`
**Usage**: TLS protocol implementation
**Examples**:
```
LibTLS: Add TLS 1.3 support
LibTLS: Fix certificate validation
LibTLS: Implement session resumption
```

### LibCrypto - Cryptography
**Path**: `Libraries/LibCrypto/`
**Usage**: Cryptographic primitives
**Examples**:
```
LibCrypto: Add Ed25519 signature support
LibCrypto: Optimize AES implementation
LibCrypto: Fix timing attack in RSA
```

### LibIPC - Inter-Process Communication
**Path**: `Libraries/LibIPC/`
**Usage**: IPC infrastructure
**Examples**:
```
LibIPC: Add validation for untrusted messages
LibIPC: Optimize message serialization
LibIPC: Fix file descriptor passing
```

### LibWasm - WebAssembly
**Path**: `Libraries/LibWasm/`
**Usage**: WebAssembly runtime
**Examples**:
```
LibWasm: Add SIMD instruction support
LibWasm: Implement memory growth
LibWasm: Fix stack overflow in recursion
```

### LibWebView - WebView Bridge
**Path**: `Libraries/LibWebView/`
**Usage**: Bridge between UI and WebContent
**Examples**:
```
LibWebView: Add download progress callback
LibWebView: Implement tab crash recovery
LibWebView: Fix memory leak in page history
```

## Services (Multi-Process Architecture)

### RequestServer - HTTP Request Handling
**Path**: `Services/RequestServer/`
**Usage**: HTTP/HTTPS request service process
**Examples**:
```
RequestServer: Implement connection pooling
RequestServer: Fix memory leak in connection handling
RequestServer: Add HTTP/2 support
```

### WebContent - Web Rendering Process
**Path**: `Services/WebContent/`
**Usage**: Sandboxed web rendering process
**Examples**:
```
WebContent: Add process per tab support
WebContent: Implement clipboard access
WebContent: Fix crash in page serialization
```

### ImageDecoder - Image Decoding Process
**Path**: `Services/ImageDecoder/`
**Usage**: Sandboxed image decoding
**Examples**:
```
ImageDecoder: Add AVIF format support
ImageDecoder: Fix heap overflow in PNG decoder
ImageDecoder: Optimize JPEG decoding
```

### WebDriver - Browser Automation
**Path**: `Services/WebDriver/`
**Usage**: WebDriver protocol implementation
**Examples**:
```
WebDriver: Implement element screenshot command
WebDriver: Add support for shadow DOM
WebDriver: Fix timeout handling
```

## UI Implementations

### UI/Qt - Qt UI Implementation
**Path**: `UI/Qt/`
**Usage**: Qt-based UI for Linux/Windows
**Examples**:
```
UI/Qt: Add dark mode support
UI/Qt: Implement developer tools
UI/Qt: Fix menu bar on Linux
```

### UI/AppKit - macOS UI Implementation
**Path**: `UI/AppKit/`
**Usage**: macOS native UI
**Examples**:
```
UI/AppKit: Add native context menus
UI/AppKit: Implement Touch Bar support
UI/AppKit: Fix window restoration
```

## Build System and Tooling

### Meta - Build System, CI, Tooling
**Path**: `Meta/`
**Usage**: CMake, CI/CD, scripts, tools
**Examples**:
```
Meta: Upgrade prettier to version 3.6.2
Meta: Add fuzzing CI workflow
Meta: Fix build on macOS 14
Meta: Increase line length enforced by prettier to 120
```

### CMake - Build Configuration
**Path**: `CMakeLists.txt`, `Meta/CMake/`
**Usage**: CMake build system changes
**Examples**:
```
CMake: Add sanitizer build preset
CMake: Fix linking on Windows
CMake: Add C++23 compiler requirement check
```

### CI - Continuous Integration
**Path**: `.github/workflows/`
**Usage**: GitHub Actions, CI configuration
**Examples**:
```
CI: Add ASAN/UBSAN test run
CI: Enable macOS ARM64 builds
CI: Fix cache invalidation
```

## Testing

### Tests - Test Infrastructure
**Path**: `Tests/`
**Usage**: Test infrastructure, test additions
**Examples**:
```
Tests: Add canvas fingerprinting test cases
Tests: Add LibWeb constraint validation tests
Tests: Fix flaky layout test
```

## Documentation

### docs - Documentation (lowercase!)
**Path**: `Documentation/`, `docs/`, `*.md`
**Usage**: Documentation updates
**Examples**:
```
docs: Add fingerprinting detection documentation
docs: Update testing guide with new patterns
docs: Fix typos in coding style guide
```

Note: Use lowercase 'docs' for documentation commits.

## Fork-Specific Categories

### Sentinel - Security Detection Components
**Path**: `Services/Sentinel/`
**Usage**: Sentinel security features (this fork only)
**Examples**:
```
Sentinel: Add canvas fingerprinting detection
Sentinel: Improve YARA rule matching performance
Sentinel: Fix memory leak in PolicyGraph
Sentinel Milestone 0.4 Phase 1: ML-based malware detection
```

### Multi-Component Changes

When changes span multiple components, use `+`:
```
RequestServer+Sentinel: Fix critical performance issues
LibWeb+LibGfx: Implement canvas color management
WebContent+LibWeb: Add process isolation for workers
```

### Generic Categories (Personal Fork)

These are acceptable in personal forks but not upstream:

- **fix**: Generic bug fixes (lowercase)
- **feat**: Generic new features (lowercase)
- **refactor**: Large-scale refactoring
- **perf**: Performance improvements
- **chore**: Maintenance tasks

Examples:
```
fix: Update for upstream API changes
feat: Add experimental Tor integration
perf: Optimize end-to-end rendering pipeline
```

## Special Commit Types

### Merge Commits
```
Merge upstream/master: prettier upgrades and line length changes
Merge branch 'feature/css-grid' into master
```

### Revert Commits
```
Revert "LibWeb: Add experimental CSS container queries"
```

### Hotfix Commits
```
LibWeb: Fix XSS vulnerability in innerHTML setter
LibJS: Fix use-after-free in garbage collector
```

## Category Selection Decision Tree

```
Are you changing code in Libraries/?
├─ Yes, in LibWeb/        → LibWeb:
├─ Yes, in LibJS/         → LibJS:
├─ Yes, in LibGfx/        → LibGfx:
├─ Yes, in LibCore/       → LibCore:
└─ Yes, in other Lib      → Lib<Name>:

Are you changing code in Services/?
├─ Yes, in RequestServer/ → RequestServer:
├─ Yes, in WebContent/    → WebContent:
├─ Yes, in Sentinel/      → Sentinel:
└─ Yes, in other service  → <ServiceName>:

Are you changing code in UI/?
├─ Yes, in UI/Qt/         → UI/Qt:
└─ Yes, in UI/AppKit/     → UI/AppKit:

Are you changing build files?
├─ Yes, CMakeLists.txt    → CMake:
├─ Yes, Meta/             → Meta:
└─ Yes, CI workflows      → CI:

Are you adding/changing tests?
└─ Yes                    → Tests:

Are you updating documentation?
└─ Yes                    → docs: (lowercase!)

Multiple components?
└─ Yes                    → Component1+Component2:

Otherwise:
└─ Use most relevant component name
```

## Validation Rules

1. **Category must be CamelCase** (except 'docs', 'fix', 'feat')
2. **No spaces in category** (use + for multiple)
3. **Colon separates category from description**
4. **Description starts with capital letter**
5. **Description is imperative mood**
6. **No period at end of first line**
7. **First line max 72 characters**

## Common Mistakes

❌ `libweb: add feature` (lowercase library name)
✅ `LibWeb: Add feature`

❌ `LibWeb : Add feature` (space before colon)
✅ `LibWeb: Add feature`

❌ `LibWeb: add feature` (lowercase description)
✅ `LibWeb: Add feature`

❌ `LibWeb: Added feature` (past tense)
✅ `LibWeb: Add feature`

❌ `LibWeb: Adding feature` (gerund)
✅ `LibWeb: Add feature`

❌ `LibWeb: Add feature.` (period at end)
✅ `LibWeb: Add feature`

❌ `Docs: Update guide` (capital D)
✅ `docs: Update guide`
