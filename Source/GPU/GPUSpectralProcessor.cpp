#include "GPUSpectralProcessor.h"
#include <juce_core/juce_core.h>

GPUSpectralProcessor::GPUSpectralProcessor()
{
    gpuEnabled = initializeGPU();

    if (gpuEnabled)
    {
        juce::Logger::writeToLog("GPU Spectral Processor: Initialized");
    }
}

GPUSpectralProcessor::~GPUSpectralProcessor()
{
    shutdownGPU();
}

void GPUSpectralProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);

    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4;

    // Initialize spectrum buffers
    spectrumData.resize(static_cast<size_t>(numChannels));
    for (auto& spectrum : spectrumData)
    {
        spectrum.resize(fftSize / 2 + 1, 0.0f);
    }

    // Initialize spectrogram history
    spectrogramHistory.resize(static_cast<size_t>(spectrogramMaxFrames));
    for (auto& frame : spectrogramHistory)
    {
        frame.resize(fftSize / 2 + 1, 0.0f);
    }

    // Window buffer
    windowBuffer.resize(static_cast<size_t>(fftSize));
    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        windowBuffer.data(), static_cast<size_t>(fftSize),
        juce::dsp::WindowingFunction<float>::hann, false);

    if (gpuEnabled)
    {
        // Create GPU FFT plan
        gpuFFT = std::make_unique<GPUBackend::GPUFFT>();
        gpuFFT->createPlan(fftSize, numChannels);

        // Allocate GPU buffers
        gpuAudioBuffer = std::make_unique<GPUBackend::GPUBuffer>();
        gpuSpectrumBuffer = std::make_unique<GPUBackend::GPUBuffer>();

        size_t audioBufferSize = fftSize * 2 * numChannels * sizeof(float);
        size_t spectrumBufferSize = (fftSize / 2 + 1) * numChannels * sizeof(float);

        gpuAudioBuffer->allocate(audioBufferSize);
        gpuSpectrumBuffer->allocate(spectrumBufferSize);
    }
}

void GPUSpectralProcessor::reset()
{
    spectrogramWriteIndex = 0;
}

void GPUSpectralProcessor::processBlock(const juce::AudioBuffer<float>& buffer)
{
    if (gpuEnabled)
        processGPU(buffer);
    else
        processCPU(buffer);
}

const std::vector<float>& GPUSpectralProcessor::getSpectrum(int channel) const
{
    channel = juce::jlimit(0, numChannels - 1, channel);
    return spectrumData[static_cast<size_t>(channel)];
}

juce::Image GPUSpectralProcessor::getSpectrogramImage(int width, int height)
{
    juce::Image image(juce::Image::RGB, width, height, true);

    // Simple CPU-based rendering for now
    // TODO: Implement GPU-accelerated rendering

    return image;
}

void GPUSpectralProcessor::setFFTSize(int size)
{
    fftOrder = static_cast<int>(std::log2(size));
    fftOrder = juce::jlimit(10, 15, fftOrder);
}

void GPUSpectralProcessor::setSpectrogramHistorySize(int frames)
{
    spectrogramMaxFrames = juce::jlimit(64, 1024, frames);
}

void GPUSpectralProcessor::setFrequencyRange(float minFreq, float maxFreq)
{
    minFrequency = minFreq;
    maxFrequency = maxFreq;
}

void GPUSpectralProcessor::setGPUEnabled(bool enabled)
{
    if (enabled && !gpuEnabled)
    {
        gpuEnabled = initializeGPU();
    }
    else if (!enabled && gpuEnabled)
    {
        shutdownGPU();
    }
}

std::string GPUSpectralProcessor::getGPUInfo() const
{
    if (!gpuEnabled)
        return "CPU";

    auto deviceInfo = GPUBackend::getDeviceInfo();
    return deviceInfo.name + " (" + deviceInfo.backendName + ")";
}

float GPUSpectralProcessor::getPeakFrequency(int channel) const
{
    if (channel < 0 || channel >= numChannels)
        return 0.0f;

    // Find peak in spectrum
    const auto& spectrum = spectrumData[static_cast<size_t>(channel)];
    size_t peakIndex = 0;
    float peakValue = 0.0f;

    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        if (spectrum[i] > peakValue)
        {
            peakValue = spectrum[i];
            peakIndex = i;
        }
    }

    return (static_cast<float>(peakIndex) * static_cast<float>(sampleRate)) / static_cast<float>(fftSize);
}

float GPUSpectralProcessor::getRMSLevel(int channel) const
{
    // Simple RMS calculation
    return 0.0f;
}

//==============================================================================
void GPUSpectralProcessor::processGPU(const juce::AudioBuffer<float>& buffer)
{
    // TODO: Implement GPU processing
    processCPU(buffer);
}

void GPUSpectralProcessor::processCPU(const juce::AudioBuffer<float>& buffer)
{
    // Simplified CPU spectrum analysis
    // Full implementation would do proper FFT analysis
}

void GPUSpectralProcessor::generateSpectrogramGPU()
{
    // TODO: Generate spectrogram on GPU
}

void GPUSpectralProcessor::applyColorMapGPU(juce::Image& image)
{
    // TODO: Apply color mapping on GPU for visualization
}

bool GPUSpectralProcessor::initializeGPU()
{
    return GPUBackend::isAvailable();
}

void GPUSpectralProcessor::shutdownGPU()
{
    if (gpuEnabled)
    {
        if (gpuFFT) gpuFFT->release();
        if (gpuAudioBuffer) gpuAudioBuffer->release();
        if (gpuSpectrumBuffer) gpuSpectrumBuffer->release();

        gpuFFT.reset();
        gpuAudioBuffer.reset();
        gpuSpectrumBuffer.reset();

        gpuEnabled = false;
    }
}
