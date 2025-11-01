#!/usr/bin/env bash
# Setup CI Environment for Ladybird Build
# Installs dependencies, sets up vcpkg, and configures caching

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# Install dependencies based on OS
install_dependencies() {
    local os=$(detect_os)

    log_info "Installing dependencies for $os..."

    if [[ "$os" == "linux" ]]; then
        install_linux_deps
    elif [[ "$os" == "macos" ]]; then
        install_macos_deps
    else
        log_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi
}

install_linux_deps() {
    local distro=""

    if [ -f /etc/os-release ]; then
        . /etc/os-release
        distro=$ID
    fi

    log_info "Detected Linux distribution: $distro"

    if [[ "$distro" == "ubuntu" ]] || [[ "$distro" == "debian" ]]; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            ninja-build \
            ccache \
            pkg-config \
            gcc-14 \
            g++-14 \
            clang-20 \
            clang++-20 \
            libgl1-mesa-dev \
            libpulse-dev \
            libasound2-dev \
            curl \
            zip \
            unzip \
            tar \
            git
    elif [[ "$distro" == "fedora" ]] || [[ "$distro" == "rhel" ]]; then
        sudo dnf install -y \
            gcc \
            gcc-c++ \
            cmake \
            ninja-build \
            ccache \
            pkg-config \
            mesa-libGL-devel \
            pulseaudio-libs-devel \
            curl \
            zip \
            unzip \
            tar \
            git
    else
        log_warn "Unknown Linux distribution. Please install dependencies manually."
    fi
}

install_macos_deps() {
    if ! command -v brew &> /dev/null; then
        log_error "Homebrew not found. Please install Homebrew first."
        exit 1
    fi

    log_info "Installing dependencies via Homebrew..."

    brew install \
        cmake \
        ninja \
        ccache \
        pkg-config
}

# Setup vcpkg
setup_vcpkg() {
    local vcpkg_root="${VCPKG_ROOT:-${PWD}/Build/vcpkg}"

    log_info "Setting up vcpkg at $vcpkg_root..."

    if [ -d "$vcpkg_root" ]; then
        log_warn "vcpkg directory already exists. Skipping clone."
    else
        mkdir -p "$(dirname "$vcpkg_root")"
        git clone https://github.com/microsoft/vcpkg.git "$vcpkg_root"
    fi

    cd "$vcpkg_root"

    # Bootstrap vcpkg
    if [[ $(detect_os) == "linux" ]] || [[ $(detect_os) == "macos" ]]; then
        ./bootstrap-vcpkg.sh
    else
        ./bootstrap-vcpkg.bat
    fi

    cd - > /dev/null

    log_info "vcpkg setup complete"
    log_info "Set VCPKG_ROOT=$vcpkg_root in your environment"
}

# Configure ccache
setup_ccache() {
    local ccache_dir="${CCACHE_DIR:-${HOME}/.ccache}"
    local ccache_max_size="${CCACHE_MAX_SIZE:-2G}"

    log_info "Configuring ccache..."

    mkdir -p "$ccache_dir"

    ccache --set-config=cache_dir="$ccache_dir"
    ccache --set-config=max_size="$ccache_max_size"
    ccache --set-config=compression=true
    ccache --set-config=compression_level=6
    ccache --set-config=sloppiness=time_macros

    log_info "ccache configured:"
    log_info "  - Cache directory: $ccache_dir"
    log_info "  - Max size: $ccache_max_size"
    log_info "  - Compression: enabled"

    ccache --show-stats
}

# Create cache directories
setup_cache_dirs() {
    log_info "Creating cache directories..."

    mkdir -p Build/caches/vcpkg-binary-cache
    mkdir -p Build/caches/download-cache
    mkdir -p .ccache

    log_info "Cache directories created"
}

# Set environment variables
setup_environment() {
    log_info "Setting up environment variables..."

    local vcpkg_root="${VCPKG_ROOT:-${PWD}/Build/vcpkg}"
    local ccache_dir="${CCACHE_DIR:-${HOME}/.ccache}"

    cat > ci-env.sh <<EOF
# Ladybird CI Environment Variables
export LADYBIRD_SOURCE_DIR="${PWD}"
export VCPKG_ROOT="$vcpkg_root"
export CCACHE_DIR="$ccache_dir"
export VCPKG_BINARY_SOURCES="clear;files,${PWD}/Build/caches/vcpkg-binary-cache,readwrite"

# Compiler settings
export CC="${CC:-gcc-14}"
export CXX="${CXX:-g++-14}"

# ccache compiler check (ignores plugin changes)
export CCACHE_COMPILERCHECK="%compiler% -v"
EOF

    log_info "Environment variables written to ci-env.sh"
    log_info "Source this file: source ci-env.sh"
}

# Verify setup
verify_setup() {
    log_info "Verifying setup..."

    local errors=0

    # Check CMake
    if command -v cmake &> /dev/null; then
        log_info "✓ CMake found: $(cmake --version | head -1)"
    else
        log_error "✗ CMake not found"
        errors=$((errors + 1))
    fi

    # Check Ninja
    if command -v ninja &> /dev/null; then
        log_info "✓ Ninja found: $(ninja --version)"
    else
        log_error "✗ Ninja not found"
        errors=$((errors + 1))
    fi

    # Check ccache
    if command -v ccache &> /dev/null; then
        log_info "✓ ccache found: $(ccache --version | head -1)"
    else
        log_error "✗ ccache not found"
        errors=$((errors + 1))
    fi

    # Check vcpkg
    local vcpkg_root="${VCPKG_ROOT:-${PWD}/Build/vcpkg}"
    if [ -f "$vcpkg_root/vcpkg" ] || [ -f "$vcpkg_root/vcpkg.exe" ]; then
        log_info "✓ vcpkg found at $vcpkg_root"
    else
        log_error "✗ vcpkg not found"
        errors=$((errors + 1))
    fi

    # Check compiler
    if command -v "${CC:-gcc-14}" &> /dev/null; then
        log_info "✓ Compiler found: $(${CC:-gcc-14} --version | head -1)"
    else
        log_error "✗ Compiler ${CC:-gcc-14} not found"
        errors=$((errors + 1))
    fi

    if [ $errors -eq 0 ]; then
        log_info "✓ All checks passed!"
        return 0
    else
        log_error "✗ $errors checks failed"
        return 1
    fi
}

# Main
main() {
    log_info "Setting up Ladybird CI environment..."

    install_dependencies
    setup_vcpkg
    setup_ccache
    setup_cache_dirs
    setup_environment
    verify_setup

    log_info "CI environment setup complete!"
    log_info ""
    log_info "Next steps:"
    log_info "  1. Source the environment: source ci-env.sh"
    log_info "  2. Configure build: cmake --preset Release"
    log_info "  3. Build: cmake --build Build"
}

main "$@"
