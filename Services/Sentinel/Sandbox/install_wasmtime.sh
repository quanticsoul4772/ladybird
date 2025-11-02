#!/bin/bash
set -euo pipefail

echo "=== Wasmtime Installation Script ==="
echo "This script will install Wasmtime v38.0.3 for Sentinel sandbox"
echo ""

# Detect architecture and OS
ARCH=$(uname -m)
OS=$(uname -s | tr '[:upper:]' '[:lower:]')

echo "Detected: $OS on $ARCH"
echo ""

# Determine download URL
if [ "$OS" = "linux" ] && [ "$ARCH" = "x86_64" ]; then
    URL="https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-linux.tar.xz"
    FILENAME="wasmtime-v38.0.3-x86_64-linux.tar.xz"
    DIRNAME="wasmtime-v38.0.3-x86_64-linux"
elif [ "$OS" = "linux" ] && [ "$ARCH" = "aarch64" ]; then
    URL="https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-linux.tar.xz"
    FILENAME="wasmtime-v38.0.3-aarch64-linux.tar.xz"
    DIRNAME="wasmtime-v38.0.3-aarch64-linux"
elif [ "$OS" = "darwin" ] && [ "$ARCH" = "arm64" ]; then
    URL="https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-aarch64-macos.tar.xz"
    FILENAME="wasmtime-v38.0.3-aarch64-macos.tar.xz"
    DIRNAME="wasmtime-v38.0.3-aarch64-macos"
elif [ "$OS" = "darwin" ] && [ "$ARCH" = "x86_64" ]; then
    URL="https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-macos.tar.xz"
    FILENAME="wasmtime-v38.0.3-x86_64-macos.tar.xz"
    DIRNAME="wasmtime-v38.0.3-x86_64-macos"
else
    echo "ERROR: Unsupported platform: $OS $ARCH"
    echo "Please download manually from: https://github.com/bytecodealliance/wasmtime/releases"
    exit 1
fi

echo "Download URL: $URL"
echo ""

# Download
echo "Downloading Wasmtime..."
if command -v curl &> /dev/null; then
    curl -LO "$URL"
elif command -v wget &> /dev/null; then
    wget "$URL"
else
    echo "ERROR: Neither curl nor wget found. Please install one of them."
    exit 1
fi

# Extract
echo "Extracting..."
tar xf "$FILENAME"

# Install
echo ""
echo "Installing to /usr/local/wasmtime..."
echo "This requires sudo permissions."
echo ""

if sudo mv "$DIRNAME" /usr/local/wasmtime 2>/dev/null; then
    echo "Wasmtime installed successfully to /usr/local/wasmtime"
else
    echo "Failed to install to /usr/local/wasmtime"
    echo "Trying user-local installation to ~/wasmtime instead..."
    mv "$DIRNAME" ~/wasmtime
    echo "Wasmtime installed to ~/wasmtime"
    echo "Note: You may need to update CMakeLists.txt paths"
fi

# Cleanup
echo ""
echo "Cleaning up..."
rm -f "$FILENAME"

# Verify
echo ""
echo "Verifying installation..."
if [ -d "/usr/local/wasmtime" ]; then
    ls -lh /usr/local/wasmtime/lib/libwasmtime.* 2>/dev/null || echo "Warning: Library not found"
    ls -lh /usr/local/wasmtime/include/wasmtime.h 2>/dev/null || echo "Warning: Header not found"
    INSTALL_PATH="/usr/local/wasmtime"
elif [ -d "$HOME/wasmtime" ]; then
    ls -lh ~/wasmtime/lib/libwasmtime.* 2>/dev/null || echo "Warning: Library not found"
    ls -lh ~/wasmtime/include/wasmtime.h 2>/dev/null || echo "Warning: Header not found"
    INSTALL_PATH="$HOME/wasmtime"
else
    echo "ERROR: Wasmtime installation failed"
    exit 1
fi

echo ""
echo "=== Installation Complete ==="
echo "Installed to: $INSTALL_PATH"
echo ""
echo "Next steps:"
echo "1. Rebuild Ladybird:"
echo "   cd /home/rbsmith4/ladybird"
echo "   cmake --preset Release"
echo "   cmake --build Build/release --target sentinelservice"
echo ""
echo "2. Verify Wasmtime was detected:"
echo "   grep -i wasmtime Build/release/CMakeCache.txt"
echo ""
echo "3. Run tests:"
echo "   ./Build/release/bin/TestSandbox"
echo ""
