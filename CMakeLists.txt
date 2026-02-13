cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(VinylRestorationSuite VERSION 1.6.2)

# Add JUCE as a subdirectory (using local submodule)
add_subdirectory(JUCE)

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#==============================================================================
# MP3 Support (LAME encoding + mpg123 decoding)
#==============================================================================
set(VRS_USE_VCPKG_DEFAULT OFF)
if(DEFINED ENV{VCPKG_ROOT})
    set(VRS_USE_VCPKG_DEFAULT ON)
endif()
option(ENABLE_MP3_SUPPORT "Enable MP3 read/write support via LAME and mpg123" ON)
option(VRS_USE_VCPKG "Use vcpkg for MP3 dependencies when available" ${VRS_USE_VCPKG_DEFAULT})

set(MP3_ENABLED OFF)
set(MP3_LIBRARIES "")
set(MP3_COMPILE_DEFINITIONS "")

if(ENABLE_MP3_SUPPORT)
    if(VRS_USE_VCPKG)
        find_package(mpg123 CONFIG QUIET)
        find_package(MPG123 CONFIG QUIET)
        find_package(mp3lame CONFIG QUIET)
        find_package(MP3LAME CONFIG QUIET)
        find_package(lame CONFIG QUIET)
        find_package(LAME CONFIG QUIET)

        if(TARGET mpg123::mpg123)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES mpg123::mpg123)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_MPG123=1)
        elseif(TARGET mpg123::libmpg123)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES mpg123::libmpg123)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_MPG123=1)
        elseif(TARGET MPG123::libmpg123)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES MPG123::libmpg123)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_MPG123=1)
        elseif(TARGET MPG123::MPG123)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES MPG123::MPG123)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_MPG123=1)
        endif()

        if(TARGET mp3lame::mp3lame)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES mp3lame::mp3lame)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
        elseif(TARGET MP3LAME::mp3lame)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES MP3LAME::mp3lame)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
        elseif(TARGET MP3LAME::MP3LAME)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES MP3LAME::MP3LAME)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
        elseif(TARGET LAME::mp3lame)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES LAME::mp3lame)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
        elseif(TARGET lame::lame)
            set(MP3_ENABLED ON)
            list(APPEND MP3_LIBRARIES lame::lame)
            list(APPEND MP3_COMPILE_DEFINITIONS USE_LAME=1)
        endif()
    endif()

    if(NOT MP3_ENABLED)
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

#==============================================================================
# ONNX Runtime (AI Denoiser)
#==============================================================================
option(ENABLE_ONNX_RUNTIME "Enable ONNX Runtime for AI denoise" ON)
set(ONNXRUNTIME_VERSION "1.17.3" CACHE STRING "ONNX Runtime version")
set(ONNXRUNTIME_DIR "" CACHE PATH "Override ONNX Runtime root directory")
option(ONNXRUNTIME_USE_CUDA "Use CUDA-enabled ONNX Runtime on Linux" OFF)
option(ONNXRUNTIME_USE_ROCM "Use ROCm-enabled ONNX Runtime on Linux" OFF)
option(ONNXRUNTIME_USE_DML "Enable DirectML EP on Windows" ON)
option(ONNXRUNTIME_USE_COREML "Enable CoreML EP on macOS" ON)

set(ONNXRUNTIME_INCLUDE_DIR "")
set(ONNXRUNTIME_LIBRARY "")
set(ONNXRUNTIME_RUNTIME_LIBS "")
set(ONNXRUNTIME_COMPILE_DEFINITIONS "")

if(ENABLE_ONNX_RUNTIME)
    if(NOT ONNXRUNTIME_DIR)
        if(WIN32)
            set(ONNXRUNTIME_PACKAGE "onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip")
            if(ONNXRUNTIME_USE_DML)
                set(ONNXRUNTIME_USE_DML_NUGET ON)
            endif()
        elseif(APPLE)
            set(ONNXRUNTIME_PACKAGE "onnxruntime-osx-universal2-${ONNXRUNTIME_VERSION}.tgz")
        else()
            if(ONNXRUNTIME_USE_CUDA)
                set(ONNXRUNTIME_PACKAGE "onnxruntime-linux-x64-gpu-${ONNXRUNTIME_VERSION}.tgz")
            elseif(ONNXRUNTIME_USE_ROCM)
                set(ONNXRUNTIME_PACKAGE "onnxruntime-linux-x64-rocm-${ONNXRUNTIME_VERSION}.tgz")
            else()
                set(ONNXRUNTIME_PACKAGE "onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")
            endif()
        endif()

        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_PACKAGE}")
        set(ONNXRUNTIME_ROOT "${CMAKE_BINARY_DIR}/onnxruntime")
        set(ONNXRUNTIME_ARCHIVE "${CMAKE_BINARY_DIR}/${ONNXRUNTIME_PACKAGE}")

        string(REPLACE ".zip" "" ONNXRUNTIME_EXTRACT_DIR "${ONNXRUNTIME_PACKAGE}")
        string(REPLACE ".tgz" "" ONNXRUNTIME_EXTRACT_DIR "${ONNXRUNTIME_EXTRACT_DIR}")

        if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h" OR (WIN32 AND ONNXRUNTIME_USE_DML AND NOT EXISTS "${ONNXRUNTIME_ROOT}/include/dml_provider_factory.h"))
            message(STATUS "Downloading ONNX Runtime from ${ONNXRUNTIME_URL}")
            file(DOWNLOAD "${ONNXRUNTIME_URL}" "${ONNXRUNTIME_ARCHIVE}" SHOW_PROGRESS STATUS ONNXRUNTIME_DOWNLOAD_STATUS)
            list(GET ONNXRUNTIME_DOWNLOAD_STATUS 0 ONNXRUNTIME_DOWNLOAD_CODE)
            if(NOT ONNXRUNTIME_DOWNLOAD_CODE EQUAL 0 AND ONNXRUNTIME_FALLBACK_PACKAGE)
                set(ONNXRUNTIME_PACKAGE "${ONNXRUNTIME_FALLBACK_PACKAGE}")
                set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_PACKAGE}")
                set(ONNXRUNTIME_ARCHIVE "${CMAKE_BINARY_DIR}/${ONNXRUNTIME_PACKAGE}")
                string(REPLACE ".zip" "" ONNXRUNTIME_EXTRACT_DIR "${ONNXRUNTIME_PACKAGE}")
                string(REPLACE ".tgz" "" ONNXRUNTIME_EXTRACT_DIR "${ONNXRUNTIME_EXTRACT_DIR}")
                message(WARNING "DirectML package download failed, falling back to ${ONNXRUNTIME_PACKAGE}")
                file(DOWNLOAD "${ONNXRUNTIME_URL}" "${ONNXRUNTIME_ARCHIVE}" SHOW_PROGRESS STATUS ONNXRUNTIME_DOWNLOAD_STATUS)
            endif()
            if(WIN32)
                execute_process(
                    COMMAND powershell -ExecutionPolicy Bypass -Command
                        "Expand-Archive -Force '${ONNXRUNTIME_ARCHIVE}' '${CMAKE_BINARY_DIR}'"
                    RESULT_VARIABLE ONNXRUNTIME_EXTRACT_RESULT
                )
            else()
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E tar xvf "${ONNXRUNTIME_ARCHIVE}"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                    RESULT_VARIABLE ONNXRUNTIME_EXTRACT_RESULT
                )
            endif()
            if(NOT ONNXRUNTIME_EXTRACT_RESULT EQUAL 0)
                message(WARNING "Failed to extract ONNX Runtime archive (${ONNXRUNTIME_EXTRACT_RESULT})")
            endif()
            execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${ONNXRUNTIME_ROOT}")
            execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                            "${CMAKE_BINARY_DIR}/${ONNXRUNTIME_EXTRACT_DIR}"
                            "${ONNXRUNTIME_ROOT}")
            if(WIN32 AND ONNXRUNTIME_USE_DML AND NOT EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime_providers_dml.dll")
                set(ONNXRUNTIME_NUGET_URL "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/${ONNXRUNTIME_VERSION}")
                set(ONNXRUNTIME_NUGET_ARCHIVE "${CMAKE_BINARY_DIR}/Microsoft.ML.OnnxRuntime.DirectML.${ONNXRUNTIME_VERSION}.zip")
                set(ONNXRUNTIME_NUGET_EXTRACT_DIR "${CMAKE_BINARY_DIR}/onnxruntime_directml_nuget")
                set(ONNXRUNTIME_NUGET_DML_DLL "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/runtimes/win-x64/native/onnxruntime_providers_dml.dll")
                if(NOT EXISTS "${ONNXRUNTIME_NUGET_DML_DLL}")
                    message(STATUS "Downloading ONNX Runtime DirectML NuGet from ${ONNXRUNTIME_NUGET_URL}")
                    file(DOWNLOAD "${ONNXRUNTIME_NUGET_URL}" "${ONNXRUNTIME_NUGET_ARCHIVE}" SHOW_PROGRESS STATUS ONNXRUNTIME_NUGET_STATUS)
                    execute_process(
                        COMMAND powershell -ExecutionPolicy Bypass -Command
                            "Expand-Archive -Force '${ONNXRUNTIME_NUGET_ARCHIVE}' '${ONNXRUNTIME_NUGET_EXTRACT_DIR}'"
                        RESULT_VARIABLE ONNXRUNTIME_NUGET_EXTRACT_RESULT
                    )
                    if(NOT ONNXRUNTIME_NUGET_EXTRACT_RESULT EQUAL 0)
                        message(WARNING "Failed to extract ONNX Runtime DirectML NuGet (${ONNXRUNTIME_NUGET_EXTRACT_RESULT})")
                    endif()
                endif()
                set(ONNXRUNTIME_NUGET_INCLUDE_DIR "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/build/native/include")
                if(EXISTS "${ONNXRUNTIME_NUGET_INCLUDE_DIR}/dml_provider_factory.h")
                    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                                    "${ONNXRUNTIME_NUGET_INCLUDE_DIR}"
                                    "${ONNXRUNTIME_ROOT}/include")
                    if(EXISTS "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/build/native/lib/x64")
                        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                                        "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/build/native/lib/x64"
                                        "${ONNXRUNTIME_ROOT}/lib")
                    elseif(EXISTS "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/build/native/lib")
                        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                                        "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/build/native/lib"
                                        "${ONNXRUNTIME_ROOT}/lib")
                    endif()
                    if(EXISTS "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/runtimes/win-x64/native")
                        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                                        "${ONNXRUNTIME_NUGET_EXTRACT_DIR}/runtimes/win-x64/native"
                                        "${ONNXRUNTIME_ROOT}/lib")
                    endif()
                else()
                    message(WARNING "DirectML NuGet package missing DML headers; DML will be unavailable.")
                endif()
            endif()
        endif()

        set(ONNXRUNTIME_DIR "${ONNXRUNTIME_ROOT}")
    endif()

    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")

    if(WIN32)
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib")
        set(ONNXRUNTIME_RUNTIME_DIR "${ONNXRUNTIME_DIR}/lib")
        set(QNN_BACKEND_DLLS "")
        if(EXISTS "${ONNXRUNTIME_DIR}/bin")
            set(ONNXRUNTIME_RUNTIME_DIR "${ONNXRUNTIME_DIR}/bin")
        endif()
        if(EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime.dll")
        endif()
        if(EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_shared.dll")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_shared.dll")
        endif()
        if(EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_dml.dll")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_dml.dll")
        endif()
        if(EXISTS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_qnn.dll")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_RUNTIME_DIR}/onnxruntime_providers_qnn.dll")
        endif()
        set(QNN_BACKEND_PATH "$ENV{VRS_QNN_BACKEND_PATH}")
        if(QNN_BACKEND_PATH AND EXISTS "${QNN_BACKEND_PATH}")
            get_filename_component(QNN_BACKEND_DIR "${QNN_BACKEND_PATH}" DIRECTORY)
            list(APPEND QNN_BACKEND_DLLS "${QNN_BACKEND_PATH}")
            file(GLOB QNN_EXTRA_DLLS "${QNN_BACKEND_DIR}/Qnn*.dll")
            if(QNN_EXTRA_DLLS)
                list(APPEND QNN_BACKEND_DLLS ${QNN_EXTRA_DLLS})
            endif()
            if(EXISTS "${QNN_BACKEND_DIR}/onnxruntime_providers_qnn.dll")
                list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${QNN_BACKEND_DIR}/onnxruntime_providers_qnn.dll")
            endif()
        endif()
        if(ONNXRUNTIME_USE_DML)
            list(APPEND ONNXRUNTIME_COMPILE_DEFINITIONS ONNX_RUNTIME_USE_DML=1)
        endif()
    elseif(APPLE)
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_DIR}/lib/libonnxruntime.dylib")
        if(EXISTS "${ONNXRUNTIME_LIBRARY}")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_LIBRARY}")
        endif()
        if(ONNXRUNTIME_USE_COREML)
            set(ONNXRUNTIME_COREML_HEADER "${ONNXRUNTIME_DIR}/include/coreml_provider_factory.h")
            if(EXISTS "${ONNXRUNTIME_COREML_HEADER}")
                list(APPEND ONNXRUNTIME_COMPILE_DEFINITIONS ONNX_RUNTIME_USE_COREML=1)
            else()
                message(WARNING "CoreML provider header not found; disabling CoreML EP.")
                set(ONNXRUNTIME_USE_COREML OFF)
            endif()
        endif()
    else()
        set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so")
        if(EXISTS "${ONNXRUNTIME_LIBRARY}")
            list(APPEND ONNXRUNTIME_RUNTIME_LIBS "${ONNXRUNTIME_LIBRARY}")
        endif()
        if(ONNXRUNTIME_USE_CUDA)
            list(APPEND ONNXRUNTIME_COMPILE_DEFINITIONS ONNX_RUNTIME_USE_CUDA=1)
        endif()
        if(ONNXRUNTIME_USE_ROCM)
            list(APPEND ONNXRUNTIME_COMPILE_DEFINITIONS ONNX_RUNTIME_USE_ROCM=1)
        endif()
    endif()

    if(NOT ONNXRUNTIME_LIBRARY OR NOT EXISTS "${ONNXRUNTIME_LIBRARY}")
        message(WARNING "ONNX Runtime library not found; AI denoise will be disabled.")
        set(ENABLE_ONNX_RUNTIME OFF)
    else()
        message(STATUS "ONNX Runtime enabled: ${ONNXRUNTIME_DIR}")
        list(APPEND ONNXRUNTIME_COMPILE_DEFINITIONS ENABLE_ONNX_RUNTIME=1)
    endif()
endif()

# Source files
set(SOURCE_FILES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/ClickRemoval.cpp
    Source/DSP/Decrackle.cpp
    Source/DSP/NoiseReduction.cpp
    Source/DSP/FilterBank.cpp
    Source/DSP/OnnxDenoiser.cpp
    Source/DSP/SpectralProcessor.cpp
    Source/GUI/WaveformDisplay.cpp
    Source/GUI/CorrectionListView.cpp
    Source/GUI/SettingsComponent.cpp
    Source/GUI/StandaloneWindow.cpp
    Source/GUI/SpectrogramDisplay.cpp
    Source/Processors/BatchProcessor.cpp
    Source/Processors/TrackDetector.cpp
    Source/Utils/AudioFileManager.cpp
    Source/Utils/LameMP3AudioFormat.cpp
    Source/Utils/SettingsManager.cpp
    ${GPU_SOURCE_FILES}
)

set(HEADER_FILES
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/DSP/ClickRemoval.h
    Source/DSP/Decrackle.h
    Source/DSP/NoiseReduction.h
    Source/DSP/FilterBank.h
    Source/DSP/OnnxDenoiser.h
    Source/DSP/SpectralProcessor.h
    Source/GUI/WaveformDisplay.h
    Source/GUI/CorrectionListView.h
    Source/GUI/SettingsComponent.h
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
    Source/Utils/SettingsManager.h
    ${GPU_HEADER_FILES}
)

option(VRS_COPY_PLUGIN_AFTER_BUILD "Copy VST3 to system folder after build" OFF)

# Add the plugin target (VST3 only)
juce_add_plugin(VinylRestorationSuite
    PRODUCT_NAME "Vinyl Restoration Suite v1.6.2"
    COMPANY_NAME "flarkAUDIO"
    PLUGIN_MANUFACTURER_CODE Flrk
    PLUGIN_CODE Vrs1
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${VRS_COPY_PLUGIN_AFTER_BUILD}
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
    ${ONNXRUNTIME_COMPILE_DEFINITIONS}
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
        ${ONNXRUNTIME_LIBRARY}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(ENABLE_ONNX_RUNTIME)
    target_include_directories(VinylRestorationSuite PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
endif()

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
    Source/DSP/Decrackle.cpp
    Source/DSP/NoiseReduction.cpp
    Source/DSP/FilterBank.cpp
    Source/DSP/OnnxDenoiser.cpp
    Source/DSP/SpectralProcessor.cpp
    Source/GUI/WaveformDisplay.cpp
    Source/GUI/CorrectionListView.cpp
    Source/GUI/SettingsComponent.cpp
    Source/GUI/StandaloneWindow.cpp
    Source/GUI/SpectrogramDisplay.cpp
    Source/Processors/BatchProcessor.cpp
    Source/Processors/TrackDetector.cpp
    Source/Utils/AudioFileManager.cpp
    Source/Utils/LameMP3AudioFormat.cpp
    Source/Utils/SettingsManager.cpp

    Source/DSP/ClickRemoval.h
    Source/DSP/Decrackle.h
    Source/DSP/NoiseReduction.h
    Source/DSP/FilterBank.h
    Source/DSP/OnnxDenoiser.h
    Source/DSP/SpectralProcessor.h
    Source/GUI/WaveformDisplay.h
    Source/GUI/CorrectionListView.h
    Source/GUI/SettingsComponent.h
    Source/GUI/StandaloneWindow.h
    Source/GUI/SpectrogramDisplay.h
    Source/Processors/BatchProcessor.h
    Source/Processors/TrackDetector.h
    Source/Utils/AudioFileManager.h
    Source/Utils/AudioUndoManager.h
    Source/Utils/LameMP3AudioFormat.h
    Source/Utils/SettingsManager.h
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
    ${ONNXRUNTIME_COMPILE_DEFINITIONS}
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
        ${ONNXRUNTIME_LIBRARY}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(ENABLE_ONNX_RUNTIME)
    target_include_directories(VinylRestorationSuiteStandalone PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
endif()

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

# Copy vcpkg runtime DLLs (MP3 and dependencies)
if(WIN32 AND VRS_USE_VCPKG AND MP3_ENABLED)
    add_custom_command(TARGET VinylRestorationSuite_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEST_DIR="$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>"
            -DRUNTIME_DLLS="$<TARGET_RUNTIME_DLLS:VinylRestorationSuite_VST3>"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
        COMMENT "Installing vcpkg runtime libraries for VST3"
    )

    add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDEST_DIR="$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>"
            -DRUNTIME_DLLS="$<TARGET_RUNTIME_DLLS:VinylRestorationSuiteStandalone>"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
        COMMENT "Installing vcpkg runtime libraries for Standalone"
    )

    set(VCPKG_BIN_DIR "")
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_BIN_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/bin")
    endif()

    set(MP3_RUNTIME_DLLS "")
    foreach(mp3_dll libmp3lame.dll libmp3lame.DLL mpg123.dll out123.dll syn123.dll)
        if(VCPKG_BIN_DIR AND EXISTS "${VCPKG_BIN_DIR}/${mp3_dll}")
            list(APPEND MP3_RUNTIME_DLLS "${VCPKG_BIN_DIR}/${mp3_dll}")
        endif()
    endforeach()

    if(MP3_RUNTIME_DLLS)
        add_custom_command(TARGET VinylRestorationSuite_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MP3_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>/"
            COMMENT "Installing MP3 runtime libraries for VST3"
        )

        add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MP3_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/"
            COMMENT "Installing MP3 runtime libraries for Standalone"
        )
    else()
        message(WARNING "MP3 runtime DLLs not found under vcpkg bin; MP3 may fail at runtime.")
    endif()
endif()

#==============================================================================
# Install ONNX Runtime libs and model
#==============================================================================
if(ENABLE_ONNX_RUNTIME)
    if(ONNXRUNTIME_RUNTIME_LIBS)
        add_custom_command(TARGET VinylRestorationSuite_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ONNXRUNTIME_RUNTIME_LIBS}
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>/"
            COMMENT "Installing ONNX Runtime runtime libraries for VST3"
        )

        add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ONNXRUNTIME_RUNTIME_LIBS}
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/"
            COMMENT "Installing ONNX Runtime runtime libraries for Standalone"
        )
    endif()

    if(QNN_BACKEND_DLLS)
        add_custom_command(TARGET VinylRestorationSuite_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QNN_BACKEND_DLLS}
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>/"
            COMMENT "Installing QNN backend libraries for VST3"
        )

        add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QNN_BACKEND_DLLS}
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/"
            COMMENT "Installing QNN backend libraries for Standalone"
        )
    endif()

    set(RNNOISE_MODEL_FILES "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/models/rnnoise_48k.onnx")
        list(APPEND RNNOISE_MODEL_FILES "${CMAKE_SOURCE_DIR}/models/rnnoise_48k.onnx")
    endif()
    file(GLOB RNNOISE_OPT_MODELS "${CMAKE_SOURCE_DIR}/models/rnnoise_48k_olive_*.onnx")
    if(RNNOISE_OPT_MODELS)
        list(APPEND RNNOISE_MODEL_FILES ${RNNOISE_OPT_MODELS})
    endif()

    if(RNNOISE_MODEL_FILES)
        add_custom_command(TARGET VinylRestorationSuite_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RNNOISE_MODEL_FILES}
                "$<TARGET_FILE_DIR:VinylRestorationSuite_VST3>/models/"
            COMMENT "Installing RNNoise model(s) for VST3"
        )

        add_custom_command(TARGET VinylRestorationSuiteStandalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RNNOISE_MODEL_FILES}
                "$<TARGET_FILE_DIR:VinylRestorationSuiteStandalone>/models/"
            COMMENT "Installing RNNoise model(s) for Standalone"
        )
    else()
        message(WARNING "RNNoise model not found under ${CMAKE_SOURCE_DIR}/models")
    endif()
endif()
