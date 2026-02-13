#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#if defined(ENABLE_ONNX_RUNTIME)
#include <onnxruntime_cxx_api.h>
#endif

class OnnxDenoiser
{
public:
    enum class Provider
    {
        autoSelect,
        cpu,
        dml,
        qnn,
        cuda,
        rocm,
        coreml
    };

    OnnxDenoiser();
    ~OnnxDenoiser();

    void prepare (double newSampleRate, int newNumChannels, int maxBlockSize);
    void reset();

    void setEnabled (bool shouldEnable) { enabled = shouldEnable; }
    bool isEnabled() const { return enabled; }

    bool isReady() const { return session != nullptr; }

    void setModelPath (const juce::File& file);
    void clearModelPath();
    bool loadDefaultModelIfNeeded();

    void processBlock (juce::AudioBuffer<float>& buffer, float mix);

    void setPreferredProvider (Provider provider);
    Provider getPreferredProvider() const { return preferredProvider; }
    Provider getActiveProvider() const { return activeProvider; }

    void setAllowFallback (bool shouldAllow) { allowFallback = shouldAllow; }
    bool getAllowFallback() const { return allowFallback; }

    void setDmlDeviceId (int deviceId) { dmlDeviceId = deviceId; }
    int getDmlDeviceId() const { return dmlDeviceId; }

    void setQnnBackendPath (const juce::String& path) { qnnBackendPath = path; }
    juce::String getQnnBackendPath() const { return qnnBackendPath; }

    static juce::String providerToString (Provider provider);
    static Provider providerFromString (const juce::String& name);

private:
    struct RingBuffer
    {
        void resize (int newCapacity);
        void clear();
        int available() const { return availableSamples; }
        void push (const float* data, int count);
        int pop (float* dest, int count, bool fillWithZeros);

        std::vector<float> buffer;
        int capacity = 0;
        int readIndex = 0;
        int writeIndex = 0;
        int availableSamples = 0;
    };

    struct ChannelState
    {
        juce::LagrangeInterpolator resamplerIn;
        juce::LagrangeInterpolator resamplerOut;
        RingBuffer inputFifo;
        RingBuffer outputFifo;
        std::vector<float> tempIn;
        std::vector<float> tempOut;
        std::vector<float> frameIn;
        std::vector<float> frameOut;
    };

    void ensureSession();
    bool loadModelInternal (const juce::File& file);
    juce::File getDefaultModelFile() const;
    std::vector<juce::File> getModelCandidates() const;
    std::vector<int64_t> resolveInputShape (int frameSize) const;
    bool tryCreateSessionForProvider (const juce::File& file, Provider provider);
    bool isProviderUsable (Provider provider) const;
    std::vector<Provider> getProviderFallbackOrder() const;

    void processChannel (ChannelState& state, const float* input, float* output, int numSamples, float mix);
    bool runModel (const float* input, float* output, int frameSize);

    double hostSampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 0;
    bool enabled = false;

    static constexpr double modelSampleRate = 48000.0;
    static constexpr int modelFrameSize = 480;

    double resampleInRatio = 1.0;
    double resampleOutRatio = 1.0;

    juce::File modelPath;
    Provider preferredProvider = Provider::autoSelect;
    Provider activeProvider = Provider::cpu;
    bool allowFallback = true;
    int dmlDeviceId = 0;
    juce::String qnnBackendPath;

    std::vector<ChannelState> channels;

#if defined(ENABLE_ONNX_RUNTIME)
    Ort::Env env;
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memoryInfo;

    std::string inputName;
    std::string outputName;
    std::vector<int64_t> modelInputShape;
#endif
};
