# CMake Build System Skill

Expert guidance for working with Ladybird's CMake-based build system, including adding libraries, executables, managing dependencies, and troubleshooting build issues.

## Overview

Ladybird uses **CMake 3.25+** with **Ninja** as the build generator, wrapped by the **Meta/ladybird.py** Python script. The build system supports multiple platforms (Linux, macOS, Windows), multiple presets (Release, Debug, Sanitizers), and integrates with **vcpkg** for dependency management.

## Architecture

### Build System Components

1. **CMakeLists.txt** (root) - Top-level project configuration
2. **CMakePresets.json** - Build configuration presets
3. **Meta/ladybird.py** - Primary build wrapper script
4. **Meta/CMake/*.cmake** - CMake utility modules
5. **vcpkg** - Package manager for dependencies (in Build/vcpkg/)

### Key CMake Modules

Located in `Meta/CMake/`:
- `code_generators.cmake` - Code generation functions (IPC, generators)
- `utils.cmake` - General utility functions
- `lagom_compile_options.cmake` - Compiler flags
- `sanitizers.cmake` - Sanitizer configuration
- `use_linker.cmake` - Linker selection
- `libweb_generators.cmake` - LibWeb-specific generators
- `audio.cmake`, `vulkan.cmake`, `skia.cmake` - External library integration

## CMake Presets

### Available Presets

Defined in `CMakePresets.json`:

| Preset | Build Dir | Type | Purpose |
|--------|-----------|------|---------|
| **Release** | Build/release | RelWithDebInfo | Default optimized build |
| **Debug** | Build/debug | Debug | Debug symbols, no optimization |
| **All_Debug** | Build/alldebug | Debug | All debug macros enabled |
| **Sanitizer** | Build/sanitizers | RelWithDebInfo | ASAN + UBSAN enabled |
| **Distribution** | Build/distribution | Release | Static linking for distribution |
| **Windows_Experimental** | Build/debug | Debug | Windows/Clang-CL build |
| **Fuzzers** | Build/fuzzers | Special | LibFuzzer fuzzing build |
| **Swift_Release** | Build/swift | RelWithDebInfo | Swift-enabled build |

### Preset Configuration

Each preset inherits from `base` and configures:
- Build type (Debug, Release, RelWithDebInfo)
- vcpkg triplets overlay
- Sanitizer flags
- Compiler selection
- Platform-specific options

### Using Presets

```bash
# Via ladybird.py (recommended)
./Meta/ladybird.py build --preset Debug
./Meta/ladybird.py run --preset Sanitizer

# Environment variable
BUILD_PRESET=Debug ./Meta/ladybird.py build

# Direct CMake (advanced)
cmake --preset Debug
cmake --build Build/debug
```

## Adding New Libraries

### Library Structure Pattern

Ladybird libraries follow this structure:

```
Libraries/LibYourLib/
├── CMakeLists.txt
├── YourClass.h
├── YourClass.cpp
├── SubModule/
│   ├── SubClass.h
│   └── SubClass.cpp
└── Forward.h
```

### CMakeLists.txt Template for Libraries

```cmake
# List all source files
set(SOURCES
    YourClass.cpp
    SubModule/SubClass.cpp
    AnotherFile.cpp
)

# Optional: Generated sources (IPC, code generators)
set(GENERATED_SOURCES
    GeneratedFile.cpp
    GeneratedFile.h
)

# Create library target
# Use ladybird_lib() macro for proper configuration
ladybird_lib(LibYourLib yourlib)

# Link dependencies
target_link_libraries(LibYourLib
    PUBLIC LibCore AK          # Public dependencies (exposed in headers)
    PRIVATE LibCrypto LibGfx   # Private dependencies (implementation only)
)

# Optional: Include directories
target_include_directories(LibYourLib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

# Optional: Compile definitions
target_compile_definitions(LibYourLib
    PRIVATE YOURLIB_DEBUG=1
)

# Optional: Platform-specific configuration
if (WIN32)
    target_compile_definitions(LibYourLib PRIVATE YOURLIB_WINDOWS=1)
endif()

# Optional: Code generation
if (DEFINED GENERATED_SOURCES)
    ladybird_generated_sources(LibYourLib)
endif()
```

### Key Library Functions

From `Meta/CMake/utils.cmake` and custom macros:

- **ladybird_lib(target_name linkname)** - Create a library with proper export/import settings
- **ladybird_generated_sources(target_name)** - Handle generated sources
- **compile_ipc(source output)** - Compile IPC definitions
- **invoke_cpp_generator(name generator source header impl)** - Run C++ code generators
- **invoke_py_generator(name script source header impl)** - Run Python generators

### Adding Library to Build

1. Create library directory under `Libraries/`
2. Add CMakeLists.txt
3. Update parent `Libraries/CMakeLists.txt` (auto-included via subdirectories)
4. Reference in dependent targets via `target_link_libraries()`

## Adding New Executables/Services

### Service Structure Pattern

Services in `Services/` follow this pattern:

```
Services/YourService/
├── CMakeLists.txt
├── main.cpp
├── YourService.ipc
├── ConnectionFromClient.h
├── ConnectionFromClient.cpp
└── YourLogic.cpp
```

### CMakeLists.txt Template for Services

```cmake
# Disable Qt auto-tools if not needed
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

# Service implementation as library (for testing)
set(SOURCES
    ConnectionFromClient.cpp
    YourLogic.cpp
    AnotherFile.cpp
)

set(GENERATED_SOURCES
    YourServiceClientEndpoint.h
    YourServiceServerEndpoint.h
)

add_library(yourserviceservice STATIC ${SOURCES} ${GENERATED_SOURCES})
ladybird_generated_sources(yourserviceservice)

# Main service executable
add_executable(YourService main.cpp)

# Test executables
add_executable(TestYourService TestYourService.cpp)

# Include paths
target_include_directories(yourserviceservice
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..
    PRIVATE ${LADYBIRD_SOURCE_DIR}/Services/
)

# Linking
target_link_libraries(YourService PRIVATE yourserviceservice)
target_link_libraries(TestYourService PRIVATE yourserviceservice)
target_link_libraries(yourserviceservice
    PUBLIC LibCore LibIPC LibMain
    PRIVATE LibCrypto LibFileSystem
)

# Optional: Platform-specific linking
if (WIN32)
    lagom_windows_bin(YourService)
endif()

if (HAIKU)
    target_link_libraries(YourService PRIVATE network)
endif()

# Optional: Install targets
install(TARGETS YourService
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Install data files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Base/res/yourservice/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ladybird/yourservice
)
```

### IPC Code Generation

If service uses IPC:

```cmake
# Compile IPC endpoint definitions
compile_ipc(YourService.ipc YourServiceClientEndpoint.h)
compile_ipc(YourServer.ipc YourServiceServerEndpoint.h)

# Add to GENERATED_SOURCES
set(GENERATED_SOURCES
    YourServiceClientEndpoint.h
    YourServiceServerEndpoint.h
)

# Mark as generated
ladybird_generated_sources(yourserviceservice)
```

### Adding Service to Build

1. Create service directory under `Services/`
2. Add CMakeLists.txt
3. Update `Services/CMakeLists.txt`:
   ```cmake
   add_subdirectory(YourService)
   ```

## Dependency Management

### Internal Dependencies

Ladybird libraries available for linking:

**Core Libraries:**
- `AK` - Application Kit (root, not LibAK)
- `LibCore` - Core OS abstraction
- `LibIPC` - Inter-process communication
- `LibMain` - Main entry point wrapper

**Web Engine:**
- `LibWeb` - HTML/CSS/DOM engine
- `LibJS` - JavaScript engine
- `LibWasm` - WebAssembly

**Graphics/Media:**
- `LibGfx` - 2D graphics
- `LibMedia` - Media playback
- `LibAudio` - Audio

**Networking:**
- `LibHTTP` - HTTP client
- `LibTLS` - TLS/SSL
- `LibWebSocket` - WebSocket
- `LibURL` - URL parsing

**Utilities:**
- `LibCrypto` - Cryptography
- `LibCompress` - Compression
- `LibTextCodec` - Text encoding
- `LibUnicode` - Unicode support
- `LibRegex` - Regular expressions
- `LibDatabase` - SQLite wrapper
- `LibFileSystem` - File operations

### External Dependencies (vcpkg)

Managed via vcpkg, defined in `vcpkg.json`:

**Finding Packages:**
```cmake
# Standard CMake find_package
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc i18n)

# Qt (if ENABLE_QT)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# pkg-config fallback
find_package(PkgConfig)
pkg_check_modules(angle REQUIRED IMPORTED_TARGET angle)
```

**Linking External Packages:**
```cmake
target_link_libraries(YourTarget
    PRIVATE
        OpenSSL::Crypto
        OpenSSL::SSL
        CURL::libcurl
        ICU::uc
        ICU::i18n
)
```

### Custom External Dependencies

For libraries not in vcpkg:

```cmake
# Find library
find_library(YOURLIB_LIBRARY
    NAMES yourlib libyourlib
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)

find_path(YOURLIB_INCLUDE_DIR
    yourlib.h
    PATHS /usr/include /usr/local/include
)

# Check if found
if(NOT YOURLIB_LIBRARY)
    message(WARNING "YourLib not found. Feature disabled.")
else()
    message(STATUS "Found YourLib: ${YOURLIB_LIBRARY}")
    target_include_directories(YourTarget PRIVATE ${YOURLIB_INCLUDE_DIR})
    target_link_libraries(YourTarget PRIVATE ${YOURLIB_LIBRARY})
endif()
```

## Compiler Flags and Options

### Lagom Compile Options

From `Meta/CMake/lagom_compile_options.cmake`:

```cmake
# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add custom compile options
add_cxx_compile_options(-Wall -Wextra)
add_cxx_compile_options(-Wno-expansion-to-defined)
add_cxx_compile_options(-Wno-user-defined-literals)
```

### Sanitizer Configuration

From `Meta/CMake/sanitizers.cmake`:

**Address Sanitizer (ASAN):**
```cmake
set(ENABLE_ADDRESS_SANITIZER ON CACHE BOOL "")
```

**Undefined Behavior Sanitizer (UBSAN):**
```cmake
set(ENABLE_UNDEFINED_SANITIZER ON CACHE BOOL "")
```

**Runtime Options:**
```bash
export ASAN_OPTIONS="strict_string_checks=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1"
export UBSAN_OPTIONS="print_stacktrace=1:print_summary=1:halt_on_error=1"
```

### Platform-Specific Flags

```cmake
if (HAIKU)
    add_compile_definitions(_DEFAULT_SOURCE _GNU_SOURCE __USE_GNU)
endif()

if (WIN32)
    # Windows-specific flags
    set(CMAKE_C_COMPILER "clang-cl")
    set(CMAKE_CXX_COMPILER "clang-cl")
endif()

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
endif()
```

## Build Performance Optimization

### Parallel Builds

```bash
# Via ladybird.py
./Meta/ladybird.py build -j $(nproc)

# Environment variable
export MAKEJOBS=$(nproc)
./Meta/ladybird.py build

# Direct ninja
ninja -C Build/release -j $(nproc)
```

### Incremental Builds

```bash
# Only rebuild changed targets
./Meta/ladybird.py build YourTarget

# Rebuild specific test
./Meta/ladybird.py build TestYourFeature
```

### ccache Integration

Automatically enabled if ccache is installed. See `Meta/CMake/setup_ccache.cmake`.

### vcpkg Binary Cache

Configured in CMakePresets.json:
```json
"VCPKG_BINARY_SOURCES": "clear;files,${fileDir}/Build/caches/vcpkg-binary-cache,readwrite"
```

Pre-compiled packages cached in `Build/caches/vcpkg-binary-cache/`.

## Cross-Platform Build Patterns

### Linux

```bash
# Standard build
./Meta/ladybird.py build --preset Release

# AArch64 specific (requires GN for Skia)
# Handled automatically by Meta/ladybird.py configure_skia_jemalloc()
```

### macOS

```bash
# Standard build
./Meta/ladybird.py build --preset Release

# Deployment target set automatically:
# - 14.0 for non-Swift builds
# - 15.0 for Swift-enabled builds
```

### Windows

```bash
# Must run from Visual Studio Developer Command Prompt
./Meta/ladybird.py build --preset Windows_Experimental

# Uses Clang-CL compiler:
# CMAKE_C_COMPILER=clang-cl
# CMAKE_CXX_COMPILER=clang-cl
```

### Platform Detection

From `Meta/host_platform.py`:

```python
platform = Platform()
if platform.host_system == HostSystem.Linux:
    # Linux-specific
elif platform.host_system == HostSystem.macOS:
    # macOS-specific
elif platform.host_system == HostSystem.Windows:
    # Windows-specific
```

## Troubleshooting Common Build Errors

### Error: vcpkg not found

**Symptom:**
```
CMake Error: CMAKE_TOOLCHAIN_FILE not set
```

**Solution:**
```bash
# Ensure vcpkg is built
./Meta/ladybird.py vcpkg

# Or full rebuild
./Meta/ladybird.py rebuild
```

### Error: Ninja not found

**Symptom:**
```
Unable to find a build program corresponding to "Ninja"
```

**Solution:**
```bash
# Linux
sudo apt install ninja-build

# macOS
brew install ninja

# vcpkg downloads its own ninja to Build/vcpkg/
```

### Error: Compiler version too old

**Symptom:**
```
C++ compiler does not support C++23
```

**Solution:**
```bash
# Requires gcc-14 or clang-20
# Linux
sudo apt install gcc-14 g++-14
# Or
sudo apt install clang-20

# Set compiler explicitly
./Meta/ladybird.py build --cc gcc-14 --cxx g++-14
```

### Error: Missing external dependency

**Symptom:**
```
Could not find package Qt6 (requested version 6.0)
```

**Solution:**
```bash
# Option 1: Install via system package manager
sudo apt install qt6-base-dev  # Linux
brew install qt@6              # macOS

# Option 2: Disable feature if optional
cmake --preset Release -DENABLE_QT=OFF

# Option 3: Clean vcpkg cache
rm -rf Build/vcpkg Build/caches
./Meta/ladybird.py vcpkg
```

### Error: Generated files not found

**Symptom:**
```
fatal error: 'YourServiceEndpoint.h' file not found
```

**Solution:**
```bash
# Ensure IPC files are compiled
# Check GENERATED_SOURCES is set correctly
# Check ladybird_generated_sources() is called

# Clean build may help
./Meta/ladybird.py clean
./Meta/ladybird.py build
```

### Error: Linker errors (undefined reference)

**Symptom:**
```
undefined reference to `YourClass::method()'
```

**Solution:**
```bash
# Check target_link_libraries() order (PUBLIC before PRIVATE)
# Ensure source file is in SOURCES list
# For templates, implementation must be in header

# Rebuild from clean state
./Meta/ladybird.py rebuild
```

### Error: Out of memory during build

**Symptom:**
```
c++: fatal error: Killed signal terminated program cc1plus
```

**Solution:**
```bash
# Reduce parallel jobs
./Meta/ladybird.py build -j 2

# Use Release preset instead of Debug (smaller)
./Meta/ladybird.py build --preset Release

# Increase swap space or use machine with more RAM
```

### Error: Permission denied on Build directory

**Symptom:**
```
CMake Error: Cannot create directory Build/release
```

**Solution:**
```bash
# Never run ladybird.py as root
# Fix permissions:
sudo chown -R $(whoami):$(whoami) Build/
```

## Advanced Build Techniques

### Custom Presets

Create custom preset in `CMakePresets.json`:

```json
{
  "name": "MyCustom",
  "inherits": "unix_base",
  "displayName": "My Custom Config",
  "binaryDir": "${fileDir}/Build/mycustom",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "ENABLE_CUSTOM_FEATURE": "ON"
  }
}
```

### Out-of-Tree Builds

```bash
# Custom build directory
cmake -S . -B /path/to/custom/build --preset Release
cmake --build /path/to/custom/build
```

### Building Specific Targets

```bash
# Build only LibWeb
ninja -C Build/release LibWeb

# Build only Ladybird executable
ninja -C Build/release Ladybird

# Build all tests
ninja -C Build/release test
```

### Verbose Build Output

```bash
# Via CMake
cmake --build Build/release --verbose

# Via Ninja
ninja -C Build/release -v

# Via ladybird.py (passes through)
./Meta/ladybird.py build -- -v
```

### Build System Introspection

```bash
# List all targets
ninja -C Build/release -t targets

# Show dependency graph
ninja -C Build/release -t graph | dot -Tpng > deps.png

# Build statistics
ninja -C Build/release -t compdb > compile_commands.json
```

## Testing Integration

### Test Executables

Add test executables alongside libraries/services:

```cmake
add_executable(TestYourFeature TestYourFeature.cpp)
target_link_libraries(TestYourFeature PRIVATE yourlib LibCore LibMain)
```

### CTest Integration

```cmake
# Enable testing
enable_testing()

# Add test
add_test(NAME TestYourFeature COMMAND TestYourFeature)

# Set test properties
set_tests_properties(TestYourFeature PROPERTIES
    ENVIRONMENT "LADYBIRD_SOURCE_DIR=${LADYBIRD_SOURCE_DIR}"
    TIMEOUT 60
)
```

### Running Tests

```bash
# All tests
./Meta/ladybird.py test

# Pattern match
./Meta/ladybird.py test ".*YourFeature.*"

# CTest directly
cd Build/release && ctest
cd Build/release && ctest -R YourFeature
CTEST_OUTPUT_ON_FAILURE=1 ctest
```

## Build System Best Practices

1. **Always use ladybird_lib() macro** for libraries (handles exports)
2. **Separate library and executable** (library for testing, executable for main)
3. **Use GENERATED_SOURCES** for all generated files
4. **Call ladybird_generated_sources()** after defining generated sources
5. **Use PUBLIC/PRIVATE link visibility** correctly:
   - PUBLIC: dependency used in public headers
   - PRIVATE: dependency used only in .cpp files
6. **Platform-specific code** goes in conditionals (if(WIN32), if(APPLE))
7. **Optional dependencies** should gracefully degrade with message(WARNING)
8. **Install targets** for user-facing executables and data files
9. **Test executables** should link against library, not duplicate sources
10. **IPC definitions** must be compiled with compile_ipc() before use

## Real-World Examples

See `examples/` directory for:
- `add-library-example.txt` - Complete library CMakeLists.txt
- `add-executable-example.txt` - Complete service CMakeLists.txt
- `custom-preset-example.json` - Custom preset definition
- `dependency-integration-example.txt` - External dependency integration

## Quick Reference

### Essential Commands

```bash
# Configure and build
./Meta/ladybird.py build

# Clean rebuild
./Meta/ladybird.py rebuild

# Run Ladybird
./Meta/ladybird.py run

# Run tests
./Meta/ladybird.py test

# Clean build
./Meta/ladybird.py clean

# Custom preset
./Meta/ladybird.py build --preset Debug
```

### Essential CMake Functions

```cmake
# Library
ladybird_lib(LibName linkname)

# Generated sources
ladybird_generated_sources(target)

# IPC compilation
compile_ipc(source.ipc OutputEndpoint.h)

# Code generation
invoke_cpp_generator(name generator source header impl)
invoke_py_generator(name script source header impl)
```

### Essential Build Variables

```cmake
${LADYBIRD_SOURCE_DIR}      # Root source directory
${CMAKE_CURRENT_SOURCE_DIR} # Current CMakeLists.txt directory
${CMAKE_CURRENT_BINARY_DIR} # Current build output directory
${CMAKE_INSTALL_BINDIR}     # Installation bin directory
${CMAKE_INSTALL_DATADIR}    # Installation data directory
${VCPKG_INSTALLED_DIR}      # vcpkg installation directory
```

## Further Reading

- `Documentation/BuildInstructionsLadybird.md` - Platform build setup
- `CMakePresets.json` - All available presets
- `Meta/CMake/*.cmake` - CMake utility modules
- `Meta/ladybird.py` - Build script implementation
- `Libraries/LibWeb/CMakeLists.txt` - Large library example
- `Services/RequestServer/CMakeLists.txt` - Service example
- `Services/Sentinel/CMakeLists.txt` - Service with external deps example

## Related Skills

### Build Foundation for All Skills
This skill is foundational for building Ladybird. Reference when:
- **[ipc-security](../ipc-security/SKILL.md)**: Compile IPC definitions with compile_ipc()
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Add LibWeb sources and compile IDL files
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Build bytecode compiler and runtime components
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Build fuzzer targets with ENABLE_FUZZERS=ON
- **[tor-integration](../tor-integration/SKILL.md)**: Link external libraries (tor, socks)

### Build Configuration
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Build with sanitizers using Sanitizer preset. Enable ASAN/UBSAN/MSAN for debugging.
- **[browser-performance](../browser-performance/SKILL.md)**: Build Release preset for profiling. Understand optimization flags and LTO configuration.
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Configure CI builds with CMake presets. Use ccache and vcpkg binary caching.

### Development Workflow
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Build C++23 code with proper compiler flags. Understand requirements for gcc-14/clang-20.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Build test-web runner and WPT infrastructure.
- **[ladybird-git-workflow](../ladybird-git-workflow/SKILL.md)**: Build workflow integration. Use Meta/ladybird.py for consistent builds.

### Documentation
- **[documentation-generation](../documentation-generation/SKILL.md)**: Generate build documentation and dependency graphs. Document CMake patterns and build targets.
