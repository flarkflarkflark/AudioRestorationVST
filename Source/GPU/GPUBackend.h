#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * GPU Backend Abstraction Layer
 *
 * Provides unified interface for GPU compute across different platforms:
 * - OpenCL (AMD, NVIDIA, Intel, Apple - universal)
 * - CUDA (NVIDIA - optimized)
 * - HIP/ROCm (AMD - optimized)
 * - Vulkan Compute (cross-platform, modern)
 * - Intel oneAPI (Intel - optimized)
 *
 * Auto-detects available backend at compile time via CMake definitions.
 */

// Backend detection (set by CMake)
#if defined(USE_CUDA)
    #define GPU_BACKEND_CUDA 1
    #include <cuda_runtime.h>
    #include <cufft.h>
#elif defined(USE_HIP)
    #define GPU_BACKEND_HIP 1
    #include <hip/hip_runtime.h>
    #include <rocfft/rocfft.h>
#elif defined(USE_OPENCL)
    #define GPU_BACKEND_OPENCL 1
    #ifdef __APPLE__
        #include <OpenCL/opencl.h>
    #else
        #include <CL/cl.h>
    #endif
#elif defined(USE_VULKAN)
    #define GPU_BACKEND_VULKAN 1
    #include <vulkan/vulkan.h>
#elif defined(USE_ONEAPI)
    #define GPU_BACKEND_ONEAPI 1
    #include <CL/sycl.hpp>
#else
    #define GPU_BACKEND_NONE 1
#endif

namespace GPUBackend
{
    //==============================================================================
    /** GPU device information */
    struct DeviceInfo
    {
        std::string name;
        std::string vendor;
        size_t totalMemory;
        size_t availableMemory;
        int computeUnits;
        int maxWorkGroupSize;
        std::string backendName;
    };

    //==============================================================================
    /** Initialize GPU backend and select device */
    bool initialize();

    /** Cleanup GPU resources */
    void shutdown();

    /** Check if GPU is available and initialized */
    bool isAvailable();

    /** Get current device information */
    DeviceInfo getDeviceInfo();

    /** Get backend name (OpenCL, CUDA, HIP, etc.) */
    std::string getBackendName();

    //==============================================================================
    /** GPU memory buffer wrapper */
    class GPUBuffer
    {
    public:
        GPUBuffer() = default;
        ~GPUBuffer();

        /** Allocate GPU memory */
        bool allocate(size_t sizeInBytes);

        /** Copy data from host to GPU */
        bool upload(const void* hostData, size_t sizeInBytes);

        /** Copy data from GPU to host */
        bool download(void* hostData, size_t sizeInBytes);

        /** Free GPU memory */
        void release();

        /** Get buffer size in bytes */
        size_t getSize() const { return size; }

        /** Get native GPU buffer handle */
        void* getNativeHandle() const { return nativeBuffer; }

    private:
        void* nativeBuffer = nullptr;
        size_t size = 0;

    private:
        GPUBuffer(const GPUBuffer&) = delete;
        GPUBuffer& operator=(const GPUBuffer&) = delete;
    };

    //==============================================================================
    /** GPU FFT wrapper - unified interface for cuFFT/rocFFT/clFFT/VkFFT */
    class GPUFFT
    {
    public:
        enum class Direction
        {
            Forward,
            Inverse
        };

        GPUFFT() = default;
        ~GPUFFT();

        /** Create FFT plan for given size */
        bool createPlan(int fftSize, int batchSize = 1);

        /** Execute forward FFT (real input -> complex output) */
        bool executeForward(GPUBuffer& input, GPUBuffer& output);

        /** Execute inverse FFT (complex input -> real output) */
        bool executeInverse(GPUBuffer& input, GPUBuffer& output);

        /** Destroy FFT plan */
        void release();

        /** Get FFT size */
        int getFFTSize() const { return fftSize; }

    private:
        void* fftPlan = nullptr;
        int fftSize = 0;
        int batchSize = 1;

    private:
        GPUFFT(const GPUFFT&) = delete;
        GPUFFT& operator=(const GPUFFT&) = delete;
    };

    //==============================================================================
    /** GPU kernel execution - for custom compute kernels */
    class GPUKernel
    {
    public:
        GPUKernel() = default;
        ~GPUKernel();

        /** Load kernel from source code string */
        bool loadFromSource(const std::string& kernelSource, const std::string& kernelName);

        /** Set kernel argument */
        bool setArgument(int index, GPUBuffer& buffer);
        bool setArgument(int index, float value);
        bool setArgument(int index, int value);

        /** Execute kernel with given work size */
        bool execute(size_t globalWorkSize, size_t localWorkSize = 256);

        /** Release kernel resources */
        void release();

    private:
        void* nativeKernel = nullptr;
        void* nativeProgram = nullptr;

    private:
        GPUKernel(const GPUKernel&) = delete;
        GPUKernel& operator=(const GPUKernel&) = delete;
    };

    //==============================================================================
    /** Synchronization - wait for GPU operations to complete */
    void synchronize();

    /** Get last error message */
    std::string getLastError();

} // namespace GPUBackend
