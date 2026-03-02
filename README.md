# Vinyl Restoration Suite

[![Build Status](https://github.com/flarkflarkflark/AudioRestorationVST/actions/workflows/release.yml/badge.svg)](https://github.com/flarkflarkflark/AudioRestorationVST/actions)
[![Release](https://img.shields.io/github/v/release/flarkflarkflark/AudioRestorationVST)](https://github.com/flarkflarkflark/AudioRestorationVST/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A professional audio restoration plugin and standalone application inspired by Wave Corrector and optimized for modern workflows. Built with JUCE and featuring AI-powered noise reduction.

## Download (v1.6.32)

**Latest Portable Releases: [v1.6.32](https://github.com/flarkflarkflark/AudioRestorationVST/releases/latest)**

Pre-built binaries are available for:
- **Linux**: Portable **AppImage**, VST3, and Standalone Binary
- **macOS**: Portable **DMG** (Universal: Intel & ARM), VST3, and App
- **Windows**: Portable ZIP (Standalone EXE + DLLs), and VST3

### Installation & Usage

- **Linux (AppImage)**: Download the `.AppImage` file, make it executable (`chmod +x`), and run it.
- **macOS (DMG)**: Open the `.dmg` file and drag the application to your Applications folder.
- **Windows (Portable)**: Extract the `.zip` file and run `Vinyl Restoration Suite.exe`.
- **VST3 Plugins**: Copy the `.vst3` folder/file to your DAW's VST3 folder.

## Features

### Currently Implemented (v1.6.32)
- **AI-Powered Denoise**: Advanced RNNoise-based AI denoiser with DirectML/NPU acceleration support.
- **Advanced Click & Pop Removal**: Automatic detection and repair using crossfade and spline interpolation techniques.
- **Real-time Spectral Processing**: High-quality FFT-based noise reduction with profile capture.
- **10-Band Graphic EQ**: Precise frequency control (31Hz - 16kHz) with vintage Philips-style glowing interface.
- **Rumble & Hum Filters**: Dedicated filters for subsonic rumble (5-150Hz) and AC power line hum (40-80Hz).
- **Difference Mode**: Listen exclusively to the noise and clicks being removed for surgical precision.
- **Recording & Track Detection**: Record directly into the app with automatic silence-based track splitting.
- **Fully Scalable GUI**: Professional, utilitarian interface scalable from 25% to 400% (supports 4K/5K displays).
- **Visual Feedback**: Real-time waveform and spectral views with activity-glowing controls.

## Technical Details

- **Framework**: JUCE 8.0+
- **AI Engine**: ONNX Runtime (supporting CPU, DirectML, CoreML, CUDA, ROCm)
- **Language**: C++17
- **Formats**: VST3, Standalone App
- **Company**: flarkAUDIO

## Building from Source

### Prerequisites
- CMake 3.22+
- C++17 compatible compiler (MSVC 2022, Clang, or GCC)
- JUCE Framework (included as submodule)

### Quick Start
```bash
# Clone the repository
git clone --recursive https://github.com/flarkflarkflark/AudioRestorationVST.git
cd AudioRestorationVST

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Credits

- Inspired by **Wave Corrector** by Ganymede Test & Measurement
- Built with **JUCE Framework**
- AI Denoiser based on **RNNoise**
- Developed by **flarkAUDIO**
