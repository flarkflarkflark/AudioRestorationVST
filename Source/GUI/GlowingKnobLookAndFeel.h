#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Custom LookAndFeel for Glowing Activity Knobs
 *
 * Creates rotary sliders with an inner glow effect that radiates
 * from center to edge when the associated DSP module is active.
 * The glow intensity reflects real-time processing activity.
 */
class GlowingKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlowingKnobLookAndFeel()
    {
        // Default glow color (can be customized per knob)
        setGlowColour (juce::Colours::cyan);
    }

    /** Set the glow color for this knob */
    void setGlowColour (juce::Colour colour)
    {
        glowColour = colour;
    }

    /** Set the glow intensity (0.0 = off, 1.0 = full) */
    void setGlowIntensity (float intensity)
    {
        glowIntensity = juce::jlimit (0.0f, 1.0f, intensity);
    }

    /** Get current glow intensity */
    float getGlowIntensity() const { return glowIntensity; }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto centerX = bounds.getCentreX();
        auto centerY = bounds.getCentreY();

        // === PHILIPS GA 312 VINTAGE STYLE ===

        // 1. Outer chrome ring (metallic effect with gradient)
        juce::ColourGradient chromeGradient (
            juce::Colour (0xffe0e0e0), centerX, centerY - radius,  // Light silver top
            juce::Colour (0xff808080), centerX, centerY + radius,  // Darker bottom
            false);
        chromeGradient.addColour (0.3, juce::Colour (0xfff5f5f5)); // Highlight
        chromeGradient.addColour (0.7, juce::Colour (0xff606060)); // Shadow
        g.setGradientFill (chromeGradient);
        g.fillEllipse (centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);

        // 2. Outer edge shadow for 3D depth
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawEllipse (centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // 3. Inner black/dark area
        auto innerRadius = radius * 0.75f;
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillEllipse (centerX - innerRadius, centerY - innerRadius,
                      innerRadius * 2.0f, innerRadius * 2.0f);

        // 4. Inner glow ring (translucent - vintage style)
        if (glowIntensity > 0.01f)
        {
            // Create smooth radial glow from center outward
            auto glowRadius = innerRadius * 0.85f;

            for (int i = 0; i < 16; ++i)
            {
                float layerProgress = static_cast<float> (i) / 16.0f;
                float currentRadius = glowRadius * (0.3f + layerProgress * 0.7f);
                float alpha = glowIntensity * (1.0f - layerProgress) * 0.2f;

                g.setColour (glowColour.withAlpha (alpha));
                g.fillEllipse (centerX - currentRadius, centerY - currentRadius,
                              currentRadius * 2.0f, currentRadius * 2.0f);
            }

            // Bright inner glow core
            auto coreRadius = glowRadius * 0.4f;
            g.setColour (glowColour.withAlpha (glowIntensity * 0.6f));
            g.fillEllipse (centerX - coreRadius, centerY - coreRadius,
                          coreRadius * 2.0f, coreRadius * 2.0f);
        }

        // 5. Subtle inner ring highlight (vintage translucent effect)
        auto ringRadius = innerRadius * 0.9f;
        juce::ColourGradient ringGradient (
            juce::Colours::white.withAlpha (0.1f), centerX, centerY - ringRadius,
            juce::Colours::transparentBlack, centerX, centerY + ringRadius,
            false);
        g.setGradientFill (ringGradient);
        g.drawEllipse (centerX - ringRadius, centerY - ringRadius,
                      ringRadius * 2.0f, ringRadius * 2.0f, 2.0f);

        // 6. White pointer line (Philips style - thin and precise)
        juce::Path pointer;
        auto pointerLength = innerRadius * 0.7f;
        auto pointerWidth = 2.5f;

        // Create pointer from center to edge
        pointer.startNewSubPath (0, -pointerLength * 0.3f);
        pointer.lineTo (0, -pointerLength);

        // Apply rotation and translation
        pointer.applyTransform (juce::AffineTransform::rotation (toAngle)
                                                       .translated (centerX, centerY));

        // Draw pointer with glow when active
        if (glowIntensity > 0.4f)
        {
            // Glow halo around pointer
            g.setColour (glowColour.withAlpha (glowIntensity * 0.5f));
            g.strokePath (pointer, juce::PathStrokeType (pointerWidth + 2.0f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }

        // Main pointer (white)
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (pointer, juce::PathStrokeType (pointerWidth,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // 7. Center dot (vintage style)
        auto dotRadius = 3.0f;

        // Chrome center dot
        juce::ColourGradient dotGradient (
            juce::Colour (0xfff0f0f0), centerX - dotRadius * 0.5f, centerY - dotRadius * 0.5f,
            juce::Colour (0xffa0a0a0), centerX + dotRadius * 0.5f, centerY + dotRadius * 0.5f,
            true);
        g.setGradientFill (dotGradient);
        g.fillEllipse (centerX - dotRadius, centerY - dotRadius,
                      dotRadius * 2.0f, dotRadius * 2.0f);

        // Highlight on center dot
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillEllipse (centerX - dotRadius * 0.5f, centerY - dotRadius * 0.7f,
                      dotRadius, dotRadius * 0.8f);
    }

    //==============================================================================
    // Vintage Philips-style linear slider (for EQ)
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackWidth = 16.0f; // DOUBLED width for more dramatic glow visibility
            auto thumbWidth = width * 0.8f;
            auto thumbHeight = 16.0f;

            // Center the track
            auto trackX = x + width * 0.5f - trackWidth * 0.5f;

            // 1. Draw chrome track/rail
            juce::ColourGradient trackGradient (
                juce::Colour (0xffa0a0a0), trackX, static_cast<float> (y),
                juce::Colour (0xff505050), trackX + trackWidth, static_cast<float> (y),
                false);
            trackGradient.addColour (0.5, juce::Colour (0xffd0d0d0));

            g.setGradientFill (trackGradient);
            g.fillRoundedRectangle (trackX, static_cast<float> (y), trackWidth,
                                   static_cast<float> (height), 3.0f);

            // Track borders
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawRoundedRectangle (trackX, static_cast<float> (y), trackWidth,
                                   static_cast<float> (height), 3.0f, 1.0f);

            // 2. Center line (0 dB indicator)
            auto centerY = y + height * 0.5f;
            g.setColour (juce::Colour (0xff808080));
            g.drawLine (trackX - 4.0f, centerY, trackX + trackWidth + 4.0f, centerY, 2.0f);

            // 3. Glow indicator on track (when not at center)
            auto sliderY = sliderPos;
            auto distanceFromCenter = std::abs (sliderY - centerY);
            if (distanceFromCenter > 2.0f && glowIntensity > 0.01f)
            {
                // Draw glow along track from center to thumb position
                auto glowStart = juce::jmin (centerY, sliderY);
                auto glowEnd = juce::jmax (centerY, sliderY);
                auto glowHeight = glowEnd - glowStart;

                // Multiple glow layers
                for (int i = 0; i < 8; ++i)
                {
                    float expansion = static_cast<float> (i) * 1.5f;
                    float alpha = glowIntensity * (1.0f - static_cast<float> (i) / 8.0f) * 0.15f;

                    g.setColour (glowColour.withAlpha (alpha));
                    g.fillRoundedRectangle (trackX - expansion, glowStart - expansion,
                                           trackWidth + expansion * 2.0f, glowHeight + expansion * 2.0f,
                                           3.0f + expansion);
                }
            }

            // 4. Round chrome slider thumb (Philips style) - SMALLER
            auto thumbRadius = width * 0.32f; // Reduced from 0.4f to make smaller
            auto thumbCenterX = x + width * 0.5f;
            auto thumbCenterY = sliderPos;

            // Thumb shadow
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (thumbCenterX - thumbRadius + 1.5f, thumbCenterY - thumbRadius + 2.0f,
                          thumbRadius * 2.0f, thumbRadius * 2.0f);

            // Glow halo when active (outer glow)
            if (glowIntensity > 0.2f)
            {
                for (int i = 0; i < 5; ++i)
                {
                    float expansion = static_cast<float> (i) * 2.0f;
                    float alpha = glowIntensity * (1.0f - static_cast<float> (i) / 5.0f) * 0.3f;

                    g.setColour (glowColour.withAlpha (alpha));
                    g.fillEllipse (thumbCenterX - thumbRadius - expansion,
                                  thumbCenterY - thumbRadius - expansion,
                                  (thumbRadius + expansion) * 2.0f,
                                  (thumbRadius + expansion) * 2.0f);
                }
            }

            // Outer chrome ring
            juce::ColourGradient outerRing (
                juce::Colour (0xffe0e0e0), thumbCenterX, thumbCenterY - thumbRadius,
                juce::Colour (0xff808080), thumbCenterX, thumbCenterY + thumbRadius,
                false);
            outerRing.addColour (0.3, juce::Colour (0xfff5f5f5));
            outerRing.addColour (0.7, juce::Colour (0xff606060));

            g.setGradientFill (outerRing);
            g.fillEllipse (thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                          thumbRadius * 2.0f, thumbRadius * 2.0f);

            // Dark inner circle WITH INNER GLOW
            auto innerRadius = thumbRadius * 0.7f;

            // INNER GLOW - radial glow from center outward when active
            if (glowIntensity > 0.01f)
            {
                // Multiple concentric glow layers from center
                for (int i = 0; i < 12; ++i)
                {
                    float layerProgress = static_cast<float> (i) / 12.0f;
                    float currentRadius = innerRadius * (0.2f + layerProgress * 0.8f);
                    float alpha = glowIntensity * (1.0f - layerProgress) * 0.25f;

                    g.setColour (glowColour.withAlpha (alpha));
                    g.fillEllipse (thumbCenterX - currentRadius, thumbCenterY - currentRadius,
                                  currentRadius * 2.0f, currentRadius * 2.0f);
                }

                // Bright core glow at center
                auto coreRadius = innerRadius * 0.35f;
                g.setColour (glowColour.withAlpha (glowIntensity * 0.5f));
                g.fillEllipse (thumbCenterX - coreRadius, thumbCenterY - coreRadius,
                              coreRadius * 2.0f, coreRadius * 2.0f);
            }

            // Dark base (only visible when not glowing)
            g.setColour (juce::Colour (0xff2a2a2a).withAlpha (1.0f - glowIntensity * 0.6f));
            g.fillEllipse (thumbCenterX - innerRadius, thumbCenterY - innerRadius,
                          innerRadius * 2.0f, innerRadius * 2.0f);

            // Highlight on top (3D effect)
            juce::ColourGradient highlight (
                juce::Colours::white.withAlpha (0.4f), thumbCenterX, thumbCenterY - thumbRadius * 0.5f,
                juce::Colours::transparentBlack, thumbCenterX, thumbCenterY,
                false);
            g.setGradientFill (highlight);
            g.fillEllipse (thumbCenterX - thumbRadius * 0.8f, thumbCenterY - thumbRadius * 0.8f,
                          thumbRadius * 1.6f, thumbRadius * 1.2f);

            // Outer edge definition
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.drawEllipse (thumbCenterX - thumbRadius, thumbCenterY - thumbRadius,
                          thumbRadius * 2.0f, thumbRadius * 2.0f, 1.0f);
        }
        else
        {
            // Fallback to default for other slider types
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                             sliderPos, minSliderPos, maxSliderPos,
                                             style, slider);
        }
    }

private:
    juce::Colour glowColour;
    float glowIntensity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowingKnobLookAndFeel)
};
