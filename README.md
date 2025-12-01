# Audio Restoration Suite

A professional audio restoration plugin and standalone application inspired by Wave Corrector, built with JUCE.

## Features

### Core Restoration Tools
- **Click & Pop Removal**: Automatic detection and repair of vinyl clicks and surface noise
- **Noise Reduction**: FFT-based spectral noise reduction for tape hiss and background noise
- **Hum & Rumble Filters**: Remove low-frequency rumble and AC hum (50/60Hz)
- **Graphic EQ**: Multi-band frequency correction and tonal balance

### Standalone Features
- **Batch Processing**: Process multiple files automatically
- **Track Detection**: Automatic album track splitting
- **Session Management**: Save/load correction sessions
- **Waveform Editor**: Visual editing with correction overlay
- **Multi-format Support**: WAV, FLAC, MP3, OGG

### VST Plugin Features
- Real-time processing in DAW
- Automation support
- Low-latency operation
- Preset management

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

## License

[Your License Here]

## Credits

Inspired by Wave Corrector by Ganymede Test & Measurement
