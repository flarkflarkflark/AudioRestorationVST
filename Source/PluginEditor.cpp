#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioRestorationEditor::AudioRestorationEditor (AudioRestorationProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set editor size - fully scalable and resizable
    setSize (950, 800);
    setResizable (true, true);
    setResizeLimits (640, 480, 2560, 1440); // Min 640x480, max supports 1440p displays

    //==============================================================================
    // Global Controls - Difference Mode
    differenceModeButton.setButtonText ("Listen to Difference (Hear what's being removed)");
    differenceModeButton.setColour (juce::ToggleButton::textColourId, juce::Colours::orange);
    addAndMakeVisible (differenceModeButton);

    differenceModeAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "differenceMode", differenceModeButton);

    //==============================================================================
    // Click Removal Section
    clickGroup.setText ("Click & Pop Removal");
    clickGroup.setTextLabelPosition (juce::Justification::centredLeft);
    addAndMakeVisible (clickGroup);

    clickSensitivitySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    clickSensitivitySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (clickSensitivitySlider);

    clickSensitivityLabel.setText ("Sensitivity", juce::dontSendNotification);
    clickSensitivityLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (clickSensitivityLabel);

    clickBypassButton.setButtonText ("Bypass");
    addAndMakeVisible (clickBypassButton);

    clickSensitivityAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "clickSensitivity", clickSensitivitySlider);
    clickBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "clickBypass", clickBypassButton);

    //==============================================================================
    // Noise Reduction Section
    noiseGroup.setText ("Noise Reduction");
    noiseGroup.setTextLabelPosition (juce::Justification::centredLeft);
    addAndMakeVisible (noiseGroup);

    noiseReductionSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    noiseReductionSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (noiseReductionSlider);

    noiseReductionLabel.setText ("Reduction (dB)", juce::dontSendNotification);
    noiseReductionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (noiseReductionLabel);

    noiseBypassButton.setButtonText ("Bypass");
    addAndMakeVisible (noiseBypassButton);

    captureProfileButton.setButtonText ("Capture Noise Profile");
    addAndMakeVisible (captureProfileButton);
    captureProfileButton.onClick = [this]
    {
        // Capture noise profile from current audio
        audioProcessor.getNoiseReduction().captureProfile();
    };

    noiseReductionAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "noiseReduction", noiseReductionSlider);
    noiseBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "noiseBypass", noiseBypassButton);

    //==============================================================================
    // Filter Section
    filterGroup.setText ("Filters");
    filterGroup.setTextLabelPosition (juce::Justification::centredLeft);
    addAndMakeVisible (filterGroup);

    rumbleSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    rumbleSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (rumbleSlider);

    rumbleLabel.setText ("Rumble (Hz)", juce::dontSendNotification);
    rumbleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rumbleLabel);

    rumbleBypassButton.setButtonText ("Bypass");
    addAndMakeVisible (rumbleBypassButton);

    humSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    humSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (humSlider);

    humLabel.setText ("Hum (Hz)", juce::dontSendNotification);
    humLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (humLabel);

    humBypassButton.setButtonText ("Bypass");
    addAndMakeVisible (humBypassButton);

    rumbleAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "rumbleFilter", rumbleSlider);
    rumbleBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "rumbleBypass", rumbleBypassButton);
    humAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "humFilter", humSlider);
    humBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "humBypass", humBypassButton);

    //==============================================================================
    // Graphic EQ Section
    eqGroup.setText ("Graphic EQ");
    eqGroup.setTextLabelPosition (juce::Justification::centredLeft);
    addAndMakeVisible (eqGroup);

    const std::vector<juce::String> eqFreqLabels = {
        "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };

    for (size_t i = 0; i < eqFreqLabels.size(); ++i)
    {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle (juce::Slider::LinearVertical);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        addAndMakeVisible (*slider);

        auto label = std::make_unique<juce::Label>();
        label->setText (eqFreqLabels[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*label);

        auto paramID = "eqBand" + juce::String (i);
        auto attachment = std::make_unique<SliderAttachment> (
            audioProcessor.getParameters(), paramID, *slider);

        eqSliders.push_back (std::move (slider));
        eqLabels.push_back (std::move (label));
        eqAttachments.push_back (std::move (attachment));
    }
}

AudioRestorationEditor::~AudioRestorationEditor()
{
}

//==============================================================================
void AudioRestorationEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("Audio Restoration Suite", getLocalBounds().removeFromTop (40),
                      juce::Justification::centred, 1);
}

void AudioRestorationEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Title area
    area.removeFromTop (40);

    // Difference mode toggle (global control)
    auto diffModeArea = area.removeFromTop (35);
    differenceModeButton.setBounds (diffModeArea.withSizeKeepingCentre (450, 30));

    area.removeFromTop (5); // Small gap

    // Top row: Click removal, Noise reduction, Filters
    auto topRow = area.removeFromTop (200);

    auto clickArea = topRow.removeFromLeft (250).reduced (5);
    clickGroup.setBounds (clickArea);
    auto clickContent = clickArea.reduced (10).withTrimmedTop (20);
    clickSensitivitySlider.setBounds (clickContent.removeFromTop (100).withSizeKeepingCentre (100, 100));
    clickSensitivityLabel.setBounds (clickContent.removeFromTop (20));
    clickBypassButton.setBounds (clickContent.removeFromTop (30).withSizeKeepingCentre (100, 30));

    auto noiseArea = topRow.removeFromLeft (270).reduced (5);
    noiseGroup.setBounds (noiseArea);
    auto noiseContent = noiseArea.reduced (10).withTrimmedTop (20);
    noiseReductionSlider.setBounds (noiseContent.removeFromTop (100).withSizeKeepingCentre (100, 100));
    noiseReductionLabel.setBounds (noiseContent.removeFromTop (20));
    captureProfileButton.setBounds (noiseContent.removeFromTop (30).withSizeKeepingCentre (180, 30));
    noiseBypassButton.setBounds (noiseContent.removeFromTop (30).withSizeKeepingCentre (100, 30));

    auto filterArea = topRow.reduced (5);
    filterGroup.setBounds (filterArea);
    auto filterContent = filterArea.reduced (10).withTrimmedTop (20);

    // Safety check for width
    int halfWidth = filterContent.getWidth() > 0 ? filterContent.getWidth() / 2 : 0;
    auto rumbleArea = filterContent.removeFromLeft (halfWidth).reduced (5);
    rumbleSlider.setBounds (rumbleArea.removeFromTop (100).withSizeKeepingCentre (100, 100));
    rumbleLabel.setBounds (rumbleArea.removeFromTop (20));
    rumbleBypassButton.setBounds (rumbleArea.removeFromTop (30).withSizeKeepingCentre (100, 30));

    auto humArea = filterContent.reduced (5);
    humSlider.setBounds (humArea.removeFromTop (100).withSizeKeepingCentre (100, 100));
    humLabel.setBounds (humArea.removeFromTop (20));
    humBypassButton.setBounds (humArea.removeFromTop (30).withSizeKeepingCentre (100, 30));

    // Bottom row: Graphic EQ - ensure it gets all remaining space
    area.removeFromTop (10);
    auto eqArea = area; // Use all remaining area for EQ
    eqGroup.setBounds (eqArea);
    auto eqContent = eqArea.reduced (10).withTrimmedTop (25);

    // Safety check to prevent division by zero
    if (!eqSliders.empty() && eqContent.getWidth() > 0 && eqContent.getHeight() > 0)
    {
        int sliderWidth = eqContent.getWidth() / static_cast<int> (eqSliders.size());
        if (sliderWidth > 0)
        {
            for (size_t i = 0; i < eqSliders.size(); ++i)
            {
                auto sliderArea = eqContent.removeFromLeft (sliderWidth).reduced (3);
                eqLabels[i]->setBounds (sliderArea.removeFromBottom (25));
                eqSliders[i]->setBounds (sliderArea);
            }
        }
    }
}
