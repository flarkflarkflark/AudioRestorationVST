#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioRestorationProcessor::AudioRestorationProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
      parameters (*this, nullptr, juce::Identifier ("AudioRestoration"), createParameterLayout())
{
    // Get parameter pointers for fast access in processBlock
    clickSensitivityParam = parameters.getRawParameterValue ("clickSensitivity");
    noiseReductionParam = parameters.getRawParameterValue ("noiseReduction");
    rumbleFilterParam = parameters.getRawParameterValue ("rumbleFilter");
    humFilterParam = parameters.getRawParameterValue ("humFilter");
}

AudioRestorationProcessor::~AudioRestorationProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioRestorationProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Global parameters
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "uiScale",
        "UI Scale",
        juce::StringArray {"25%", "50%", "75%", "100%", "125%", "150%", "200%", "300%", "400%"},
        3)); // Default to 100% (index 3)

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "differenceMode",
        "Difference Mode",
        false));

    // Click removal parameters
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "clickSensitivity",
        "Click Sensitivity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        50.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "clickBypass",
        "Click Bypass",
        false));

    // Noise reduction parameters
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "noiseReduction",
        "Noise Reduction",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),
        0.0f,
        "dB"));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "noiseBypass",
        "Noise Bypass",
        false));

    // Filter parameters
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "rumbleFilter",
        "Rumble Filter",
        juce::NormalisableRange<float> (5.0f, 150.0f, 0.1f),
        20.0f,
        "Hz"));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "rumbleBypass",
        "Rumble Bypass",
        true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "humFilter",
        "Hum Filter",
        juce::NormalisableRange<float> (40.0f, 80.0f, 0.1f),
        60.0f,
        "Hz"));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "humBypass",
        "Hum Bypass",
        true));

    // Graphic EQ bypass
    layout.add (std::make_unique<juce::AudioParameterBool> (
        "eqBypass",
        "EQ Bypass",
        true));

    // Graphic EQ bands (10 bands)
    const std::vector<float> eqFreqs = {31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
                                         1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};

    for (size_t i = 0; i < eqFreqs.size(); ++i)
    {
        auto paramID = "eqBand" + juce::String (i);
        auto paramName = juce::String (eqFreqs[i]) + " Hz";

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            paramID,
            paramName,
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f),
            0.0f,
            "dB"));
    }

    return layout;
}

//==============================================================================
const juce::String AudioRestorationProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioRestorationProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioRestorationProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioRestorationProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioRestorationProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioRestorationProcessor::getNumPrograms()
{
    return 1;
}

int AudioRestorationProcessor::getCurrentProgram()
{
    return 0;
}

void AudioRestorationProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioRestorationProcessor::getProgramName (int index)
{
    return {};
}

void AudioRestorationProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioRestorationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    // Prepare DSP modules
    clickRemoval.prepare (spec);
    noiseReduction.prepare (spec);
    filterBank.prepare (spec);
}

void AudioRestorationProcessor::releaseResources()
{
    clickRemoval.reset();
    noiseReduction.reset();
    filterBank.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioRestorationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Support mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output layouts must match
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AudioRestorationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Store original audio for difference mode
    bool differenceModeEnabled = *parameters.getRawParameterValue ("differenceMode") > 0.5f;
    juce::AudioBuffer<float> originalBuffer;
    if (differenceModeEnabled)
    {
        originalBuffer.makeCopyOf (buffer);
    }

    // Create audio block for DSP processing
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // Processing chain:
    // 1. Click removal (if enabled)
    if (!*parameters.getRawParameterValue ("clickBypass"))
    {
        clickRemoval.setSensitivity (*clickSensitivityParam);
        clickRemoval.process (context);
    }

    // 2. Spectral noise reduction (if enabled)
    if (!*parameters.getRawParameterValue ("noiseBypass"))
    {
        noiseReduction.setReduction (*noiseReductionParam);
        noiseReduction.process (context);
    }

    // 3. Filter bank (rumble, hum, EQ)
    bool rumbleBypass = *parameters.getRawParameterValue ("rumbleBypass") > 0.5f;
    bool humBypass = *parameters.getRawParameterValue ("humBypass") > 0.5f;
    bool eqBypass = *parameters.getRawParameterValue ("eqBypass") > 0.5f;

    filterBank.setRumbleFilter (*rumbleFilterParam, rumbleBypass);
    filterBank.setHumFilter (*humFilterParam, humBypass);

    // Update EQ bands and check if any are active (only if EQ not bypassed)
    bool anyEQActive = false;
    if (!eqBypass)
    {
        for (int i = 0; i < 10; ++i)
        {
            auto paramID = "eqBand" + juce::String (i);
            float gain = *parameters.getRawParameterValue (paramID);
            filterBank.setEQBand (i, gain);
            if (std::abs (gain) > 0.01f)
                anyEQActive = true;
        }
    }

    // ALWAYS measure band activity for visual feedback (regardless of bypass state)
    filterBank.measureBandActivityForMetering (block);

    // Only process filterbank if something is enabled
    if (!rumbleBypass || !humBypass || anyEQActive)
    {
        filterBank.process (context);
    }

    // Difference mode: output what was removed (original - processed)
    if (differenceModeEnabled)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* processedData = buffer.getWritePointer (channel);
            const auto* originalData = originalBuffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                processedData[sample] = originalData[sample] - processedData[sample];
            }
        }
    }

    // Store a copy of the processed audio for spectrum analyzer visualization
    visualizationBuffer.makeCopyOf (buffer);
}

//==============================================================================
bool AudioRestorationProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioRestorationProcessor::createEditor()
{
    return new AudioRestorationEditor (*this);
}

//==============================================================================
void AudioRestorationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioRestorationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioRestorationProcessor();
}
