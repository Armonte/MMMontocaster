#!/bin/bash

echo "Setting up CCCaster development environment on Ubuntu 24.04 LTS..."
echo "================================================================"

# Update package list
echo "Updating package list..."
sudo apt update

# Install essential build tools
echo "Installing essential build tools..."
sudo apt install -y build-essential git rsync zip unzip

# Install MinGW-w64 cross-compiler
echo "Installing MinGW-w64 cross-compiler..."
sudo apt install -y mingw-w64

# Install additional dependencies
echo "Installing additional dependencies..."
sudo apt install -y wine64 wine32 libwine-dev
sudo apt install -y libpng-dev zlib1g-dev
sudo apt install -y libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt install -y libsdl2-dev

# Install development headers for X11 (needed for some 3rdparty libraries)
echo "Installing X11 development headers..."
sudo apt install -y libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxi-dev

# Check if MinGW-w64 is properly installed
echo "Verifying MinGW-w64 installation..."
if ! command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: MinGW-w64 not found. Please check installation."
    exit 1
fi

echo "MinGW-w64 found: $(i686-w64-mingw32-gcc --version | head -n1)"

# Initialize git submodules
echo "Initializing git submodules..."
git submodule update --init --recursive

# Build AntTweakBar library
echo "Building AntTweakBar library..."
cd 3rdparty/AntTweakBar/src
make -f Makefile.linux clean
make -f Makefile.linux
cd ../../..

# Verify the build
if [ -f "3rdparty/AntTweakBar/lib/libAntTweakBar.a" ]; then
    echo "AntTweakBar library built successfully!"
else
    echo "WARNING: AntTweakBar library build may have failed. Check manually."
fi

# Test basic compilation
echo "Testing basic compilation..."
make clean
make depend

echo ""
echo "================================================================"
echo "Development environment setup complete!"
echo ""
echo "You can now try building CCCaster with:"
echo "  make logging    # For logging build"
echo "  make debug      # For debug build"
echo "  make release    # For release build"
echo ""
echo "Note: The first build may take longer as it compiles all dependencies."
echo "================================================================"
