#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Decrackle Processor
 *
 * Clean-room implementation inspired by classic decrackle concepts:
 * detect rapid changes in the second derivative and replace flagged
 * samples with a local average.
 */
class Decrackle
{
public:
    Decrackle() = default;

    void setFactor (float newFactor)
    {
        factor = juce::jlimit (0.01f, 1.0f, newFactor);
    }

    void setAverageWidth (int newWidth)
    {
        averageWidth = juce::jlimit (1, 10, newWidth);
    }

    void process (juce::AudioBuffer<float>& buffer);

private:
    float factor = 0.5f;
    int averageWidth = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Decrackle)
};
