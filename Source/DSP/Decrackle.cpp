#include "Decrackle.h"

#include <cmath>
#include <cstring>

void Decrackle::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples < 3 || numChannels == 0)
        return;

    const int width = juce::jmin (averageWidth, numSamples / 2);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);

        double absSum = 0.0;
        for (int i = 1; i < numSamples; ++i)
            absSum += std::abs (data[i] - data[i - 1]);

        const float meanAbsDelta = (numSamples > 1) ? static_cast<float> (absSum / (numSamples - 1)) : 0.0f;
        const float scaledFactor = factor * meanAbsDelta;

        std::vector<uint8_t> flags (static_cast<size_t> (numSamples), 0);

        for (int i = 2; i < numSamples; ++i)
        {
            const float dy0 = data[i - 1] - data[i - 2];
            const float dy1 = data[i] - data[i - 1];
            const float dy2dx = dy1 - dy0;

            if (std::abs (dy2dx) > scaledFactor)
                flags[static_cast<size_t> (i - 1)] = 1;
        }

        std::vector<float> temp (data, data + numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const int start = juce::jmax (0, i - width);
            const int end = juce::jmin (numSamples - 1, i + width);

            float sum = 0.0f;
            const int count = end - start + 1;
            for (int j = start; j <= end; ++j)
                sum += temp[static_cast<size_t> (j)];

            const float mean = (count > 0) ? (sum / static_cast<float> (count)) : temp[static_cast<size_t> (i)];
            const bool neighborFlagged = flags[static_cast<size_t> (i)] ||
                                         (i > 0 && flags[static_cast<size_t> (i - 1)]) ||
                                         (i + 1 < numSamples && flags[static_cast<size_t> (i + 1)]);

            if (flags[static_cast<size_t> (i)] != 0)
                temp[static_cast<size_t> (i)] = mean;
            else if (neighborFlagged)
                temp[static_cast<size_t> (i)] = 0.5f * (temp[static_cast<size_t> (i)] + mean);
        }

        std::memcpy (data, temp.data(), static_cast<size_t> (numSamples) * sizeof (float));
    }
}
