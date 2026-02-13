#include "SettingsManager.h"

namespace
{
    constexpr const char* kProviderKey = "aiProvider";
    constexpr const char* kFallbackKey = "aiAllowFallback";
    constexpr const char* kDmlDeviceKey = "aiDmlDeviceId";
    constexpr const char* kQnnBackendKey = "aiQnnBackendPath";
    constexpr const char* kModelPathKey = "aiModelPath";
}

SettingsManager& SettingsManager::getInstance()
{
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "VinylRestorationSuite";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "VinylRestorationSuite";
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;

    appProperties.setStorageParameters (options);
    appProperties.getUserSettings()->saveIfNeeded();
}

SettingsManager::DenoiseSettings SettingsManager::getDenoiseSettings()
{
    auto* props = appProperties.getUserSettings();
    DenoiseSettings settings;
    settings.provider = props->getValue (kProviderKey, "Auto");
    settings.allowFallback = props->getBoolValue (kFallbackKey, true);
    settings.dmlDeviceId = props->getIntValue (kDmlDeviceKey, 0);
    settings.qnnBackendPath = props->getValue (kQnnBackendKey, "");
    settings.modelPath = props->getValue (kModelPathKey, "");
    return settings;
}

void SettingsManager::setDenoiseSettings (const DenoiseSettings& settings)
{
    auto* props = appProperties.getUserSettings();
    props->setValue (kProviderKey, settings.provider);
    props->setValue (kFallbackKey, settings.allowFallback);
    props->setValue (kDmlDeviceKey, settings.dmlDeviceId);
    props->setValue (kQnnBackendKey, settings.qnnBackendPath);
    props->setValue (kModelPathKey, settings.modelPath);
    props->saveIfNeeded();
}

juce::PropertiesFile& SettingsManager::getProperties()
{
    return *appProperties.getUserSettings();
}

juce::File SettingsManager::getSettingsFile()
{
    auto* props = appProperties.getUserSettings();
    return props != nullptr ? props->getFile() : juce::File();
}
