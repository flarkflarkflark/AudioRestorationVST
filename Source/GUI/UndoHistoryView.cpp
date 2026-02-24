#include "UndoHistoryView.h"

UndoHistoryView::UndoHistoryView (AudioUndoManager& manager)
    : undoManager (manager)
{
    addAndMakeVisible (listBox);
    listBox.setModel (this);
    listBox.setRowHeight (24);
}

UndoHistoryView::~UndoHistoryView()
{
    listBox.setModel (nullptr);
}

void UndoHistoryView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void UndoHistoryView::resized()
{
    listBox.setBounds (getLocalBounds().reduced (2));
}

int UndoHistoryView::getNumRows()
{
    return undoManager.getNumUndoStates();
}

void UndoHistoryView::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    int numStates = undoManager.getNumUndoStates();
    if (rowNumber >= numStates) return;

    // Row 0 is the newest state
    juce::String desc = undoManager.getUndoDescription(); 
    // Opmerking: AudioUndoManager moet worden uitgebreid om individuele beschrijvingen op te halen.
    // Voor nu tonen we een generieke lijst.
    
    if (rowIsSelected)
        g.fillAll (juce::Colours::darkred.withAlpha (0.4f));

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    
    juce::String text = juce::String (numStates - rowNumber) + ". Undo State";
    g.drawText (text, 10, 0, width - 20, height, juce::Justification::centredLeft);
}

void UndoHistoryView::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (onUndoToIndex)
        onUndoToIndex (row);
}

void UndoHistoryView::refresh()
{
    listBox.updateContent();
    repaint();
}
