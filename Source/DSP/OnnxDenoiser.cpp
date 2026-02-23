#include "OnnxDenoiser.h"

#if defined(ENABLE_ONNX_RUNTIME)
#include <onnxruntime_c_api.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(ONNX_RUNTIME_USE_COREML)
#if __has_include(<coreml_provider_factory.h>)
#include <coreml_provider_factory.h>
#define VRS_HAS_COREML_PROVIDER_HEADER 1
#else
#define VRS_HAS_COREML_PROVIDER_HEADER 0
#endif
#else
#define VRS_HAS_COREML_PROVIDER_HEADER 0
#endif

#if defined(ONNX_RUNTIME_USE_DML) && JUCE_WINDOWS
using DmlAppendFn = OrtStatus* (ORT_API_CALL*) (OrtSessionOptions*, int);

static bool appendDmlProvider (Ort::SessionOptions& options, int deviceId)
{
    static juce::DynamicLibrary dmlLibrary;
    static DmlAppendFn appendFn = nullptr;

    if (appendFn == nullptr)
    {
        const juce::StringArray searchNames { "onnxruntime_providers_dml.dll",
                                              "onnxruntime_providers_shared.dll",
                                              "onnxruntime.dll" };

        for (const auto& name : searchNames)
        {
            dmlLibrary.close();
            dmlLibrary.open (name);
            if (dmlLibrary.getNativeHandle() != nullptr)
            {
                appendFn = reinterpret_cast<DmlAppendFn> (dmlLibrary.getFunction ("OrtSessionOptionsAppendExecutionProvider_DML"));
                if (appendFn != nullptr)
                    break;
            }
        }

        if (appendFn == nullptr)
        {
            auto moduleFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            auto baseDir = moduleFile.isDirectory() ? moduleFile : moduleFile.getParentDirectory();

            for (const auto& name : searchNames)
            {
                dmlLibrary.close();
                auto candidate = baseDir.getChildFile (name);
                if (candidate.existsAsFile())
                {
                    dmlLibrary.open (candidate.getFullPathName());
                    if (dmlLibrary.getNativeHandle() != nullptr)
                    {
                        appendFn = reinterpret_cast<DmlAppendFn> (dmlLibrary.getFunction ("OrtSessionOptionsAppendExecutionProvider_DML"));
                        if (appendFn != nullptr)
                            break;
                    }
                }
            }
        }
    }

    if (appendFn == nullptr)
        return false;

    return appendFn (options, deviceId) == nullptr;
}
#endif

#if defined(ENABLE_ONNX_RUNTIME)
static bool ensureQnnProviderLoaded (const juce::String& backendPath)
{
    static juce::DynamicLibrary qnnLibrary;

    if (qnnLibrary.getNativeHandle() != nullptr)
        return true;

    const juce::StringArray searchNames { "onnxruntime_providers_qnn.dll" };

    for (const auto& name : searchNames)
    {
        qnnLibrary.open (name);
        if (qnnLibrary.getNativeHandle() != nullptr)
            return true;
    }

    if (backendPath.isNotEmpty())
    {
        auto backendDir = juce::File (backendPath).getParentDirectory();
        if (backendDir.exists())
        {
            auto candidate = backendDir.getChildFile ("onnxruntime_providers_qnn.dll");
            if (candidate.existsAsFile())
            {
                qnnLibrary.open (candidate.getFullPathName());
                if (qnnLibrary.getNativeHandle() != nullptr)
                    return true;
            }
        }
    }

    auto moduleFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto baseDir = moduleFile.isDirectory() ? moduleFile : moduleFile.getParentDirectory();
    auto candidate = baseDir.getChildFile ("onnxruntime_providers_qnn.dll");
    if (candidate.existsAsFile())
        qnnLibrary.open (candidate.getFullPathName());

    return qnnLibrary.getNativeHandle() != nullptr;
}

static bool appendQnnProvider (Ort::SessionOptions& options, const juce::String& backendPath)
{
    if (backendPath.isEmpty())
        return false;

    if (!juce::File (backendPath).existsAsFile())
        return false;

    ensureQnnProviderLoaded (backendPath);

    const OrtApi& api = Ort::GetApi();
    const char* keys[1] = {};
    const char* values[1] = {};
    size_t numKeys = 0;

    if (backendPath.isNotEmpty())
    {
        keys[0] = "backend_path";
        values[0] = backendPath.toRawUTF8();
        numKeys = 1;
    }

    OrtStatus* status = api.SessionOptionsAppendExecutionProvider (options, "QNN", keys, values, numKeys);
    if (status != nullptr)
    {
        api.ReleaseStatus (status);
        return false;
    }

    return true;
}
#endif

OnnxDenoiser::OnnxDenoiser()
    : env (ORT_LOGGING_LEVEL_WARNING, "VRS_OnnxDenoiser"),
      sessionOptions(),
      memoryInfo (Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault))
{
    sessionOptions.SetIntraOpNumThreads (1);
    sessionOptions.SetInterOpNumThreads (1);
    sessionOptions.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_BASIC);
}

OnnxDenoiser::~OnnxDenoiser()
{
    session.reset();
}

void OnnxDenoiser::prepare (double newSampleRate, int newNumChannels, int maxBlockSize)
{
    hostSampleRate = newSampleRate;
    numChannels = newNumChannels;
    maxBlock = maxBlockSize;

    resampleInRatio = hostSampleRate / modelSampleRate;
    resampleOutRatio = modelSampleRate / hostSampleRate;

    channels.resize ((size_t) numChannels);
    int fifoCapacity = modelFrameSize * 32;
    for (auto& channel : channels)
    {
        channel.resamplerIn.reset();
        channel.resamplerOut.reset();
        channel.inputFifo.resize (fifoCapacity);
        channel.outputFifo.resize (fifoCapacity);
        channel.tempIn.clear();
        channel.tempOut.clear();
        channel.frameIn.resize ((size_t) modelFrameSize);
        channel.frameOut.resize ((size_t) modelFrameSize);
    }

    loadDefaultModelIfNeeded();
}

void OnnxDenoiser::reset()
{
    for (auto& channel : channels)
    {
        channel.resamplerIn.reset();
        channel.resamplerOut.reset();
        channel.inputFifo.clear();
        channel.outputFifo.clear();
    }
}

void OnnxDenoiser::setModelPath (const juce::File& file)
{
    modelPath = file;
    session.reset();
}

void OnnxDenoiser::clearModelPath()
{
    modelPath = juce::File();
    session.reset();
}

void OnnxDenoiser::setPreferredProvider (Provider provider)
{
    preferredProvider = provider;
    session.reset();
}

juce::String OnnxDenoiser::providerToString (Provider provider)
{
    switch (provider)
    {
        case Provider::autoSelect: return "Auto";
        case Provider::cpu: return "CPU";
        case Provider::dml: return "DML";
        case Provider::qnn: return "QNN";
        case Provider::cuda: return "CUDA";
        case Provider::rocm: return "ROCM";
        case Provider::coreml: return "CoreML";
    }

    return "Auto";
}

OnnxDenoiser::Provider OnnxDenoiser::providerFromString (const juce::String& name)
{
    auto normalized = name.trim().toLowerCase();
    if (normalized == "cpu")
        return Provider::cpu;
    if (normalized == "dml" || normalized == "directml")
        return Provider::dml;
    if (normalized == "qnn" || normalized == "npu")
        return Provider::qnn;
    if (normalized == "cuda")
        return Provider::cuda;
    if (normalized == "rocm")
        return Provider::rocm;
    if (normalized == "coreml")
        return Provider::coreml;

    return Provider::autoSelect;
}

bool OnnxDenoiser::loadDefaultModelIfNeeded()
{
    if (session != nullptr)
        return true;

    const auto providers = getProviderFallbackOrder();
    if (modelPath.existsAsFile())
    {
        for (const auto provider : providers)
        {
            if (!isProviderUsable (provider))
                continue;

            if (tryCreateSessionForProvider (modelPath, provider))
                return true;
        }
    }

    const auto candidates = getModelCandidates();

    for (const auto& candidate : candidates)
    {
        if (!candidate.existsAsFile())
            continue;

        for (const auto provider : providers)
        {
            if (!isProviderUsable (provider))
                continue;

            if (tryCreateSessionForProvider (candidate, provider))
            {
                modelPath = candidate;
                return true;
            }
        }
    }

    return false;
}

void OnnxDenoiser::processBlock (juce::AudioBuffer<float>& buffer, float mix)
{
    if (!enabled)
        return;

    if (mix <= 0.0f)
        return;

    if (!loadDefaultModelIfNeeded())
        return;

    mix = juce::jlimit (0.0f, 1.0f, mix);

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = juce::jmin (buffer.getNumChannels(), (int) channels.size());

    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        processChannel (channels[(size_t) ch], channelData, channelData, numSamples, mix);
    }
}

void OnnxDenoiser::processChannel (ChannelState& state,
                                   const float* input,
                                   float* output,
                                   int numSamples,
                                   float mix)
{
    const int targetSamples = (int) std::ceil (numSamples / resampleInRatio);
    if ((int) state.tempIn.size() < targetSamples)
        state.tempIn.resize ((size_t) targetSamples);

    const int producedSamples = state.resamplerIn.process (resampleInRatio,
                                                           input,
                                                           state.tempIn.data(),
                                                           targetSamples);
    state.inputFifo.push (state.tempIn.data(), producedSamples);

    while (state.inputFifo.available() >= modelFrameSize)
    {
        state.inputFifo.pop (state.frameIn.data(), modelFrameSize, false);
        if (runModel (state.frameIn.data(), state.frameOut.data(), modelFrameSize))
            state.outputFifo.push (state.frameOut.data(), modelFrameSize);
        else
            state.outputFifo.push (state.frameIn.data(), modelFrameSize);
    }

    const int requiredModelSamples = (int) std::ceil (resampleOutRatio * numSamples);
    if ((int) state.tempOut.size() < requiredModelSamples)
        state.tempOut.resize ((size_t) requiredModelSamples);

    state.outputFifo.pop (state.tempOut.data(), requiredModelSamples, true);

    state.resamplerOut.process (resampleOutRatio,
                                state.tempOut.data(),
                                output,
                                numSamples);

    if (mix < 1.0f)
    {
        const float dryMix = 1.0f - mix;
        for (int i = 0; i < numSamples; ++i)
            output[i] = input[i] * dryMix + output[i] * mix;
    }
}

bool OnnxDenoiser::runModel (const float* input, float* output, int frameSize)
{
    if (session == nullptr)
        return false;

    try
    {
        auto inputShape = resolveInputShape (frameSize);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float> (memoryInfo,
                                                                  const_cast<float*> (input),
                                                                  (size_t) frameSize,
                                                                  inputShape.data(),
                                                                  inputShape.size());

        const char* inputNames[] = { inputName.c_str() };
        const char* outputNames[] = { outputName.c_str() };

        auto outputTensor = session->Run (Ort::RunOptions { nullptr },
                                          inputNames, &inputTensor, 1,
                                          outputNames, 1);

        if (outputTensor.size() == 0)
            return false;

        float* outputData = outputTensor[0].GetTensorMutableData<float>();
        auto outputInfo = outputTensor[0].GetTensorTypeAndShapeInfo();
        size_t outputCount = outputInfo.GetElementCount();
        size_t samplesToCopy = juce::jmin ((size_t) frameSize, outputCount);
        std::memcpy (output, outputData, sizeof (float) * samplesToCopy);
        if (samplesToCopy < (size_t) frameSize)
            std::fill (output + samplesToCopy, output + frameSize, 0.0f);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool OnnxDenoiser::tryCreateSessionForProvider (const juce::File& file, Provider provider)
{
    if (!file.existsAsFile())
        return false;

    session.reset();

    try
    {
        sessionOptions = Ort::SessionOptions();
        sessionOptions.SetIntraOpNumThreads (1);
        sessionOptions.SetInterOpNumThreads (1);
        sessionOptions.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        bool providerReady = true;
        if (provider == Provider::dml)
        {
           #if defined(ONNX_RUNTIME_USE_DML) && JUCE_WINDOWS
            int deviceId = dmlDeviceId;
            if (deviceId < 0)
            {
                if (const char* envDevice = std::getenv ("VRS_DML_DEVICE_ID"))
                    deviceId = std::atoi (envDevice);
                else
                    deviceId = 0;
            }
            providerReady = appendDmlProvider (sessionOptions, deviceId);
           #else
            providerReady = false;
           #endif
        }
        else if (provider == Provider::qnn)
        {
            juce::String backend = qnnBackendPath;
            if (backend.isEmpty())
                backend = juce::SystemStats::getEnvironmentVariable ("VRS_QNN_BACKEND_PATH", "");
            providerReady = appendQnnProvider (sessionOptions, backend);
        }
        else if (provider == Provider::cuda)
        {
           #if defined(ONNX_RUNTIME_USE_CUDA)
            OrtSessionOptionsAppendExecutionProvider_CUDA (sessionOptions, 0);
           #else
            providerReady = false;
           #endif
        }
        else if (provider == Provider::rocm)
        {
           #if defined(ONNX_RUNTIME_USE_ROCM)
            OrtSessionOptionsAppendExecutionProvider_ROCM (sessionOptions, 0);
           #else
            providerReady = false;
           #endif
        }
        else if (provider == Provider::coreml)
        {
           #if defined(ONNX_RUNTIME_USE_COREML) && VRS_HAS_COREML_PROVIDER_HEADER
            OrtSessionOptionsAppendExecutionProvider_CoreML (sessionOptions, 0);
           #else
            providerReady = false;
           #endif
        }

        if (!providerReady)
            return false;

       #if JUCE_WINDOWS
        auto filePath = file.getFullPathName();
        session = std::make_unique<Ort::Session> (env, filePath.toWideCharPointer(), sessionOptions);
       #else
        session = std::make_unique<Ort::Session> (env, file.getFullPathName().toRawUTF8(), sessionOptions);
       #endif

        Ort::AllocatorWithDefaultOptions allocator;
        inputName = session->GetInputNameAllocated (0, allocator).get();
        outputName = session->GetOutputNameAllocated (0, allocator).get();

        auto inputTypeInfo = session->GetInputTypeInfo (0);
        auto tensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
        modelInputShape = tensorInfo.GetShape();

        activeProvider = provider;
    }
    catch (...)
    {
        session.reset();
        return false;
    }

    return session != nullptr;
}

std::vector<int64_t> OnnxDenoiser::resolveInputShape (int frameSize) const
{
    if (modelInputShape.empty())
        return { 1, frameSize };

    auto shape = modelInputShape;
    for (auto& dim : shape)
    {
        if (dim < 0)
            dim = frameSize;
    }

    if (shape.size() == 1)
        shape[0] = frameSize;
    else if (shape.size() == 2)
    {
        shape[0] = 1;
        shape[1] = frameSize;
    }
    else if (shape.size() == 3)
    {
        shape[0] = 1;
        shape[1] = 1;
        shape[2] = frameSize;
    }

    return shape;
}

void OnnxDenoiser::ensureSession()
{
    if (session == nullptr && modelPath.existsAsFile())
        loadModelInternal (modelPath);
}

bool OnnxDenoiser::loadModelInternal (const juce::File& file)
{
    const auto providers = getProviderFallbackOrder();
    for (const auto provider : providers)
    {
        if (!isProviderUsable (provider))
            continue;

        if (tryCreateSessionForProvider (file, provider))
            return true;
    }

    return false;
}

juce::File OnnxDenoiser::getDefaultModelFile() const
{
    const auto candidates = getModelCandidates();
    for (const auto& candidate : candidates)
    {
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

std::vector<juce::File> OnnxDenoiser::getModelCandidates() const
{
    std::vector<juce::File> candidates;
    juce::StringArray searchDirs;
    if (const char* envDir = std::getenv ("VRS_MODEL_DIR"))
        searchDirs.add (juce::String (envDir));

    auto execFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    if (execFile.exists())
        searchDirs.addIfNotAlreadyThere ((execFile.isDirectory() ? execFile : execFile.getParentDirectory()).getFullPathName());

    auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    if (appFile.exists())
        searchDirs.addIfNotAlreadyThere ((appFile.isDirectory() ? appFile : appFile.getParentDirectory()).getFullPathName());

    juce::StringArray filenamePriority;
    const auto addByProvider = [&filenamePriority](Provider provider)
    {
        switch (provider)
        {
            case Provider::dml: filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_dml.onnx"); break;
            case Provider::qnn: filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_qnn.onnx"); break;
            case Provider::cuda: filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_cuda.onnx"); break;
            case Provider::rocm: filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_rocm.onnx"); break;
            case Provider::coreml: filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_coreml.onnx"); break;
            case Provider::cpu: break;
            case Provider::autoSelect: break;
        }
    };

    if (preferredProvider == Provider::autoSelect)
    {
        addByProvider (Provider::qnn);
        addByProvider (Provider::dml);
        addByProvider (Provider::cuda);
        addByProvider (Provider::rocm);
        addByProvider (Provider::coreml);
    }
    else
    {
        addByProvider (preferredProvider);
    }

    filenamePriority.addIfNotAlreadyThere ("rnnoise_48k_olive_cpu.onnx");
    filenamePriority.addIfNotAlreadyThere ("rnnoise_48k.onnx");

    for (const auto& dir : searchDirs)
    {
        juce::File modelDir (dir);
        auto candidateDir = modelDir.getChildFile ("models");
        for (const auto& filename : filenamePriority)
            candidates.push_back (candidateDir.getChildFile (filename));
    }

    return candidates;
}

bool OnnxDenoiser::isProviderUsable (Provider provider) const
{
    if (provider == Provider::cpu)
        return true;

    if (provider == Provider::dml)
    {
       #if defined(ONNX_RUNTIME_USE_DML) && JUCE_WINDOWS
        return true;
       #else
        return false;
       #endif
    }

    if (provider == Provider::qnn)
        return true;

    if (provider == Provider::cuda)
    {
       #if defined(ONNX_RUNTIME_USE_CUDA)
        return true;
       #else
        return false;
       #endif
    }

    if (provider == Provider::rocm)
    {
       #if defined(ONNX_RUNTIME_USE_ROCM)
        return true;
       #else
        return false;
       #endif
    }

    if (provider == Provider::coreml)
    {
       #if defined(ONNX_RUNTIME_USE_COREML) && VRS_HAS_COREML_PROVIDER_HEADER
        return true;
       #else
        return false;
       #endif
    }

    return false;
}

std::vector<OnnxDenoiser::Provider> OnnxDenoiser::getProviderFallbackOrder() const
{
    if (!allowFallback && preferredProvider != Provider::autoSelect)
        return { preferredProvider };

    if (preferredProvider == Provider::autoSelect)
        return { Provider::qnn, Provider::dml, Provider::cuda, Provider::rocm, Provider::coreml, Provider::cpu };

    std::vector<Provider> order;
    auto appendUnique = [&order](Provider provider)
    {
        if (std::find (order.begin(), order.end(), provider) == order.end())
            order.push_back (provider);
    };

    appendUnique (preferredProvider);
    if (allowFallback)
    {
        appendUnique (Provider::qnn);
        appendUnique (Provider::dml);
        appendUnique (Provider::cuda);
        appendUnique (Provider::rocm);
        appendUnique (Provider::coreml);
        appendUnique (Provider::cpu);
    }

    return order;
}

#else

OnnxDenoiser::OnnxDenoiser() = default;
OnnxDenoiser::~OnnxDenoiser() = default;

void OnnxDenoiser::prepare (double, int, int) {}
void OnnxDenoiser::reset() {}
void OnnxDenoiser::setModelPath (const juce::File&) {}
void OnnxDenoiser::clearModelPath() {}
void OnnxDenoiser::setPreferredProvider (Provider) {}
bool OnnxDenoiser::loadDefaultModelIfNeeded() { return false; }
void OnnxDenoiser::processBlock (juce::AudioBuffer<float>&, float) {}

juce::String OnnxDenoiser::providerToString (Provider) { return "Disabled"; }
OnnxDenoiser::Provider OnnxDenoiser::providerFromString (const juce::String&) { return Provider::cpu; }

#endif

void OnnxDenoiser::RingBuffer::resize (int newCapacity)
{
    capacity = juce::jmax (newCapacity, 1);
    buffer.assign ((size_t) capacity, 0.0f);
    readIndex = 0;
    writeIndex = 0;
    availableSamples = 0;
}

void OnnxDenoiser::RingBuffer::clear()
{
    readIndex = 0;
    writeIndex = 0;
    availableSamples = 0;
    std::fill (buffer.begin(), buffer.end(), 0.0f);
}

void OnnxDenoiser::RingBuffer::push (const float* data, int count)
{
    if (count <= 0)
        return;

    if (count > capacity)
        count = capacity;

    int remaining = count;
    while (remaining > 0)
    {
        int space = capacity - writeIndex;
        int toCopy = juce::jmin (space, remaining);
        std::memcpy (buffer.data() + writeIndex, data + (count - remaining), sizeof (float) * (size_t) toCopy);
        writeIndex = (writeIndex + toCopy) % capacity;
        remaining -= toCopy;
    }

    availableSamples = juce::jmin (capacity, availableSamples + count);
    if (availableSamples == capacity)
        readIndex = writeIndex;
}

int OnnxDenoiser::RingBuffer::pop (float* dest, int count, bool fillWithZeros)
{
    if (count <= 0)
        return 0;

    int toRead = juce::jmin (count, availableSamples);
    int remaining = toRead;
    while (remaining > 0)
    {
        int space = capacity - readIndex;
        int toCopy = juce::jmin (space, remaining);
        std::memcpy (dest + (toRead - remaining), buffer.data() + readIndex, sizeof (float) * (size_t) toCopy);
        readIndex = (readIndex + toCopy) % capacity;
        remaining -= toCopy;
    }

    availableSamples -= toRead;

    if (fillWithZeros && toRead < count)
    {
        std::fill (dest + toRead, dest + count, 0.0f);
    }

    return toRead;
}
