#pragma once

#include <juce_data_structures/juce_data_structures.h>

class SettingsManager
{
public:
    struct DenoiseSettings
    {
        juce::String provider = "Auto";
        bool allowFallback = true;
        int dmlDeviceId = 0;
        juce::String qnnBackendPath;
        juce::String modelPath;
    };

    static SettingsManager& getInstance();

    DenoiseSettings getDenoiseSettings();
    void setDenoiseSettings (const DenoiseSettings& settings);

    juce::PropertiesFile& getProperties();
    juce::File getSettingsFile();

private:
    SettingsManager();

    juce::ApplicationProperties appProperties;
};
