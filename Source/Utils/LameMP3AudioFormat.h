#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

/**
 * MP3 Audio Format using open-source libraries
 *
 * Reading: Uses mpg123 library for high-quality decoding
 * Writing: Uses LAME library for MP3 encoding
 *
 * This provides a patent-free, open-source alternative to proprietary MP3 codecs.
 */

#if USE_LAME || USE_MPG123

class LameMP3AudioFormat : public juce::AudioFormat
{
public:
    LameMP3AudioFormat();
    ~LameMP3AudioFormat() override;

    //==============================================================================
    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool isCompressed() override;
    juce::StringArray getQualityOptions() override;

    //==============================================================================
    juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream,
                                              bool deleteStreamIfOpeningFails) override;

    // JUCE 7+ signature for createWriterFor
    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                              const juce::AudioFormatWriterOptions& options) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LameMP3AudioFormat)
};

#endif // USE_LAME || USE_MPG123
