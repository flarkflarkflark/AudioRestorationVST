#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Vintage Philips/Erres 1960s Audio Equipment Look and Feel
 *
 * Inspired by classic turntables like the Philips GA 312 and Erres SG 1312.
 * Features:
 * - Warm wood grain texture background
 * - Chrome/metallic accents
 * - Art deco styling
 * - Vintage color palette (browns, golds, silver)
 * - Illuminated controls
 */
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel()
    {
        // Vintage color scheme
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff3a2a1a)); // Dark brown wood
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b5545)); // Medium brown
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8d5b7)); // Warm cream
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a1f17));
        setColour (juce::Label::textColourId, juce::Colour (0xffe8d5b7));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe8d5b7));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff2a1f17));
    }

    //==============================================================================
    // Custom background painting
    void drawResizableWindowBackground (juce::Graphics& g, int w, int h,
                                        const juce::BorderSize<int>&,
                                        juce::ResizableWindow&) override
    {
        // Wood grain texture effect
        auto woodColour1 = juce::Colour (0xff3a2a1a); // Dark brown
        auto woodColour2 = juce::Colour (0xff4a3525); // Medium brown
        auto woodColour3 = juce::Colour (0xff2a1a0f); // Very dark

        // Create wood grain pattern with gradients
        for (int y = 0; y < h; y += 20)
        {
            float variation = std::sin (static_cast<float> (y) * 0.05f) * 0.3f + 0.5f;
            auto grainColour = woodColour1.interpolatedWith (woodColour2, variation);

            juce::ColourGradient grain (
                grainColour.darker (0.2f), 0.0f, static_cast<float> (y),
                grainColour.brighter (0.1f), static_cast<float> (w), static_cast<float> (y),
                false);

            g.setGradientFill (grain);
            g.fillRect (0, y, w, 20);
        }

        // Add subtle wood grain lines
        g.setColour (woodColour3.withAlpha (0.3f));
        for (int y = 0; y < h; y += 40)
        {
            int offset = (y / 40) % 3;
            g.drawLine (0.0f, static_cast<float> (y), static_cast<float> (w),
                       static_cast<float> (y), 0.5f + offset * 0.3f);
        }

        // Vignette effect (darker edges)
        juce::ColourGradient vignette (
            juce::Colours::transparentBlack, static_cast<float> (w) / 2.0f, static_cast<float> (h) / 2.0f,
            juce::Colours::black.withAlpha (0.4f), 0.0f, 0.0f,
            true);
        g.setGradientFill (vignette);
        g.fillRect (0, 0, w, h);
    }

    //==============================================================================
    // Vintage chrome GroupComponent
    void drawGroupComponentOutline (juce::Graphics& g, int width, int height,
                                    const juce::String& text, const juce::Justification& position,
                                    juce::GroupComponent& component) override
    {
        auto textH = 20.0f;
        auto indent = 3.0f;
        auto textEdgeGap = 8.0f;

        // Chrome frame with 3D effect
        auto outlineColour = juce::Colour (0xffc0c0c0); // Silver
        auto highlightColour = juce::Colour (0xfff0f0f0); // Bright silver
        auto shadowColour = juce::Colour (0xff606060); // Dark silver

        // Main chrome border
        g.setColour (outlineColour);
        g.drawRoundedRectangle (indent, textH * 0.5f,
                               width - indent * 2.0f, height - textH * 0.5f - indent,
                               6.0f, 2.0f);

        // Top highlight
        g.setColour (highlightColour);
        g.drawRoundedRectangle (indent + 1.0f, textH * 0.5f + 1.0f,
                               width - indent * 2.0f - 2.0f, height - textH * 0.5f - indent - 2.0f,
                               5.0f, 1.0f);

        // Bottom shadow
        g.setColour (shadowColour);
        g.drawLine (indent + 2.0f, height - indent - 1.0f,
                   width - indent - 2.0f, height - indent - 1.0f, 1.5f);

        // Text background (chrome plate)
        auto textWidth = component.getFont().getStringWidthFloat (text) + textEdgeGap * 2.0f;
        auto textX = (width - textWidth) * 0.5f;

        juce::ColourGradient textBg (
            juce::Colour (0xffd0d0d0), textX, 0.0f,
            juce::Colour (0xff909090), textX + textWidth, textH,
            false);

        g.setGradientFill (textBg);
        g.fillRoundedRectangle (textX, 0.0f, textWidth, textH, 3.0f);

        // Text border
        g.setColour (shadowColour);
        g.drawRoundedRectangle (textX, 0.0f, textWidth, textH, 3.0f, 0.8f);

        // Draw text
        g.setColour (juce::Colour (0xff2a1a0f)); // Dark brown text on chrome
        g.setFont (component.getFont());
        g.drawText (text,
                   juce::roundToInt (textX), 0,
                   juce::roundToInt (textWidth), juce::roundToInt (textH),
                   juce::Justification::centred, true);
    }

    //==============================================================================
    // Vintage toggle button (chrome with illumination)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        auto toggleState = button.getToggleState();

        // Background (chrome button)
        juce::ColourGradient bgGradient (
            juce::Colour (0xffb0b0b0), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff707070), bounds.getCentreX(), bounds.getBottom(),
            false);
        g.setGradientFill (bgGradient);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Border
        g.setColour (juce::Colour (0xff404040));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Illuminated when active (like vintage indicator lamps)
        if (toggleState)
        {
            auto lampBounds = bounds.reduced (bounds.getWidth() * 0.3f, bounds.getHeight() * 0.35f);

            // Glow effect
            for (int i = 0; i < 5; ++i)
            {
                float expansion = static_cast<float> (i) * 1.5f;
                float alpha = 0.3f * (1.0f - static_cast<float> (i) / 5.0f);
                g.setColour (juce::Colours::orange.withAlpha (alpha));
                g.fillEllipse (lampBounds.expanded (expansion));
            }

            // Bright core
            g.setColour (juce::Colours::orange.brighter (0.5f));
            g.fillEllipse (lampBounds);
        }

        // Text
        g.setColour (toggleState ? juce::Colours::white : juce::Colour (0xffe8d5b7));
        g.setFont (13.0f);
        g.drawFittedText (button.getButtonText(),
                         button.getLocalBounds().reduced (bounds.getWidth() * 0.2f, 0),
                         juce::Justification::centredRight, 1);
    }

    //==============================================================================
    // Vintage text button (chrome/bakelite style)
    void drawButtonBackground (juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

        // Bakelite/plastic button effect
        auto btnColour = juce::Colour (0xff4a3525); // Brown bakelite
        if (shouldDrawButtonAsDown)
            btnColour = btnColour.darker (0.3f);
        else if (shouldDrawButtonAsHighlighted)
            btnColour = btnColour.brighter (0.2f);

        juce::ColourGradient gradient (
            btnColour.brighter (0.2f), bounds.getCentreX(), bounds.getY(),
            btnColour.darker (0.2f), bounds.getCentreX(), bounds.getBottom(),
            false);

        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Chrome edge highlight
        g.setColour (juce::Colour (0xffa0a0a0).withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.5f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        g.setFont (14.0f);
        g.setColour (juce::Colour (0xffe8d5b7)); // Warm cream text

        auto bounds = button.getLocalBounds();
        g.drawFittedText (button.getButtonText(), bounds,
                         juce::Justification::centred, 1);
    }

    //==============================================================================
    // Vintage combo box
    void drawComboBox (juce::Graphics& g, int width, int height,
                      bool isButtonDown, int buttonX, int buttonY,
                      int buttonW, int buttonH, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (1.0f);

        // Bakelite background
        juce::ColourGradient bg (
            juce::Colour (0xff3a2a1a), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff2a1a0f), bounds.getCentreX(), bounds.getBottom(),
            false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, 3.0f);

        // Chrome border
        g.setColour (juce::Colour (0xff909090));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        // Arrow
        auto arrowZone = juce::Rectangle<int> (buttonX, buttonY, buttonW, buttonH).toFloat();
        juce::Path arrow;
        arrow.addTriangle (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                          arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                          arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);

        g.setColour (juce::Colour (0xffc0c0c0));
        g.fillPath (arrow);
    }

    //==============================================================================
    // Vintage label
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (label.getFont());

            auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                            juce::jmax (1, (int) ((float) textArea.getHeight() / label.getFont().getHeight())),
                            label.getMinimumHorizontalScale());
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageLookAndFeel)
};
