param(
    [string]$Triplet = "x64-windows"
)

function Find-Vcpkg {
    if ($env:VCPKG_ROOT) {
        $candidate = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $default = "C:\vcpkg\vcpkg.exe"
    if (Test-Path $default) {
        return $default
    }

    $paths = ($env:PATH -split ';') | Where-Object { $_ -and (Test-Path $_) }
    foreach ($path in $paths) {
        $candidate = Join-Path $path "vcpkg.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

$vcpkg = Find-Vcpkg
if (-not $vcpkg) {
    Write-Error "vcpkg.exe not found. Set VCPKG_ROOT or add vcpkg to PATH."
    exit 1
}

Write-Host "Using vcpkg at $vcpkg"
& $vcpkg install mp3lame mpg123 --triplet $Triplet
