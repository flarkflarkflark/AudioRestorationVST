#pragma once

#include <juce_dsp/juce_dsp.h>

/**
 * Spectral Processor Utilities
 *
 * Helper class for FFT-based spectral analysis and processing.
 * Used by noise reduction and spectrum display.
 */
class SpectralProcessor
{
public:
    SpectralProcessor() = default;

    //==============================================================================
    /** Initialize FFT with specified order */
    void initialize (int order)
    {
        fftOrder = order;
        fftSize = 1 << fftOrder;

        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        fftData.resize (fftSize * 2);
        window.resize (fftSize);

        // Create Hann window
        juce::dsp::WindowingFunction<float>::fillWindowingTables (
            window.data(), fftSize,
            juce::dsp::WindowingFunction<float>::hann, false);
    }

    //==============================================================================
    /** Perform forward FFT with windowing */
    void performFFT (const float* inputData, float* realOut, float* imagOut)
    {
        // Copy and apply window
        for (int i = 0; i < fftSize; ++i)
        {
            fftData[i] = inputData[i] * window[i];
            fftData[fftSize + i] = 0.0f; // Clear imaginary part
        }

        // Perform FFT
        fft->performRealOnlyForwardTransform (fftData.data(), true);

        // Extract real and imaginary parts
        for (int i = 0; i < fftSize / 2 + 1; ++i)
        {
            realOut[i] = fftData[i * 2];
            imagOut[i] = fftData[i * 2 + 1];
        }
    }

    /** Perform inverse FFT */
    void performIFFT (const float* realIn, const float* imagIn, float* outputData)
    {
        // Reconstruct complex spectrum
        for (int i = 0; i < fftSize / 2 + 1; ++i)
        {
            fftData[i * 2] = realIn[i];
            fftData[i * 2 + 1] = imagIn[i];
        }

        // Perform inverse FFT
        fft->performRealOnlyInverseTransform (fftData.data());

        // Copy output with window compensation
        for (int i = 0; i < fftSize; ++i)
            outputData[i] = fftData[i];
    }

    //==============================================================================
    /** Calculate magnitude spectrum */
    static void calculateMagnitude (const float* real, const float* imag,
                                     float* magnitude, int numBins)
    {
        for (int i = 0; i < numBins; ++i)
        {
            magnitude[i] = std::sqrt (real[i] * real[i] + imag[i] * imag[i]);
        }
    }

    /** Calculate phase spectrum */
    static void calculatePhase (const float* real, const float* imag,
                                float* phase, int numBins)
    {
        for (int i = 0; i < numBins; ++i)
        {
            phase[i] = std::atan2 (imag[i], real[i]);
        }
    }

    /** Reconstruct from magnitude and phase */
    static void reconstructFromMagPhase (float* real, float* imag,
                                         const float* magnitude, const float* phase,
                                         int numBins)
    {
        for (int i = 0; i < numBins; ++i)
        {
            real[i] = magnitude[i] * std::cos (phase[i]);
            imag[i] = magnitude[i] * std::sin (phase[i]);
        }
    }

    //==============================================================================
    int getFFTSize() const { return fftSize; }
    int getFFTOrder() const { return fftOrder; }

private:
    //==============================================================================
    int fftOrder = 11;
    int fftSize = 2048;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftData;
    std::vector<float> window;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralProcessor)
};
