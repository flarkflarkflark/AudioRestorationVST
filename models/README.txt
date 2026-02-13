Place the RNNoise ONNX model here.

Required filename:
  rnnoise_48k.onnx

Optional Olive-optimized variants:
  rnnoise_48k_olive_dml.onnx
  rnnoise_48k_olive_coreml.onnx
  rnnoise_48k_olive_cuda.onnx
  rnnoise_48k_olive_rocm.onnx
  rnnoise_48k_olive_qnn.onnx
  rnnoise_48k_olive_cpu.onnx

If you don't have a model yet, run:
  powershell -ExecutionPolicy Bypass -File tools\download_rnnoise_onnx.ps1 -Url <model-url>

Optional environment overrides:
  VRS_MODEL_DIR: extra model search folder
  VRS_QNN_BACKEND_PATH: full path to QNN backend DLL
