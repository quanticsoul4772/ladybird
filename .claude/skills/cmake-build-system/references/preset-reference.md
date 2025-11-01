# CMake Preset Reference

Complete reference for all available CMake presets in Ladybird.

## Preset Overview

CMake presets are defined in `CMakePresets.json` and provide pre-configured build environments for different purposes. Each preset configures:
- Build type (Debug/Release/RelWithDebInfo)
- Build directory location
- vcpkg triplet overlay
- Compiler flags
- Sanitizer options
- Feature flags

## Using Presets

### Via ladybird.py (Recommended)

```bash
./Meta/ladybird.py build --preset <preset-name>
./Meta/ladybird.py run --preset <preset-name>
./Meta/ladybird.py test --preset <preset-name>
```

### Environment Variable

```bash
export BUILD_PRESET=Debug
./Meta/ladybird.py build
```

### Direct CMake

```bash
cmake --preset <preset-name>
cmake --build --preset <preset-name>
ctest --preset <preset-name>
```

## Available Presets

### Release (Default)

**Purpose:** Default optimized build for development and general use

**Configuration:**
- **Build Type:** RelWithDebInfo
- **Build Directory:** `Build/release`
- **vcpkg Triplet:** release-triplets
- **Optimizations:** `-O2` with debug symbols
- **Sanitizers:** Disabled
- **Platforms:** Linux, macOS, FreeBSD

**When to use:**
- Default choice for most development
- Faster execution than Debug
- Debug symbols for debugging when needed
- Testing production-like performance

**Usage:**
```bash
./Meta/ladybird.py build --preset Release
./Meta/ladybird.py run --preset Release
```

---

### Debug

**Purpose:** Full debugging experience with no optimizations

**Configuration:**
- **Build Type:** Debug
- **Build Directory:** `Build/debug`
- **vcpkg Triplet:** debug-triplets
- **Optimizations:** `-O0 -g`
- **Sanitizers:** Disabled
- **Debug Macros:** Standard set
- **Platforms:** Linux, macOS, FreeBSD

**When to use:**
- Active debugging with GDB/LLDB
- Step-by-step code execution
- Inspecting variables
- Understanding control flow
- Slower execution acceptable

**Pros:**
- Best debugging experience
- All symbols available
- No compiler optimization interference

**Cons:**
- Slower execution (2-10x slower)
- Larger binaries
- Longer compile times

**Usage:**
```bash
./Meta/ladybird.py debug --preset Debug
# Or build and debug manually
./Meta/ladybird.py build --preset Debug
gdb Build/debug/bin/Ladybird
```

---

### All_Debug

**Purpose:** Debug build with all debug macros enabled

**Configuration:**
- **Build Type:** Debug
- **Build Directory:** `Build/alldebug`
- **vcpkg Triplet:** debug-triplets
- **Optimizations:** `-O0 -g`
- **Debug Macros:** `ENABLE_ALL_THE_DEBUG_MACROS=ON`
- **Sanitizers:** Disabled
- **Platforms:** Linux, macOS, FreeBSD

**Additional Features:**
- All `dbgln()` statements active
- All debug assertions enabled
- Maximum diagnostic output

**When to use:**
- Deep debugging of complex issues
- Understanding internal state changes
- Tracing execution flow
- Need verbose logging
- Investigating rare bugs

**Cons:**
- Very verbose output
- Significantly slower execution
- Large log files
- May mask timing-dependent bugs

**Usage:**
```bash
./Meta/ladybird.py build --preset All_Debug
./Meta/ladybird.py run --preset All_Debug 2>&1 | tee debug.log
```

---

### Sanitizer

**Purpose:** Release build with AddressSanitizer and UndefinedBehaviorSanitizer

**Configuration:**
- **Build Type:** RelWithDebInfo
- **Build Directory:** `Build/sanitizers`
- **vcpkg Triplet:** sanitizer-triplets
- **Optimizations:** `-O2 -g` with sanitizer instrumentation
- **Sanitizers:**
  - `ENABLE_ADDRESS_SANITIZER=ON`
  - `ENABLE_UNDEFINED_SANITIZER=ON`
- **Platforms:** Linux, macOS (limited Windows support)

**Environment Variables:**
```bash
ASAN_OPTIONS="strict_string_checks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1:allocator_may_return_null=1"
UBSAN_OPTIONS="print_stacktrace=1:print_summary=1:halt_on_error=1"
```

**Detects:**
- Memory safety issues:
  - Use-after-free
  - Buffer overflows
  - Stack/heap overflow
  - Memory leaks
  - Double-free
- Undefined behavior:
  - Integer overflow
  - Null pointer dereference
  - Invalid shifts
  - Type errors

**When to use:**
- CI/CD testing
- Pre-release validation
- Hunting memory bugs
- Validating new code
- Ensuring memory safety

**Cons:**
- 2-3x slower execution
- 2-3x memory overhead
- May alter program behavior slightly

**Usage:**
```bash
./Meta/ladybird.py build --preset Sanitizer
./Meta/ladybird.py test --preset Sanitizer
./Meta/ladybird.py run --preset Sanitizer
```

---

### Distribution

**Purpose:** Optimized release build with static linking for distribution

**Configuration:**
- **Build Type:** Release
- **Build Directory:** `Build/distribution`
- **vcpkg Triplet:** distribution-triplets
- **Optimizations:** `-O3` (maximum optimization)
- **Linking:** Static (`BUILD_SHARED_LIBS=OFF`)
- **Debug Symbols:** Minimal
- **Platforms:** Linux, macOS, FreeBSD

**When to use:**
- Building release packages
- Creating standalone binaries
- Distribution to end users
- Production deployments
- Minimizing external dependencies

**Pros:**
- Maximum performance
- No runtime dependencies
- Single-file distribution
- Best optimization

**Cons:**
- Longest compile times
- Largest binary size
- Difficult to debug
- Cannot update libraries independently

**Usage:**
```bash
./Meta/ladybird.py build --preset Distribution
./Meta/ladybird.py install --preset Distribution
```

---

### Windows_Experimental

**Purpose:** Experimental Windows build using Clang-CL

**Configuration:**
- **Build Type:** Debug
- **Build Directory:** `Build/debug`
- **vcpkg Triplet:** debug-triplets
- **Compiler:** clang-cl (Clang with MSVC ABI)
- **Architecture:** x64
- **Platform:** Windows only
- **Status:** Experimental, not all targets supported

**Requirements:**
- Visual Studio 2022+ with Clang/LLVM toolchain
- Must run from Visual Studio Developer Command Prompt

**When to use:**
- Windows development
- Testing Windows compatibility
- Cross-platform verification

**Limitations:**
- Not all features work
- Some targets may not build
- Limited platform support

**Usage:**
```powershell
# From Visual Studio Developer Command Prompt
./Meta/ladybird.py build --preset Windows_Experimental
```

---

### Windows_CI

**Purpose:** Windows CI build configuration

**Configuration:**
- **Build Type:** RelWithDebInfo
- **Build Directory:** `Build/release`
- **vcpkg Triplet:** release-triplets
- **Compiler:** clang-cl
- **Flag:** `ENABLE_WINDOWS_CI=ON`
- **Platform:** Windows only

**When to use:**
- Automated CI/CD on Windows
- GitHub Actions Windows runners
- Build verification on Windows

---

### Windows_Sanitizer_Experimental

**Purpose:** Windows build with sanitizers (experimental)

**Configuration:**
- **Build Type:** RelWithDebInfo
- **Build Directory:** `Build/sanitizers`
- **vcpkg Triplet:** sanitizer-triplets
- **Compiler:** clang-cl
- **Sanitizers:** ASAN + UBSAN
- **Platform:** Windows only
- **Status:** Experimental

**Limitations:**
- Limited sanitizer support on Windows
- Not all sanitizers available
- Some features may not work

---

### Fuzzers

**Purpose:** Build for fuzzing with LibFuzzer

**Configuration:**
- **Build Type:** Custom
- **Build Directory:** `Build/fuzzers`
- **vcpkg Triplet:** distribution-triplets
- **Linking:** Static
- **Flags:**
  - `ENABLE_FUZZERS_LIBFUZZER=ON`
  - `ENABLE_ADDRESS_SANITIZER=ON`
  - `ENABLE_QT=OFF` (no GUI)
- **Platforms:** Linux (primary), macOS

**When to use:**
- Fuzzing LibWeb components
- Finding parser bugs
- Automated security testing
- Continuous fuzzing integration

**Usage:**
```bash
./Meta/ladybird.py build --preset Fuzzers
# Run specific fuzzer
Build/fuzzers/bin/FuzzHTML corpus/ -max_total_time=3600
```

---

### Swift_Release

**Purpose:** Swift-enabled release build (experimental)

**Configuration:**
- **Build Type:** RelWithDebInfo
- **Build Directory:** `Build/swift`
- **Flag:** `ENABLE_SWIFT=ON`
- **Deployment Target:** macOS 15.0+
- **Compilers:** Must be explicitly set
- **Platform:** macOS only

**When to use:**
- Developing Swift integration
- Testing Swift interop
- Swift-based features

**Requirements:**
- Swift compiler
- Xcode 16+
- macOS 15.0+

**Usage:**
```bash
./Meta/ladybird.py build --preset Swift_Release
```

---

## Preset Inheritance Hierarchy

```
base (hidden)
├── Environment: VCPKG_ROOT, VCPKG_BINARY_SOURCES
├── Cache: CMAKE_TOOLCHAIN_FILE, LADYBIRD_CACHE_DIR
└── Generator: Ninja

unix_base (hidden) → base
├── Condition: Not Windows
├── Used by Unix-like systems
└── Presets:
    ├── Release
    ├── Debug
    ├── All_Debug
    ├── Sanitizer
    ├── Distribution
    ├── Fuzzers
    └── Swift_Release

windows_base (hidden) → base
├── Condition: Windows only
├── Compiler: clang-cl
├── Architecture: x64
└── Presets:
    ├── Windows_Experimental
    ├── Windows_CI
    └── Windows_Sanitizer_CI
```

## Preset Comparison Table

| Preset | Build Type | Optimization | Debug Symbols | Sanitizers | Build Time | Runtime Speed | Binary Size | Use Case |
|--------|------------|--------------|---------------|------------|------------|---------------|-------------|----------|
| Release | RelWithDebInfo | -O2 | Yes | No | Medium | Fast | Medium | Default development |
| Debug | Debug | -O0 | Full | No | Fast | Slow | Large | Active debugging |
| All_Debug | Debug | -O0 | Full | No | Fast | Very Slow | Very Large | Deep debugging |
| Sanitizer | RelWithDebInfo | -O2 | Yes | ASAN+UBSAN | Medium | Slow | Large | Bug hunting |
| Distribution | Release | -O3 | Minimal | No | Slow | Very Fast | Large (static) | Production |
| Fuzzers | Special | -O1 | Yes | ASAN | Medium | Medium | Medium | Fuzzing |

## Creating Custom Presets

### Template for Custom Preset

```json
{
  "name": "MyCustomPreset",
  "inherits": "unix_base",
  "displayName": "My Custom Build",
  "description": "Description of custom preset",
  "binaryDir": "${fileDir}/Build/mycustom",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "CUSTOM_OPTION": "ON",
    "CMAKE_CXX_FLAGS": "-custom-flag"
  }
}
```

Add to `CMakePresets.json` under `configurePresets` array.

## Common CMake Variables

Variables you can set in preset `cacheVariables`:

### Build Configuration
- `CMAKE_BUILD_TYPE`: Debug, Release, RelWithDebInfo, MinSizeRel
- `BUILD_SHARED_LIBS`: ON/OFF (static vs shared libraries)
- `CMAKE_EXPORT_COMPILE_COMMANDS`: ON (for language servers)

### Compiler Selection
- `CMAKE_C_COMPILER`: Path to C compiler
- `CMAKE_CXX_COMPILER`: Path to C++ compiler
- `CMAKE_Swift_COMPILER`: Path to Swift compiler

### Compiler Flags
- `CMAKE_CXX_FLAGS`: Additional C++ flags
- `CMAKE_C_FLAGS`: Additional C flags
- `CMAKE_EXE_LINKER_FLAGS`: Linker flags
- `CMAKE_SHARED_LINKER_FLAGS`: Shared library linker flags

### Features
- `ENABLE_QT`: ON/OFF (Qt UI support)
- `ENABLE_SWIFT`: ON/OFF (Swift integration)
- `ENABLE_GUI_TARGETS`: ON/OFF (GUI applications)
- `BUILD_TESTING`: ON/OFF (test executables)

### Sanitizers
- `ENABLE_ADDRESS_SANITIZER`: ON/OFF
- `ENABLE_UNDEFINED_SANITIZER`: ON/OFF
- `ENABLE_MEMORY_SANITIZER`: ON/OFF
- `ENABLE_THREAD_SANITIZER`: ON/OFF

### Ladybird-Specific
- `ENABLE_ALL_THE_DEBUG_MACROS`: ON/OFF
- `ENABLE_FUZZERS_LIBFUZZER`: ON/OFF
- `ENABLE_WINDOWS_CI`: ON/OFF
- `LADYBIRD_CACHE_DIR`: Path to cache directory

### vcpkg
- `CMAKE_TOOLCHAIN_FILE`: Path to vcpkg toolchain
- `VCPKG_INSTALL_OPTIONS`: vcpkg install options
- `VCPKG_OVERLAY_TRIPLETS`: Custom triplet directory

## Preset Selection Guide

**I want to...**

- **Develop normally:** Use `Release`
- **Debug a crash:** Use `Debug` or `Sanitizer`
- **Find memory bugs:** Use `Sanitizer`
- **Maximum debugging output:** Use `All_Debug`
- **Test performance:** Use `Release` or `Distribution`
- **Build for release:** Use `Distribution`
- **Run CI tests:** Use `Sanitizer` on Linux, `Release` on others
- **Fuzz test code:** Use `Fuzzers`
- **Develop on Windows:** Use `Windows_Experimental`
- **Develop Swift integration:** Use `Swift_Release`

## Troubleshooting

### Preset not found
```
Error: Preset "XYZ" not found
```
**Solution:** Check `CMakePresets.json` for valid preset names

### Compiler not supported
```
Error: Compiler does not support C++23
```
**Solution:** Install gcc-14+ or clang-20+, set compiler explicitly

### vcpkg errors
```
Error: vcpkg install failed
```
**Solution:**
1. Clean vcpkg: `rm -rf Build/vcpkg Build/caches`
2. Rebuild: `./Meta/ladybird.py vcpkg`
3. Try again: `./Meta/ladybird.py build`

### Wrong preset used
Check which preset is active:
```bash
cat Build/release/CMakeCache.txt | grep CMAKE_BUILD_TYPE
```

## Performance Comparison

Approximate relative performance (Release = 1.0):

| Preset | Compile Time | Runtime Speed | Memory Usage |
|--------|--------------|---------------|--------------|
| Release | 1.0x | 1.0x | 1.0x |
| Debug | 0.7x | 0.2x | 1.2x |
| All_Debug | 0.7x | 0.1x | 1.3x |
| Sanitizer | 1.1x | 0.4x | 2.5x |
| Distribution | 1.5x | 1.2x | 0.9x |

*Values are approximate and vary by system and workload*
