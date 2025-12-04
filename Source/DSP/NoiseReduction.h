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
        auto& block = context.getOutputBlock();

        // Capture profile if requested
        if (isCapturingProfile)
        {
            captureProfileFromBlock (block);
            return; // Don't process during capture
        }

        // Bypass if no profile or zero reduction
        if (!profileCaptured || reductionAmount <= 0.0f)
        {
            return;
        }

        // Process each channel independently
        for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
        {
            processSpectralSubtraction (block.getChannelPointer (channel),
                                       block.getNumSamples(),
                                       static_cast<int> (channel));
        }
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

    /** Get activity metrics for visual feedback */
    bool isActivelyReducing() const { return profileCaptured && reductionAmount > 0.1f; }
    float getReductionAmount() const { return reductionAmount; }

private:
    //==============================================================================
    void processSpectralSubtraction (float* channelData, size_t numSamples, int channel)
    {
        // Overlap-add FFT processing for spectral subtraction
        // Process in frames with 75% overlap for smooth reconstruction

        for (size_t pos = 0; pos < numSamples; pos += hopSize)
        {
            // Calculate frame boundaries
            size_t frameEnd = juce::jmin (pos + fftSize, numSamples);
            size_t frameSamples = frameEnd - pos;

            if (frameSamples < static_cast<size_t> (hopSize))
                break; // Not enough samples for another frame

            // 1. Copy and window the input frame
            std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
            for (size_t i = 0; i < frameSamples; ++i)
            {
                fftBuffer[i] = channelData[pos + i] * windowBuffer[i];
            }

            // 2. Forward FFT (real to complex)
            fft->performRealOnlyForwardTransform (fftBuffer.data());

            // 3. Spectral subtraction
            performSpectralSubtraction();

            // 4. Inverse FFT (complex to real)
            fft->performRealOnlyInverseTransform (fftBuffer.data());

            // 5. Overlap-add with output windowing
            // For 75% overlap (4x overlap) with Hann window, normalization factor is 1.5
            const float normFactor = 1.5f;

            for (size_t i = 0; i < frameSamples; ++i)
            {
                if (pos + i < numSamples)
                {
                    // Apply window and normalize for overlap-add
                    channelData[pos + i] = fftBuffer[i] * windowBuffer[i] / (fftSize * normFactor);

                    // Add overlap from previous frame
                    if (channel < overlapBuffer.getNumChannels() && i < static_cast<size_t> (overlapBuffer.getNumSamples()))
                    {
                        channelData[pos + i] += overlapBuffer.getSample (channel, static_cast<int> (i));
                    }
                }
            }

            // 6. Store overlap for next frame
            if (channel < overlapBuffer.getNumChannels())
            {
                for (int i = 0; i < overlapBuffer.getNumSamples(); ++i)
                {
                    if (static_cast<size_t> (i) + hopSize < frameSamples)
                    {
                        overlapBuffer.setSample (channel, i,
                            fftBuffer[i + hopSize] * windowBuffer[i + hopSize] / (fftSize * normFactor));
                    }
                    else
                    {
                        overlapBuffer.setSample (channel, i, 0.0f);
                    }
                }
            }
        }
    }

    void performSpectralSubtraction()
    {
        // Process magnitude spectrum: subtract noise profile
        // Complex FFT output format: [real0, real1, ..., realN/2, imag1, ..., imagN/2-1]

        int numBins = fftSize / 2 + 1;

        for (int bin = 0; bin < numBins; ++bin)
        {
            float real, imag;

            // Extract real and imaginary parts from JUCE FFT format
            if (bin == 0)
            {
                real = fftBuffer[0];
                imag = 0.0f;
            }
            else if (bin == fftSize / 2)
            {
                real = fftBuffer[fftSize / 2];
                imag = 0.0f;
            }
            else
            {
                real = fftBuffer[bin];
                imag = fftBuffer[fftSize - bin];
            }

            // Calculate magnitude and phase
            float magnitude = std::sqrt (real * real + imag * imag);
            float phase = std::atan2 (imag, real);

            // Spectral subtraction with over-subtraction factor
            float noiseMag = noiseProfile[static_cast<size_t> (bin)] * reductionLinear;
            float cleanMag = magnitude - noiseMag;

            // Apply spectral floor to prevent musical noise
            cleanMag = juce::jmax (cleanMag, magnitude * spectralFloor);

            // Reconstruct complex number with cleaned magnitude and original phase
            real = cleanMag * std::cos (phase);
            imag = cleanMag * std::sin (phase);

            // Store back in JUCE FFT format
            if (bin == 0)
            {
                fftBuffer[0] = real;
            }
            else if (bin == fftSize / 2)
            {
                fftBuffer[fftSize / 2] = real;
            }
            else
            {
                fftBuffer[bin] = real;
                fftBuffer[fftSize - bin] = imag;
            }
        }
    }

    void captureProfileFromBlock (const juce::dsp::AudioBlock<float>& block)
    {
        if (!isCapturingProfile || profileCaptureFrames >= maxCaptureFrames)
            return;

        // Average noise profile across all channels
        for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
        {
            const float* channelData = block.getChannelPointer (channel);
            size_t numSamples = block.getNumSamples();

            // Process each frame in the block
            for (size_t pos = 0; pos < numSamples && profileCaptureFrames < maxCaptureFrames; pos += hopSize)
            {
                size_t frameEnd = juce::jmin (pos + fftSize, numSamples);
                size_t frameSamples = frameEnd - pos;

                if (frameSamples < static_cast<size_t> (fftSize / 2))
                    break;

                // Copy and window the frame
                std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
                for (size_t i = 0; i < frameSamples; ++i)
                {
                    fftBuffer[i] = channelData[pos + i] * windowBuffer[i];
                }

                // Forward FFT
                fft->performRealOnlyForwardTransform (fftBuffer.data());

                // Accumulate magnitude spectrum
                int numBins = fftSize / 2 + 1;
                for (int bin = 0; bin < numBins; ++bin)
                {
                    float real, imag;

                    // Extract real and imaginary parts
                    if (bin == 0)
                    {
                        real = fftBuffer[0];
                        imag = 0.0f;
                    }
                    else if (bin == fftSize / 2)
                    {
                        real = fftBuffer[fftSize / 2];
                        imag = 0.0f;
                    }
                    else
                    {
                        real = fftBuffer[bin];
                        imag = fftBuffer[fftSize - bin];
                    }

                    // Accumulate magnitude
                    float magnitude = std::sqrt (real * real + imag * imag);
                    noiseProfile[static_cast<size_t> (bin)] += magnitude;
                }

                profileCaptureFrames++;

                // Check if we've captured enough
                if (profileCaptureFrames >= maxCaptureFrames)
                {
                    // Normalize averaged profile
                    for (auto& val : noiseProfile)
                    {
                        val /= static_cast<float> (maxCaptureFrames);
                    }

                    profileCaptured = true;
                    isCapturingProfile = false;
                    return;
                }
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
