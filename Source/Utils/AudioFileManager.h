#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

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
    AudioFileManager() = default;

    //==============================================================================
    /** Load audio file */
    bool loadAudioFile (const juce::File& file, juce::AudioBuffer<float>& buffer,
                        double& sampleRate)
    {
        // TODO: Implement file loading
        return false;
    }

    /** Save audio file */
    bool saveAudioFile (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                        double sampleRate, int bitDepth = 16)
    {
        // TODO: Implement file saving
        return false;
    }

    //==============================================================================
    /** Save session file (corrections, settings) */
    bool saveSession (const juce::File& sessionFile,
                      const juce::File& audioFile,
                      const juce::var& sessionData)
    {
        // TODO: Save session as JSON or XML
        return false;
    }

    /** Load session file */
    bool loadSession (const juce::File& sessionFile,
                      juce::File& audioFile,
                      juce::var& sessionData)
    {
        // TODO: Load session from JSON or XML
        return false;
    }

private:
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
};
