# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Audio Restoration Suite is a professional audio restoration tool built with JUCE that compiles to both:
1. **VST3 Plugin** - For use in DAWs (real-time processing)
2. **Standalone Application** - Full-featured audio editor like Wave Corrector

The project implements professional audio restoration algorithms including click removal, spectral noise reduction, filtering, and EQ.

## Building the Project

### Initial Setup
```bash
# Clone JUCE framework (first time only)
git submodule add -b master https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build both VST and Standalone
cmake --build . --config Release

# For debug build
cmake --build . --config Debug
```

### Build Outputs
- **VST3**: `build/AudioRestoration_artefacts/Release/VST3/Audio Restoration.vst3`
- **Standalone**: `build/AudioRestoration_artefacts/Release/Standalone/Audio Restoration`

### Rebuilding After Changes
```bash
cd build
cmake --build . --config Release
```

### Testing
```bash
# Run standalone application
./build/AudioRestoration_artefacts/Release/Standalone/Audio\ Restoration

# Install VST3 for testing (Linux)
cp -r build/AudioRestoration_artefacts/Release/VST3/*.vst3 ~/.vst3/

# Install VST3 for testing (Windows)
# Copy to C:\Program Files\Common Files\VST3\
```

## Architecture

### High-Level Structure

```
AudioRestoration/
├── Source/
│   ├── PluginProcessor.cpp/h      # Audio processing engine (VST + Standalone)
│   ├── PluginEditor.cpp/h         # Main GUI controller
│   ├── DSP/                       # Digital Signal Processing modules
│   │   ├── ClickRemoval.cpp/h     # Click detection & cubic spline interpolation
│   │   ├── NoiseReduction.cpp/h   # FFT-based spectral subtraction
│   │   ├── FilterBank.cpp/h       # IIR filters (hum, rumble, EQ)
│   │   └── SpectralProcessor.cpp/h # FFT utilities and spectral analysis
│   ├── GUI/                       # User Interface components
│   │   ├── WaveformDisplay.cpp/h  # Zoomable waveform with correction overlay
│   │   └── CorrectionListView.cpp/h # List of detected/applied corrections
│   ├── Processors/                # High-level processing orchestration
│   │   └── BatchProcessor.cpp/h   # Batch file processing (Standalone only)
│   └── Utils/                     # Utility classes
│       └── AudioFileManager.cpp/h # File I/O, session save/load
└── Resources/                     # Images, presets, default settings
```

### DSP Architecture

The DSP processing chain follows this flow:

1. **Input Buffer** → Raw audio from file or DAW
2. **Click Detection** → Analyze for transient anomalies
3. **Click Removal** → Cubic spline interpolation over detected clicks
4. **Spectral Processing** → FFT → Noise profile subtraction → IFFT
5. **Filter Bank** → High-pass (rumble) → Notch (hum) → Parametric EQ
6. **Output Buffer** → Processed audio to file or DAW

### Key Design Patterns

#### Separation of VST and Standalone Logic
- **Shared**: All DSP code in `Source/DSP/` works identically in both modes
- **VST-specific**: Real-time processing, parameter automation, low latency
- **Standalone-specific**: File I/O, batch processing, session management, waveform editing

Use `#if JUCE_STANDALONE_APPLICATION` to conditionally compile standalone-only features.

#### Parameter Management
JUCE's `AudioProcessorValueTreeState` handles all parameters:
- VST: Parameters are automated by DAW
- Standalone: Parameters controlled by GUI directly

#### Thread Safety
- DSP runs on audio thread (real-time priority)
- GUI runs on message thread
- Use `juce::MessageManager::callAsync()` for GUI updates from audio thread
- Use atomic variables or locks for shared state

## DSP Implementation Details

### 1. Click & Pop Removal (`Source/DSP/ClickRemoval.cpp`)

**Algorithm** (based on Wave Corrector's approach):
```
1. Detection Phase:
   - Calculate first and second derivatives of waveform
   - Detect sharp discontinuities (clicks) using threshold
   - Filter out periodic signals (music) using autocorrelation
   - Store click positions and estimated widths

2. Correction Phase:
   - For each detected click:
     - Extract clean samples before/after click
     - Fit cubic spline through clean regions
     - Replace corrupted samples with interpolated curve
     - Apply short crossfade at boundaries
```

**Key Parameters**:
- `sensitivity`: Threshold for click detection (0-100)
- `maxWidth`: Maximum samples to interpolate (typically 100-1000)
- `stereoMode`: Process channels independently or correlated

**Implementation Notes**:
- Use `juce::dsp::WindowingFunction` for crossfade envelopes
- Process in blocks for efficiency but maintain click list across blocks
- For VST: Use look-ahead buffer to detect clicks before they play
- Correction list should be saved with session (standalone only)

### 2. Noise Reduction (`Source/DSP/NoiseReduction.cpp`)

**Algorithm** (Spectral Subtraction):
```
1. Profile Capture:
   - Analyze silent/noise-only section of audio
   - Compute average FFT magnitude spectrum (noise profile)
   - Option: Capture dual profile for varying noise (shellac records)

2. Real-time Processing:
   - FFT of current audio block (use overlap-add)
   - Subtract noise profile from magnitude spectrum
   - Apply spectral floor to prevent musical noise
   - Inverse FFT to reconstruct audio
   - Overlap-add with windowing (Hann window)
```

**Key Parameters**:
- `reductionAmount`: How much noise to remove (0-24 dB)
- `noiseProfile`: Stored spectral profile
- `fftSize`: FFT size (2048-8192, affects quality vs latency)

**Implementation Notes**:
- Use `juce::dsp::FFT` for FFT operations
- Implement overlap-add with 75% overlap (hop size = fftSize/4)
- Store separate profiles for left/right channels
- For VST: Higher latency mode for better quality, or lower latency for real-time

### 3. Filter Bank (`Source/DSP/FilterBank.cpp`)

**Filters**:
- **Rumble Filter**: 4th-order Butterworth high-pass (20-80 Hz cutoff)
- **Hum Filter**: Notch filter at 50/60 Hz (Q=10-30)
- **Graphic EQ**: 10-band parametric EQ (31Hz to 16kHz)

**Implementation Notes**:
- Use `juce::dsp::IIR::Filter` for all filters
- Use `juce::dsp::ProcessorChain` to chain filters efficiently
- Ensure filter coefficients update smoothly (use parameter smoothing)
- Match Wave Corrector's frequency bands: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz

### 4. Spectral Processor (`Source/DSP/SpectralProcessor.cpp`)

Utility class for:
- FFT/IFFT operations with proper windowing
- Spectral analysis for displays (spectrum view, spectrogram)
- Magnitude/phase separation and reconstruction

## GUI Implementation

### Waveform Display (`Source/GUI/WaveformDisplay.cpp`)

**Features**:
- Zoomable waveform view (horizontal: time, vertical: amplitude)
- Overlay corrected vs uncorrected waveforms
- Visual markers for clicks, track boundaries, cue points
- Playback cursor
- Mouse interaction: click to position, drag to select block

**Implementation**:
- Use `juce::AudioThumbnail` for efficient waveform rendering
- Draw corrections as colored overlays
- Support multiple zoom levels (match Wave Corrector scales)
- Use double buffering for smooth scrolling

### Correction List View (`Source/GUI/CorrectionListView.cpp`)

**Features**:
- Scrollable list of all detected/applied corrections
- Show: position (time), magnitude, width, type
- Click to select/audition correction
- Right-click context menu: delete, adjust, re-detect

**Implementation**:
- Use `juce::ListBox` or `juce::TableListBox`
- Sync with waveform display selection
- Filter criteria (show only large clicks, user corrections, etc.)

## Standalone-Specific Features

### Batch Processing (`Source/Processors/BatchProcessor.cpp`)

Process multiple files with saved settings:
```
1. User adds files to batch queue
2. For each file:
   - Load audio
   - Apply: click removal → noise reduction → filtering → normalization
   - Optionally: detect tracks and split
   - Save processed file(s)
   - Log results
3. Report completion
```

**Implementation**:
- Run on background thread (not audio thread)
- Update progress bar and log window
- Support cancellation
- Save batch presets (XML or JSON)

### Track Detection & Splitting

**Algorithm**:
```
1. Analyze waveform for silence regions (below threshold)
2. Identify silence >= minimum duration (e.g., 2 seconds)
3. Split audio at silence midpoints
4. Apply fade-in/fade-out at track boundaries
5. Save as separate files with track numbering
```

**Implementation**:
- Use RMS level detection with adjustable threshold
- Match Wave Corrector's "gapless" mode (split at midpoint)
- Support manual track boundary adjustment
- Extract metadata (track titles, artist, album) if available

### Session Management (`Source/Utils/AudioFileManager.cpp`)

**Session File Format** (JSON or XML):
```json
{
  "audioFile": "/path/to/file.wav",
  "corrections": [
    {"position": 12345, "width": 100, "magnitude": 0.8, "type": "click"},
    {"position": 45678, "width": 50, "magnitude": 0.5, "type": "pop"}
  ],
  "noiseProfile": [...],
  "trackBoundaries": [0, 234567, 456789],
  "parameters": {
    "clickSensitivity": 50,
    "noiseReduction": 12,
    "rumbleFilter": true
  }
}
```

**Implementation**:
- Use `juce::XmlElement` or `juce::var` (JSON)
- Save corrections even if audio file is modified elsewhere
- Verify audio file hasn't changed (checksum or timestamp)
- Support session restore on startup

## Common Development Tasks

### Adding a New DSP Effect

1. Create new class in `Source/DSP/` inheriting from `juce::dsp::ProcessorBase`
2. Implement `prepare()`, `process()`, `reset()` methods
3. Add parameters to `PluginProcessor::createParameterLayout()`
4. Add to processing chain in `PluginProcessor::processBlock()`
5. Add GUI controls in `PluginEditor`

### Adding GUI Components

1. Create component class inheriting from `juce::Component`
2. Override `paint()` and `resized()` methods
3. Add to `PluginEditor` layout
4. Connect to parameters using `AudioProcessorValueTreeState::Attachment`

### Testing DSP Changes

```bash
# Build in debug mode
cd build && cmake --build . --config Debug

# Run standalone with test file
./build/AudioRestoration_artefacts/Debug/Standalone/Audio\ Restoration

# Or run in VST host (Reaper, Carla, etc.)
```

### Debugging

- Use `DBG()` macro for console output: `DBG("Value: " << value);`
- JUCE Profiler: `juce::Time::getMillisecondCounter()` for timing
- Enable JUCE assertions: `jassert(condition);`
- Use logging: `juce::Logger::writeToLog()`

## Performance Considerations

### VST Plugin Mode
- **Latency**: Minimize for real-time use
  - Click removal: Use small look-ahead buffer (512-2048 samples)
  - Noise reduction: Use smaller FFT (2048) or disable for low-latency
- **CPU Usage**: Optimize hot paths
  - Use SIMD when possible (`juce::dsp::SIMDRegister`)
  - Process in blocks, not sample-by-sample
  - Disable unused effects

### Standalone Mode
- Can use larger buffers and FFT sizes for better quality
- Batch processing can be slower but more thorough
- Display updates should not block audio thread

## File Format Support

Required formats (via JUCE):
- **WAV**: Native support (`juce::WavAudioFormat`)
- **AIFF**: Native support (`juce::AiffAudioFormat`)
- **FLAC**: Requires FLAC library (bundled with JUCE)
- **OGG Vorbis**: Requires Ogg Vorbis library
- **MP3**: Read-only (licensing restrictions on encoding)

For additional formats, use external libraries (e.g., libsndfile).

## Configuration Files

### Default Settings
Store in: `~/.config/AudioRestoration/` (Linux) or `%APPDATA%\AudioRestoration\` (Windows)
- `settings.xml`: User preferences (GUI layout, default paths)
- `presets/`: Saved preset files
- `sessions/`: Auto-saved sessions

## Known Limitations

1. **VST Latency**: Noise reduction and click removal add latency (report via `setLatencySamples()`)
2. **FFT Artifacts**: Spectral processing can create "musical noise" if over-applied
3. **Real-time Click Detection**: Difficult to achieve perfect detection without look-ahead
4. **Session Compatibility**: Sessions tied to original audio file location

## References

### Wave Corrector Features (from documentation)
- Click correction using cubic spline interpolation
- FFT-based hiss filter with noise profile capture
- Rumble detector and filter
- Graphic equalizer with presets
- Batch processing with configurable pipeline
- Track detection with adjustable silence threshold
- Session file format for saving corrections
- Waveform view with correction overlay
- Spectrum and spectrogram display
- Support for 16/24-bit audio at 44.1, 48, 88.2, 96 kHz

### Key JUCE Classes to Use
- `juce::AudioProcessor` - Base class for plugin
- `juce::AudioProcessorEditor` - Base class for GUI
- `juce::AudioProcessorValueTreeState` - Parameter management
- `juce::dsp::FFT` - Fast Fourier Transform
- `juce::dsp::IIR::Filter` - Infinite impulse response filters
- `juce::dsp::ProcessorChain` - Chain multiple processors
- `juce::AudioThumbnail` - Waveform display
- `juce::AudioFormatManager` - File I/O

### Useful Resources
- JUCE Documentation: https://docs.juce.com/
- JUCE Forum: https://forum.juce.com/
- DSP Guide: https://www.dspguide.com/
- Spectral Audio Processing: "Spectral Audio Signal Processing" by Julius O. Smith III

## Development Workflow

### Typical Feature Development
1. Create feature branch: `git checkout -b feature/new-filter`
2. Implement DSP algorithm in `Source/DSP/`
3. Add unit tests (if applicable)
4. Add GUI controls in `Source/GUI/`
5. Test in both VST and Standalone modes
6. Update this CLAUDE.md if architecture changes
7. Commit and create pull request

### Code Style
- Follow JUCE naming conventions:
  - Classes: `PascalCase`
  - Methods: `camelCase`
  - Private members: `m_camelCase` or just `camelCase`
- Use JUCE types: `juce::String`, `juce::Array`, etc.
- Prefer `std::unique_ptr` for ownership
- Use `juce::AudioBuffer<float>` for audio data

## Troubleshooting

### Build Issues
- **CMake can't find JUCE**: Ensure submodule is initialized: `git submodule update --init`
- **Compiler errors**: Check C++17 support and JUCE version (7.0+)
- **Linker errors**: Verify all JUCE modules are linked in CMakeLists.txt

### Runtime Issues
- **No audio output**: Check `PluginProcessor::prepareToPlay()` is called
- **Crackling/glitches**: Increase buffer size or optimize processing
- **VST not recognized**: Ensure VST3 is properly installed in plugin folder
- **Standalone won't load files**: Check `AudioFormatManager` has formats registered

### Performance Issues
- **High CPU usage**: Profile with JUCE profiler, optimize FFT size
- **GUI lag**: Ensure GUI updates are async, not on audio thread
- **Memory leaks**: Use JUCE leak detector, check smart pointer usage
