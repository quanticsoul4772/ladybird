# Common Build Errors and Solutions

Comprehensive guide to diagnosing and fixing common Ladybird build errors.

## Quick Diagnosis

Run the diagnostic script:
```bash
./.claude/skills/cmake-build-system/scripts/diagnose_build_failure.sh
```

Or check dependencies:
```bash
./.claude/skills/cmake-build-system/scripts/check_dependencies.sh
```

---

## CMake Configuration Errors

### Error: CMake version too old

**Symptom:**
```
CMake 3.22 or higher is required.  You are running version 3.20.0
```

**Cause:** CMake version < 3.25

**Solution:**
```bash
# Ubuntu/Debian
sudo apt remove cmake
wget https://github.com/Kitware/CMake/releases/download/v3.28.0/cmake-3.28.0-linux-x86_64.tar.gz
sudo tar xzf cmake-3.28.0-linux-x86_64.tar.gz -C /opt/
sudo ln -s /opt/cmake-3.28.0-linux-x86_64/bin/cmake /usr/local/bin/cmake

# Or use snap
sudo snap install cmake --classic

# macOS
brew install cmake

# Verify
cmake --version
```

---

### Error: CMAKE_TOOLCHAIN_FILE not set

**Symptom:**
```
CMake Error: CMAKE_TOOLCHAIN_FILE is not set to vcpkg toolchain
```

**Cause:** vcpkg not bootstrapped

**Solution:**
```bash
# Bootstrap vcpkg
./Meta/ladybird.py vcpkg

# Or manual vcpkg setup
export VCPKG_ROOT="$(pwd)/Build/vcpkg"
git clone https://github.com/Microsoft/vcpkg.git Build/vcpkg
Build/vcpkg/bootstrap-vcpkg.sh
```

---

### Error: Ninja not found

**Symptom:**
```
CMake Error: Unable to find a build program corresponding to "Ninja"
```

**Cause:** Ninja build tool not installed

**Solution:**
```bash
# vcpkg will download Ninja, but you can install manually:

# Ubuntu/Debian
sudo apt install ninja-build

# macOS
brew install ninja

# Or let vcpkg handle it
./Meta/ladybird.py vcpkg
```

---

### Error: Could not find toolchain file

**Symptom:**
```
CMake Error: Could not find CMAKE_TOOLCHAIN_FILE: .../vcpkg.cmake
```

**Cause:** vcpkg not in expected location

**Solution:**
```bash
# Clean and rebuild vcpkg
rm -rf Build/vcpkg Build/caches
./Meta/ladybird.py vcpkg
./Meta/ladybird.py build
```

---

## Compiler Errors

### Error: Compiler does not support C++23

**Symptom:**
```
CMake Error: The C++ compiler does not support std=c++23
```

**Cause:** Compiler version too old (need gcc-14+ or clang-20+)

**Solution:**
```bash
# Check compiler version
gcc --version
clang --version

# Install gcc-14
sudo apt install gcc-14 g++-14

# Or clang-20
sudo apt install clang-20

# Build with specific compiler
./Meta/ladybird.py build --cc gcc-14 --cxx g++-14
```

---

### Error: Compiler crashed / killed

**Symptom:**
```
c++: fatal error: Killed signal terminated program cc1plus
```

**Cause:** Out of memory during compilation

**Solution:**
```bash
# Reduce parallel jobs
./Meta/ladybird.py build -j 2  # or -j 1

# Use Release preset (smaller binaries)
./Meta/ladybird.py build --preset Release

# Increase swap space
sudo dd if=/dev/zero of=/swapfile bs=1G count=8
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

---

### Error: Unknown type name (C++23 feature)

**Symptom:**
```
error: 'expected' is not a member of 'std'
error: unknown type name 'std::expected'
```

**Cause:** Standard library doesn't support C++23

**Solution:**
```bash
# Ensure matching stdlib version
# GCC 14 includes libstdc++14
# Clang 20 needs libc++19+

# Check which stdlib is being used
echo | gcc-14 -E -Wp,-v -x c++ - 2>&1 | grep "include"

# Install correct versions
sudo apt install gcc-14 g++-14 libstdc++-14-dev
```

---

## Dependency Errors

### Error: Could not find package Qt6

**Symptom:**
```
CMake Error at CMakeLists.txt:69 (find_package):
  Could not find a package configuration file provided by "Qt6"
```

**Cause:** Qt6 not installed (if ENABLE_QT=ON)

**Solution:**
```bash
# Option 1: Install Qt6
sudo apt install qt6-base-dev qt6-tools-dev  # Linux
brew install qt@6                             # macOS

# Option 2: Disable Qt
cmake --preset Release -DENABLE_QT=OFF
```

---

### Error: Could not find OpenSSL

**Symptom:**
```
CMake Error: Could not find a package configuration file provided by "OpenSSL"
```

**Cause:** OpenSSL development files not installed

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libssl-dev

# macOS
brew install openssl@3

# If still not found, set path
export OPENSSL_ROOT_DIR=/usr/local/opt/openssl@3  # macOS
```

---

### Error: Could not find ICU

**Symptom:**
```
CMake Error: Could not find ICU (missing: ICU_INCLUDE_DIR ICU_LIBRARY)
```

**Cause:** ICU development files not installed

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libicu-dev

# macOS
brew install icu4c
export ICU_ROOT=/usr/local/opt/icu4c  # or /opt/homebrew/opt/icu4c
```

---

### Error: CURL library not found

**Symptom:**
```
CMake Error: Could not find CURL (missing: CURL_LIBRARY CURL_INCLUDE_DIR)
```

**Cause:** CURL development files not installed

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev

# macOS
brew install curl
```

---

## vcpkg Errors

### Error: vcpkg install failed

**Symptom:**
```
error: failed to install package xyz
```

**Cause:** vcpkg package installation failure

**Solution:**
```bash
# Clean vcpkg completely
rm -rf Build/vcpkg Build/caches

# Rebuild vcpkg and dependencies
./Meta/ladybird.py vcpkg

# Check vcpkg logs
cat Build/vcpkg/buildtrees/xyz/build-*.log

# Try again
./Meta/ladybird.py build
```

---

### Error: vcpkg binary sources not accessible

**Symptom:**
```
warning: Failed to access vcpkg binary cache
```

**Cause:** Network issues or permissions

**Solution:**
```bash
# Use local cache only
export VCPKG_BINARY_SOURCES="clear;files,$(pwd)/Build/caches/vcpkg-binary-cache,readwrite"

# Or disable binary cache
export VCPKG_BINARY_SOURCES="clear"

./Meta/ladybird.py build
```

---

### Error: vcpkg out of disk space

**Symptom:**
```
error: failed to extract package: No space left on device
```

**Cause:** Insufficient disk space

**Solution:**
```bash
# Check available space
df -h

# Clean vcpkg caches
rm -rf Build/caches

# Clean build directories
./Meta/ladybird.py clean

# If still insufficient, clean vcpkg buildtrees
rm -rf Build/vcpkg/buildtrees
rm -rf Build/vcpkg/downloads
```

---

## Linker Errors

### Error: Undefined reference to symbol

**Symptom:**
```
undefined reference to `YourClass::method()'
```

**Cause:**
1. Missing source file in SOURCES list
2. Missing library in target_link_libraries
3. Wrong link order

**Solution:**
```cmake
# Check CMakeLists.txt

# 1. Ensure source file is listed
set(SOURCES
    YourClass.cpp  # Make sure this is included
)

# 2. Link required library
target_link_libraries(YourTarget
    PRIVATE LibRequired
)

# 3. Check link order (dependencies last)
target_link_libraries(YourTarget
    PRIVATE
        LibA        # Uses LibB
        LibB        # Put after LibA
)
```

---

### Error: Multiple definition of symbol

**Symptom:**
```
error: multiple definition of `YourClass::method()'
```

**Cause:**
1. Template/inline function defined in .cpp
2. Source file included multiple times
3. Duplicate source in multiple targets

**Solution:**
```cpp
// Option 1: Move to header if template/inline
template<typename T>
inline void method() { /* implementation */ }

// Option 2: Ensure proper include guards
#pragma once

// Option 3: Check CMakeLists.txt for duplicates
# Remove duplicate entries in SOURCES
```

---

## Build Errors

### Error: Permission denied on Build directory

**Symptom:**
```
CMake Error: Cannot create directory Build/release: Permission denied
```

**Cause:** Build directory owned by root

**Solution:**
```bash
# Never run ladybird.py as root!

# Fix permissions
sudo chown -R $(whoami):$(whoami) Build/

# Rebuild
./Meta/ladybird.py clean
./Meta/ladybird.py build
```

---

### Error: No rule to make target

**Symptom:**
```
ninja: error: '...', needed by '...', missing and no known rule to make it
```

**Cause:** Stale build state or missing generated file

**Solution:**
```bash
# Clean and rebuild
./Meta/ladybird.py rebuild

# Or clean specific preset
./Meta/ladybird.py clean --preset Release
./Meta/ladybird.py build --preset Release
```

---

### Error: File not found during build

**Symptom:**
```
fatal error: SomeHeader.h: No such file or directory
```

**Cause:**
1. Missing include directory
2. Header not generated yet
3. Typo in include path

**Solution:**
```cmake
# Check CMakeLists.txt

# Ensure include directories are set
target_include_directories(YourTarget
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}  # For generated files
)

# If generated file, ensure generation happens first
ladybird_generated_sources(YourTarget)
```

---

## Generated File Errors

### Error: Generated header not found

**Symptom:**
```
fatal error: 'YourServiceEndpoint.h' file not found
```

**Cause:** IPC file not compiled before use

**Solution:**
```cmake
# Ensure compile_ipc is called BEFORE using header
compile_ipc(YourService.ipc YourServiceEndpoint.h)

set(GENERATED_SOURCES
    YourServiceEndpoint.h
)

# Mark as generated
ladybird_generated_sources(yourtarget)
```

---

### Error: Generated source out of date

**Symptom:**
```
error: ...Endpoint.h is out of date
```

**Cause:** Stale generated file

**Solution:**
```bash
# Clean generated files
rm -rf Build/release/Libraries/
rm -rf Build/release/Services/

# Rebuild
./Meta/ladybird.py build
```

---

## Runtime Errors

### Error: Shared library not found

**Symptom:**
```
error while loading shared libraries: libweb.so: cannot open shared object file
```

**Cause:** Library not in library path

**Solution:**
```bash
# Add build directory to LD_LIBRARY_PATH
export LD_LIBRARY_PATH="$(pwd)/Build/release/lib:$LD_LIBRARY_PATH"

# Or use static linking (Distribution preset)
./Meta/ladybird.py build --preset Distribution
```

---

### Error: Cannot execute binary

**Symptom:**
```
cannot execute binary file: Exec format error
```

**Cause:** Wrong architecture or corrupted binary

**Solution:**
```bash
# Check architecture
file Build/release/bin/Ladybird
uname -m

# Clean rebuild if mismatch
./Meta/ladybird.py rebuild
```

---

## Test Errors

### Error: Tests not found

**Symptom:**
```
No tests were found!!!
```

**Cause:** Tests not built or not in test registry

**Solution:**
```bash
# Ensure tests are built
./Meta/ladybird.py build

# Run tests
./Meta/ladybird.py test

# Or run CTest directly
cd Build/release && ctest
```

---

### Error: Test timeout

**Symptom:**
```
***Timeout
```

**Cause:** Test taking too long

**Solution:**
```bash
# Increase timeout
export CTEST_OUTPUT_ON_FAILURE=1
ctest --timeout 300  # 5 minutes

# Or run specific test
Build/release/bin/TestYourFeature
```

---

## Platform-Specific Errors

### Linux: Missing system libraries

**Symptom:**
```
/usr/bin/ld: cannot find -lpthread
```

**Solution:**
```bash
# Install build essentials
sudo apt install build-essential

# Install common development packages
sudo apt install \
    libssl-dev \
    libcurl4-openssl-dev \
    libicu-dev \
    libxml2-dev \
    pkg-config
```

---

### macOS: Xcode not found

**Symptom:**
```
xcrun: error: unable to find utility "clang"
```

**Solution:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Accept license
sudo xcodebuild -license accept

# Verify
xcode-select -p
```

---

### macOS: Library not found in Homebrew

**Symptom:**
```
library not found for -licu
```

**Solution:**
```bash
# Install via Homebrew
brew install icu4c

# Link libraries
brew link icu4c --force

# Set pkg-config path
export PKG_CONFIG_PATH="/opt/homebrew/opt/icu4c/lib/pkgconfig:$PKG_CONFIG_PATH"
```

---

### Windows: Visual Studio not found

**Symptom:**
```
ladybird.py must be run from a Visual Studio enabled environment
```

**Solution:**
1. Open "Developer Command Prompt for VS 2022"
2. Or run: `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"`
3. Then run build: `./Meta/ladybird.py build --preset Windows_Experimental`

---

## Recovery Procedures

### Nuclear Option: Complete Clean

When all else fails:

```bash
# 1. Remove everything
rm -rf Build/

# 2. Clean git (if applicable)
git clean -fdx

# 3. Re-bootstrap
./Meta/ladybird.py vcpkg

# 4. Build from scratch
./Meta/ladybird.py build

# 5. If still failing, check dependencies
./.claude/skills/cmake-build-system/scripts/check_dependencies.sh
```

---

## Getting Help

If errors persist:

1. **Run diagnostic script:**
   ```bash
   ./.claude/skills/cmake-build-system/scripts/diagnose_build_failure.sh
   ```

2. **Capture build log:**
   ```bash
   ./Meta/ladybird.py build 2>&1 | tee build.log
   ```

3. **Check CMake error log:**
   ```bash
   cat Build/release/CMakeFiles/CMakeError.log
   ```

4. **Verbose build:**
   ```bash
   ./Meta/ladybird.py build -- -v
   ```

5. **Check vcpkg logs:**
   ```bash
   cat Build/vcpkg/buildtrees/<package>/build-*.log
   ```

6. **Search for similar issues:**
   - GitHub Issues: https://github.com/LadybirdBrowser/ladybird/issues
   - Discord: https://discord.gg/ladybird

7. **Provide information when asking for help:**
   - OS and version
   - Compiler and version
   - Full error message
   - Build command used
   - Output of diagnostic script
