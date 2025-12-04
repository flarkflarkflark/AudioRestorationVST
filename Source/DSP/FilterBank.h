#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/**
 * Filter Bank Processor
 *
 * Implements:
 * - Rumble filter (high-pass, 20-120 Hz)
 * - Hum filter (notch at 50/60 Hz)
 * - 10-band graphic EQ (31 Hz to 16 kHz)
 * - Per-band activity metering for visual feedback
 */
class FilterBank
{
public:
    FilterBank()
    {
        // Initialize band activity levels
        for (int i = 0; i < 10; ++i)
        {
            bandActivityLevels[i].store (0.0f);
            smoothedBandLevels[i] = 0.0f;
        }
    }

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Prepare filters
        juce::dsp::ProcessSpec monoSpec = spec;
        monoSpec.numChannels = 1;

        rumbleFilterL.prepare (monoSpec);
        rumbleFilterR.prepare (monoSpec);
        rumbleFilterL.reset();
        rumbleFilterR.reset();

        humFilterL.prepare (monoSpec);
        humFilterR.prepare (monoSpec);
        humFilterL.reset();
        humFilterR.reset();

        // Initialize 10-band EQ
        eqBandsL.clear();
        eqBandsR.clear();
        eqCoeffs.clear();
        eqMeteringBandsL.clear();
        meteringCoeffs.clear();

        for (int i = 0; i < 10; ++i)
        {
            eqBandsL.emplace_back();
            eqBandsR.emplace_back();
            eqBandsL[i].prepare (monoSpec);
            eqBandsR[i].prepare (monoSpec);
            eqBandsL[i].reset();
            eqBandsR[i].reset();
            eqCoeffs.push_back (nullptr);

            // Metering filters
            eqMeteringBandsL.emplace_back();
            eqMeteringBandsL[i].prepare (monoSpec);
            eqMeteringBandsL[i].reset();
            meteringCoeffs.push_back (nullptr);
        }

        updateFilters();
    }

    void reset()
    {
        rumbleFilterL.reset();
        rumbleFilterR.reset();
        humFilterL.reset();
        humFilterR.reset();
        for (size_t i = 0; i < eqBandsL.size(); ++i)
        {
            eqBandsL[i].reset();
            eqBandsR[i].reset();
        }
    }

    void process (juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto& block = context.getOutputBlock();
        auto numChannels = block.getNumChannels();
        auto numSamples = block.getNumSamples();

        if (numChannels == 0 || numSamples == 0)
            return;

        // Process left channel (or mono)
        auto leftBlock = block.getSingleChannelBlock (0);
        juce::dsp::ProcessContextReplacing<float> leftContext (leftBlock);

        if (!rumbleBypass)
            rumbleFilterL.process (leftContext);
        if (!humBypass)
            humFilterL.process (leftContext);
        for (size_t i = 0; i < eqBandsL.size(); ++i)
            if (std::abs (eqGains[i]) > 0.01f)
                eqBandsL[i].process (leftContext);

        // Process right channel if stereo
        if (numChannels > 1)
        {
            auto rightBlock = block.getSingleChannelBlock (1);
            juce::dsp::ProcessContextReplacing<float> rightContext (rightBlock);

            if (!rumbleBypass)
                rumbleFilterR.process (rightContext);
            if (!humBypass)
                humFilterR.process (rightContext);
            for (size_t i = 0; i < eqBandsR.size(); ++i)
                if (std::abs (eqGains[i]) > 0.01f)
                    eqBandsR[i].process (rightContext);
        }
    }

    //==============================================================================
    /** Set rumble filter cutoff frequency (5-150 Hz) */
    void setRumbleFilter (float cutoffHz, bool bypass)
    {
        rumbleBypass = bypass;
        rumbleFreq = juce::jlimit (5.0f, 150.0f, cutoffHz);
        updateRumbleFilter();
    }

    /** Set hum filter center frequency (40-80 Hz, covers 50/60Hz and harmonics) */
    void setHumFilter (float centerHz, bool bypass)
    {
        humBypass = bypass;
        humFreq = juce::jlimit (40.0f, 80.0f, centerHz);
        updateHumFilter();
    }

    /** Set EQ band gain */
    void setEQBand (int bandIndex, float gainDB)
    {
        if (bandIndex >= 0 && bandIndex < 10)
        {
            eqGains[bandIndex] = juce::jlimit (-12.0f, 12.0f, gainDB);
            updateEQBand (bandIndex);
        }
    }

    /** Get activity level for specific EQ band (0.0 to 1.0) */
    float getBandActivityLevel (int bandIndex) const
    {
        if (bandIndex >= 0 && bandIndex < 10)
            return bandActivityLevels[bandIndex].load();
        return 0.0f;
    }

    /** Measure band activity for visual feedback metering - call independently from processing */
    void measureBandActivityForMetering (const juce::dsp::AudioBlock<float>& block)
    {
        measureBandActivity (block);
    }

private:
    //==============================================================================
    /** Measure activity level in each frequency band for visual feedback */
    void measureBandActivity (const juce::dsp::AudioBlock<float>& block)
    {
        // Create temporary buffer for band-pass filtered signal
        juce::AudioBuffer<float> tempBuffer (static_cast<int> (block.getNumChannels()),
                                              static_cast<int> (block.getNumSamples()));

        // Measure each band
        for (size_t bandIdx = 0; bandIdx < eqMeteringBandsL.size(); ++bandIdx)
        {
            // Copy input to temp buffer
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                juce::FloatVectorOperations::copy (tempBuffer.getWritePointer (static_cast<int> (ch)),
                                                   block.getChannelPointer (ch),
                                                   static_cast<int> (block.getNumSamples()));
            }

            // Apply band-pass filter for metering (left channel)
            auto tempBlock = juce::dsp::AudioBlock<float> (tempBuffer);
            auto singleChannelBlock = tempBlock.getSingleChannelBlock (0);
            juce::dsp::ProcessContextReplacing<float> meteringContext (singleChannelBlock);
            eqMeteringBandsL[bandIdx].process (meteringContext);

            // Calculate BOTH RMS and Peak level of filtered signal for dramatic response
            float rmsLevel = 0.0f;
            float peakLevel = 0.0f;
            for (size_t ch = 0; ch < block.getNumChannels() && ch < 2; ++ch)
            {
                const float* data = tempBuffer.getReadPointer (static_cast<int> (ch));
                float sum = 0.0f;
                float localPeak = 0.0f;
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    sum += data[i] * data[i];
                    localPeak = juce::jmax (localPeak, std::abs (data[i]));
                }
                rmsLevel += std::sqrt (sum / static_cast<float> (block.getNumSamples()));
                peakLevel = juce::jmax (peakLevel, localPeak);
            }
            rmsLevel /= static_cast<float> (block.getNumChannels());

            // Combine RMS and Peak for more dramatic visual response
            // 70% peak (fast/dramatic) + 30% RMS (smooth/stable)
            float combinedLevel = peakLevel * 0.7f + rmsLevel * 0.3f;

            // Smooth the level with envelope follower (attack/release)
            const float attackCoeff = 0.5f;   // Very fast attack for realtime response
            const float releaseCoeff = 0.97f; // Medium release for smooth but responsive visual

            if (combinedLevel > smoothedBandLevels[bandIdx])
                smoothedBandLevels[bandIdx] = combinedLevel * (1.0f - attackCoeff) + smoothedBandLevels[bandIdx] * attackCoeff;
            else
                smoothedBandLevels[bandIdx] = combinedLevel * (1.0f - releaseCoeff) + smoothedBandLevels[bandIdx] * releaseCoeff;

            // Moderate normalization for selective visibility
            // Only boost when there's actual content in that specific frequency band
            float normalizedLevel = smoothedBandLevels[bandIdx] * 6.0f; // More selective (was 12.0)
            normalizedLevel = juce::jlimit (0.0f, 1.0f, normalizedLevel);

            // Store for GUI access
            bandActivityLevels[bandIdx].store (normalizedLevel);
        }
    }

    void updateFilters()
    {
        updateRumbleFilter();
        updateHumFilter();
        for (int i = 0; i < 10; ++i)
        {
            updateEQBand (i);
            updateMeteringBand (i);
        }
    }

    void updateRumbleFilter()
    {
        // 4th order Butterworth high-pass
        rumbleCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sampleRate, rumbleFreq, 0.707f);
        rumbleFilterL.coefficients = rumbleCoeffs;
        rumbleFilterR.coefficients = rumbleCoeffs;
    }

    void updateHumFilter()
    {
        // Notch filter with Q=30 for sharp rejection
        humCoeffs = juce::dsp::IIR::Coefficients<float>::makeNotch (
            sampleRate, humFreq, 30.0f);
        humFilterL.coefficients = humCoeffs;
        humFilterR.coefficients = humCoeffs;
    }

    void updateEQBand (int bandIndex)
    {
        const std::array<float, 10> eqFreqs = {
            31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
            1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
        };

        if (bandIndex >= 0 && bandIndex < 10)
        {
            float freq = eqFreqs[bandIndex];
            float gain = eqGains[bandIndex];
            float Q = 1.0f; // Moderate Q for graphic EQ

            // Peaking filter for each band
            eqCoeffs[bandIndex] = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, freq, Q, juce::Decibels::decibelsToGain (gain));
            eqBandsL[bandIndex].coefficients = eqCoeffs[bandIndex];
            eqBandsR[bandIndex].coefficients = eqCoeffs[bandIndex];
        }
    }

    void updateMeteringBand (int bandIndex)
    {
        const std::array<float, 10> eqFreqs = {
            31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
            1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
        };

        if (bandIndex >= 0 && bandIndex < 10)
        {
            float freq = eqFreqs[bandIndex];
            // ULTRA-high Q for EXTREMELY selective frequency detection
            // This makes each band respond ONLY to its specific narrow frequency range
            float Q = 15.0f; // Ultra-tight band measurement (was 4.0)

            // Band-pass filter for activity metering with minimal boost
            meteringCoeffs[bandIndex] = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate, freq, Q, juce::Decibels::decibelsToGain (3.0f)); // +3dB boost (was +6dB)
            eqMeteringBandsL[bandIndex].coefficients = meteringCoeffs[bandIndex];
        }
    }

    //==============================================================================
    double sampleRate = 44100.0;

    // Rumble filter (high-pass)
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> rumbleCoeffs;
    juce::dsp::IIR::Filter<float> rumbleFilterL, rumbleFilterR;
    float rumbleFreq = 20.0f;
    bool rumbleBypass = true;

    // Hum filter (notch)
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> humCoeffs;
    juce::dsp::IIR::Filter<float> humFilterL, humFilterR;
    float humFreq = 60.0f;
    bool humBypass = true;

    // Graphic EQ (10 bands)
    std::vector<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>> eqCoeffs;
    std::vector<juce::dsp::IIR::Filter<float>> eqBandsL;
    std::vector<juce::dsp::IIR::Filter<float>> eqBandsR;
    std::array<float, 10> eqGains = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Band activity metering for visual feedback
    std::vector<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>> meteringCoeffs;
    std::vector<juce::dsp::IIR::Filter<float>> eqMeteringBandsL;
    std::array<std::atomic<float>, 10> bandActivityLevels;
    std::array<float, 10> smoothedBandLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBank)
};
