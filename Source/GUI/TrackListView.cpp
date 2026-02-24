#include "TrackListView.h"

TrackListView::TrackListView()
{
    addAndMakeVisible (listBox);
    listBox.setModel (this);
    listBox.setRowHeight (24);
}

TrackListView::~TrackListView()
{
    listBox.setModel (nullptr);
}

void TrackListView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);
}

void TrackListView::resized()
{
    listBox.setBounds (getLocalBounds().reduced (2));
}

int TrackListView::getNumRows()
{
    return static_cast<int> (markers.size());
}

void TrackListView::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= markers.size()) return;

    if (rowIsSelected)
        g.fillAll (juce::Colours::darkblue.withAlpha (0.4f));

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    
    auto& m = markers[rowNumber];
    juce::String text = juce::String (rowNumber + 1) + ". " + m.name + " [" + juce::String (m.position) + "]";
    g.drawText (text, 10, 0, width - 20, height, juce::Justification::centredLeft);
}

void TrackListView::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row >= 0 && row < markers.size())
    {
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu m;
            m.addItem (1, "Delete Marker");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, row](int res) {
                if (res == 1 && onMarkerDeleted) onMarkerDeleted (row);
            });
        }
        else if (onMarkerClicked)
        {
            onMarkerClicked (markers[row].position);
        }
    }
}

void TrackListView::addMarker (int64_t position, const juce::String& name)
{
    markers.push_back ({position, name});
    std::sort (markers.begin(), markers.end(), [](const auto& a, const auto& b) { return a.position < b.position; });
    listBox.updateContent();
    repaint();
}

void TrackListView::removeMarker (int index)
{
    if (index >= 0 && index < markers.size())
    {
        markers.erase (markers.begin() + index);
        listBox.updateContent();
        repaint();
    }
}

void TrackListView::clearMarkers()
{
    markers.clear();
    listBox.updateContent();
    repaint();
}
