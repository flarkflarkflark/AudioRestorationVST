param(
    [Parameter(Mandatory = $true)]
    [string]$Url,
    [string]$OutFile = "models\\rnnoise_48k.onnx"
)

$outDir = Split-Path -Parent $OutFile
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

Write-Host "Downloading RNNoise ONNX model..."
Invoke-WebRequest -Uri $Url -OutFile $OutFile

Write-Host "Saved model to $OutFile"
