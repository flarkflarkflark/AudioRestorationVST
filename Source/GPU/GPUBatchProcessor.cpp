#include "GPUBatchProcessor.h"
#include <juce_core/juce_core.h>

GPUBatchProcessor::GPUBatchProcessor()
{
    gpuEnabled = initializeGPU();

    if (gpuEnabled)
    {
        juce::Logger::writeToLog("GPU Batch Processor: Initialized");
        gpuNoiseReduction = std::make_unique<GPUNoiseReduction>();
    }
}

GPUBatchProcessor::~GPUBatchProcessor()
{
    cancelProcessing();
    shutdownGPU();
}

void GPUBatchProcessor::addJob(const juce::File& inputFile, const juce::File& outputFile)
{
    FileJob job;
    job.inputFile = inputFile;
    job.outputFile = outputFile;
    jobs.push_back(job);
}

void GPUBatchProcessor::addJobs(const std::vector<std::pair<juce::File, juce::File>>& files)
{
    for (const auto& [input, output] : files)
    {
        addJob(input, output);
    }
}

void GPUBatchProcessor::clearJobs()
{
    if (!processing)
    {
        jobs.clear();
        currentJobIndex = 0;
    }
}

void GPUBatchProcessor::setSettings(const ProcessingSettings& newSettings)
{
    settings = newSettings;

    if (gpuEnabled && gpuNoiseReduction && !settings.noiseProfile.empty())
    {
        // Upload noise profile to GPU
        // TODO: Implement profile upload
    }
}

void GPUBatchProcessor::startProcessing()
{
    if (processing || jobs.empty())
        return;

    processing = true;
    cancelled = false;
    currentJobIndex = 0;

    juce::Logger::writeToLog("Batch Processing: Starting " + juce::String(jobs.size()) +
                            " jobs (" + (gpuEnabled ? "GPU" : "CPU") + ")");

    // Start worker thread
    class BatchWorkerThread : public juce::Thread
    {
    public:
        BatchWorkerThread(GPUBatchProcessor& p) : juce::Thread("GPUBatchProcessor"), processor(p) {}

        void run() override
        {
            processor.workerThreadFunction();
        }

        GPUBatchProcessor& processor;
    };

    workerThread = std::make_unique<BatchWorkerThread>(*this);
    workerThread->startThread();
}

void GPUBatchProcessor::cancelProcessing()
{
    if (processing)
    {
        cancelled = true;

        if (workerThread)
        {
            workerThread->stopThread(5000);
            workerThread.reset();
        }

        processing = false;
    }
}

float GPUBatchProcessor::getOverallProgress() const
{
    if (jobs.empty())
        return 0.0f;

    float completedJobs = static_cast<float>(currentJobIndex);
    float currentProgress = 0.0f;

    if (currentJobIndex < static_cast<int>(jobs.size()))
    {
        currentProgress = jobs[static_cast<size_t>(currentJobIndex)].progress;
    }

    return (completedJobs + currentProgress) / static_cast<float>(jobs.size());
}

std::string GPUBatchProcessor::getGPUInfo() const
{
    if (!gpuEnabled)
        return "CPU";

    auto deviceInfo = GPUBackend::getDeviceInfo();
    return deviceInfo.name + " (" + deviceInfo.backendName + ")";
}

//==============================================================================
void GPUBatchProcessor::processJobsGPU()
{
    // TODO: Implement parallel GPU processing
    // This would:
    // 1. Load multiple files into GPU memory
    // 2. Process them in parallel using GPU streams
    // 3. Save results

    // For now, process sequentially
    for (size_t i = 0; i < jobs.size() && !cancelled; ++i)
    {
        currentJobIndex = static_cast<int>(i);
        auto& job = jobs[i];

        job.success = processFileGPU(job);
        job.completed = true;
        job.progress = 1.0f;

        if (onJobComplete)
            onJobComplete(static_cast<int>(i), job.success, job.errorMessage);
    }
}

void GPUBatchProcessor::processJobCPU(FileJob& job)
{
    // CPU fallback processing
    job.errorMessage = "CPU processing not yet implemented";
    job.success = false;
}

bool GPUBatchProcessor::processFileGPU(FileJob& job)
{
    // Simplified file processing
    // TODO: Implement full GPU processing pipeline

    try
    {
        // Load file
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(job.inputFile));

        if (!reader)
        {
            job.errorMessage = "Failed to open input file";
            return false;
        }

        job.progress = 0.5f;

        // TODO: Process audio with GPU
        // - Noise reduction
        // - Click removal
        // - Filtering
        // - Normalization

        job.progress = 1.0f;
        return true;
    }
    catch (const std::exception& e)
    {
        job.errorMessage = e.what();
        return false;
    }
}

void GPUBatchProcessor::workerThreadFunction()
{
    if (gpuEnabled)
        processJobsGPU();
    else
    {
        for (auto& job : jobs)
        {
            if (cancelled)
                break;

            processJobCPU(job);
            job.completed = true;
        }
    }

    processing = false;

    if (onAllJobsComplete)
        onAllJobsComplete();
}

bool GPUBatchProcessor::initializeGPU()
{
    return GPUBackend::isAvailable();
}

void GPUBatchProcessor::shutdownGPU()
{
    if (gpuEnabled)
    {
        gpuNoiseReduction.reset();
        gpuFileBuffers.clear();
        gpuEnabled = false;
    }
}
