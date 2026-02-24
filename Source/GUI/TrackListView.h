#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class TrackListView : public juce::Component,
                      public juce::ListBoxModel
{
public:
    struct TrackMarker
    {
        int64_t position;
        juce::String name;
    };

    TrackListView();
    ~TrackListView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void addMarker (int64_t position, const juce::String& name);
    void removeMarker (int index);
    void clearMarkers();
    
    int getNumMarkers() const { return static_cast<int> (markers.size()); }
    const TrackMarker& getMarker (int index) const { return markers[index]; }

    std::function<void(int64_t)> onMarkerClicked;
    std::function<void(int)> onMarkerDeleted;

private:
    juce::ListBox listBox;
    std::vector<TrackMarker> markers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackListView)
};
