#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Click and Pop Removal Processor
 *
 * Implements multiple click removal strategies:
 * 1. Cubic spline interpolation for larger clicks
 * 2. Crossfade/envelope smoothing for smaller pops
 * 3. Automatic detection with adjustable sensitivity
 *
 * Based on Wave Corrector's approach combined with manual crossfade technique.
 */
class ClickRemoval
{
public:
    //==============================================================================
    /** Information about a detected or manual click correction */
    struct ClickInfo
    {
        int64_t position = 0;       // Sample position in stream
        int width = 0;              // Width in samples
        float magnitude = 0.0f;     // Detected magnitude
        bool isManual = false;      // User-inserted correction
    };

    ClickRemoval() = default;

    //==============================================================================
    /** Initialize with audio specifications */
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        reset();
    }

    /** Reset internal state */
    void reset()
    {
        detectedClicks.clear();
    }

    /** Process audio block */
    void process (juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();

        // TODO: Implement click detection and removal
        // For now, just pass through
        outputBlock.copyFrom (inputBlock);

        if (sensitivity > 0.0f)
        {
            detectAndRemoveClicks (outputBlock);
        }
    }

    //==============================================================================
    /** Set click detection sensitivity (0-100) */
    void setSensitivity (float newSensitivity)
    {
        sensitivity = juce::jlimit (0.0f, 100.0f, newSensitivity);
    }

    /** Set maximum width for click correction in samples */
    void setMaxWidth (int samples)
    {
        maxClickWidth = samples;
    }

    /** Set removal method */
    enum RemovalMethod
    {
        SplineInterpolation,    // Cubic spline for large clicks
        CrossfadeSmoothing,     // Fade in/out for small pops (Reaper-style)
        Automatic               // Choose based on click size
    };

    void setRemovalMethod (RemovalMethod method)
    {
        removalMethod = method;
    }

    //==============================================================================
    /** Manually mark a click for removal (for GUI/standalone mode) */
    void addManualClick (int64_t samplePosition, int width)
    {
        ClickInfo click;
        click.position = samplePosition;
        click.width = width;
        click.isManual = true;
        detectedClicks.push_back (click);
    }

    /** Get list of detected clicks (for GUI display) */
    const std::vector<ClickInfo>& getDetectedClicks() const
    {
        return detectedClicks;
    }

private:

    //==============================================================================
    void detectAndRemoveClicks (juce::dsp::AudioBlock<float>& block)
    {
        // TODO: Implement click detection algorithm
        // 1. Calculate first and second derivatives
        // 2. Detect sharp discontinuities above threshold
        // 3. Filter out periodic signals (music)
        // 4. Add detected clicks to list

        for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
        {
            auto* channelData = block.getChannelPointer (channel);
            auto numSamples = block.getNumSamples();

            // Simple derivative-based detection (placeholder)
            for (size_t i = 2; i < numSamples - 2; ++i)
            {
                float derivative = std::abs (channelData[i] - channelData[i - 1]);
                float threshold = 0.1f * (sensitivity / 100.0f);

                if (derivative > threshold)
                {
                    // Potential click detected - apply removal
                    removeClickAt (channelData, static_cast<int> (i), numSamples);
                }
            }
        }
    }

    void removeClickAt (float* channelData, int position, size_t numSamples)
    {
        // Choose removal method
        RemovalMethod method = removalMethod;
        if (method == Automatic)
        {
            // TODO: Determine based on click characteristics
            method = CrossfadeSmoothing; // Default to crossfade for now
        }

        if (method == CrossfadeSmoothing)
        {
            applyCrossfadeSmoothing (channelData, position, numSamples);
        }
        else if (method == SplineInterpolation)
        {
            applyCubicSpline (channelData, position, numSamples);
        }
    }

    //==============================================================================
    /** Apply crossfade smoothing (Reaper-style manual technique) */
    void applyCrossfadeSmoothing (float* channelData, int position, size_t numSamples)
    {
        // Create a short fade around the click
        int fadeLength = juce::jmin (maxClickWidth / 2, 64); // Typically 32-64 samples
        int fadeStart = juce::jmax (0, position - fadeLength);
        int fadeEnd = juce::jmin (static_cast<int> (numSamples) - 1, position + fadeLength);

        if (fadeEnd <= fadeStart + 1)
            return;

        // Get clean values at boundaries
        float startValue = channelData[fadeStart];
        float endValue = channelData[fadeEnd];

        // Apply smooth crossfade using cosine curve
        int fadeWidth = fadeEnd - fadeStart;
        for (int i = fadeStart; i <= fadeEnd; ++i)
        {
            float phase = static_cast<float> (i - fadeStart) / static_cast<float> (fadeWidth);
            // Cosine interpolation for smooth curve
            float weight = 0.5f - 0.5f * std::cos (phase * juce::MathConstants<float>::pi);
            channelData[i] = startValue + (endValue - startValue) * weight;
        }
    }

    //==============================================================================
    /** Apply cubic spline interpolation (Wave Corrector approach) */
    void applyCubicSpline (float* channelData, int position, size_t numSamples)
    {
        // TODO: Implement cubic spline interpolation
        // For now, use simple linear interpolation as placeholder

        int width = juce::jmin (maxClickWidth, 128);
        int start = juce::jmax (0, position - width / 2);
        int end = juce::jmin (static_cast<int> (numSamples) - 1, position + width / 2);

        if (end <= start + 1)
            return;

        float startValue = channelData[start];
        float endValue = channelData[end];

        // Linear interpolation (TODO: replace with cubic spline)
        for (int i = start; i <= end; ++i)
        {
            float t = static_cast<float> (i - start) / static_cast<float> (end - start);
            channelData[i] = startValue + (endValue - startValue) * t;
        }
    }

    //==============================================================================
    double sampleRate = 44100.0;
    juce::uint32 numChannels = 2;
    float sensitivity = 50.0f;
    int maxClickWidth = 512;
    RemovalMethod removalMethod = Automatic;

    std::vector<ClickInfo> detectedClicks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickRemoval)
};
