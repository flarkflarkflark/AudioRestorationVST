#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioRestorationEditor::AudioRestorationEditor (AudioRestorationProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Load logo from embedded PNG data
    #include "Resources/VRSLogoData.h"
    logoImage = juce::ImageFileFormat::loadFrom (VRSlogo_png, VRSlogo_png_len);

    // Set editor size - fully scalable and resizable
    setSize (baseWidth, baseHeight);
    setResizable (true, true);
    setResizeLimits (500, 400, 2560, 1440); // Min 500x400, max supports 1440p displays

    //==============================================================================
    // UI Scale Selector
    scaleLabel.setText ("UI Scale:", juce::dontSendNotification);
    scaleLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (scaleLabel);

    scaleSelector.addItem ("25%", 1);
    scaleSelector.addItem ("50%", 2);
    scaleSelector.addItem ("75%", 3);
    scaleSelector.addItem ("100%", 4);
    scaleSelector.addItem ("125%", 5);
    scaleSelector.addItem ("150%", 6);
    scaleSelector.addItem ("200%", 7);
    scaleSelector.addItem ("300%", 8);
    scaleSelector.addItem ("400%", 9);
    scaleSelector.setSelectedId (4); // Default 100%
    addAndMakeVisible (scaleSelector);

    scaleSelector.onChange = [this]
    {
        const float scaleValues[] = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f};
        int selectedIndex = scaleSelector.getSelectedId() - 1;
        if (selectedIndex >= 0 && selectedIndex < 9)
        {
            setScaleFactor (scaleValues[selectedIndex]);
        }
    };

    scaleAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getParameters(), "uiScale", scaleSelector);

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
    clickBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (clickBypassButton);

    clickSensitivityAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "clickSensitivity", clickSensitivitySlider);
    clickBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "clickBypass", clickBypassButton);

    // Setup glowing knob for Click Removal (orange/red glow)
    clickKnobLAF.setGlowColour (juce::Colours::orange);
    clickSensitivitySlider.setLookAndFeel (&clickKnobLAF);

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
    noiseBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
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

    // Setup glowing knob for Noise Reduction (cyan/blue glow)
    noiseKnobLAF.setGlowColour (juce::Colours::cyan);
    noiseReductionSlider.setLookAndFeel (&noiseKnobLAF);

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
    rumbleBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (rumbleBypassButton);

    humSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    humSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (humSlider);

    humLabel.setText ("Hum (Hz)", juce::dontSendNotification);
    humLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (humLabel);

    humBypassButton.setButtonText ("Bypass");
    humBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (humBypassButton);

    rumbleAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "rumbleFilter", rumbleSlider);
    rumbleBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "rumbleBypass", rumbleBypassButton);

    // Setup glowing knob for Rumble Filter (purple glow)
    rumbleKnobLAF.setGlowColour (juce::Colours::purple);
    rumbleSlider.setLookAndFeel (&rumbleKnobLAF);

    humAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.getParameters(), "humFilter", humSlider);
    humBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "humBypass", humBypassButton);

    // Setup glowing knob for Hum Filter (magenta glow)
    humKnobLAF.setGlowColour (juce::Colours::magenta);
    humSlider.setLookAndFeel (&humKnobLAF);

    //==============================================================================
    // Graphic EQ Section
    eqGroup.setText ("Graphic EQ");
    eqGroup.setTextLabelPosition (juce::Justification::centredLeft);
    addAndMakeVisible (eqGroup);

    // Spectrum Analyzer removed per user request
    // addAndMakeVisible (spectrumAnalyzer);
    // spectrumAnalyzer.toBack();

    eqBypassButton.setButtonText ("Bypass");
    eqBypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (eqBypassButton);

    eqBypassAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getParameters(), "eqBypass", eqBypassButton);

    const std::vector<juce::String> eqFreqLabels = {
        "31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"
    };

    // Setup glowing EQ sliders (vertical with round chrome thumbs)
    for (size_t i = 0; i < eqFreqLabels.size(); ++i)
    {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle (juce::Slider::LinearVertical);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*slider);

        // Create custom LookAndFeel for each EQ band with DISTINCT multicolor spectrum
        // Each band gets its own very distinct color for maximum visibility
        auto laf = std::make_unique<GlowingKnobLookAndFeel>();

        // DRASTIC color differences between bands (no smooth gradient)
        // 10 distinct colors across spectrum for dramatic visual separation
        const std::array<float, 10> hueValues = {
            0.0f,   // 31 Hz  - Pure Red
            0.05f,  // 62 Hz  - Red-Orange
            0.12f,  // 125 Hz - Orange
            0.16f,  // 250 Hz - Orange-Yellow
            0.22f,  // 500 Hz - Yellow
            0.33f,  // 1k Hz  - Green
            0.45f,  // 2k Hz  - Cyan-Green
            0.52f,  // 4k Hz  - Cyan
            0.58f,  // 8k Hz  - Cyan-Blue
            0.65f   // 16k Hz - Blue
        };

        laf->setGlowColour (juce::Colour::fromHSV (hueValues[i], 0.95f, 1.0f, 1.0f)); // Full saturation & brightness
        eqKnobLAFs.push_back (std::move (laf));

        auto label = std::make_unique<juce::Label>();
        label->setText (eqFreqLabels[i], juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*label);

        auto paramID = "eqBand" + juce::String (i);
        auto attachment = std::make_unique<SliderAttachment> (
            audioProcessor.getParameters(), paramID, *slider);

        // Apply vintage LookAndFeel to EQ sliders
        slider->setLookAndFeel (eqKnobLAFs[i].get());

        eqSliders.push_back (std::move (slider));
        eqLabels.push_back (std::move (label));
        eqAttachments.push_back (std::move (attachment));
    }

    // Start timer for visual feedback updates (20 Hz)
    startTimer (50);
}

AudioRestorationEditor::~AudioRestorationEditor()
{
    stopTimer();

    // Reset LookAndFeel to avoid dangling pointers
    clickSensitivitySlider.setLookAndFeel (nullptr);
    noiseReductionSlider.setLookAndFeel (nullptr);
    rumbleSlider.setLookAndFeel (nullptr);
    humSlider.setLookAndFeel (nullptr);

    for (auto& slider : eqSliders)
    {
        if (slider)
            slider->setLookAndFeel (nullptr);
    }
}

//==============================================================================
void AudioRestorationEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto titleArea = getLocalBounds().removeFromTop (40);

    // Draw logo on the left
    auto leftArea = titleArea;
    if (logoImage.isValid())
    {
        int logoHeight = 30;
        float aspectRatio = (float) logoImage.getWidth() / (float) logoImage.getHeight();
        int logoWidth = (int) (logoHeight * aspectRatio);

        // Calculate vertical center position for alignment
        int topMargin = (titleArea.getHeight() - logoHeight) / 2;

        // Draw logo at calculated vertical position
        juce::Rectangle<float> logoRect (10.0f, (float)topMargin, (float)logoWidth, (float)logoHeight);
        g.drawImage (logoImage, logoRect, juce::RectanglePlacement::centred);
    }

    // Draw title with version in the center
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    juce::String titleText = "Vinyl Restoration Suite v" + juce::String (JucePlugin_VersionString);
    g.drawFittedText (titleText, titleArea, juce::Justification::centred, 1);
}

void AudioRestorationEditor::timerCallback()
{
    // Update Click Removal glow intensity based on activity
    bool clickBypassed = *audioProcessor.getParameters().getRawParameterValue ("clickBypass") > 0.5f;
    float clickSensitivity = *audioProcessor.getParameters().getRawParameterValue ("clickSensitivity");

    if (!clickBypassed && clickSensitivity > 0.0f)
    {
        float clickRate = audioProcessor.getClickRemoval().getClickRate();
        // Map click rate to glow intensity (0-10 clicks/sec -> 0.0-1.0)
        float intensity = juce::jlimit (0.0f, 1.0f, clickRate / 10.0f);

        // Smooth interpolation
        float currentIntensity = clickKnobLAF.getGlowIntensity();
        float smoothed = currentIntensity + (intensity - currentIntensity) * 0.2f;
        clickKnobLAF.setGlowIntensity (smoothed);
    }
    else
    {
        clickKnobLAF.setGlowIntensity (0.0f);
    }
    clickSensitivitySlider.repaint();

    // Update Noise Reduction glow intensity
    bool noiseBypassed = *audioProcessor.getParameters().getRawParameterValue ("noiseBypass") > 0.5f;
    float noiseReduction = *audioProcessor.getParameters().getRawParameterValue ("noiseReduction");

    if (!noiseBypassed && audioProcessor.getNoiseReduction().isActivelyReducing())
    {
        float reduction = audioProcessor.getNoiseReduction().getReductionAmount();
        // Map reduction amount (0-24 dB) to glow intensity
        float targetIntensity = juce::jlimit (0.0f, 1.0f, reduction / 24.0f);

        // Smooth interpolation
        float currentIntensity = noiseKnobLAF.getGlowIntensity();
        float smoothed = currentIntensity + (targetIntensity - currentIntensity) * 0.15f;
        noiseKnobLAF.setGlowIntensity (smoothed);
    }
    else if (!noiseBypassed && audioProcessor.getNoiseReduction().hasProfile())
    {
        // Profile captured but not actively reducing - dim glow
        noiseKnobLAF.setGlowIntensity (0.3f);
    }
    else
    {
        // Smooth fade out
        float currentIntensity = noiseKnobLAF.getGlowIntensity();
        noiseKnobLAF.setGlowIntensity (currentIntensity * 0.9f);
    }
    noiseReductionSlider.repaint();

    // Update Rumble Filter glow intensity
    bool rumbleBypassed = *audioProcessor.getParameters().getRawParameterValue ("rumbleBypass") > 0.5f;
    if (!rumbleBypassed)
    {
        // Rumble filter cutoff frequency affects intensity (higher cutoff = more filtering = brighter)
        float cutoff = *audioProcessor.getParameters().getRawParameterValue ("rumbleFilter");
        // Map cutoff (5-150 Hz) to glow intensity
        float targetIntensity = juce::jlimit (0.0f, 1.0f, (cutoff - 5.0f) / 145.0f);

        // Smooth interpolation
        float currentIntensity = rumbleKnobLAF.getGlowIntensity();
        float smoothed = currentIntensity + (targetIntensity - currentIntensity) * 0.15f;
        rumbleKnobLAF.setGlowIntensity (smoothed);
    }
    else
    {
        // Smooth fade out
        float currentIntensity = rumbleKnobLAF.getGlowIntensity();
        rumbleKnobLAF.setGlowIntensity (currentIntensity * 0.9f);
    }
    rumbleSlider.repaint();

    // Update Hum Filter glow intensity
    bool humBypassed = *audioProcessor.getParameters().getRawParameterValue ("humBypass") > 0.5f;
    if (!humBypassed)
    {
        // Hum filter is notch at 50/60 Hz - show strong consistent intensity when active
        float targetIntensity = 0.8f;

        // Smooth interpolation
        float currentIntensity = humKnobLAF.getGlowIntensity();
        float smoothed = currentIntensity + (targetIntensity - currentIntensity) * 0.15f;
        humKnobLAF.setGlowIntensity (smoothed);
    }
    else
    {
        // Smooth fade out
        float currentIntensity = humKnobLAF.getGlowIntensity();
        humKnobLAF.setGlowIntensity (currentIntensity * 0.9f);
    }
    humSlider.repaint();

    // Update EQ band glow intensities based on REALTIME frequency content
    // Show activity ALWAYS (even when bypassed) so you can see what's in the signal
    bool eqBypassed = *audioProcessor.getParameters().getRawParameterValue ("eqBypass") > 0.5f;

    for (size_t i = 0; i < eqKnobLAFs.size() && i < eqSliders.size(); ++i)
    {
        // Get actual frequency band activity level from FilterBank (0.0-1.0)
        float bandActivity = audioProcessor.getFilterBank().getBandActivityLevel (static_cast<int> (i));

        float targetIntensity = 0.0f;

        if (!eqBypassed)
        {
            // When EQ is active: combine band activity with gain setting
            auto paramID = "eqBand" + juce::String (static_cast<int> (i));
            float gain = *audioProcessor.getParameters().getRawParameterValue (paramID);
            float gainFactor = std::abs (gain) / 12.0f; // 0.0 at 0dB, 1.0 at ±12dB

            // DRAMATIC glow: pure band activity, amplified by gain
            // Even at 0 dB, show full activity level
            targetIntensity = bandActivity * (0.7f + gainFactor * 0.3f); // 70%-100% range

            // Apply non-linear curve for more dramatic effect
            targetIntensity = std::pow (targetIntensity, 0.7f); // Gamma curve for brighter mids
        }
        else
        {
            // When bypassed: show band activity at good visibility
            // Still want to see what's in the signal clearly
            targetIntensity = bandActivity * 0.75f; // Up to 75% when bypassed
            targetIntensity = std::pow (targetIntensity, 0.8f); // Slight gamma for visibility
        }

        targetIntensity = juce::jlimit (0.0f, 1.0f, targetIntensity);

        // FAST interpolation for realtime response
        float currentIntensity = eqKnobLAFs[i]->getGlowIntensity();
        float smoothed = currentIntensity + (targetIntensity - currentIntensity) * 0.35f; // 35% per frame = very responsive
        eqKnobLAFs[i]->setGlowIntensity (smoothed);

        eqSliders[i]->repaint();
    }

    // Spectrum analyzer removed per user request
    // const auto& vizBuffer = audioProcessor.getVisualizationBuffer();
    // if (vizBuffer.getNumSamples() > 0)
    // {
    //     spectrumAnalyzer.pushAudioSamples (vizBuffer, vizBuffer.getNumSamples());
    // }
    // spectrumAnalyzer.updateSpectrum();
}

void AudioRestorationEditor::setScaleFactor (float newScale)
{
    if (newScale == scaleFactor)
        return;

    scaleFactor = juce::jlimit (0.25f, 4.0f, newScale);

    // Apply scale transform to the entire editor
    setTransform (juce::AffineTransform::scale (scaleFactor));

    // Update the actual window size based on the scaled content
    int scaledWidth = juce::roundToInt (baseWidth * scaleFactor);
    int scaledHeight = juce::roundToInt (baseHeight * scaleFactor);

    // Update size limits based on scale
    setResizeLimits (
        juce::roundToInt (500 * scaleFactor),
        juce::roundToInt (400 * scaleFactor),
        juce::roundToInt (2560 * scaleFactor),
        juce::roundToInt (1440 * scaleFactor));

    // Resize to maintain proportions
    setSize (scaledWidth, scaledHeight);

    // Notify parent window of size change
    if (auto* peer = getPeer())
        peer->handleMovedOrResized();
}

void AudioRestorationEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Title area with scale selector on the right
    auto titleArea = area.removeFromTop (40);
    auto scaleArea = titleArea.removeFromRight (180);
    scaleLabel.setBounds (scaleArea.removeFromLeft (70));
    scaleSelector.setBounds (scaleArea.removeFromLeft (100).reduced (0, 5));

    // Difference mode toggle (global control)
    auto diffModeArea = area.removeFromTop (35);
    differenceModeButton.setBounds (diffModeArea.withSizeKeepingCentre (450, 30));

    area.removeFromTop (5); // Small gap

    // Top row: Click removal, Noise reduction, Filters (increased height for better proportions)
    // All sections now scale proportionally with window width
    auto topRow = area.removeFromTop (240);
    int totalWidth = topRow.getWidth();

    // Calculate proportional widths: Click (27%), Noise (29%), Filters (44%)
    int clickWidth = totalWidth * 27 / 100;
    int noiseWidth = totalWidth * 29 / 100;
    // Filter gets the remainder

    auto clickArea = topRow.removeFromLeft (clickWidth).reduced (5);
    clickGroup.setBounds (clickArea);

    auto clickContent = clickArea.reduced (10).withTrimmedTop (20);

    // Place bypass button in top-right corner (inside group bounds)
    auto clickBypassArea = clickContent.removeFromTop (25).removeFromRight (90).reduced (3, 0);
    clickBypassButton.setBounds (clickBypassArea);

    clickContent.removeFromTop (5); // Small gap after bypass
    clickSensitivitySlider.setBounds (clickContent.removeFromTop (100).withSizeKeepingCentre (100, 100));
    clickSensitivityLabel.setBounds (clickContent.removeFromTop (20));

    auto noiseArea = topRow.removeFromLeft (noiseWidth).reduced (5);
    noiseGroup.setBounds (noiseArea);

    auto noiseContent = noiseArea.reduced (10).withTrimmedTop (20);

    // Place bypass button in top-right corner (inside group bounds)
    auto noiseBypassArea = noiseContent.removeFromTop (25).removeFromRight (90).reduced (3, 0);
    noiseBypassButton.setBounds (noiseBypassArea);

    noiseContent.removeFromTop (5); // Small gap after bypass
    noiseReductionSlider.setBounds (noiseContent.removeFromTop (100).withSizeKeepingCentre (100, 100));
    noiseReductionLabel.setBounds (noiseContent.removeFromTop (20));
    noiseContent.removeFromTop (10);
    captureProfileButton.setBounds (noiseContent.removeFromTop (30).withSizeKeepingCentre (180, 30));

    auto filterArea = topRow.reduced (5);
    filterGroup.setBounds (filterArea);
    auto filterContent = filterArea.reduced (10).withTrimmedTop (20);

    // Safety check for width
    int halfWidth = filterContent.getWidth() > 0 ? filterContent.getWidth() / 2 : 0;
    auto rumbleArea = filterContent.removeFromLeft (halfWidth).reduced (5);

    // Place rumble bypass in top-right of its area (aligned with other bypasses)
    auto rumbleBypassArea = rumbleArea.removeFromTop (25).removeFromRight (80).reduced (2, 0);
    rumbleBypassButton.setBounds (rumbleBypassArea);

    rumbleArea.removeFromTop (5); // Small gap after bypass
    rumbleSlider.setBounds (rumbleArea.removeFromTop (100).withSizeKeepingCentre (100, 100));
    rumbleLabel.setBounds (rumbleArea.removeFromTop (20));

    auto humArea = filterContent.reduced (5);

    // Place hum bypass in top-right of its area (aligned with other bypasses)
    auto humBypassArea = humArea.removeFromTop (25).removeFromRight (80).reduced (2, 0);
    humBypassButton.setBounds (humBypassArea);

    humArea.removeFromTop (5); // Small gap after bypass
    humSlider.setBounds (humArea.removeFromTop (100).withSizeKeepingCentre (100, 100));
    humLabel.setBounds (humArea.removeFromTop (20));

    // Bottom row: Graphic EQ - more compact size for better proportions
    area.removeFromTop (10);
    auto eqArea = area; // Use all remaining area for EQ
    eqGroup.setBounds (eqArea);

    auto eqContent = eqArea.reduced (10).withTrimmedTop (20);

    // Place EQ bypass button in top-right corner (inside group bounds)
    auto eqBypassArea = eqContent.removeFromTop (25).removeFromRight (90).reduced (3, 0);
    eqBypassButton.setBounds (eqBypassArea);

    eqContent.removeFromTop (5); // Small gap after bypass

    // Spectrum analyzer removed per user request
    // auto spectrumArea = eqContent;
    // spectrumArea.removeFromBottom (25);
    // spectrumAnalyzer.setBounds (spectrumArea);

    // Safety check to prevent division by zero
    if (!eqSliders.empty() && eqContent.getWidth() > 0 && eqContent.getHeight() > 0)
    {
        int sliderWidth = eqContent.getWidth() / static_cast<int> (eqSliders.size());
        if (sliderWidth > 0)
        {
            for (size_t i = 0; i < eqSliders.size(); ++i)
            {
                auto sliderArea = eqContent.removeFromLeft (sliderWidth).reduced (3);

                // Layout: Slider (top), Label (bottom)
                auto labelArea = sliderArea.removeFromBottom (25);

                // Position slider with round thumb (takes all remaining space)
                eqSliders[i]->setBounds (sliderArea);

                // Position label
                eqLabels[i]->setBounds (labelArea);
            }
        }
    }
}
