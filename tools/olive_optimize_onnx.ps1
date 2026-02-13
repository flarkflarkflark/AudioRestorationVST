param(
    [string]$InputModel = "models\\rnnoise_48k.onnx",
    [string]$OutputModel = "models\\rnnoise_48k_olive_dml.onnx",
    [ValidateSet("dml","coreml","cuda","rocm","cpu")]
    [string]$Accelerator = "dml"
)

$localOlive = Join-Path $PSScriptRoot "..\\.venv-olive\\Scripts\\olive.exe"
$oliveCmd = "olive"
if (Test-Path $localOlive) {
    $oliveCmd = $localOlive
} elseif (-not (Get-Command $oliveCmd -ErrorAction SilentlyContinue)) {
    Write-Error "Olive CLI not found. Install with: pip install olive-ai or run from .\\.venv-olive"
    exit 1
}

$provider = switch ($Accelerator) {
    "dml" { "DmlExecutionProvider" }
    "cuda" { "CUDAExecutionProvider" }
    "rocm" { "ROCMExecutionProvider" }
    "cpu" { "CPUExecutionProvider" }
    "coreml" {
        Write-Warning "CoreML provider not available in Olive CLI; using CPU provider."
        "CPUExecutionProvider"
    }
}

$outputDir = Join-Path $PSScriptRoot "..\\build\\olive-output"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Write-Host "Running Olive optimization ($Accelerator) with onnxpeepholeoptimizer..."
& $oliveCmd run-pass --pass-name onnxpeepholeoptimizer -m $InputModel -o $outputDir --provider $provider

$optimized = Get-ChildItem -Path $outputDir -Filter *.onnx | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $optimized) {
    Write-Error "Olive did not produce an optimized model in $outputDir"
    exit 1
}

Copy-Item $optimized.FullName $OutputModel -Force
Write-Host "Saved optimized model to $OutputModel"
