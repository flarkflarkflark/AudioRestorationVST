#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/ClickRemoval.h"
#include "DSP/NoiseReduction.h"
#include "DSP/FilterBank.h"

//==============================================================================
/**
 * Audio Restoration Processor
 *
 * Main audio processing class that handles both VST plugin and standalone modes.
 * Implements the complete audio restoration pipeline:
 * - Click and pop removal
 * - Spectral noise reduction
 * - Hum and rumble filtering
 * - Graphic EQ
 */
class AudioRestorationProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioRestorationProcessor();
    ~AudioRestorationProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter management
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // DSP module access for GUI
    ClickRemoval& getClickRemoval() { return clickRemoval; }
    NoiseReduction& getNoiseReduction() { return noiseReduction; }
    FilterBank& getFilterBank() { return filterBank; }

private:
    //==============================================================================
    // Parameter management
    juce::AudioProcessorValueTreeState parameters;

    // DSP modules
    ClickRemoval clickRemoval;
    NoiseReduction noiseReduction;
    FilterBank filterBank;

    // Parameter listeners
    std::atomic<float>* clickSensitivityParam = nullptr;
    std::atomic<float>* noiseReductionParam = nullptr;
    std::atomic<float>* rumbleFilterParam = nullptr;
    std::atomic<float>* humFilterParam = nullptr;

    // Create parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRestorationProcessor)
};
