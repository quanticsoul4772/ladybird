#!/usr/bin/env bash
# Diagnose common Ladybird build failures

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LADYBIRD_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "============================================"
echo "Ladybird Build Failure Diagnostic Tool"
echo "============================================"
echo

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

check_fail() {
    echo -e "${RED}✗${NC} $1"
}

check_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Check CMake version
echo "Checking CMake..."
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    REQUIRED_VERSION="3.25.0"

    if [ "$CMAKE_VERSION" != "unknown" ]; then
        if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$CMAKE_VERSION" | sort -V | head -n1)" = "$REQUIRED_VERSION" ]; then
            check_pass "CMake version $CMAKE_VERSION (>= 3.25 required)"
        else
            check_fail "CMake version $CMAKE_VERSION is too old (>= 3.25 required)"
            echo "  Install: sudo apt install cmake (Linux) or brew install cmake (macOS)"
        fi
    fi
else
    check_fail "CMake not found"
    echo "  Install: sudo apt install cmake (Linux) or brew install cmake (macOS)"
fi
echo

# Check Ninja
echo "Checking Ninja..."
if command -v ninja &> /dev/null; then
    NINJA_VERSION=$(ninja --version 2>/dev/null || echo "unknown")
    check_pass "Ninja found (version $NINJA_VERSION)"
else
    check_warn "Ninja not found (vcpkg will download it)"
    echo "  Or install: sudo apt install ninja-build (Linux) or brew install ninja (macOS)"
fi
echo

# Check compilers
echo "Checking compilers..."
if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -n1 | grep -oP '\d+\.\d+' | head -n1 || echo "unknown")
    if [ "$GCC_VERSION" != "unknown" ]; then
        MAJOR_VERSION=$(echo "$GCC_VERSION" | cut -d. -f1)
        if [ "$MAJOR_VERSION" -ge 14 ]; then
            check_pass "GCC version $GCC_VERSION (gcc-14+ required for C++23)"
        else
            check_warn "GCC version $GCC_VERSION is too old (gcc-14+ required)"
            echo "  Install: sudo apt install gcc-14 g++-14"
        fi
    fi
else
    check_warn "GCC not found"
fi

if command -v clang &> /dev/null; then
    CLANG_VERSION=$(clang --version | head -n1 | grep -oP '\d+\.\d+' | head -n1 || echo "unknown")
    if [ "$CLANG_VERSION" != "unknown" ]; then
        MAJOR_VERSION=$(echo "$CLANG_VERSION" | cut -d. -f1)
        if [ "$MAJOR_VERSION" -ge 20 ]; then
            check_pass "Clang version $CLANG_VERSION (clang-20+ required for C++23)"
        else
            check_warn "Clang version $CLANG_VERSION is too old (clang-20+ required)"
            echo "  Install: sudo apt install clang-20"
        fi
    fi
else
    check_warn "Clang not found"
fi
echo

# Check Python
echo "Checking Python..."
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    check_pass "Python3 found (version $PYTHON_VERSION)"
else
    check_fail "Python3 not found (required for build scripts)"
    echo "  Install: sudo apt install python3"
fi
echo

# Check vcpkg
echo "Checking vcpkg..."
VCPKG_DIR="${LADYBIRD_ROOT}/Build/vcpkg"
if [ -d "$VCPKG_DIR" ]; then
    if [ -f "${VCPKG_DIR}/vcpkg" ] || [ -f "${VCPKG_DIR}/vcpkg.exe" ]; then
        check_pass "vcpkg found in Build/vcpkg"
    else
        check_warn "vcpkg directory exists but vcpkg binary not found"
        echo "  Rebuild: ./Meta/ladybird.py vcpkg"
    fi
else
    check_warn "vcpkg not found in Build/vcpkg"
    echo "  Bootstrap: ./Meta/ladybird.py vcpkg"
fi
echo

# Check build directories
echo "Checking build directories..."
for preset in "release" "debug" "sanitizers"; do
    BUILD_DIR="${LADYBIRD_ROOT}/Build/${preset}"
    if [ -d "$BUILD_DIR" ]; then
        if [ -f "${BUILD_DIR}/build.ninja" ]; then
            check_pass "Build directory exists: Build/${preset}"
        else
            check_warn "Build directory exists but not configured: Build/${preset}"
            echo "  Configure: ./Meta/ladybird.py build --preset $(echo $preset | sed 's/release/Release/;s/debug/Debug/;s/sanitizers/Sanitizer/')"
        fi
    fi
done
echo

# Check disk space
echo "Checking disk space..."
if command -v df &> /dev/null; then
    AVAILABLE_GB=$(df -BG "${LADYBIRD_ROOT}" | tail -n1 | awk '{print $4}' | sed 's/G//')
    if [ "$AVAILABLE_GB" -gt 20 ]; then
        check_pass "Available disk space: ${AVAILABLE_GB}GB"
    elif [ "$AVAILABLE_GB" -gt 10 ]; then
        check_warn "Available disk space: ${AVAILABLE_GB}GB (20GB+ recommended)"
    else
        check_fail "Low disk space: ${AVAILABLE_GB}GB (20GB+ recommended)"
        echo "  Clean: ./Meta/ladybird.py clean or rm -rf Build/"
    fi
fi
echo

# Check memory
echo "Checking system memory..."
if [ -f /proc/meminfo ]; then
    TOTAL_MEM_GB=$(grep MemTotal /proc/meminfo | awk '{printf "%.0f", $2/1024/1024}')
    if [ "$TOTAL_MEM_GB" -ge 16 ]; then
        check_pass "System memory: ${TOTAL_MEM_GB}GB"
    elif [ "$TOTAL_MEM_GB" -ge 8 ]; then
        check_warn "System memory: ${TOTAL_MEM_GB}GB (16GB+ recommended)"
        echo "  Use fewer parallel jobs: ./Meta/ladybird.py build -j 2"
    else
        check_fail "Low system memory: ${TOTAL_MEM_GB}GB (8GB minimum, 16GB+ recommended)"
        echo "  Use fewer parallel jobs: ./Meta/ladybird.py build -j 1"
    fi
elif command -v sysctl &> /dev/null; then
    # macOS
    TOTAL_MEM_GB=$(sysctl -n hw.memsize | awk '{printf "%.0f", $1/1024/1024/1024}')
    if [ "$TOTAL_MEM_GB" -ge 16 ]; then
        check_pass "System memory: ${TOTAL_MEM_GB}GB"
    else
        check_warn "System memory: ${TOTAL_MEM_GB}GB (16GB+ recommended)"
    fi
fi
echo

# Check for common issues in Build directory
echo "Checking for common build issues..."

# Check for permission issues
if [ -d "${LADYBIRD_ROOT}/Build" ]; then
    if [ ! -w "${LADYBIRD_ROOT}/Build" ]; then
        check_fail "Build directory is not writable"
        echo "  Fix: sudo chown -R \$(whoami):\$(whoami) Build/"
    else
        check_pass "Build directory is writable"
    fi
fi

# Check for stale CMakeCache
for preset in "release" "debug" "sanitizers"; do
    BUILD_DIR="${LADYBIRD_ROOT}/Build/${preset}"
    if [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        AGE_DAYS=$(find "${BUILD_DIR}/CMakeCache.txt" -mtime +7 2>/dev/null | wc -l)
        if [ "$AGE_DAYS" -gt 0 ]; then
            check_warn "CMakeCache.txt is more than 7 days old in Build/${preset}"
            echo "  Consider: ./Meta/ladybird.py rebuild --preset $(echo $preset | sed 's/release/Release/;s/debug/Debug/;s/sanitizers/Sanitizer/')"
        fi
    fi
done
echo

# Check for running out of space during build
echo "Checking build directory sizes..."
if [ -d "${LADYBIRD_ROOT}/Build" ]; then
    BUILD_SIZE_GB=$(du -sh "${LADYBIRD_ROOT}/Build" 2>/dev/null | awk '{print $1}' || echo "unknown")
    echo "  Build directory size: $BUILD_SIZE_GB"

    if [ -d "${LADYBIRD_ROOT}/Build/vcpkg" ]; then
        VCPKG_SIZE_GB=$(du -sh "${LADYBIRD_ROOT}/Build/vcpkg" 2>/dev/null | awk '{print $1}' || echo "unknown")
        echo "  vcpkg directory size: $VCPKG_SIZE_GB"
    fi
fi
echo

# Check for common dependency issues
echo "Checking common dependencies..."
DEPS_MISSING=0

check_dep() {
    local pkg=$1
    local name=$2
    local install_cmd=$3

    if dpkg -l | grep -q "^ii  $pkg" 2>/dev/null || command -v "$name" &> /dev/null; then
        check_pass "$name found"
    else
        check_warn "$name not found (may be optional)"
        [ -n "$install_cmd" ] && echo "  Install: $install_cmd"
        DEPS_MISSING=1
    fi
}

if [ "$(uname)" = "Linux" ]; then
    check_dep "libssl-dev" "OpenSSL development" "sudo apt install libssl-dev"
    check_dep "libcurl4-openssl-dev" "CURL development" "sudo apt install libcurl4-openssl-dev"
    check_dep "libicu-dev" "ICU development" "sudo apt install libicu-dev"
    check_dep "libxml2-dev" "LibXML2 development" "sudo apt install libxml2-dev"
fi
echo

# Suggest next steps
echo "============================================"
echo "Suggested Actions:"
echo "============================================"
echo

if [ $DEPS_MISSING -eq 0 ]; then
    echo "1. Try a clean rebuild:"
    echo "   ./Meta/ladybird.py clean"
    echo "   ./Meta/ladybird.py build"
    echo
    echo "2. If that fails, try rebuilding vcpkg:"
    echo "   rm -rf Build/vcpkg Build/caches"
    echo "   ./Meta/ladybird.py vcpkg"
    echo "   ./Meta/ladybird.py build"
    echo
    echo "3. Check build logs for specific errors:"
    echo "   ./Meta/ladybird.py build 2>&1 | tee build.log"
    echo "   grep -i error build.log"
    echo
    echo "4. Try a different preset:"
    echo "   ./Meta/ladybird.py build --preset Debug  # Faster to build"
    echo
    echo "5. Use fewer parallel jobs if memory-constrained:"
    echo "   ./Meta/ladybird.py build -j 2"
else
    echo "1. Install missing dependencies (see warnings above)"
    echo
    echo "2. Then try building:"
    echo "   ./Meta/ladybird.py build"
fi
echo
