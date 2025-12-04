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
    struct Correction
    {
        int64_t position;      // Sample position
        float magnitude;       // 0.0 to 1.0
        int width;            // Width in samples
        juce::String type;    // "Auto" or "Manual"
        bool applied;         // Has correction been applied?

        Correction (int64_t pos, float mag, int w, const juce::String& t, bool app = false)
            : position (pos), magnitude (mag), width (w), type (t), applied (app) {}
    };

    //==============================================================================
    CorrectionListView();

    void addCorrection (int64_t position, float magnitude, int width,
                       const juce::String& type = "Auto", bool applied = false);
    void clearCorrections();
    void markAllApplied();
    int getNumCorrections() const { return corrections.size(); }
    const Correction& getCorrection (int index) const { return corrections[index]; }

    /** Set sample rate for time display */
    void setSampleRate (double sr) { sampleRate = sr; }

    /** Set status text to display */
    void setStatusText (const juce::String& text) { statusText = text; repaint(); }

    /** Set callback for when correction is selected */
    std::function<void(int64_t position)> onCorrectionSelected;

    /** Set callback for audition request (play audio around correction) */
    std::function<void(int64_t position, float durationSec)> onAuditionCorrection;

    /** Set callback for delete correction request */
    std::function<void(int index)> onDeleteCorrection;

    /** Set callback for adjust correction request (opens dialog) */
    std::function<void(int index)> onAdjustCorrection;

    /** Remove a correction by index */
    void removeCorrection (int index);

    /** Update a correction's parameters */
    void updateCorrection (int index, int64_t newPosition, float newMagnitude, int newWidth);

    //==============================================================================
    void resized() override;
    void paint (juce::Graphics& g) override;

    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground (juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell (juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent& event) override;
    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                              juce::Component* existingComponentToUpdate) override;

private:
    juce::String formatTime (int64_t samplePosition);

    juce::TableListBox table;
    std::vector<Correction> corrections;
    double sampleRate = 44100.0;
    juce::String statusText = "Ready";

    enum ColumnIds
    {
        TimeColumn = 1,
        MagnitudeColumn = 2,
        WidthColumn = 3,
        TypeColumn = 4,
        AppliedColumn = 5
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrectionListView)
};
