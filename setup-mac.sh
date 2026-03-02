#!/bin/bash

# setup-mac.sh - Build script for macOS Ventura (13.0+)
# Targets Apple Silicon (M1/M2/M3) and Intel (x64) as Universal2

set -e

# 1. Install dependencies via Homebrew if needed
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Please install it from https://brew.sh/"
    # exit 1
else
    echo "Installing build dependencies..."
    brew install cmake lame mpg123
fi

# 2. Prepare build directory
mkdir -p build-mac
cd build-mac

# 3. Configure CMake
echo "Configuring for macOS Ventura (Universal2)..."
cmake .. 
    -DCMAKE_BUILD_TYPE=Release 
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" 
    -DCMAKE_OSX_DEPLOYMENT_TARGET="13.0" 
    -DENABLE_ONNX_RUNTIME=ON 
    -DONNXRUNTIME_USE_COREML=ON 
    -DVRS_USE_VCPKG=OFF

# 4. Build the project
echo "Building Standalone and VST3..."
cmake --build . --config Release --parallel $(sysctl -n hw.ncpu)

echo "--------------------------------------------------------"
echo "Build complete!"
echo "Standalone App: build-mac/VinylRestorationSuiteStandalone_artefacts/Release/Vinyl Restoration Suite.app"
echo "VST3 Plugin:    build-mac/VinylRestorationSuite_artefacts/Release/VST3/Vinyl Restoration Suite v1.6.32.vst3"
echo "--------------------------------------------------------"
