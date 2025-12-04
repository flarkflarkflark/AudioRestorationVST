#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <functional>

/**
 * Audio File Manager
 *
 * Handles:
 * - Audio file I/O (WAV, FLAC, MP3, OGG)
 * - Session save/load (corrections, settings, track boundaries)
 * - Metadata management
 *
 * Standalone mode only.
 */
class AudioFileManager
{
public:
    AudioFileManager();

    //==============================================================================
    /** Progress callback: (progress 0.0-1.0, statusMessage) -> shouldContinue */
    using ProgressCallback = std::function<bool(double progress, const juce::String& status)>;

    /** Load audio file (WAV, FLAC, MP3, OGG) */
    bool loadAudioFile (const juce::File& file,
                        juce::AudioBuffer<float>& buffer,
                        double& sampleRate);

    /** Load audio file with progress reporting */
    bool loadAudioFileWithProgress (const juce::File& file,
                                    juce::AudioBuffer<float>& buffer,
                                    double& sampleRate,
                                    ProgressCallback progressCallback);

    /** Save audio file (WAV or FLAC) */
    bool saveAudioFile (const juce::File& file,
                        const juce::AudioBuffer<float>& buffer,
                        double sampleRate,
                        int bitDepth = 16);

    //==============================================================================
    /** Save session file (corrections, settings) as JSON */
    bool saveSession (const juce::File& sessionFile,
                      const juce::File& audioFile,
                      const juce::var& sessionData);

    /** Load session file from JSON */
    bool loadSession (const juce::File& sessionFile,
                      juce::File& audioFile,
                      juce::var& sessionData);

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
};
