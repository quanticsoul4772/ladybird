# Ladybird CMake Structure

## Overview

Ladybird's build system is organized hierarchically with CMakeLists.txt files at multiple levels, orchestrated by a Python wrapper script and configured through CMake presets.

## Directory Structure

```
ladybird/
├── CMakeLists.txt                          # Root project configuration
├── CMakePresets.json                       # Build configuration presets
├── vcpkg.json                              # Dependency specifications
├── Meta/
│   ├── ladybird.py                         # Primary build wrapper script
│   └── CMake/                              # CMake utility modules
│       ├── code_generators.cmake           # Code generation functions
│       ├── utils.cmake                     # General utilities
│       ├── lagom_compile_options.cmake     # Compiler flags
│       ├── sanitizers.cmake                # Sanitizer configuration
│       ├── use_linker.cmake                # Linker selection
│       ├── libweb_generators.cmake         # LibWeb generators
│       ├── audio.cmake                     # Audio library integration
│       ├── vulkan.cmake                    # Vulkan integration
│       └── skia.cmake                      # Skia integration
├── Libraries/
│   ├── AK/                                 # Application Kit (no CMakeLists)
│   ├── LibCore/
│   │   └── CMakeLists.txt
│   ├── LibWeb/
│   │   └── CMakeLists.txt
│   ├── LibJS/
│   │   └── CMakeLists.txt
│   └── ...
├── Services/
│   ├── CMakeLists.txt                      # Top-level services
│   ├── WebContent/
│   │   └── CMakeLists.txt
│   ├── RequestServer/
│   │   └── CMakeLists.txt
│   ├── Sentinel/
│   │   └── CMakeLists.txt
│   └── ...
├── UI/
│   ├── CMakeLists.txt
│   ├── Qt/
│   │   └── CMakeLists.txt
│   └── AppKit/
│       └── CMakeLists.txt
├── Build/                                  # Generated build artifacts
│   ├── release/                            # Release preset build
│   ├── debug/                              # Debug preset build
│   ├── sanitizers/                         # Sanitizer preset build
│   ├── vcpkg/                              # vcpkg installation
│   └── caches/                             # vcpkg binary caches
└── Tests/
    └── LibWeb/
        └── CMakeLists.txt
```

## Root CMakeLists.txt

Located at: `ladybird/CMakeLists.txt`

**Key responsibilities:**
1. Set minimum CMake version (3.25)
2. Configure vcpkg toolchain
3. Set deployment targets (macOS)
4. Define project metadata
5. Include Lagom (SerenityOS compatibility layer)
6. Find required dependencies (OpenSSL, Qt6)
7. Add subdirectories (Services, UI)
8. Configure custom targets (lint, style check)

**Important settings:**
```cmake
cmake_minimum_required(VERSION 3.25)
project(ladybird VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Lagom integration
include(UI/cmake/EnableLagom.cmake)
include(lagom_options NO_POLICY_SCOPE)
include(lagom_compile_options)
```

## CMakePresets.json

Located at: `ladybird/CMakePresets.json`

**Structure:**
- `configurePresets`: Build configuration variants
- `buildPresets`: Build execution configurations
- `testPresets`: Test execution configurations

**Key presets:**
- `base`: Hidden base configuration with vcpkg setup
- `unix_base`: Unix-specific base (Linux, macOS, FreeBSD)
- `windows_base`: Windows-specific base (Clang-CL)
- Public presets: Release, Debug, Sanitizer, Distribution, etc.

**Preset inheritance:**
```
base
├── unix_base
│   ├── Release
│   ├── Debug
│   ├── Sanitizer
│   └── Distribution
└── windows_base
    ├── Windows_Experimental
    └── Windows_CI
```

## Meta/ladybird.py

Located at: `Meta/ladybird.py`

**Commands:**
- `build` - Configure and build
- `run` - Build and execute
- `test` - Build and run tests
- `debug` - Launch in debugger
- `profile` - Launch with profiling
- `clean` - Clean build directory
- `rebuild` - Clean and build
- `vcpkg` - Bootstrap vcpkg
- `addr2line` - Resolve addresses to source

**Workflow:**
1. Parse arguments
2. Validate environment
3. Determine preset and build directory
4. Configure build environment (VCPKG_ROOT, PATH)
5. Bootstrap vcpkg if needed
6. Run CMake configure
7. Execute command (build/run/test)

## Library CMakeLists.txt Pattern

**Template structure:**
```cmake
# 1. Source files
set(SOURCES file1.cpp file2.cpp ...)

# 2. Generated sources (optional)
set(GENERATED_SOURCES gen1.cpp gen1.h ...)

# 3. Create library
ladybird_lib(LibName linkname [EXPLICIT_SYMBOL_EXPORT])

# 4. Dependencies
target_link_libraries(LibName
    PUBLIC  PublicDeps
    PRIVATE PrivateDeps
)

# 5. Code generation (optional)
compile_ipc(source.ipc OutputEndpoint.h)
invoke_cpp_generator(...)
ladybird_generated_sources(LibName)
```

**Example: LibWeb**

Location: `Libraries/LibWeb/CMakeLists.txt`

Key features:
- 1000+ source files
- Multiple code generators (CSS, HTML, IDL)
- External dependencies (ANGLE, SDL3, Skia)
- IPC compilation
- Platform-specific configuration

## Service CMakeLists.txt Pattern

**Template structure:**
```cmake
# 1. Disable Qt auto-tools (if not needed)
set(CMAKE_AUTOMOC OFF)

# 2. Source files
set(SOURCES ...)
set(GENERATED_SOURCES ...)

# 3. Service library (for testing)
add_library(servicelib STATIC ${SOURCES} ${GENERATED_SOURCES})

# 4. Main executable
add_executable(Service main.cpp)

# 5. Test executables
add_executable(TestService ...)

# 6. Include directories
target_include_directories(servicelib
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..
)

# 7. Linking
target_link_libraries(Service PRIVATE servicelib)
target_link_libraries(servicelib PUBLIC LibCore LibIPC ...)

# 8. IPC compilation
compile_ipc(Service.ipc ServiceEndpoint.h)
ladybird_generated_sources(servicelib)

# 9. Installation (optional)
install(TARGETS Service RUNTIME DESTINATION bin)
```

**Example: RequestServer**

Location: `Services/RequestServer/CMakeLists.txt`

Key features:
- Static library `requestserverservice`
- Main executable `RequestServer`
- Test executables
- External dependency (CURL)
- Platform-specific linking (Solaris, Haiku)
- IPC endpoints

## UI CMakeLists.txt Pattern

**Qt UI:**

Location: `UI/Qt/CMakeLists.txt`

Key features:
- Qt auto-tools enabled (AUTOMOC, AUTORCC, AUTOUIC)
- Qt resource compilation (.qrc files)
- Platform-specific executables (bundle on macOS)
- Icon generation

**AppKit UI:**

Location: `UI/AppKit/main.mm`

Key features:
- Objective-C++ source files
- macOS frameworks (AppKit, Cocoa)
- Application bundle structure

## CMake Utility Modules

### code_generators.cmake

**Functions:**
- `compile_ipc(source output)` - Compile IPC definitions
- `embed_as_string(...)` - Embed files as C++ strings
- `generate_clang_module_map(...)` - Generate Clang module maps for Swift

### utils.cmake

**Functions:**
- `ladybird_generated_sources(target)` - Mark sources as generated
- `invoke_cpp_generator(...)` - Run C++ code generators
- `invoke_py_generator(...)` - Run Python code generators
- `download_file(url path)` - Download files with caching
- `add_lagom_library_install_rules(...)` - Configure library installation

### lagom_compile_options.cmake

**Functionality:**
- Set C++23 standard
- Configure warning flags
- Platform-specific flags
- Optimization levels

### sanitizers.cmake

**Options:**
- `ENABLE_ADDRESS_SANITIZER` - ASAN
- `ENABLE_UNDEFINED_SANITIZER` - UBSAN
- `ENABLE_MEMORY_SANITIZER` - MSAN (requires special builds)
- `ENABLE_THREAD_SANITIZER` - TSAN

### libweb_generators.cmake

**Functions:**
- `generate_css_implementation()` - CSS property codegen
- `generate_html_implementation()` - HTML element codegen
- `generate_js_bindings(target)` - JavaScript/WebIDL bindings

## Build Directory Structure

After building with preset "Release":

```
Build/
├── release/                                # Release preset build output
│   ├── build.ninja                         # Ninja build file
│   ├── CMakeCache.txt                      # CMake configuration cache
│   ├── compile_commands.json               # Compilation database
│   ├── bin/                                # Executables
│   │   ├── Ladybird                        # Main browser
│   │   ├── WebContent                      # WebContent process
│   │   ├── RequestServer                   # Request server
│   │   └── ...
│   ├── lib/                                # Libraries
│   │   ├── libweb.a
│   │   ├── libjs.a
│   │   └── ...
│   └── Libraries/                          # Per-library build files
├── vcpkg/                                  # vcpkg installation
│   ├── vcpkg                               # vcpkg binary
│   ├── installed/                          # Installed packages
│   ├── buildtrees/                         # Build trees
│   ├── packages/                           # Built packages
│   └── downloads/                          # Downloaded sources
└── caches/                                 # Build caches
    └── vcpkg-binary-cache/                 # Binary package cache
```

## Code Generation Flow

1. **IPC Endpoints:**
   - Input: `Service.ipc`
   - Generator: `IPCCompiler`
   - Output: `ServiceEndpoint.h`
   - Used by: Services for IPC communication

2. **CSS Properties:**
   - Input: CSS spec definitions
   - Generator: Python scripts
   - Output: `PropertyID.cpp`, `Enums.cpp`, etc.
   - Used by: LibWeb for CSS parsing/rendering

3. **HTML Elements:**
   - Input: HTML spec definitions
   - Generator: Python scripts
   - Output: Element factory code
   - Used by: LibWeb for HTML parsing

4. **Web IDL:**
   - Input: `.idl` files
   - Generator: `BindingGenerator`
   - Output: JavaScript binding code
   - Used by: LibWeb/LibJS for Web APIs

## Dependency Resolution Order

1. **AK** (Application Kit) - Base data structures
2. **LibCore** - Core OS abstraction (depends on AK)
3. **LibIPC** - Inter-process communication (depends on LibCore)
4. **LibGfx** - Graphics primitives (depends on LibCore)
5. **LibJS** - JavaScript engine (depends on LibGfx, LibCrypto)
6. **LibWeb** - Web engine (depends on LibJS, LibGfx, many others)
7. **Services** - Browser processes (depend on LibWeb, LibIPC)
8. **UI** - User interfaces (depend on Services, LibWeb)

## Build Targets

**Common targets:**
- `all` - Build everything
- `Ladybird` - Main browser executable
- `LibWeb` - Web engine library
- `LibJS` - JavaScript engine library
- `WebContent` - WebContent process
- `RequestServer` - Network request server
- `test` - All test executables
- `install` - Install binaries and data

**Custom targets:**
- `lint-shell-scripts` - Lint shell scripts
- `check-style` - Run style checker

## Build Performance Considerations

1. **Parallel builds:** Use `-j$(nproc)` with ninja
2. **ccache:** Automatically enabled if installed
3. **vcpkg binary cache:** Speeds up dependency rebuilds
4. **Incremental builds:** Only rebuild changed files
5. **Preset selection:** Debug builds faster than Release
6. **Target selection:** Build specific targets instead of `all`

## Best Practices

1. **Always use presets** instead of manual CMake configuration
2. **Use ladybird.py wrapper** instead of direct CMake/Ninja
3. **Separate library and executable** for testability
4. **Mark generated sources** with `ladybird_generated_sources()`
5. **Use PUBLIC/PRIVATE visibility** correctly in `target_link_libraries()`
6. **Platform-specific code** in conditionals, not separate files when possible
7. **Optional dependencies** should gracefully degrade
8. **Test executables** should link library, not duplicate sources
