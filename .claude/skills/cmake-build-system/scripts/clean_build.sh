#!/usr/bin/env bash
# Complete clean rebuild of Ladybird

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LADYBIRD_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
PRESET="${BUILD_PRESET:-Release}"
CLEAN_VCPKG=0
CLEAN_CACHES=0
JOBS=""

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Complete clean rebuild of Ladybird browser.

OPTIONS:
    -p, --preset PRESET    Build preset (Release, Debug, Sanitizer, etc.)
                           Default: ${PRESET}
    -j, --jobs N           Number of parallel jobs (default: auto)
    -v, --vcpkg            Also clean vcpkg installation
    -c, --caches           Also clean vcpkg binary caches
    -a, --all              Clean everything (vcpkg + caches)
    -h, --help             Show this help message

EXAMPLES:
    $0                     # Clean rebuild with Release preset
    $0 -p Debug            # Clean rebuild with Debug preset
    $0 -a                  # Nuclear clean (removes vcpkg too)
    $0 -j 4                # Rebuild with 4 parallel jobs

PRESETS:
    Release                Optimized release build (default)
    Debug                  Debug build with symbols
    All_Debug              Debug with all debug macros
    Sanitizer              ASAN + UBSAN enabled
    Distribution           Static linking for distribution
    Windows_Experimental   Windows build

EOF
}

log_info() {
    echo -e "${BLUE}==>${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--preset)
            PRESET="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -v|--vcpkg)
            CLEAN_VCPKG=1
            shift
            ;;
        -c|--caches)
            CLEAN_CACHES=1
            shift
            ;;
        -a|--all)
            CLEAN_VCPKG=1
            CLEAN_CACHES=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

cd "${LADYBIRD_ROOT}"

echo "============================================"
echo "Ladybird Clean Rebuild Script"
echo "============================================"
echo "Preset: ${PRESET}"
echo "Jobs: ${JOBS:-auto}"
echo "Clean vcpkg: $([ $CLEAN_VCPKG -eq 1 ] && echo 'yes' || echo 'no')"
echo "Clean caches: $([ $CLEAN_CACHES -eq 1 ] && echo 'yes' || echo 'no')"
echo "============================================"
echo

# Determine build directory
case "${PRESET}" in
    Release)
        BUILD_DIR="Build/release"
        ;;
    Debug)
        BUILD_DIR="Build/debug"
        ;;
    All_Debug)
        BUILD_DIR="Build/alldebug"
        ;;
    Sanitizer)
        BUILD_DIR="Build/sanitizers"
        ;;
    Distribution)
        BUILD_DIR="Build/distribution"
        ;;
    Windows_Experimental|Windows_CI)
        BUILD_DIR="Build/debug"
        ;;
    Swift_Release)
        BUILD_DIR="Build/swift"
        ;;
    *)
        log_error "Unknown preset: ${PRESET}"
        log_info "Valid presets: Release, Debug, All_Debug, Sanitizer, Distribution, Windows_Experimental"
        exit 1
        ;;
esac

# Step 1: Clean build directory
if [ -d "${BUILD_DIR}" ]; then
    log_info "Cleaning build directory: ${BUILD_DIR}"
    SIZE=$(du -sh "${BUILD_DIR}" 2>/dev/null | awk '{print $1}' || echo "unknown")
    log_info "Current size: ${SIZE}"

    rm -rf "${BUILD_DIR}"
    log_success "Build directory cleaned"
else
    log_info "Build directory does not exist: ${BUILD_DIR}"
fi
echo

# Step 2: Clean user variables
USER_VARS="Meta/CMake/vcpkg/user-variables.cmake"
if [ -f "${USER_VARS}" ]; then
    log_info "Cleaning user variables: ${USER_VARS}"
    rm -f "${USER_VARS}"
    log_success "User variables cleaned"
fi
echo

# Step 3: Clean vcpkg (if requested)
if [ $CLEAN_VCPKG -eq 1 ]; then
    if [ -d "Build/vcpkg" ]; then
        log_warn "Cleaning vcpkg installation (this will require rebuilding all dependencies)"
        SIZE=$(du -sh "Build/vcpkg" 2>/dev/null | awk '{print $1}' || echo "unknown")
        log_info "Current size: ${SIZE}"

        rm -rf "Build/vcpkg"
        log_success "vcpkg cleaned"
    else
        log_info "vcpkg directory does not exist"
    fi
    echo
fi

# Step 4: Clean caches (if requested)
if [ $CLEAN_CACHES -eq 1 ]; then
    if [ -d "Build/caches" ]; then
        log_warn "Cleaning vcpkg binary caches (this will slow down first build)"
        SIZE=$(du -sh "Build/caches" 2>/dev/null | awk '{print $1}' || echo "unknown")
        log_info "Current size: ${SIZE}"

        rm -rf "Build/caches"
        log_success "Caches cleaned"
    else
        log_info "Caches directory does not exist"
    fi
    echo
fi

# Step 5: Bootstrap vcpkg (if we cleaned it)
if [ $CLEAN_VCPKG -eq 1 ]; then
    log_info "Bootstrapping vcpkg (this may take several minutes)..."
    if ./Meta/ladybird.py vcpkg; then
        log_success "vcpkg bootstrapped"
    else
        log_error "Failed to bootstrap vcpkg"
        exit 1
    fi
    echo
fi

# Step 6: Configure build
log_info "Configuring build with preset: ${PRESET}"
BUILD_ARGS=("--preset" "${PRESET}")

if [ -n "${JOBS}" ]; then
    BUILD_ARGS+=("-j" "${JOBS}")
fi

if ./Meta/ladybird.py build "${BUILD_ARGS[@]}"; then
    log_success "Build completed successfully"
else
    log_error "Build failed"
    log_info "Check build logs for details"
    log_info "Try running with verbose output:"
    log_info "  ./Meta/ladybird.py build --preset ${PRESET} -- -v"
    exit 1
fi
echo

# Step 7: Summary
echo "============================================"
echo "Build Summary"
echo "============================================"

if [ -d "${BUILD_DIR}" ]; then
    SIZE=$(du -sh "${BUILD_DIR}" 2>/dev/null | awk '{print $1}' || echo "unknown")
    log_info "Build directory size: ${SIZE}"
fi

if [ -d "Build/vcpkg" ]; then
    SIZE=$(du -sh "Build/vcpkg" 2>/dev/null | awk '{print $1}' || echo "unknown")
    log_info "vcpkg directory size: ${SIZE}"
fi

if [ -d "Build/caches" ]; then
    SIZE=$(du -sh "Build/caches" 2>/dev/null | awk '{print $1}' || echo "unknown")
    log_info "Caches directory size: ${SIZE}"
fi

log_success "Clean rebuild complete!"
echo

log_info "Next steps:"
echo "  Run Ladybird:  ./Meta/ladybird.py run --preset ${PRESET}"
echo "  Run tests:     ./Meta/ladybird.py test --preset ${PRESET}"
echo "  Build specific target: ninja -C ${BUILD_DIR} <target>"
echo
