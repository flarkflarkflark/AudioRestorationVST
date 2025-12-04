cmake_minimum_required(VERSION 3.22)

project(VinylRestorationSuite VERSION 1.5.2)

# Add JUCE as a subdirectory (using local submodule)
add_subdirectory(JUCE)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#==============================================================================
# MP3 Support (LAME encoding + mpg123 decoding)
#==============================================================================
option(ENABLE_MP3_SUPPORT "Enable MP3 read/write support via LAME and mpg123" ON)

set(MP3_ENABLED OFF)
set(MP3_LIBRARIES "")
set(MP3_COMPILE_DEFINITIONS "")

if(ENABLE_MP3_SUPPORT)
    # Find LAME (MP3 encoder)
    find_library(LAME_LIBRARY NAMES mp3lame lame PATHS /usr/lib /usr/local/lib)
    find_path(LAME_INCLUDE_DIR lame/lame.h PATHS /usr/include /usr/local/include)

    # Find mpg123 (MP3 decoder)
    find_library(MPG123_LIBRARY NAMES mpg123 PATHS /usr/lib /usr/local/lib)
    find_path(MPG123_INCLUDE_DIR mpg123.h PATHS /usr/include /usr/local/include)

    if(LAME_LIBRARY AND LAME_INCLUDE_DIR)
        message(STATUS "MP3 Encoding: LAME found at ${LAME_LIBRARY}")
        set(MP3_ENABLED ON)
        list(APPEND MP3_LIBRARIES ${LAME_LIBRARY})
        include_directories(${LAME_INCLUDE_DIR})
        list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
    else()
        message(WARNING "LAME not found - MP3 encoding disabled. Install with: sudo pacman -S lame")
    endif()

    if(MPG123_LIBRARY AND MPG123_INCLUDE_DIR)
        message(STATUS "MP3 Decoding: mpg123 found at ${MPG123_LIBRARY}")
        set(MP3_ENABLED ON)
        list(APPEND MP3_LIBRARIES ${MPG123_LIBRARY})
        include_directories(${MPG123_INCLUDE_DIR})
        list(APPEND MP3_COMPILE_DEFINITIONS USE_MPG123=1)
    else()
        message(WARNING "mpg123 not found - MP3 decoding may be limited. Install with: sudo pacman -S mpg123")
    endif()

    if(MP3_ENABLED)
        message(STATUS "MP3 Support: Enabled")
    endif()
else()
    message(STATUS "MP3 Support: Disabled by user")
endif()

#==============================================================================
# GPU Acceleration Setup (Cross-Platform)
#==============================================================================
option(ENABLE_GPU_ACCELERATION "Enable GPU acceleration" ON)

set(GPU_BACKEND "AUTO" CACHE STRING "GPU backend to use (AUTO/OPENCL/VULKAN/CUDA/HIP/ONEAPI)")
set_property(CACHE GPU_BACKEND PROPERTY STRINGS AUTO OPENCL VULKAN CUDA HIP ONEAPI)

if(ENABLE_GPU_ACCELERATION)
    set(GPU_ENABLED OFF)
    set(GPU_SOURCE_FILES "")
    set(GPU_HEADER_FILES "")
    set(GPU_LIBRARIES "")
    set(GPU_COMPILE_DEFINITIONS "")

    # Try to detect available GPU backends
    if(GPU_BACKEND STREQUAL "AUTO")
        # Check for OpenCL (most universal - AMD, NVIDIA, Intel, Apple)
        find_package(OpenCL QUIET)
        if(OpenCL_FOUND)
            set(GPU_BACKEND "OPENCL")
        else()
            # Check for NVIDIA CUDA
            find_package(CUDAToolkit QUIET)
            if(CUDAToolkit_FOUND)
                set(GPU_BACKEND "CUDA")
            else()
                # Check for AMD ROCm/HIP
                find_package(hip QUIET)
                if(hip_FOUND)
                    set(GPU_BACKEND "HIP")
                else()
                    # Check for Vulkan
                    find_package(Vulkan QUIET)
                    if(Vulkan_FOUND)
                        set(GPU_BACKEND "VULKAN")
                    else()
                        # Check for Intel oneAPI
                        find_package(IntelDPCPP QUIET)
                        if(IntelDPCPP_FOUND)
                            set(GPU_BACKEND "ONEAPI")
                        endif()
                    endif()
                endif()
            endif()
        endif()
    endif()

    # Configure based on selected backend
    if(GPU_BACKEND STREQUAL "OPENCL")
        find_package(OpenCL REQUIRED)
        message(STATUS "GPU Acceleration: OpenCL (universal: AMD/NVIDIA/Intel/Apple)")
        set(GPU_ENABLED ON)
        set(GPU_LIBRARIES ${OpenCL_LIBRARIES})
        include_directories(${OpenCL_INCLUDE_DIRS})
        list(APPEND GPU_COMPILE_DEFINITIONS USE_OPENCL=1)

    elseif(GPU_BACKEND STREQUAL "VULKAN")
        find_package(Vulkan REQUIRED)
        message(STATUS "GPU Acceleration: Vulkan Compute (cross-platform)")
        set(GPU_ENABLED ON)
        set(GPU_LIBRARIES ${Vulkan_LIBRARIES})
        include_directories(${Vulkan_INCLUDE_DIRS})
        list(APPEND GPU_COMPILE_DEFINITIONS USE_VULKAN=1)

    elseif(GPU_BACKEND STREQUAL "CUDA")
        find_package(CUDAToolkit REQUIRED)
        message(STATUS "GPU Acceleration: CUDA (NVIDIA optimized)")
        set(GPU_ENABLED ON)
        enable_language(CUDA)
        set(GPU_LIBRARIES CUDA::cudart CUDA::cufft)
        include_directories(${CUDAToolkit_INCLUDE_DIRS})
        list(APPEND GPU_COMPILE_DEFINITIONS USE_CUDA=1)

    elseif(GPU_BACKEND STREQUAL "HIP")
        find_package(hip REQUIRED)
        find_package(rocfft REQUIRED)
        message(STATUS "GPU Acceleration: ROCm/HIP (AMD optimized)")
        set(GPU_ENABLED ON)
        set(GPU_LIBRARIES hip::host roc::rocfft)
        include_directories(${HIP_INCLUDE_DIRS} ${ROCM_PATH}/include)
        list(APPEND GPU_COMPILE_DEFINITIONS USE_HIP=1)

    elseif(GPU_BACKEND STREQUAL "ONEAPI")
        find_package(IntelDPCPP REQUIRED)
        message(STATUS "GPU Acceleration: Intel oneAPI (Intel optimized)")
        set(GPU_ENABLED ON)
        list(APPEND GPU_COMPILE_DEFINITIONS USE_ONEAPI=1)
    endif()

    # Add GPU source files if GPU is enabled
    if(GPU_ENABLED)
        list(APPEND GPU_SOURCE_FILES
            Source/GPU/GPUNoiseReduction.cpp
            Source/GPU/GPUSpectralProcessor.cpp
            Source/GPU/GPUBatchProcessor.cpp
            Source/GPU/GPUBackend.cpp
        )
        list(APPEND GPU_HEADER_FILES
            Source/GPU/GPUNoiseReduction.h
            Source/GPU/GPUSpectralProcessor.h
            Source/GPU/GPUBatchProcessor.h
            Source/GPU/GPUBackend.h
        )

        # GPU kernel files (runtime-loaded)
        set(GPU_KERNEL_FILES
            Source/GPU/kernels/spectral_subtraction.cl
            Source/GPU/kernels/spectral_subtraction.hip
            Source/GPU/kernels/spectral_subtraction.cu
        )

        message(STATUS "GPU source files will be compiled")
        message(STATUS "GPU kernels will be installed: ${GPU_KERNEL_FILES}")
    else()
        message(WARNING "GPU Acceleration requested but no backend found")
    endif()
else()
    message(STATUS "GPU Acceleration disabled by user")
    set(GPU_ENABLED OFF)
endif()

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
    Source/GUI/StandaloneWindow.cpp
    Source/GUI/SpectrogramDisplay.cpp
    Source/Processors/BatchProcessor.cpp
    Source/Processors/TrackDetector.cpp
    Source/Utils/AudioFileManager.cpp
    Source/Utils/LameMP3AudioFormat.cpp
    ${GPU_SOURCE_FILES}
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
    Source/GUI/StandaloneWindow.h
    Source/GUI/SpectrogramDisplay.h
    Source/GUI/GlowingKnobLookAndFeel.h
    Source/GUI/SpectrumAnalyzer.h
    Source/GUI/VintageVUMeter.h
    Source/Processors/BatchProcessor.h
    Source/Processors/TrackDetector.h
    Source/Utils/AudioFileManager.h
    Source/Utils/AudioUndoManager.h
    Source/Utils/LameMP3AudioFormat.h
    ${GPU_HEADER_FILES}
)

# Add the plugin target (VST3 only)
juce_add_plugin(VinylRestorationSuite
    PRODUCT_NAME "Vinyl Restoration Suite v1.5.2"
    COMPANY_NAME "flarkAUDIO"
    PLUGIN_MANUFACTURER_CODE Flrk
    PLUGIN_CODE Vrs1
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Restoration
)

# Add source files to VST3 plugin
target_sources(VinylRestorationSuite PRIVATE
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Preprocessor definitions for VST3
target_compile_definitions(VinylRestorationSuite PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_OPENGL=1
    ${GPU_COMPILE_DEFINITIONS}
    ${MP3_COMPILE_DEFINITIONS}
)

# Link JUCE modules for VST3
target_link_libraries(VinylRestorationSuite
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
        juce::juce_opengl
        ${GPU_LIBRARIES}
        ${MP3_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# Standalone Application (separate from VST3)
#==============================================================================

juce_add_gui_app(VinylRestorationSuiteStandalone
    PRODUCT_NAME "Vinyl Restoration Suite"
    COMPANY_NAME "flarkAUDIO"
)

# Add source files to standalone app
target_sources(VinylRestorationSuiteStandalone PRIVATE
    Source/Main.cpp
    Source/DSP/ClickRemoval.cpp
    Source/DSP/NoiseReduction.cpp
    Source/DSP/FilterBank.cpp
    Source/DSP/SpectralProcessor.cpp
    Source/GUI/WaveformDisplay.cpp
    Source/GUI/CorrectionListView.cpp
    Source/GUI/StandaloneWindow.cpp
    Source/GUI/SpectrogramDisplay.cpp
    Source/Processors/BatchProcessor.cpp
    Source/Processors/TrackDetector.cpp
    Source/Utils/AudioFileManager.cpp
    Source/Utils/LameMP3AudioFormat.cpp

    Source/DSP/ClickRemoval.h
    Source/DSP/NoiseReduction.h
    Source/DSP/FilterBank.h
    Source/DSP/SpectralProcessor.h
    Source/GUI/WaveformDisplay.h
    Source/GUI/CorrectionListView.h
    Source/GUI/StandaloneWindow.h
    Source/GUI/SpectrogramDisplay.h
    Source/Processors/BatchProcessor.h
    Source/Processors/TrackDetector.h
    Source/Utils/AudioFileManager.h
    Source/Utils/AudioUndoManager.h
    Source/Utils/LameMP3AudioFormat.h
)

# Preprocessor definitions for standalone
target_compile_definitions(VinylRestorationSuiteStandalone PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_OPENGL=1
    ${GPU_COMPILE_DEFINITIONS}
    ${MP3_COMPILE_DEFINITIONS}
)

# Link JUCE modules for standalone
target_link_libraries(VinylRestorationSuiteStandalone
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        ${GPU_LIBRARIES}
        ${MP3_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# Install GPU kernels alongside executables
#==============================================================================
if(GPU_ENABLED AND GPU_KERNEL_FILES)
    # Copy kernels to build directory for VST3
    add_custom_command(TARGET VinylRestorationSuite POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:VinylRestorationSuite>/kernels"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.cl
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.hip
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.cu
            "$<TARGET_FILE_DIR:VinylRestorationSuite>/kernels/"
        COMMENT "Installing GPU kernels for VST3"
    )

    # Copy kernels to build directory for Standalone
    add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/kernels"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.cl
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.hip
            ${CMAKE_SOURCE_DIR}/Source/GPU/kernels/spectral_subtraction.cu
            "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/kernels/"
        COMMENT "Installing GPU kernels for Standalone"
    )

    message(STATUS "GPU kernels will be copied to build directory")
endif()
