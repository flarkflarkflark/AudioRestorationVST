#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_opengl/juce_opengl.h>

/**
 * Waveform Display Component
 *
 * Displays audio waveform with:
 * - Zoomable view (horizontal and vertical)
 * - Overlay of corrected vs uncorrected waveforms
 * - Visual markers for clicks, track boundaries, cue points
 * - Playback cursor
 * - Mouse interaction for selection and editing
 *
 * Standalone mode only.
 */
class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener,
                        public juce::Timer
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override;

    //==============================================================================
    /** Load audio file into waveform display */
    void loadFile (const juce::File& file);

    /** Clear waveform */
    void clear();

    /** Set playback position (0.0 to 1.0) */
    void setPlaybackPosition (double position);

    /** Add click marker at sample position */
    void addClickMarker (int64_t samplePosition);

    /** Clear all click markers */
    void clearClickMarkers();

    /** Update waveform from an audio buffer (after processing) */
    void updateFromBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate);

    /** Set horizontal zoom level (samples per pixel) */
    void setHorizontalZoom (double samplesPerPixel);

    /** Set vertical zoom level (amplitude multiplier) */
    void setVerticalZoom (double amplitudeMultiplier);

    /** Set callback for when user double-clicks to seek playback position */
    std::function<void(double position)> onSeekPosition;

    /** Set callback for when selection changes */
    std::function<void(int64_t start, int64_t end)> onSelectionChanged;

    /** Set callback for context menu process actions */
    std::function<void(int actionId)> onProcessAction;

    /** Set callback for clipboard operations (cut/copy/paste/delete) */
    std::function<void(int actionId, int64_t start, int64_t end)> onClipboardAction;

    // Process action IDs for context menu
    enum ProcessActionID
    {
        actionDetectClicks = 100,
        actionRemoveClicks = 101,
        actionNoiseReduction = 102,
        actionAudioSettings = 103
    };

    // Clipboard action IDs for context menu
    enum ClipboardActionID
    {
        actionCut = 200,
        actionCopy = 201,
        actionPaste = 202,
        actionDeleteSelection = 203,
        actionCropToSelection = 204,
        actionSelectAll = 205,
        actionPlaySelection = 206
    };

    /** Check if there's data in clipboard for paste operation */
    bool hasClipboardData() const { return clipboardBuffer.getNumSamples() > 0; }

    /** Set clipboard data (called from parent after copy/cut) */
    void setClipboardData (const juce::AudioBuffer<float>& buffer, double sr)
    {
        clipboardBuffer.makeCopyOf (buffer);
        clipboardSampleRate = sr;
    }

    /** Get clipboard data */
    const juce::AudioBuffer<float>& getClipboardBuffer() const { return clipboardBuffer; }
    double getClipboardSampleRate() const { return clipboardSampleRate; }

    /** Get total number of samples (for select all) */
    int64_t getTotalSamples() const { return static_cast<int64_t>(thumbnail.getTotalLength() * sampleRate); }

    //==============================================================================
    // Public access to zoom levels (for UI controls)
    double horizontalZoom = 1.0; // Zoom multiplier (1.0 = fit all, >1 = zoomed in)
    double verticalZoom = 1.0;
    double scrollPosition = 0.0; // Position in file (0.0 to 1.0)

    //==============================================================================
    // Selection management
    void clearSelection() { selectionStart = -1; selectionEnd = -1; repaint(); }
    void setSelection (int64_t start, int64_t end) { selectionStart = start; selectionEnd = end; repaint(); }
    void getSelection (int64_t& start, int64_t& end) const { start = selectionStart; end = selectionEnd; }
    bool hasSelection() const { return selectionStart >= 0 && selectionEnd >= 0; }

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    //==============================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    void drawWaveform (juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawClickMarkers (juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawPlaybackCursor (juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawSelection (juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void showContextMenu (const juce::MouseEvent& event);

    //==============================================================================
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache {10}; // Increased cache size for better performance
    juce::AudioThumbnail thumbnail;

    double sampleRate = 44100.0;
    double playbackPosition = 0.0;

    std::vector<int64_t> clickMarkers;

    // Selection
    int64_t selectionStart = -1;
    int64_t selectionEnd = -1;
    bool isDragging = false;

    // Drag interactions
    juce::Point<float> dragStartPosition;
    bool isHorizontalZoomDrag = false;   // Left-click+drag = horizontal zoom
    bool isVerticalZoomDrag = false;     // Right-click+drag = vertical zoom
    bool isSelectionDrag = false;        // Shift+left-click+drag = selection
    double initialHorizontalZoom = 1.0;
    double initialVerticalZoom = 1.0;
    double initialScrollPosition = 0.0;
    double zoomCenterPosition = 0.0;     // Position (0-1) to zoom around

    // OpenGL hardware acceleration (optional, auto-detects)
    juce::OpenGLContext openGLContext;
    bool useOpenGL = true;

    // Clipboard buffer for cut/copy/paste
    juce::AudioBuffer<float> clipboardBuffer;
    double clipboardSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
