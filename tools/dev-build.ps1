# Local dev build for StageView on Windows (Qt 6 mingw_64 kit).
#
# Pins the exact toolchain this machine uses so a rebuild is one command:
#   pwsh tools/dev-build.ps1            # configure + build
#   pwsh tools/dev-build.ps1 -Run       # ...then launch the app
#   pwsh tools/dev-build.ps1 -Clean     # wipe the build dir first
#
# CI doesn't use this script (it installs Qt fresh via aqt) - this is purely the
# convenience wrapper for the developer's box, where Qt already lives in C:\Qt.

param(
    [switch]$Run,
    [switch]$Clean,
    [string]$Config = "Debug"
)
$ErrorActionPreference = "Stop"

$QtPrefix = "C:\Qt\6.10.2\mingw_64"
$MinGWBin = "C:\Qt\Tools\mingw1310_64\bin"
$CMake    = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$Ninja    = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

$Root  = Split-Path $PSScriptRoot -Parent
$Build = Join-Path $Root "build"

if ($Clean -and (Test-Path $Build)) { Remove-Item -Recurse -Force $Build }

# MinGW runtime DLLs (libstdc++, libgcc) and Qt DLLs must be resolvable both at
# link time and when the app runs, so prepend both bin dirs to PATH for this shell.
$env:PATH = "$MinGWBin;$QtPrefix\bin;$env:PATH"

# NOTE: every -D value is double-quoted so PowerShell interpolates the variable
# before handing the token to cmake. An unquoted `-DCMAKE_BUILD_TYPE=$Config`
# gets passed literally (cmake then caches the string "$Config") - a subtle but
# real PowerShell native-arg gotcha.
& $CMake -S $Root -B $Build -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_PREFIX_PATH=$QtPrefix" `
    "-DCMAKE_CXX_COMPILER=$MinGWBin\g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$Ninja"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $CMake --build $Build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$Exe = Join-Path $Build "bin\stageview.exe"
Write-Host "`nBuilt: $Exe" -ForegroundColor Green

if ($Run) { & $Exe }
