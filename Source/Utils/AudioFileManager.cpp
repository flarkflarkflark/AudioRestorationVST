#include "AudioFileManager.h"
#include "LameMP3AudioFormat.h"

AudioFileManager::AudioFileManager()
{
    // Register standard audio formats (WAV, AIFF)
    formatManager.registerBasicFormats();

    // Register additional formats for broader compatibility
    formatManager.registerFormat (new juce::FlacAudioFormat(), true);
    formatManager.registerFormat (new juce::OggVorbisAudioFormat(), true);

    // Register LAME/mpg123 MP3 format (open source, supports read AND write)
    #if USE_LAME || USE_MPG123
    formatManager.registerFormat (new LameMP3AudioFormat(), true);
    DBG ("Registered open-source MP3 format (LAME/mpg123)");
    #elif JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat (new juce::MP3AudioFormat(), true); // Fallback: MP3 read-only
    DBG ("Using JUCE MP3 format (read-only)");
    #endif

    #if JUCE_MAC || JUCE_IOS
    formatManager.registerFormat (new juce::CoreAudioFormat(), true); // CAF, M4A on macOS/iOS
    #endif

    #if JUCE_WINDOWS
    formatManager.registerFormat (new juce::WindowsMediaAudioFormat(), true); // WMA on Windows
    #endif

    DBG ("Registered audio formats: WAV, AIFF, FLAC, OGG, MP3");
}

bool AudioFileManager::loadAudioFile (const juce::File& file,
                                      juce::AudioBuffer<float>& buffer,
                                      double& sampleRate)
{
    // Simple version without progress - call the progress version with null callback
    return loadAudioFileWithProgress (file, buffer, sampleRate, nullptr);
}

bool AudioFileManager::loadAudioFileWithProgress (const juce::File& file,
                                                   juce::AudioBuffer<float>& buffer,
                                                   double& sampleRate,
                                                   ProgressCallback progressCallback)
{
    if (!file.existsAsFile())
    {
        DBG ("Audio file does not exist: " + file.getFullPathName());
        return false;
    }

    // Report initial progress
    if (progressCallback)
    {
        if (!progressCallback (0.0, "Opening file..."))
            return false;  // User cancelled
    }

    // Create reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        DBG ("Could not create reader for file: " + file.getFullPathName());
        return false;
    }

    // Get file properties
    sampleRate = reader->sampleRate;
    auto numChannels = static_cast<int> (reader->numChannels);
    auto lengthInSamples = static_cast<juce::int64> (reader->lengthInSamples);

    double durationSeconds = static_cast<double> (lengthInSamples) / sampleRate;
    double fileSizeMB = file.getSize() / (1024.0 * 1024.0);

    DBG ("Loading audio file:");
    DBG ("  Path: " + file.getFullPathName());
    DBG ("  Sample Rate: " + juce::String (sampleRate) + " Hz");
    DBG ("  Channels: " + juce::String (numChannels));
    DBG ("  Length: " + juce::String (lengthInSamples) + " samples");
    DBG ("  Duration: " + juce::String (durationSeconds, 2) + " seconds");
    DBG ("  File Size: " + juce::String (fileSizeMB, 1) + " MB");

    if (progressCallback)
    {
        juce::String info = juce::String (fileSizeMB, 1) + " MB, " +
                            juce::String (durationSeconds / 60.0, 1) + " min";
        if (!progressCallback (0.05, "Allocating memory (" + info + ")..."))
            return false;
    }

    // Allocate buffer
    buffer.setSize (numChannels, static_cast<int> (lengthInSamples));

    if (progressCallback)
    {
        if (!progressCallback (0.1, "Reading audio data..."))
            return false;
    }

    // Read in chunks with progress reporting
    const juce::int64 chunkSize = 1024 * 1024;  // 1M samples per chunk (~4MB for stereo float)
    juce::int64 samplesRead = 0;

    while (samplesRead < lengthInSamples)
    {
        juce::int64 samplesToRead = juce::jmin (chunkSize, lengthInSamples - samplesRead);

        // Read chunk
        reader->read (&buffer,
                      static_cast<int> (samplesRead),    // destination start sample
                      static_cast<int> (samplesToRead),  // number of samples to read
                      samplesRead,                        // source start sample
                      true,                               // use left channel
                      true);                              // use right channel

        samplesRead += samplesToRead;

        // Report progress (10% to 95% for reading)
        if (progressCallback)
        {
            double progress = 0.1 + 0.85 * (static_cast<double> (samplesRead) / static_cast<double> (lengthInSamples));
            int percent = static_cast<int> (progress * 100.0);
            juce::String status = "Reading audio... " + juce::String (percent) + "%";

            if (!progressCallback (progress, status))
            {
                DBG ("Loading cancelled by user");
                buffer.setSize (0, 0);  // Clear partial buffer
                return false;
            }
        }
    }

    if (progressCallback)
    {
        progressCallback (1.0, "Complete!");
    }

    DBG ("Audio file loaded successfully");
    return true;
}

bool AudioFileManager::saveAudioFile (const juce::File& file,
                                      const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      int bitDepth)
{
    if (buffer.getNumSamples() == 0)
    {
        DBG ("Cannot save empty buffer");
        return false;
    }

    // Delete existing file
    if (file.existsAsFile())
    {
        file.deleteFile();
    }

    // Create output stream
    std::unique_ptr<juce::FileOutputStream> outputStream (file.createOutputStream());

    if (outputStream == nullptr)
    {
        DBG ("Could not create output stream for: " + file.getFullPathName());
        return false;
    }

    // Determine format from file extension
    juce::WavAudioFormat wavFormat;
    juce::FlacAudioFormat flacFormat;
    juce::OggVorbisAudioFormat oggFormat;
    #if USE_LAME || USE_MPG123
    LameMP3AudioFormat mp3Format;
    #endif

    juce::AudioFormat* format = nullptr;
    juce::String extension = file.getFileExtension().toLowerCase();
    int qualityOption = 3;  // Default: Best quality (320 kbps for MP3)

    if (extension == ".wav")
        format = &wavFormat;
    else if (extension == ".flac")
        format = &flacFormat;
    else if (extension == ".ogg")
        format = &oggFormat;
    #if USE_LAME
    else if (extension == ".mp3")
    {
        format = &mp3Format;
        qualityOption = 3;  // Best quality (320 kbps)
        DBG ("Using LAME encoder for MP3 output (320 kbps)");
    }
    #endif
    else
    {
        DBG ("Unsupported file format for writing: " + extension);
        #if USE_LAME
        DBG ("Supported formats: WAV, FLAC, OGG, MP3");
        #else
        DBG ("Supported formats: WAV, FLAC, OGG");
        DBG ("Install LAME for MP3 writing: sudo pacman -S lame");
        #endif
        return false;
    }

    // Create writer
    std::unique_ptr<juce::AudioFormatWriter> writer;
    writer.reset (format->createWriterFor (outputStream.get(),
                                           sampleRate,
                                           (unsigned int) buffer.getNumChannels(),
                                           bitDepth,
                                           {},              // metadata
                                           qualityOption)); // quality option (for MP3: 0=128, 1=192, 2=256, 3=320 kbps)

    if (writer == nullptr)
    {
        DBG ("Could not create writer for: " + file.getFullPathName());
        return false;
    }

    // Release ownership of the stream to the writer
    outputStream.release();

    DBG ("Saving audio file:");
    DBG ("  Path: " + file.getFullPathName());
    DBG ("  Sample Rate: " + juce::String (sampleRate) + " Hz");
    DBG ("  Channels: " + juce::String (buffer.getNumChannels()));
    DBG ("  Length: " + juce::String (buffer.getNumSamples()) + " samples");
    DBG ("  Bit Depth: " + juce::String (bitDepth));

    // Write buffer to file
    bool success = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

    if (success)
        DBG ("Audio file saved successfully");
    else
        DBG ("Failed to write audio data");

    return success;
}

bool AudioFileManager::saveSession (const juce::File& sessionFile,
                                    const juce::File& audioFile,
                                    const juce::var& sessionData)
{
    if (!audioFile.existsAsFile())
    {
        DBG ("Audio file does not exist: " + audioFile.getFullPathName());
        return false;
    }

    // Create JSON object for session
    juce::DynamicObject::Ptr sessionObject = new juce::DynamicObject();

    sessionObject->setProperty ("audioFile", audioFile.getFullPathName());
    sessionObject->setProperty ("timestamp", juce::Time::getCurrentTime().toISO8601 (true));
    sessionObject->setProperty ("fileSize", audioFile.getSize());
    sessionObject->setProperty ("sessionData", sessionData);

    juce::var sessionVar (sessionObject.get());

    // Write JSON to file
    juce::String jsonString = juce::JSON::toString (sessionVar, true);

    bool success = sessionFile.replaceWithText (jsonString);

    if (success)
        DBG ("Session saved: " + sessionFile.getFullPathName());
    else
        DBG ("Failed to save session");

    return success;
}

bool AudioFileManager::loadSession (const juce::File& sessionFile,
                                    juce::File& audioFile,
                                    juce::var& sessionData)
{
    if (!sessionFile.existsAsFile())
    {
        DBG ("Session file does not exist: " + sessionFile.getFullPathName());
        return false;
    }

    // Read JSON from file
    juce::String jsonString = sessionFile.loadFileAsString();
    juce::var sessionVar = juce::JSON::parse (jsonString);

    if (!sessionVar.isObject())
    {
        DBG ("Invalid session file format");
        return false;
    }

    juce::DynamicObject* sessionObject = sessionVar.getDynamicObject();

    if (sessionObject == nullptr)
    {
        DBG ("Could not parse session object");
        return false;
    }

    // Extract audio file path
    juce::String audioFilePath = sessionObject->getProperty ("audioFile").toString();
    audioFile = juce::File (audioFilePath);

    if (!audioFile.existsAsFile())
    {
        DBG ("Audio file referenced in session does not exist: " + audioFilePath);
        return false;
    }

    // Verify file hasn't changed (optional check using file size)
    juce::int64 expectedSize = sessionObject->getProperty ("fileSize");
    juce::int64 actualSize = audioFile.getSize();

    if (expectedSize != actualSize)
    {
        DBG ("Warning: Audio file size has changed since session was saved");
        DBG ("  Expected: " + juce::String (expectedSize) + " bytes");
        DBG ("  Actual: " + juce::String (actualSize) + " bytes");
    }

    // Extract session data
    sessionData = sessionObject->getProperty ("sessionData");

    DBG ("Session loaded: " + sessionFile.getFullPathName());
    DBG ("  Audio file: " + audioFile.getFullPathName());
    DBG ("  Timestamp: " + sessionObject->getProperty ("timestamp").toString());

    return true;
}
