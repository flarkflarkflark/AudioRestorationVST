#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "WaveformDisplay.h"
#include "CorrectionListView.h"
#include "SpectrogramDisplay.h"
#include "../Processors/BatchProcessor.h"
#include "../Processors/TrackDetector.h"
#include "../Utils/AudioFileManager.h"
#include "../DSP/ClickRemoval.h"
#include "../DSP/NoiseReduction.h"
#include "../DSP/FilterBank.h"
#include "../Utils/AudioUndoManager.h"

/**
 * Standalone Window for Audio Restoration Suite
 *
 * Full-featured audio editor window with:
 * - Menu bar (File, Edit, Process, View, Help)
 * - Toolbar with common actions
 * - Waveform display with zoom controls
 * - Correction list view
 * - Transport controls (play/pause/stop)
 * - Drag & drop support for audio files
 * - Session management
 *
 * Standalone mode only.
 */
class StandaloneWindow : public juce::DocumentWindow,
                         public juce::FileDragAndDropTarget,
                         public juce::MenuBarModel,
                         public juce::Timer,
                         public juce::ChangeListener
{
public:
    StandaloneWindow();
    ~StandaloneWindow() override;

    //==============================================================================
    // DocumentWindow overrides
    void closeButtonPressed() override;

    //==============================================================================
    // FileDragAndDropTarget overrides
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    //==============================================================================
    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    //==============================================================================
    // Timer callback for playback position updates
    void timerCallback() override;

    //==============================================================================
    // ChangeListener callback for transport state
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    //==============================================================================
    // Command IDs
    enum CommandIDs
    {
        fileOpen = 1,
        fileSave,
        fileSaveAs,
        fileExport,
        fileClose,
        fileRecentClear,
        fileExit,

        editUndo,
        editRedo,
        editSelectAll,
        editDeselect,

        processDetectClicks,
        processRemoveClicks,
        processNoiseReduction,
        processCutAndSplice,
        processGraphicEQ,
        processNormalise,
        processChannelBalance,
        processWowFlutterRemoval,
        processDropoutRestoration,
        processSpeedCorrection,
        processTurntableAnalyzer,
        processDetectTracks,
        processSplitTracks,
        processBatchProcess,

        viewZoomIn,
        viewZoomOut,
        viewZoomFit,
        viewShowCorrectionList,
        viewShowSpectrogram,

        transportPlay,
        transportPause,
        transportStop,

        optionsAudioSettings,

        helpAbout,
        helpDocumentation,

        // UI Scale options
        viewScale25 = 200,
        viewScale50,
        viewScale75,
        viewScale100,
        viewScale125,
        viewScale150,
        viewScale200,
        viewScale300,
        viewScale400
    };

    //==============================================================================
    // UI Components
    class MainComponent;
    class ScaledContentWrapper;
    std::unique_ptr<MainComponent> mainComponent;
    std::unique_ptr<ScaledContentWrapper> contentWrapper;

    juce::MenuBarComponent menuBar;

    //==============================================================================
    // Audio file management
    AudioFileManager fileManager;
    juce::File currentFile;        // The loaded audio file (.wav, .flac, etc.)
    juce::File currentSessionFile; // The session file (.vrs) if saved
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate = 44100.0;
    bool hasUnsavedChanges = false;

    //==============================================================================
    // Audio playback
    juce::AudioDeviceManager audioDeviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    bool isPlaying = false;

    void showAudioSettings();
    void setUIScale (float newScale);

    //==============================================================================
    // UI Scale
    float uiScaleFactor = 1.0f;
    int baseWidth = 1200;
    int baseHeight = 800;

    //==============================================================================
    // Recent files
    juce::RecentlyOpenedFilesList recentFiles;

    //==============================================================================
    // DSP Processors for audio restoration
    ClickRemoval clickRemovalProcessor;
    NoiseReduction noiseReductionProcessor;
    FilterBank filterBankProcessor;

    //==============================================================================
    // Undo/Redo management
    AudioUndoManager undoManager;

    //==============================================================================
    // File loading state (for progress dialog)
    double loadingProgress = 0.0;
    std::atomic<bool> loadingCancelled {false};
    juce::File loadingFile;

    //==============================================================================
    // Click removal settings (persistent across operations)
    float clickSensitivity = 60.0f;      // 0-100, higher = more sensitive
    int clickMaxWidth = 500;              // Max samples to correct
    int clickRemovalMethod = 2;           // 0=Spline, 1=Crossfade, 2=Automatic

    //==============================================================================
    // Helper methods
    void openFile (const juce::File& file);
    void openFileWithProgress (const juce::File& file);
    void finishFileLoad (const juce::File& file);
    void closeFile();
    void saveFile (const juce::File& file);
    void exportFile();
    void detectClicks();
    void performClickDetection();  // Called after settings dialog
    void removeClicks();
    void applyNoiseReduction();
    void applyNoiseReductionWithSettings (float reductionDB, float profileStartSec, float profileLengthSec);
    void detectTracks();
    void splitTracks();
    void showBatchProcessor();
    void startBatchProcessing (const juce::Array<juce::File>& files, const BatchProcessor::Settings& settings);
    void showAboutDialog();
    void updateTitle();
    void loadSession (const juce::File& sessionFile);

    // Additional Process menu methods
    void cutAndSplice();
    void showGraphicEQ();
    void normalise();
    void channelBalance();
    void wowFlutterRemoval();
    void dropoutRestoration();
    void speedCorrection();
    void turntableAnalyzer();

    // View menu methods
    void showSpectrogram();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneWindow)
};

//==============================================================================
/**
 * Main content component for standalone window
 * Contains waveform display, correction list, and transport controls
 */
class StandaloneWindow::MainComponent : public juce::Component,
                                        public juce::Button::Listener,
                                        public juce::Slider::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void buttonClicked (juce::Button* button) override;
    void sliderValueChanged (juce::Slider* slider) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Keyboard handling for spacebar play/pause
    bool keyPressed (const juce::KeyPress& key) override;

    // Access to parent window for transport control
    void setParentWindow (StandaloneWindow* parent) { parentWindow = parent; }

    // Access to components
    WaveformDisplay& getWaveformDisplay() { return waveformDisplay; }
    CorrectionListView& getCorrectionListView() { return correctionListView; }

    void setAudioBuffer (const juce::AudioBuffer<float>* buffer, double sampleRate);
    void updatePlaybackPosition (double position);

private:
    StandaloneWindow* parentWindow = nullptr;
    WaveformDisplay waveformDisplay;
    CorrectionListView correctionListView;

    // Transport controls (retro cassette deck style)
    juce::TextButton rewindButton {"<<"};
    juce::TextButton playPauseButton {"Play"};  // Toggle between Play and Pause
    juce::TextButton stopButton {"Stop"};
    juce::TextButton forwardButton {">>"};
    juce::Slider positionSlider;
    juce::Label timeLabel;

    // Volume control
    juce::Slider volumeSlider;
    juce::Label volumeLabel;

    // Zoom controls
    juce::TextButton zoomInButton {"+"};
    juce::TextButton zoomOutButton {"-"};
    juce::TextButton zoomFitButton {"Fit"};

    // Toolbar buttons for quick access
    juce::TextButton toolbarOpenButton {"Open"};
    juce::TextButton toolbarSaveButton {"Save"};
    juce::TextButton toolbarUndoButton {"Undo"};
    juce::TextButton toolbarRedoButton {"Redo"};
    juce::TextButton toolbarDetectButton {"Detect"};
    juce::TextButton toolbarRemoveButton {"Remove"};
    juce::TextButton toolbarNoiseButton {"Noise"};
    juce::TextButton toolbarEQButton {"EQ"};
    juce::TextButton toolbarSpectrumButton {"Spectrum"};
    juce::TextButton toolbarSettingsButton {"Settings"};

    // Zoom sliders
    juce::Slider horizontalZoomSlider;  // Below waveform
    juce::Slider verticalZoomSlider;    // Next to waveform
    juce::Label horizontalZoomLabel;
    juce::Label verticalZoomLabel;

    // Selection editor
    juce::Label selectionStartLabel;
    juce::Label selectionEndLabel;
    juce::Label selectionLengthLabel;
    juce::TextEditor selectionStartEditor;
    juce::TextEditor selectionEndEditor;
    juce::TextButton clearSelectionButton {"Clear"};

    // Status
    juce::Label statusLabel;

    // Logo
    juce::Image logoImage;

    // Custom resizer bar for waveform/correction list split
    class ResizeDivider : public juce::Component
    {
    public:
        std::function<void(int)> onDrag;

        ResizeDivider()
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            // Draw a subtle horizontal line with grip indicators
            auto bounds = getLocalBounds();
            g.setColour (juce::Colour (0xff555555));
            g.fillRect (bounds.reduced (0, 2));

            // Draw grip dots in center
            g.setColour (juce::Colour (0xff888888));
            int cx = bounds.getCentreX();
            int cy = bounds.getCentreY();
            for (int i = -2; i <= 2; ++i)
                g.fillEllipse ((float) (cx + i * 12 - 2), (float) (cy - 2), 4.0f, 4.0f);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            dragStartY = getY();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (onDrag)
                onDrag (e.getDistanceFromDragStartY());
        }

    private:
        int dragStartY = 0;
    };

    ResizeDivider resizeDivider;
    int waveformHeight = 300;  // Absolute height in pixels

    const juce::AudioBuffer<float>* currentBuffer = nullptr;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
/**
 * Wrapper component that handles UI scaling - now uses responsive layout
 */
class StandaloneWindow::ScaledContentWrapper : public juce::Component
{
public:
    ScaledContentWrapper (MainComponent* content, int /*baseW*/, int /*baseH*/)
        : contentComponent (content)
    {
        addAndMakeVisible (content);
    }

    void setScaleFactor (float newScale)
    {
        scaleFactor = newScale;
        // For now, ignore scale factor - use responsive layout instead
        resized();
    }

    float getScaleFactor() const { return scaleFactor; }

    void resized() override
    {
        // Make content fill the entire wrapper - responsive layout
        if (contentComponent != nullptr)
        {
            contentComponent->setTransform (juce::AffineTransform()); // Clear any transform
            contentComponent->setBounds (getLocalBounds());
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2e2e2e));
    }

private:
    MainComponent* contentComponent;
    float scaleFactor = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaledContentWrapper)
};
