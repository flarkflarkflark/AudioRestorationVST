#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Correction List View Component
 *
 * Displays list of detected/applied corrections:
 * - Position (time)
 * - Magnitude
 * - Width
 * - Type (auto/manual)
 *
 * Supports:
 * - Click to select/audition
 * - Right-click context menu
 * - Filter criteria
 *
 * Standalone mode only.
 */
class CorrectionListView : public juce::Component,
                            public juce::TableListBoxModel
{
public:
    CorrectionListView()
    {
        table.setModel (this);
        table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
        table.setOutlineThickness (1);
        addAndMakeVisible (table);

        // TODO: Add columns
    }

    void resized() override
    {
        table.setBounds (getLocalBounds());
    }

    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override
    {
        // TODO: Return number of corrections
        return 0;
    }

    void paintRowBackground (juce::Graphics& g, int rowNumber,
                             int width, int height,
                             bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (juce::Colours::lightblue);
        else
            g.fillAll (rowNumber % 2 ? juce::Colours::white : juce::Colour (0xffeeeeee));
    }

    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override
    {
        // TODO: Paint cell content
    }

private:
    juce::TableListBox table;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrectionListView)
};
