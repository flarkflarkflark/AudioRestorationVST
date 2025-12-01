# Audio Restoration Suite

[![Build Status](https://github.com/flarkflarkflark/AudioRestorationVST/actions/workflows/build.yml/badge.svg)](https://github.com/flarkflarkflark/AudioRestorationVST/actions)
[![Release](https://img.shields.io/github/v/release/flarkflarkflark/AudioRestorationVST)](https://github.com/flarkflarkflark/AudioRestorationVST/releases/latest)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A professional audio restoration plugin and standalone application inspired by Wave Corrector, built with JUCE.

## Download

**Latest Release: [v1.0.0](https://github.com/flarkflarkflark/AudioRestorationVST/releases/latest)**

Pre-built binaries are available for:
- **Linux** (VST3 + Standalone)
- **macOS** (VST3 + Standalone)
- **Windows** (VST3 + Standalone)

### Installation

1. Download the appropriate package for your platform from [Releases](https://github.com/flarkflarkflark/AudioRestorationVST/releases/latest)
2. Extract the archive
3. **VST3 Plugin**: Copy to your DAW's VST3 folder
   - Linux: `~/.vst3/`
   - macOS: `~/Library/Audio/Plug-Ins/VST3/`
   - Windows: `C:\Program Files\Common Files\VST3\`
4. **Standalone**: Run the executable directly

## Features

### Currently Implemented (v1.0.0)
- **10-Band Graphic EQ**: Precise frequency control (31Hz - 16kHz)
- **Rumble Filter**: High-pass filter (5-150Hz) for subsonic rumble removal
- **Hum Filter**: Notch filter (40-80Hz) for AC hum and power line noise
- **Difference Mode**: Listen to what's being removed (A/B comparison)
- **Click Removal Framework**: Crossfade-based click repair (manual Reaper-style technique)
- **Noise Reduction Framework**: FFT-based spectral processing structure
- **Fully Resizable GUI**: Scalable interface (500x400 to 2560x1440)
- **Individual Bypass Controls**: Per-effect bypass switches
- **Real-time Processing**: Low-latency DAW integration

### Planned Features
- Automatic click detection and removal
- Noise profile capture and adaptive reduction
- Batch processing for standalone version
- Waveform display with visual editing
- Preset management system
- Additional filter types (de-esser, high-shelf, etc.)

## Building

### Prerequisites
- CMake 3.22 or higher
- C++17 compatible compiler
- JUCE Framework 7.0+
- Git

### Quick Start
```bash
# Clone JUCE as submodule
git submodule add -b master https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake ..
cmake --build . --config Release

# VST3 will be in: build/AudioRestoration_artefacts/Release/VST3/
# Standalone will be in: build/AudioRestoration_artefacts/Release/Standalone/
```

## Screenshots

*Coming soon - GUI screenshots showing the resizable interface, EQ controls, and difference mode*

## Technical Details

- **Framework**: JUCE 7.0+
- **Language**: C++17
- **Architecture**: Modular DSP design with separate processors
- **Formats**: VST3, Standalone
- **Company**: Flark Audio
- **Plugin Codes**: Manufacturer `Flrk`, Product `Arst`

## Development

See [CLAUDE.md](CLAUDE.md) for comprehensive development documentation including:
- Architecture overview
- DSP implementation details
- Build instructions
- Troubleshooting guide
- Future development roadmap

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## CI/CD

Automated builds run on every commit via GitHub Actions:
- Linux (Ubuntu 24.04)
- macOS (latest)
- Windows (latest)

All builds produce both VST3 and Standalone binaries.

## License

MIT License - See LICENSE file for details

## Credits

- Inspired by **Wave Corrector** by Ganymede Test & Measurement
- Built with **JUCE Framework**
- Developed with **Claude Code**
- Company: **Flark Audio**
