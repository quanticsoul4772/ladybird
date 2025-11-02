#!/bin/bash
# nsjail Installation Script for Sentinel Behavioral Analysis
# This script installs nsjail on Ubuntu/Debian systems

set -e  # Exit on any error

echo "=================================================="
echo "nsjail Installation Script"
echo "=================================================="
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo -e "${RED}Error: This script only works on Linux${NC}"
    exit 1
fi

# Check if running as root (for package installation)
if [[ $EUID -ne 0 ]]; then
    echo -e "${YELLOW}Warning: This script requires sudo privileges${NC}"
    echo "You will be prompted for your password"
    echo ""
fi

# Detect OS and version
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS_NAME=$NAME
    OS_VERSION=$VERSION_ID
    echo "Detected: $OS_NAME $OS_VERSION"
else
    echo -e "${RED}Error: Cannot detect OS version${NC}"
    exit 1
fi

echo ""
echo "=================================================="
echo "Step 1: Attempting package manager installation"
echo "=================================================="
echo ""

# Try package manager first (Ubuntu 20.04+)
if command -v apt-get &> /dev/null; then
    echo "Updating package lists..."
    sudo apt-get update -qq

    # Check if nsjail package exists
    if apt-cache show nsjail &> /dev/null; then
        echo -e "${GREEN}nsjail package found in repository${NC}"
        echo "Installing nsjail via apt-get..."
        sudo apt-get install -y nsjail

        # Verify installation
        if command -v nsjail &> /dev/null; then
            echo -e "${GREEN}✓ nsjail successfully installed via package manager${NC}"
            nsjail --version
            exit 0
        fi
    else
        echo -e "${YELLOW}nsjail package not available in repositories${NC}"
        echo "Proceeding to build from source..."
    fi
else
    echo -e "${YELLOW}apt-get not found, proceeding to build from source${NC}"
fi

echo ""
echo "=================================================="
echo "Step 2: Building nsjail from source"
echo "=================================================="
echo ""

# Install build dependencies
echo "Installing build dependencies..."
sudo apt-get install -y \
    autoconf \
    bison \
    flex \
    gcc \
    g++ \
    git \
    libprotobuf-dev \
    libnl-route-3-dev \
    libtool \
    make \
    pkg-config \
    protobuf-compiler

echo -e "${GREEN}✓ Build dependencies installed${NC}"
echo ""

# Clone nsjail repository
BUILD_DIR="/tmp/nsjail_build_$$"
echo "Cloning nsjail repository to $BUILD_DIR..."
git clone https://github.com/google/nsjail.git "$BUILD_DIR"
cd "$BUILD_DIR"

# Initialize submodules
echo "Initializing submodules..."
git submodule update --init

# Get commit hash for documentation
COMMIT_HASH=$(git rev-parse --short HEAD)
echo "Building nsjail from commit: $COMMIT_HASH"
echo ""

# Build nsjail
echo "Building nsjail (this may take a few minutes)..."
make -j$(nproc)

if [ ! -f "./nsjail" ]; then
    echo -e "${RED}Error: Build failed - nsjail binary not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓ nsjail built successfully${NC}"
echo ""

# Install to system
echo "Installing nsjail to /usr/local/bin..."
sudo cp nsjail /usr/local/bin/
sudo chmod 755 /usr/local/bin/nsjail

# Verify installation
if command -v nsjail &> /dev/null; then
    echo -e "${GREEN}✓ nsjail successfully installed${NC}"
else
    echo -e "${RED}Error: nsjail not found in PATH after installation${NC}"
    exit 1
fi

# Cleanup
echo "Cleaning up build directory..."
cd /
rm -rf "$BUILD_DIR"

echo ""
echo "=================================================="
echo "Step 3: Verification"
echo "=================================================="
echo ""

# Display version
echo "nsjail version:"
nsjail --version 2>&1 || nsjail --help | head -3
echo ""

# Run basic test
echo "Running basic test..."
if nsjail --mode o --time_limit 2 -- /bin/echo "nsjail test successful" 2>&1 | grep -q "test successful"; then
    echo -e "${GREEN}✓ Basic execution test passed${NC}"
else
    echo -e "${YELLOW}⚠ Basic test completed with warnings (may be normal)${NC}"
fi

echo ""
echo "=================================================="
echo "Installation Summary"
echo "=================================================="
echo ""
echo -e "${GREEN}✓ nsjail installation complete${NC}"
echo ""
echo "Binary location: $(which nsjail)"
echo "Version: $(nsjail --version 2>&1 | head -1 || echo 'See nsjail --help')"
echo "Build commit: $COMMIT_HASH"
echo ""
echo "Next steps:"
echo "1. Run verification tests (see NSJAIL_INSTALLATION.md)"
echo "2. Update NSJAIL_INSTALLATION.md with installation details"
echo "3. Proceed to Sentinel configuration file creation"
echo ""
echo "For usage examples, see:"
echo "  Services/Sentinel/Sandbox/NSJAIL_INSTALLATION.md"
echo ""
