# Compiler Support Reference

Ladybird requires modern C++23 support. This document lists supported compilers and versions.

## Minimum Requirements

### C++ Standard
- **C++23** (ISO/IEC 14882:2023)
- Full C++23 feature set required
- No compiler extensions used (standard compliance enforced)

### Required Compiler Versions

| Compiler | Minimum Version | Recommended Version | Notes |
|----------|----------------|---------------------|-------|
| GCC | 14.0 | 14.2+ | Full C++23 support from 14.0 |
| Clang | 20.0 | 20.0+ | Full C++23 support from 20.0 |
| Apple Clang | 16.0 | 16.0+ | Xcode 16+ (macOS) |
| Clang-CL | 20.0 | 20.0+ | Windows experimental support |
| MSVC | Not supported | - | Use Clang-CL instead |

## Platform-Specific Compiler Recommendations

### Linux

#### Ubuntu/Debian

**GCC 14:**
```bash
# Ubuntu 24.04+
sudo apt install gcc-14 g++-14

# Set as default
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
```

**Clang 20:**
```bash
# Add LLVM repository
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-20 main"

# Install
sudo apt install clang-20 clang++-20

# Set as default
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100
```

**Specify compiler explicitly:**
```bash
./Meta/ladybird.py build --cc gcc-14 --cxx g++-14
# Or
./Meta/ladybird.py build --cc clang-20 --cxx clang++-20
```

#### Fedora/RHEL

```bash
# GCC 14
sudo dnf install gcc-14 gcc-c++-14

# Clang 20
sudo dnf install clang-20
```

#### Arch Linux

```bash
# GCC (usually latest)
sudo pacman -S gcc

# Clang
sudo pacman -S clang
```

### macOS

#### Xcode Command Line Tools

**Required:** Xcode 16.0+ (includes Apple Clang 16.0+)

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Verify version
clang --version
```

#### Homebrew Compilers

Alternative to Xcode (optional):

```bash
# GCC 14
brew install gcc@14

# LLVM/Clang 20
brew install llvm@20

# Use Homebrew compilers
./Meta/ladybird.py build --cc /opt/homebrew/bin/gcc-14 --cxx /opt/homebrew/bin/g++-14
```

### Windows (Experimental)

#### Clang-CL (Recommended)

**Requirements:**
- Visual Studio 2022 version 17.10+
- Clang/LLVM toolchain from Visual Studio Installer
- Must run from Visual Studio Developer Command Prompt

**Installation:**
1. Open Visual Studio Installer
2. Modify Visual Studio 2022
3. Individual Components → Compilers, build tools, and runtimes
4. Select: "C++ Clang Compiler for Windows"
5. Install

**Build from VS Developer Command Prompt:**
```powershell
./Meta/ladybird.py build --preset Windows_Experimental
```

#### MSVC
**Not supported.** Use Clang-CL instead.

### FreeBSD

```bash
# Clang (system default, usually recent)
pkg install llvm20

# GCC
pkg install gcc14
```

## C++23 Features Used

Ladybird uses the following C++23 features:

### Language Features
- Deducing `this` (P0847R7)
- `if consteval` (P1938R3)
- Multidimensional subscript operator (P2128R6)
- `static operator()` (P1169R4)
- Extended floating-point types (P1467R9)
- `#elifdef` and `#elifndef` (P2334R1)
- Literal suffix for `size_t` (P0330R8)
- `std::unreachable()` (P0627R6)

### Library Features
- `std::expected` (P0323R12)
- `std::mdspan` (P0009R18)
- `std::print` and `std::println` (P2093R14)
- `std::flat_map` and `std::flat_set` (P0429R9)
- Ranges improvements (P2321R2, P2322R6)
- `std::to_underlying` (P1682R3)
- `std::invoke_r` (P2136R3)
- `std::byteswap` (P1272R4)

**Note:** Some C++23 features may be implemented in Ladybird's own AK (Application Kit) library if compiler/stdlib support is insufficient.

## Compiler Flags

### Common Flags (All Compilers)

Set in `Meta/CMake/lagom_compile_options.cmake`:

```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # No compiler extensions

# Warning flags
-Wall -Wextra -Wpedantic
-Wno-expansion-to-defined
-Wno-user-defined-literals
```

### GCC-Specific Flags

```cmake
# Enable concepts diagnostics
-fconcepts-diagnostics-depth=2

# Improve error messages
-fdiagnostics-color=always
```

### Clang-Specific Flags

```cmake
# Improve error messages
-fcolor-diagnostics

# Enable all warnings
-Weverything (selectively disabled)
```

### Platform-Specific Flags

**Linux:**
```cmake
-pthread
```

**macOS:**
```cmake
-mmacosx-version-min=14.0
-stdlib=libc++
```

**Windows (Clang-CL):**
```cmake
/std:c++latest
/EHsc  # Exception handling
```

## Checking Compiler Support

### Verify C++23 Support

**Test program:**
```cpp
#include <expected>
#include <print>
#include <version>

int main() {
    #ifndef __cpp_lib_expected
    #error "std::expected not supported"
    #endif

    std::expected<int, const char*> result = 42;
    std::println("Value: {}", result.value());
    return 0;
}
```

**Compile:**
```bash
# GCC
g++-14 -std=c++23 test.cpp -o test

# Clang
clang++-20 -std=c++23 test.cpp -o test
```

### Check Feature Macros

```cpp
#include <version>
#include <iostream>

int main() {
    std::cout << "__cplusplus: " << __cplusplus << "\n";

    #ifdef __cpp_lib_expected
    std::cout << "std::expected: " << __cpp_lib_expected << "\n";
    #endif

    #ifdef __cpp_lib_print
    std::cout << "std::print: " << __cpp_lib_print << "\n";
    #endif

    return 0;
}
```

Expected `__cplusplus` value: `202302L` (C++23)

## Compiler Selection in Build

### Default Selection

`Meta/find_compiler.py` automatically selects:
1. Check environment: `CC` and `CXX`
2. Search for versioned compilers: `gcc-14`, `clang-20`, etc.
3. Check unversioned: `gcc`, `clang`
4. Platform defaults:
   - Linux: GCC
   - macOS: Clang (Xcode)
   - Windows: Clang-CL

### Manual Selection

**Via ladybird.py:**
```bash
./Meta/ladybird.py build --cc gcc-14 --cxx g++-14
./Meta/ladybird.py build --cc clang-20 --cxx clang++-20
```

**Via environment variables:**
```bash
export CC=gcc-14
export CXX=g++-14
./Meta/ladybird.py build
```

**Via CMake cache:**
```bash
cmake --preset Release -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
```

## Cross-Compilation

### Cross-Compiling for ARM on x86_64

**Install cross-compiler:**
```bash
sudo apt install gcc-14-aarch64-linux-gnu g++-14-aarch64-linux-gnu
```

**Build:**
```bash
cmake --preset Release \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc-14 \
  -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++-14 \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64
```

## Compiler-Specific Issues

### GCC

**Known Issues:**
- GCC 13 and earlier: Incomplete C++23 support (not supported)
- Some C++23 library features may be missing even in GCC 14
- Use libstdc++ version 14+ (included with GCC 14)

**Workarounds:**
- Some features implemented in AK library
- Conditional compilation based on feature test macros

### Clang

**Known Issues:**
- Clang 19 and earlier: Incomplete C++23 support (not supported)
- Some C++23 library features depend on libc++ version
- May need libc++ 19+ for full support

**Recommended:**
- Use matching libc++ version (clang-20 with libc++-20)

### Apple Clang

**Known Issues:**
- Versioning differs from LLVM Clang
- Apple Clang 15 = LLVM Clang 16 (approximately)
- Some features lag behind upstream Clang
- Limited to macOS SDK version

**Workarounds:**
- Xcode 16+ required (Apple Clang 16)
- Use Homebrew LLVM for latest features (optional)

### Clang-CL (Windows)

**Known Issues:**
- MSVC ABI compatibility required for system libraries
- Some POSIX features unavailable
- Limited sanitizer support
- Experimental status

**Limitations:**
- Not all targets build successfully
- Some C++23 features may be unavailable
- Requires Visual Studio SDK

## Standard Library Support

### libstdc++ (GCC)

**Minimum Version:** 14.0

**Required for:**
- `std::expected`
- `std::print` / `std::println`
- C++23 ranges improvements

**Check version:**
```cpp
#include <bits/c++config.h>
#include <iostream>

int main() {
    std::cout << "__GLIBCXX__: " << __GLIBCXX__ << "\n";
}
```

### libc++ (Clang)

**Minimum Version:** 19.0

**Required for:**
- Full C++23 support
- All C++23 library features

**Check version:**
```cpp
#include <version>
#include <iostream>

int main() {
    #ifdef _LIBCPP_VERSION
    std::cout << "_LIBCPP_VERSION: " << _LIBCPP_VERSION << "\n";
    #endif
}
```

## Troubleshooting

### Error: Compiler does not support C++23

**Symptom:**
```
CMake Error: C++ compiler does not support std=c++23
```

**Solution:**
1. Check compiler version: `gcc --version` or `clang --version`
2. Install gcc-14+ or clang-20+
3. Specify compiler: `./Meta/ladybird.py build --cc gcc-14 --cxx g++-14`

### Error: C++ standard library does not support feature X

**Symptom:**
```
error: 'expected' is not a member of 'std'
```

**Solution:**
1. Ensure matching stdlib version
2. GCC: Use libstdc++14+
3. Clang: Use libc++19+
4. Update system: `sudo apt update && sudo apt upgrade`

### Error: Undefined reference to C++23 symbol

**Symptom:**
```
undefined reference to `std::println(...)'
```

**Solution:**
1. Link correct standard library version
2. Check `CMAKE_CXX_FLAGS` and linker flags
3. Ensure consistent compiler/stdlib versions

### Compiler warnings about C++23 features

**Symptom:**
```
warning: 'deducing this' is a C++23 extension
```

**Solution:**
1. Verify `-std=c++23` flag is set
2. Check `CMAKE_CXX_STANDARD` is 23
3. Ensure no accidental `-std=c++20` override

## Performance Comparison

Relative compilation speed (GCC 14 = 1.0):

| Compiler | Compile Speed | Binary Size | Runtime Speed |
|----------|---------------|-------------|---------------|
| GCC 14 | 1.0x | 1.0x | 1.0x |
| Clang 20 | 0.9x | 1.05x | 1.02x |
| Apple Clang 16 | 0.9x | 1.05x | 1.02x |
| Clang-CL 20 | 0.8x | 1.1x | 0.98x |

*Values approximate, vary by workload*

## Recommendations

**For development:**
- Linux: GCC 14 or Clang 20 (either works well)
- macOS: Apple Clang 16 (Xcode 16)
- Windows: Clang-CL 20 (experimental)

**For CI/CD:**
- Use GCC 14 for Release builds
- Use Clang 20 for Sanitizer builds
- Test on multiple compilers

**For distribution:**
- Static builds with GCC 14 recommended
- Best optimization and compatibility

**For debugging:**
- GCC 14 has excellent debugging support
- Clang 20 has better error messages
- Choose based on preference
