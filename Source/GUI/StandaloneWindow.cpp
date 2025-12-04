#include "StandaloneWindow.h"

//==============================================================================
// StandaloneWindow Implementation
//==============================================================================

StandaloneWindow::StandaloneWindow()
    : DocumentWindow ("Vinyl Restoration Suite",
                      juce::Colour (0xff2e2e2e),
                      DocumentWindow::allButtons),
      menuBar (this)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);

    // Initialize audio device
    juce::String audioError = audioDeviceManager.initialise (
        0,     // number of input channels
        2,     // number of output channels
        nullptr,  // saved state
        true   // select default device on failure
    );

    if (audioError.isNotEmpty())
    {
        DBG ("Audio device error: " + audioError);
    }

    // Setup audio transport
    transportSource.addChangeListener (this);
    audioSourcePlayer.setSource (&transportSource);
    audioDeviceManager.addAudioCallback (&audioSourcePlayer);

    // Create main component
    mainComponent = std::make_unique<MainComponent>();
    mainComponent->setParentWindow (this);

    // Wire up waveform double-click to seek playback
    mainComponent->getWaveformDisplay().onSeekPosition = [this](double position)
    {
        if (transportSource.getTotalLength() > 0.0)
        {
            double timeInSeconds = position * transportSource.getLengthInSeconds();
            transportSource.setPosition (timeInSeconds);
            DBG ("Seek to: " + juce::String (timeInSeconds) + " seconds");
        }
    };

    // Add menu bar to main component using base class method
    mainComponent->Component::addAndMakeVisible (menuBar);

    // Create scaled content wrapper
    contentWrapper = std::make_unique<ScaledContentWrapper> (mainComponent.get(), baseWidth, baseHeight);
    setContentNonOwned (contentWrapper.get(), true);

    // Set initial size
    centreWithSize (1200, 800);

    // Load recent files
    recentFiles.setMaxNumberOfItems (10);

    // Create settings directory if it doesn't exist
    auto settingsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("VinylRestorationSuite");
    settingsDir.createDirectory();

    // Load recent files from settings
    auto settingsFile = settingsDir.getChildFile ("recent_files.txt");
    if (settingsFile.existsAsFile())
    {
        recentFiles.restoreFromString (settingsFile.loadFileAsString());
        DBG ("Loaded " + juce::String (recentFiles.getNumFiles()) + " recent files from: " + settingsFile.getFullPathName());
    }
    else
    {
        DBG ("Recent files not found at: " + settingsFile.getFullPathName());
    }

    // Start timer for playback position updates
    startTimer (40); // 25 fps

    setVisible (true);
}

StandaloneWindow::~StandaloneWindow()
{
    // Save recent files to settings
    auto settingsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("VinylRestorationSuite");
    settingsDir.createDirectory();
    auto settingsFile = settingsDir.getChildFile ("recent_files.txt");
    settingsFile.replaceWithText (recentFiles.toString());
    DBG ("Saved " + juce::String (recentFiles.getNumFiles()) + " recent files");

    // Stop audio playback
    transportSource.setSource (nullptr);
    audioSourcePlayer.setSource (nullptr);
    audioDeviceManager.removeAudioCallback (&audioSourcePlayer);
    transportSource.removeChangeListener (this);

    setMenuBar (nullptr);
}

void StandaloneWindow::closeButtonPressed()
{
    if (hasUnsavedChanges)
    {
        int result = juce::AlertWindow::showYesNoCancelBox (
            juce::AlertWindow::QuestionIcon,
            "Unsaved Changes",
            "Do you want to save your changes before closing?",
            "Save", "Don't Save", "Cancel",
            nullptr, nullptr
        );

        if (result == 0) // Cancel
            return;
        else if (result == 1) // Save
        {
            if (currentFile.exists())
            {
                saveFile (currentFile);
            }
            else
            {
                // No file exists, show save dialog
                exportFile();
                return; // exportFile is async, will handle quit after save
            }
        }
        // result == 2 means "Don't Save" - fall through to quit
    }

    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

//==============================================================================
// Drag and Drop
//==============================================================================

bool StandaloneWindow::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        juce::File f (file);
        if (f.hasFileExtension ("wav;flac;aiff;mp3;ogg"))
            return true;
    }
    return false;
}

void StandaloneWindow::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0)
    {
        juce::File file (files[0]);
        openFile (file);
    }
}

//==============================================================================
// Menu Bar
//==============================================================================

juce::StringArray StandaloneWindow::getMenuBarNames()
{
    return {"File", "Edit", "Process", "View", "Options", "Help"};
}

juce::PopupMenu StandaloneWindow::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    if (topLevelMenuIndex == 0) // File
    {
        menu.addItem (fileOpen, "Open...                   Ctrl+O");
        menu.addItem (fileClose, "Close File                Ctrl+W", currentFile.exists());
        menu.addSeparator();
        menu.addItem (fileSave, "Save                      Ctrl+S", currentFile.exists());
        menu.addItem (fileSaveAs, "Save As...             Ctrl+Shift+S");
        menu.addSeparator();
        menu.addItem (fileExport, "Export Audio...           Ctrl+E", audioBuffer.getNumSamples() > 0);
        menu.addSeparator();

        // Recent files submenu
        juce::PopupMenu recentMenu;
        recentFiles.createPopupMenuItems (recentMenu, 100, true, true);
        menu.addSubMenu ("Recent Files", recentMenu);
        menu.addItem (fileRecentClear, "Clear Recent Files");

        menu.addSeparator();
        menu.addItem (fileExit, "Exit                      Ctrl+Q");
    }
    else if (topLevelMenuIndex == 1) // Edit
    {
        bool hasAudio = audioBuffer.getNumSamples() > 0;
        juce::String undoText = "Undo";
        juce::String redoText = "Redo";
        if (undoManager.canUndo())
            undoText = "Undo " + undoManager.getUndoDescription();
        if (undoManager.canRedo())
            redoText = "Redo " + undoManager.getRedoDescription();
        menu.addItem (editUndo, undoText + "                      Ctrl+Z", undoManager.canUndo());
        menu.addItem (editRedo, redoText + "                      Ctrl+Y", undoManager.canRedo());
        menu.addSeparator();
        menu.addItem (editSelectAll, "Select All                Ctrl+A", hasAudio);
        menu.addItem (editDeselect, "Deselect                  Ctrl+D", hasAudio);
    }
    else if (topLevelMenuIndex == 2) // Process
    {
        bool hasAudio = audioBuffer.getNumSamples() > 0;
        menu.addItem (processDetectClicks, "Detect Clicks", hasAudio);
        menu.addItem (processRemoveClicks, "Remove Clicks", hasAudio);
        menu.addSeparator();
        menu.addItem (processNoiseReduction, "Noise Reduction...", hasAudio);
        menu.addSeparator();
        menu.addItem (processCutAndSplice, "Cut and Splice...", hasAudio);
        menu.addSeparator();
        menu.addItem (processGraphicEQ, "Graphic Equaliser...", hasAudio);
        menu.addSeparator();
        menu.addItem (processNormalise, "Normalise...", hasAudio);
        menu.addItem (processChannelBalance, "Channel Balance...", hasAudio);
        menu.addSeparator();
        menu.addItem (processWowFlutterRemoval, "Wow & Flutter Removal...", hasAudio);
        menu.addItem (processDropoutRestoration, "Dropout Restoration...", hasAudio);
        menu.addItem (processSpeedCorrection, "Speed Correction...", hasAudio);
        menu.addSeparator();
        menu.addItem (processTurntableAnalyzer, "Turntable Analyzer...", hasAudio);
        menu.addSeparator();
        menu.addItem (processDetectTracks, "Detect Tracks", hasAudio);
        menu.addItem (processSplitTracks, "Split Tracks...", hasAudio);
        menu.addSeparator();
        menu.addItem (processBatchProcess, "Batch Process...");
    }
    else if (topLevelMenuIndex == 3) // View
    {
        menu.addItem (viewZoomIn, "Zoom In                   Ctrl++");
        menu.addItem (viewZoomOut, "Zoom Out                  Ctrl+-");
        menu.addItem (viewZoomFit, "Zoom to Fit               Ctrl+0");
        menu.addSeparator();

        // UI Scale submenu
        juce::PopupMenu scaleMenu;
        scaleMenu.addItem (viewScale25, "25%", true, uiScaleFactor == 0.25f);
        scaleMenu.addItem (viewScale50, "50%", true, uiScaleFactor == 0.50f);
        scaleMenu.addItem (viewScale75, "75%", true, uiScaleFactor == 0.75f);
        scaleMenu.addItem (viewScale100, "100%", true, uiScaleFactor == 1.00f);
        scaleMenu.addItem (viewScale125, "125%", true, uiScaleFactor == 1.25f);
        scaleMenu.addItem (viewScale150, "150%", true, uiScaleFactor == 1.50f);
        scaleMenu.addItem (viewScale200, "200%", true, uiScaleFactor == 2.00f);
        scaleMenu.addItem (viewScale300, "300%", true, uiScaleFactor == 3.00f);
        scaleMenu.addItem (viewScale400, "400%", true, uiScaleFactor == 4.00f);
        menu.addSubMenu ("UI Scale", scaleMenu);

        menu.addSeparator();
        menu.addItem (viewShowCorrectionList, "Show Correction List", true, true);
        menu.addItem (viewShowSpectrogram, "Show Spectrogram...      Ctrl+G", audioBuffer.getNumSamples() > 0);
    }
    else if (topLevelMenuIndex == 4) // Options
    {
        menu.addItem (optionsAudioSettings, "Audio Settings...");
    }
    else if (topLevelMenuIndex == 5) // Help
    {
        menu.addItem (helpAbout, "About");
        menu.addItem (helpDocumentation, "Documentation", false);
    }

    return menu;
}

void StandaloneWindow::menuItemSelected (int menuItemID, int)
{
    // Handle recent files
    if (menuItemID >= 100 && menuItemID < 200)
    {
        juce::File file = recentFiles.getFile (menuItemID - 100);
        if (file.exists())
            openFile (file);
        return;
    }

    // Handle menu commands
    switch (menuItemID)
    {
        case fileOpen:
        {
            auto chooser = std::make_shared<juce::FileChooser> ("Open Audio File or Session",
                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                       "*.wav;*.flac;*.aiff;*.ogg;*.mp3;*.vrs");

            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser] (const juce::FileChooser&)
                {
                    auto file = chooser->getResult();
                    if (file.existsAsFile())
                    {
                        if (file.hasFileExtension ("vrs"))
                            loadSession (file);
                        else
                            openFile (file);
                    }
                });
            break;
        }

        case fileClose:
            closeFile();
            break;

        case fileSave:
            // If we have a session file, save to it; otherwise show Save As dialog
            if (currentSessionFile.exists())
            {
                saveFile (currentSessionFile);
            }
            else if (currentFile.exists())
            {
                // No session yet - show Save As dialog with suggested name based on audio file
                auto suggestedFile = currentFile.withFileExtension ("vrs");
                auto chooser = std::make_shared<juce::FileChooser> ("Save Session",
                                           suggestedFile.getParentDirectory(),
                                           "*.vrs");

                chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, chooser, suggestedFile] (const juce::FileChooser&)
                    {
                        auto file = chooser->getResult();
                        if (file != juce::File())
                            saveFile (file);
                    });
            }
            break;

        case fileSaveAs:
        {
            // Use session file directory if exists, otherwise audio file directory
            juce::File startDir = currentSessionFile.exists() ? currentSessionFile.getParentDirectory()
                                : currentFile.exists() ? currentFile.getParentDirectory()
                                : juce::File::getSpecialLocation (juce::File::userHomeDirectory);
            auto chooser = std::make_shared<juce::FileChooser> ("Save Session",
                                       startDir,
                                       "*.vrs");

            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser] (const juce::FileChooser&)
                {
                    auto file = chooser->getResult();
                    if (file != juce::File())
                        saveFile (file);
                });
            break;
        }

        case fileExport:
            exportFile();
            break;

        case fileRecentClear:
            recentFiles.clear();
            menuItemsChanged();
            break;

        case fileExit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        // Edit menu
        case editUndo:
            if (undoManager.canUndo())
            {
                juce::String desc = undoManager.getUndoDescription();
                if (undoManager.undo (audioBuffer, sampleRate))
                {
                    mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);
                    hasUnsavedChanges = true;
                    updateTitle();
                    DBG ("Undo: " + desc);
                }
            }
            break;

        case editRedo:
            if (undoManager.canRedo())
            {
                juce::String desc = undoManager.getRedoDescription();
                if (undoManager.redo (audioBuffer, sampleRate))
                {
                    mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);
                    hasUnsavedChanges = true;
                    updateTitle();
                    DBG ("Redo: " + desc);
                }
            }
            break;

        case editSelectAll:
            if (audioBuffer.getNumSamples() > 0)
            {
                mainComponent->getWaveformDisplay().setSelection (0, audioBuffer.getNumSamples());
            }
            break;

        case editDeselect:
            mainComponent->getWaveformDisplay().clearSelection();
            break;

        case processDetectClicks:
            detectClicks();
            break;

        case processRemoveClicks:
            removeClicks();
            break;

        case processNoiseReduction:
            applyNoiseReduction();
            break;

        case processCutAndSplice:
            cutAndSplice();
            break;

        case processGraphicEQ:
            showGraphicEQ();
            break;

        case processNormalise:
            normalise();
            break;

        case processChannelBalance:
            channelBalance();
            break;

        case processWowFlutterRemoval:
            wowFlutterRemoval();
            break;

        case processDropoutRestoration:
            dropoutRestoration();
            break;

        case processSpeedCorrection:
            speedCorrection();
            break;

        case processTurntableAnalyzer:
            turntableAnalyzer();
            break;

        case processDetectTracks:
            detectTracks();
            break;

        case processSplitTracks:
            splitTracks();
            break;

        case processBatchProcess:
            showBatchProcessor();
            break;

        case optionsAudioSettings:
            showAudioSettings();
            break;

        case helpAbout:
            showAboutDialog();
            break;

        // UI Scale options
        case viewScale25:  setUIScale (0.25f); break;
        case viewScale50:  setUIScale (0.50f); break;
        case viewScale75:  setUIScale (0.75f); break;
        case viewScale100: setUIScale (1.00f); break;
        case viewScale125: setUIScale (1.25f); break;
        case viewScale150: setUIScale (1.50f); break;
        case viewScale200: setUIScale (2.00f); break;
        case viewScale300: setUIScale (3.00f); break;
        case viewScale400: setUIScale (4.00f); break;

        case viewShowSpectrogram:
            showSpectrogram();
            break;

        default:
            break;
    }
}

//==============================================================================
// Timer callback
//==============================================================================

void StandaloneWindow::timerCallback()
{
    // Update playback position if playing
    if (transportSource.isPlaying())
    {
        double position = transportSource.getCurrentPosition() / transportSource.getLengthInSeconds();
        mainComponent->updatePlaybackPosition (position);
    }
}

void StandaloneWindow::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Transport state changed (started/stopped)
    if (transportSource.hasStreamFinished())
    {
        transportSource.setPosition (0.0);
        isPlaying = false;
    }
}

//==============================================================================
// Helper Methods
//==============================================================================

void StandaloneWindow::openFile (const juce::File& file)
{
    if (!file.exists())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "File Not Found",
                                                "The selected file does not exist.");
        return;
    }

    auto fileSizeMB = file.getSize() / (1024.0 * 1024.0);

    // For large files (>50MB), use progress dialog
    if (fileSizeMB > 50.0)
    {
        openFileWithProgress (file);
        return;
    }

    // For smaller files, load directly with status update
    mainComponent->getCorrectionListView().setStatusText (
        "Loading " + file.getFileName() + " (" +
        juce::String (fileSizeMB, 1) + " MB)..."
    );

    // Load audio file
    if (fileManager.loadAudioFile (file, audioBuffer, sampleRate))
    {
        finishFileLoad (file);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Error",
                                                "Failed to load audio file.");
        mainComponent->getCorrectionListView().setStatusText ("Ready");
    }
}

void StandaloneWindow::openFileWithProgress (const juce::File& file)
{
    // Show loading status with file info
    auto fileSizeMB = file.getSize() / (1024.0 * 1024.0);
    mainComponent->getCorrectionListView().setStatusText (
        "Loading " + file.getFileName() + " (" +
        juce::String (fileSizeMB, 1) + " MB)..."
    );

    // Force repaint to show status
    mainComponent->getCorrectionListView().repaint();

    loadingProgress = 0.0;
    loadingFile = file;

    // Track last update time for throttling
    auto lastUpdateTime = juce::Time::getMillisecondCounter();

    // Load with progress updates to status bar
    bool success = fileManager.loadAudioFileWithProgress (
        file, audioBuffer, sampleRate,
        [this, &lastUpdateTime] (double prog, const juce::String&) -> bool
        {
            loadingProgress = prog;

            // Throttle UI updates to every 100ms
            auto now = juce::Time::getMillisecondCounter();
            if (now - lastUpdateTime > 100)
            {
                lastUpdateTime = now;
                int percent = static_cast<int> (prog * 100.0);
                mainComponent->getCorrectionListView().setStatusText (
                    "Loading... " + juce::String (percent) + "%"
                );
                mainComponent->getCorrectionListView().repaint();
            }

            return true;  // Continue loading
        }
    );

    if (success)
    {
        finishFileLoad (file);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Error",
                                                "Failed to load audio file.");
        mainComponent->getCorrectionListView().setStatusText ("Ready");
    }
}

void StandaloneWindow::finishFileLoad (const juce::File& file)
{
    currentFile = file;
    currentSessionFile = juce::File();  // Clear session file when loading new audio
    recentFiles.addFile (file);

    // Save recent files immediately (in case app crashes later)
    auto settingsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("VinylRestorationSuite");
    settingsDir.createDirectory();
    auto settingsFile = settingsDir.getChildFile ("recent_files.txt");
    settingsFile.replaceWithText (recentFiles.toString());
    DBG ("Saved recent files: " + juce::String (recentFiles.getNumFiles()) + " items");

    // Update waveform display
    mainComponent->getWaveformDisplay().loadFile (file);
    mainComponent->setAudioBuffer (&audioBuffer, sampleRate);

    // Load into transport source for playback
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    formatManager.registerFormat (new juce::FlacAudioFormat(), true);
    formatManager.registerFormat (new juce::OggVorbisAudioFormat(), true);

    #if JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat (new juce::MP3AudioFormat(), true);
    #endif

    auto* reader = formatManager.createReaderFor (file);

    if (reader != nullptr)
    {
        readerSource.reset (new juce::AudioFormatReaderSource (reader, true));
        transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    }

    // Clear corrections and undo history
    mainComponent->getCorrectionListView().clearCorrections();
    undoManager.clear();

    hasUnsavedChanges = false;
    updateTitle();

    DBG ("Opened file: " + file.getFullPathName());

    auto durationSec = audioBuffer.getNumSamples() / sampleRate;
    juce::String status = "Loaded: " + juce::String (durationSec / 60.0, 1) + " min, " +
                          juce::String (sampleRate / 1000.0, 1) + " kHz";
    mainComponent->getCorrectionListView().setStatusText (status);
}


void StandaloneWindow::closeFile()
{
    // Stop playback
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();
    isPlaying = false;

    // Clear audio buffer
    audioBuffer.setSize (0, 0);
    currentFile = juce::File();
    currentSessionFile = juce::File();

    // Clear displays
    mainComponent->getWaveformDisplay().clear();
    mainComponent->getCorrectionListView().clearCorrections();
    mainComponent->setAudioBuffer (nullptr, sampleRate);

    // Reset state
    hasUnsavedChanges = false;
    updateTitle();

    mainComponent->getCorrectionListView().setStatusText ("No file loaded");
    DBG ("Closed file");
}

void StandaloneWindow::saveFile (const juce::File& file)
{
    // Ensure the file has .vrs extension
    juce::File sessionFile = file;
    if (!sessionFile.hasFileExtension ("vrs"))
        sessionFile = sessionFile.withFileExtension ("vrs");

    // Save session (corrections, settings, etc.)
    juce::var sessionData;
    // TODO: Build session data from current state

    if (fileManager.saveSession (sessionFile, currentFile, sessionData))
    {
        currentSessionFile = sessionFile;  // Track the session file
        recentFiles.addFile (sessionFile); // Add to recent files

        // Save recent files immediately
        auto settingsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("VinylRestorationSuite");
        settingsDir.createDirectory();
        settingsDir.getChildFile ("recent_files.txt").replaceWithText (recentFiles.toString());

        hasUnsavedChanges = false;
        updateTitle();
        DBG ("Saved session: " + sessionFile.getFullPathName());
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Save Failed",
                                                "Failed to save session file.");
    }
}

void StandaloneWindow::exportFile()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Export Audio",
                               currentFile.getParentDirectory(),
                               "*.wav;*.flac;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser&)
        {
            juce::File outputFile = chooser->getResult();

            if (outputFile != juce::File())
            {
                if (fileManager.saveAudioFile (outputFile, audioBuffer, sampleRate, 24))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                            "Export Complete",
                                                            "Audio exported successfully.");
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                            "Export Failed",
                                                            "Failed to export audio file.");
                }
            }
        });
}

void StandaloneWindow::detectClicks()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create click detection settings dialog
    auto* dialog = new juce::AlertWindow ("Click Detection Settings",
                                          "Configure click detection parameters:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextEditor ("sensitivity", juce::String (clickSensitivity, 0), "Sensitivity (0-100):");
    dialog->addTextEditor ("maxWidth", juce::String (clickMaxWidth), "Max Click Width (samples):");
    dialog->addComboBox ("method", {"Spline Interpolation", "Crossfade Smoothing", "Automatic"}, "Removal Method:");
    dialog->getComboBoxComponent ("method")->setSelectedId (clickRemovalMethod + 1);

    dialog->addButton ("Detect", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                // Get settings from dialog
                clickSensitivity = dialog->getTextEditorContents ("sensitivity").getFloatValue();
                clickSensitivity = juce::jlimit (0.0f, 100.0f, clickSensitivity);
                clickMaxWidth = dialog->getTextEditorContents ("maxWidth").getIntValue();
                clickMaxWidth = juce::jlimit (10, 2000, clickMaxWidth);
                clickRemovalMethod = dialog->getComboBoxComponent ("method")->getSelectedId() - 1;

                // Now perform the actual detection
                performClickDetection();
            }
            delete dialog;
        }), true);
}

void StandaloneWindow::performClickDetection()
{
    // Check if there's a selection - if so, only detect in selection
    auto& waveform = mainComponent->getWaveformDisplay();
    int64_t selStart = -1, selEnd = -1;
    waveform.getSelection (selStart, selEnd);

    int scanStart = 0;
    int scanEnd = audioBuffer.getNumSamples();
    juce::String rangeInfo = "whole file";

    if (selStart >= 0 && selEnd > selStart)
    {
        scanStart = static_cast<int> (selStart);
        scanEnd = static_cast<int> (juce::jmin (selEnd, (int64_t) audioBuffer.getNumSamples()));
        double startSec = scanStart / sampleRate;
        double endSec = scanEnd / sampleRate;
        rangeInfo = juce::String (startSec, 2) + "s - " + juce::String (endSec, 2) + "s";
    }

    mainComponent->getCorrectionListView().setStatusText ("Detecting clicks in " + rangeInfo + "...");
    DBG ("Starting click detection on " + juce::String (scanEnd - scanStart) + " samples (" + rangeInfo + ")");
    DBG ("Settings: sensitivity=" + juce::String (clickSensitivity) + ", maxWidth=" + juce::String (clickMaxWidth) + ", method=" + juce::String (clickRemovalMethod));

    // Configure click detection
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = static_cast<juce::uint32> (audioBuffer.getNumChannels());
    spec.maximumBlockSize = 2048;

    clickRemovalProcessor.prepare (spec);
    clickRemovalProcessor.reset();  // Clear previous detections
    clickRemovalProcessor.setSensitivity (clickSensitivity);
    clickRemovalProcessor.setMaxWidth (clickMaxWidth);
    clickRemovalProcessor.setRemovalMethod (static_cast<ClickRemoval::RemovalMethod> (clickRemovalMethod));
    clickRemovalProcessor.setStoreDetectedClicks (true);  // IMPORTANT: Store clicks for GUI display
    clickRemovalProcessor.setApplyRemoval (false);  // Detection only - don't modify audio
    clickRemovalProcessor.resetSamplePosition();   // Reset position counter
    clickRemovalProcessor.setSampleOffset (scanStart);  // Set offset for correct position reporting

    // Create a copy of the buffer for detection (non-destructive scan)
    juce::AudioBuffer<float> scanBuffer;
    scanBuffer.makeCopyOf (audioBuffer);

    // Process in blocks to detect clicks
    const int blockSize = 2048;
    int totalClicks = 0;
    int totalSamples = scanEnd - scanStart;
    int lastProgressPercent = 0;

    for (int startSample = scanStart; startSample < scanEnd; startSample += blockSize)
    {
        int samplesThisBlock = juce::jmin (blockSize, scanEnd - startSample);

        juce::dsp::AudioBlock<float> block (scanBuffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (scanBuffer.getNumChannels()),
                                            static_cast<size_t> (startSample),
                                            static_cast<size_t> (samplesThisBlock));

        juce::dsp::ProcessContextReplacing<float> context (block);
        clickRemovalProcessor.process (context);

        totalClicks += clickRemovalProcessor.getClicksDetectedLastBlock();

        // Update progress every 5%
        int currentPercent = (int) (100.0 * (startSample - scanStart) / (double) totalSamples);
        if (currentPercent >= lastProgressPercent + 5)
        {
            lastProgressPercent = currentPercent;
            mainComponent->getCorrectionListView().setStatusText (
                "Detecting clicks... " + juce::String (currentPercent) + "% (" +
                juce::String (totalClicks) + " found)");
            repaint();
        }
    }

    DBG ("Total clicks detected during scan: " + juce::String (totalClicks));

    // Get detected clicks and add to correction list
    const auto& detectedClicks = clickRemovalProcessor.getDetectedClicks();

    DBG ("Clicks stored in vector: " + juce::String (detectedClicks.size()));

    mainComponent->getCorrectionListView().clearCorrections();
    mainComponent->getWaveformDisplay().clearClickMarkers();

    for (const auto& click : detectedClicks)
    {
        mainComponent->getCorrectionListView().addCorrection (
            click.position,
            click.magnitude,
            click.width,
            click.isManual ? "Manual" : "Auto",
            false
        );

        // Add marker to waveform display
        mainComponent->getWaveformDisplay().addClickMarker (click.position);
    }

    if (!detectedClicks.empty())
        hasUnsavedChanges = true;

    updateTitle();

    juce::String message = "Detected " + juce::String (detectedClicks.size()) + " clicks/pops.";
    mainComponent->getCorrectionListView().setStatusText (message);

    if (detectedClicks.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Click Detection Complete",
                                                "No clicks or pops detected in the audio.\n\n"
                                                "The audio appears to be clean, or try adjusting sensitivity.");
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Click Detection Complete",
                                                message + "\n\nUse 'Process > Remove Clicks' to apply corrections.");
    }
}

void StandaloneWindow::removeClicks()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Save state for undo
    undoManager.saveState (audioBuffer, sampleRate, "Click Removal");

    mainComponent->getCorrectionListView().setStatusText ("Removing clicks...");
    DBG ("Starting click removal on " + juce::String (audioBuffer.getNumSamples()) + " samples");

    // Configure click removal processor
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = static_cast<juce::uint32> (audioBuffer.getNumChannels());
    spec.maximumBlockSize = 2048;

    clickRemovalProcessor.prepare (spec);
    clickRemovalProcessor.reset();
    clickRemovalProcessor.setSensitivity (clickSensitivity);
    clickRemovalProcessor.setMaxWidth (clickMaxWidth);
    clickRemovalProcessor.setRemovalMethod (static_cast<ClickRemoval::RemovalMethod> (clickRemovalMethod));
    clickRemovalProcessor.setStoreDetectedClicks (false);  // Don't need to store, just remove
    clickRemovalProcessor.setApplyRemoval (true);  // Actually remove the clicks
    clickRemovalProcessor.resetSamplePosition();

    // Process the actual audio buffer (destructive operation)
    const int blockSize = 2048;
    int totalClicksRemoved = 0;

    for (int startSample = 0; startSample < audioBuffer.getNumSamples(); startSample += blockSize)
    {
        int samplesThisBlock = juce::jmin (blockSize, audioBuffer.getNumSamples() - startSample);

        juce::dsp::AudioBlock<float> block (audioBuffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (audioBuffer.getNumChannels()),
                                            static_cast<size_t> (startSample),
                                            static_cast<size_t> (samplesThisBlock));

        juce::dsp::ProcessContextReplacing<float> context (block);
        clickRemovalProcessor.process (context);

        totalClicksRemoved += clickRemovalProcessor.getClicksDetectedLastBlock();
    }

    DBG ("Total clicks removed: " + juce::String (totalClicksRemoved));

    // Mark corrections as applied in the list
    mainComponent->getCorrectionListView().markAllApplied();

    // Clear the click markers from waveform display (red lines)
    mainComponent->getWaveformDisplay().clearClickMarkers();

    // Clear the correction list as well since clicks are now removed
    mainComponent->getCorrectionListView().clearCorrections();

    // Update waveform display to show processed audio
    mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

    hasUnsavedChanges = true;
    updateTitle();

    juce::String message = "Removed " + juce::String (totalClicksRemoved) + " clicks/pops.";
    mainComponent->getCorrectionListView().setStatusText (message);

    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Click Removal Complete",
                                            message);
}

void StandaloneWindow::applyNoiseReduction()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create noise reduction settings dialog
    auto* dialog = new juce::AlertWindow ("Noise Reduction Settings",
                                          "Configure noise reduction parameters:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextEditor ("reduction", "12", "Reduction Amount (dB):");
    dialog->addTextEditor ("profileStart", "0", "Profile Start (seconds):");
    dialog->addTextEditor ("profileLength", "1", "Profile Length (seconds):");

    dialog->addButton ("Apply", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                float reductionDB = dialog->getTextEditorContents ("reduction").getFloatValue();
                float profileStart = dialog->getTextEditorContents ("profileStart").getFloatValue();
                float profileLength = dialog->getTextEditorContents ("profileLength").getFloatValue();

                reductionDB = juce::jlimit (0.0f, 24.0f, reductionDB);
                profileStart = juce::jmax (0.0f, profileStart);
                profileLength = juce::jmax (0.1f, profileLength);

                // Apply noise reduction on message thread (for GUI responsiveness)
                juce::MessageManager::callAsync ([this, reductionDB, profileStart, profileLength]()
                {
                    applyNoiseReductionWithSettings (reductionDB, profileStart, profileLength);
                });
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::applyNoiseReductionWithSettings (float reductionDB, float profileStartSec, float profileLengthSec)
{
    // Save state for undo
    undoManager.saveState (audioBuffer, sampleRate, "Noise Reduction");

    mainComponent->getCorrectionListView().setStatusText ("Applying noise reduction...");

    // Configure noise reduction processor
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.numChannels = static_cast<juce::uint32> (audioBuffer.getNumChannels());
    spec.maximumBlockSize = 2048;

    noiseReductionProcessor.prepare (spec);
    noiseReductionProcessor.setReduction (reductionDB);

    // Calculate profile sample range
    int profileStartSample = static_cast<int> (profileStartSec * sampleRate);
    int profileLengthSamples = static_cast<int> (profileLengthSec * sampleRate);
    profileStartSample = juce::jlimit (0, audioBuffer.getNumSamples() - 1, profileStartSample);
    profileLengthSamples = juce::jmin (profileLengthSamples, audioBuffer.getNumSamples() - profileStartSample);

    // Capture noise profile from specified section
    noiseReductionProcessor.captureProfile();

    // Feed profile section to the processor
    const int blockSize = 2048;
    for (int startSample = profileStartSample;
         startSample < profileStartSample + profileLengthSamples;
         startSample += blockSize)
    {
        int samplesThisBlock = juce::jmin (blockSize, profileStartSample + profileLengthSamples - startSample);

        juce::dsp::AudioBlock<float> block (audioBuffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (audioBuffer.getNumChannels()),
                                            static_cast<size_t> (startSample),
                                            static_cast<size_t> (samplesThisBlock));

        juce::dsp::ProcessContextReplacing<float> context (block);
        noiseReductionProcessor.process (context);

        if (noiseReductionProcessor.hasProfile())
            break;  // Profile captured
    }

    if (!noiseReductionProcessor.hasProfile())
    {
        mainComponent->getCorrectionListView().setStatusText ("Failed to capture noise profile");
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Noise Reduction Failed",
                                                "Could not capture noise profile. Try a different section.");
        return;
    }

    // Now process the entire audio buffer
    mainComponent->getCorrectionListView().setStatusText ("Processing audio...");

    for (int startSample = 0; startSample < audioBuffer.getNumSamples(); startSample += blockSize)
    {
        int samplesThisBlock = juce::jmin (blockSize, audioBuffer.getNumSamples() - startSample);

        juce::dsp::AudioBlock<float> block (audioBuffer.getArrayOfWritePointers(),
                                            static_cast<size_t> (audioBuffer.getNumChannels()),
                                            static_cast<size_t> (startSample),
                                            static_cast<size_t> (samplesThisBlock));

        juce::dsp::ProcessContextReplacing<float> context (block);
        noiseReductionProcessor.process (context);
    }

    // Update waveform display
    mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

    hasUnsavedChanges = true;
    updateTitle();

    juce::String message = "Applied " + juce::String (reductionDB, 1) + " dB noise reduction.";
    mainComponent->getCorrectionListView().setStatusText (message);

    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Noise Reduction Complete",
                                            message);
}

void StandaloneWindow::detectTracks()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    TrackDetector detector;
    TrackDetector::DetectionSettings settings;
    settings.silenceThresholdDb = -40.0f;
    settings.minSilenceDurationSeconds = 2.0;
    settings.minTrackDurationSeconds = 10.0;

    auto boundaries = detector.detectTracks (audioBuffer, sampleRate, settings);

    juce::String message = "Detected " + juce::String (boundaries.size()) + " track boundaries.";
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Track Detection",
                                            message);
}

void StandaloneWindow::splitTracks()
{
    // TODO: Implement track splitting with dialog
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Split Tracks",
                                            "Track splitting will be implemented here.");
}

void StandaloneWindow::showBatchProcessor()
{
    // Create batch processor settings dialog
    auto* dialog = new juce::AlertWindow ("Batch Processor",
                                          "Configure batch processing settings:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextBlock ("Select files to process and configure settings:");

    // Settings
    dialog->addComboBox ("clickRemoval", {"Disabled", "Low (25)", "Medium (50)", "High (75)", "Maximum (100)"}, "Click Removal:");
    dialog->getComboBoxComponent ("clickRemoval")->setSelectedItemIndex (2); // Default: Medium

    dialog->addComboBox ("noiseReduction", {"Disabled", "6 dB", "12 dB", "18 dB", "24 dB"}, "Noise Reduction:");
    dialog->getComboBoxComponent ("noiseReduction")->setSelectedItemIndex (0); // Default: Disabled

    dialog->addComboBox ("rumbleFilter", {"Disabled", "20 Hz", "40 Hz", "60 Hz", "80 Hz"}, "Rumble Filter:");
    dialog->getComboBoxComponent ("rumbleFilter")->setSelectedItemIndex (0); // Default: Disabled

    dialog->addComboBox ("humFilter", {"Disabled", "50 Hz (EU)", "60 Hz (US)"}, "Hum Filter:");
    dialog->getComboBoxComponent ("humFilter")->setSelectedItemIndex (0); // Default: Disabled

    dialog->addComboBox ("normalize", {"Disabled", "-0.5 dB", "-1 dB", "-3 dB"}, "Normalize:");
    dialog->getComboBoxComponent ("normalize")->setSelectedItemIndex (1); // Default: -0.5 dB

    dialog->addComboBox ("bitDepth", {"16-bit", "24-bit", "32-bit float"}, "Output Bit Depth:");
    dialog->getComboBoxComponent ("bitDepth")->setSelectedItemIndex (1); // Default: 24-bit

    dialog->addButton ("Select Files...", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                // Get settings from dialog
                BatchProcessor::Settings settings;

                int clickIdx = dialog->getComboBoxComponent ("clickRemoval")->getSelectedItemIndex();
                settings.clickRemoval = clickIdx > 0;
                settings.clickSensitivity = clickIdx > 0 ? (clickIdx * 25.0f) : 0.0f;

                int noiseIdx = dialog->getComboBoxComponent ("noiseReduction")->getSelectedItemIndex();
                settings.noiseReduction = noiseIdx > 0;
                settings.noiseReductionDB = noiseIdx > 0 ? (noiseIdx * 6.0f) : 0.0f;

                int rumbleIdx = dialog->getComboBoxComponent ("rumbleFilter")->getSelectedItemIndex();
                settings.rumbleFilter = rumbleIdx > 0;
                settings.rumbleFreq = rumbleIdx > 0 ? (rumbleIdx * 20.0f) : 20.0f;

                int humIdx = dialog->getComboBoxComponent ("humFilter")->getSelectedItemIndex();
                settings.humFilter = humIdx > 0;
                settings.humFreq = humIdx == 1 ? 50.0f : 60.0f;

                int normIdx = dialog->getComboBoxComponent ("normalize")->getSelectedItemIndex();
                settings.normalize = normIdx > 0;
                float normValues[] = {0.0f, -0.5f, -1.0f, -3.0f};
                settings.normalizeDB = normValues[normIdx];

                int bitIdx = dialog->getComboBoxComponent ("bitDepth")->getSelectedItemIndex();
                int bitDepths[] = {16, 24, 32};
                settings.outputBitDepth = bitDepths[bitIdx];

                // Show file chooser
                auto chooser = std::make_shared<juce::FileChooser> ("Select Audio Files to Process",
                               juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                               "*.wav;*.flac;*.aiff;*.ogg;*.mp3");

                chooser->launchAsync (juce::FileBrowserComponent::openMode |
                                     juce::FileBrowserComponent::canSelectFiles |
                                     juce::FileBrowserComponent::canSelectMultipleItems,
                    [this, settings, chooser] (const juce::FileChooser&)
                    {
                        auto results = chooser->getResults();
                        if (results.size() > 0)
                        {
                            startBatchProcessing (results, settings);
                        }
                    });
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::startBatchProcessing (const juce::Array<juce::File>& files, const BatchProcessor::Settings& settings)
{
    // Create batch processor
    auto batchProcessor = std::make_unique<BatchProcessor>();

    // Add files to queue
    for (const auto& file : files)
    {
        batchProcessor->addFile (file);
    }

    // Set progress callback
    batchProcessor->setProgressCallback ([this] (const BatchProcessor::ProgressInfo& info)
    {
        mainComponent->getCorrectionListView().setStatusText (
            "Batch: " + juce::String (info.currentFileIndex) + "/" +
            juce::String (info.totalFiles) + " - " + info.currentFileName);
    });

    // Set completion callback
    batchProcessor->setCompletionCallback ([this] (bool success, const juce::String& message)
    {
        mainComponent->getCorrectionListView().setStatusText ("Ready");
        juce::AlertWindow::showMessageBoxAsync (
            success ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
            "Batch Processing Complete",
            message);
    });

    // Start processing
    mainComponent->getCorrectionListView().setStatusText ("Starting batch processing...");
    batchProcessor->startProcessing (settings);

    // Note: batchProcessor will be destroyed here, but the thread will continue
    // This is a limitation - in production, we'd want to keep the processor alive
    // For now, we'll just start it and let it run
    batchProcessor.release();  // Release ownership to prevent deletion
}

void StandaloneWindow::showAboutDialog()
{
    // Borderless about overlay that closes on any click
    // With audio demo effect: crackling vinyl that "cleans up" over time
    class AboutOverlay : public juce::Component,
                         public juce::Timer,
                         public juce::AudioSource
    {
    public:
        AboutOverlay (juce::AudioDeviceManager* deviceMgr)
            : audioDeviceManager (deviceMgr)
        {
            setOpaque (false);
            setWantsKeyboardFocus (true);

            // Initialize audio demo
            if (audioDeviceManager != nullptr)
            {
                audioSourcePlayer.setSource (this);
                audioDeviceManager->addAudioCallback (&audioSourcePlayer);
            }

            // Start animation timer (30fps for smooth rotation)
            startTimerHz (30);
        }

        ~AboutOverlay() override
        {
            stopTimer();
            if (audioDeviceManager != nullptr)
            {
                audioDeviceManager->removeAudioCallback (&audioSourcePlayer);
                audioSourcePlayer.setSource (nullptr);
            }
        }

        //==============================================================================
        // AudioSource implementation - generates demo sound with fading crackle
        void prepareToPlay (int samplesPerBlockExpected, double newSampleRate) override
        {
            currentSampleRate = newSampleRate;
            phase1 = phase2 = phase3 = 0.0;
        }

        void releaseResources() override {}

        void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
        {
            auto* leftChannel = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
            auto* rightChannel = bufferToFill.buffer->getNumChannels() > 1
                                    ? bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample)
                                    : leftChannel;

            // Musical chord: C major (C4, E4, G4)
            const double freq1 = 261.63;  // C4
            const double freq2 = 329.63;  // E4
            const double freq3 = 392.00;  // G4

            const double phaseInc1 = freq1 / currentSampleRate;
            const double phaseInc2 = freq2 / currentSampleRate;
            const double phaseInc3 = freq3 / currentSampleRate;

            // Crackling decreases over time (first 5 seconds)
            float crackleAmount = juce::jmax (0.0f, 1.0f - (animationTime / 5.0f));
            float musicVolume = 0.15f;
            float crackleVolume = 0.12f * crackleAmount;

            for (int i = 0; i < bufferToFill.numSamples; ++i)
            {
                // Generate warm chord with slight detuning for vinyl feel
                float music = (float) (std::sin (phase1 * juce::MathConstants<double>::twoPi) * 0.4 +
                                       std::sin (phase2 * juce::MathConstants<double>::twoPi) * 0.3 +
                                       std::sin (phase3 * juce::MathConstants<double>::twoPi) * 0.3);

                // Add harmonics for richer tone
                music += (float) (std::sin (phase1 * 2.0 * juce::MathConstants<double>::twoPi) * 0.1);
                music += (float) (std::sin (phase3 * 2.0 * juce::MathConstants<double>::twoPi) * 0.08);

                // Generate vinyl crackle (random pops and continuous hiss)
                float crackle = 0.0f;

                // Continuous low-level hiss
                crackle += (random.nextFloat() * 2.0f - 1.0f) * 0.3f;

                // Random pops/clicks (sparse)
                if (random.nextFloat() < 0.001f)
                    crackle += (random.nextFloat() > 0.5f ? 1.0f : -1.0f) * 2.0f;

                // Medium crackles
                if (random.nextFloat() < 0.003f)
                    crackle += (random.nextFloat() * 2.0f - 1.0f) * 0.8f;

                // Combine music and crackle
                float sample = music * musicVolume + crackle * crackleVolume;

                // Apply soft clipping for warmth
                sample = std::tanh (sample * 1.5f) * 0.7f;

                leftChannel[i] = sample;
                rightChannel[i] = sample;

                phase1 += phaseInc1;
                phase2 += phaseInc2;
                phase3 += phaseInc3;

                if (phase1 >= 1.0) phase1 -= 1.0;
                if (phase2 >= 1.0) phase2 -= 1.0;
                if (phase3 >= 1.0) phase3 -= 1.0;
            }
        }

        //==============================================================================
        // Timer callback for animation
        void timerCallback() override
        {
            animationTime += 1.0f / 30.0f;
            vinylRotation += 0.5f;  // Rotate vinyl
            if (vinylRotation >= 360.0f)
                vinylRotation -= 360.0f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            // Semi-transparent dark overlay for entire screen
            g.fillAll (juce::Colour (0xcc000000));

            // Calculate centered content area (twice the original size: 1000x1160)
            float contentWidth = 1000.0f;
            float contentHeight = 1100.0f;
            float contentX = (bounds.getWidth() - contentWidth) / 2.0f;
            float contentY = (bounds.getHeight() - contentHeight) / 2.0f;

            // Draw rounded content background
            juce::Rectangle<float> contentBounds (contentX, contentY, contentWidth, contentHeight);
            g.setColour (juce::Colour (0xff1a1a1a));
            g.fillRoundedRectangle (contentBounds, 20.0f);

            // Subtle border
            g.setColour (juce::Colour (0xff3a3a3a));
            g.drawRoundedRectangle (contentBounds, 20.0f, 2.0f);

            // Vinyl record area - larger to fill more of the window
            float vinylSize = 850.0f;
            float vinylX = contentX + (contentWidth - vinylSize) / 2.0f;
            float vinylY = contentY + 20.0f;
            auto centre = juce::Point<float> (vinylX + vinylSize / 2.0f, vinylY + vinylSize / 2.0f);
            float radius = vinylSize / 2.0f;

            // Draw outer shadow/glow (static, not rotating)
            for (int i = 30; i > 0; --i)
            {
                float alpha = 0.02f * (30 - i);
                g.setColour (juce::Colours::black.withAlpha (alpha));
                g.fillEllipse (centre.x - radius - i, centre.y - radius - i,
                              (radius + i) * 2.0f, (radius + i) * 2.0f);
            }

            // Apply rotation for the vinyl (33 RPM feel)
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (
                vinylRotation * juce::MathConstants<float>::pi / 180.0f,
                centre.x, centre.y));

            // Draw vinyl record (black disc)
            g.setColour (juce::Colour (0xff080808));
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

            // Draw grooves (concentric circles) - alternating light/dark for visibility
            int grooveCount = 0;
            for (float r = radius * 0.48f; r < radius * 0.98f; r += 2.0f)
            {
                // Alternate between darker and lighter grooves
                if (grooveCount % 2 == 0)
                    g.setColour (juce::Colour (0xff1c1c1c));  // Lighter groove
                else
                    g.setColour (juce::Colour (0xff101010));  // Darker groove

                g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
                grooveCount++;
            }

            // Draw shiny reflection on vinyl (subtle highlight)
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillEllipse (centre.x - radius * 0.7f, centre.y - radius * 0.85f,
                          radius * 0.9f, radius * 0.5f);

            // Draw label area (center circle) with gold gradient
            float labelRadius = radius * 0.48f;
            juce::ColourGradient gradient (juce::Colour (0xffe8c547), centre.x - labelRadius * 0.3f, centre.y - labelRadius * 0.3f,
                                          juce::Colour (0xffc9a227), centre.x + labelRadius * 0.5f, centre.y + labelRadius * 0.5f,
                                          true);
            g.setGradientFill (gradient);
            g.fillEllipse (centre.x - labelRadius, centre.y - labelRadius,
                          labelRadius * 2.0f, labelRadius * 2.0f);

            // Draw label border (dark ring)
            g.setColour (juce::Colour (0xff8b7355));
            g.drawEllipse (centre.x - labelRadius, centre.y - labelRadius,
                          labelRadius * 2.0f, labelRadius * 2.0f, 5.0f);

            // Draw spindle hole (center)
            float holeRadius = radius * 0.045f;
            g.setColour (juce::Colour (0xff2a2a2a));
            g.fillEllipse (centre.x - holeRadius, centre.y - holeRadius,
                          holeRadius * 2.0f, holeRadius * 2.0f);

            // Draw text on label with arc text for title
            g.setColour (juce::Colour (0xff2a2008));

            // Draw "VINYL RESTORATION SUITE" in an arc at top of label
            {
                juce::String arcText = "VINYL RESTORATION SUITE";
                float arcRadius = labelRadius * 0.82f;  // Radius for the text arc (near edge of label)
                float arcStartAngle = -juce::MathConstants<float>::pi * 0.78f;  // Start angle
                float arcEndAngle = -juce::MathConstants<float>::pi * 0.22f;    // End angle
                float arcSpan = arcEndAngle - arcStartAngle;

                g.setFont (juce::Font (juce::FontOptions (22.0f)).boldened());  // Smaller to fit within label

                // Letter spacing: extra pixels to add between each character
                float letterSpacing = 2.0f;

                // Calculate total text width to properly space characters
                float totalWidth = 0.0f;
                for (int i = 0; i < arcText.length(); ++i)
                {
                    totalWidth += g.getCurrentFont().getStringWidthFloat (arcText.substring (i, i + 1));
                    totalWidth += letterSpacing;  // Add spacing after each character
                }
                totalWidth -= letterSpacing;  // Remove trailing spacing

                // Draw each character along the arc
                float currentAngle = arcStartAngle;
                float anglePerUnit = arcSpan / totalWidth;

                for (int i = 0; i < arcText.length(); ++i)
                {
                    juce::String ch = arcText.substring (i, i + 1);
                    float charWidth = g.getCurrentFont().getStringWidthFloat (ch);

                    // Position at middle of character
                    float charAngle = currentAngle + (charWidth * 0.5f) * anglePerUnit;

                    // Calculate position on arc
                    float x = centre.x + arcRadius * std::cos (charAngle);
                    float y = centre.y + arcRadius * std::sin (charAngle);

                    // Save graphics state
                    g.saveState();

                    // First rotate around origin, then translate to position
                    g.addTransform (juce::AffineTransform::rotation (charAngle + juce::MathConstants<float>::halfPi)
                                    .translated (x, y));

                    // Draw character centered at origin
                    g.drawText (ch, -20, -15, 40, 30, juce::Justification::centred);

                    g.restoreState();

                    // Advance by character width plus letter spacing
                    currentAngle += (charWidth + letterSpacing) * anglePerUnit;
                }
            }

            // Divider line above spindle
            g.setColour (juce::Colour (0xff5a4a28));
            g.drawHorizontalLine ((int) (centre.y - labelRadius * 0.22f),
                                  centre.x - labelRadius * 0.55f,
                                  centre.x + labelRadius * 0.55f);

            // ===== SPINDLE HOLE IS AT centre.y =====

            // Divider line below spindle
            g.drawHorizontalLine ((int) (centre.y + labelRadius * 0.22f),
                                  centre.x - labelRadius * 0.55f,
                                  centre.x + labelRadius * 0.55f);

            // Company name BELOW the spindle - LARGE
            g.setColour (juce::Colour (0xff2a2008));
            g.setFont (juce::Font (juce::FontOptions (38.0f)).boldened());
            g.drawText ("flarkAUDIO",
                       centre.x - labelRadius * 0.9f, centre.y + labelRadius * 0.22f,
                       labelRadius * 1.8f, 44.0f,
                       juce::Justification::centred);

            // Year - smaller
            g.setFont (juce::Font (juce::FontOptions (18.0f)));
            g.drawText ("2024-2025",
                       centre.x - labelRadius * 0.9f, centre.y + labelRadius * 0.48f,
                       labelRadius * 1.8f, 26.0f,
                       juce::Justification::centred);

            // Version at bottom of label - smaller
            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawText ("Version 1.5.2",
                       centre.x - labelRadius * 0.9f, centre.y + labelRadius * 0.62f,
                       labelRadius * 1.8f, 24.0f,
                       juce::Justification::centred);

            // Restore graphics state (end vinyl rotation)
            g.restoreState();

            // Features text below vinyl - in label gold color
            g.setColour (juce::Colour (0xffe8c547));  // Gold label color
            g.setFont (juce::Font (juce::FontOptions (22.0f)).boldened());

            float textY = vinylY + vinylSize + 40.0f;
            g.drawText ("Professional audio restoration for vinyl and tape transfers",
                       contentX + 40.0f, textY, contentWidth - 80.0f, 32.0f,
                       juce::Justification::centred);

            g.setColour (juce::Colours::grey);
            g.setFont (juce::Font (juce::FontOptions (20.0f)));
            g.drawText ("Click removal  -  Noise reduction  -  Graphic EQ  -  Track detection",
                       contentX + 40.0f, textY + 40.0f, contentWidth - 80.0f, 28.0f,
                       juce::Justification::centred);

            // Close hint at bottom
            g.setColour (juce::Colours::darkgrey);
            g.setFont (juce::Font (juce::FontOptions (18.0f)));
            g.drawText ("Click anywhere to close",
                       contentX + 40.0f, contentY + contentHeight - 50.0f, contentWidth - 80.0f, 28.0f,
                       juce::Justification::centred);

            // Show restoration progress indicator
            float progress = juce::jmin (1.0f, animationTime / 5.0f);
            if (progress < 1.0f)
            {
                g.setColour (juce::Colour (0xffe8c547).withAlpha (0.8f));
                g.setFont (juce::Font (juce::FontOptions (16.0f)).italicised());
                g.drawText ("Restoring audio... " + juce::String ((int)(progress * 100)) + "%",
                           contentX + 40.0f, contentY + contentHeight - 80.0f, contentWidth - 80.0f, 24.0f,
                           juce::Justification::centred);

                // Progress bar
                float barWidth = 300.0f;
                float barHeight = 6.0f;
                float barX = contentX + (contentWidth - barWidth) / 2.0f;
                float barY = contentY + contentHeight - 100.0f;

                g.setColour (juce::Colour (0xff3a3a3a));
                g.fillRoundedRectangle (barX, barY, barWidth, barHeight, 3.0f);

                g.setColour (juce::Colour (0xffe8c547));
                g.fillRoundedRectangle (barX, barY, barWidth * progress, barHeight, 3.0f);
            }
            else
            {
                g.setColour (juce::Colour (0xff4CAF50));  // Green
                g.setFont (juce::Font (juce::FontOptions (16.0f)).boldened());
                g.drawText ("Audio restored!",
                           contentX + 40.0f, contentY + contentHeight - 80.0f, contentWidth - 80.0f, 24.0f,
                           juce::Justification::centred);
            }
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            // Close on any click
            delete this;
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            // Close on Escape
            if (key == juce::KeyPress::escapeKey)
            {
                delete this;
                return true;
            }
            return false;
        }

        void inputAttemptWhenModal() override
        {
            // Close when clicking outside (modal input attempt)
            delete this;
        }

    private:
        juce::AudioDeviceManager* audioDeviceManager = nullptr;
        juce::AudioSourcePlayer audioSourcePlayer;
        juce::Random random;

        // Audio synthesis
        double currentSampleRate = 44100.0;
        double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;

        // Animation state
        float animationTime = 0.0f;
        float vinylRotation = 0.0f;
    };

    // Create and show the overlay with audio demo
    auto* overlay = new AboutOverlay (&audioDeviceManager);

    // Get the display bounds to cover the entire screen area
    auto displayBounds = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
    overlay->setBounds (displayBounds);

    // Add to desktop as a borderless window
    overlay->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    overlay->setVisible (true);
    overlay->toFront (true);
    overlay->grabKeyboardFocus();

    // Enter modal state so it captures all input
    overlay->enterModalState (true, nullptr, true);
}

void StandaloneWindow::updateTitle()
{
    juce::String title = "Vinyl Restoration Suite";

    // Show session file name if saved, otherwise audio file name
    if (currentSessionFile.exists())
    {
        title += " - " + currentSessionFile.getFileName();
        if (hasUnsavedChanges)
            title += " *";

        // Show the source audio file in status bar
        if (currentFile.exists() && mainComponent != nullptr)
        {
            juce::String status = "Source: " + currentFile.getFileName();
            auto durationSec = audioBuffer.getNumSamples() / sampleRate;
            status += " (" + juce::String (durationSec / 60.0, 1) + " min)";
            mainComponent->getCorrectionListView().setStatusText (status);
        }
    }
    else if (currentFile.exists())
    {
        title += " - " + currentFile.getFileName();
        if (hasUnsavedChanges)
            title += " *";
    }

    setName (title);

    // Also update the native title bar
    if (getPeer() != nullptr)
        getPeer()->setTitle (title);
}

void StandaloneWindow::loadSession (const juce::File& sessionFile)
{
    juce::File audioFile;
    juce::var sessionData;

    if (!fileManager.loadSession (sessionFile, audioFile, sessionData))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Load Session Failed",
                                                "Could not load session file.\nThe audio file may have been moved or deleted.");
        return;
    }

    // Load the referenced audio file
    if (!fileManager.loadAudioFile (audioFile, audioBuffer, sampleRate))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Load Session Failed",
                                                "Could not load the audio file referenced by this session.");
        return;
    }

    currentFile = audioFile;
    currentSessionFile = sessionFile;  // Track the session file
    recentFiles.addFile (sessionFile);

    // Save recent files immediately
    auto settingsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("VinylRestorationSuite");
    settingsDir.createDirectory();
    settingsDir.getChildFile ("recent_files.txt").replaceWithText (recentFiles.toString());

    // Update waveform display
    mainComponent->getWaveformDisplay().loadFile (audioFile);
    mainComponent->setAudioBuffer (&audioBuffer, sampleRate);

    // Load into transport source for playback
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    formatManager.registerFormat (new juce::FlacAudioFormat(), true);
    formatManager.registerFormat (new juce::OggVorbisAudioFormat(), true);

    #if JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat (new juce::MP3AudioFormat(), true);
    #endif

    auto* reader = formatManager.createReaderFor (audioFile);
    if (reader != nullptr)
    {
        readerSource.reset (new juce::AudioFormatReaderSource (reader, true));
        transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    }

    // TODO: Restore corrections from sessionData
    mainComponent->getCorrectionListView().clearCorrections();

    hasUnsavedChanges = false;
    updateTitle();

    DBG ("Session loaded: " + sessionFile.getFullPathName());
    DBG ("Audio file: " + audioFile.getFullPathName());

    auto durationSec = audioBuffer.getNumSamples() / sampleRate;
    juce::String status = "Session loaded: " + juce::String (durationSec / 60.0, 1) + " min";
    mainComponent->getCorrectionListView().setStatusText (status);
}

//==============================================================================
// Additional Process Menu Methods
//==============================================================================

void StandaloneWindow::cutAndSplice()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Check for selection
    auto& waveform = mainComponent->getWaveformDisplay();
    int64_t selStart = -1, selEnd = -1;
    waveform.getSelection (selStart, selEnd);

    if (selStart < 0 || selEnd <= selStart)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Cut and Splice",
                                                "Please select a region to cut first.\n\n"
                                                "Hold Shift and drag on the waveform to make a selection.");
        return;
    }

    // Create dialog for cut and splice options
    auto* dialog = new juce::AlertWindow ("Cut and Splice",
                                          "Remove the selected region and join the remaining audio.",
                                          juce::AlertWindow::QuestionIcon);

    double startSec = selStart / sampleRate;
    double endSec = selEnd / sampleRate;
    double lengthSec = (selEnd - selStart) / sampleRate;

    dialog->addTextBlock ("Selection: " + juce::String (startSec, 2) + "s - " +
                         juce::String (endSec, 2) + "s (" +
                         juce::String (lengthSec, 3) + " seconds)");

    dialog->addComboBox ("fadeType", {"No crossfade", "5ms crossfade", "10ms crossfade", "25ms crossfade", "50ms crossfade"}, "Splice type:");
    dialog->getComboBoxComponent ("fadeType")->setSelectedItemIndex (2); // Default: 10ms

    dialog->addButton ("Cut", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog, selStart, selEnd] (int result)
        {
            if (result == 1)
            {
                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Cut and Splice");

                int fadeIdx = dialog->getComboBoxComponent ("fadeType")->getSelectedItemIndex();
                int fadeSamples[] = {0, (int)(0.005 * sampleRate), (int)(0.010 * sampleRate),
                                     (int)(0.025 * sampleRate), (int)(0.050 * sampleRate)};
                int fadeLength = fadeSamples[fadeIdx];

                // Create new buffer without the selected region
                int64_t cutLength = selEnd - selStart;
                int newLength = audioBuffer.getNumSamples() - static_cast<int>(cutLength);

                if (newLength <= 0)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                            "Error",
                                                            "Cannot cut entire audio file.");
                    delete dialog;
                    return;
                }

                juce::AudioBuffer<float> newBuffer (audioBuffer.getNumChannels(), newLength);

                // Copy audio before selection
                int beforeEnd = static_cast<int>(juce::jmin (selStart, (int64_t)audioBuffer.getNumSamples()));
                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    newBuffer.copyFrom (ch, 0, audioBuffer, ch, 0, beforeEnd);
                }

                // Copy audio after selection
                int afterStart = static_cast<int>(juce::jmin (selEnd, (int64_t)audioBuffer.getNumSamples()));
                int afterLength = audioBuffer.getNumSamples() - afterStart;
                if (afterLength > 0)
                {
                    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                    {
                        newBuffer.copyFrom (ch, beforeEnd, audioBuffer, ch, afterStart, afterLength);
                    }
                }

                // Apply crossfade at splice point
                if (fadeLength > 0 && beforeEnd > fadeLength && afterLength > fadeLength)
                {
                    int splicePoint = beforeEnd;
                    int fadeStart = splicePoint - fadeLength;
                    int fadeEnd = splicePoint + fadeLength;

                    // Ensure we don't exceed buffer bounds
                    fadeEnd = juce::jmin (fadeEnd, newBuffer.getNumSamples());

                    for (int ch = 0; ch < newBuffer.getNumChannels(); ++ch)
                    {
                        auto* channelData = newBuffer.getWritePointer (ch);
                        for (int i = fadeStart; i < fadeEnd; ++i)
                        {
                            float phase = (float)(i - fadeStart) / (float)(fadeEnd - fadeStart);
                            float fade = 0.5f - 0.5f * std::cos (phase * juce::MathConstants<float>::pi);
                            // Smooth the splice point
                            if (i < splicePoint)
                                channelData[i] *= fade;
                        }
                    }
                }

                // Replace audio buffer
                audioBuffer = newBuffer;

                // Update displays
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);
                mainComponent->getWaveformDisplay().clearSelection();
                mainComponent->setAudioBuffer (&audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                juce::String message = "Cut " + juce::String ((selEnd - selStart) / sampleRate, 3) + " seconds.";
                mainComponent->getCorrectionListView().setStatusText (message);

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Cut and Splice Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::showGraphicEQ()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create 10-band graphic EQ dialog
    auto* dialog = new juce::AlertWindow ("Graphic Equaliser",
                                          "Adjust frequency bands (dB gain, -12 to +12):",
                                          juce::AlertWindow::QuestionIcon);

    // Standard 10-band frequencies
    const char* bands[] = {"31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz",
                           "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};

    for (int i = 0; i < 10; ++i)
    {
        dialog->addTextEditor (juce::String (i), "0", bands[i]);
    }

    dialog->addButton ("Apply", 1);
    dialog->addButton ("Reset", 2);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                // Get gain values
                float gains[10];
                bool hasChanges = false;
                for (int i = 0; i < 10; ++i)
                {
                    gains[i] = juce::jlimit (-12.0f, 12.0f,
                                            dialog->getTextEditorContents (juce::String (i)).getFloatValue());
                    if (std::abs (gains[i]) > 0.1f)
                        hasChanges = true;
                }

                if (!hasChanges)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                            "No Changes",
                                                            "All bands are set to 0 dB.");
                    delete dialog;
                    return;
                }

                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Graphic EQ");

                mainComponent->getCorrectionListView().setStatusText ("Applying EQ...");

                // Configure filter bank
                juce::dsp::ProcessSpec spec;
                spec.sampleRate = sampleRate;
                spec.numChannels = static_cast<juce::uint32> (audioBuffer.getNumChannels());
                spec.maximumBlockSize = 2048;

                filterBankProcessor.prepare (spec);

                // Set EQ gains (FilterBank has 10 bands at 31, 62, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz)
                for (int i = 0; i < 10; ++i)
                {
                    filterBankProcessor.setEQBand (i, gains[i]);
                }

                // Process audio
                const int blockSize = 2048;
                for (int startSample = 0; startSample < audioBuffer.getNumSamples(); startSample += blockSize)
                {
                    int samplesThisBlock = juce::jmin (blockSize, audioBuffer.getNumSamples() - startSample);

                    juce::dsp::AudioBlock<float> block (audioBuffer.getArrayOfWritePointers(),
                                                        static_cast<size_t> (audioBuffer.getNumChannels()),
                                                        static_cast<size_t> (startSample),
                                                        static_cast<size_t> (samplesThisBlock));

                    juce::dsp::ProcessContextReplacing<float> context (block);
                    filterBankProcessor.process (context);
                }

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                mainComponent->getCorrectionListView().setStatusText ("EQ applied successfully");
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Graphic Equaliser",
                                                        "EQ applied successfully.");
            }
            else if (result == 2)
            {
                // Reset - reopen dialog
                delete dialog;
                showGraphicEQ();
                return;
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::normalise()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create normalise dialog
    auto* dialog = new juce::AlertWindow ("Normalise",
                                          "Adjust audio level to target peak:",
                                          juce::AlertWindow::QuestionIcon);

    // Find current peak level
    float currentPeak = 0.0f;
    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        auto range = juce::FloatVectorOperations::findMinAndMax (
            audioBuffer.getReadPointer (ch), audioBuffer.getNumSamples());
        currentPeak = juce::jmax (currentPeak, std::abs (range.getStart()), std::abs (range.getEnd()));
    }
    float currentPeakDb = juce::Decibels::gainToDecibels (currentPeak);

    dialog->addTextBlock ("Current peak level: " + juce::String (currentPeakDb, 1) + " dB");
    dialog->addTextEditor ("targetLevel", "-0.5", "Target peak level (dB):");
    dialog->addComboBox ("mode", {"Peak normalise", "RMS normalise"}, "Mode:");

    dialog->addButton ("Apply", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog, currentPeak] (int result)
        {
            if (result == 1)
            {
                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Normalise");

                float targetDb = juce::jlimit (-20.0f, 0.0f,
                                               dialog->getTextEditorContents ("targetLevel").getFloatValue());
                int mode = dialog->getComboBoxComponent ("mode")->getSelectedItemIndex();

                mainComponent->getCorrectionListView().setStatusText ("Normalising...");

                float targetGain = juce::Decibels::decibelsToGain (targetDb);
                float gainFactor;

                if (mode == 0) // Peak normalise
                {
                    if (currentPeak > 0.0001f)
                        gainFactor = targetGain / currentPeak;
                    else
                        gainFactor = 1.0f;
                }
                else // RMS normalise
                {
                    // Calculate RMS
                    float sumSquares = 0.0f;
                    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                    {
                        const float* data = audioBuffer.getReadPointer (ch);
                        for (int i = 0; i < audioBuffer.getNumSamples(); ++i)
                            sumSquares += data[i] * data[i];
                    }
                    float rms = std::sqrt (sumSquares / (audioBuffer.getNumSamples() * audioBuffer.getNumChannels()));

                    if (rms > 0.0001f)
                        gainFactor = targetGain / rms;
                    else
                        gainFactor = 1.0f;

                    // Limit gain to avoid clipping
                    float maxGain = 1.0f / currentPeak;
                    gainFactor = juce::jmin (gainFactor, maxGain * 0.99f);
                }

                // Apply gain
                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    juce::FloatVectorOperations::multiply (
                        audioBuffer.getWritePointer (ch), gainFactor, audioBuffer.getNumSamples());
                }

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                float appliedDb = juce::Decibels::gainToDecibels (gainFactor);
                juce::String message = "Applied " + juce::String (appliedDb, 1) + " dB gain.";
                mainComponent->getCorrectionListView().setStatusText (message);

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Normalise Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::channelBalance()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    if (audioBuffer.getNumChannels() < 2)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Channel Balance",
                                                "This file is mono. Channel balance requires stereo audio.");
        return;
    }

    // Calculate current balance
    float leftRms = 0.0f, rightRms = 0.0f;
    const float* leftData = audioBuffer.getReadPointer (0);
    const float* rightData = audioBuffer.getReadPointer (1);

    for (int i = 0; i < audioBuffer.getNumSamples(); ++i)
    {
        leftRms += leftData[i] * leftData[i];
        rightRms += rightData[i] * rightData[i];
    }
    leftRms = std::sqrt (leftRms / audioBuffer.getNumSamples());
    rightRms = std::sqrt (rightRms / audioBuffer.getNumSamples());

    float currentBalance = 0.0f;
    if (leftRms + rightRms > 0.0001f)
        currentBalance = (rightRms - leftRms) / (leftRms + rightRms) * 100.0f;

    // Create dialog
    auto* dialog = new juce::AlertWindow ("Channel Balance",
                                          "Adjust left/right channel balance:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextBlock ("Current balance: " + juce::String (currentBalance, 1) + "% " +
                         (currentBalance > 0 ? "(right)" : currentBalance < 0 ? "(left)" : "(center)"));
    dialog->addTextBlock ("Left RMS: " + juce::String (juce::Decibels::gainToDecibels (leftRms), 1) + " dB\n"
                         "Right RMS: " + juce::String (juce::Decibels::gainToDecibels (rightRms), 1) + " dB");

    dialog->addTextEditor ("leftGain", "0", "Left channel gain (dB):");
    dialog->addTextEditor ("rightGain", "0", "Right channel gain (dB):");

    dialog->addButton ("Apply", 1);
    dialog->addButton ("Auto-Balance", 2);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog, leftRms, rightRms] (int result)
        {
            if (result == 1 || result == 2)
            {
                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Channel Balance");

                float leftGain, rightGain;

                if (result == 2) // Auto-balance
                {
                    // Calculate gains to make both channels equal RMS
                    float avgRms = (leftRms + rightRms) / 2.0f;
                    leftGain = (leftRms > 0.0001f) ? (avgRms / leftRms) : 1.0f;
                    rightGain = (rightRms > 0.0001f) ? (avgRms / rightRms) : 1.0f;
                }
                else
                {
                    leftGain = juce::Decibels::decibelsToGain (
                        juce::jlimit (-24.0f, 24.0f, dialog->getTextEditorContents ("leftGain").getFloatValue()));
                    rightGain = juce::Decibels::decibelsToGain (
                        juce::jlimit (-24.0f, 24.0f, dialog->getTextEditorContents ("rightGain").getFloatValue()));
                }

                mainComponent->getCorrectionListView().setStatusText ("Adjusting balance...");

                // Apply gains
                juce::FloatVectorOperations::multiply (
                    audioBuffer.getWritePointer (0), leftGain, audioBuffer.getNumSamples());
                juce::FloatVectorOperations::multiply (
                    audioBuffer.getWritePointer (1), rightGain, audioBuffer.getNumSamples());

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                juce::String message = "Balance adjusted: L " +
                    juce::String (juce::Decibels::gainToDecibels (leftGain), 1) + " dB, R " +
                    juce::String (juce::Decibels::gainToDecibels (rightGain), 1) + " dB";
                mainComponent->getCorrectionListView().setStatusText (message);

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Channel Balance Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::wowFlutterRemoval()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create wow & flutter dialog with eccentric record correction
    auto* dialog = new juce::AlertWindow ("Wow & Flutter Removal",
                                          "Correct speed variations from eccentric records or tape wow:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextBlock ("For eccentric records (off-center hole), specify RPM and measured eccentricity.\n"
                          "The cartridge 'sway' at the outer grooves indicates eccentricity.");

    dialog->addComboBox ("source", {"Vinyl Record", "Tape/Cassette", "Auto-detect"}, "Source type:");
    dialog->getComboBoxComponent ("source")->setSelectedItemIndex (0);

    dialog->addComboBox ("rpm", {"33 1/3 RPM", "45 RPM", "78 RPM"}, "Record speed:");
    dialog->getComboBoxComponent ("rpm")->setSelectedItemIndex (0);

    dialog->addTextEditor ("eccentricity", "0.5", "Eccentricity (mm, 0-2):");
    dialog->addTextEditor ("phase", "0", "Phase offset (degrees, 0-360):");

    dialog->addComboBox ("mode", {"Preview only", "Apply correction"}, "Mode:");
    dialog->getComboBoxComponent ("mode")->setSelectedItemIndex (1);

    dialog->addButton ("Process", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                int sourceType = dialog->getComboBoxComponent ("source")->getSelectedItemIndex();
                int rpmIndex = dialog->getComboBoxComponent ("rpm")->getSelectedItemIndex();
                float eccentricity = dialog->getTextEditorContents ("eccentricity").getFloatValue();
                float phaseOffset = dialog->getTextEditorContents ("phase").getFloatValue();
                bool applyCorrection = dialog->getComboBoxComponent ("mode")->getSelectedItemIndex() == 1;

                // Clamp values
                eccentricity = juce::jlimit (0.0f, 2.0f, eccentricity);
                phaseOffset = std::fmod (phaseOffset, 360.0f);

                // Calculate wow frequency based on RPM
                float rpm = (rpmIndex == 0) ? 33.333f : (rpmIndex == 1) ? 45.0f : 78.0f;
                float wowFrequency = rpm / 60.0f;  // Hz (revolutions per second)

                // Calculate pitch deviation from eccentricity
                // Typical groove radius at outer edge ~145mm, inner ~60mm
                // Average ~100mm for calculation
                float avgGrooveRadius = 100.0f;  // mm
                float pitchDeviation = (eccentricity / avgGrooveRadius) * 100.0f;  // percentage

                if (pitchDeviation < 0.01f)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                            "No Correction Needed",
                                                            "Eccentricity is too small to require correction.");
                    delete dialog;
                    return;
                }

                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Eccentric Record Correction");

                mainComponent->getCorrectionListView().setStatusText (
                    "Correcting wow at " + juce::String (wowFrequency, 3) + " Hz, " +
                    juce::String (pitchDeviation, 2) + "% deviation...");

                DBG ("Eccentric correction: RPM=" + juce::String (rpm) +
                     ", wow freq=" + juce::String (wowFrequency, 3) + " Hz" +
                     ", eccentricity=" + juce::String (eccentricity) + " mm" +
                     ", pitch deviation=" + juce::String (pitchDeviation, 3) + "%");

                //==============================================================
                // ECCENTRIC RECORD CORRECTION ALGORITHM
                // Uses variable-rate resampling to correct sinusoidal pitch error
                //==============================================================

                const int numSamples = audioBuffer.getNumSamples();
                const int numChannels = audioBuffer.getNumChannels();

                // Create output buffer (same size - we're correcting, not changing duration)
                juce::AudioBuffer<float> outputBuffer (numChannels, numSamples);
                outputBuffer.clear();

                // Calculate samples per revolution
                double samplesPerRevolution = sampleRate / wowFrequency;

                // Phase offset in radians
                double phaseRad = phaseOffset * juce::MathConstants<double>::pi / 180.0;

                // Pitch deviation as ratio (e.g., 0.5% = 0.005)
                double deviationRatio = pitchDeviation / 100.0;

                // Process using sinusoidal resampling
                // The idea: at each output sample, calculate where in the input
                // we should read from, accounting for the sinusoidal speed variation

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    const float* input = audioBuffer.getReadPointer (ch);
                    float* output = outputBuffer.getWritePointer (ch);

                    // Track cumulative phase for input position
                    double inputPos = 0.0;

                    for (int outSample = 0; outSample < numSamples; ++outSample)
                    {
                        // Calculate current phase in the wow cycle
                        double wowPhase = (2.0 * juce::MathConstants<double>::pi * outSample / samplesPerRevolution) + phaseRad;

                        // The original recording has speed variation: speed = 1 + deviation * sin(phase)
                        // To correct, we need to read at variable rate: 1 / (1 + deviation * sin(phase))
                        // Approximation for small deviations: 1 - deviation * sin(phase)
                        double speedCorrection = 1.0 - deviationRatio * std::sin (wowPhase);

                        // Advance input position by corrected amount
                        inputPos += speedCorrection;

                        // Cubic interpolation for high-quality resampling
                        int idx = static_cast<int> (inputPos);
                        double frac = inputPos - idx;

                        if (idx >= 1 && idx < numSamples - 2)
                        {
                            // Cubic Hermite interpolation
                            float y0 = input[idx - 1];
                            float y1 = input[idx];
                            float y2 = input[idx + 1];
                            float y3 = input[idx + 2];

                            float c0 = y1;
                            float c1 = 0.5f * (y2 - y0);
                            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                            float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

                            float t = static_cast<float> (frac);
                            output[outSample] = ((c3 * t + c2) * t + c1) * t + c0;
                        }
                        else if (idx >= 0 && idx < numSamples)
                        {
                            // Linear interpolation at boundaries
                            int idx1 = juce::jmin (idx + 1, numSamples - 1);
                            output[outSample] = input[idx] + static_cast<float> (frac) * (input[idx1] - input[idx]);
                        }
                    }
                }

                // Copy result back
                audioBuffer = outputBuffer;

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                juce::String message = "Eccentric record correction applied.\n\n"
                                       "RPM: " + juce::String (rpm, 1) + "\n"
                                       "Wow frequency: " + juce::String (wowFrequency, 3) + " Hz\n"
                                       "Eccentricity: " + juce::String (eccentricity, 1) + " mm\n"
                                       "Pitch correction: +/- " + juce::String (pitchDeviation, 2) + "%";

                mainComponent->getCorrectionListView().setStatusText ("Eccentric correction complete");

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Wow & Flutter Removal Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::dropoutRestoration()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create dropout restoration dialog
    auto* dialog = new juce::AlertWindow ("Dropout Restoration",
                                          "Detect and repair signal dropouts (tape damage, scratches):",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextBlock ("Dropouts are sudden signal losses that appear as silence or noise bursts.");

    dialog->addTextEditor ("threshold", "-40", "Detection threshold (dB):");
    dialog->addTextEditor ("minLength", "5", "Minimum dropout length (ms):");
    dialog->addComboBox ("method", {"Interpolate", "Copy from other channel", "Silence"}, "Repair method:");

    dialog->addButton ("Detect & Repair", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Dropout Restoration");

                float thresholdDb = juce::jlimit (-80.0f, -10.0f,
                                                  dialog->getTextEditorContents ("threshold").getFloatValue());
                float minLengthMs = juce::jlimit (1.0f, 100.0f,
                                                  dialog->getTextEditorContents ("minLength").getFloatValue());
                int method = dialog->getComboBoxComponent ("method")->getSelectedItemIndex();

                float thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb);
                int minLengthSamples = static_cast<int> (minLengthMs * 0.001f * sampleRate);

                mainComponent->getCorrectionListView().setStatusText ("Detecting dropouts...");

                int dropoutsFound = 0;
                int dropoutsRepaired = 0;

                // Detect and repair dropouts for each channel
                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    float* channelData = audioBuffer.getWritePointer (ch);
                    int numSamples = audioBuffer.getNumSamples();

                    int dropoutStart = -1;

                    for (int i = 0; i < numSamples; ++i)
                    {
                        bool isBelowThreshold = std::abs (channelData[i]) < thresholdLinear;

                        if (isBelowThreshold && dropoutStart < 0)
                        {
                            dropoutStart = i;
                        }
                        else if (!isBelowThreshold && dropoutStart >= 0)
                        {
                            int dropoutLength = i - dropoutStart;

                            if (dropoutLength >= minLengthSamples)
                            {
                                dropoutsFound++;

                                // Repair based on method
                                if (method == 0) // Interpolate
                                {
                                    float startVal = (dropoutStart > 0) ? channelData[dropoutStart - 1] : 0.0f;
                                    float endVal = (i < numSamples) ? channelData[i] : 0.0f;

                                    for (int j = dropoutStart; j < i; ++j)
                                    {
                                        float t = (float)(j - dropoutStart) / (float)dropoutLength;
                                        // Cosine interpolation for smooth transition
                                        float weight = 0.5f - 0.5f * std::cos (t * juce::MathConstants<float>::pi);
                                        channelData[j] = startVal + (endVal - startVal) * weight;
                                    }
                                    dropoutsRepaired++;
                                }
                                else if (method == 1 && audioBuffer.getNumChannels() > 1) // Copy from other channel
                                {
                                    int otherCh = (ch == 0) ? 1 : 0;
                                    const float* otherData = audioBuffer.getReadPointer (otherCh);

                                    for (int j = dropoutStart; j < i; ++j)
                                    {
                                        channelData[j] = otherData[j];
                                    }
                                    dropoutsRepaired++;
                                }
                                else if (method == 2) // Silence (already silent, just mark as handled)
                                {
                                    dropoutsRepaired++;
                                }
                            }

                            dropoutStart = -1;
                        }
                    }
                }

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                juce::String message = "Found " + juce::String (dropoutsFound) + " dropouts, repaired " +
                                       juce::String (dropoutsRepaired) + ".";
                mainComponent->getCorrectionListView().setStatusText (message);

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Dropout Restoration Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::speedCorrection()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create speed correction dialog
    auto* dialog = new juce::AlertWindow ("Speed Correction",
                                          "Correct playback speed errors:",
                                          juce::AlertWindow::QuestionIcon);

    double durationSec = audioBuffer.getNumSamples() / sampleRate;
    dialog->addTextBlock ("Current duration: " + juce::String (durationSec, 2) + " seconds\n"
                         "Sample rate: " + juce::String (sampleRate) + " Hz");

    dialog->addComboBox ("mode", {"Percentage adjustment", "Match target duration", "Convert sample rate"}, "Correction mode:");
    dialog->getComboBoxComponent ("mode")->setSelectedItemIndex (0);

    dialog->addTextEditor ("value", "100", "Speed percentage (50-200%):");

    dialog->addTextBlock ("\nPresets:");
    dialog->addComboBox ("preset", {"Custom", "33 1/3 -> 45 RPM (+35.1%)", "45 -> 33 1/3 RPM (-25.9%)",
                                    "78 -> 33 1/3 RPM (-57.3%)", "33 1/3 -> 78 RPM (+133.8%)"}, "Common corrections:");

    dialog->addButton ("Apply", 1);
    dialog->addButton ("Preview", 2);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog, durationSec] (int result)
        {
            if (result == 1)
            {
                int preset = dialog->getComboBoxComponent ("preset")->getSelectedItemIndex();

                float speedPercent;
                if (preset == 0) // Custom
                {
                    speedPercent = juce::jlimit (50.0f, 200.0f,
                                                 dialog->getTextEditorContents ("value").getFloatValue());
                }
                else
                {
                    // Preset values (33 1/3 = 33.333, 45, 78 RPM)
                    float presetValues[] = {100.0f, 135.1f, 74.1f, 42.7f, 233.8f};
                    speedPercent = presetValues[preset];
                }

                if (std::abs (speedPercent - 100.0f) < 0.1f)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                            "No Change",
                                                            "Speed is already at 100%.");
                    delete dialog;
                    return;
                }

                // Save state for undo
                undoManager.saveState (audioBuffer, sampleRate, "Speed Correction");

                mainComponent->getCorrectionListView().setStatusText ("Applying speed correction...");

                // Calculate new length
                float speedRatio = 100.0f / speedPercent;
                int newNumSamples = static_cast<int> (audioBuffer.getNumSamples() * speedRatio);

                // Create new buffer with interpolated samples
                juce::AudioBuffer<float> newBuffer (audioBuffer.getNumChannels(), newNumSamples);

                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                {
                    const float* srcData = audioBuffer.getReadPointer (ch);
                    float* destData = newBuffer.getWritePointer (ch);

                    for (int i = 0; i < newNumSamples; ++i)
                    {
                        // Calculate source position with linear interpolation
                        float srcPos = i / speedRatio;
                        int srcIdx = static_cast<int> (srcPos);
                        float frac = srcPos - srcIdx;

                        if (srcIdx + 1 < audioBuffer.getNumSamples())
                        {
                            destData[i] = srcData[srcIdx] * (1.0f - frac) + srcData[srcIdx + 1] * frac;
                        }
                        else if (srcIdx < audioBuffer.getNumSamples())
                        {
                            destData[i] = srcData[srcIdx];
                        }
                        else
                        {
                            destData[i] = 0.0f;
                        }
                    }
                }

                // Replace buffer
                audioBuffer = newBuffer;

                // Update display
                mainComponent->getWaveformDisplay().updateFromBuffer (audioBuffer, sampleRate);
                mainComponent->setAudioBuffer (&audioBuffer, sampleRate);

                hasUnsavedChanges = true;
                updateTitle();

                double newDurationSec = audioBuffer.getNumSamples() / sampleRate;
                juce::String message = "Speed adjusted to " + juce::String (speedPercent, 1) + "%. " +
                                       "New duration: " + juce::String (newDurationSec, 2) + " seconds.";
                mainComponent->getCorrectionListView().setStatusText (message);

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                        "Speed Correction Complete",
                                                        message);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::turntableAnalyzer()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Show settings dialog
    auto* dialog = new juce::AlertWindow ("Turntable Quality Analyzer",
                                          "Analyze recording to measure turntable and record quality:",
                                          juce::AlertWindow::QuestionIcon);

    dialog->addTextBlock ("This analyzer measures quality parameters from your recording.\n"
                          "It separates TURNTABLE artifacts (periodic) from RECORD artifacts (random).");

    dialog->addComboBox ("rpm", {"33 1/3 RPM", "45 RPM", "78 RPM", "Auto-detect"}, "Record speed:");
    dialog->getComboBoxComponent ("rpm")->setSelectedItemIndex (0);

    dialog->addTextEditor ("brand", "", "Turntable brand (optional):");
    dialog->addTextEditor ("model", "", "Turntable model (optional):");

    dialog->addButton ("Analyze", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                int rpmIndex = dialog->getComboBoxComponent ("rpm")->getSelectedItemIndex();
                juce::String brand = dialog->getTextEditorContents ("brand");
                juce::String model = dialog->getTextEditorContents ("model");

                mainComponent->getCorrectionListView().setStatusText ("Analyzing turntable quality...");

                //==============================================================
                // TURNTABLE QUALITY ANALYSIS
                //==============================================================

                const int numSamples = audioBuffer.getNumSamples();
                const int numChannels = audioBuffer.getNumChannels();
                const float* dataL = audioBuffer.getReadPointer (0);
                const float* dataR = numChannels > 1 ? audioBuffer.getReadPointer (1) : dataL;

                // Determine RPM and wow frequency
                float rpm = (rpmIndex == 0) ? 33.333f : (rpmIndex == 1) ? 45.0f : (rpmIndex == 2) ? 78.0f : 33.333f;
                float wowFreq = rpm / 60.0f;  // Hz

                //--------------------------------------------------------------
                // 1. WOW & FLUTTER MEASUREMENT
                // Detect periodic pitch modulation using autocorrelation
                //--------------------------------------------------------------

                // Use short-time autocorrelation to track pitch variations
                const int frameSize = 2048;
                const int hopSize = 512;
                int numFrames = (numSamples - frameSize) / hopSize;

                std::vector<float> pitchDeviations;
                float prevPitch = 0.0f;

                for (int frame = 0; frame < numFrames; ++frame)
                {
                    int startSample = frame * hopSize;

                    // Autocorrelation for pitch detection
                    float maxCorr = 0.0f;
                    int bestLag = 0;

                    // Search for fundamental in 50-2000 Hz range
                    int minLag = static_cast<int>(sampleRate / 2000.0);
                    int maxLag = static_cast<int>(sampleRate / 50.0);
                    maxLag = juce::jmin(maxLag, frameSize / 2);

                    for (int lag = minLag; lag < maxLag; ++lag)
                    {
                        float corr = 0.0f;
                        for (int i = 0; i < frameSize - lag; ++i)
                        {
                            corr += dataL[startSample + i] * dataL[startSample + i + lag];
                        }
                        if (corr > maxCorr)
                        {
                            maxCorr = corr;
                            bestLag = lag;
                        }
                    }

                    if (bestLag > 0 && maxCorr > 0.1f)
                    {
                        float pitch = static_cast<float>(sampleRate) / bestLag;
                        if (prevPitch > 0.0f)
                        {
                            float deviation = (pitch - prevPitch) / prevPitch * 100.0f;
                            pitchDeviations.push_back(deviation);
                        }
                        prevPitch = pitch;
                    }
                }

                // Calculate wow & flutter (RMS of pitch deviations)
                float wowFlutterRMS = 0.0f;
                float wowFlutterPeak = 0.0f;
                if (!pitchDeviations.empty())
                {
                    for (float dev : pitchDeviations)
                    {
                        wowFlutterRMS += dev * dev;
                        wowFlutterPeak = juce::jmax(wowFlutterPeak, std::abs(dev));
                    }
                    wowFlutterRMS = std::sqrt(wowFlutterRMS / pitchDeviations.size());
                }

                //--------------------------------------------------------------
                // 2. RUMBLE MEASUREMENT
                // Measure low frequency content (<30 Hz)
                //--------------------------------------------------------------

                // Simple IIR low-pass filter at 30 Hz for rumble extraction
                float rumbleEnergy = 0.0f;
                float signalEnergy = 0.0f;
                float rumbleFiltered = 0.0f;
                float alpha = 2.0f * juce::MathConstants<float>::pi * 30.0f / static_cast<float>(sampleRate);
                alpha = alpha / (1.0f + alpha);

                for (int i = 0; i < numSamples; ++i)
                {
                    float sample = (dataL[i] + dataR[i]) * 0.5f;
                    rumbleFiltered = alpha * sample + (1.0f - alpha) * rumbleFiltered;
                    rumbleEnergy += rumbleFiltered * rumbleFiltered;
                    signalEnergy += sample * sample;
                }

                float rumbleDB = -100.0f;
                if (signalEnergy > 0.0f && rumbleEnergy > 0.0f)
                {
                    rumbleDB = 10.0f * std::log10(rumbleEnergy / signalEnergy);
                }

                //--------------------------------------------------------------
                // 3. SURFACE NOISE / SNR
                // Analyze noise floor in quiet passages
                //--------------------------------------------------------------

                // Find quietest 1-second segment
                const int windowSize = static_cast<int>(sampleRate);
                float minRMS = 1.0f;
                float maxRMS = 0.0f;

                for (int start = 0; start < numSamples - windowSize; start += windowSize / 2)
                {
                    float rms = 0.0f;
                    for (int i = 0; i < windowSize; ++i)
                    {
                        float s = dataL[start + i];
                        rms += s * s;
                    }
                    rms = std::sqrt(rms / windowSize);
                    minRMS = juce::jmin(minRMS, rms);
                    maxRMS = juce::jmax(maxRMS, rms);
                }

                float snrDB = -60.0f;
                if (minRMS > 0.0f && maxRMS > 0.0f)
                {
                    snrDB = 20.0f * std::log10(maxRMS / minRMS);
                }

                float noiseFloorDB = 20.0f * std::log10(juce::jmax(minRMS, 0.00001f));

                //--------------------------------------------------------------
                // 4. CLICK/POP DENSITY (Record quality indicator)
                //--------------------------------------------------------------

                int clickCount = 0;
                float prevSample = 0.0f;
                float threshold = 0.3f;  // Derivative threshold for click detection

                for (int i = 1; i < numSamples; ++i)
                {
                    float derivative = std::abs(dataL[i] - prevSample);
                    if (derivative > threshold)
                    {
                        clickCount++;
                        i += 100;  // Skip ahead to avoid counting same click multiple times
                    }
                    prevSample = dataL[i];
                }

                float durationMin = numSamples / sampleRate / 60.0f;
                float clicksPerMinute = clickCount / durationMin;

                //--------------------------------------------------------------
                // 5. CHANNEL BALANCE
                //--------------------------------------------------------------

                float energyL = 0.0f, energyR = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                {
                    energyL += dataL[i] * dataL[i];
                    energyR += dataR[i] * dataR[i];
                }

                float channelBalanceDB = 0.0f;
                if (energyL > 0.0f && energyR > 0.0f)
                {
                    channelBalanceDB = 10.0f * std::log10(energyR / energyL);
                }

                //--------------------------------------------------------------
                // Calculate scores
                //--------------------------------------------------------------

                // Turntable score (based on periodic artifacts)
                int turntableScore = 100;
                if (wowFlutterRMS > 0.3f) turntableScore -= 40;
                else if (wowFlutterRMS > 0.1f) turntableScore -= 20;
                else if (wowFlutterRMS > 0.05f) turntableScore -= 10;

                if (rumbleDB > -50.0f) turntableScore -= 30;
                else if (rumbleDB > -60.0f) turntableScore -= 15;
                else if (rumbleDB > -70.0f) turntableScore -= 5;

                turntableScore = juce::jmax(0, turntableScore);

                // Record score (based on random artifacts)
                int recordScore = 100;
                if (clicksPerMinute > 50.0f) recordScore -= 40;
                else if (clicksPerMinute > 20.0f) recordScore -= 25;
                else if (clicksPerMinute > 10.0f) recordScore -= 10;

                if (noiseFloorDB > -40.0f) recordScore -= 30;
                else if (noiseFloorDB > -50.0f) recordScore -= 15;
                else if (noiseFloorDB > -55.0f) recordScore -= 5;

                recordScore = juce::jmax(0, recordScore);

                int combinedScore = (turntableScore + recordScore) / 2;

                //--------------------------------------------------------------
                // Build report
                //--------------------------------------------------------------

                juce::String report;
                report << "════════════════════════════════════════════\n";
                report << "       TURNTABLE QUALITY REPORT\n";
                report << "════════════════════════════════════════════\n\n";

                if (brand.isNotEmpty() || model.isNotEmpty())
                {
                    report << "Equipment: " << brand << " " << model << "\n";
                }
                report << "Analyzed at " << juce::String(rpm, 1) << " RPM\n\n";

                report << "────────────────────────────────────────────\n";
                report << " TURNTABLE SCORE: " << turntableScore << "/100\n";
                report << "────────────────────────────────────────────\n";
                report << "  Wow & Flutter: " << juce::String(wowFlutterRMS, 3) << "% WRMS";
                if (wowFlutterRMS < 0.05f) report << " (Excellent)\n";
                else if (wowFlutterRMS < 0.1f) report << " (Good)\n";
                else if (wowFlutterRMS < 0.2f) report << " (Fair)\n";
                else report << " (Poor)\n";

                report << "  Peak W&F: " << juce::String(wowFlutterPeak, 3) << "%\n";
                report << "  Rumble: " << juce::String(rumbleDB, 1) << " dB";
                if (rumbleDB < -70.0f) report << " (Excellent)\n";
                else if (rumbleDB < -60.0f) report << " (Good)\n";
                else if (rumbleDB < -50.0f) report << " (Fair)\n";
                else report << " (Poor)\n";

                report << "  Channel Balance: " << juce::String(std::abs(channelBalanceDB), 1) << " dB\n\n";

                report << "────────────────────────────────────────────\n";
                report << " RECORD SCORE: " << recordScore << "/100\n";
                report << "────────────────────────────────────────────\n";
                report << "  Surface Noise: " << juce::String(noiseFloorDB, 1) << " dB";
                if (noiseFloorDB < -55.0f) report << " (Excellent)\n";
                else if (noiseFloorDB < -50.0f) report << " (Good)\n";
                else if (noiseFloorDB < -40.0f) report << " (Fair)\n";
                else report << " (Poor)\n";

                report << "  Click Density: " << juce::String(clicksPerMinute, 0) << " /min";
                if (clicksPerMinute < 10.0f) report << " (Excellent)\n";
                else if (clicksPerMinute < 20.0f) report << " (Good)\n";
                else if (clicksPerMinute < 50.0f) report << " (Fair)\n";
                else report << " (Poor)\n";

                report << "  Dynamic Range: " << juce::String(snrDB, 1) << " dB\n\n";

                report << "════════════════════════════════════════════\n";
                report << " COMBINED SCORE: " << combinedScore << "/100\n";
                report << "════════════════════════════════════════════\n";

                DBG(report);

                mainComponent->getCorrectionListView().setStatusText (
                    "Analysis complete - TT: " + juce::String(turntableScore) +
                    "/100, Record: " + juce::String(recordScore) + "/100");

                // Show results in a larger dialog
                auto* resultDialog = new juce::AlertWindow ("Analysis Results",
                                                            "",
                                                            juce::AlertWindow::InfoIcon);
                resultDialog->addTextBlock (report);
                resultDialog->addButton ("OK", 1);
                resultDialog->addButton ("Copy to Clipboard", 2);

                resultDialog->enterModalState (true, juce::ModalCallbackFunction::create (
                    [resultDialog, report] (int btnResult)
                    {
                        if (btnResult == 2)
                        {
                            juce::SystemClipboard::copyTextToClipboard (report);
                        }
                        delete resultDialog;
                    }
                ), true);
            }
            delete dialog;
        }
    ), true);
}

void StandaloneWindow::showAudioSettings()
{
    // Show JUCE's built-in audio settings dialog
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = juce::Colours::darkgrey;
    options.content.setOwned (new juce::AudioDeviceSelectorComponent (
        audioDeviceManager,
        0,  // min input channels
        0,  // max input channels
        2,  // min output channels
        2,  // max output channels
        false,  // show MIDI input
        false,  // show MIDI output
        false,  // show channels as stereo pairs
        false   // hide advanced options
    ));
    options.content->setSize (500, 400);
    options.launchAsync();
}

void StandaloneWindow::setUIScale (float newScale)
{
    if (std::abs (newScale - uiScaleFactor) < 0.01f)
        return;

    uiScaleFactor = juce::jlimit (0.25f, 4.0f, newScale);

    // Calculate new window size
    int scaledWidth = juce::roundToInt (baseWidth * uiScaleFactor);
    int scaledHeight = juce::roundToInt (baseHeight * uiScaleFactor);

    // Apply scale via the wrapper
    if (contentWrapper != nullptr)
    {
        contentWrapper->setScaleFactor (uiScaleFactor);
        contentWrapper->setSize (scaledWidth, scaledHeight);
    }

    // Update window constraints based on scale
    setResizeLimits (
        juce::roundToInt (600 * uiScaleFactor),
        juce::roundToInt (400 * uiScaleFactor),
        juce::roundToInt (3840 * uiScaleFactor),
        juce::roundToInt (2160 * uiScaleFactor));

    // Resize the window to fit the scaled content
    setSize (scaledWidth, scaledHeight);

    DBG ("UI Scale set to: " + juce::String (uiScaleFactor * 100.0f, 0) + "%");
}

//==============================================================================
// Spectrogram Display
//==============================================================================

void StandaloneWindow::showSpectrogram()
{
    if (audioBuffer.getNumSamples() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "No Audio",
                                                "Please load an audio file first.");
        return;
    }

    // Create a new window to display the spectrogram
    class SpectrogramWindow : public juce::DocumentWindow
    {
    public:
        SpectrogramWindow (const juce::AudioBuffer<float>& buffer, double sampleRate,
                          const juce::String& fileName)
            : DocumentWindow ("Spectrogram - " + fileName,
                             juce::Colour (0xff1a1a2e),
                             DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            spectrogram = std::make_unique<SpectrogramDisplay>();
            spectrogram->setSize (800, 400);
            setContentOwned (spectrogram.get(), true);
            spectrogram.release(); // Window now owns it

            // Analyze the buffer
            auto* display = dynamic_cast<SpectrogramDisplay*> (getContentComponent());
            if (display != nullptr)
                display->analyzeBuffer (buffer, sampleRate);

            setUsingNativeTitleBar (true);
            setResizable (true, true);
            centreWithSize (900, 500);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            delete this;
        }

    private:
        std::unique_ptr<SpectrogramDisplay> spectrogram;
    };

    // Create the spectrogram window (self-deleting)
    new SpectrogramWindow (audioBuffer, sampleRate, currentFile.getFileName());
}

//==============================================================================
// MainComponent Implementation
//==============================================================================

StandaloneWindow::MainComponent::MainComponent()
{
    // Load logo from embedded PNG data
    #include "../Resources/VRSLogoData.h"
    logoImage = juce::ImageFileFormat::loadFrom (VRSlogo_png, VRSlogo_png_len);

    // Add waveform display
    addAndMakeVisible (waveformDisplay);

    // Add correction list
    addAndMakeVisible (correctionListView);

    // Transport controls (retro cassette deck style)
    addAndMakeVisible (rewindButton);
    addAndMakeVisible (playPauseButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (forwardButton);

    rewindButton.addListener (this);
    playPauseButton.addListener (this);
    stopButton.addListener (this);
    forwardButton.addListener (this);

    rewindButton.setTooltip ("Skip backward 5 seconds");
    playPauseButton.setTooltip ("Play/Pause playback (Spacebar)");
    stopButton.setTooltip ("Stop playback and return to start");
    forwardButton.setTooltip ("Skip forward 5 seconds");

    // Enable keyboard focus for spacebar control
    setWantsKeyboardFocus (true);

    // Position slider
    addAndMakeVisible (positionSlider);
    positionSlider.setRange (0.0, 1.0);
    positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    positionSlider.addListener (this);
    positionSlider.setTooltip ("Seek playback position");

    // Time label
    addAndMakeVisible (timeLabel);
    timeLabel.setText ("00:00.000", juce::dontSendNotification);
    timeLabel.setJustificationType (juce::Justification::centred);

    // Volume slider
    addAndMakeVisible (volumeSlider);
    volumeSlider.setRange (0.0, 100.0, 1.0);  // 0-100% with 1% steps
    volumeSlider.setValue (70.0); // 70% default volume
    volumeSlider.setSliderStyle (juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    volumeSlider.setTextValueSuffix ("%");
    volumeSlider.addListener (this);
    volumeSlider.setTooltip ("Adjust playback volume (0-100%)");

    // Volume label
    addAndMakeVisible (volumeLabel);
    volumeLabel.setText ("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType (juce::Justification::centred);

    // Zoom controls
    addAndMakeVisible (zoomInButton);
    addAndMakeVisible (zoomOutButton);
    addAndMakeVisible (zoomFitButton);

    zoomInButton.addListener (this);
    zoomOutButton.addListener (this);
    zoomFitButton.addListener (this);

    zoomInButton.setTooltip ("Zoom in waveform horizontally");
    zoomOutButton.setTooltip ("Zoom out waveform horizontally");
    zoomFitButton.setTooltip ("Fit entire waveform in view");

    // Horizontal zoom slider (below waveform)
    addAndMakeVisible (horizontalZoomSlider);
    horizontalZoomSlider.setRange (1.0, 100.0, 0.1);
    horizontalZoomSlider.setValue (1.0);
    horizontalZoomSlider.setSkewFactorFromMidPoint (10.0);
    horizontalZoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    horizontalZoomSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    horizontalZoomSlider.addListener (this);
    horizontalZoomSlider.setTooltip ("Horizontal zoom (1x to 100x)");

    addAndMakeVisible (horizontalZoomLabel);
    horizontalZoomLabel.setText ("H-Zoom:", juce::dontSendNotification);
    horizontalZoomLabel.setJustificationType (juce::Justification::centredRight);

    // Vertical zoom slider (next to waveform)
    addAndMakeVisible (verticalZoomSlider);
    verticalZoomSlider.setRange (0.1, 10.0, 0.1);
    verticalZoomSlider.setValue (1.0);
    verticalZoomSlider.setSkewFactorFromMidPoint (1.0);
    verticalZoomSlider.setSliderStyle (juce::Slider::LinearVertical);
    verticalZoomSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    verticalZoomSlider.addListener (this);
    verticalZoomSlider.setTooltip ("Vertical zoom (amplitude)");

    addAndMakeVisible (verticalZoomLabel);
    verticalZoomLabel.setText ("V-Zoom", juce::dontSendNotification);
    verticalZoomLabel.setJustificationType (juce::Justification::centred);

    // Toolbar buttons for quick access to common functions
    addAndMakeVisible (toolbarOpenButton);
    addAndMakeVisible (toolbarSaveButton);
    addAndMakeVisible (toolbarUndoButton);
    addAndMakeVisible (toolbarRedoButton);
    addAndMakeVisible (toolbarDetectButton);
    addAndMakeVisible (toolbarRemoveButton);
    addAndMakeVisible (toolbarNoiseButton);
    addAndMakeVisible (toolbarEQButton);
    addAndMakeVisible (toolbarSpectrumButton);
    addAndMakeVisible (toolbarSettingsButton);

    toolbarOpenButton.addListener (this);
    toolbarSaveButton.addListener (this);
    toolbarUndoButton.addListener (this);
    toolbarRedoButton.addListener (this);
    toolbarDetectButton.addListener (this);
    toolbarRemoveButton.addListener (this);
    toolbarNoiseButton.addListener (this);
    toolbarEQButton.addListener (this);
    toolbarSpectrumButton.addListener (this);
    toolbarSettingsButton.addListener (this);

    toolbarOpenButton.setTooltip ("Open audio file (Ctrl+O)");
    toolbarSaveButton.setTooltip ("Save session (Ctrl+S)");
    toolbarUndoButton.setTooltip ("Undo last action (Ctrl+Z)");
    toolbarRedoButton.setTooltip ("Redo last action (Ctrl+Y)");
    toolbarDetectButton.setTooltip ("Detect clicks in audio");
    toolbarRemoveButton.setTooltip ("Remove detected clicks");
    toolbarNoiseButton.setTooltip ("Apply noise reduction");
    toolbarEQButton.setTooltip ("Open graphic equalizer");
    toolbarSpectrumButton.setTooltip ("Show spectrogram view");
    toolbarSettingsButton.setTooltip ("Audio device settings");

    // Style toolbar buttons with gold accent color
    auto goldColor = juce::Colour (0xffe8c547);
    auto darkBg = juce::Colour (0xff3a3a3a);
    for (auto* btn : { &toolbarOpenButton, &toolbarSaveButton, &toolbarUndoButton, &toolbarRedoButton,
                       &toolbarDetectButton, &toolbarRemoveButton, &toolbarNoiseButton, &toolbarEQButton,
                       &toolbarSpectrumButton, &toolbarSettingsButton })
    {
        btn->setColour (juce::TextButton::buttonColourId, darkBg);
        btn->setColour (juce::TextButton::textColourOffId, goldColor);
    }

    // Selection editor
    addAndMakeVisible (selectionStartLabel);
    selectionStartLabel.setText ("Start:", juce::dontSendNotification);
    selectionStartLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (selectionStartEditor);
    selectionStartEditor.setInputRestrictions (12, "0123456789.:,");
    selectionStartEditor.setTooltip ("Selection start time (mm:ss.ms)");

    addAndMakeVisible (selectionEndLabel);
    selectionEndLabel.setText ("End:", juce::dontSendNotification);
    selectionEndLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (selectionEndEditor);
    selectionEndEditor.setInputRestrictions (12, "0123456789.:,");
    selectionEndEditor.setTooltip ("Selection end time (mm:ss.ms)");

    addAndMakeVisible (selectionLengthLabel);
    selectionLengthLabel.setText ("Length: 00:00.000", juce::dontSendNotification);
    selectionLengthLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (clearSelectionButton);
    clearSelectionButton.addListener (this);
    clearSelectionButton.setTooltip ("Clear current selection");

    // Enable keyboard focus for spacebar handling
    setWantsKeyboardFocus (true);

    // Status label
    addAndMakeVisible (statusLabel);
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    // Create resizer divider for waveform/correction list split
    addAndMakeVisible (resizeDivider);
    resizeDivider.onDrag = [this] (int deltaY)
    {
        // Update waveform height based on drag
        int newHeight = waveformHeight + deltaY;
        newHeight = juce::jlimit (100, getHeight() - 200, newHeight);  // Min 100px, leave space for other controls
        if (newHeight != waveformHeight)
        {
            waveformHeight = newHeight;
            resized();
        }
    };

    // Setup waveform display callbacks
    waveformDisplay.onSelectionChanged = [this] (int64_t start, int64_t end)
    {
        // Update selection editor with start/end/length values
        if (currentBuffer != nullptr && currentSampleRate > 0)
        {
            double startSeconds = start / currentSampleRate;
            double endSeconds = end / currentSampleRate;
            double lengthSeconds = (end - start) / currentSampleRate;

            int startMinutes = (int) (startSeconds / 60.0);
            double startSecs = startSeconds - startMinutes * 60;
            int endMinutes = (int) (endSeconds / 60.0);
            double endSecs = endSeconds - endMinutes * 60;
            int lengthMinutes = (int) (lengthSeconds / 60.0);
            double lengthSecs = lengthSeconds - lengthMinutes * 60;

            selectionStartEditor.setText (juce::String::formatted ("%02d:%06.3f", startMinutes, startSecs), juce::dontSendNotification);
            selectionEndEditor.setText (juce::String::formatted ("%02d:%06.3f", endMinutes, endSecs), juce::dontSendNotification);
            selectionLengthLabel.setText ("Length: " + juce::String::formatted ("%02d:%06.3f", lengthMinutes, lengthSecs), juce::dontSendNotification);

            // Set playback position to selection start so next play starts from selection
            if (parentWindow && !parentWindow->isPlaying)
            {
                parentWindow->transportSource.setPosition (startSeconds);
                double totalLength = currentBuffer->getNumSamples() / currentSampleRate;
                if (totalLength > 0)
                {
                    double position = startSeconds / totalLength;
                    positionSlider.setValue (position, juce::dontSendNotification);
                    waveformDisplay.setPlaybackPosition (position);
                }
            }
        }
    };

    // Setup process action callback from context menu
    waveformDisplay.onProcessAction = [this] (int actionId)
    {
        if (!parentWindow)
            return;

        switch (actionId)
        {
            case WaveformDisplay::actionDetectClicks:
                parentWindow->detectClicks();
                break;
            case WaveformDisplay::actionRemoveClicks:
                parentWindow->removeClicks();
                break;
            case WaveformDisplay::actionNoiseReduction:
                parentWindow->applyNoiseReduction();
                break;
            case WaveformDisplay::actionAudioSettings:
                parentWindow->showAudioSettings();
                break;
        }
    };

    // Setup clipboard action callback from context menu
    waveformDisplay.onClipboardAction = [this] (int actionId, int64_t selStart, int64_t selEnd)
    {
        if (!parentWindow)
            return;

        auto& buffer = parentWindow->audioBuffer;
        auto sr = parentWindow->sampleRate;

        if (buffer.getNumSamples() == 0)
            return;

        // Ensure valid selection bounds
        if (selStart > selEnd)
            std::swap (selStart, selEnd);

        int startSample = static_cast<int> (juce::jmax ((int64_t) 0, selStart));
        int endSample = static_cast<int> (juce::jmin ((int64_t) buffer.getNumSamples(), selEnd));
        int selLength = endSample - startSample;

        switch (actionId)
        {
            case WaveformDisplay::actionCut:
                if (selLength > 0)
                {
                    // Save undo state
                    parentWindow->undoManager.saveState (buffer, sr, "Cut");

                    // Copy to clipboard
                    juce::AudioBuffer<float> clipboardData (buffer.getNumChannels(), selLength);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        clipboardData.copyFrom (ch, 0, buffer, ch, startSample, selLength);
                    waveformDisplay.setClipboardData (clipboardData, sr);

                    // Remove from buffer
                    int newLength = buffer.getNumSamples() - selLength;
                    juce::AudioBuffer<float> newBuffer (buffer.getNumChannels(), newLength);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        newBuffer.copyFrom (ch, 0, buffer, ch, 0, startSample);
                        if (endSample < buffer.getNumSamples())
                            newBuffer.copyFrom (ch, startSample, buffer, ch, endSample, buffer.getNumSamples() - endSample);
                    }
                    buffer = newBuffer;

                    waveformDisplay.updateFromBuffer (buffer, sr);
                    waveformDisplay.clearSelection();
                    parentWindow->hasUnsavedChanges = true;
                    parentWindow->updateTitle();
                    DBG ("Cut " + juce::String (selLength) + " samples");
                }
                break;

            case WaveformDisplay::actionCopy:
                if (selLength > 0)
                {
                    juce::AudioBuffer<float> clipboardData (buffer.getNumChannels(), selLength);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        clipboardData.copyFrom (ch, 0, buffer, ch, startSample, selLength);
                    waveformDisplay.setClipboardData (clipboardData, sr);
                    DBG ("Copied " + juce::String (selLength) + " samples to clipboard");
                }
                break;

            case WaveformDisplay::actionPaste:
                if (waveformDisplay.hasClipboardData())
                {
                    // Save undo state
                    parentWindow->undoManager.saveState (buffer, sr, "Paste");

                    const auto& clipData = waveformDisplay.getClipboardBuffer();
                    int insertPos = startSample;  // Paste at selection start (or cursor)
                    int pasteLen = clipData.getNumSamples();

                    // Create new buffer with space for pasted data
                    int newLength = buffer.getNumSamples() + pasteLen;
                    juce::AudioBuffer<float> newBuffer (buffer.getNumChannels(), newLength);

                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        // Copy before insertion point
                        if (insertPos > 0)
                            newBuffer.copyFrom (ch, 0, buffer, ch, 0, insertPos);

                        // Copy clipboard data (handle channel mismatch)
                        int srcCh = juce::jmin (ch, clipData.getNumChannels() - 1);
                        newBuffer.copyFrom (ch, insertPos, clipData, srcCh, 0, pasteLen);

                        // Copy after insertion point
                        if (insertPos < buffer.getNumSamples())
                            newBuffer.copyFrom (ch, insertPos + pasteLen, buffer, ch, insertPos, buffer.getNumSamples() - insertPos);
                    }
                    buffer = newBuffer;

                    waveformDisplay.updateFromBuffer (buffer, sr);
                    waveformDisplay.setSelection (insertPos, insertPos + pasteLen);
                    parentWindow->hasUnsavedChanges = true;
                    parentWindow->updateTitle();
                    DBG ("Pasted " + juce::String (pasteLen) + " samples");
                }
                break;

            case WaveformDisplay::actionDeleteSelection:
                if (selLength > 0)
                {
                    // Save undo state
                    parentWindow->undoManager.saveState (buffer, sr, "Delete Selection");

                    // Remove selection from buffer
                    int newLength = buffer.getNumSamples() - selLength;
                    juce::AudioBuffer<float> newBuffer (buffer.getNumChannels(), newLength);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    {
                        newBuffer.copyFrom (ch, 0, buffer, ch, 0, startSample);
                        if (endSample < buffer.getNumSamples())
                            newBuffer.copyFrom (ch, startSample, buffer, ch, endSample, buffer.getNumSamples() - endSample);
                    }
                    buffer = newBuffer;

                    waveformDisplay.updateFromBuffer (buffer, sr);
                    waveformDisplay.clearSelection();
                    parentWindow->hasUnsavedChanges = true;
                    parentWindow->updateTitle();
                    DBG ("Deleted " + juce::String (selLength) + " samples");
                }
                break;

            case WaveformDisplay::actionCropToSelection:
                if (selLength > 0)
                {
                    // Save undo state
                    parentWindow->undoManager.saveState (buffer, sr, "Crop to Selection");

                    // Keep only the selected portion
                    juce::AudioBuffer<float> newBuffer (buffer.getNumChannels(), selLength);
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        newBuffer.copyFrom (ch, 0, buffer, ch, startSample, selLength);
                    buffer = newBuffer;

                    waveformDisplay.updateFromBuffer (buffer, sr);
                    waveformDisplay.clearSelection();
                    parentWindow->hasUnsavedChanges = true;
                    parentWindow->updateTitle();
                    DBG ("Cropped to selection: " + juce::String (selLength) + " samples");
                }
                break;

            case WaveformDisplay::actionSelectAll:
                waveformDisplay.setSelection (0, buffer.getNumSamples());
                break;

            case WaveformDisplay::actionPlaySelection:
                if (selLength > 0 && parentWindow->transportSource.getTotalLength() > 0)
                {
                    double startSec = startSample / sr;
                    parentWindow->transportSource.setPosition (startSec);
                    parentWindow->transportSource.start();
                }
                break;
        }
    };

    // Setup correction list callbacks
    correctionListView.onAuditionCorrection = [this] (int64_t position, float durationSec)
    {
        if (!parentWindow || parentWindow->transportSource.getTotalLength() <= 0)
            return;

        // Play audio centered around the correction position
        double sr = parentWindow->sampleRate;
        double positionSec = position / sr;
        double startSec = juce::jmax (0.0, positionSec - durationSec / 2.0);

        parentWindow->transportSource.setPosition (startSec);
        parentWindow->transportSource.start();

        DBG ("Auditioning correction at " + juce::String (positionSec, 2) + " sec");
    };

    correctionListView.onDeleteCorrection = [this] (int index)
    {
        correctionListView.removeCorrection (index);
        DBG ("Deleted correction at index " + juce::String (index));
    };

    correctionListView.onAdjustCorrection = [this] (int index)
    {
        if (index < 0 || index >= correctionListView.getNumCorrections())
            return;

        const auto& correction = correctionListView.getCorrection (index);

        // Create adjustment dialog
        auto* dialog = new juce::AlertWindow ("Adjust Correction",
                                              "Modify correction parameters:",
                                              juce::MessageBoxIconType::NoIcon);

        dialog->addTextEditor ("position", juce::String (correction.position), "Position (samples):");
        dialog->addTextEditor ("magnitude", juce::String (correction.magnitude, 2), "Magnitude (0-1):");
        dialog->addTextEditor ("width", juce::String (correction.width), "Width (samples):");

        dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey, 0, 0));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey, 0, 0));

        int corrIndex = index;  // Capture for lambda
        dialog->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, dialog, corrIndex] (int result)
            {
                if (result == 1)
                {
                    int64_t newPos = dialog->getTextEditorContents ("position").getLargeIntValue();
                    float newMag = dialog->getTextEditorContents ("magnitude").getFloatValue();
                    int newWidth = dialog->getTextEditorContents ("width").getIntValue();

                    correctionListView.updateCorrection (corrIndex, newPos, newMag, newWidth);
                    DBG ("Updated correction " + juce::String (corrIndex));
                }
                delete dialog;
            }), true);
    };
}

StandaloneWindow::MainComponent::~MainComponent()
{
}

void StandaloneWindow::MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2e2e2e));

    // Draw logo in the top right corner (in the volume slider area)
    if (logoImage.isValid())
    {
        int logoWidth = 60;
        int logoHeight = (int) (logoImage.getHeight() * (logoWidth / (float) logoImage.getWidth()));
        int x = getWidth() - logoWidth - 10;  // Top right corner
        int y = 55;  // Below transport controls

        g.setOpacity (0.9f);
        g.drawImage (logoImage, x, y, logoWidth, logoHeight,
                     0, 0, logoImage.getWidth(), logoImage.getHeight());
        g.setOpacity (1.0f);
    }
}

void StandaloneWindow::MainComponent::resized()
{
    auto area = getLocalBounds();

    // Menu bar at very top (if present)
    for (auto* child : getChildren())
    {
        if (auto* menuBar = dynamic_cast<juce::MenuBarComponent*> (child))
        {
            menuBar->setBounds (area.removeFromTop (juce::LookAndFeel::getDefaultLookAndFeel()
                                                    .getDefaultMenuBarHeight()));
            break;
        }
    }

    // Toolbar row directly under menu bar
    auto toolbarArea = area.removeFromTop (36);
    toolbarArea.reduce (10, 3);

    // Reserve space on right for logo/volume area (90px to match volume area + margin)
    toolbarArea.removeFromRight (90);

    const int btnWidth = 60;
    const int btnSpacing = 3;
    const int separatorWidth = 10;

    // File operations
    toolbarOpenButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (btnSpacing);
    toolbarSaveButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (separatorWidth);  // Separator

    // Undo/Redo
    toolbarUndoButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (btnSpacing);
    toolbarRedoButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (separatorWidth);  // Separator

    // Process operations
    toolbarDetectButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (btnSpacing);
    toolbarRemoveButton.setBounds (toolbarArea.removeFromLeft (btnWidth + 5));  // Slightly wider
    toolbarArea.removeFromLeft (btnSpacing);
    toolbarNoiseButton.setBounds (toolbarArea.removeFromLeft (btnWidth));
    toolbarArea.removeFromLeft (btnSpacing);
    toolbarEQButton.setBounds (toolbarArea.removeFromLeft (btnWidth - 15));  // Shorter
    toolbarArea.removeFromLeft (separatorWidth);  // Separator

    // View and settings (right-aligned within remaining space)
    toolbarSettingsButton.setBounds (toolbarArea.removeFromRight (btnWidth + 5));
    toolbarArea.removeFromRight (btnSpacing);
    toolbarSpectrumButton.setBounds (toolbarArea.removeFromRight (btnWidth + 15));

    // Volume slider uses right side of window (80px wide), but leave room for logo at top
    auto volumeArea = area.removeFromRight (80);
    volumeArea.reduce (10, 10);

    // Leave space at top for the logo (logo is ~60px wide, ~100px tall with padding)
    volumeArea.removeFromTop (120);  // Space for logo above volume slider

    // Reserve space at very bottom for volume label (under slider's text box)
    auto volumeLabelArea = volumeArea.removeFromBottom (20);
    volumeLabel.setBounds (volumeLabelArea);

    // The slider has TextBoxBelow which adds ~20px, so leave some margin
    volumeArea.removeFromBottom (25);  // Space for slider's built-in text box

    // Volume slider fills remaining vertical space (shorter now)
    volumeSlider.setBounds (volumeArea);

    // Left side contains all other content
    area.removeFromRight (5); // spacing

    // Transport controls at top (cassette deck style)
    auto transportArea = area.removeFromTop (50);
    transportArea.reduce (10, 10);

    rewindButton.setBounds (transportArea.removeFromLeft (50));
    transportArea.removeFromLeft (5);
    playPauseButton.setBounds (transportArea.removeFromLeft (80));  // Wider for toggle button
    transportArea.removeFromLeft (5);
    stopButton.setBounds (transportArea.removeFromLeft (60));
    transportArea.removeFromLeft (5);
    forwardButton.setBounds (transportArea.removeFromLeft (50));
    transportArea.removeFromLeft (10);

    // Time label and position slider
    timeLabel.setBounds (transportArea.removeFromRight (100));
    transportArea.removeFromRight (10);
    positionSlider.setBounds (transportArea);

    // Constants for control heights
    const int resizerHeight = 10;

    // Waveform area - use stored height
    auto waveformArea = area.removeFromTop (waveformHeight);

    // Vertical zoom slider on left side of waveform
    auto vZoomArea = waveformArea.removeFromLeft (50);
    vZoomArea.reduce (5, 10);

    verticalZoomLabel.setBounds (vZoomArea.removeFromTop (50));
    verticalZoomSlider.setBounds (vZoomArea);

    waveformArea.removeFromLeft (5); // Small gap
    waveformDisplay.setBounds (waveformArea);

    // Resizer divider between waveform and controls
    resizeDivider.setBounds (area.removeFromTop (resizerHeight));

    // Zoom buttons + horizontal zoom slider directly under resizer (same row)
    auto zoomButtonArea = area.removeFromTop (40);
    zoomButtonArea.reduce (10, 5);

    zoomInButton.setBounds (zoomButtonArea.removeFromLeft (40));
    zoomButtonArea.removeFromLeft (5);
    zoomOutButton.setBounds (zoomButtonArea.removeFromLeft (40));
    zoomButtonArea.removeFromLeft (5);
    zoomFitButton.setBounds (zoomButtonArea.removeFromLeft (60));
    zoomButtonArea.removeFromLeft (10);

    // Horizontal zoom slider on same row as buttons
    horizontalZoomLabel.setBounds (zoomButtonArea.removeFromLeft (60));
    zoomButtonArea.removeFromLeft (5);
    horizontalZoomSlider.setBounds (zoomButtonArea);

    area.removeFromTop (5); // Small gap

    // Selection editor directly under zoom buttons
    auto selectionArea = area.removeFromTop (35);
    selectionArea.reduce (10, 5);

    selectionStartLabel.setBounds (selectionArea.removeFromLeft (45));
    selectionArea.removeFromLeft (3);
    selectionStartEditor.setBounds (selectionArea.removeFromLeft (85));
    selectionArea.removeFromLeft (10);

    selectionEndLabel.setBounds (selectionArea.removeFromLeft (35));
    selectionArea.removeFromLeft (3);
    selectionEndEditor.setBounds (selectionArea.removeFromLeft (85));
    selectionArea.removeFromLeft (10);

    selectionLengthLabel.setBounds (selectionArea.removeFromLeft (150));
    selectionArea.removeFromLeft (10);

    clearSelectionButton.setBounds (selectionArea.removeFromLeft (60));

    area.removeFromTop (5); // Small gap

    // Status bar at bottom
    auto statusArea = area.removeFromBottom (30);
    statusArea.reduce (10, 5);
    statusLabel.setBounds (statusArea);

    // Correction list takes remaining space
    correctionListView.setBounds (area);
}

void StandaloneWindow::MainComponent::buttonClicked (juce::Button* button)
{
    if (button == &playPauseButton)
    {
        if (parentWindow && parentWindow->transportSource.getTotalLength() > 0.0)
        {
            if (parentWindow->isPlaying)
            {
                // Pause
                parentWindow->transportSource.stop();
                parentWindow->isPlaying = false;
                playPauseButton.setButtonText ("Play");
                DBG ("Playback paused");
            }
            else
            {
                // Play/Resume
                parentWindow->transportSource.start();
                parentWindow->isPlaying = true;
                playPauseButton.setButtonText ("Pause");
                DBG ("Playback started");
            }
        }
    }
    else if (button == &stopButton)
    {
        if (parentWindow)
        {
            parentWindow->transportSource.stop();
            parentWindow->isPlaying = false;
            playPauseButton.setButtonText ("Play");

            // Reset to selection start if selection exists, otherwise beginning
            int64_t selStart = -1, selEnd = -1;
            waveformDisplay.getSelection (selStart, selEnd);

            if (selStart >= 0 && selEnd > selStart && currentBuffer != nullptr)
            {
                // Go to selection start
                double startSeconds = selStart / currentSampleRate;
                double totalLength = currentBuffer->getNumSamples() / currentSampleRate;
                parentWindow->transportSource.setPosition (startSeconds);
                waveformDisplay.setPlaybackPosition (startSeconds / totalLength);
                positionSlider.setValue (startSeconds / totalLength, juce::dontSendNotification);
                DBG ("Playback stopped - position set to selection start: " + juce::String (startSeconds, 2) + "s");
            }
            else
            {
                // No selection - go to beginning
                parentWindow->transportSource.setPosition (0.0);
                waveformDisplay.setPlaybackPosition (0.0);
                positionSlider.setValue (0.0, juce::dontSendNotification);
                DBG ("Playback stopped - position set to beginning");
            }
        }
    }
    else if (button == &rewindButton)
    {
        // Skip backward 5 seconds
        if (parentWindow && parentWindow->transportSource.getTotalLength() > 0.0)
        {
            double currentPos = parentWindow->transportSource.getCurrentPosition();
            double newPos = juce::jmax (0.0, currentPos - 5.0);
            parentWindow->transportSource.setPosition (newPos);
            DBG ("Rewound to: " + juce::String (newPos, 2) + "s");
        }
    }
    else if (button == &forwardButton)
    {
        // Skip forward 5 seconds
        if (parentWindow && parentWindow->transportSource.getTotalLength() > 0.0)
        {
            double currentPos = parentWindow->transportSource.getCurrentPosition();
            double totalLength = parentWindow->transportSource.getLengthInSeconds();
            double newPos = juce::jmin (totalLength, currentPos + 5.0);
            parentWindow->transportSource.setPosition (newPos);
            DBG ("Fast-forwarded to: " + juce::String (newPos, 2) + "s");
        }
    }
    else if (button == &zoomInButton)
    {
        // Zoom in = increase zoom multiplier
        double newZoom = waveformDisplay.horizontalZoom * 2.0;
        waveformDisplay.setHorizontalZoom (newZoom);
        horizontalZoomSlider.setValue (newZoom, juce::dontSendNotification);
    }
    else if (button == &zoomOutButton)
    {
        // Zoom out = decrease zoom multiplier
        double newZoom = juce::jmax (1.0, waveformDisplay.horizontalZoom * 0.5);
        waveformDisplay.setHorizontalZoom (newZoom);
        horizontalZoomSlider.setValue (newZoom, juce::dontSendNotification);
    }
    else if (button == &zoomFitButton)
    {
        // Fit entire waveform
        waveformDisplay.setHorizontalZoom (1.0);
        waveformDisplay.scrollPosition = 0.0;
        horizontalZoomSlider.setValue (1.0, juce::dontSendNotification);
    }
    else if (button == &clearSelectionButton)
    {
        // Clear waveform selection
        waveformDisplay.clearSelection();

        selectionStartEditor.setText ("");
        selectionEndEditor.setText ("");
        selectionLengthLabel.setText ("Length: 00:00.000", juce::dontSendNotification);

        DBG ("Selection cleared");
    }
    //==============================================================================
    // Toolbar button handlers
    //==============================================================================
    else if (button == &toolbarOpenButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::fileOpen, 0);
    }
    else if (button == &toolbarSaveButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::fileSave, 0);
    }
    else if (button == &toolbarUndoButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::editUndo, 0);
    }
    else if (button == &toolbarRedoButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::editRedo, 0);
    }
    else if (button == &toolbarDetectButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::processDetectClicks, 0);
    }
    else if (button == &toolbarRemoveButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::processRemoveClicks, 0);
    }
    else if (button == &toolbarNoiseButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::processNoiseReduction, 0);
    }
    else if (button == &toolbarEQButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::processGraphicEQ, 0);
    }
    else if (button == &toolbarSpectrumButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::viewShowSpectrogram, 0);
    }
    else if (button == &toolbarSettingsButton)
    {
        if (parentWindow)
            parentWindow->menuItemSelected (StandaloneWindow::optionsAudioSettings, 0);
    }
}

void StandaloneWindow::MainComponent::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &positionSlider)
    {
        double position = positionSlider.getValue();
        waveformDisplay.setPlaybackPosition (position);

        // Instant seek: update transport source position (even during playback)
        if (parentWindow && currentBuffer != nullptr)
        {
            double timeInSeconds = position * (currentBuffer->getNumSamples() / currentSampleRate);
            parentWindow->transportSource.setPosition (timeInSeconds);

            // Update time label
            int minutes = (int) (timeInSeconds / 60.0);
            double seconds = timeInSeconds - minutes * 60;
            juce::String timeString = juce::String::formatted ("%02d:%06.3f", minutes, seconds);
            timeLabel.setText (timeString, juce::dontSendNotification);
        }
    }
    else if (slider == &volumeSlider)
    {
        // Update volume (slider is 0-100, gain is 0-1)
        float volume = (float) volumeSlider.getValue() / 100.0f;
        if (parentWindow)
        {
            parentWindow->transportSource.setGain (volume);
            DBG ("Volume set to: " + juce::String (volumeSlider.getValue(), 0) + "%");
        }
    }
    else if (slider == &horizontalZoomSlider)
    {
        // Update horizontal zoom
        double zoomValue = horizontalZoomSlider.getValue();
        waveformDisplay.setHorizontalZoom (zoomValue);
        DBG ("Horizontal zoom: " + juce::String (zoomValue, 1) + "x");
    }
    else if (slider == &verticalZoomSlider)
    {
        // Update vertical zoom
        double zoomValue = verticalZoomSlider.getValue();
        waveformDisplay.setVerticalZoom (zoomValue);
        DBG ("Vertical zoom: " + juce::String (zoomValue, 1) + "x");
    }
}

bool StandaloneWindow::MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Spacebar toggles play/pause
    if (key == juce::KeyPress::spaceKey)
    {
        if (parentWindow && parentWindow->currentFile.exists())
        {
            if (parentWindow->isPlaying)
            {
                // Pause playback
                parentWindow->transportSource.stop();
                parentWindow->isPlaying = false;
                playPauseButton.setButtonText ("Play");
                DBG ("Playback paused (spacebar)");
            }
            else
            {
                // Start/resume playback
                parentWindow->transportSource.start();
                parentWindow->isPlaying = true;
                playPauseButton.setButtonText ("Pause");
                DBG ("Playback started (spacebar)");
            }
            return true; // Key was handled
        }
    }

    // Home key - go to beginning
    if (key == juce::KeyPress::homeKey)
    {
        if (parentWindow && parentWindow->currentFile.exists())
        {
            parentWindow->transportSource.setPosition (0.0);
            waveformDisplay.setPlaybackPosition (0.0);
            updatePlaybackPosition (0.0);
            DBG ("Transport: Go to beginning (Home)");
            return true;
        }
    }

    // End key - go to end
    if (key == juce::KeyPress::endKey)
    {
        if (parentWindow && parentWindow->currentFile.exists())
        {
            double length = parentWindow->transportSource.getLengthInSeconds();
            parentWindow->transportSource.setPosition (length);
            waveformDisplay.setPlaybackPosition (1.0);
            updatePlaybackPosition (1.0);
            DBG ("Transport: Go to end (End)");
            return true;
        }
    }

    return false; // Key not handled
}

void StandaloneWindow::MainComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Mouse wheel scrubbing on position slider area
    if (positionSlider.getBounds().contains (event.getPosition()) && parentWindow && currentBuffer != nullptr)
    {
        double currentPos = positionSlider.getValue();
        double delta = wheel.deltaY * 0.01;  // Scroll sensitivity
        double newPos = juce::jlimit (0.0, 1.0, currentPos + delta);

        positionSlider.setValue (newPos, juce::sendNotificationSync);

        // The slider value changed handler will update the transport position
        event.source.enableUnboundedMouseMovement (false);
    }
}

void StandaloneWindow::MainComponent::setAudioBuffer (const juce::AudioBuffer<float>* buffer, double sampleRate)
{
    currentBuffer = buffer;
    currentSampleRate = sampleRate;

    if (buffer != nullptr)
    {
        statusLabel.setText (juce::String (buffer->getNumChannels()) + " channels, " +
                           juce::String (sampleRate / 1000.0, 1) + " kHz, " +
                           juce::String (buffer->getNumSamples() / sampleRate, 2) + " seconds",
                           juce::dontSendNotification);

        // Grab keyboard focus so spacebar works immediately after loading
        grabKeyboardFocus();
    }
}

void StandaloneWindow::MainComponent::updatePlaybackPosition (double position)
{
    positionSlider.setValue (position, juce::dontSendNotification);
    waveformDisplay.setPlaybackPosition (position);
}
