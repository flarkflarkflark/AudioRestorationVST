#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GUI/GlowingKnobLookAndFeel.h"
// #include "GUI/SpectrumAnalyzer.h"  // Removed per user request

//==============================================================================
/**
 * Audio Restoration Editor
 *
 * Main GUI for both VST and Standalone modes.
 * Features:
 * - Parameter controls for all DSP modules
 * - Waveform display with correction overlay (standalone)
 * - Correction list view (standalone)
 * - Real-time spectrum analyzer
 */
class AudioRestorationEditor  : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    AudioRestorationEditor (AudioRestorationProcessor&);
    ~AudioRestorationEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    // Interface Scaling
    void setScaleFactor (float newScale);
    float getScaleFactor() const { return scaleFactor; }

    //==============================================================================
    // Visual Feedback Timer
    void timerCallback() override;

private:
    AudioRestorationProcessor& audioProcessor;

    // Interface scaling
    float scaleFactor = 1.0f;
    static constexpr int baseWidth = 950;
    static constexpr int baseHeight = 800;

    //==============================================================================
    // UI Components

    // Global Controls
    juce::ComboBox scaleSelector;
    juce::Label scaleLabel;
    juce::ToggleButton differenceModeButton;
    juce::Label differenceModeLabel;

    // Click Removal Section
    juce::GroupComponent clickGroup;
    juce::Slider clickSensitivitySlider;
    juce::Label clickSensitivityLabel;
    juce::ToggleButton clickBypassButton;

    // Noise Reduction Section
    juce::GroupComponent noiseGroup;
    juce::Slider noiseReductionSlider;
    juce::Label noiseReductionLabel;
    juce::ToggleButton noiseBypassButton;
    juce::TextButton captureProfileButton;

    // Filter Section
    juce::GroupComponent filterGroup;
    juce::Slider rumbleSlider;
    juce::Label rumbleLabel;
    juce::ToggleButton rumbleBypassButton;
    juce::Slider humSlider;
    juce::Label humLabel;
    juce::ToggleButton humBypassButton;

    // Graphic EQ Section
    juce::GroupComponent eqGroup;
    juce::ToggleButton eqBypassButton;
    std::vector<std::unique_ptr<juce::Slider>> eqSliders;
    std::vector<std::unique_ptr<juce::Label>> eqLabels;

    // Parameter attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<ButtonAttachment> differenceModeAttachment;
    std::unique_ptr<SliderAttachment> clickSensitivityAttachment;
    std::unique_ptr<ButtonAttachment> clickBypassAttachment;
    std::unique_ptr<SliderAttachment> noiseReductionAttachment;
    std::unique_ptr<ButtonAttachment> noiseBypassAttachment;
    std::unique_ptr<SliderAttachment> rumbleAttachment;
    std::unique_ptr<ButtonAttachment> rumbleBypassAttachment;
    std::unique_ptr<SliderAttachment> humAttachment;
    std::unique_ptr<ButtonAttachment> humBypassAttachment;

    std::unique_ptr<ButtonAttachment> eqBypassAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> eqAttachments;

    // Custom LookAndFeel for glowing knobs
    GlowingKnobLookAndFeel clickKnobLAF;
    GlowingKnobLookAndFeel noiseKnobLAF;
    GlowingKnobLookAndFeel rumbleKnobLAF;
    GlowingKnobLookAndFeel humKnobLAF;
    std::vector<std::unique_ptr<GlowingKnobLookAndFeel>> eqKnobLAFs;

    // Spectrum Analyzer removed per user request
    // SpectrumAnalyzer spectrumAnalyzer;

    // Logo image
    juce::Image logoImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRestorationEditor)
};
