#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "WaveformDisplay.h"
#include "CorrectionListView.h"
#include "SpectrogramDisplay.h"
#include "ReaperLookAndFeel.h"
#include "../Processors/BatchProcessor.h"
#include "../Processors/TrackDetector.h"
#include "../Utils/AudioFileManager.h"
#include "../DSP/ClickRemoval.h"
#include "../DSP/Decrackle.h"
#include "../DSP/NoiseReduction.h"
#include "../DSP/FilterBank.h"
#include "../DSP/OnnxDenoiser.h"
#include "../Utils/AudioUndoManager.h"
#include "../Utils/SettingsManager.h"
#include <array>

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
    void requestAppQuit();

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
        fileRecord,
        fileClose,
        fileRecentClear,
        fileExit,

        editUndo,
        editRedo,
        editSelectAll,
        editDeselect,

        processDetectClicks,
        processRemoveClicks,
        processDecrackle,
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
        optionsProcessingSettings,
        optionsAIDenoise,

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
    std::unique_ptr<MainComponent> mainComponent;

    ReaperLookAndFeel reaperLookAndFeel;
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
    class AudioRecorder;
    std::unique_ptr<AudioRecorder> recorder;
    bool isPlaying = false;
    bool isRecording = false;
    bool monitoringEnabled = true;
    juce::MemoryBlock bufferedRecording;
    float meterLevelLeft = 0.0f;
    float meterLevelRight = 0.0f;
    bool showCorrectionList = true;
    bool quitAfterExport = false;
    juce::TooltipWindow tooltipWindow {this, 500};
    bool aiDenoiseEnabled = false;

    void showAudioSettings();
    void showProcessingSettings();
    void setUIScale (float newScale);
    void applyDenoiserSettings();
    void toggleRecording();
    void toggleMonitoring();
    void startRecording();
    void stopRecording();
    void setRecordingState (bool recording);
    void loadRecordingFromMemory (juce::MemoryBlock&& data);

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
    Decrackle decrackleProcessor;
    NoiseReduction noiseReductionProcessor;
    FilterBank filterBankProcessor;
    TrackDetector trackDetector;
    class DenoiseAudioSource;
    std::unique_ptr<DenoiseAudioSource> denoiseSource;
    OnnxDenoiser realtimeDenoiser;

    //==============================================================================
    // Undo/Redo management
    AudioUndoManager undoManager;

    //==============================================================================
    // Batch processing state
    std::unique_ptr<BatchProcessor> activeBatchProcessor;
    juce::File lastBatchOutputDirectory;

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
    std::array<float, 10> lastEqGains {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};

    //==============================================================================
    // Helper methods
    void openFile (const juce::File& file);
    void openFileWithProgress (const juce::File& file);
    void finishFileLoad (const juce::File& file);
    void closeFile();
    bool saveFile (const juce::File& file);
    void exportFile();
    bool promptToSaveIfNeeded (const juce::String& actionName);
    bool saveCurrentSessionForPrompt();
    void detectClicks();
    void performClickDetection();  // Called after settings dialog
    void removeClicks();
    void applyDecrackle();
    void applyNoiseReduction();
    void applyNoiseReductionWithSettings (float reductionDB,
                                          float profileStartSec,
                                          float profileLengthSec,
                                          float adaptiveRate,
                                          int processStartSample,
                                          int processEndSample);
    void detectTracks();
    void splitTracks();
    void showBatchProcessor();
    void startBatchProcessing (const juce::Array<juce::File>& files, const BatchProcessor::Settings& settings);
    void showAboutDialog();
    void updateTitle();
    void loadSession (const juce::File& sessionFile);
    bool fetchDiscogsMetadata (const juce::String& discogsUrl,
                               const juce::String& token,
                               juce::String& albumTitle,
                               juce::String& artistName,
                               juce::String& year,
                               juce::StringArray& trackNames,
                               juce::String& errorMessage);

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

    struct ProcessingRange
    {
        int start = 0;
        int end = 0;
        bool hasSelection = false;
        juce::String rangeInfo = "whole file";
    };

    ProcessingRange getProcessingRange() const;

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

    void zoomIn();
    void zoomOut();
    void zoomFit();

    // Access to parent window for transport control
    void setParentWindow (StandaloneWindow* parent) { parentWindow = parent; }

    // Access to components
    WaveformDisplay& getWaveformDisplay() { return waveformDisplay; }
    CorrectionListView& getCorrectionListView() { return correctionListView; }
    bool isLoopSelectionEnabled() const { return loopSelectionButton.getToggleState(); }

    void setAudioBuffer (const juce::AudioBuffer<float>* buffer, double sampleRate);
    void updatePlaybackPosition (double position);
    void setMeterLevel (float leftLevel, float rightLevel);
    void setCorrectionListVisible (bool visible);
    void setRecording (bool recording);
    void setTransportTime (double seconds);

private:
    StandaloneWindow* parentWindow = nullptr;
    WaveformDisplay waveformDisplay;
    CorrectionListView correctionListView;

    // Transport controls (retro cassette deck style)
    juce::TextButton rewindButton {"<<"};
    juce::TextButton playPauseButton {"Play"};  // Toggle between Play and Pause
    juce::TextButton stopButton {"Stop"};
    juce::TextButton recordButton {"Rec"};
    juce::ToggleButton monitorButton {"Mon"};
    juce::TextButton forwardButton {">>"};
    juce::ToggleButton loopSelectionButton {"Loop Sel"};
    juce::TextButton zoomToSelectionButton {"Zoom Sel"};
    juce::Slider positionSlider;
    juce::Label timeLabel;

    // Volume control
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    class LedMeter : public juce::Component
    {
    public:
        void setLevels (float leftLevel, float rightLevel)
        {
            left = juce::jlimit (0.0f, 1.0f, leftLevel);
            right = juce::jlimit (0.0f, 1.0f, rightLevel);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();
            g.fillAll (juce::Colour (0xff1b1b1b));

            const int segments = 12;
            const int gap = 2;
            int segmentHeight = (bounds.getHeight() - (segments - 1) * gap) / segments;
            int litLeft = juce::roundToInt (left * segments);
            int litRight = juce::roundToInt (right * segments);
            int meterGap = 2;
            int columnWidth = (bounds.getWidth() - meterGap) / 2;
            auto leftBounds = bounds.withWidth (columnWidth);
            auto rightBounds = bounds.withX (bounds.getX() + columnWidth + meterGap)
                                   .withWidth (bounds.getWidth() - columnWidth - meterGap);

            for (int i = 0; i < segments; ++i)
            {
                int y = bounds.getBottom() - (i + 1) * segmentHeight - i * gap;
                juce::Rectangle<int> leftSegment (leftBounds.getX(), y, leftBounds.getWidth(), segmentHeight);
                juce::Rectangle<int> rightSegment (rightBounds.getX(), y, rightBounds.getWidth(), segmentHeight);

                bool litL = i < litLeft;
                bool litR = i < litRight;

                if (litL)
                {
                    if (i > 9)
                        g.setColour (juce::Colours::red);
                    else if (i > 7)
                        g.setColour (juce::Colours::orange);
                    else
                        g.setColour (juce::Colours::green);
                }
                else
                {
                    g.setColour (juce::Colour (0xff2a2a2a));
                }

                g.fillRect (leftSegment);

                if (litR)
                {
                    if (i > 9)
                        g.setColour (juce::Colours::red);
                    else if (i > 7)
                        g.setColour (juce::Colours::orange);
                    else
                        g.setColour (juce::Colours::green);
                }
                else
                {
                    g.setColour (juce::Colour (0xff2a2a2a));
                }

                g.fillRect (rightSegment);
            }

            g.setColour (juce::Colour (0xff3a3a3a));
            g.drawRect (bounds, 1);
        }

    private:
        float left = 0.0f;
        float right = 0.0f;
    };
    LedMeter volumeMeter;

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
    bool correctionListVisible = true;

    bool seekToSelectionStart();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

