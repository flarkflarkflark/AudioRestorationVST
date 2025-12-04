#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <thread>
#include <atomic>

/**
 * Spek-style Spectrogram Display
 *
 * Full-file spectrogram visualization similar to Spek (https://spek.cc)
 * - Time on X-axis (entire audio file)
 * - Frequency on Y-axis (0 to Nyquist, linear scale)
 * - Intensity as color (multiple palettes available)
 * - Axis labels and dB scale
 */
class SpectrogramDisplay : public juce::Component,
                           public juce::Timer
{
public:
    //==============================================================================
    enum class Palette
    {
        Spectrum,   // Dan Bruton's visible spectrum
        Sox,        // Rob Sykes' SoX palette
        Mono        // Grayscale
    };

    //==============================================================================
    SpectrogramDisplay();
    ~SpectrogramDisplay() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    //==============================================================================
    /** Analyze audio buffer and generate spectrogram */
    void analyzeBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate);

    /** Clear the spectrogram */
    void clear();

    /** Set the color palette */
    void setPalette (Palette newPalette);
    Palette getPalette() const { return currentPalette; }

    /** Set dB range (default: -120 to 0) */
    void setDbRange (float lowerDb, float upperDb);

    /** Set FFT size (must be power of 2, default: 2048) */
    void setFftSize (int newSize);

    /** Get analysis progress (0.0 to 1.0) */
    float getProgress() const { return analysisProgress.load(); }

    /** Check if analysis is running */
    bool isAnalyzing() const { return analyzing.load(); }

private:
    //==============================================================================
    void generateSpectrogram();
    void stopAnalysis();
    juce::Colour getColourForLevel (float level) const;
    juce::Colour getSpectrumPaletteColour (float level) const;
    juce::Colour getSoxPaletteColour (float level) const;
    juce::Colour getMonoPaletteColour (float level) const;

    void drawFrequencyAxis (juce::Graphics& g, juce::Rectangle<int> area);
    void drawTimeAxis (juce::Graphics& g, juce::Rectangle<int> area);
    void drawDbScale (juce::Graphics& g, juce::Rectangle<int> area);

    juce::String formatFrequency (float freq) const;
    juce::String formatTime (double seconds) const;

    //==============================================================================
    // Audio data
    juce::AudioBuffer<float> audioData;
    double audioSampleRate = 44100.0;

    // FFT
    int fftOrder = 11;  // 2048 point FFT
    int fftSize = 1 << fftOrder;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    // Spectrogram image
    juce::Image spectrogramImage;
    std::atomic<bool> imageReady {false};
    std::atomic<bool> analyzing {false};
    std::atomic<float> analysisProgress {0.0f};
    std::unique_ptr<std::thread> analysisThread;

    // Display settings
    Palette currentPalette = Palette::Spectrum;
    float lowerDbRange = -120.0f;
    float upperDbRange = 0.0f;

    // Margins for axes
    static constexpr int leftMargin = 50;    // Frequency axis
    static constexpr int rightMargin = 60;   // dB scale
    static constexpr int topMargin = 10;
    static constexpr int bottomMargin = 30;  // Time axis

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramDisplay)
};
