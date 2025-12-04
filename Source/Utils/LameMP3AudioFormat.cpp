#include "LameMP3AudioFormat.h"

#if USE_LAME || USE_MPG123

#if USE_LAME
#include <lame/lame.h>
#endif

#if USE_MPG123
#include <mpg123.h>
#endif

//==============================================================================
// MP3 Reader using mpg123
//==============================================================================
#if USE_MPG123

class LameMP3AudioFormatReader : public juce::AudioFormatReader
{
public:
    LameMP3AudioFormatReader (juce::InputStream* sourceStream, const juce::String& formatName)
        : juce::AudioFormatReader (sourceStream, formatName),
          inputStream (sourceStream)
    {
        // Initialize mpg123
        int err = MPG123_OK;
        mh = mpg123_new (nullptr, &err);

        if (mh == nullptr || err != MPG123_OK)
        {
            DBG ("mpg123 initialization failed");
            return;
        }

        // Set up for feeding data manually
        if (mpg123_open_feed (mh) != MPG123_OK)
        {
            DBG ("mpg123 open_feed failed");
            mpg123_delete (mh);
            mh = nullptr;
            return;
        }

        // Read enough data to get format info
        juce::MemoryBlock initialData;
        initialData.setSize (16384);

        auto bytesRead = inputStream->read (initialData.getData(), (int) initialData.getSize());
        if (bytesRead > 0)
        {
            allData.append (initialData.getData(), bytesRead);

            mpg123_feed (mh, static_cast<const unsigned char*> (initialData.getData()), bytesRead);

            // Try to get format
            long rate;
            int channels, encoding;
            if (mpg123_getformat (mh, &rate, &channels, &encoding) == MPG123_OK)
            {
                sampleRate = static_cast<double> (rate);
                numChannels = static_cast<unsigned int> (channels);
                bitsPerSample = 16;  // mpg123 typically outputs 16-bit
                usesFloatingPointData = false;

                // Read entire file to determine length
                while (!inputStream->isExhausted())
                {
                    juce::MemoryBlock moreData;
                    moreData.setSize (16384);
                    auto moreBytesRead = inputStream->read (moreData.getData(), (int) moreData.getSize());
                    if (moreBytesRead > 0)
                        allData.append (moreData.getData(), moreBytesRead);
                    else
                        break;
                }

                // Feed all data and count samples
                mpg123_close (mh);
                mpg123_open_feed (mh);
                mpg123_feed (mh, static_cast<const unsigned char*> (allData.getData()), allData.getSize());

                // Scan to get length
                size_t done;
                unsigned char buffer[8192];
                juce::int64 totalSamples = 0;

                int result;
                while ((result = mpg123_read (mh, buffer, sizeof (buffer), &done)) == MPG123_OK)
                {
                    totalSamples += done / (2 * numChannels);  // 16-bit samples
                }
                if (result == MPG123_DONE || result == MPG123_NEED_MORE)
                {
                    totalSamples += done / (2 * numChannels);
                }

                lengthInSamples = totalSamples;

                // Reset for reading
                mpg123_close (mh);
                mpg123_open_feed (mh);
                mpg123_feed (mh, static_cast<const unsigned char*> (allData.getData()), allData.getSize());

                isValid = true;
                DBG ("MP3 opened: " + juce::String (sampleRate) + " Hz, " +
                     juce::String (numChannels) + " ch, " +
                     juce::String (lengthInSamples) + " samples");
            }
        }
    }

    ~LameMP3AudioFormatReader() override
    {
        if (mh != nullptr)
        {
            mpg123_close (mh);
            mpg123_delete (mh);
        }
    }

    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        if (!isValid || mh == nullptr)
            return false;

        // Reset and seek to start
        mpg123_close (mh);
        mpg123_open_feed (mh);
        mpg123_feed (mh, static_cast<const unsigned char*> (allData.getData()), allData.getSize());

        // Skip to start position
        std::vector<unsigned char> tempBuffer (8192);
        juce::int64 samplesDecoded = 0;
        size_t done;

        while (samplesDecoded < startSampleInFile)
        {
            int result = mpg123_read (mh, tempBuffer.data(), tempBuffer.size(), &done);
            if (result != MPG123_OK && result != MPG123_NEW_FORMAT)
                break;

            juce::int64 samplesInBuffer = done / (2 * numChannels);
            samplesDecoded += samplesInBuffer;
        }

        // Now read the requested samples
        std::vector<short> outputBuffer (numSamples * numChannels);
        juce::int64 samplesRead = 0;

        while (samplesRead < numSamples)
        {
            int result = mpg123_read (mh, reinterpret_cast<unsigned char*> (outputBuffer.data() + samplesRead * numChannels),
                                      (numSamples - samplesRead) * 2 * numChannels, &done);

            if (result == MPG123_OK || result == MPG123_NEW_FORMAT)
            {
                samplesRead += done / (2 * numChannels);
            }
            else
            {
                break;
            }
        }

        // Convert to int and copy to destination
        for (int ch = 0; ch < numDestChannels && ch < static_cast<int> (numChannels); ++ch)
        {
            if (destSamples[ch] != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    if (i < samplesRead)
                    {
                        short sample = outputBuffer[i * numChannels + ch];
                        destSamples[ch][startOffsetInDestBuffer + i] = sample << 16;
                    }
                    else
                    {
                        destSamples[ch][startOffsetInDestBuffer + i] = 0;
                    }
                }
            }
        }

        return true;
    }

    bool isValid = false;

private:
    mpg123_handle* mh = nullptr;
    juce::InputStream* inputStream;
    juce::MemoryBlock allData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LameMP3AudioFormatReader)
};

#endif // USE_MPG123

//==============================================================================
// MP3 Writer using LAME
//==============================================================================
#if USE_LAME

class LameMP3AudioFormatWriter : public juce::AudioFormatWriter
{
public:
    LameMP3AudioFormatWriter (juce::OutputStream* destStream,
                              double sampleRateToUse,
                              unsigned int numberOfChannels,
                              int quality)
        : juce::AudioFormatWriter (destStream, "MP3", sampleRateToUse, numberOfChannels, 16),
          outputStream (destStream),
          qualityIndex (quality)
    {
        // Initialize LAME
        lame = lame_init();
        if (lame == nullptr)
        {
            DBG ("LAME initialization failed");
            return;
        }

        // Configure LAME
        lame_set_in_samplerate (lame, static_cast<int> (sampleRateToUse));
        lame_set_num_channels (lame, static_cast<int> (numberOfChannels));

        // Quality presets
        // 0 = Low (128 kbps), 1 = Medium (192 kbps), 2 = High (256 kbps), 3 = Best (320 kbps)
        int bitrates[] = {128, 192, 256, 320};
        int bitrate = bitrates[juce::jlimit (0, 3, quality)];

        lame_set_brate (lame, bitrate);
        lame_set_mode (lame, numberOfChannels == 1 ? MONO : JOINT_STEREO);
        lame_set_quality (lame, 2);  // 2 = high quality, reasonably fast

        if (lame_init_params (lame) < 0)
        {
            DBG ("LAME init_params failed");
            lame_close (lame);
            lame = nullptr;
            return;
        }

        mp3Buffer.resize (1.25 * 8192 + 7200);  // Recommended buffer size from LAME docs
        isValid = true;

        DBG ("LAME MP3 writer initialized: " + juce::String (bitrate) + " kbps");
    }

    ~LameMP3AudioFormatWriter() override
    {
        if (lame != nullptr)
        {
            // Flush remaining data
            int bytesWritten = lame_encode_flush (lame, mp3Buffer.data(), static_cast<int> (mp3Buffer.size()));
            if (bytesWritten > 0)
            {
                outputStream->write (mp3Buffer.data(), bytesWritten);
            }

            lame_close (lame);
        }
    }

    bool write (const int** samplesToWrite, int numSamples) override
    {
        if (!isValid || lame == nullptr)
            return false;

        // Convert int samples to float
        std::vector<float> leftChannel (numSamples);
        std::vector<float> rightChannel (numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            leftChannel[i] = samplesToWrite[0][i] / 2147483648.0f;
            if (numChannels > 1)
                rightChannel[i] = samplesToWrite[1][i] / 2147483648.0f;
            else
                rightChannel[i] = leftChannel[i];
        }

        // Encode
        int bytesWritten = lame_encode_buffer_ieee_float (lame,
                                                          leftChannel.data(),
                                                          rightChannel.data(),
                                                          numSamples,
                                                          mp3Buffer.data(),
                                                          static_cast<int> (mp3Buffer.size()));

        if (bytesWritten > 0)
        {
            return outputStream->write (mp3Buffer.data(), bytesWritten);
        }
        else if (bytesWritten < 0)
        {
            DBG ("LAME encoding error: " + juce::String (bytesWritten));
            return false;
        }

        return true;
    }

    bool isValid = false;

private:
    lame_global_flags* lame = nullptr;
    juce::OutputStream* outputStream;
    int qualityIndex;
    std::vector<unsigned char> mp3Buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LameMP3AudioFormatWriter)
};

#endif // USE_LAME

//==============================================================================
// LameMP3AudioFormat implementation
//==============================================================================

LameMP3AudioFormat::LameMP3AudioFormat()
    : juce::AudioFormat ("MP3", ".mp3")
{
#if USE_MPG123
    // Initialize mpg123 library (once)
    static bool mpg123Initialized = false;
    if (!mpg123Initialized)
    {
        mpg123_init();
        mpg123Initialized = true;
        DBG ("mpg123 library initialized");
    }
#endif
}

LameMP3AudioFormat::~LameMP3AudioFormat()
{
}

juce::Array<int> LameMP3AudioFormat::getPossibleSampleRates()
{
    return { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };
}

juce::Array<int> LameMP3AudioFormat::getPossibleBitDepths()
{
    return { 16 };
}

bool LameMP3AudioFormat::canDoStereo()
{
    return true;
}

bool LameMP3AudioFormat::canDoMono()
{
    return true;
}

bool LameMP3AudioFormat::isCompressed()
{
    return true;
}

juce::StringArray LameMP3AudioFormat::getQualityOptions()
{
    return { "Low (128 kbps)", "Medium (192 kbps)", "High (256 kbps)", "Best (320 kbps)" };
}

juce::AudioFormatReader* LameMP3AudioFormat::createReaderFor (juce::InputStream* sourceStream,
                                                               bool deleteStreamIfOpeningFails)
{
#if USE_MPG123
    auto reader = std::make_unique<LameMP3AudioFormatReader> (sourceStream, getFormatName());
    if (reader->isValid)
        return reader.release();

    if (deleteStreamIfOpeningFails)
        delete sourceStream;
#else
    juce::ignoreUnused (sourceStream, deleteStreamIfOpeningFails);
#endif

    return nullptr;
}

std::unique_ptr<juce::AudioFormatWriter> LameMP3AudioFormat::createWriterFor (
    std::unique_ptr<juce::OutputStream>& streamToWriteTo,
    const juce::AudioFormatWriterOptions& options)
{
#if USE_LAME
    // Extract options using getter methods
    double sampleRateToUse = options.getSampleRate();
    unsigned int numberOfChannels = static_cast<unsigned int> (options.getNumChannels());
    int qualityOptionIndex = options.getQualityOptionIndex();

    // Release the stream ownership to create writer (writer takes ownership)
    auto* rawStream = streamToWriteTo.release();

    auto writer = std::make_unique<LameMP3AudioFormatWriter> (rawStream, sampleRateToUse,
                                                               numberOfChannels, qualityOptionIndex);
    if (writer->isValid)
        return writer;

    // If failed, restore stream ownership
    streamToWriteTo.reset (rawStream);
#else
    juce::ignoreUnused (streamToWriteTo, options);
#endif

    return nullptr;
}

#endif // USE_LAME || USE_MPG123
