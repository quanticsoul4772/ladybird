# Wasmtime CMake Integration Guide

**For:** Developers modifying Wasmtime detection in CMake
**Target:** Ladybird build system
**Status:** Documentation for Phase 1c integration

## CMake Configuration

Wasmtime detection and linkage is handled in the Sentinel CMakeLists.txt file.

### Standard Detection

The build system automatically searches for Wasmtime:

```cmake
find_package(Wasmtime 38 REQUIRED)
```

This looks in standard locations:
- `/usr/local/wasmtime` (Linux/macOS default)
- `/usr/wasmtime`
- `~/wasmtime` (user-local installation)
- System `pkg-config` paths
- `CMAKE_PREFIX_PATH` environment variable

### Custom Installation Path

If Wasmtime is installed to a non-standard location:

```bash
# Set CMAKE_PREFIX_PATH before configuring
export CMAKE_PREFIX_PATH="/custom/wasmtime/path:$CMAKE_PREFIX_PATH"

# Or on command line
cmake --preset Release -DCMAKE_PREFIX_PATH="/custom/wasmtime/path"
```

### Manual Configuration

If Wasmtime detection fails, manually specify paths:

```bash
cmake --preset Release \
  -DWasmtime_LIBRARY=/path/to/libwasmtime.so \
  -DWasmtime_INCLUDE_DIR=/path/to/include
```

## Build Output

Successful Wasmtime detection shows:

```
-- Found Wasmtime: /usr/local/wasmtime
-- Wasmtime version: 38.0.3
-- Wasmtime library: /usr/local/wasmtime/lib/libwasmtime.so
-- Wasmtime include: /usr/local/wasmtime/include
```

## CMakeCache Variables

Key variables set after configuration:

| Variable | Purpose | Example |
|----------|---------|---------|
| `Wasmtime_FOUND` | Detection success | ON/OFF |
| `Wasmtime_LIBRARY` | Library path | `/usr/local/wasmtime/lib/libwasmtime.so` |
| `Wasmtime_INCLUDE_DIR` | Headers path | `/usr/local/wasmtime/include` |
| `Wasmtime_VERSION` | Installed version | `38.0.3` |
| `ENABLE_WASMTIME` | Feature enabled | ON/OFF |

Check after configuration:

```bash
grep -i wasmtime Build/release/CMakeCache.txt | head -20
```

## CMakeLists.txt Pattern

Recommended pattern for linking Wasmtime in target:

```cmake
# Find Wasmtime
find_package(Wasmtime 38)
if(Wasmtime_FOUND)
    message(STATUS "Found Wasmtime: ${Wasmtime_LIBRARY}")
    set(ENABLE_WASMTIME ON)
else()
    message(STATUS "Wasmtime not found - using stub mode")
    set(ENABLE_WASMTIME OFF)
endif()

# Link against Wasmtime if found
if(ENABLE_WASMTIME)
    target_link_libraries(MyTarget PRIVATE Wasmtime::wasmtime)
    target_compile_definitions(MyTarget PRIVATE WASMTIME_ENABLED=1)
else()
    target_compile_definitions(MyTarget PRIVATE WASMTIME_ENABLED=0)
endif()
```

## Compilation Flags

When Wasmtime is enabled:

```
-DWASMTIME_ENABLED=1  # Feature flag for conditional compilation
```

Use in code:

```cpp
#ifdef WASMTIME_ENABLED
    // Real WASM execution
    WasmExecutor executor(module_path);
    executor.execute(input);
#else
    // Stub implementation
    return stub_execute(input);
#endif
```

## Linking

Wasmtime provides two libraries:

| Library | Purpose | Static |
|---------|---------|--------|
| `libwasmtime.so` | Runtime library | Dynamic |
| `libwasmtime_c_api.a` | C API bindings | Static (bundled) |

The main library (`libwasmtime.so`) bundles the C API.

Link like this:

```cmake
target_link_libraries(MyTarget PRIVATE Wasmtime::wasmtime)
```

This automatically handles:
- Library path
- Header includes
- Required dependencies (DWARF, zstd, etc.)

## Debugging CMake Detection

If Wasmtime isn't detected:

```bash
# Check CMake verbose output
cmake --preset Release -DCMAKE_VERBOSE_MAKEFILE=ON
cmake --build Build/release --verbose 2>&1 | grep -i wasmtime

# Check FindWasmtime.cmake output
cmake --trace-expand Build/release 2>&1 | grep -i wasmtime | head -20

# Manual detection test
pkg-config --cflags --libs wasmtime
```

## Platform-Specific Notes

### Linux

Standard installation locations:
- `/usr/local/wasmtime` (sudo install)
- `~/wasmtime` (user install)
- `/opt/wasmtime` (alternative)

Dependencies (usually included in Wasmtime binary):
- `libdl.so.2` (dynamic linker)
- `libc.so.6` (C library)

### macOS

Standard installation locations:
- `/usr/local/wasmtime`
- `/opt/homebrew/opt/wasmtime` (if installed via Homebrew)
- `~/wasmtime` (user install)

Dylib path issues:
```bash
# Check dylib references
otool -L /usr/local/wasmtime/lib/libwasmtime.dylib

# Install to standard location if @rpath fails
sudo mkdir -p /usr/local/wasmtime/{lib,include}
```

## Testing Detection

After configuring CMake:

```bash
# Verify variables are set
grep "Wasmtime_LIBRARY\|Wasmtime_INCLUDE\|Wasmtime_VERSION\|ENABLE_WASMTIME" \
    Build/release/CMakeCache.txt

# Check linking works
cd Build/release
ninja -v sentinelservice 2>&1 | grep -i wasmtime
```

## Troubleshooting

### Issue: CMake says "Wasmtime not found"

**Check installation:**
```bash
ls -lh /usr/local/wasmtime/lib/libwasmtime.*
ls -lh /usr/local/wasmtime/include/wasmtime.h
```

**Verify CMake can find it:**
```bash
cmake --debug-output --preset Release 2>&1 | grep -i wasmtime
```

**Use absolute paths:**
```bash
cmake --preset Release \
  -DWasmtime_LIBRARY=/usr/local/wasmtime/lib/libwasmtime.so \
  -DWasmtime_INCLUDE_DIR=/usr/local/wasmtime/include
```

### Issue: Linking fails even though CMake found Wasmtime

Check library format:

```bash
file /usr/local/wasmtime/lib/libwasmtime.so
nm -D /usr/local/wasmtime/lib/libwasmtime.so | wc -l
```

May indicate:
- Library is for wrong platform
- Library is corrupted
- Library is a stub

Reinstall Wasmtime:
```bash
rm -rf /usr/local/wasmtime
./Services/Sentinel/Sandbox/install_wasmtime.sh
```

## Advanced: Custom FindWasmtime.cmake

If the default detection fails, create custom finder:

```cmake
# CMake/FindWasmtime.cmake
find_path(Wasmtime_INCLUDE_DIR
    NAMES wasmtime.h
    PATHS /usr/local/wasmtime/include /opt/wasmtime/include)

find_library(Wasmtime_LIBRARY
    NAMES wasmtime
    PATHS /usr/local/wasmtime/lib /opt/wasmtime/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Wasmtime
    REQUIRED_VARS Wasmtime_LIBRARY Wasmtime_INCLUDE_DIR)

if(Wasmtime_FOUND)
    add_library(Wasmtime::wasmtime SHARED IMPORTED)
    set_target_properties(Wasmtime::wasmtime PROPERTIES
        IMPORTED_LOCATION ${Wasmtime_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Wasmtime_INCLUDE_DIR})
endif()
```

Then use:
```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")
find_package(Wasmtime REQUIRED)
```

## Reference

- **Wasmtime Releases:** https://github.com/bytecodealliance/wasmtime/releases
- **Wasmtime C API:** https://github.com/bytecodealliance/wasmtime/blob/main/crates/c-api/include/wasmtime.h
- **CMake find_package:** https://cmake.org/cmake/help/latest/command/find_package.html
- **Installation Guide:** `docs/WASMTIME_INSTALLATION.md`
