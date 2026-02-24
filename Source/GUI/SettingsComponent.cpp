#include "SettingsComponent.h"

#if JUCE_WINDOWS
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace
{
    juce::File findFileUpwards (juce::File startDir, const juce::String& relativePath, int maxLevels)
    {
        if (!startDir.exists())
            return {};

        for (int level = 0; level <= maxLevels; ++level)
        {
            auto candidate = startDir.getChildFile (relativePath);
            if (candidate.existsAsFile())
                return candidate;

            auto parent = startDir.getParentDirectory();
            if (!parent.exists() || parent == startDir)
                break;
            startDir = parent;
        }

        return {};
    }

    juce::File findExecutableInPath (const juce::String& exeName)
    {
        auto pathVar = juce::SystemStats::getEnvironmentVariable ("PATH", "");
        juce::StringArray dirs;
       #if JUCE_WINDOWS
        dirs.addTokens (pathVar, ";", "");
       #else
        dirs.addTokens (pathVar, ":", "");
       #endif
        dirs.removeEmptyStrings();
        
        juce::String platformExeName = exeName;
       #if JUCE_WINDOWS
        if (!platformExeName.endsWithIgnoreCase (".exe"))
            platformExeName += ".exe";
       #endif

        for (const auto& dir : dirs)
        {
            juce::File candidate = juce::File (dir).getChildFile (platformExeName);
            if (candidate.existsAsFile())
                return candidate;
        }
        return {};
    }

    juce::File findLibraryNearApp (const juce::String& name)
    {
        juce::String libName = name;
       #if !JUCE_WINDOWS
        if (libName.endsWith (".dll"))
            libName = "lib" + libName.upToLastOccurrenceOf (".dll", false, false) + ".so";
       #endif

        juce::Array<juce::File> dirs;
        auto execFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        if (execFile.exists())
            dirs.add (execFile.isDirectory() ? execFile : execFile.getParentDirectory());

        auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        if (appFile.exists())
            dirs.addIfNotAlreadyThere (appFile.isDirectory() ? appFile : appFile.getParentDirectory());

        for (const auto& dir : dirs)
        {
            auto candidate = dir.getChildFile (libName);
            if (candidate.existsAsFile())
                return candidate;
        }
        return {};
    }

    bool hasProviderSymbol (const juce::File& file, const char* symbol)
    {
        if (!file.existsAsFile())
            return false;

        juce::DynamicLibrary lib;
        lib.open (file.getFullPathName());
        return lib.getFunction (symbol) != nullptr;
    }

    bool hasDmlProviderSupport()
    {
       #if !JUCE_WINDOWS
        return false;
       #endif
        auto dmlProvider = findLibraryNearApp ("onnxruntime_providers_dml.dll");
        if (hasProviderSymbol (dmlProvider, "OrtSessionOptionsAppendExecutionProvider_DML"))
            return true;

        auto sharedProvider = findLibraryNearApp ("onnxruntime_providers_shared.dll");
        if (hasProviderSymbol (sharedProvider, "OrtSessionOptionsAppendExecutionProvider_DML"))
            return true;

        auto coreRuntime = findLibraryNearApp ("onnxruntime.dll");
        return hasProviderSymbol (coreRuntime, "OrtSessionOptionsAppendExecutionProvider_DML");
    }

    juce::File findInstallScript()
    {
        juce::String scriptName = 
           #if JUCE_WINDOWS
            "install_vcpkg_mp3.ps1";
           #else
            "install_linux_deps.sh";
           #endif

        auto cwd = juce::File::getCurrentWorkingDirectory();
        auto candidate = cwd.getChildFile ("tools").getChildFile (scriptName);
        if (candidate.existsAsFile())
            return candidate;

        auto execFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        auto baseDir = execFile.isDirectory() ? execFile : execFile.getParentDirectory();
        return findFileUpwards (baseDir, "tools/" + scriptName, 6);
    }

    juce::String formatStatus (const juce::String& name, bool ok)
    {
        return name + ": " + (ok ? "OK" : "Missing");
    }
}

class SettingsComponent::ProviderTestThread : public juce::Thread
{
public:
    ProviderTestThread (SettingsComponent& ownerToUse,
                        const SettingsManager::DenoiseSettings& settingsToUse,
                        const std::vector<ProviderEntry>& providersToUse)
        : juce::Thread ("ProviderTestThread"),
          owner (ownerToUse),
          settings (settingsToUse),
          providers (providersToUse)
    {
    }

    void run() override
    {
        struct ProviderResult
        {
            juce::String name;
            bool success = false;
            double elapsedMs = 0.0;
        };

        std::vector<ProviderResult> results;

        auto buildReport = [&results]()
        {
            juce::String report;
            for (const auto& result : results)
            {
                report += result.name + ": " + (result.success ? "OK" : "Failed");
                if (result.success)
                    report += " (" + juce::String (result.elapsedMs, 2) + " ms)";
                report += "\n";
            }
            return report;
        };

        auto publishReport = [this, &buildReport, &results](bool isFinal)
        {
            auto report = buildReport();
            if (report.isEmpty())
                report = "No results. Test canceled.";
            juce::MessageManager::callAsync ([this, report, isFinal]()
            {
                owner.testResultsEditor.setText (report, juce::dontSendNotification);
                if (isFinal)
                    owner.providerTestThread.reset();
            });
        };

        for (const auto& entry : providers)
        {
            if (threadShouldExit())
                break;

            ProviderResult result;
            result.name = entry.label;
            try
            {
                OnnxDenoiser tester;
                tester.setPreferredProvider (entry.provider);
                tester.setAllowFallback (false);
                tester.setDmlDeviceId (settings.dmlDeviceId);
                tester.setQnnBackendPath (settings.qnnBackendPath);
                if (settings.modelPath.isNotEmpty())
                    tester.setModelPath (juce::File (settings.modelPath));

                tester.prepare (48000.0, 1, 480);
                tester.setEnabled (true);

                juce::AudioBuffer<float> buffer (1, 480);
                buffer.clear();

                auto start = juce::Time::getMillisecondCounterHiRes();
                tester.processBlock (buffer, 1.0f);
                auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;

                result.success = tester.isReady();
                result.elapsedMs = elapsed;
            }
            catch (...)
            {
                result.success = false;
                result.elapsedMs = 0.0;
            }
            results.push_back (result);
            publishReport (false);
        }

        publishReport (true);
    }

private:
    SettingsComponent& owner;
    SettingsManager::DenoiseSettings settings;
    std::vector<ProviderEntry> providers;
};

SettingsComponent::SettingsComponent (ApplyCallback onApplyCallback,
                                      ProviderCallback activeProviderCallback,
                                      bool allowInstallActionsIn)
    : onApply (std::move (onApplyCallback)),
      onGetActiveProvider (std::move (activeProviderCallback)),
      allowInstallActions (allowInstallActionsIn)
{
    providerEntries = {
        { "Auto (best available)", OnnxDenoiser::Provider::autoSelect },
        { "CPU", OnnxDenoiser::Provider::cpu },
        { "DirectML (GPU/NPU)", OnnxDenoiser::Provider::dml },
        { "QNN (NPU)", OnnxDenoiser::Provider::qnn },
        { "CUDA", OnnxDenoiser::Provider::cuda },
        { "ROCM", OnnxDenoiser::Provider::rocm },
        { "CoreML", OnnxDenoiser::Provider::coreml }
    };

    addAndMakeVisible (tabs);
    tabs.addTab ("AI", juce::Colour (0xff2e2e2e), &aiPanel, false);
    tabs.addTab ("Dependencies", juce::Colour (0xff2e2e2e), &depsPanel, false);
    tabs.addTab ("Advanced", juce::Colour (0xff2e2e2e), &advancedPanel, false);

    providerLabel.setText ("Provider", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (providerLabel);

    int providerIndex = 1;
    for (const auto& entry : providerEntries)
        providerBox.addItem (entry.label, providerIndex++);
    providerBox.addListener (this);
    aiPanel.addAndMakeVisible (providerBox);

    fallbackToggle.setButtonText ("Allow fallback if provider fails");
    aiPanel.addAndMakeVisible (fallbackToggle);

    aiHintLabel.setText ("Auto tries QNN -> DML -> CPU. QNN requires a backend DLL from the QNN SDK.", juce::dontSendNotification);
    aiHintLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    aiPanel.addAndMakeVisible (aiHintLabel);

    dmlDeviceLabel.setText ("DirectML Device ID", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (dmlDeviceLabel);
    dmlDeviceSlider.setRange (0, 8, 1);
    dmlDeviceSlider.setSliderStyle (juce::Slider::IncDecButtons);
    dmlDeviceSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    aiPanel.addAndMakeVisible (dmlDeviceSlider);

    dmlDeviceListLabel.setText ("DirectML Devices", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (dmlDeviceListLabel);
    dmlDeviceListEditor.setMultiLine (true);
    dmlDeviceListEditor.setReadOnly (true);
    dmlDeviceListEditor.setReturnKeyStartsNewLine (true);
    dmlDeviceListEditor.setScrollbarsShown (true);
    aiPanel.addAndMakeVisible (dmlDeviceListEditor);

    qnnBackendLabel.setText ("QNN Backend Path", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (qnnBackendLabel);
    aiPanel.addAndMakeVisible (qnnBackendEditor);
    qnnBackendBrowseButton.addListener (this);
    aiPanel.addAndMakeVisible (qnnBackendBrowseButton);

    modelPathLabel.setText ("Model Path (optional)", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (modelPathLabel);
    aiPanel.addAndMakeVisible (modelPathEditor);
    modelPathBrowseButton.addListener (this);
    aiPanel.addAndMakeVisible (modelPathBrowseButton);

    testProvidersButton.addListener (this);
    aiPanel.addAndMakeVisible (testProvidersButton);

    testResultsEditor.setMultiLine (true);
    testResultsEditor.setReadOnly (true);
    testResultsEditor.setText ("Run tests to benchmark providers.\n");
    aiPanel.addAndMakeVisible (testResultsEditor);

    activeProviderLabel.setText ("Active provider: -", juce::dontSendNotification);
    aiPanel.addAndMakeVisible (activeProviderLabel);

    onnxStatusLabel.setText ("ONNX Runtime: -", juce::dontSendNotification);
    depsPanel.addAndMakeVisible (onnxStatusLabel);
    mp3StatusLabel.setText ("MP3: -", juce::dontSendNotification);
    depsPanel.addAndMakeVisible (mp3StatusLabel);
    
   #if JUCE_WINDOWS
    vcpkgStatusLabel.setText ("vcpkg: -", juce::dontSendNotification);
    depsPanel.addAndMakeVisible (vcpkgStatusLabel);

    depsHintLabel.setText ("Use vcpkg to install LAME/mpg123. ONNX providers must be next to the binary.", juce::dontSendNotification);
   #else
    depsHintLabel.setText ("Install LAME and mpg123 via your package manager. ONNX libraries must be in the app folder or /usr/lib.", juce::dontSendNotification);
   #endif
    depsHintLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    depsPanel.addAndMakeVisible (depsHintLabel);

    refreshDepsButton.addListener (this);
    depsPanel.addAndMakeVisible (refreshDepsButton);

    installMp3Button.addListener (this);
    installMp3Button.setButtonText (
       #if JUCE_WINDOWS
        "Install MP3 (vcpkg)"
       #else
        "Install Dependencies"
       #endif
    );
    depsPanel.addAndMakeVisible (installMp3Button);
    installMp3Button.setEnabled (allowInstallActions);

    settingsPathLabel.setText ("Settings file: -", juce::dontSendNotification);
    advancedPanel.addAndMakeVisible (settingsPathLabel);
    resetButton.addListener (this);
    advancedPanel.addAndMakeVisible (resetButton);

    applyButton.addListener (this);
    addAndMakeVisible (applyButton);
    closeButton.addListener (this);
    addAndMakeVisible (closeButton);

    loadFromSettings();
    refreshDependencyStatus();
    updateActiveProviderLabel();
}

SettingsComponent::~SettingsComponent()
{
    providerBox.removeListener (this);
    qnnBackendBrowseButton.removeListener (this);
    modelPathBrowseButton.removeListener (this);
    testProvidersButton.removeListener (this);
    refreshDepsButton.removeListener (this);
    installMp3Button.removeListener (this);
    resetButton.removeListener (this);
    applyButton.removeListener (this);
    closeButton.removeListener (this);
}

void SettingsComponent::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    auto footer = bounds.removeFromBottom (40);

    tabs.setBounds (bounds);

    applyButton.setBounds (footer.removeFromRight (100));
    footer.removeFromRight (8);
    closeButton.setBounds (footer.removeFromRight (100));

    auto aiArea = aiPanel.getLocalBounds().reduced (12);
    auto row = aiArea.removeFromTop (26);
    providerLabel.setBounds (row.removeFromLeft (160));
    providerBox.setBounds (row.removeFromLeft (260));

    aiArea.removeFromTop (8);
    fallbackToggle.setBounds (aiArea.removeFromTop (24));

    aiArea.removeFromTop (4);
    aiHintLabel.setBounds (aiArea.removeFromTop (32));

    aiArea.removeFromTop (8);
    row = aiArea.removeFromTop (26);
    dmlDeviceLabel.setBounds (row.removeFromLeft (160));
    dmlDeviceSlider.setBounds (row.removeFromLeft (200));

    aiArea.removeFromTop (6);
    row = aiArea.removeFromTop (22);
    dmlDeviceListLabel.setBounds (row.removeFromLeft (160));
    aiArea.removeFromTop (4);
    dmlDeviceListEditor.setBounds (aiArea.removeFromTop (72));

    aiArea.removeFromTop (8);
    row = aiArea.removeFromTop (26);
    qnnBackendLabel.setBounds (row.removeFromLeft (160));
    qnnBackendEditor.setBounds (row.removeFromLeft (360));
    row.removeFromLeft (8);
    qnnBackendBrowseButton.setBounds (row.removeFromLeft (100));

    aiArea.removeFromTop (8);
    row = aiArea.removeFromTop (26);
    modelPathLabel.setBounds (row.removeFromLeft (160));
    modelPathEditor.setBounds (row.removeFromLeft (360));
    row.removeFromLeft (8);
    modelPathBrowseButton.setBounds (row.removeFromLeft (100));

    aiArea.removeFromTop (8);
    testProvidersButton.setBounds (aiArea.removeFromTop (28).removeFromLeft (140));

    aiArea.removeFromTop (8);
    activeProviderLabel.setBounds (aiArea.removeFromTop (22));

    aiArea.removeFromTop (8);
    testResultsEditor.setBounds (aiArea);

    auto depsArea = depsPanel.getLocalBounds().reduced (12);
    onnxStatusLabel.setBounds (depsArea.removeFromTop (22));
    depsArea.removeFromTop (6);
    mp3StatusLabel.setBounds (depsArea.removeFromTop (22));
    depsArea.removeFromTop (6);
    vcpkgStatusLabel.setBounds (depsArea.removeFromTop (22));
    depsArea.removeFromTop (12);

    depsHintLabel.setBounds (depsArea.removeFromTop (36));
    depsArea.removeFromTop (8);

    auto depsRow = depsArea.removeFromTop (28);
    refreshDepsButton.setBounds (depsRow.removeFromLeft (170));
    depsRow.removeFromLeft (8);
    installMp3Button.setBounds (depsRow.removeFromLeft (170));

    auto advArea = advancedPanel.getLocalBounds().reduced (12);
    settingsPathLabel.setBounds (advArea.removeFromTop (22));
    advArea.removeFromTop (12);
    resetButton.setBounds (advArea.removeFromTop (28).removeFromLeft (160));
}

void SettingsComponent::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &providerBox)
        updateActiveProviderLabel();
}

void SettingsComponent::buttonClicked (juce::Button* button)
{
    if (button == &applyButton)
    {
        saveToSettings();
        if (onApply)
            onApply();
        updateActiveProviderLabel();
    }
    else if (button == &closeButton)
    {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);
    }
    else if (button == &qnnBackendBrowseButton)
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Select QNN backend library");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser&)
            {
                auto file = chooser->getResult();
                if (file.existsAsFile())
                    qnnBackendEditor.setText (file.getFullPathName());
            });
    }
    else if (button == &modelPathBrowseButton)
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Select ONNX model");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser] (const juce::FileChooser&)
            {
                auto file = chooser->getResult();
                if (file.existsAsFile())
                    modelPathEditor.setText (file.getFullPathName());
            });
    }
    else if (button == &testProvidersButton)
    {
        runProviderTests();
    }
    else if (button == &refreshDepsButton)
    {
        refreshDependencyStatus();
    }
    else if (button == &installMp3Button)
    {
       #if JUCE_WINDOWS
        if (!allowInstallActions)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::InfoIcon,
                "Install MP3 Dependencies",
                "Run tools\\install_vcpkg_mp3.ps1 from the project folder to install LAME and mpg123.");
            return;
        }

        auto script = findInstallScript();
        if (!script.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Install MP3 Dependencies",
                "install_vcpkg_mp3.ps1 not found. Run it from the source repo under tools\\.");
            return;
        }

        juce::ChildProcess proc;
        auto vcpkgRoot = juce::SystemStats::getEnvironmentVariable ("VCPKG_ROOT", "");
        if (vcpkgRoot.isEmpty())
        {
            auto defaultVcpkg = juce::File ("C:\\vcpkg\\vcpkg.exe");
            if (defaultVcpkg.existsAsFile())
                vcpkgRoot = defaultVcpkg.getParentDirectory().getFullPathName();
        }

        juce::String command;
        if (vcpkgRoot.isNotEmpty())
        {
            auto escapedRoot = vcpkgRoot.replaceCharacter ('\'', '`');
            auto escapedScript = script.getFullPathName().replaceCharacter ('\'', '`');
            command = "powershell -ExecutionPolicy Bypass -Command \"$env:VCPKG_ROOT='"
                      + escapedRoot + "'; & '" + escapedScript + "'\"";
        }
        else
        {
            command = "powershell -ExecutionPolicy Bypass -File \"" + script.getFullPathName() + "\"";
        }
        if (!proc.start (command))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Install MP3 Dependencies",
                "Failed to start installer process.");
            return;
        }

        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::InfoIcon,
            "Install MP3 Dependencies",
            "Installer started in background. Re-run 'Check Dependencies' when finished.");
       #else
        juce::String message = "To install missing dependencies on Linux, run:\n\n"
                               "Ubuntu/Debian:\nsudo apt install lame libmpg123-dev\n\n"
                               "Arch Linux:\nsudo pacman -S lame mpg123\n\n"
                               "Fedora:\nsudo dnf install lame mpg123-devel";
        
        auto script = findInstallScript();
        if (script.existsAsFile())
        {
            juce::ChildProcess proc;
            if (proc.start ("bash \"" + script.getFullPathName() + "\""))
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::InfoIcon,
                    "Installing Dependencies",
                    "Installation script started in background.\n\n" + message);
                return;
            }
        }

        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::InfoIcon,
            "Install Dependencies",
            message);
       #endif
    }
    else if (button == &resetButton)
    {
        SettingsManager::DenoiseSettings defaults;
        SettingsManager::getInstance().setDenoiseSettings (defaults);
        loadFromSettings();
        if (onApply)
            onApply();
    }
}

void SettingsComponent::loadFromSettings()
{
    const auto settings = SettingsManager::getInstance().getDenoiseSettings();
    auto provider = OnnxDenoiser::providerFromString (settings.provider);

    for (int i = 0; i < (int) providerEntries.size(); ++i)
    {
        if (providerEntries[(size_t) i].provider == provider)
        {
            providerBox.setSelectedId (i + 1, juce::dontSendNotification);
            break;
        }
    }

    fallbackToggle.setToggleState (settings.allowFallback, juce::dontSendNotification);
    dmlDeviceSlider.setValue (settings.dmlDeviceId, juce::dontSendNotification);
    qnnBackendEditor.setText (settings.qnnBackendPath, juce::dontSendNotification);
    modelPathEditor.setText (settings.modelPath, juce::dontSendNotification);

    settingsPathLabel.setText ("Settings file: " + SettingsManager::getInstance().getSettingsFile().getFullPathName(),
                               juce::dontSendNotification);
}

void SettingsComponent::saveToSettings()
{
    SettingsManager::DenoiseSettings settings;
    int index = providerBox.getSelectedId() - 1;
    if (index >= 0 && index < (int) providerEntries.size())
        settings.provider = OnnxDenoiser::providerToString (providerEntries[(size_t) index].provider);

    settings.allowFallback = fallbackToggle.getToggleState();
    settings.dmlDeviceId = (int) dmlDeviceSlider.getValue();
    settings.qnnBackendPath = qnnBackendEditor.getText().trim();
    settings.modelPath = modelPathEditor.getText().trim();

    SettingsManager::getInstance().setDenoiseSettings (settings);
}

void SettingsComponent::refreshDependencyStatus()
{
    auto ortQnn = findLibraryNearApp ("onnxruntime_providers_qnn.dll");
    auto ortCore = findLibraryNearApp ("onnxruntime.dll");
    
    // Also check system paths on Linux
   #if !JUCE_WINDOWS
    if (!ortCore.existsAsFile())
        ortCore = juce::File ("/usr/lib/libonnxruntime.so");
    if (!ortCore.existsAsFile())
        ortCore = juce::File ("/usr/local/lib/libonnxruntime.so");
   #endif

    auto dmlAvailable = hasDmlProviderSupport();
    auto qnnBackendPath = SettingsManager::getInstance().getDenoiseSettings().qnnBackendPath;
    if (qnnBackendPath.isEmpty())
        qnnBackendPath = juce::SystemStats::getEnvironmentVariable ("VRS_QNN_BACKEND_PATH", "");
    bool qnnBackendOk = false;
    bool qnnProviderOk = ortQnn.existsAsFile();

    if (qnnBackendPath.isNotEmpty())
    {
        juce::File backendFile (qnnBackendPath);
        qnnBackendOk = backendFile.existsAsFile();
        if (!qnnProviderOk && backendFile.existsAsFile())
        {
            auto providerFromBackend = backendFile.getParentDirectory().getChildFile (
               #if JUCE_WINDOWS
                "onnxruntime_providers_qnn.dll"
               #else
                "libonnxruntime_providers_qnn.so"
               #endif
            );
            qnnProviderOk = providerFromBackend.existsAsFile();
        }
    }

    juce::String qnnStatus;
    if (qnnProviderOk && qnnBackendOk)
        qnnStatus = "OK";
    else if (!qnnProviderOk && qnnBackendOk)
        qnnStatus = "Missing (provider DLL)";
    else if (qnnProviderOk && !qnnBackendOk && qnnBackendPath.isNotEmpty())
        qnnStatus = "Missing (backend)";
    else
        qnnStatus = "Missing";

    onnxStatusLabel.setText (
        "ONNX Runtime: " + juce::String (ortCore.existsAsFile() ? "OK" : "Missing") +
            ", DML: " + juce::String (dmlAvailable ? "OK" : "Missing") +
            ", QNN: " + qnnStatus,
        juce::dontSendNotification);

   #if defined(USE_LAME) || defined(USE_MPG123)
    juce::String mp3Status = "MP3 support: Enabled (";
   #if defined(USE_LAME)
    mp3Status += "LAME";
   #else
    mp3Status += "LAME missing";
   #endif
    mp3Status += ", ";
   #if defined(USE_MPG123)
    mp3Status += "mpg123";
   #else
    mp3Status += "mpg123 missing";
   #endif
    mp3Status += ")";
   #else
    juce::String mp3Status = "MP3 support: Disabled (missing LAME/mpg123 at build time)";
   #endif

    mp3StatusLabel.setText (mp3Status, juce::dontSendNotification);

   #if JUCE_WINDOWS
    auto vcpkgRoot = juce::SystemStats::getEnvironmentVariable ("VCPKG_ROOT", "");
    auto vcpkgExe = findExecutableInPath ("vcpkg.exe");
    if (!vcpkgExe.existsAsFile())
    {
        auto defaultVcpkg = juce::File ("C:\\vcpkg\\vcpkg.exe");
        if (defaultVcpkg.existsAsFile())
            vcpkgExe = defaultVcpkg;
    }
    juce::String vcpkgStatus = "vcpkg: ";
    if (vcpkgExe.existsAsFile())
        vcpkgStatus += "Found (" + vcpkgExe.getFullPathName() + ")";
    else if (vcpkgRoot.isNotEmpty())
        vcpkgStatus += "VCPKG_ROOT set (" + vcpkgRoot + ")";
    else
        vcpkgStatus += "Not found";
    vcpkgStatusLabel.setText (vcpkgStatus, juce::dontSendNotification);
   #endif

    refreshDmlDeviceList();
}

void SettingsComponent::refreshDmlDeviceList()
{
   #if JUCE_WINDOWS
    juce::StringArray lines;
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1 (__uuidof (IDXGIFactory1), reinterpret_cast<void**> (&factory));
    if (SUCCEEDED (hr) && factory != nullptr)
    {
        int deviceCount = 0;
        for (UINT index = 0; ; ++index)
        {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1 (index, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;

            DXGI_ADAPTER_DESC1 desc {};
            if (SUCCEEDED (adapter->GetDesc1 (&desc)))
            {
                const double bytesToMb = 1024.0 * 1024.0;
                const double dedicatedMb = static_cast<double> (desc.DedicatedVideoMemory) / bytesToMb;
                const double sharedMb = static_cast<double> (desc.SharedSystemMemory) / bytesToMb;
                juce::String memoryText = juce::String (dedicatedMb, 0) + " MB";
                if (dedicatedMb < 1.0 && sharedMb > 0.0)
                    memoryText = juce::String (sharedMb, 0) + " MB shared";

                lines.add (juce::String (index) + ": " + juce::String (desc.Description) + " (" + memoryText + ")");
                ++deviceCount;
            }

            adapter->Release();
        }

        factory->Release();

        if (deviceCount > 0)
            dmlDeviceSlider.setRange (0, deviceCount - 1, 1);
    }

    if (lines.isEmpty())
        lines.add ("No DirectML-capable devices detected.");

    dmlDeviceListEditor.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);
   #else
    dmlDeviceListEditor.setText ("DirectML devices list is available on Windows only.", juce::dontSendNotification);
   #endif
}

void SettingsComponent::updateActiveProviderLabel()
{
    juce::String active = onGetActiveProvider ? onGetActiveProvider() : "-";
    activeProviderLabel.setText ("Active provider: " + active, juce::dontSendNotification);
}

void SettingsComponent::runProviderTests()
{
    SettingsManager::DenoiseSettings settings = SettingsManager::getInstance().getDenoiseSettings();
    testResultsEditor.setText ("Running provider tests...\n");

    if (providerTestThread != nullptr)
        providerTestThread->signalThreadShouldExit();

    providerTestThread = std::make_unique<ProviderTestThread> (*this, settings, providerEntries);
    providerTestThread->startThread();
}
