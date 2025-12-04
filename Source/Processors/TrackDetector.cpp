#include "TrackDetector.h"
#include <juce_audio_formats/juce_audio_formats.h>

TrackDetector::TrackDetector()
{
}

std::vector<TrackDetector::TrackBoundary> TrackDetector::detectTracks (const juce::AudioBuffer<float>& buffer,
                                                                       double sampleRate,
                                                                       const DetectionSettings& settings)
{
    std::vector<TrackBoundary> detectedBoundaries;

    if (buffer.getNumSamples() == 0)
        return detectedBoundaries;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int windowSize = settings.rmsWindowSamples;

    // Convert thresholds to sample counts
    const int64_t minSilenceSamples = (int64_t) (settings.minSilenceDurationSeconds * sampleRate);
    const int64_t minTrackSamples = (int64_t) (settings.minTrackDurationSeconds * sampleRate);

    // Detect silence regions
    std::vector<std::pair<int64_t, int64_t>> silenceRegions; // [start, end] pairs

    int64_t silenceStart = -1;
    bool inSilence = false;

    for (int64_t i = 0; i < numSamples; i += windowSize / 2) // 50% overlap
    {
        int samplesToCheck = juce::jmin (windowSize, (int) (numSamples - i));

        // Calculate level (RMS or peak) across all channels
        float level = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float channelLevel = settings.useRMSDetection
                ? calculateRMS (buffer, ch, (int) i, samplesToCheck)
                : calculatePeak (buffer, ch, (int) i, samplesToCheck);

            level = juce::jmax (level, channelLevel);
        }

        // Convert to dB
        float levelDb = juce::Decibels::gainToDecibels (level, -100.0f);

        bool isSilent = levelDb < settings.silenceThresholdDb;

        if (isSilent && !inSilence)
        {
            // Start of silence
            silenceStart = i;
            inSilence = true;
        }
        else if (!isSilent && inSilence)
        {
            // End of silence
            int64_t silenceEnd = i;
            int64_t silenceDuration = silenceEnd - silenceStart;

            if (silenceDuration >= minSilenceSamples)
            {
                silenceRegions.emplace_back (silenceStart, silenceEnd);
            }

            inSilence = false;
        }
    }

    // Handle silence at end of file
    if (inSilence)
    {
        int64_t silenceDuration = numSamples - silenceStart;
        if (silenceDuration >= minSilenceSamples)
        {
            silenceRegions.emplace_back (silenceStart, numSamples);
        }
    }

    // Create track boundaries at midpoints of silence regions
    // First track starts at 0
    int64_t lastTrackEnd = 0;

    for (const auto& silenceRegion : silenceRegions)
    {
        // Calculate midpoint of silence
        int64_t midpoint = (silenceRegion.first + silenceRegion.second) / 2;

        // Check if track is long enough
        int64_t trackDuration = midpoint - lastTrackEnd;
        if (trackDuration >= minTrackSamples)
        {
            detectedBoundaries.emplace_back (midpoint, false);
            lastTrackEnd = midpoint;
        }
    }

    // Store detected boundaries
    boundaries = detectedBoundaries;
    sortBoundaries();

    return boundaries;
}

void TrackDetector::addManualBoundary (int64_t position, const juce::String& name)
{
    boundaries.emplace_back (position, true, name);
    sortBoundaries();
}

void TrackDetector::removeBoundary (int index)
{
    if (index >= 0 && index < static_cast<int> (boundaries.size()))
    {
        boundaries.erase (boundaries.begin() + index);
    }
}

void TrackDetector::clearBoundaries()
{
    boundaries.clear();
}

std::vector<juce::AudioBuffer<float>> TrackDetector::splitIntoTracks (const juce::AudioBuffer<float>& buffer,
                                                                      double sampleRate,
                                                                      bool applyFades)
{
    std::vector<juce::AudioBuffer<float>> tracks;

    if (boundaries.empty())
    {
        // No boundaries - return entire buffer as single track
        juce::AudioBuffer<float> track (buffer.getNumChannels(), buffer.getNumSamples());

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            track.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

        if (applyFades)
        {
            int fadeSamples = (int) (0.01 * sampleRate); // 10ms fade
            applyFadeInOut (track, fadeSamples);
        }

        tracks.push_back (std::move (track));
        return tracks;
    }

    sortBoundaries();

    // Create tracks between boundaries
    int64_t startSample = 0;

    for (const auto& boundary : boundaries)
    {
        int64_t endSample = boundary.position;
        int64_t trackLength = endSample - startSample;

        if (trackLength > 0)
        {
            juce::AudioBuffer<float> track (buffer.getNumChannels(), (int) trackLength);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                track.copyFrom (ch, 0, buffer, ch, (int) startSample, (int) trackLength);

            if (applyFades)
            {
                int fadeSamples = juce::jmin ((int) (0.01 * sampleRate), (int) trackLength / 10);
                applyFadeInOut (track, fadeSamples);
            }

            tracks.push_back (std::move (track));
        }

        startSample = endSample;
    }

    // Final track (from last boundary to end)
    int64_t finalTrackLength = buffer.getNumSamples() - startSample;
    if (finalTrackLength > 0)
    {
        juce::AudioBuffer<float> track (buffer.getNumChannels(), (int) finalTrackLength);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            track.copyFrom (ch, 0, buffer, ch, (int) startSample, (int) finalTrackLength);

        if (applyFades)
        {
            int fadeSamples = juce::jmin ((int) (0.01 * sampleRate), (int) finalTrackLength / 10);
            applyFadeInOut (track, fadeSamples);
        }

        tracks.push_back (std::move (track));
    }

    return tracks;
}

bool TrackDetector::exportTracks (const juce::AudioBuffer<float>& buffer,
                                 double sampleRate,
                                 const juce::File& outputDirectory,
                                 const juce::String& baseName,
                                 const juce::String& extension)
{
    if (!outputDirectory.exists())
        outputDirectory.createDirectory();

    auto tracks = splitIntoTracks (buffer, sampleRate, true);

    juce::WavAudioFormat wavFormat;
    juce::FlacAudioFormat flacFormat;

    juce::AudioFormat* format = nullptr;

    if (extension.toLowerCase() == "wav")
        format = &wavFormat;
    else if (extension.toLowerCase() == "flac")
        format = &flacFormat;
    else
        return false;

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        juce::String trackName = baseName + "_Track_" + juce::String ((int) (i + 1));

        // Use custom name if available
        if (i < boundaries.size() && boundaries[i].name.isNotEmpty())
            trackName = baseName + "_" + boundaries[i].name;

        juce::File outputFile = outputDirectory.getChildFile (trackName + "." + extension);

        // Create output stream
        std::unique_ptr<juce::FileOutputStream> fileStream (outputFile.createOutputStream());

        if (fileStream == nullptr)
        {
            DBG ("Failed to create output file: " + outputFile.getFullPathName());
            return false;
        }

        // Create writer
        std::unique_ptr<juce::AudioFormatWriter> writer (format->createWriterFor (fileStream.get(),
                                                                                   sampleRate,
                                                                                   tracks[i].getNumChannels(),
                                                                                   24, // 24-bit
                                                                                   {},
                                                                                   0));

        if (writer != nullptr)
        {
            fileStream.release(); // Writer takes ownership

            // Write audio data
            writer->writeFromAudioSampleBuffer (tracks[i], 0, tracks[i].getNumSamples());

            DBG ("Exported track: " + outputFile.getFullPathName());
        }
        else
        {
            DBG ("Failed to create writer for: " + outputFile.getFullPathName());
            return false;
        }
    }

    return true;
}

float TrackDetector::calculateRMS (const juce::AudioBuffer<float>& buffer, int channel,
                                   int startSample, int numSamples)
{
    if (channel >= buffer.getNumChannels() || numSamples == 0)
        return 0.0f;

    const float* data = buffer.getReadPointer (channel, startSample);

    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        sum += data[i] * data[i];

    return std::sqrt (sum / numSamples);
}

float TrackDetector::calculatePeak (const juce::AudioBuffer<float>& buffer, int channel,
                                    int startSample, int numSamples)
{
    if (channel >= buffer.getNumChannels() || numSamples == 0)
        return 0.0f;

    const float* data = buffer.getReadPointer (channel, startSample);

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = juce::jmax (peak, std::abs (data[i]));

    return peak;
}

void TrackDetector::applyFadeInOut (juce::AudioBuffer<float>& buffer, int fadeSamples)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    fadeSamples = juce::jmin (fadeSamples, numSamples / 2);

    // Fade in
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);

        for (int i = 0; i < fadeSamples; ++i)
        {
            float gain = (float) i / (float) fadeSamples;
            data[i] *= gain;
        }
    }

    // Fade out
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);

        for (int i = 0; i < fadeSamples; ++i)
        {
            int sampleIndex = numSamples - fadeSamples + i;
            float gain = 1.0f - ((float) i / (float) fadeSamples);
            data[sampleIndex] *= gain;
        }
    }
}

void TrackDetector::sortBoundaries()
{
    std::sort (boundaries.begin(), boundaries.end(),
               [](const TrackBoundary& a, const TrackBoundary& b) { return a.position < b.position; });
}
