#include "CorrectionListView.h"

CorrectionListView::CorrectionListView()
{
    table.setModel (this);
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::grey);
    table.setOutlineThickness (1);
    table.setRowHeight (22);
    addAndMakeVisible (table);

    // Add columns
    table.getHeader().addColumn ("Time", TimeColumn, 100, 50, 200,
                                  juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Magnitude", MagnitudeColumn, 80, 50, 150,
                                  juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Width", WidthColumn, 80, 50, 150,
                                  juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Type", TypeColumn, 80, 50, 150,
                                  juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn ("Applied", AppliedColumn, 80, 50, 150,
                                  juce::TableHeaderComponent::defaultFlags);
}

void CorrectionListView::addCorrection (int64_t position, float magnitude, int width,
                                       const juce::String& type, bool applied)
{
    corrections.emplace_back (position, magnitude, width, type, applied);

    // Sort by position
    std::sort (corrections.begin(), corrections.end(),
               [](const Correction& a, const Correction& b) { return a.position < b.position; });

    table.updateContent();
    repaint();
}

void CorrectionListView::clearCorrections()
{
    corrections.clear();
    table.updateContent();
    repaint();
}

void CorrectionListView::markAllApplied()
{
    for (auto& correction : corrections)
    {
        correction.applied = true;
    }
    table.updateContent();
    repaint();
}

void CorrectionListView::removeCorrection (int index)
{
    if (index >= 0 && index < static_cast<int> (corrections.size()))
    {
        corrections.erase (corrections.begin() + index);
        table.updateContent();
        repaint();
    }
}

void CorrectionListView::updateCorrection (int index, int64_t newPosition, float newMagnitude, int newWidth)
{
    if (index >= 0 && index < static_cast<int> (corrections.size()))
    {
        corrections[index].position = newPosition;
        corrections[index].magnitude = newMagnitude;
        corrections[index].width = newWidth;
        table.updateContent();
        repaint();
    }
}

void CorrectionListView::resized()
{
    auto bounds = getLocalBounds();
    // Reserve space for status text at bottom
    bounds.removeFromBottom (25);
    table.setBounds (bounds);
}

void CorrectionListView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Draw status text at bottom
    if (statusText.isNotEmpty())
    {
        auto bounds = getLocalBounds().removeFromBottom (25);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (12.0f);
        g.drawText (statusText, bounds.reduced (5, 2), juce::Justification::centredLeft);
    }
}

int CorrectionListView::getNumRows()
{
    return static_cast<int> (corrections.size());
}

void CorrectionListView::paintRowBackground (juce::Graphics& g, int rowNumber,
                                             int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff4a90e2).withAlpha (0.3f)); // Light blue selection
    else
        g.fillAll (rowNumber % 2 ? juce::Colours::white : juce::Colour (0xfff0f0f0)); // Alternating rows
}

void CorrectionListView::paintCell (juce::Graphics& g, int rowNumber, int columnId,
                                   int width, int height, bool rowIsSelected)
{
    if (rowNumber >= static_cast<int> (corrections.size()))
        return;

    const auto& correction = corrections[rowNumber];

    g.setColour (rowIsSelected ? juce::Colours::black : juce::Colour (0xff222222));
    g.setFont (14.0f);

    juce::String text;

    switch (columnId)
    {
        case TimeColumn:
            text = formatTime (correction.position);
            break;

        case MagnitudeColumn:
            text = juce::String (correction.magnitude, 2);
            break;

        case WidthColumn:
            text = juce::String (correction.width) + " samples";
            break;

        case TypeColumn:
            text = correction.type;
            break;

        case AppliedColumn:
            text = correction.applied ? "Yes" : "No";
            g.setColour (correction.applied ? juce::Colours::green : juce::Colours::orange);
            break;
    }

    g.drawText (text, 5, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void CorrectionListView::cellClicked (int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int> (corrections.size()))
    {
        const auto& correction = corrections[rowNumber];

        // Right-click: show context menu
        if (event.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;

            menu.addItem (1, "Audition (1 sec)");
            menu.addItem (2, "Audition (2 sec)");
            menu.addItem (3, "Audition (5 sec)");
            menu.addSeparator();
            menu.addItem (4, "Adjust Correction...");
            menu.addSeparator();
            menu.addItem (5, "Delete Correction");
            menu.addItem (6, "Delete All Corrections");
            menu.addSeparator();
            menu.addItem (7, "Go to Position");

            int selectedRow = rowNumber;  // Capture for lambda
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                juce::Rectangle<int> (event.getScreenX(), event.getScreenY(), 1, 1)),
                [this, selectedRow] (int result)
                {
                    if (selectedRow < 0 || selectedRow >= static_cast<int> (corrections.size()))
                        return;

                    const auto& corr = corrections[selectedRow];

                    switch (result)
                    {
                        case 1: // Audition 1 sec
                            if (onAuditionCorrection)
                                onAuditionCorrection (corr.position, 1.0f);
                            break;

                        case 2: // Audition 2 sec
                            if (onAuditionCorrection)
                                onAuditionCorrection (corr.position, 2.0f);
                            break;

                        case 3: // Audition 5 sec
                            if (onAuditionCorrection)
                                onAuditionCorrection (corr.position, 5.0f);
                            break;

                        case 4: // Adjust Correction
                            if (onAdjustCorrection)
                                onAdjustCorrection (selectedRow);
                            break;

                        case 5: // Delete Correction
                            if (onDeleteCorrection)
                                onDeleteCorrection (selectedRow);
                            else
                                removeCorrection (selectedRow);
                            break;

                        case 6: // Delete All
                            clearCorrections();
                            break;

                        case 7: // Go to Position
                            if (onCorrectionSelected)
                                onCorrectionSelected (corr.position);
                            break;
                    }
                });

            return;  // Don't process as regular click
        }

        // Left-click: notify listeners of selection
        if (onCorrectionSelected)
            onCorrectionSelected (correction.position);

        DBG ("Selected correction at: " + formatTime (correction.position));
    }
}

juce::Component* CorrectionListView::refreshComponentForCell (int rowNumber, int columnId,
                                                              bool isRowSelected,
                                                              juce::Component* existingComponentToUpdate)
{
    // No custom components needed, using paintCell instead
    return nullptr;
}

juce::String CorrectionListView::formatTime (int64_t samplePosition)
{
    double timeInSeconds = samplePosition / sampleRate;

    int hours = (int) (timeInSeconds / 3600.0);
    int minutes = (int) ((timeInSeconds - hours * 3600) / 60.0);
    double seconds = timeInSeconds - hours * 3600 - minutes * 60;

    if (hours > 0)
        return juce::String::formatted ("%d:%02d:%06.3f", hours, minutes, seconds);
    else
        return juce::String::formatted ("%d:%06.3f", minutes, seconds);
}
