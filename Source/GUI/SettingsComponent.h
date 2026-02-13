#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/OnnxDenoiser.h"
#include "../Utils/SettingsManager.h"

class SettingsComponent : public juce::Component,
                          private juce::Button::Listener,
                          private juce::ComboBox::Listener
{
public:
    using ApplyCallback = std::function<void()>;
    using ProviderCallback = std::function<juce::String()>;

    SettingsComponent (ApplyCallback onApplyCallback,
                      ProviderCallback activeProviderCallback,
                      bool allowInstallActions);
    ~SettingsComponent() override;

    void resized() override;

private:
    struct ProviderEntry
    {
        juce::String label;
        OnnxDenoiser::Provider provider = OnnxDenoiser::Provider::autoSelect;
    };

    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;

    void loadFromSettings();
    void saveToSettings();
    void refreshDependencyStatus();
    void refreshDmlDeviceList();
    void updateActiveProviderLabel();
    void runProviderTests();

    juce::String getProviderLabel (OnnxDenoiser::Provider provider) const;
    bool isStandalone() const { return allowInstallActions; }

    ApplyCallback onApply;
    ProviderCallback onGetActiveProvider;
    bool allowInstallActions = false;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    juce::Component aiPanel;
    juce::Component depsPanel;
    juce::Component advancedPanel;

    // AI panel
    juce::Label providerLabel;
    juce::ComboBox providerBox;
    juce::ToggleButton fallbackToggle;
    juce::Label aiHintLabel;

    juce::Label dmlDeviceLabel;
    juce::Slider dmlDeviceSlider;
    juce::Label dmlDeviceListLabel;
    juce::TextEditor dmlDeviceListEditor;

    juce::Label qnnBackendLabel;
    juce::TextEditor qnnBackendEditor;
    juce::TextButton qnnBackendBrowseButton { "Browse..." };

    juce::Label modelPathLabel;
    juce::TextEditor modelPathEditor;
    juce::TextButton modelPathBrowseButton { "Browse..." };

    juce::TextButton testProvidersButton { "Test Providers" };
    juce::TextEditor testResultsEditor;

    juce::Label activeProviderLabel;

    // Dependency panel
    juce::Label onnxStatusLabel;
    juce::Label mp3StatusLabel;
    juce::Label vcpkgStatusLabel;
    juce::Label depsHintLabel;
    juce::TextButton refreshDepsButton { "Check Dependencies" };
    juce::TextButton installMp3Button { "Install MP3 (vcpkg)" };

    // Advanced panel
    juce::Label settingsPathLabel;
    juce::TextButton resetButton { "Reset Settings" };

    // Footer
    juce::TextButton applyButton { "Apply" };
    juce::TextButton closeButton { "Close" };

    std::vector<ProviderEntry> providerEntries;
    class ProviderTestThread;
    std::unique_ptr<ProviderTestThread> providerTestThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsComponent)
};
