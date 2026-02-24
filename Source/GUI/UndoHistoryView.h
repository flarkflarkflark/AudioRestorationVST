#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/AudioUndoManager.h"

class UndoHistoryView : public juce::Component,
                        public juce::ListBoxModel
{
public:
    UndoHistoryView (AudioUndoManager& manager);
    ~UndoHistoryView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void refresh();

    std::function<void(int)> onUndoToIndex;

private:
    AudioUndoManager& undoManager;
    juce::ListBox listBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoHistoryView)
};
