# Quick Start Guide

## Initial Setup

1. **Clone with Submodules**
   ```bash
   git clone --recursive https://github.com/flarkflarkflark/AudioRestorationVST.git
   cd AudioRestorationVST
   ```

2. **Build the Project**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

3. **Optional: MP3 support (vcpkg Windows)**
   ```bash
   set VCPKG_ROOT=C:\path\to\vcpkg
   powershell -ExecutionPolicy Bypass -File tools\install_vcpkg_mp3.ps1
   cmake .. -DVRS_USE_VCPKG=ON -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
   ```

## Installation

- **VST3**: Copy from `build/VinylRestorationSuite_artefacts/Release/VST3/` to your system VST3 folder.
- **Standalone**: Run the executable in `build/VinylRestorationSuiteStandalone_artefacts/Release/`.

## Restoring Vinyl: Workflow

1. **Import/Record**: Drag an audio file into the standalone window or use the Record button to capture a transfer.
2. **Click Removal**: Adjust the **Sensitivity** knob. Use **Difference Mode** to ensure you're not removing musical transients.
3. **AI Denoise**: Enable the AI denoiser for broadband surface noise. Choose the appropriate provider (CPU/DirectML) in Settings.
4. **Spectral NR**: If specific noise patterns remain, use the **Capture Profile** button on a silent section and dial in the reduction.
5. **EQ & Filters**: Use the **Rumble** filter for subsonic noise and the **Graphic EQ** to restore tonal balance.
6. **Track Detection**: Click **Detect Tracks** to automatically find gaps between songs.
7. **Export**: Export individual tracks or the entire restored file.

## Key restoration techniques

- **Crossfade Smoothing**: Replicates the manual Reaper crossfade technique for a seamless, natural repair of small clicks.
- **Difference Mode**: ALWAYS use this to check your work. If you hear music in the difference signal, back off the sensitivity.
- **AI Acceleration**: On Windows, use the **DirectML** provider in AI settings to leverage your GPU/NPU for lower CPU load.

## Getting Help

- See `CLAUDE.md` for technical details and architecture.
- Follow development on [GitHub](https://github.com/flarkflarkflark/AudioRestorationVST).
