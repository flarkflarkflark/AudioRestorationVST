cmake_minimum_required(VERSION 3.22)

project(AudioRestoration VERSION 1.0.0)

# Add JUCE as a subdirectory (using local submodule)
add_subdirectory(JUCE)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCE_FILES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/ClickRemoval.cpp
    Source/DSP/NoiseReduction.cpp
    Source/DSP/FilterBank.cpp
    Source/DSP/SpectralProcessor.cpp
    Source/GUI/WaveformDisplay.cpp
    Source/GUI/CorrectionListView.cpp
    Source/Processors/BatchProcessor.cpp
    Source/Utils/AudioFileManager.cpp
)

set(HEADER_FILES
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/DSP/ClickRemoval.h
    Source/DSP/NoiseReduction.h
    Source/DSP/FilterBank.h
    Source/DSP/SpectralProcessor.h
    Source/GUI/WaveformDisplay.h
    Source/GUI/CorrectionListView.h
    Source/Processors/BatchProcessor.h
    Source/Utils/AudioFileManager.h
)

# Add the plugin target
juce_add_plugin(AudioRestoration
    PRODUCT_NAME "Audio Restoration"
    COMPANY_NAME "Flark Audio"
    PLUGIN_MANUFACTURER_CODE Flrk
    PLUGIN_CODE Arst
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Restoration
)

# Add source files
target_sources(AudioRestoration PRIVATE
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Preprocessor definitions
target_compile_definitions(AudioRestoration PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Link JUCE modules
target_link_libraries(AudioRestoration
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
