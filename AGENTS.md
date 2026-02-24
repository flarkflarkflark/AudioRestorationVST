# Vinyl Restoration Suite - Project TODO

## DSP & Audio Engine
- [x] **Functional Volume Controls**: Implement gain scaling for the new Record and Monitor faders. (Completed in 1.6.28)
- [x] **Recording Track Detection**: (Completed in 1.6.28)
    - Implement real-time silence detection during recording.
    - Window prompt: "Is this a new track?" when silence is detected.
    - Automatic track marker placement during live recording.
- [x] **Editing Tools**: (Completed in 1.6.28)
    - Add Fade-in and Fade-out tools for selections.
    - Enhance track markers for manual editing and adjustment.
- [ ] **Export & Metadata**:
    - Implement track naming in the editor.
    - Add ID3 tag support for exports (Artist, Album, Title).
    - Auto-naming files based on track names during export.

## GUI & Visualization
- [x] **View Choice**: Add a toggle to switch between Normal Waveform and Spectral Waveform views. (Completed in 1.6.28)
- [ ] **Preset System**: Save and load restoration settings (Click sensitivity, EQ, Noise profile).
- [x] **Logo Consistency**: Ensure flarkAUDIO logo is displayed without "AUDIO" text everywhere. (Verified)

## AI & Optimization
- [ ] **Linux Model Optimization**: Use Microsoft Olive to create CUDA/ROCm optimized models for Linux.
- [ ] **Model Selection UI**: Allow users to choose between standard and optimized models.

## Infrastructure
- [ ] **Fix CI Builds**: Commit current Linux build fixes to restore GitHub Actions status.
- [ ] **Linux Packaging**: Create AppImage or Flatpak for easier distribution.
