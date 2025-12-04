#include "BatchProcessor.h"
#include "../Utils/AudioFileManager.h"
#include "../DSP/ClickRemoval.h"
#include "../DSP/NoiseReduction.h"
#include "../DSP/FilterBank.h"
#include <juce_events/juce_events.h>

BatchProcessor::BatchProcessor()
    : juce::Thread ("BatchProcessorThread")
{
}

BatchProcessor::~BatchProcessor()
{
    stopThread (5000); // Wait up to 5 seconds for thread to finish
}

void BatchProcessor::addFile (const juce::File& file)
{
    if (file.existsAsFile())
    {
        fileQueue.push_back (file);
        DBG ("Added file to batch queue: " + file.getFullPathName());
    }
}

void BatchProcessor::clearQueue()
{
    fileQueue.clear();
    DBG ("Batch queue cleared");
}

int BatchProcessor::getQueueSize() const
{
    return static_cast<int> (fileQueue.size());
}

void BatchProcessor::startProcessing (const Settings& settings)
{
    if (isThreadRunning())
    {
        DBG ("Batch processing already in progress");
        return;
    }

    currentSettings = settings;
    shouldCancel = false;
    startThread();
    DBG ("Batch processing started with " + juce::String (fileQueue.size()) + " files");
}

void BatchProcessor::cancelProcessing()
{
    DBG ("Cancelling batch processing...");
    shouldCancel = true;
    signalThreadShouldExit();
}

void BatchProcessor::run()
{
    int totalFiles = static_cast<int> (fileQueue.size());
    int successCount = 0;
    int failureCount = 0;

    for (int i = 0; i < totalFiles; ++i)
    {
        if (threadShouldExit() || shouldCancel)
        {
            notifyCompletion (false, "Batch processing cancelled");
            return;
        }

        const juce::File& inputFile = fileQueue[i];

        ProgressInfo info;
        info.currentFileIndex = i + 1;
        info.totalFiles = totalFiles;
        info.currentFileName = inputFile.getFileName();
        info.progress = static_cast<float> (i) / static_cast<float> (totalFiles);
        info.status = "Processing: " + inputFile.getFileName();
        notifyProgress (info);

        if (processFile (inputFile, currentSettings, i))
            successCount++;
        else
            failureCount++;
    }

    juce::String message = "Batch processing complete: " +
                           juce::String (successCount) + " succeeded, " +
                           juce::String (failureCount) + " failed";

    notifyCompletion (failureCount == 0, message);
}

bool BatchProcessor::processFile (const juce::File& inputFile, const Settings& settings, int /*fileIndex*/)
{
    DBG ("Processing file " + juce::String (fileIndex + 1) + ": " + inputFile.getFullPathName());

    // Load audio file
    AudioFileManager fileManager;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;

    if (!fileManager.loadAudioFile (inputFile, buffer, sampleRate))
    {
        DBG ("Failed to load file: " + inputFile.getFullPathName());
        return false;
    }

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Configure DSP processing spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = static_cast<juce::uint32> (numChannels);
    spec.maximumBlockSize = 2048;

    const int blockSize = 2048;

    // Apply Click & Pop Removal
    if (settings.clickRemoval && !shouldCancel)
    {
        DBG ("Applying click removal...");

        ClickRemoval clickProcessor;
        clickProcessor.prepare (spec);
        clickProcessor.setSensitivity (settings.clickSensitivity);
        clickProcessor.setRemovalMethod (ClickRemoval::Automatic);

        for (int startSample = 0; startSample < numSamples && !shouldCancel; startSample += blockSize)
        {
            int samplesThisBlock = juce::jmin (blockSize, numSamples - startSample);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                static_cast<size_t> (numChannels),
                                                static_cast<size_t> (startSample),
                                                static_cast<size_t> (samplesThisBlock));

            juce::dsp::ProcessContextReplacing<float> context (block);
            clickProcessor.process (context);
        }
    }

    // Apply Noise Reduction
    if (settings.noiseReduction && !shouldCancel)
    {
        DBG ("Applying noise reduction...");

        NoiseReduction noiseProcessor;
        noiseProcessor.prepare (spec);
        noiseProcessor.setReduction (settings.noiseReductionDB);

        // Capture noise profile from first second of audio (assuming it contains noise)
        noiseProcessor.captureProfile();

        int profileSamples = juce::jmin (static_cast<int> (sampleRate), numSamples);
        for (int startSample = 0; startSample < profileSamples && !shouldCancel; startSample += blockSize)
        {
            int samplesThisBlock = juce::jmin (blockSize, profileSamples - startSample);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                static_cast<size_t> (numChannels),
                                                static_cast<size_t> (startSample),
                                                static_cast<size_t> (samplesThisBlock));

            juce::dsp::ProcessContextReplacing<float> context (block);
            noiseProcessor.process (context);

            if (noiseProcessor.hasProfile())
                break;
        }

        // Process entire file with noise reduction
        if (noiseProcessor.hasProfile())
        {
            for (int startSample = 0; startSample < numSamples && !shouldCancel; startSample += blockSize)
            {
                int samplesThisBlock = juce::jmin (blockSize, numSamples - startSample);

                juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                    static_cast<size_t> (numChannels),
                                                    static_cast<size_t> (startSample),
                                                    static_cast<size_t> (samplesThisBlock));

                juce::dsp::ProcessContextReplacing<float> context (block);
                noiseProcessor.process (context);
            }
        }
    }

    // Apply Filters (Rumble and Hum)
    if ((settings.rumbleFilter || settings.humFilter) && !shouldCancel)
    {
        DBG ("Applying filters...");

        FilterBank filterProcessor;
        filterProcessor.prepare (spec);

        if (settings.rumbleFilter)
            filterProcessor.setRumbleFilter (settings.rumbleFreq, false);
        else
            filterProcessor.setRumbleFilter (20.0f, true);

        if (settings.humFilter)
            filterProcessor.setHumFilter (settings.humFreq, false);
        else
            filterProcessor.setHumFilter (60.0f, true);

        for (int startSample = 0; startSample < numSamples && !shouldCancel; startSample += blockSize)
        {
            int samplesThisBlock = juce::jmin (blockSize, numSamples - startSample);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                                static_cast<size_t> (numChannels),
                                                static_cast<size_t> (startSample),
                                                static_cast<size_t> (samplesThisBlock));

            juce::dsp::ProcessContextReplacing<float> context (block);
            filterProcessor.process (context);
        }
    }

    // Apply Normalization
    if (settings.normalize && !shouldCancel)
    {
        DBG ("Applying normalization...");
        float maxLevel = 0.0f;

        // Find peak level
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                maxLevel = juce::jmax (maxLevel, std::abs (channelData[i]));
            }
        }

        if (maxLevel > 0.0f)
        {
            float targetLevel = juce::Decibels::decibelsToGain (settings.normalizeDB);
            float gain = targetLevel / maxLevel;

            // Apply gain
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.applyGain (ch, 0, numSamples, gain);
            }

            DBG ("Normalized with gain: " + juce::String (juce::Decibels::gainToDecibels (gain), 2) + " dB");
        }
    }

    if (shouldCancel)
    {
        DBG ("Processing cancelled");
        return false;
    }

    // Determine output file
    juce::File outputFile;
    if (settings.outputDirectory.isDirectory())
    {
        juce::String outputName = inputFile.getFileNameWithoutExtension() + "_processed.wav";
        outputFile = settings.outputDirectory.getChildFile (outputName);
    }
    else
    {
        // Save next to original file
        juce::String outputName = inputFile.getFileNameWithoutExtension() + "_processed.wav";
        outputFile = inputFile.getSiblingFile (outputName);
    }

    // Save processed audio
    if (!fileManager.saveAudioFile (outputFile, buffer, sampleRate, settings.outputBitDepth))
    {
        DBG ("Failed to save file: " + outputFile.getFullPathName());
        return false;
    }

    DBG ("Successfully processed and saved: " + outputFile.getFullPathName());
    return true;
}

void BatchProcessor::notifyProgress (const ProgressInfo& info)
{
    if (progressCallback)
    {
        juce::MessageManager::callAsync ([this, info]() {
            if (progressCallback)
                progressCallback (info);
        });
    }
}

void BatchProcessor::notifyCompletion (bool success, const juce::String& message)
{
    DBG (message);

    if (completionCallback)
    {
        juce::MessageManager::callAsync ([this, success, message]() {
            if (completionCallback)
                completionCallback (success, message);
        });
    }
}
