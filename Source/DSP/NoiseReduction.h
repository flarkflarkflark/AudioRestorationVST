#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SpectralProcessor.h"

/**
 * Spectral Noise Reduction Processor
 *
 * Implements FFT-based noise reduction using spectral subtraction.
 * Features:
 * - Noise profile capture from silent sections
 * - Adjustable reduction amount
 * - Spectral floor to prevent musical noise artifacts
 * - Dual profile support for varying noise (shellac records)
 */
class NoiseReduction
{
public:
    NoiseReduction() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;

        // Initialize FFT
        fftOrder = 11; // 2048 samples
        fftSize = 1 << fftOrder;
        hopSize = fftSize / 4; // 75% overlap

        fft = std::make_unique<juce::dsp::FFT> (fftOrder);

        // Allocate buffers
        fftBuffer.resize (fftSize * 2); // Real + imaginary
        windowBuffer.resize (fftSize);
        overlapBuffer.setSize (static_cast<int> (numChannels), fftSize);
        overlapBuffer.clear();

        // Create Hann window
        juce::dsp::WindowingFunction<float>::fillWindowingTables (
            windowBuffer.data(), fftSize,
            juce::dsp::WindowingFunction<float>::hann, false);

        // Initialize noise profile
        noiseProfile.resize (fftSize / 2 + 1);
        std::fill (noiseProfile.begin(), noiseProfile.end(), 0.0f);
        profileCaptured = false;

        reset();
    }

    void reset()
    {
        overlapBuffer.clear();
        processingPosition = 0;
    }

    void process (juce::dsp::ProcessContextReplacing<float>& context)
    {
        if (!profileCaptured || reductionAmount <= 0.0f)
        {
            // Bypass if no profile or zero reduction
            return;
        }

        auto& block = context.getOutputBlock();

        // TODO: Implement overlap-add FFT processing
        // For now, bypass
        // processSpectralSubtraction (block);
    }

    //==============================================================================
    /** Capture noise profile from current audio section */
    void captureProfile()
    {
        isCapturingProfile = true;
        profileCaptureFrames = 0;
        std::fill (noiseProfile.begin(), noiseProfile.end(), 0.0f);
    }

    /** Set reduction amount in dB */
    void setReduction (float dB)
    {
        reductionAmount = juce::jlimit (0.0f, 24.0f, dB);
        reductionLinear = juce::Decibels::decibelsToGain (reductionAmount);
    }

    /** Check if noise profile has been captured */
    bool hasProfile() const { return profileCaptured; }

    /** Clear noise profile */
    void clearProfile()
    {
        std::fill (noiseProfile.begin(), noiseProfile.end(), 0.0f);
        profileCaptured = false;
    }

private:
    //==============================================================================
    void processSpectralSubtraction (juce::dsp::AudioBlock<float>& block)
    {
        // TODO: Implement spectral subtraction
        // Algorithm:
        // 1. Apply windowing
        // 2. FFT
        // 3. Compute magnitude spectrum
        // 4. Subtract noise profile from magnitude
        // 5. Apply spectral floor
        // 6. Reconstruct with original phase
        // 7. IFFT
        // 8. Overlap-add with previous frame
    }

    void captureProfileFromBlock (const juce::dsp::AudioBlock<float>& block)
    {
        // TODO: Accumulate FFT magnitude spectra for profile
        if (isCapturingProfile && profileCaptureFrames < maxCaptureFrames)
        {
            // Process and average multiple frames
            profileCaptureFrames++;

            if (profileCaptureFrames >= maxCaptureFrames)
            {
                // Normalize averaged profile
                for (auto& val : noiseProfile)
                    val /= static_cast<float> (maxCaptureFrames);

                profileCaptured = true;
                isCapturingProfile = false;
            }
        }
    }

    //==============================================================================
    double sampleRate = 44100.0;
    juce::uint32 numChannels = 2;

    // FFT parameters
    int fftOrder = 11;
    int fftSize = 2048;
    int hopSize = 512;
    std::unique_ptr<juce::dsp::FFT> fft;

    // Buffers
    std::vector<float> fftBuffer;
    std::vector<float> windowBuffer;
    juce::AudioBuffer<float> overlapBuffer;
    int processingPosition = 0;

    // Noise profile
    std::vector<float> noiseProfile;
    bool profileCaptured = false;
    bool isCapturingProfile = false;
    int profileCaptureFrames = 0;
    const int maxCaptureFrames = 20; // Average over ~1 second

    // Parameters
    float reductionAmount = 0.0f;
    float reductionLinear = 1.0f;
    const float spectralFloor = 0.01f; // -40 dB

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseReduction)
};
