#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Filter Bank Processor
 *
 * Implements:
 * - Rumble filter (high-pass, 20-120 Hz)
 * - Hum filter (notch at 50/60 Hz)
 * - 10-band graphic EQ (31 Hz to 16 kHz)
 */
class FilterBank
{
public:
    FilterBank() = default;

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
        for (int i = 0; i < 10; ++i)
        {
            eqBandsL.emplace_back();
            eqBandsR.emplace_back();
            eqBandsL[i].prepare (monoSpec);
            eqBandsR[i].prepare (monoSpec);
            eqBandsL[i].reset();
            eqBandsR[i].reset();
            eqCoeffs.push_back (nullptr);
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

private:
    //==============================================================================
    void updateFilters()
    {
        updateRumbleFilter();
        updateHumFilter();
        for (int i = 0; i < 10; ++i)
            updateEQBand (i);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBank)
};
