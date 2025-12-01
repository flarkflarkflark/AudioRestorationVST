#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

/**
 * Batch Processor
 *
 * Processes multiple audio files with saved settings.
 * Standalone mode only.
 *
 * Features:
 * - File queue management
 * - Progress tracking
 * - Cancellation support
 * - Results logging
 */
class BatchProcessor
{
public:
    BatchProcessor() = default;

    struct Settings
    {
        bool clickRemoval = true;
        float clickSensitivity = 50.0f;
        bool noiseReduction = false;
        float noiseReductionDB = 12.0f;
        bool rumbleFilter = false;
        float rumbleFreq = 20.0f;
        bool normalize = false;
        float normalizeDB = -0.5f;
        bool detectTracks = false;
    };

    //==============================================================================
    void addFile (const juce::File& file)
    {
        fileQueue.push_back (file);
    }

    void clearQueue()
    {
        fileQueue.clear();
    }

    int getQueueSize() const
    {
        return static_cast<int> (fileQueue.size());
    }

    //==============================================================================
    void processQueue (const Settings& settings)
    {
        // TODO: Process all files in queue
        // Run on background thread
        // Update progress callback
        // Log results
    }

    void cancel()
    {
        shouldCancel = true;
    }

private:
    std::vector<juce::File> fileQueue;
    std::atomic<bool> shouldCancel {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatchProcessor)
};
