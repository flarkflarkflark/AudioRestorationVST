# NPU Implementation voor Vinyl Restoration VST

## Context

Flark ontwikkelt een AudioRestorationVST in JUCE voor vinyl restoration. We willen de NPU (Neural Processing Unit) via DirectML gebruiken voor real-time click removal en noise reduction. 

### Waarom NPU?

1. NPU is geschikt voor continue, lage-latency ML inferentie
2. GPU blijft vrij voor andere DAW taken
3. Vinyl restoration specifieke ML modellen zijn relatief compact
4. Energie-efficiënt: ~5W vs ~50W voor GPU
5. Parallellisme mogelijk voor L/R channels

---

## Architectuur

### VST3 Plugin Structuur

```
AudioRestorationVST/
├── Source/
│   ├── PluginProcessor.h/cpp      # JUCE audio processing
│   ├── NPUManager.h/cpp            # NPU/DirectML wrapper
│   ├── VinylRestoration.h/cpp      # Restoration algorithms
│   └── Models/
│       ├── click_detector.onnx     # Click detection model
│       ├── click_interpolator.onnx # Click repair model
│       └── vinyl_denoiser.onnx     # Noise reduction model
├── CMakeLists.txt
└── README.md
```

### Data Flow

```
Audio Input (vinyl) 
  → JUCE processBlock() 
  → Frame buffering
  → NPU inference (ONNX Runtime + DirectML)
  → Post-processing
  → Audio Output (restored)
```

---

## Implementatie

### 1. NPU Manager Class

**NPUManager.h**

```cpp
#pragma once
#include <JuceHeader.h>
#include <onnxruntime_cxx_api.h>
#include <dml_provider_factory.h>

class NPUManager {
public:
    NPUManager();
    ~NPUManager();
    
    bool initialize();
    bool loadModel(const juce::File& modelPath, const juce::String& modelName);
    
    // Process audio frames through NPU
    std::vector<float> processFrame(
        const std::vector<float>& inputFrame,
        const juce::String& modelName
    );
    
    bool isNPUAvailable() const { return npuAvailable; }
    float getProcessingTime() const { return lastProcessingTimeMs; }
    
private:
    Ort::Env env;
    Ort::SessionOptions sessionOptions;
    std::map<juce::String, std::unique_ptr<Ort::Session>> sessions;
    
    bool npuAvailable = false;
    float lastProcessingTimeMs = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NPUManager)
};
```

**NPUManager.cpp**

```cpp
#include "NPUManager.h"

NPUManager::NPUManager() 
    : env(ORT_LOGGING_LEVEL_WARNING, "VinylRestorationNPU") 
{
}

bool NPUManager::initialize() {
    try {
        // Enable DirectML (NPU) execution provider
        sessionOptions.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL
        );
        
        // Append DirectML provider - dit target de NPU
        const OrtApi& ortApi = Ort::GetApi();
        OrtDmlApi* dmlApi;
        ortApi.GetExecutionProviderApi("DML", ORT_API_VERSION, 
                                       reinterpret_cast<const void**>(&dmlApi));
        
        sessionOptions.AppendExecutionProvider_DML(0); // Device 0 = NPU
        
        npuAvailable = true;
        DBG("NPU initialized successfully via DirectML");
        return true;
        
    } catch (const Ort::Exception& e) {
        DBG("NPU initialization failed: " + juce::String(e.what()));
        npuAvailable = false;
        return false;
    }
}

bool NPUManager::loadModel(const juce::File& modelPath, 
                           const juce::String& modelName) {
    try {
        auto session = std::make_unique<Ort::Session>(
            env,
            modelPath.getFullPathName().toWideCharPointer(),
            sessionOptions
        );
        
        sessions[modelName] = std::move(session);
        DBG("Loaded model: " + modelName);
        return true;
        
    } catch (const Ort::Exception& e) {
        DBG("Failed to load model " + modelName + ": " + juce::String(e.what()));
        return false;
    }
}

std::vector<float> NPUManager::processFrame(
    const std::vector<float>& inputFrame,
    const juce::String& modelName) 
{
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    
    auto sessionIt = sessions.find(modelName);
    if (sessionIt == sessions.end()) {
        return inputFrame; // Passthrough if model not found
    }
    
    auto& session = sessionIt->second;
    
    try {
        // Input tensor shape: [1, frame_size]
        std::vector<int64_t> inputShape = {1, (int64_t)inputFrame.size()};
        
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault
        );
        
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            const_cast<float*>(inputFrame.data()),
            inputFrame.size(),
            inputShape.data(),
            inputShape.size()
        );
        
        // Run inference
        const char* inputNames[] = {"audio_input"};
        const char* outputNames[] = {"audio_output"};
        
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNames, &inputTensor, 1,
            outputNames, 1
        );
        
        // Extract output
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        size_t outputSize = outputTensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
        
        std::vector<float> output(outputData, outputData + outputSize);
        
        lastProcessingTimeMs = juce::Time::getMillisecondCounterHiRes() - startTime;
        
        return output;
        
    } catch (const Ort::Exception& e) {
        DBG("NPU inference failed: " + juce::String(e.what()));
        return inputFrame; // Fallback to passthrough
    }
}
```

---

### 2. Vinyl Restoration Processing

**VinylRestoration.h**

```cpp
#pragma once
#include "NPUManager.h"
#include <JuceHeader.h>

class VinylRestoration {
public:
    VinylRestoration();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    
    // Main processing function
    void processBlock(juce::AudioBuffer<float>& buffer);
    
    // Enable/disable specific restoration features
    void setClickRemovalEnabled(bool enabled) { clickRemovalEnabled = enabled; }
    void setNoiseReductionEnabled(bool enabled) { noiseReductionEnabled = enabled; }
    
    // Parameters
    void setClickSensitivity(float sensitivity) { clickSensitivity = sensitivity; }
    void setNoiseReductionAmount(float amount) { noiseAmount = amount; }
    
    bool isNPUActive() const { return npuManager.isNPUAvailable(); }
    
private:
    NPUManager npuManager;
    
    bool clickRemovalEnabled = true;
    bool noiseReductionEnabled = true;
    
    float clickSensitivity = 0.5f;
    float noiseAmount = 0.5f;
    
    double currentSampleRate = 44100.0;
    int hopSize = 512; // Frame size for NPU processing
    
    // Buffering for frame-based processing
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    int bufferPosition = 0;
    
    // Processing methods
    void processClickRemoval(juce::AudioBuffer<float>& buffer);
    void processNoiseReduction(juce::AudioBuffer<float>& buffer);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VinylRestoration)
};
```

**VinylRestoration.cpp**

```cpp
#include "VinylRestoration.h"

VinylRestoration::VinylRestoration() {
    // Initialize NPU
    if (npuManager.initialize()) {
        // Load models from plugin resources
        auto modelsDir = juce::File::getSpecialLocation(
            juce::File::currentApplicationFile
        ).getChildFile("Models");
        
        npuManager.loadModel(
            modelsDir.getChildFile("click_detector.onnx"), 
            "click_detector"
        );
        npuManager.loadModel(
            modelsDir.getChildFile("click_interpolator.onnx"),
            "click_interpolator"
        );
        npuManager.loadModel(
            modelsDir.getChildFile("vinyl_denoiser.onnx"),
            "denoiser"
        );
    }
}

void VinylRestoration::prepare(double sampleRate, int samplesPerBlock) {
    currentSampleRate = sampleRate;
    
    // Buffer for frame-based processing
    inputBuffer.setSize(2, hopSize * 2);
    outputBuffer.setSize(2, hopSize * 2);
    
    reset();
}

void VinylRestoration::reset() {
    inputBuffer.clear();
    outputBuffer.clear();
    bufferPosition = 0;
}

void VinylRestoration::processBlock(juce::AudioBuffer<float>& buffer) {
    if (!npuManager.isNPUAvailable()) {
        return; // Bypass if NPU not available
    }
    
    // Process in order: click removal first, then noise reduction
    if (clickRemovalEnabled) {
        processClickRemoval(buffer);
    }
    
    if (noiseReductionEnabled) {
        processNoiseReduction(buffer);
    }
}

void VinylRestoration::processClickRemoval(juce::AudioBuffer<float>& buffer) {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel) {
        auto* channelData = buffer.getWritePointer(channel);
        
        // Process in frames of hopSize
        for (int i = 0; i < numSamples; i += hopSize) {
            int frameSamples = juce::jmin(hopSize, numSamples - i);
            
            // Prepare input frame
            std::vector<float> inputFrame(channelData + i, 
                                         channelData + i + frameSamples);
            
            // 1. Detect clicks
            auto clickMask = npuManager.processFrame(inputFrame, "click_detector");
            
            // 2. For detected clicks, interpolate
            bool hasClicks = false;
            for (float mask : clickMask) {
                if (mask > clickSensitivity) {
                    hasClicks = true;
                    break;
                }
            }
            
            if (hasClicks) {
                auto repairedFrame = npuManager.processFrame(
                    inputFrame, 
                    "click_interpolator"
                );
                
                // Copy repaired audio back
                std::copy(repairedFrame.begin(), repairedFrame.end(), 
                         channelData + i);
            }
        }
    }
}

void VinylRestoration::processNoiseReduction(juce::AudioBuffer<float>& buffer) {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < numChannels; ++channel) {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < numSamples; i += hopSize) {
            int frameSamples = juce::jmin(hopSize, numSamples - i);
            
            std::vector<float> inputFrame(channelData + i, 
                                         channelData + i + frameSamples);
            
            // Denoise via NPU
            auto denoisedFrame = npuManager.processFrame(inputFrame, "denoiser");
            
            // Mix based on noiseAmount parameter
            for (int j = 0; j < frameSamples; ++j) {
                channelData[i + j] = inputFrame[j] * (1.0f - noiseAmount) +
                                    denoisedFrame[j] * noiseAmount;
            }
        }
    }
}
```

---

### 3. Integration in Plugin Processor

**PluginProcessor.h additions**

```cpp
class AudioRestorationVSTProcessor : public juce::AudioProcessor {
public:
    // ... existing code ...
    
private:
    VinylRestoration vinylRestoration;
    
    // Parameters
    juce::AudioParameterBool* clickRemovalParam;
    juce::AudioParameterFloat* clickSensitivityParam;
    juce::AudioParameterBool* noiseReductionParam;
    juce::AudioParameterFloat* noiseAmountParam;
};
```

**PluginProcessor.cpp**

```cpp
void AudioRestorationVSTProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) 
{
    juce::ScopedNoDenormals noDenormals;
    
    // Update parameters
    vinylRestoration.setClickRemovalEnabled(clickRemovalParam->get());
    vinylRestoration.setClickSensitivity(clickSensitivityParam->get());
    vinylRestoration.setNoiseReductionEnabled(noiseReductionParam->get());
    vinylRestoration.setNoiseReductionAmount(noiseAmountParam->get());
    
    // Process via NPU
    vinylRestoration.processBlock(buffer);
}
```

---

## ML Modellen voor Vinyl Restoration

### 1. Click Detector Model

```python
# Training script voor click detection
import torch
import torch.nn as nn

class ClickDetector(nn.Module):
    def __init__(self, frame_size=512):
        super().__init__()
        self.conv1 = nn.Conv1d(1, 32, kernel_size=15, padding=7)
        self.conv2 = nn.Conv1d(32, 64, kernel_size=11, padding=5)
        self.conv3 = nn.Conv1d(64, 1, kernel_size=7, padding=3)
        
    def forward(self, x):
        # x shape: [batch, 1, frame_size]
        x = torch.relu(self.conv1(x))
        x = torch.relu(self.conv2(x))
        x = torch.sigmoid(self.conv3(x))  # Click probability mask
        return x

# Export naar ONNX voor NPU
model = ClickDetector()
dummy_input = torch.randn(1, 1, 512)
torch.onnx.export(
    model, 
    dummy_input,
    "click_detector.onnx",
    input_names=['audio_input'],
    output_names=['audio_output'],
    dynamic_axes={'audio_input': {2: 'frame_size'}}
)
```

### 2. Click Interpolator (Repair)

```python
class ClickInterpolator(nn.Module):
    """U-Net style architecture voor click repair"""
    def __init__(self):
        super().__init__()
        # Encoder
        self.enc1 = nn.Conv1d(1, 32, 15, padding=7)
        self.enc2 = nn.Conv1d(32, 64, 11, padding=5)
        
        # Decoder
        self.dec1 = nn.Conv1d(64, 32, 11, padding=5)
        self.dec2 = nn.Conv1d(32, 1, 15, padding=7)
        
    def forward(self, x):
        # Skip connections voor betere reconstructie
        e1 = torch.relu(self.enc1(x))
        e2 = torch.relu(self.enc2(e1))
        
        d1 = torch.relu(self.dec1(e2) + e1)
        out = self.dec2(d1)
        return out
```

### 3. Vinyl Denoiser (RNNoise-style)

```python
class VinylDenoiser(nn.Module):
    """Stateful GRU-based denoiser voor vinyl rumble/hiss"""
    def __init__(self, frame_size=512):
        super().__init__()
        self.gru = nn.GRU(frame_size, 256, num_layers=2, batch_first=True)
        self.fc = nn.Linear(256, frame_size)
        
    def forward(self, x):
        # x shape: [batch, 1, frame_size]
        x = x.transpose(1, 2)  # [batch, frame_size, 1]
        out, _ = self.gru(x)
        out = self.fc(out)
        return out.transpose(1, 2)  # Back to [batch, 1, frame_size]
```

### Olive Optimization voor NPU

```python
# optimize_for_npu.py
from olive.workflows import run as olive_run

config = {
    "input_model": {
        "type": "PyTorchModel",
        "model_path": "click_detector.onnx"
    },
    "systems": {
        "local_system": {
            "type": "LocalSystem",
            "accelerators": ["npu"]
        }
    },
    "passes": {
        "onnx_quantization": {
            "type": "OnnxQuantization",
            "data_config": "calibration_data.json"
        },
        "onnx_optimization": {
            "type": "OnnxOptimization",
            "optimization_level": 2
        }
    }
}

olive_run(config)
# Output: Geoptimaliseerde .onnx voor DirectML/NPU
```

---

## Deployment

### CMakeLists.txt aanpassingen

```cmake
# Add ONNX Runtime
find_package(onnxruntime REQUIRED)

target_link_libraries(AudioRestorationVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        onnxruntime::onnxruntime
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Copy ONNX models to plugin bundle
add_custom_command(TARGET AudioRestorationVST POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Models
        $<TARGET_FILE_DIR:AudioRestorationVST>/Models
)
```

### Model Deployment

Models bundlen in VST3:

```
AudioRestorationVST.vst3/
└── Contents/
    ├── x86_64-win/
    │   ├── AudioRestorationVST.vst3
    │   └── onnxruntime.dll
    └── Models/
        ├── click_detector.onnx
        ├── click_interpolator.onnx
        └── vinyl_denoiser.onnx
```

---

## Voordelen van NPU voor Vinyl Restoration

1. **Latency**: NPU heeft typisch <10ms inference time voor deze kleine modellen
2. **GPU blijft vrij**: REAPER/DAW kan GPU gebruiken voor visualisaties/andere plugins
3. **Energie-efficiënt**: NPU gebruikt ~5W vs ~50W voor GPU
4. **Parallellisme**: Meerdere NPU instances mogelijk (L/R channel parallel)
5. **Vinyl-specifiek**: Clicks/pops zijn ideaal voor ML detection (hoge SNR events)

### Performance verwachtingen

- Click detection: ~2ms per 512 sample frame
- Click interpolation: ~5ms per frame
- Noise reduction: ~8ms per frame
- **Total latency: <20ms @ 44.1kHz** (acceptabel voor real-time)

---

## Next Steps

1. Train basic click detector op gelabelde vinyl recordings
2. Test NPU inference speed met dummy ONNX model
3. Integreer NPUManager in bestaande VST codebase
4. A/B test: CPU vs NPU processing quality en latency
5. Optimize model size (quantization) voor NPU

---

## Dataset Requirements

### Voor Click Detection Training

- **Clean vinyl recordings**: zonder clicks (voor negative samples)
- **Vinyl met clicks**: natuurlijke clicks/pops (voor positive samples)
- **Synthetic clicks**: gegenereerde clicks op clean audio
- Minimum: ~100 uur gelabelde audio
- Sample rate: 44.1kHz of hoger

### Labeling Tools

- Audacity: manual click marking
- Python script voor synthetic click generation
- Semi-automated detection + manual verification

---

## Testing Framework

```python
# test_npu_performance.py
import onnxruntime as ort
import numpy as np
import time

def benchmark_npu():
    # Load model
    session = ort.InferenceSession(
        "click_detector.onnx",
        providers=['DmlExecutionProvider']
    )
    
    # Test data
    audio = np.random.randn(1, 1, 512).astype(np.float32)
    
    # Warmup
    for _ in range(10):
        session.run(None, {'audio_input': audio})
    
    # Benchmark
    times = []
    for _ in range(1000):
        start = time.perf_counter()
        session.run(None, {'audio_input': audio})
        times.append(time.perf_counter() - start)
    
    print(f"Mean inference time: {np.mean(times)*1000:.2f}ms")
    print(f"99th percentile: {np.percentile(times, 99)*1000:.2f}ms")

if __name__ == "__main__":
    benchmark_npu()
```

---

## References

- [ONNX Runtime DirectML](https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html)
- [Microsoft Olive Toolkit](https://github.com/microsoft/Olive)
- [JUCE Framework](https://juce.com/)
- [RNNoise Paper](https://jmvalin.ca/demo/rnnoise/)
