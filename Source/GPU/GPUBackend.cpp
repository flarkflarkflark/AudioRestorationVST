#include "GPUBackend.h"
#include <juce_core/juce_core.h>

namespace GPUBackend
{
    //==============================================================================
    // Global state
    static bool g_initialized = false;
    static std::string g_lastError;
    static DeviceInfo g_deviceInfo;

    #if defined(GPU_BACKEND_OPENCL)
        static cl_context g_clContext = nullptr;
        static cl_command_queue g_clQueue = nullptr;
        static cl_device_id g_clDevice = nullptr;
    #elif defined(GPU_BACKEND_CUDA)
        static int g_cudaDevice = 0;
    #elif defined(GPU_BACKEND_HIP)
        static int g_hipDevice = 0;
    #elif defined(GPU_BACKEND_VULKAN)
        static VkInstance g_vkInstance = VK_NULL_HANDLE;
        static VkDevice g_vkDevice = VK_NULL_HANDLE;
        static VkPhysicalDevice g_vkPhysicalDevice = VK_NULL_HANDLE;
    #endif

    //==============================================================================
    bool initialize()
    {
        if (g_initialized)
            return true;

        juce::Logger::writeToLog("GPU Backend: Initializing...");

        #if defined(GPU_BACKEND_OPENCL)
            // OpenCL initialization
            cl_int err;
            cl_platform_id platform;
            cl_uint numPlatforms;

            err = clGetPlatformIDs(1, &platform, &numPlatforms);
            if (err != CL_SUCCESS || numPlatforms == 0)
            {
                g_lastError = "No OpenCL platforms found";
                juce::Logger::writeToLog("GPU Backend: " + g_lastError);
                return false;
            }

            err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &g_clDevice, nullptr);
            if (err != CL_SUCCESS)
            {
                g_lastError = "No OpenCL GPU devices found";
                juce::Logger::writeToLog("GPU Backend: " + g_lastError);
                return false;
            }

            g_clContext = clCreateContext(nullptr, 1, &g_clDevice, nullptr, nullptr, &err);
            if (err != CL_SUCCESS)
            {
                g_lastError = "Failed to create OpenCL context";
                return false;
            }

            #if defined(CL_VERSION_2_0)
                g_clQueue = clCreateCommandQueueWithProperties(g_clContext, g_clDevice, nullptr, &err);
            #else
                g_clQueue = clCreateCommandQueue(g_clContext, g_clDevice, 0, &err);
            #endif

            if (err != CL_SUCCESS)
            {
                g_lastError = "Failed to create OpenCL command queue";
                clReleaseContext(g_clContext);
                return false;
            }

            // Get device info
            char deviceName[256];
            char vendor[256];
            cl_ulong globalMem;
            cl_uint computeUnits;
            size_t maxWorkGroupSize;

            clGetDeviceInfo(g_clDevice, CL_DEVICE_NAME, sizeof(deviceName), deviceName, nullptr);
            clGetDeviceInfo(g_clDevice, CL_DEVICE_VENDOR, sizeof(vendor), vendor, nullptr);
            clGetDeviceInfo(g_clDevice, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &globalMem, nullptr);
            clGetDeviceInfo(g_clDevice, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &computeUnits, nullptr);
            clGetDeviceInfo(g_clDevice, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &maxWorkGroupSize, nullptr);

            g_deviceInfo.name = deviceName;
            g_deviceInfo.vendor = vendor;
            g_deviceInfo.totalMemory = globalMem;
            g_deviceInfo.availableMemory = globalMem; // Simplified
            g_deviceInfo.computeUnits = computeUnits;
            g_deviceInfo.maxWorkGroupSize = static_cast<int>(maxWorkGroupSize);
            g_deviceInfo.backendName = "OpenCL";

            g_initialized = true;
            juce::Logger::writeToLog("GPU Backend: OpenCL initialized (" + g_deviceInfo.name + ")");
            return true;

        #elif defined(GPU_BACKEND_CUDA)
            // CUDA initialization
            cudaError_t err = cudaSetDevice(0);
            if (err != cudaSuccess)
            {
                g_lastError = "CUDA device not found";
                return false;
            }

            cudaDeviceProp prop;
            cudaGetDeviceProperties(&prop, 0);

            g_deviceInfo.name = prop.name;
            g_deviceInfo.vendor = "NVIDIA";
            g_deviceInfo.totalMemory = prop.totalGlobalMem;
            g_deviceInfo.availableMemory = prop.totalGlobalMem;
            g_deviceInfo.computeUnits = prop.multiProcessorCount;
            g_deviceInfo.maxWorkGroupSize = prop.maxThreadsPerBlock;
            g_deviceInfo.backendName = "CUDA";

            g_initialized = true;
            juce::Logger::writeToLog("GPU Backend: CUDA initialized (" + g_deviceInfo.name + ")");
            return true;

        #elif defined(GPU_BACKEND_HIP)
            // HIP initialization
            hipError_t err = hipSetDevice(0);
            if (err != hipSuccess)
            {
                g_lastError = "HIP device not found";
                return false;
            }

            hipDeviceProp_t prop;
            hipGetDeviceProperties(&prop, 0);

            g_deviceInfo.name = prop.name;
            g_deviceInfo.vendor = "AMD";
            g_deviceInfo.totalMemory = prop.totalGlobalMem;
            g_deviceInfo.availableMemory = prop.totalGlobalMem;
            g_deviceInfo.computeUnits = prop.multiProcessorCount;
            g_deviceInfo.maxWorkGroupSize = prop.maxThreadsPerBlock;
            g_deviceInfo.backendName = "ROCm/HIP";

            g_initialized = true;
            juce::Logger::writeToLog("GPU Backend: ROCm/HIP initialized (" + g_deviceInfo.name + ")");
            return true;

        #elif defined(GPU_BACKEND_VULKAN)
            // Vulkan initialization
            // TODO: Implement Vulkan initialization
            g_lastError = "Vulkan backend not yet implemented";
            return false;

        #elif defined(GPU_BACKEND_ONEAPI)
            // Intel oneAPI initialization
            // TODO: Implement oneAPI initialization
            g_lastError = "oneAPI backend not yet implemented";
            return false;

        #else
            g_lastError = "No GPU backend compiled";
            return false;
        #endif
    }

    void shutdown()
    {
        if (!g_initialized)
            return;

        #if defined(GPU_BACKEND_OPENCL)
            if (g_clQueue) clReleaseCommandQueue(g_clQueue);
            if (g_clContext) clReleaseContext(g_clContext);
            g_clQueue = nullptr;
            g_clContext = nullptr;
            g_clDevice = nullptr;
        #elif defined(GPU_BACKEND_CUDA)
            cudaDeviceReset();
        #elif defined(GPU_BACKEND_HIP)
            hipDeviceReset();
        #endif

        g_initialized = false;
        juce::Logger::writeToLog("GPU Backend: Shutdown");
    }

    bool isAvailable()
    {
        return g_initialized;
    }

    DeviceInfo getDeviceInfo()
    {
        return g_deviceInfo;
    }

    std::string getBackendName()
    {
        #if defined(GPU_BACKEND_OPENCL)
            return "OpenCL";
        #elif defined(GPU_BACKEND_CUDA)
            return "CUDA";
        #elif defined(GPU_BACKEND_HIP)
            return "ROCm/HIP";
        #elif defined(GPU_BACKEND_VULKAN)
            return "Vulkan";
        #elif defined(GPU_BACKEND_ONEAPI)
            return "oneAPI";
        #else
            return "None";
        #endif
    }

    void synchronize()
    {
        #if defined(GPU_BACKEND_OPENCL)
            if (g_clQueue)
                clFinish(g_clQueue);
        #elif defined(GPU_BACKEND_CUDA)
            cudaDeviceSynchronize();
        #elif defined(GPU_BACKEND_HIP)
            hipDeviceSynchronize();
        #endif
    }

    std::string getLastError()
    {
        return g_lastError;
    }

    //==============================================================================
    // GPUBuffer implementation
    //==============================================================================
    GPUBuffer::~GPUBuffer()
    {
        release();
    }

    bool GPUBuffer::allocate(size_t sizeInBytes)
    {
        release();

        size = sizeInBytes;

        #if defined(GPU_BACKEND_OPENCL)
            cl_int err;
            nativeBuffer = clCreateBuffer(g_clContext, CL_MEM_READ_WRITE, sizeInBytes, nullptr, &err);
            return (err == CL_SUCCESS);
        #elif defined(GPU_BACKEND_CUDA)
            return (cudaMalloc(&nativeBuffer, sizeInBytes) == cudaSuccess);
        #elif defined(GPU_BACKEND_HIP)
            return (hipMalloc(&nativeBuffer, sizeInBytes) == hipSuccess);
        #else
            return false;
        #endif
    }

    bool GPUBuffer::upload(const void* hostData, size_t sizeInBytes)
    {
        if (!nativeBuffer || sizeInBytes > size)
            return false;

        #if defined(GPU_BACKEND_OPENCL)
            cl_int err = clEnqueueWriteBuffer(g_clQueue, static_cast<cl_mem>(nativeBuffer),
                                             CL_TRUE, 0, sizeInBytes, hostData, 0, nullptr, nullptr);
            return (err == CL_SUCCESS);
        #elif defined(GPU_BACKEND_CUDA)
            return (cudaMemcpy(nativeBuffer, hostData, sizeInBytes, cudaMemcpyHostToDevice) == cudaSuccess);
        #elif defined(GPU_BACKEND_HIP)
            return (hipMemcpy(nativeBuffer, hostData, sizeInBytes, hipMemcpyHostToDevice) == hipSuccess);
        #else
            return false;
        #endif
    }

    bool GPUBuffer::download(void* hostData, size_t sizeInBytes)
    {
        if (!nativeBuffer || sizeInBytes > size)
            return false;

        #if defined(GPU_BACKEND_OPENCL)
            cl_int err = clEnqueueReadBuffer(g_clQueue, static_cast<cl_mem>(nativeBuffer),
                                            CL_TRUE, 0, sizeInBytes, hostData, 0, nullptr, nullptr);
            return (err == CL_SUCCESS);
        #elif defined(GPU_BACKEND_CUDA)
            return (cudaMemcpy(hostData, nativeBuffer, sizeInBytes, cudaMemcpyDeviceToHost) == cudaSuccess);
        #elif defined(GPU_BACKEND_HIP)
            return (hipMemcpy(hostData, nativeBuffer, sizeInBytes, hipMemcpyDeviceToHost) == hipSuccess);
        #else
            return false;
        #endif
    }

    void GPUBuffer::release()
    {
        if (nativeBuffer)
        {
            #if defined(GPU_BACKEND_OPENCL)
                clReleaseMemObject(static_cast<cl_mem>(nativeBuffer));
            #elif defined(GPU_BACKEND_CUDA)
                cudaFree(nativeBuffer);
            #elif defined(GPU_BACKEND_HIP)
                hipFree(nativeBuffer);
            #endif

            nativeBuffer = nullptr;
            size = 0;
        }
    }

    //==============================================================================
    // GPUFFT implementation
    //==============================================================================
    GPUFFT::~GPUFFT()
    {
        release();
    }

    bool GPUFFT::createPlan(int fftSizeParam, int batchSizeParam)
    {
        fftSize = fftSizeParam;
        batchSize = batchSizeParam;

        #if defined(GPU_BACKEND_CUDA)
            cufftResult result = cufftPlan1d(reinterpret_cast<cufftHandle*>(&fftPlan),
                                            fftSize, CUFFT_R2C, batchSize);
            return (result == CUFFT_SUCCESS);
        #elif defined(GPU_BACKEND_HIP)
            rocfft_status status = rocfft_plan_create(reinterpret_cast<rocfft_plan*>(&fftPlan),
                                                     rocfft_placement_notinplace,
                                                     rocfft_transform_type_real_forward,
                                                     rocfft_precision_single,
                                                     1, // 1D FFT
                                                     reinterpret_cast<size_t*>(&fftSize),
                                                     batchSize,
                                                     nullptr);
            return (status == rocfft_status_success);
        #elif defined(GPU_BACKEND_OPENCL)
            // OpenCL FFT requires clFFT library which may not be available
            // For AMD GPUs, use HIP/ROCm backend instead (better performance)
            // For now, signal that FFT should fall back to CPU
            g_lastError = "OpenCL FFT not implemented - use HIP backend for AMD GPUs or CPU fallback";
            juce::Logger::writeToLog("GPU Backend: OpenCL FFT not available, using CPU fallback");
            return false;  // Will trigger CPU fallback in calling code
        #else
            return false;
        #endif
    }

    bool GPUFFT::executeForward(GPUBuffer& input, GPUBuffer& output)
    {
        #if defined(GPU_BACKEND_CUDA)
            cufftResult result = cufftExecR2C(reinterpret_cast<cufftHandle>(fftPlan),
                                             static_cast<cufftReal*>(input.getNativeHandle()),
                                             static_cast<cufftComplex*>(output.getNativeHandle()));
            return (result == CUFFT_SUCCESS);
        #elif defined(GPU_BACKEND_HIP)
            // Execute rocFFT forward transform
            rocfft_execution_info execInfo;
            rocfft_status status = rocfft_execution_info_create(&execInfo);
            if (status != rocfft_status_success)
                return false;

            void* inputBuffer[] = {input.getNativeHandle()};
            void* outputBuffer[] = {output.getNativeHandle()};

            status = rocfft_execute(reinterpret_cast<rocfft_plan>(fftPlan),
                                   inputBuffer,
                                   outputBuffer,
                                   execInfo);

            rocfft_execution_info_destroy(execInfo);
            return (status == rocfft_status_success);
        #else
            return false;
        #endif
    }

    bool GPUFFT::executeInverse(GPUBuffer& input, GPUBuffer& output)
    {
        #if defined(GPU_BACKEND_CUDA)
            cufftResult result = cufftExecC2R(reinterpret_cast<cufftHandle>(fftPlan),
                                             static_cast<cufftComplex*>(input.getNativeHandle()),
                                             static_cast<cufftReal*>(output.getNativeHandle()));
            return (result == CUFFT_SUCCESS);
        #elif defined(GPU_BACKEND_HIP)
            // Execute rocFFT inverse transform
            rocfft_execution_info execInfo;
            rocfft_status status = rocfft_execution_info_create(&execInfo);
            if (status != rocfft_status_success)
                return false;

            void* inputBuffer[] = {input.getNativeHandle()};
            void* outputBuffer[] = {output.getNativeHandle()};

            status = rocfft_execute(reinterpret_cast<rocfft_plan>(fftPlan),
                                   inputBuffer,
                                   outputBuffer,
                                   execInfo);

            rocfft_execution_info_destroy(execInfo);
            return (status == rocfft_status_success);
        #else
            return false;
        #endif
    }

    void GPUFFT::release()
    {
        if (fftPlan)
        {
            #if defined(GPU_BACKEND_CUDA)
                cufftDestroy(reinterpret_cast<cufftHandle>(fftPlan));
            #elif defined(GPU_BACKEND_HIP)
                rocfft_plan_destroy(reinterpret_cast<rocfft_plan>(fftPlan));
            #endif

            fftPlan = nullptr;
        }
    }

    //==============================================================================
    // GPUKernel implementation
    //==============================================================================
    GPUKernel::~GPUKernel()
    {
        release();
    }

    bool GPUKernel::loadFromSource(const std::string& kernelSource, const std::string& kernelName)
    {
        #if defined(GPU_BACKEND_OPENCL)
            const char* source = kernelSource.c_str();
            size_t sourceSize = kernelSource.length();

            cl_int err;
            cl_program program = clCreateProgramWithSource(g_clContext, 1, &source, &sourceSize, &err);
            if (err != CL_SUCCESS)
            {
                g_lastError = "Failed to create OpenCL program";
                return false;
            }

            // Compile program
            err = clBuildProgram(program, 1, &g_clDevice, "-cl-fast-relaxed-math", nullptr, nullptr);
            if (err != CL_SUCCESS)
            {
                // Get build log
                char buildLog[4096];
                clGetProgramBuildInfo(program, g_clDevice, CL_PROGRAM_BUILD_LOG,
                                     sizeof(buildLog), buildLog, nullptr);
                g_lastError = "OpenCL kernel compilation failed:\n" + std::string(buildLog);
                clReleaseProgram(program);
                return false;
            }

            // Create kernel
            cl_kernel kernel = clCreateKernel(program, kernelName.c_str(), &err);
            if (err != CL_SUCCESS)
            {
                g_lastError = "Failed to create OpenCL kernel: " + kernelName;
                clReleaseProgram(program);
                return false;
            }

            nativeProgram = program;
            nativeKernel = kernel;
            return true;

        #elif defined(GPU_BACKEND_HIP)
            // HIP: Compile kernel at runtime
            hipModule_t module;
            hipError_t err = hipModuleLoadData(&module, kernelSource.c_str());
            if (err != hipSuccess)
            {
                g_lastError = "Failed to load HIP module";
                return false;
            }

            hipFunction_t function;
            err = hipModuleGetFunction(&function, module, kernelName.c_str());
            if (err != hipSuccess)
            {
                g_lastError = "Failed to get HIP kernel function: " + kernelName;
                hipModuleUnload(module);
                return false;
            }

            nativeProgram = module;
            nativeKernel = function;
            return true;

        #elif defined(GPU_BACKEND_CUDA)
            // CUDA: Load PTX/cubin module
            CUmodule module;
            CUresult err = cuModuleLoadData(&module, kernelSource.c_str());
            if (err != CUDA_SUCCESS)
            {
                g_lastError = "Failed to load CUDA module";
                return false;
            }

            CUfunction function;
            err = cuModuleGetFunction(&function, module, kernelName.c_str());
            if (err != CUDA_SUCCESS)
            {
                g_lastError = "Failed to get CUDA kernel function: " + kernelName;
                cuModuleUnload(module);
                return false;
            }

            nativeProgram = module;
            nativeKernel = function;
            return true;

        #else
            g_lastError = "No GPU backend available for kernel compilation";
            return false;
        #endif
    }

    bool GPUKernel::setArgument(int index, GPUBuffer& buffer)
    {
        #if defined(GPU_BACKEND_OPENCL)
            cl_mem bufferHandle = static_cast<cl_mem>(buffer.getNativeHandle());
            cl_int err = clSetKernelArg(static_cast<cl_kernel>(nativeKernel), index,
                                       sizeof(cl_mem), &bufferHandle);
            return (err == CL_SUCCESS);

        #elif defined(GPU_BACKEND_HIP)
            void* bufferPtr = buffer.getNativeHandle();
            hipError_t err = hipModuleLaunchKernel(
                static_cast<hipFunction_t>(nativeKernel),
                1, 1, 1,  // Will be set in execute()
                1, 1, 1,
                0, nullptr,
                &bufferPtr, nullptr);
            // Note: HIP uses different argument mechanism, this is simplified
            return (err == hipSuccess);

        #else
            return false;
        #endif
    }

    bool GPUKernel::setArgument(int index, float value)
    {
        #if defined(GPU_BACKEND_OPENCL)
            cl_int err = clSetKernelArg(static_cast<cl_kernel>(nativeKernel), index,
                                       sizeof(float), &value);
            return (err == CL_SUCCESS);
        #else
            return false;
        #endif
    }

    bool GPUKernel::setArgument(int index, int value)
    {
        #if defined(GPU_BACKEND_OPENCL)
            cl_int err = clSetKernelArg(static_cast<cl_kernel>(nativeKernel), index,
                                       sizeof(int), &value);
            return (err == CL_SUCCESS);
        #else
            return false;
        #endif
    }

    bool GPUKernel::execute(size_t globalWorkSize, size_t localWorkSize)
    {
        #if defined(GPU_BACKEND_OPENCL)
            size_t global = globalWorkSize;
            size_t local = localWorkSize;

            cl_int err = clEnqueueNDRangeKernel(g_clQueue, static_cast<cl_kernel>(nativeKernel),
                                               1, nullptr, &global, &local,
                                               0, nullptr, nullptr);
            return (err == CL_SUCCESS);

        #elif defined(GPU_BACKEND_HIP)
            // Launch HIP kernel
            int blockSize = static_cast<int>(localWorkSize);
            int gridSize = (static_cast<int>(globalWorkSize) + blockSize - 1) / blockSize;

            hipError_t err = hipModuleLaunchKernel(
                static_cast<hipFunction_t>(nativeKernel),
                gridSize, 1, 1,
                blockSize, 1, 1,
                0, nullptr,
                nullptr, nullptr);
            return (err == hipSuccess);

        #elif defined(GPU_BACKEND_CUDA)
            // Launch CUDA kernel
            int blockSize = static_cast<int>(localWorkSize);
            int gridSize = (static_cast<int>(globalWorkSize) + blockSize - 1) / blockSize;

            CUresult err = cuLaunchKernel(
                static_cast<CUfunction>(nativeKernel),
                gridSize, 1, 1,
                blockSize, 1, 1,
                0, nullptr,
                nullptr, nullptr);
            return (err == CUDA_SUCCESS);

        #else
            return false;
        #endif
    }

    void GPUKernel::release()
    {
        #if defined(GPU_BACKEND_OPENCL)
            if (nativeKernel)
                clReleaseKernel(static_cast<cl_kernel>(nativeKernel));
            if (nativeProgram)
                clReleaseProgram(static_cast<cl_program>(nativeProgram));

        #elif defined(GPU_BACKEND_HIP)
            if (nativeProgram)
                hipModuleUnload(static_cast<hipModule_t>(nativeProgram));

        #elif defined(GPU_BACKEND_CUDA)
            if (nativeProgram)
                cuModuleUnload(static_cast<CUmodule>(nativeProgram));
        #endif

        nativeKernel = nullptr;
        nativeProgram = nullptr;
    }

} // namespace GPUBackend
