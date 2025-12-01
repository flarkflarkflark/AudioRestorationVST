# Quick Start Guide

## Initial Setup

1. **Get JUCE Framework**
   ```bash
   cd /mnt/PRODUCTION/PROGRAMS/AudioRestorationVST
   git init
   git submodule add -b master https://github.com/juce-framework/JUCE.git JUCE
   git submodule update --init --recursive
   ```

2. **Build the Project**
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build . --config Release
   ```

3. **Test the Plugin**
   - **VST3**: Copy from `build/AudioRestoration_artefacts/Release/VST3/` to your VST3 folder
   - **Standalone**: Run `build/AudioRestoration_artefacts/Release/Standalone/Audio Restoration`

## Development Workflow

### Making Changes
1. Edit source files in `Source/`
2. Rebuild: `cd build && cmake --build . --config Release`
3. Test in standalone or DAW

### Adding DSP Features
1. Edit DSP classes in `Source/DSP/`
2. Key areas to implement:
   - `ClickRemoval.cpp`: Complete detection and removal algorithms
   - `NoiseReduction.cpp`: Implement spectral subtraction
   - `FilterBank.cpp`: Already functional, can add more filters

### Testing Click Removal
The click removal includes your Reaper crossfade technique:
- `CrossfadeSmoothing` mode: Short fade in/out around click (like your manual method)
- `SplineInterpolation` mode: Cubic spline for larger clicks
- `Automatic` mode: Chooses best method based on click size

## Current Status

**Working:**
- Basic VST3 and Standalone compilation structure
- Parameter management (all controls connected)
- Filter bank (rumble, hum, 10-band EQ)
- GUI with all controls laid out

**Needs Implementation:**
- [ ] Click detection algorithm
- [ ] Cubic spline interpolation (currently uses linear)
- [ ] Spectral noise reduction (FFT processing)
- [ ] Waveform display (standalone)
- [ ] Batch processing (standalone)
- [ ] Session save/load (standalone)

## Next Steps

1. **Implement click removal algorithm** in `Source/DSP/ClickRemoval.cpp`
2. **Implement noise reduction** in `Source/DSP/NoiseReduction.cpp`
3. **Test with real audio** containing clicks and noise
4. **Add waveform display** for standalone mode
5. **Implement batch processing** for multiple files

## Getting Help

- See `CLAUDE.md` for detailed architecture and development guide
- JUCE documentation: https://docs.juce.com/
- JUCE forum: https://forum.juce.com/
