#!/usr/bin/env bash
# Check and verify build dependencies for Ladybird

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_info() {
    echo -e "${BLUE}==>${NC} $1"
}

FAILED=0
WARNINGS=0

echo "============================================"
echo "Ladybird Build Dependency Checker"
echo "============================================"
echo

# Detect OS
OS="Unknown"
if [ "$(uname)" = "Linux" ]; then
    OS="Linux"
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO="$ID"
    fi
elif [ "$(uname)" = "Darwin" ]; then
    OS="macOS"
elif [ "$(uname)" = "FreeBSD" ]; then
    OS="FreeBSD"
elif [ "$(uname -s | cut -c 1-5)" = "MINGW" ] || [ "$(uname -s | cut -c 1-5)" = "MSYS_" ]; then
    OS="Windows"
fi

log_info "Detected OS: $OS"
echo

# Check CMake
echo "Checking CMake..."
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    if [ "$CMAKE_VERSION" != "unknown" ]; then
        REQUIRED="3.25.0"
        if [ "$(printf '%s\n' "$REQUIRED" "$CMAKE_VERSION" | sort -V | head -n1)" = "$REQUIRED" ]; then
            check_pass "CMake $CMAKE_VERSION (>= 3.25 required)"
        else
            check_fail "CMake $CMAKE_VERSION is too old (>= 3.25 required)"
            FAILED=1
        fi
    else
        check_warn "CMake found but version unknown"
    fi
else
    check_fail "CMake not found"
    FAILED=1
    case $OS in
        Linux)
            echo "  Install: sudo apt install cmake"
            ;;
        macOS)
            echo "  Install: brew install cmake"
            ;;
        FreeBSD)
            echo "  Install: pkg install cmake"
            ;;
    esac
fi
echo

# Check Ninja
echo "Checking Ninja..."
if command -v ninja &> /dev/null; then
    NINJA_VERSION=$(ninja --version 2>/dev/null || echo "unknown")
    check_pass "Ninja $NINJA_VERSION"
else
    check_warn "Ninja not found (vcpkg will download it)"
    WARNINGS=1
    case $OS in
        Linux)
            echo "  Or install: sudo apt install ninja-build"
            ;;
        macOS)
            echo "  Or install: brew install ninja"
            ;;
    esac
fi
echo

# Check Git
echo "Checking Git..."
if command -v git &> /dev/null; then
    GIT_VERSION=$(git --version | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    check_pass "Git $GIT_VERSION"
else
    check_fail "Git not found (required for build)"
    FAILED=1
fi
echo

# Check compilers
echo "Checking C++ compilers..."
COMPILER_FOUND=0

if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -n1 | grep -oP '\d+\.\d+' | head -n1 || echo "unknown")
    if [ "$GCC_VERSION" != "unknown" ]; then
        MAJOR=$(echo "$GCC_VERSION" | cut -d. -f1)
        if [ "$MAJOR" -ge 14 ]; then
            check_pass "GCC $GCC_VERSION (gcc-14+ required for C++23)"
            COMPILER_FOUND=1
        else
            check_warn "GCC $GCC_VERSION is too old (gcc-14+ required)"
            WARNINGS=1
            case $OS in
                Linux)
                    echo "  Install: sudo apt install gcc-14 g++-14"
                    ;;
            esac
        fi
    fi
fi

if command -v clang &> /dev/null; then
    CLANG_VERSION=$(clang --version | head -n1 | grep -oP '\d+\.\d+' | head -n1 || echo "unknown")
    if [ "$CLANG_VERSION" != "unknown" ]; then
        MAJOR=$(echo "$CLANG_VERSION" | cut -d. -f1)
        if [ "$MAJOR" -ge 20 ]; then
            check_pass "Clang $CLANG_VERSION (clang-20+ required for C++23)"
            COMPILER_FOUND=1
        else
            check_warn "Clang $CLANG_VERSION is too old (clang-20+ required)"
            WARNINGS=1
            case $OS in
                Linux)
                    echo "  Install: sudo apt install clang-20"
                    ;;
            esac
        fi
    fi
fi

if [ $COMPILER_FOUND -eq 0 ]; then
    check_fail "No suitable C++ compiler found (need gcc-14+ or clang-20+)"
    FAILED=1
fi
echo

# Check Python
echo "Checking Python..."
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    check_pass "Python3 $PYTHON_VERSION"
else
    check_fail "Python3 not found (required for build scripts)"
    FAILED=1
    case $OS in
        Linux)
            echo "  Install: sudo apt install python3"
            ;;
        macOS)
            echo "  Install: brew install python3"
            ;;
    esac
fi
echo

# Check pkg-config
echo "Checking pkg-config..."
if command -v pkg-config &> /dev/null; then
    PKGCONFIG_VERSION=$(pkg-config --version 2>/dev/null || echo "unknown")
    check_pass "pkg-config $PKGCONFIG_VERSION"
else
    check_warn "pkg-config not found (may be needed for some dependencies)"
    WARNINGS=1
    case $OS in
        Linux)
            echo "  Install: sudo apt install pkg-config"
            ;;
        macOS)
            echo "  Install: brew install pkg-config"
            ;;
    esac
fi
echo

# Check curl
echo "Checking curl..."
if command -v curl &> /dev/null; then
    CURL_VERSION=$(curl --version | head -n1 | grep -oP '\d+\.\d+\.\d+' | head -n1 || echo "unknown")
    check_pass "curl $CURL_VERSION"
else
    check_warn "curl not found (may be needed for downloads)"
    WARNINGS=1
fi
echo

# Platform-specific checks
if [ "$OS" = "Linux" ]; then
    echo "Checking Linux-specific dependencies..."

    # Check for essential development packages
    check_package() {
        local pkg=$1
        local desc=$2
        local cmd=$3

        if dpkg -l | grep -q "^ii  $pkg" 2>/dev/null; then
            check_pass "$desc ($pkg)"
        else
            check_warn "$desc not found ($pkg)"
            WARNINGS=1
            [ -n "$cmd" ] && echo "  Install: $cmd"
        fi
    }

    check_package "build-essential" "Build essentials" "sudo apt install build-essential"
    check_package "libssl-dev" "OpenSSL development" "sudo apt install libssl-dev"
    check_package "libcurl4-openssl-dev" "CURL development" "sudo apt install libcurl4-openssl-dev"
    check_package "libicu-dev" "ICU development" "sudo apt install libicu-dev"
    check_package "libxml2-dev" "LibXML2 development" "sudo apt install libxml2-dev"

    echo
    echo "Checking optional Linux dependencies..."
    check_package "qt6-base-dev" "Qt6 base (optional)" "sudo apt install qt6-base-dev"
    check_package "libpulse-dev" "PulseAudio development (optional)" "sudo apt install libpulse-dev"
    check_package "libasound2-dev" "ALSA development (optional)" "sudo apt install libasound2-dev"

    echo
fi

if [ "$OS" = "macOS" ]; then
    echo "Checking macOS-specific dependencies..."

    # Check Homebrew
    if command -v brew &> /dev/null; then
        check_pass "Homebrew installed"

        # Check for essential packages
        if brew list openssl@3 &> /dev/null; then
            check_pass "OpenSSL (via Homebrew)"
        else
            check_warn "OpenSSL not found"
            echo "  Install: brew install openssl@3"
            WARNINGS=1
        fi

        if brew list icu4c &> /dev/null; then
            check_pass "ICU (via Homebrew)"
        else
            check_warn "ICU not found"
            echo "  Install: brew install icu4c"
            WARNINGS=1
        fi
    else
        check_warn "Homebrew not found (recommended for macOS)"
        echo "  Install: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        WARNINGS=1
    fi

    # Check Xcode Command Line Tools
    if xcode-select -p &> /dev/null; then
        check_pass "Xcode Command Line Tools installed"
    else
        check_fail "Xcode Command Line Tools not found"
        echo "  Install: xcode-select --install"
        FAILED=1
    fi

    echo
fi

# Check for optional tools
echo "Checking optional development tools..."

if command -v ccache &> /dev/null; then
    CCACHE_VERSION=$(ccache --version | head -n1 | grep -oP '\d+\.\d+(\.\d+)?' || echo "unknown")
    check_pass "ccache $CCACHE_VERSION (speeds up rebuilds)"
else
    check_warn "ccache not found (optional, but recommended)"
    WARNINGS=1
    case $OS in
        Linux)
            echo "  Install: sudo apt install ccache"
            ;;
        macOS)
            echo "  Install: brew install ccache"
            ;;
    esac
fi

if command -v gdb &> /dev/null; then
    GDB_VERSION=$(gdb --version | head -n1 | grep -oP '\d+\.\d+' | head -n1 || echo "unknown")
    check_pass "GDB $GDB_VERSION (debugging)"
elif command -v lldb &> /dev/null; then
    LLDB_VERSION=$(lldb --version | head -n1 | grep -oP '\d+\.\d+\.\d+' | head -n1 || echo "unknown")
    check_pass "LLDB $LLDB_VERSION (debugging)"
else
    check_warn "No debugger found (gdb or lldb recommended)"
    WARNINGS=1
fi

if command -v valgrind &> /dev/null; then
    VALGRIND_VERSION=$(valgrind --version | grep -oP '\d+\.\d+\.\d+' || echo "unknown")
    check_pass "Valgrind $VALGRIND_VERSION (profiling)"
else
    check_warn "Valgrind not found (optional, for profiling)"
    WARNINGS=1
fi

echo

# System requirements
echo "Checking system requirements..."

# Check disk space
if command -v df &> /dev/null; then
    AVAILABLE_GB=$(df -BG . | tail -n1 | awk '{print $4}' | sed 's/G//')
    if [ "$AVAILABLE_GB" -gt 20 ]; then
        check_pass "Available disk space: ${AVAILABLE_GB}GB (20GB+ required)"
    elif [ "$AVAILABLE_GB" -gt 10 ]; then
        check_warn "Available disk space: ${AVAILABLE_GB}GB (20GB+ recommended)"
        WARNINGS=1
    else
        check_fail "Low disk space: ${AVAILABLE_GB}GB (20GB+ required)"
        FAILED=1
    fi
fi

# Check memory
if [ -f /proc/meminfo ]; then
    TOTAL_MEM_GB=$(grep MemTotal /proc/meminfo | awk '{printf "%.0f", $2/1024/1024}')
    if [ "$TOTAL_MEM_GB" -ge 16 ]; then
        check_pass "System memory: ${TOTAL_MEM_GB}GB (16GB+ recommended)"
    elif [ "$TOTAL_MEM_GB" -ge 8 ]; then
        check_warn "System memory: ${TOTAL_MEM_GB}GB (16GB+ recommended)"
        echo "  Consider using fewer parallel jobs: ./Meta/ladybird.py build -j 2"
        WARNINGS=1
    else
        check_warn "Low system memory: ${TOTAL_MEM_GB}GB (8GB minimum)"
        echo "  Use single-threaded build: ./Meta/ladybird.py build -j 1"
        WARNINGS=1
    fi
elif command -v sysctl &> /dev/null; then
    # macOS
    TOTAL_MEM_GB=$(sysctl -n hw.memsize | awk '{printf "%.0f", $1/1024/1024/1024}')
    if [ "$TOTAL_MEM_GB" -ge 16 ]; then
        check_pass "System memory: ${TOTAL_MEM_GB}GB"
    else
        check_warn "System memory: ${TOTAL_MEM_GB}GB (16GB+ recommended)"
        WARNINGS=1
    fi
fi

echo
echo "============================================"
echo "Summary"
echo "============================================"

if [ $FAILED -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    check_pass "All dependencies satisfied! You're ready to build Ladybird."
    echo
    echo "Next steps:"
    echo "  1. Build: ./Meta/ladybird.py build"
    echo "  2. Run:   ./Meta/ladybird.py run"
    echo "  3. Test:  ./Meta/ladybird.py test"
elif [ $FAILED -eq 0 ]; then
    check_warn "Required dependencies satisfied, but some optional dependencies missing."
    echo "  You can build, but consider installing optional dependencies."
    echo
    echo "Build with:"
    echo "  ./Meta/ladybird.py build"
else
    check_fail "Missing required dependencies! Please install them before building."
    echo
    echo "Install required dependencies first, then try:"
    echo "  ./Meta/ladybird.py build"
fi

echo

exit $FAILED
