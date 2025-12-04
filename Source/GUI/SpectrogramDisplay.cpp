#include "SpectrogramDisplay.h"

SpectrogramDisplay::SpectrogramDisplay()
{
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>> (
        fftSize, juce::dsp::WindowingFunction<float>::hann);

    setOpaque (true);
    startTimerHz (30);  // Update at 30fps during analysis
}

SpectrogramDisplay::~SpectrogramDisplay()
{
    stopAnalysis();
    stopTimer();
}

void SpectrogramDisplay::stopAnalysis()
{
    analyzing.store (false);
    if (analysisThread && analysisThread->joinable())
        analysisThread->join();
    analysisThread.reset();
}

void SpectrogramDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Dark background
    g.fillAll (juce::Colour (0xff1a1a2e));

    // Calculate spectrogram area
    auto spectrogramArea = bounds.reduced (leftMargin, topMargin)
                                  .withTrimmedRight (rightMargin)
                                  .withTrimmedBottom (bottomMargin);

    if (imageReady.load() && spectrogramImage.isValid())
    {
        // Draw the spectrogram image
        g.drawImage (spectrogramImage, spectrogramArea.toFloat(),
                     juce::RectanglePlacement::stretchToFit);
    }
    else if (analyzing.load())
    {
        // Show progress
        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        int percent = static_cast<int> (analysisProgress.load() * 100.0f);
        g.drawText ("Analyzing: " + juce::String (percent) + "%",
                    spectrogramArea, juce::Justification::centred);

        // Progress bar
        auto progressBounds = spectrogramArea.withSizeKeepingCentre (200, 8);
        g.setColour (juce::Colour (0xff333355));
        g.fillRoundedRectangle (progressBounds.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff6699ff));
        progressBounds.setWidth (static_cast<int> (progressBounds.getWidth() * analysisProgress.load()));
        g.fillRoundedRectangle (progressBounds.toFloat(), 4.0f);
    }
    else if (audioData.getNumSamples() == 0)
    {
        // No audio loaded
        g.setColour (juce::Colours::grey);
        g.setFont (16.0f);
        g.drawText ("Load an audio file to view spectrogram",
                    spectrogramArea, juce::Justification::centred);
    }

    // Draw border around spectrogram
    g.setColour (juce::Colour (0xff444466));
    g.drawRect (spectrogramArea);

    // Draw axes
    drawFrequencyAxis (g, bounds);
    drawTimeAxis (g, bounds);
    drawDbScale (g, bounds);
}

void SpectrogramDisplay::resized()
{
    // Regenerate spectrogram at new size if we have data
    if (audioData.getNumSamples() > 0 && !analyzing.load())
    {
        generateSpectrogram();
    }
}

void SpectrogramDisplay::timerCallback()
{
    if (analyzing.load())
        repaint();
}

void SpectrogramDisplay::analyzeBuffer (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    stopAnalysis();

    audioData.makeCopyOf (buffer);
    audioSampleRate = sampleRate;
    imageReady.store (false);

    generateSpectrogram();
}

void SpectrogramDisplay::clear()
{
    stopAnalysis();
    audioData.setSize (0, 0);
    spectrogramImage = juce::Image();
    imageReady.store (false);
    repaint();
}

void SpectrogramDisplay::setPalette (Palette newPalette)
{
    if (currentPalette != newPalette)
    {
        currentPalette = newPalette;
        if (audioData.getNumSamples() > 0 && !analyzing.load())
            generateSpectrogram();
    }
}

void SpectrogramDisplay::setDbRange (float lowerDb, float upperDb)
{
    lowerDbRange = lowerDb;
    upperDbRange = upperDb;
    if (audioData.getNumSamples() > 0 && !analyzing.load())
        generateSpectrogram();
}

void SpectrogramDisplay::setFftSize (int newSize)
{
    // Must be power of 2
    int order = static_cast<int> (std::log2 (newSize));
    if ((1 << order) != newSize)
        order = 11; // Default to 2048

    if (order != fftOrder)
    {
        fftOrder = order;
        fftSize = 1 << fftOrder;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        window = std::make_unique<juce::dsp::WindowingFunction<float>> (
            fftSize, juce::dsp::WindowingFunction<float>::hann);

        if (audioData.getNumSamples() > 0 && !analyzing.load())
            generateSpectrogram();
    }
}

void SpectrogramDisplay::generateSpectrogram()
{
    if (audioData.getNumSamples() == 0)
        return;

    stopAnalysis();

    // Calculate image dimensions
    auto bounds = getLocalBounds();
    int imageWidth = bounds.getWidth() - leftMargin - rightMargin;
    int imageHeight = bounds.getHeight() - topMargin - bottomMargin;

    if (imageWidth < 10 || imageHeight < 10)
        return;

    analyzing.store (true);
    analysisProgress.store (0.0f);

    // Start analysis in background thread
    analysisThread = std::make_unique<std::thread> ([this, imageWidth, imageHeight]()
    {
        // Create image
        juce::Image newImage (juce::Image::RGB, imageWidth, imageHeight, true);

        // Mix to mono
        std::vector<float> monoData (audioData.getNumSamples());
        for (int i = 0; i < audioData.getNumSamples(); ++i)
        {
            float sample = 0.0f;
            for (int ch = 0; ch < audioData.getNumChannels(); ++ch)
                sample += audioData.getSample (ch, i);
            monoData[i] = sample / static_cast<float> (audioData.getNumChannels());
        }

        // Calculate hop size to cover entire file in imageWidth columns
        int hopSize = static_cast<int> (monoData.size() / imageWidth);
        hopSize = juce::jmax (1, hopSize);

        // FFT buffers
        std::vector<float> fftData (fftSize * 2, 0.0f);
        std::vector<float> magnitudes (fftSize / 2);

        // Process each column
        for (int col = 0; col < imageWidth; ++col)
        {
            if (!analyzing.load())
                break;

            // Get samples for this column
            int startSample = col * hopSize;

            // Clear and fill FFT buffer
            std::fill (fftData.begin(), fftData.end(), 0.0f);

            for (int i = 0; i < fftSize && (startSample + i) < static_cast<int> (monoData.size()); ++i)
                fftData[i] = monoData[startSample + i];

            // Apply window
            window->multiplyWithWindowingTable (fftData.data(), fftSize);

            // Perform FFT
            fft->performFrequencyOnlyForwardTransform (fftData.data());

            // Calculate magnitudes in dB
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                float magnitude = fftData[bin];
                float db = juce::Decibels::gainToDecibels (magnitude, lowerDbRange);
                magnitudes[bin] = db;
            }

            // Map to image pixels (frequency on Y axis, 0 at bottom)
            for (int row = 0; row < imageHeight; ++row)
            {
                // Map row to frequency bin (bottom = low freq, top = high freq)
                int y = imageHeight - 1 - row;
                float binFloat = static_cast<float> (row) / imageHeight * (fftSize / 2);
                int bin = static_cast<int> (binFloat);
                bin = juce::jlimit (0, fftSize / 2 - 1, bin);

                // Normalize dB to 0-1 range
                float db = magnitudes[bin];
                float level = (db - lowerDbRange) / (upperDbRange - lowerDbRange);
                level = juce::jlimit (0.0f, 1.0f, level);

                // Get color and set pixel
                juce::Colour colour = getColourForLevel (level);
                newImage.setPixelAt (col, y, colour);
            }

            // Update progress
            analysisProgress.store (static_cast<float> (col + 1) / imageWidth);
        }

        if (analyzing.load())
        {
            spectrogramImage = newImage;
            imageReady.store (true);
        }

        analyzing.store (false);

        // Trigger repaint on message thread
        juce::MessageManager::callAsync ([this]() { repaint(); });
    });
}

juce::Colour SpectrogramDisplay::getColourForLevel (float level) const
{
    switch (currentPalette)
    {
        case Palette::Spectrum:
            return getSpectrumPaletteColour (level);
        case Palette::Sox:
            return getSoxPaletteColour (level);
        case Palette::Mono:
            return getMonoPaletteColour (level);
        default:
            return getSpectrumPaletteColour (level);
    }
}

juce::Colour SpectrogramDisplay::getSpectrumPaletteColour (float level) const
{
    // Dan Bruton's visible spectrum algorithm
    float r, g, b;

    if (level < 0.15f)
    {
        // Black to blue
        float t = level / 0.15f;
        r = 0.0f;
        g = 0.0f;
        b = t;
    }
    else if (level < 0.275f)
    {
        // Blue to cyan
        float t = (level - 0.15f) / 0.125f;
        r = 0.0f;
        g = t;
        b = 1.0f;
    }
    else if (level < 0.325f)
    {
        // Cyan to green
        float t = (level - 0.275f) / 0.05f;
        r = 0.0f;
        g = 1.0f;
        b = 1.0f - t;
    }
    else if (level < 0.5f)
    {
        // Green to yellow
        float t = (level - 0.325f) / 0.175f;
        r = t;
        g = 1.0f;
        b = 0.0f;
    }
    else if (level < 0.6625f)
    {
        // Yellow to red
        float t = (level - 0.5f) / 0.1625f;
        r = 1.0f;
        g = 1.0f - t;
        b = 0.0f;
    }
    else
    {
        // Red to white
        float t = (level - 0.6625f) / 0.3375f;
        r = 1.0f;
        g = t;
        b = t;
    }

    return juce::Colour::fromFloatRGBA (r, g, b, 1.0f);
}

juce::Colour SpectrogramDisplay::getSoxPaletteColour (float level) const
{
    // Rob Sykes' SoX palette using sinusoidal transitions
    float r = 0.0f, g = 0.0f, b = 0.0f;

    // Red channel
    if (level >= 0.13f)
    {
        if (level < 0.73f)
            r = std::sin ((level - 0.13f) / 0.60f * juce::MathConstants<float>::halfPi);
        else
            r = 1.0f;
    }

    // Green channel
    if (level >= 0.6f)
    {
        if (level < 0.91f)
            g = std::sin ((level - 0.6f) / 0.31f * juce::MathConstants<float>::halfPi);
        else
            g = 1.0f;
    }

    // Blue channel
    if (level < 0.6f)
    {
        if (level < 0.0f)
            b = 0.5f;
        else
            b = 0.5f + 0.5f * std::sin (level / 0.6f * juce::MathConstants<float>::pi);
    }
    else if (level < 0.78f)
    {
        b = std::cos ((level - 0.6f) / 0.18f * juce::MathConstants<float>::halfPi);
    }

    return juce::Colour::fromFloatRGBA (r, g, b, 1.0f);
}

juce::Colour SpectrogramDisplay::getMonoPaletteColour (float level) const
{
    juce::uint8 v = static_cast<juce::uint8> (level * 255.0f);
    return juce::Colour (v, v, v);
}

void SpectrogramDisplay::drawFrequencyAxis (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Left side frequency axis
    auto axisArea = area.removeFromLeft (leftMargin - 5);
    axisArea.removeFromTop (topMargin);
    axisArea.removeFromBottom (bottomMargin);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (10.0f);

    // Frequency markers
    float nyquist = static_cast<float> (audioSampleRate / 2.0);
    float freqSteps[] = {1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f};

    for (float freq : freqSteps)
    {
        if (freq > nyquist)
            break;

        float normFreq = freq / nyquist;
        int y = axisArea.getBottom() - static_cast<int> (normFreq * axisArea.getHeight());

        // Draw tick
        g.drawHorizontalLine (y, static_cast<float> (axisArea.getRight() - 3),
                              static_cast<float> (axisArea.getRight() + 5));

        // Draw label
        g.drawText (formatFrequency (freq),
                    axisArea.getX(), y - 6, axisArea.getWidth() - 5, 12,
                    juce::Justification::centredRight);
    }

    // Draw "0" at bottom
    g.drawText ("0", axisArea.getX(), axisArea.getBottom() - 6,
                axisArea.getWidth() - 5, 12, juce::Justification::centredRight);
}

void SpectrogramDisplay::drawTimeAxis (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Bottom time axis
    auto axisArea = area.removeFromBottom (bottomMargin - 5);
    axisArea.removeFromLeft (leftMargin);
    axisArea.removeFromRight (rightMargin);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (10.0f);

    double duration = audioData.getNumSamples() / audioSampleRate;

    // Determine appropriate time step
    double timeStep;
    if (duration < 10.0)
        timeStep = 1.0;
    else if (duration < 60.0)
        timeStep = 5.0;
    else if (duration < 300.0)
        timeStep = 30.0;
    else if (duration < 600.0)
        timeStep = 60.0;
    else
        timeStep = 120.0;

    for (double t = 0.0; t <= duration; t += timeStep)
    {
        float normTime = static_cast<float> (t / duration);
        int x = axisArea.getX() + static_cast<int> (normTime * axisArea.getWidth());

        // Draw tick
        g.drawVerticalLine (x, static_cast<float> (axisArea.getY() - 5),
                            static_cast<float> (axisArea.getY() + 3));

        // Draw label
        g.drawText (formatTime (t), x - 25, axisArea.getY() + 3, 50, 15,
                    juce::Justification::centred);
    }
}

void SpectrogramDisplay::drawDbScale (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Right side dB scale/legend
    auto scaleArea = area.removeFromRight (rightMargin - 5);
    scaleArea.removeFromTop (topMargin);
    scaleArea.removeFromBottom (bottomMargin);

    // Draw gradient bar
    auto barArea = scaleArea.removeFromLeft (15);
    barArea.reduce (2, 0);

    for (int y = 0; y < barArea.getHeight(); ++y)
    {
        float level = 1.0f - static_cast<float> (y) / barArea.getHeight();
        juce::Colour colour = getColourForLevel (level);
        g.setColour (colour);
        g.drawHorizontalLine (barArea.getY() + y,
                              static_cast<float> (barArea.getX()),
                              static_cast<float> (barArea.getRight()));
    }

    // Draw border
    g.setColour (juce::Colour (0xff444466));
    g.drawRect (barArea);

    // Draw dB labels
    g.setColour (juce::Colours::lightgrey);
    g.setFont (9.0f);

    float dbRange = upperDbRange - lowerDbRange;
    float dbStep = 20.0f;  // -20 dB steps

    for (float db = upperDbRange; db >= lowerDbRange; db -= dbStep)
    {
        float normDb = (db - lowerDbRange) / dbRange;
        int y = scaleArea.getBottom() - static_cast<int> (normDb * scaleArea.getHeight());

        g.drawText (juce::String (static_cast<int> (db)) + " dB",
                    barArea.getRight() + 3, y - 6, 40, 12,
                    juce::Justification::centredLeft);
    }
}

juce::String SpectrogramDisplay::formatFrequency (float freq) const
{
    if (freq >= 1000.0f)
        return juce::String (freq / 1000.0f, 0) + "k";
    return juce::String (static_cast<int> (freq));
}

juce::String SpectrogramDisplay::formatTime (double seconds) const
{
    int mins = static_cast<int> (seconds / 60.0);
    int secs = static_cast<int> (seconds) % 60;

    if (mins > 0)
        return juce::String (mins) + ":" + juce::String (secs).paddedLeft ('0', 2);
    return juce::String (secs) + "s";
}
