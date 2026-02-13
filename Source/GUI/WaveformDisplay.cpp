#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay()
    : thumbnail (128, formatManager, thumbnailCache)
{
    // Register all supported audio formats (WAV, AIFF, FLAC, OGG, MP3)
    formatManager.registerBasicFormats();
    formatManager.registerFormat (new juce::FlacAudioFormat(), true);
    formatManager.registerFormat (new juce::OggVorbisAudioFormat(), true);

    #if JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat (new juce::MP3AudioFormat(), true);
    #endif

    thumbnail.addChangeListener (this);
    startTimer (40); // 25 fps for smooth cursor updates

    // Enable OpenGL hardware acceleration for smooth waveform rendering
    #if JUCE_OPENGL
    try
    {
        openGLContext.setComponentPaintingEnabled (true);
        openGLContext.attachTo (*this);
        DBG ("WaveformDisplay: OpenGL hardware acceleration enabled");
    }
    catch (...)
    {
        DBG ("WaveformDisplay: OpenGL initialization failed, using software rendering");
        useOpenGL = false;
    }
    #else
    useOpenGL = false;
    #endif
}

WaveformDisplay::~WaveformDisplay()
{
    thumbnail.removeChangeListener (this);

    #if JUCE_OPENGL
    if (useOpenGL)
        openGLContext.detach();
    #endif
}

void WaveformDisplay::loadFile (const juce::File& file)
{
    // Clear previous waveform first
    thumbnail.clear();

    auto* reader = formatManager.createReaderFor (file);

    if (reader != nullptr)
    {
        sampleRate = reader->sampleRate;
        DBG ("Audio file info: " + juce::String (reader->numChannels) + " channels, " +
             juce::String (sampleRate) + " Hz, " +
             juce::String (reader->lengthInSamples) + " samples");
        delete reader;

        // Load the file into the thumbnail
        thumbnail.setSource (new juce::FileInputSource (file));

        DBG ("Waveform thumbnail loading started for: " + file.getFullPathName());

        // Force immediate repaint
        repaint();
    }
    else
    {
        DBG ("Failed to create reader for file: " + file.getFullPathName());
    }
}

void WaveformDisplay::clear()
{
    thumbnail.clear();
    clickMarkers.clear();
    selectionStart = -1;
    selectionEnd = -1;
    playbackPosition = 0.0;
    repaint();
}

void WaveformDisplay::setPlaybackPosition (double position)
{
    playbackPosition = juce::jlimit (0.0, 1.0, position);
}

void WaveformDisplay::addClickMarker (int64_t samplePosition)
{
    clickMarkers.push_back (samplePosition);
    std::sort (clickMarkers.begin(), clickMarkers.end());
    repaint();
}

void WaveformDisplay::clearClickMarkers()
{
    clickMarkers.clear();
    repaint();
}

void WaveformDisplay::updateFromBuffer (const juce::AudioBuffer<float>& buffer, double newSampleRate)
{
    if (buffer.getNumSamples() == 0)
        return;

    sampleRate = newSampleRate;

    // Clear previous thumbnail and reset it from the buffer
    thumbnail.clear();

    // Reset the thumbnail with new data
    thumbnail.reset (buffer.getNumChannels(), newSampleRate, buffer.getNumSamples());

    // Add the entire buffer to the thumbnail
    thumbnail.addBlock (0, buffer, 0, buffer.getNumSamples());

    repaint();
    DBG ("Waveform updated from buffer: " + juce::String (buffer.getNumSamples()) + " samples");
}

void WaveformDisplay::setHorizontalZoom (double samplesPerPixel)
{
    horizontalZoom = juce::jmax (1.0, samplesPerPixel);
    repaint();
}

void WaveformDisplay::setVerticalZoom (double amplitudeMultiplier)
{
    verticalZoom = juce::jlimit (0.1, 10.0, amplitudeMultiplier);
    repaint();
}

bool WaveformDisplay::zoomToSelection()
{
    if (selectionStart < 0 || selectionEnd < 0 || sampleRate <= 0.0)
        return false;

    double totalLength = thumbnail.getTotalLength();
    if (totalLength <= 0.0)
        return false;

    double selStartTime = selectionStart / sampleRate;
    double selEndTime = selectionEnd / sampleRate;
    double selDuration = selEndTime - selStartTime;

    if (selDuration <= 0.0)
        return false;

    horizontalZoom = totalLength / selDuration;
    horizontalZoom = juce::jlimit (1.0, 100.0, horizontalZoom);

    double maxScroll = totalLength - (totalLength / horizontalZoom);
    if (maxScroll > 0.0)
        scrollPosition = juce::jlimit (0.0, 1.0, selStartTime / maxScroll);
    else
        scrollPosition = 0.0;

    repaint();
    return true;
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll (juce::Colour (0xff1e1e1e));

    if (thumbnail.getTotalLength() > 0.0)
    {
        auto rulerBounds = bounds.removeFromTop (timeRulerHeight);
        drawTimeRuler (g, rulerBounds);

        auto waveformBounds = bounds;
        drawWaveform (g, waveformBounds);
        drawClickMarkers (g, waveformBounds);
        drawSelection (g, waveformBounds);
        drawPlaybackCursor (g, waveformBounds);
    }
    else if (thumbnail.isFullyLoaded() == false && thumbnail.getNumChannels() > 0)
    {
        // Thumbnail is loading
        g.setColour (juce::Colours::lightgrey);
        g.setFont (16.0f);
        g.drawText ("Loading waveform...", bounds, juce::Justification::centred);
    }
    else
    {
        // No audio loaded
        g.setColour (juce::Colours::grey);
        g.setFont (16.0f);
        g.drawText ("No audio file loaded", bounds, juce::Justification::centred);
    }

    // Border
    g.setColour (juce::Colours::darkgrey);
    g.drawRect (bounds, 1);
}

void WaveformDisplay::resized()
{
    // Update zoom to fit if needed
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    if (event.position.y <= timeRulerHeight)
    {
        double totalLength = thumbnail.getTotalLength();
        double visibleDuration = totalLength / horizontalZoom;
        double viewStartTime = scrollPosition * juce::jmax (0.001, totalLength - visibleDuration);
        double clickTime = viewStartTime + (event.position.x / (double) getWidth()) * visibleDuration;
        clickTime = juce::jlimit (0.0, totalLength, clickTime);
        int64_t clickSample = static_cast<int64_t> (clickTime * sampleRate);

        if (event.mods.isRightButtonDown())
        {
            selectionEnd = clickSample;
            if (selectionStart < 0)
                selectionStart = clickSample;
        }
        else
        {
            selectionStart = clickSample;
            if (selectionEnd < 0)
                selectionEnd = clickSample;
        }

        if (selectionStart > selectionEnd)
            std::swap (selectionStart, selectionEnd);

        if (onSelectionChanged && selectionStart >= 0 && selectionEnd >= 0)
            onSelectionChanged (selectionStart, selectionEnd);

        repaint();
        return;
    }

    // Store drag start state
    dragStartPosition = event.position;
    isDragging = false;
    isSelectionDrag = false;
    isHandleDrag = false;
    activeHandle = HandleDrag::none;

    // Calculate click time position
    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double viewStartTime = scrollPosition * juce::jmax (0.001, totalLength - visibleDuration);
    double clickTime = viewStartTime + (event.position.x / (double) getWidth()) * visibleDuration;
    clickTime = juce::jlimit (0.0, totalLength, clickTime);

    // Store for context menu and other uses
    zoomCenterPosition = clickTime / totalLength;

    // Right-click: Just store position for context menu (handled in mouseUp)
    if (event.mods.isRightButtonDown())
    {
        return;
    }

    // Left-click: Start selection or set playhead
    // Selection starts from click position
    int64_t clickSample = static_cast<int64_t> (clickTime * sampleRate);

    if (event.mods.isLeftButtonDown() && hasSelection())
    {
        double startTime = (selectionStart / sampleRate);
        double endTime = (selectionEnd / sampleRate);
        double startPosInView = (startTime - viewStartTime) / visibleDuration;
        double endPosInView = (endTime - viewStartTime) / visibleDuration;
        float x1 = (float) (startPosInView * getWidth());
        float x2 = (float) (endPosInView * getWidth());
        float handleRadius = (float) selectionHandleWidth * 1.5f;

        if (std::abs (event.position.x - x1) <= handleRadius)
        {
            isHandleDrag = true;
            activeHandle = HandleDrag::start;
            return;
        }
        if (std::abs (event.position.x - x2) <= handleRadius)
        {
            isHandleDrag = true;
            activeHandle = HandleDrag::end;
            return;
        }
    }

    // If Shift is held and we have an existing selection, extend it
    if (event.mods.isShiftDown() && hasSelection())
    {
        // Extend selection to click position
        if (clickSample < selectionStart)
            selectionStart = clickSample;
        else
            selectionEnd = clickSample;

        isSelectionDrag = true;
    }
    else
    {
        // Start new selection from click position
        selectionStart = clickSample;
        selectionEnd = clickSample;
        isSelectionDrag = true;
    }

    // Set playhead to click position
    playbackPosition = clickTime / totalLength;

    repaint();
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    // Right-click drag: Do nothing (or could pan view)
    if (event.mods.isRightButtonDown())
        return;

    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double viewStartTime = scrollPosition * juce::jmax (0.001, totalLength - visibleDuration);

    // Handle dragging selection handles
    if (isHandleDrag && activeHandle != HandleDrag::none)
    {
        double totalLength = thumbnail.getTotalLength();
        double visibleDuration = totalLength / horizontalZoom;
        double viewStartTime = scrollPosition * juce::jmax (0.001, totalLength - visibleDuration);
        double dragTime = viewStartTime + (event.position.x / (double) getWidth()) * visibleDuration;
        dragTime = juce::jlimit (0.0, totalLength, dragTime);

        int64_t dragSample = static_cast<int64_t> (dragTime * sampleRate);

        if (activeHandle == HandleDrag::start)
            selectionStart = dragSample;
        else if (activeHandle == HandleDrag::end)
            selectionEnd = dragSample;

        if (selectionStart > selectionEnd)
            std::swap (selectionStart, selectionEnd);

        repaint();
        return;
    }

    // Left-click drag: Make/extend selection (standard audio editor behavior)
    if (isSelectionDrag)
    {
        isDragging = true;

        // Calculate drag time position
        double dragTime = viewStartTime + (event.position.x / (double) getWidth()) * visibleDuration;
        dragTime = juce::jlimit (0.0, totalLength, dragTime);

        int64_t dragSample = static_cast<int64_t> (dragTime * sampleRate);

        // Update selection end (start stays where click began)
        selectionEnd = dragSample;

        repaint();
    }
}

void WaveformDisplay::mouseUp (const juce::MouseEvent& event)
{
    if (event.position.y <= timeRulerHeight && event.mods.isRightButtonDown())
        return;

    // Handle right-click context menu
    if (event.mods.isRightButtonDown())
    {
        showContextMenu (event);
        return;
    }

    // Finalize selection
    if (isSelectionDrag)
    {
        // If no drag occurred (just a click), clear selection and set playhead
        float dragDistance = dragStartPosition.getDistanceFrom (event.position);
        if (dragDistance < 3.0f)
        {
            // Just a click - set playhead, no selection
            clearSelection();

            // Notify to seek to this position
            if (onSeekPosition)
            {
                onSeekPosition (zoomCenterPosition);
            }
        }
        else
        {
            // Ensure selectionStart < selectionEnd
            if (selectionStart > selectionEnd)
                std::swap (selectionStart, selectionEnd);

            // Notify listener of selection change
            if (onSelectionChanged && selectionStart >= 0 && selectionEnd >= 0)
                onSelectionChanged (selectionStart, selectionEnd);

            DBG ("Selection: " + juce::String (selectionStart) + " - " + juce::String (selectionEnd) +
                 " (" + juce::String ((selectionEnd - selectionStart) / sampleRate, 2) + " sec)");
        }
    }

    if (isHandleDrag)
    {
        if (onSelectionChanged && selectionStart >= 0 && selectionEnd >= 0)
            onSelectionChanged (selectionStart, selectionEnd);
    }

    // Reset drag state
    isSelectionDrag = false;
    isDragging = false;
    isHandleDrag = false;
    activeHandle = HandleDrag::none;
}

void WaveformDisplay::showContextMenu (const juce::MouseEvent& event)
{
    juce::PopupMenu menu;

    // Edit menu (clipboard operations) - at top for easy access
    juce::PopupMenu editMenu;
    editMenu.addItem (actionCut, "Cut                     Ctrl+X", hasSelection());
    editMenu.addItem (actionCopy, "Copy                    Ctrl+C", hasSelection());
    editMenu.addItem (actionPaste, "Paste                   Ctrl+V", hasClipboardData());
    editMenu.addItem (actionDeleteSelection, "Delete Selection        Del", hasSelection());
    editMenu.addSeparator();
    editMenu.addItem (actionCropToSelection, "Crop to Selection", hasSelection());
    editMenu.addItem (actionSelectAll, "Select All              Ctrl+A", thumbnail.getTotalLength() > 0.0);
    menu.addSubMenu ("Edit", editMenu);
    menu.addSeparator();

    // Selection playback
    menu.addItem (actionPlaySelection, "Play Selection          Space", hasSelection());
    menu.addSeparator();

    // Process menu
    juce::PopupMenu processMenu;
    processMenu.addItem (actionDetectClicks, "Detect Clicks");
    processMenu.addItem (actionRemoveClicks, "Remove Clicks");
    processMenu.addSeparator();
    processMenu.addItem (actionNoiseReduction, "Noise Reduction...");
    menu.addSubMenu ("Process", processMenu);
    menu.addSeparator();

    // View options
    menu.addItem (1, "Zoom to Fit");
    menu.addItem (2, "Zoom to Selection", hasSelection());
    menu.addSeparator();
    menu.addItem (3, "Clear Selection", hasSelection());
    menu.addSeparator();
    menu.addItem (4, "Mark Click at Cursor");
    menu.addItem (5, "Clear All Markers", !clickMarkers.empty());
    menu.addSeparator();

    // Options
    menu.addItem (actionAudioSettings, "Audio Settings...");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
        juce::Rectangle<int> (event.getScreenX(), event.getScreenY(), 1, 1)),
        [this] (int result)
        {
            switch (result)
            {
                case 1: // Zoom to Fit
                    horizontalZoom = 1.0;
                    verticalZoom = 1.0;
                    scrollPosition = 0.0;
                    repaint();
                    break;

                case 2: // Zoom to Selection
                    if (hasSelection())
                    {
                        double totalLength = thumbnail.getTotalLength();
                        double selStartTime = selectionStart / sampleRate;
                        double selEndTime = selectionEnd / sampleRate;
                        double selDuration = selEndTime - selStartTime;

                        if (selDuration > 0.001)
                        {
                            horizontalZoom = totalLength / selDuration;
                            horizontalZoom = juce::jlimit (1.0, 100.0, horizontalZoom);

                            double maxScroll = totalLength - (totalLength / horizontalZoom);
                            if (maxScroll > 0.0)
                                scrollPosition = selStartTime / maxScroll;
                            else
                                scrollPosition = 0.0;

                            scrollPosition = juce::jlimit (0.0, 1.0, scrollPosition);
                            repaint();
                        }
                    }
                    break;

                case 3: // Clear Selection
                    clearSelection();
                    break;

                case 4: // Mark Click at Cursor
                    {
                        double totalLength = thumbnail.getTotalLength();
                        double visibleDuration = totalLength / horizontalZoom;
                        double startTime = scrollPosition * (totalLength - visibleDuration);
                        // Use the position where context menu was triggered
                        double clickTime = startTime + (zoomCenterPosition * totalLength - startTime);
                        addClickMarker ((int64_t) (clickTime * sampleRate));
                    }
                    break;

                case 5: // Clear All Markers
                    clearClickMarkers();
                    break;

                default:
                    // Handle process actions via callback
                    if (result >= 100 && result < 200 && onProcessAction)
                    {
                        onProcessAction (result);
                    }
                    // Handle clipboard actions via callback
                    else if (result >= 200 && onClipboardAction)
                    {
                        onClipboardAction (result, selectionStart, selectionEnd);
                    }
                    break;
            }
        });
}

void WaveformDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    // Calculate position in file (0.0 to 1.0)
    double totalLength = thumbnail.getTotalLength();
    double clickTime = (event.position.x / (double) getWidth()) * totalLength;
    double position = clickTime / totalLength;

    // Notify listener to seek to this position
    if (onSeekPosition)
        onSeekPosition (position);

    DBG ("Double-click seek to: " + juce::String (position));
}

void WaveformDisplay::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    if (!event.mods.isCtrlDown() && !event.mods.isCommandDown() && !event.mods.isShiftDown())
    {
        if (hasSelection())
        {
            int64_t selStart = -1, selEnd = -1;
            getSelection (selStart, selEnd);
            if (selStart >= 0 && selEnd > selStart)
            {
                double totalLength = thumbnail.getTotalLength();
                double selectionMidTime = ((selStart + selEnd) * 0.5) / sampleRate;
                double zoomFactor = wheel.deltaY > 0 ? 1.25 : 0.8;
                double newZoom = juce::jlimit (1.0, 100.0, horizontalZoom * zoomFactor);

                if (newZoom != horizontalZoom)
                {
                    horizontalZoom = newZoom;
                    double newVisibleDuration = totalLength / horizontalZoom;
                    double newViewStart = selectionMidTime - 0.5 * newVisibleDuration;
                    double maxScroll = totalLength - newVisibleDuration;

                    if (maxScroll > 0.0)
                        scrollPosition = juce::jlimit (0.0, 1.0, newViewStart / maxScroll);
                    else
                        scrollPosition = 0.0;

                    repaint();
                }
                return;
            }
        }
    }

    // Ctrl+wheel: Horizontal zoom (centered on mouse position)
    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        // Get mouse position as fraction of view
        double mouseXFraction = event.position.x / (double) getWidth();

        // Calculate current view parameters
        double totalLength = thumbnail.getTotalLength();
        double visibleDuration = totalLength / horizontalZoom;
        double viewStartTime = scrollPosition * juce::jmax (0.001, totalLength - visibleDuration);
        double mouseTime = viewStartTime + mouseXFraction * visibleDuration;

        // Apply zoom
        double zoomFactor = wheel.deltaY > 0 ? 1.25 : 0.8;  // Smoother zoom steps
        double newZoom = juce::jlimit (1.0, 100.0, horizontalZoom * zoomFactor);

        if (newZoom != horizontalZoom)
        {
            horizontalZoom = newZoom;

            // Adjust scroll to keep mouse position stable
            double newVisibleDuration = totalLength / horizontalZoom;
            double newViewStart = mouseTime - mouseXFraction * newVisibleDuration;
            double maxScroll = totalLength - newVisibleDuration;

            if (maxScroll > 0.0)
                scrollPosition = juce::jlimit (0.0, 1.0, newViewStart / maxScroll);
            else
                scrollPosition = 0.0;

            repaint();
        }
    }
    // Shift+wheel: Vertical zoom (amplitude)
    else if (event.mods.isShiftDown())
    {
        double zoomFactor = wheel.deltaY > 0 ? 1.15 : 0.87;
        setVerticalZoom (verticalZoom * zoomFactor);
    }
    // Plain wheel: Scroll/pan horizontally through waveform
    else
    {
        if (horizontalZoom > 1.0)
        {
            // Scroll amount proportional to visible duration
            double scrollAmount = wheel.deltaY * 0.1;  // 10% of view per scroll tick
            scrollPosition = juce::jlimit (0.0, 1.0, scrollPosition - scrollAmount);
            repaint();
        }
    }
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}

void WaveformDisplay::timerCallback()
{
    // Smooth cursor updates
    repaint();
}

void WaveformDisplay::drawWaveform (juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    // Calculate visible range based on zoom and scroll
    double totalLength = thumbnail.getTotalLength();

    if (totalLength <= 0.0)
        return;

    // Calculate the visible portion of the waveform
    double visibleDuration = totalLength / horizontalZoom;
    double startTime = scrollPosition * (totalLength - visibleDuration);
    startTime = juce::jmax (0.0, startTime);

    double endTime = startTime + visibleDuration;
    endTime = juce::jmin (totalLength, endTime);

    // Draw waveform with zoom applied
    g.setColour (juce::Colour (0xff4a90e2)); // Blue waveform
    thumbnail.drawChannels (g, bounds, startTime, endTime, verticalZoom);
}

void WaveformDisplay::drawTimeRuler (juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    g.setColour (juce::Colour (0xff232323));
    g.fillRect (bounds);

    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double startTime = scrollPosition * (totalLength - visibleDuration);
    startTime = juce::jmax (0.0, startTime);
    double endTime = juce::jmin (totalLength, startTime + visibleDuration);

    const double desiredStep = visibleDuration / 8.0;
    const double stepOptions[] = {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0, 60.0, 120.0, 300.0};
    double step = stepOptions[0];
    for (double option : stepOptions)
    {
        if (option >= desiredStep)
        {
            step = option;
            break;
        }
    }

    g.setColour (juce::Colour (0xff8c8c8c));
    g.setFont (12.0f);

    double firstTick = std::floor (startTime / step) * step;
    for (double t = firstTick; t <= endTime + step; t += step)
    {
        if (t < startTime)
            continue;
        double pos = (t - startTime) / visibleDuration;
        int x = bounds.getX() + (int) (pos * bounds.getWidth());
        g.drawVerticalLine (x, (float) bounds.getBottom() - 6.0f, (float) bounds.getBottom());

        int minutes = (int) (t / 60.0);
        double seconds = t - minutes * 60;
        juce::String label = juce::String::formatted ("%02d:%04.1f", minutes, seconds);
        g.drawText (label, x + 2, bounds.getY(), 70, bounds.getHeight(), juce::Justification::centredLeft);
    }

    if (selectionStart >= 0 && selectionEnd >= 0)
    {
        double selStartTime = selectionStart / sampleRate;
        double selEndTime = selectionEnd / sampleRate;

        if (selEndTime >= startTime && selStartTime <= endTime)
        {
            g.setColour (juce::Colour (0xff4a90e2));
            auto drawMarker = [&] (double time, const juce::String& text)
            {
                double pos = (time - startTime) / visibleDuration;
                int x = bounds.getX() + (int) (pos * bounds.getWidth());
                g.drawVerticalLine (x, (float) bounds.getY(), (float) bounds.getBottom());
                g.drawText (text, x + 2, bounds.getY(), 70, bounds.getHeight(), juce::Justification::centredLeft);
            };

            int startMin = (int) (selStartTime / 60.0);
            double startSec = selStartTime - startMin * 60;
            int endMin = (int) (selEndTime / 60.0);
            double endSec = selEndTime - endMin * 60;

            drawMarker (selStartTime, "L " + juce::String::formatted ("%02d:%04.1f", startMin, startSec));
            drawMarker (selEndTime, "R " + juce::String::formatted ("%02d:%04.1f", endMin, endSec));
        }
    }
}

void WaveformDisplay::drawClickMarkers (juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (thumbnail.getTotalLength() <= 0.0 || clickMarkers.empty())
        return;

    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double startTime = scrollPosition * (totalLength - visibleDuration);
    startTime = juce::jmax (0.0, startTime);
    double endTime = juce::jmin (totalLength, startTime + visibleDuration);

    g.setColour (juce::Colours::red.withAlpha (0.7f));

    for (auto markerSample : clickMarkers)
    {
        double markerTime = markerSample / sampleRate;
        if (markerTime < startTime || markerTime > endTime)
            continue;

        double posInView = (markerTime - startTime) / visibleDuration;
        float x = (float) (posInView * bounds.getWidth());
        g.drawVerticalLine ((int) x, (float) bounds.getY(), (float) bounds.getBottom());

        // Draw small circle at marker
        g.fillEllipse (x - 3.0f, bounds.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}

void WaveformDisplay::drawPlaybackCursor (juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (thumbnail.getTotalLength() <= 0.0)
        return;

    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double startTime = scrollPosition * (totalLength - visibleDuration);
    startTime = juce::jmax (0.0, startTime);
    double endTime = juce::jmin (totalLength, startTime + visibleDuration);

    double playheadTime = playbackPosition * totalLength;
    if (playheadTime < startTime || playheadTime > endTime)
        return;

    double posInView = (playheadTime - startTime) / visibleDuration;
    float x = (float) (posInView * bounds.getWidth());

    g.setColour (juce::Colours::yellow.withAlpha (0.8f));
    g.drawVerticalLine ((int) x, (float) bounds.getY(), (float) bounds.getBottom());
}

void WaveformDisplay::drawSelection (juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (selectionStart < 0 || selectionEnd < 0 || thumbnail.getTotalLength() <= 0.0)
        return;

    // Calculate visible range based on zoom
    double totalLength = thumbnail.getTotalLength();
    double visibleDuration = totalLength / horizontalZoom;
    double startTime = scrollPosition * (totalLength - visibleDuration);
    startTime = juce::jmax (0.0, startTime);
    double endTime = startTime + visibleDuration;

    // Convert selection samples to time
    double selStartTime = selectionStart / sampleRate;
    double selEndTime = selectionEnd / sampleRate;

    // Check if selection is visible
    if (selEndTime < startTime || selStartTime > endTime)
        return;

    // Calculate pixel positions within visible range
    double startPosInView = (selStartTime - startTime) / visibleDuration;
    double endPosInView = (selEndTime - startTime) / visibleDuration;

    float x1 = (float) (startPosInView * bounds.getWidth());
    float x2 = (float) (endPosInView * bounds.getWidth());

    // Clamp to bounds
    x1 = juce::jlimit (0.0f, (float) bounds.getWidth(), x1);
    x2 = juce::jlimit (0.0f, (float) bounds.getWidth(), x2);

    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.fillRect (x1, (float) bounds.getY(), x2 - x1, (float) bounds.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawRect (x1, (float) bounds.getY(), x2 - x1, (float) bounds.getHeight(), 1.0f);

    // Draw draggable handles
    g.setColour (juce::Colour (0xff4a90e2));
    float handleHeight = 20.0f;
    float handleY = bounds.getY() + 4.0f;
    g.fillRect (x1 - selectionHandleWidth * 0.5f, handleY, (float) selectionHandleWidth, handleHeight);
    g.fillRect (x2 - selectionHandleWidth * 0.5f, handleY, (float) selectionHandleWidth, handleHeight);
}
