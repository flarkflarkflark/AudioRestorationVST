#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Vintage VU-Meter Display for EQ Bands
 *
 * Displays gain level in classic analog VU-meter style with:
 * - Vertical bar display
 * - Green/yellow/red segments
 * - Glowing effect based on level
 * - Chrome frame
 */
class VintageVUMeter : public juce::Component
{
public:
    VintageVUMeter()
    {
        setInterceptsMouseClicks (false, false);
    }

    void setLevel (float gainDB)
    {
        // Convert dB to normalized level (-12 to +12 dB -> 0.0 to 1.0)
        auto normalized = juce::jlimit (0.0f, 1.0f, (gainDB + 12.0f) / 24.0f);

        if (std::abs (normalized - currentLevel) > 0.001f)
        {
            currentLevel = normalized;
            repaint();
        }
    }

    void setGlowColour (juce::Colour colour)
    {
        glowColour = colour;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

        // 1. Chrome frame background
        juce::ColourGradient frameGradient (
            juce::Colour (0xffc0c0c0), bounds.getCentreX(), bounds.getY(),
            juce::Colour (0xff707070), bounds.getCentreX(), bounds.getBottom(),
            false);
        frameGradient.addColour (0.3, juce::Colour (0xffe0e0e0));
        frameGradient.addColour (0.7, juce::Colour (0xff505050));

        g.setGradientFill (frameGradient);
        g.fillRoundedRectangle (bounds, 2.0f);

        // Frame border
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

        // 2. Inner dark background (meter face)
        auto meterBounds = bounds.reduced (2.0f);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRoundedRectangle (meterBounds, 1.0f);

        // 3. Center line (0 dB mark)
        auto centerY = meterBounds.getCentreY();
        g.setColour (juce::Colour (0xff505050));
        g.drawHorizontalLine (juce::roundToInt (centerY),
                             meterBounds.getX() + 1.0f,
                             meterBounds.getRight() - 1.0f);

        // 4. Draw level bar
        if (currentLevel > 0.51f || currentLevel < 0.49f) // Not at center (0 dB)
        {
            auto levelHeight = std::abs (currentLevel - 0.5f) * height;
            auto barBounds = meterBounds.reduced (1.0f);

            juce::Rectangle<float> levelBar;

            if (currentLevel > 0.5f) // Boost
            {
                // Bar goes upward from center
                levelBar = juce::Rectangle<float> (
                    barBounds.getX(),
                    centerY - levelHeight,
                    barBounds.getWidth(),
                    levelHeight
                );
            }
            else // Cut
            {
                // Bar goes downward from center
                levelBar = juce::Rectangle<float> (
                    barBounds.getX(),
                    centerY,
                    barBounds.getWidth(),
                    levelHeight
                );
            }

            // Determine bar color based on level
            juce::Colour barColour;
            float intensity = std::abs (currentLevel - 0.5f) * 2.0f; // 0.0 to 1.0

            if (intensity < 0.5f)
                barColour = juce::Colours::green; // Low level
            else if (intensity < 0.75f)
                barColour = juce::Colours::yellow; // Medium level
            else
                barColour = juce::Colours::orange; // High level

            // Glow effect behind bar
            for (int i = 0; i < 6; ++i)
            {
                float expansion = static_cast<float> (i) * 1.0f;
                float alpha = intensity * (1.0f - static_cast<float> (i) / 6.0f) * 0.25f;

                g.setColour (glowColour.withAlpha (alpha));
                g.fillRoundedRectangle (levelBar.expanded (expansion), 1.0f);
            }

            // Solid bar with gradient
            juce::ColourGradient barGradient (
                barColour.brighter (0.3f), levelBar.getCentreX(), levelBar.getY(),
                barColour.darker (0.2f), levelBar.getCentreX(), levelBar.getBottom(),
                false);

            g.setGradientFill (barGradient);
            g.fillRoundedRectangle (levelBar, 1.0f);

            // Bar highlight
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillRoundedRectangle (levelBar.reduced (1.0f, 0.0f).removeFromLeft (2.0f), 1.0f);
        }

        // 5. Scale markings
        g.setColour (juce::Colour (0xff808080).withAlpha (0.5f));
        auto markX = meterBounds.getRight() - 1.0f;

        // Draw tick marks at -12, -6, 0, +6, +12 dB
        for (int db = -12; db <= 12; db += 6)
        {
            float normalizedPos = (static_cast<float> (db) + 12.0f) / 24.0f;
            float y = meterBounds.getBottom() - normalizedPos * meterBounds.getHeight();

            float tickLength = (db == 0) ? 4.0f : 2.0f;
            g.drawHorizontalLine (juce::roundToInt (y),
                                 markX - tickLength,
                                 markX);
        }
    }

private:
    float currentLevel = 0.5f; // 0.5 = 0 dB (center)
    juce::Colour glowColour = juce::Colours::green;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageVUMeter)
};
