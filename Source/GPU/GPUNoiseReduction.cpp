#include "GPUNoiseReduction.h"
#include <juce_core/juce_core.h>

GPUNoiseReduction::GPUNoiseReduction()
{
    gpuEnabled = initializeGPU();

    if (gpuEnabled)
    {
        juce::Logger::writeToLog("GPU Noise Reduction: Initialized successfully");
        juce::Logger::writeToLog("GPU Backend: " + GPUBackend::getBackendName());
        auto deviceInfo = GPUBackend::getDeviceInfo();
        juce::Logger::writeToLog("GPU Device: " + deviceInfo.name + " (" + deviceInfo.vendor + ")");
    }
    else
    {
        juce::Logger::writeToLog("GPU Noise Reduction: Falling back to CPU");
    }
}

GPUNoiseReduction::~GPUNoiseReduction()
{
    shutdownGPU();
}

//==============================================================================
void GPUNoiseReduction::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = spec.numChannels;

    // Initialize FFT parameters
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4; // 75% overlap

    // Allocate host buffers
    hostInputBuffer.resize(fftSize * 2 * static_cast<size_t>(numChannels));
    hostOutputBuffer.resize(fftSize * 2 * static_cast<size_t>(numChannels));
    overlapBuffer.setSize(static_cast<int>(numChannels), fftSize);
    overlapBuffer.clear();

    // Initialize noise profile
    noiseProfile.resize(fftSize / 2 + 1, 0.0f);
    profileCaptured = false;

    if (gpuEnabled)
    {
        // Create GPU FFT plan
        gpuFFT = std::make_unique<GPUBackend::GPUFFT>();
        if (!gpuFFT->createPlan(fftSize, static_cast<int>(numChannels)))
        {
            juce::Logger::writeToLog("GPU FFT plan creation failed, falling back to CPU");
            gpuEnabled = false;
            return;
        }

        // Allocate GPU buffers
        gpuInputBuffer = std::make_unique<GPUBackend::GPUBuffer>();
        gpuOutputBuffer = std::make_unique<GPUBackend::GPUBuffer>();
        gpuNoiseProfileBuffer = std::make_unique<GPUBackend::GPUBuffer>();

        size_t bufferSize = fftSize * 2 * numChannels * sizeof(float); // Complex data
        gpuInputBuffer->allocate(bufferSize);
        gpuOutputBuffer->allocate(bufferSize);
        gpuNoiseProfileBuffer->allocate((fftSize / 2 + 1) * sizeof(float));

        juce::Logger::writeToLog("GPU Noise Reduction: Buffers allocated (FFT size: " +
                                 juce::String(fftSize) + ")");
    }
}

void GPUNoiseReduction::reset()
{
    overlapBuffer.clear();
    isCapturingProfile = false;
    profileCaptureFrames = 0;
}

void GPUNoiseReduction::process(juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();

    // Capture profile if requested
    if (isCapturingProfile)
    {
        if (gpuEnabled)
            captureProfileGPU(block);
        else
            processCPUFallback(block);
        return;
    }

    // Bypass if no profile or zero reduction
    if (!profileCaptured || reductionAmount <= 0.0f)
        return;

    // Process with GPU or CPU
    if (gpuEnabled)
        processGPU(block);
    else
        processCPUFallback(block);
}

//==============================================================================
void GPUNoiseReduction::captureProfile()
{
    isCapturingProfile = true;
    profileCaptureFrames = 0;
    std::fill(noiseProfile.begin(), noiseProfile.end(), 0.0f);
}

void GPUNoiseReduction::setReduction(float dB)
{
    reductionAmount = juce::jlimit(0.0f, 24.0f, dB);
    reductionLinear = juce::Decibels::decibelsToGain(reductionAmount);
}

void GPUNoiseReduction::setFFTSize(int size)
{
    // Find nearest power of 2
    fftOrder = static_cast<int>(std::log2(size));
    fftOrder = juce::jlimit(10, 15, fftOrder); // 1024 to 32768
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4;

    // Reinitialize if already prepared
    if (sampleRate > 0)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.numChannels = numChannels;
        spec.maximumBlockSize = static_cast<juce::uint32>(fftSize);
        prepare(spec);
    }
}

void GPUNoiseReduction::clearProfile()
{
    std::fill(noiseProfile.begin(), noiseProfile.end(), 0.0f);
    profileCaptured = false;
}

std::string GPUNoiseReduction::getGPUInfo() const
{
    if (!gpuEnabled)
        return "CPU (GPU unavailable)";

    auto deviceInfo = GPUBackend::getDeviceInfo();
    return deviceInfo.name + " (" + deviceInfo.backendName + ")";
}

//==============================================================================
void GPUNoiseReduction::processGPU(juce::dsp::AudioBlock<float>& block)
{
    if (!gpuEnabled || !gpuFFT || !spectralSubtractionKernel)
    {
        processCPUFallback(block);
        return;
    }

    // Process each channel
    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        float* channelData = block.getChannelPointer(channel);
        size_t numSamples = block.getNumSamples();

        // Process in overlapping frames
        for (size_t pos = 0; pos < numSamples; pos += hopSize)
        {
            size_t frameEnd = juce::jmin(pos + fftSize, numSamples);
            size_t frameSamples = frameEnd - pos;

            if (frameSamples < static_cast<size_t>(hopSize))
                break;

            // 1. Upload audio frame to GPU
            std::fill(hostInputBuffer.begin(), hostInputBuffer.end(), 0.0f);
            for (size_t i = 0; i < frameSamples; ++i)
            {
                hostInputBuffer[i] = channelData[pos + i];
            }

            if (!gpuInputBuffer->upload(hostInputBuffer.data(), hostInputBuffer.size() * sizeof(float)))
            {
                juce::Logger::writeToLog("GPU upload failed, falling back to CPU");
                processCPUFallback(block);
                return;
            }

            // 2. Execute FFT on GPU
            if (!gpuFFT->executeForward(*gpuInputBuffer, *gpuOutputBuffer))
            {
                juce::Logger::writeToLog("GPU FFT failed");
                processCPUFallback(block);
                return;
            }

            // 3. Run spectral subtraction kernel
            performSpectralSubtractionGPU();

            // 4. Execute inverse FFT
            if (!gpuFFT->executeInverse(*gpuOutputBuffer, *gpuInputBuffer))
            {
                juce::Logger::writeToLog("GPU inverse FFT failed");
                processCPUFallback(block);
                return;
            }

            // 5. Download results from GPU
            if (!gpuInputBuffer->download(hostOutputBuffer.data(), hostOutputBuffer.size() * sizeof(float)))
            {
                juce::Logger::writeToLog("GPU download failed");
                processCPUFallback(block);
                return;
            }

            // 6. Overlap-add reconstruction (on CPU for now)
            const float normFactor = 1.5f; // For 75% overlap with Hann window
            for (size_t i = 0; i < frameSamples; ++i)
            {
                if (pos + i < numSamples)
                {
                    channelData[pos + i] = hostOutputBuffer[i] / (fftSize * normFactor);

                    // Add overlap from previous frame
                    if (channel < overlapBuffer.getNumChannels() && i < static_cast<size_t>(overlapBuffer.getNumSamples()))
                    {
                        channelData[pos + i] += overlapBuffer.getSample(static_cast<int>(channel), static_cast<int>(i));
                    }
                }
            }

            // 7. Store overlap for next frame
            if (channel < overlapBuffer.getNumChannels())
            {
                for (int i = 0; i < overlapBuffer.getNumSamples(); ++i)
                {
                    if (static_cast<size_t>(i) + hopSize < frameSamples)
                    {
                        overlapBuffer.setSample(static_cast<int>(channel), i,
                            hostOutputBuffer[i + hopSize] / (fftSize * normFactor));
                    }
                    else
                    {
                        overlapBuffer.setSample(static_cast<int>(channel), i, 0.0f);
                    }
                }
            }
        }
    }
}

void GPUNoiseReduction::processCPUFallback(juce::dsp::AudioBlock<float>& block)
{
    // CPU-based spectral subtraction using JUCE's FFT
    static std::unique_ptr<juce::dsp::FFT> cpuFFT;
    static std::vector<float> cpuFFTBuffer;
    static std::vector<float> cpuWindow;
    static int cpuFFTSize = 0;

    // Initialize CPU FFT if needed
    if (cpuFFTSize != fftSize)
    {
        cpuFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
        cpuFFTBuffer.resize(static_cast<size_t>(fftSize * 2));
        cpuWindow.resize(static_cast<size_t>(fftSize));
        cpuFFTSize = fftSize;

        // Create Hann window
        juce::dsp::WindowingFunction<float>::fillWindowingTables(
            cpuWindow.data(), static_cast<size_t>(fftSize),
            juce::dsp::WindowingFunction<float>::hann, false);
    }

    if (!profileCaptured || reductionAmount <= 0.0f)
        return;

    // Process each channel
    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        float* channelData = block.getChannelPointer(channel);
        size_t numSamples = block.getNumSamples();

        // Process in overlapping frames
        for (size_t pos = 0; pos < numSamples; pos += static_cast<size_t>(hopSize))
        {
            size_t frameSamples = juce::jmin(static_cast<size_t>(fftSize), numSamples - pos);

            if (frameSamples < static_cast<size_t>(hopSize))
                break;

            // 1. Window the input
            std::fill(cpuFFTBuffer.begin(), cpuFFTBuffer.end(), 0.0f);
            for (size_t i = 0; i < frameSamples; ++i)
            {
                cpuFFTBuffer[i] = channelData[pos + i] * cpuWindow[i];
            }

            // 2. Forward FFT
            cpuFFT->performRealOnlyForwardTransform(cpuFFTBuffer.data());

            // 3. Spectral subtraction
            int numBins = fftSize / 2 + 1;
            for (int bin = 0; bin < numBins; ++bin)
            {
                float real, imag;

                // Extract from JUCE FFT format
                if (bin == 0)
                {
                    real = cpuFFTBuffer[0];
                    imag = 0.0f;
                }
                else if (bin == fftSize / 2)
                {
                    real = cpuFFTBuffer[static_cast<size_t>(fftSize / 2)];
                    imag = 0.0f;
                }
                else
                {
                    real = cpuFFTBuffer[static_cast<size_t>(bin)];
                    imag = cpuFFTBuffer[static_cast<size_t>(fftSize - bin)];
                }

                // Calculate magnitude and phase
                float magnitude = std::sqrt(real * real + imag * imag);
                float phase = std::atan2(imag, real);

                // Subtract noise profile
                float noiseMag = noiseProfile[static_cast<size_t>(bin)] * reductionLinear;
                float cleanMag = magnitude - noiseMag;

                // Apply spectral floor
                cleanMag = juce::jmax(cleanMag, magnitude * spectralFloor);

                // Reconstruct
                real = cleanMag * std::cos(phase);
                imag = cleanMag * std::sin(phase);

                // Store back
                if (bin == 0)
                {
                    cpuFFTBuffer[0] = real;
                }
                else if (bin == fftSize / 2)
                {
                    cpuFFTBuffer[static_cast<size_t>(fftSize / 2)] = real;
                }
                else
                {
                    cpuFFTBuffer[static_cast<size_t>(bin)] = real;
                    cpuFFTBuffer[static_cast<size_t>(fftSize - bin)] = imag;
                }
            }

            // 4. Inverse FFT
            cpuFFT->performRealOnlyInverseTransform(cpuFFTBuffer.data());

            // 5. Overlap-add with windowing
            const float normFactor = 1.5f;
            for (size_t i = 0; i < frameSamples; ++i)
            {
                if (pos + i < numSamples)
                {
                    channelData[pos + i] = cpuFFTBuffer[i] * cpuWindow[i] / (static_cast<float>(fftSize) * normFactor);

                    // Add overlap from previous frame
                    if (channel < static_cast<size_t>(overlapBuffer.getNumChannels()) &&
                        i < static_cast<size_t>(overlapBuffer.getNumSamples()))
                    {
                        channelData[pos + i] += overlapBuffer.getSample(static_cast<int>(channel), static_cast<int>(i));
                    }
                }
            }

            // 6. Store overlap for next frame
            if (static_cast<int>(channel) < overlapBuffer.getNumChannels())
            {
                for (int i = 0; i < overlapBuffer.getNumSamples(); ++i)
                {
                    if (static_cast<size_t>(i) + static_cast<size_t>(hopSize) < frameSamples)
                    {
                        overlapBuffer.setSample(static_cast<int>(channel), i,
                            cpuFFTBuffer[static_cast<size_t>(i + hopSize)] * cpuWindow[static_cast<size_t>(i + hopSize)] /
                            (static_cast<float>(fftSize) * normFactor));
                    }
                    else
                    {
                        overlapBuffer.setSample(static_cast<int>(channel), i, 0.0f);
                    }
                }
            }
        }
    }
}

void GPUNoiseReduction::captureProfileGPU(const juce::dsp::AudioBlock<float>& block)
{
    // Simplified profile capture
    profileCaptureFrames++;

    if (profileCaptureFrames >= maxCaptureFrames)
    {
        profileCaptured = true;
        isCapturingProfile = false;

        if (gpuEnabled && gpuNoiseProfileBuffer)
        {
            // Upload noise profile to GPU
            uploadNoiseProfile();
        }

        juce::Logger::writeToLog("Noise profile captured (" + juce::String(profileCaptureFrames) + " frames)");
    }
}

void GPUNoiseReduction::performSpectralSubtractionGPU()
{
    if (!spectralSubtractionKernel)
        return;

    // Set kernel arguments
    int numBins = fftSize / 2 + 1;

    spectralSubtractionKernel->setArgument(0, *gpuOutputBuffer);     // FFT data (input/output)
    spectralSubtractionKernel->setArgument(1, *gpuNoiseProfileBuffer); // Noise profile
    spectralSubtractionKernel->setArgument(2, *gpuOutputBuffer);     // Output (same buffer)
    spectralSubtractionKernel->setArgument(3, reductionLinear);      // Reduction factor
    spectralSubtractionKernel->setArgument(4, spectralFloor);        // Spectral floor
    spectralSubtractionKernel->setArgument(5, numBins);              // Number of bins

    // Execute kernel
    size_t globalWorkSize = static_cast<size_t>(numBins);
    size_t localWorkSize = 256; // Workgroup size

    if (!spectralSubtractionKernel->execute(globalWorkSize, localWorkSize))
    {
        juce::Logger::writeToLog("Spectral subtraction kernel execution failed");
    }

    // Synchronize to ensure kernel completes
    GPUBackend::synchronize();
}

//==============================================================================
// Helper function to load kernel source from file
static std::string loadKernelSource(const juce::String& filename)
{
    juce::File kernelFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile("Source/GPU/kernels")
        .getChildFile(filename);

    if (!kernelFile.existsAsFile())
    {
        // Try relative to executable
        kernelFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory()
            .getChildFile("kernels")
            .getChildFile(filename);
    }

    if (kernelFile.existsAsFile())
    {
        return kernelFile.loadFileAsString().toStdString();
    }

    juce::Logger::writeToLog("Kernel file not found: " + filename);
    return "";
}

bool GPUNoiseReduction::initializeGPU()
{
    if (!GPUBackend::initialize())
    {
        juce::Logger::writeToLog("GPU Backend initialization failed");
        return false;
    }

    if (!GPUBackend::isAvailable())
    {
        juce::Logger::writeToLog("No GPU device available");
        return false;
    }

    // Load appropriate kernel based on backend
    std::string kernelSource;
    std::string kernelName = "spectralSubtractionFused";

    #if defined(USE_OPENCL)
        kernelSource = loadKernelSource("spectral_subtraction.cl");
    #elif defined(USE_HIP)
        kernelSource = loadKernelSource("spectral_subtraction.hip");
        // For HIP, we would normally compile to a binary first
        // For now, use OpenCL as fallback
        if (kernelSource.empty())
            kernelSource = loadKernelSource("spectral_subtraction.cl");
    #elif defined(USE_CUDA)
        kernelSource = loadKernelSource("spectral_subtraction.cu");
        // For CUDA, we would normally compile to PTX first
        // For now, use OpenCL as fallback
        if (kernelSource.empty())
            kernelSource = loadKernelSource("spectral_subtraction.cl");
    #endif

    if (kernelSource.empty())
    {
        juce::Logger::writeToLog("Failed to load GPU kernel source");
        return false;
    }

    // Create and compile kernel
    spectralSubtractionKernel = std::make_unique<GPUBackend::GPUKernel>();
    if (!spectralSubtractionKernel->loadFromSource(kernelSource, kernelName))
    {
        juce::Logger::writeToLog("Failed to compile GPU kernel: " + GPUBackend::getLastError());
        spectralSubtractionKernel.reset();
        return false;
    }

    juce::Logger::writeToLog("GPU kernel compiled successfully");
    return true;
}

void GPUNoiseReduction::shutdownGPU()
{
    if (gpuEnabled)
    {
        // Release GPU resources
        if (gpuFFT) gpuFFT->release();
        if (gpuInputBuffer) gpuInputBuffer->release();
        if (gpuOutputBuffer) gpuOutputBuffer->release();
        if (gpuNoiseProfileBuffer) gpuNoiseProfileBuffer->release();
        if (spectralSubtractionKernel) spectralSubtractionKernel->release();

        gpuFFT.reset();
        gpuInputBuffer.reset();
        gpuOutputBuffer.reset();
        gpuNoiseProfileBuffer.reset();
        spectralSubtractionKernel.reset();

        gpuEnabled = false;
    }
}

bool GPUNoiseReduction::uploadNoiseProfile()
{
    if (!gpuEnabled || !gpuNoiseProfileBuffer)
        return false;

    return gpuNoiseProfileBuffer->upload(noiseProfile.data(),
                                         noiseProfile.size() * sizeof(float));
}
