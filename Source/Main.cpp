#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "GUI/StandaloneWindow.h"

//==============================================================================
/**
 * Vinyl Restoration Suite - Standalone Application
 */
class VinylRestorationApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    VinylRestorationApplication() {}

    const juce::String getApplicationName() override       { return "Vinyl Restoration Suite"; }
    const juce::String getApplicationVersion() override    { return "1.5.2"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);

        // Create main window
        mainWindow.reset (new StandaloneWindow());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
    }

private:
    std::unique_ptr<StandaloneWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (VinylRestorationApplication)
