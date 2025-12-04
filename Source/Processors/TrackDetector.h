#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/**
 * Track Detector
 *
 * Detects track boundaries in audio files based on silence detection.
 * Used in standalone mode for automatically splitting vinyl rips into individual tracks.
 *
 * Algorithm:
 * 1. Analyze waveform for silence regions (RMS below threshold)
 * 2. Identify silence >= minimum duration
 * 3. Mark track boundaries at silence midpoints
 * 4. Support manual boundary adjustment
 *
 * Standalone mode only.
 */
class TrackDetector
{
public:
    struct TrackBoundary
    {
        int64_t position;      // Sample position
        bool isManual;         // User-defined vs auto-detected
        juce::String name;     // Optional track name

        TrackBoundary (int64_t pos, bool manual = false, const juce::String& trackName = juce::String())
            : position (pos), isManual (manual), name (trackName) {}
    };

    struct DetectionSettings
    {
        float silenceThresholdDb = -40.0f;  // RMS threshold in dB
        double minSilenceDurationSeconds = 2.0;  // Minimum silence duration to consider
        double minTrackDurationSeconds = 10.0;   // Minimum track length
        bool useRMSDetection = true;         // Use RMS vs peak detection
        int rmsWindowSamples = 1024;         // Window size for RMS calculation
    };

    //==============================================================================
    TrackDetector();

    /** Detect track boundaries in audio buffer */
    std::vector<TrackBoundary> detectTracks (const juce::AudioBuffer<float>& buffer,
                                             double sampleRate,
                                             const DetectionSettings& settings);

    /** Add manual track boundary */
    void addManualBoundary (int64_t position, const juce::String& name = juce::String());

    /** Remove boundary at index */
    void removeBoundary (int index);

    /** Get all boundaries (sorted by position) */
    const std::vector<TrackBoundary>& getBoundaries() const { return boundaries; }

    /** Clear all boundaries */
    void clearBoundaries();

    /** Split audio buffer into separate track buffers based on boundaries */
    std::vector<juce::AudioBuffer<float>> splitIntoTracks (const juce::AudioBuffer<float>& buffer,
                                                           double sampleRate,
                                                           bool applyFades = true);

    /** Export individual tracks to files */
    bool exportTracks (const juce::AudioBuffer<float>& buffer,
                      double sampleRate,
                      const juce::File& outputDirectory,
                      const juce::String& baseName,
                      const juce::String& extension = "wav");

private:
    float calculateRMS (const juce::AudioBuffer<float>& buffer, int channel,
                       int startSample, int numSamples);

    float calculatePeak (const juce::AudioBuffer<float>& buffer, int channel,
                        int startSample, int numSamples);

    void applyFadeInOut (juce::AudioBuffer<float>& buffer, int fadeSamples);

    void sortBoundaries();

    std::vector<TrackBoundary> boundaries;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackDetector)
};
