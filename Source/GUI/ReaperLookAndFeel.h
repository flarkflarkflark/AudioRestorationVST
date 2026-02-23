#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Reaper-inspired Look and Feel
 * 
 * Dark, professional, and utilitarian.
 */
class ReaperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReaperLookAndFeel()
    {
        // Dark theme colors (Reaper-like)
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff212121));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5a5a5a));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff888888));
        setColour (juce::Slider::trackColourId, juce::Colour (0xff1a1a1a));
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff121212));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffcccccc));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1a1a));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        
        setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2a));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff444444));
        
        setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a1a));
        setColour (juce::ListBox::outlineColourId, juce::Colour (0xff333333));
        
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2a2a2a));
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xffaaaaaa));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffcccccc));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff444444));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 3.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

        auto baseColour = backgroundColour;
        if (shouldDrawButtonAsDown)         baseColour = baseColour.darker (0.2f);
        else if (shouldDrawButtonAsHighlighted) baseColour = baseColour.brighter (0.1f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        g.setColour (button.findColour (juce::ComboBox::outlineColourId).withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearVertical)
        {
            auto trackWidth = (style == juce::Slider::LinearHorizontal) ? (float)height * 0.3f : (float)width * 0.3f;
            auto isHorizontal = (style == juce::Slider::LinearHorizontal);
            
            juce::Rectangle<float> track;
            if (isHorizontal)
                track = { (float)x, y + height * 0.5f - trackWidth * 0.5f, (float)width, trackWidth };
            else
                track = { x + width * 0.5f - trackWidth * 0.5f, (float)y, trackWidth, (float)height };

            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle (track, 2.0f);
            
            g.setColour (slider.findColour (juce::Slider::outlineColourId));
            g.drawRoundedRectangle (track, 2.0f, 1.0f);

            // Thumb
            float thumbSize = trackWidth * 2.0f;
            juce::Rectangle<float> thumb;
            if (isHorizontal)
                thumb = { sliderPos - thumbSize * 0.5f, y + height * 0.5f - thumbSize * 0.5f, thumbSize, thumbSize };
            else
                thumb = { x + width * 0.5f - thumbSize * 0.5f, sliderPos - thumbSize * 0.5f, thumbSize, thumbSize };

            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (thumb.reduced (1.0f));
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawEllipse (thumb.reduced (1.0f), 1.0f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaperLookAndFeel)
};
