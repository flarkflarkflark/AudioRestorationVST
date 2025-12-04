#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

/**
 * Real-time Spectrum Analyzer Component
 *
 * Displays frequency spectrum with vertical bars behind EQ sliders.
 * Color gradient: Blue (low) -> Green (mid) -> Red (high)
 */
class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer()
    {
        // Initialize FFT for spectrum analysis
        fft = std::make_unique<juce::dsp::FFT>(fftOrder);

        // Clear spectrum data
        for (auto& level : spectrumLevels)
            level = 0.0f;

        setOpaque (false); // Transparent background
    }

    ~SpectrumAnalyzer() override = default;

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw spectrum bars
        const int numBars = 10; // Match EQ bands
        const float barWidth = bounds.getWidth() / static_cast<float> (numBars);
        const float barSpacing = 4.0f;

        for (int i = 0; i < numBars; ++i)
        {
            float barX = i * barWidth;
            float barHeight = bounds.getHeight() * spectrumLevels[i];

            // Color gradient: Blue (low freq) -> Green (mid) -> Red (high freq)
            juce::Colour barColour;
            float normalizedPos = static_cast<float> (i) / (numBars - 1);

            if (normalizedPos < 0.5f)
            {
                // Blue to Green (low to mid frequencies)
                float t = normalizedPos * 2.0f;
                barColour = juce::Colour::fromRGB (
                    static_cast<juce::uint8> (0 * (1.0f - t) + 0 * t),    // R: 0 -> 0
                    static_cast<juce::uint8> (100 * (1.0f - t) + 255 * t), // G: 100 -> 255
                    static_cast<juce::uint8> (255 * (1.0f - t) + 0 * t)    // B: 255 -> 0
                );
            }
            else
            {
                // Green to Red (mid to high frequencies)
                float t = (normalizedPos - 0.5f) * 2.0f;
                barColour = juce::Colour::fromRGB (
                    static_cast<juce::uint8> (0 * (1.0f - t) + 255 * t),   // R: 0 -> 255
                    static_cast<juce::uint8> (255 * (1.0f - t) + 50 * t),  // G: 255 -> 50
                    static_cast<juce::uint8> (0 * (1.0f - t) + 0 * t)      // B: 0 -> 0
                );
            }

            // Draw bar with gradient from bottom (brighter) to top (darker)
            juce::ColourGradient gradient (
                barColour.withAlpha (0.7f),
                barX + barWidth / 2.0f, bounds.getBottom(),
                barColour.withAlpha (0.3f),
                barX + barWidth / 2.0f, bounds.getBottom() - barHeight,
                false
            );

            g.setGradientFill (gradient);
            g.fillRoundedRectangle (
                barX + barSpacing / 2.0f,
                bounds.getBottom() - barHeight,
                barWidth - barSpacing,
                barHeight,
                2.0f
            );

            // Subtle outline
            g.setColour (barColour.withAlpha (0.5f));
            g.drawRoundedRectangle (
                barX + barSpacing / 2.0f,
                bounds.getBottom() - barHeight,
                barWidth - barSpacing,
                barHeight,
                2.0f,
                1.0f
            );
        }
    }

    //==============================================================================
    /** Push audio samples for spectrum analysis */
    void pushAudioSamples (const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (numSamples == 0)
            return;

        // Mix to mono and push to FIFO
        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                if (i < buffer.getNumSamples())
                    monoSample += buffer.getSample (ch, i);
            }
            monoSample /= static_cast<float> (juce::jmax (1, buffer.getNumChannels()));

            audioFifo[fifoIndex++] = monoSample;

            if (fifoIndex >= fftSize)
            {
                // Perform FFT analysis
                performFFT();
                fifoIndex = 0;
            }
        }
    }

    /** Update spectrum display (call from timer) */
    void updateSpectrum()
    {
        repaint();
    }

private:
    void performFFT()
    {
        // Copy to temporary buffer for FFT processing
        std::array<float, fftSize * 2> fftData = {};
        std::copy(audioFifo.begin(), audioFifo.begin() + fftSize, fftData.begin());

        // Apply Hann window
        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);
        window.multiplyWithWindowingTable (fftData.data(), fftSize);

        // Perform FFT
        fft->performFrequencyOnlyForwardTransform (fftData.data());

        // EQ band center frequencies: 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
        const float sampleRate = 44100.0f;
        const float binWidth = sampleRate / (float)fftSize;

        // Define frequency ranges for each band (in Hz)
        const std::array<std::pair<float, float>, 10> bandFreqs = {{
            {20.0f, 44.0f},      // 31 Hz band
            {44.0f, 88.0f},      // 62 Hz band
            {88.0f, 177.0f},     // 125 Hz band
            {177.0f, 354.0f},    // 250 Hz band
            {354.0f, 707.0f},    // 500 Hz band
            {707.0f, 1414.0f},   // 1 kHz band
            {1414.0f, 2828.0f},  // 2 kHz band
            {2828.0f, 5657.0f},  // 4 kHz band
            {5657.0f, 11314.0f}, // 8 kHz band
            {11314.0f, 22050.0f} // 16 kHz band
        }};

        for (int band = 0; band < 10; ++band)
        {
            float lowFreq = bandFreqs[band].first;
            float highFreq = bandFreqs[band].second;

            // Convert frequencies to bin indices
            int lowBin = (int)(lowFreq / binWidth);
            int highBin = (int)(highFreq / binWidth);

            // Clamp to valid range
            lowBin = juce::jmax(0, juce::jmin(lowBin, fftSize / 2 - 1));
            highBin = juce::jmax(lowBin + 1, juce::jmin(highBin, fftSize / 2));

            // Calculate average magnitude in this frequency band
            float bandLevel = 0.0f;
            int numBins = highBin - lowBin;
            if (numBins > 0)
            {
                for (int bin = lowBin; bin < highBin; ++bin)
                {
                    bandLevel += fftData[bin];
                }
                bandLevel /= (float)numBins;
            }

            // Scale and normalize
            float normalizedLevel = juce::jlimit (0.0f, 1.0f, bandLevel * 8.0f);

            // Smooth per-band attack/release - INDEPENDENT for each band!
            const float attackCoeff = 0.3f;   // Fast rise
            const float releaseCoeff = 0.92f; // Slow fall

            if (normalizedLevel > spectrumLevels[band])
            {
                // Attack - rise quickly to new level
                spectrumLevels[band] += (normalizedLevel - spectrumLevels[band]) * attackCoeff;
            }
            else
            {
                // Release - fall slowly
                spectrumLevels[band] *= releaseCoeff;
            }
        }
    }

    //==============================================================================
    static constexpr int fftOrder = 11; // 2048 point FFT
    static constexpr int fftSize = 1 << fftOrder;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::array<float, fftSize * 2> audioFifo = {};
    int fifoIndex = 0;

    std::array<float, 10> spectrumLevels = {}; // 10 bands matching EQ

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
