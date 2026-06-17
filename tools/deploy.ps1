# Make StageView a standalone, double-clickable app.
#
# A freshly-built stageview.exe only runs when the Qt and MinGW DLLs are on the
# PATH (as the dev-build wrapper arranges). To launch it by double-clicking from
# Explorer - or to hand the folder to someone else - the runtime has to sit next to
# the exe. windeployqt copies the Qt DLLs and QML plugins; we then add the three
# MinGW runtime DLLs windeployqt doesn't know about.
#
#   pwsh tools/dev-build.ps1      # build first
#   pwsh tools/deploy.ps1         # then make build/bin standalone
#   # build/bin/stageview.exe now double-clicks cleanly

$ErrorActionPreference = "Stop"

$QtBin    = "C:\Qt\6.10.2\mingw_64\bin"
$MinGWBin = "C:\Qt\Tools\mingw1310_64\bin"
$Root     = Split-Path $PSScriptRoot -Parent
$Exe      = Join-Path $Root "build\bin\stageview.exe"
$BinDir   = Split-Path $Exe

if (-not (Test-Path $Exe)) { throw "Build first - not found: $Exe" }

# windeployqt needs the Qt bin (and MinGW for objdump) reachable.
$env:PATH = "$QtBin;$MinGWBin;$env:PATH"

& "$QtBin\windeployqt.exe" --qmldir (Join-Path $Root "app\qml") --no-translations $Exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

# The compiler runtime windeployqt omits for MinGW kits.
Copy-Item "$MinGWBin\libgcc_s_seh-1.dll", "$MinGWBin\libstdc++-6.dll", "$MinGWBin\libwinpthread-1.dll" $BinDir -Force

Write-Host "`nDeployed standalone app: $Exe" -ForegroundColor Green
Write-Host "You can now double-click it from Explorer." -ForegroundColor Green
