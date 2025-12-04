#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <atomic>

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
class BatchProcessor : public juce::Thread
{
public:
    BatchProcessor();
    ~BatchProcessor() override;

    struct Settings
    {
        bool clickRemoval = true;
        float clickSensitivity = 50.0f;
        bool noiseReduction = false;
        float noiseReductionDB = 12.0f;
        bool rumbleFilter = false;
        float rumbleFreq = 20.0f;
        bool humFilter = false;
        float humFreq = 50.0f;
        bool normalize = false;
        float normalizeDB = -0.5f;
        bool detectTracks = false;
        int outputBitDepth = 16;
        juce::File outputDirectory;
    };

    struct ProgressInfo
    {
        int currentFileIndex = 0;
        int totalFiles = 0;
        juce::String currentFileName;
        float progress = 0.0f; // 0.0 to 1.0
        juce::String status;
    };

    //==============================================================================
    /** Callback function types */
    using ProgressCallback = std::function<void(const ProgressInfo&)>;
    using CompletionCallback = std::function<void(bool success, const juce::String& message)>;

    void setProgressCallback (ProgressCallback callback) { progressCallback = callback; }
    void setCompletionCallback (CompletionCallback callback) { completionCallback = callback; }

    //==============================================================================
    void addFile (const juce::File& file);
    void clearQueue();
    int getQueueSize() const;

    //==============================================================================
    /** Start batch processing (runs on background thread) */
    void startProcessing (const Settings& settings);

    /** Cancel current batch operation */
    void cancelProcessing();

    /** Check if batch is currently processing */
    bool isProcessing() const { return isThreadRunning(); }

private:
    void run() override;
    bool processFile (const juce::File& inputFile, const Settings& settings, int fileIndex);
    void notifyProgress (const ProgressInfo& info);
    void notifyCompletion (bool success, const juce::String& message);

    std::vector<juce::File> fileQueue;
    Settings currentSettings;
    std::atomic<bool> shouldCancel {false};

    ProgressCallback progressCallback;
    CompletionCallback completionCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatchProcessor)
};
