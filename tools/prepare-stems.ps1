# Prepare audio stems for the StageView C++ audio engine.
#
# Converts the 8 musical stems (MP3) to mono 44.1 kHz 16-bit WAV in a LOCAL cache
# outside the repo, and writes a stems.json the engine reads. Mono because each
# source is a single point on the stage that the engine pans to stereo.
#
# The audio is copyrighted commercial music - it stays local and is NEVER committed.
# Only this script and the (audio-free) stems.json describe the pipeline.
#
#   pwsh tools/prepare-stems.ps1
#   pwsh tools/prepare-stems.ps1 -Source "D:\some\other\song" -Out "D:\cache"

param(
    [string]$Source = "I:\BACKUP\Traxx\_StagingDB\Sex on Fire__5f16b2",
    [string]$Out = "$env:LOCALAPPDATA\StageView\stems"
)
$ErrorActionPreference = "Stop"

$ffmpeg = (Get-Command ffmpeg -EA SilentlyContinue).Source
if (-not $ffmpeg) { throw "ffmpeg not found on PATH" }
if (-not (Test-Path $Source)) { throw "source stem folder not found: $Source" }
New-Item -ItemType Directory -Force $Out | Out-Null

# file -> display name, initial level (0..1) and pan (-1..1). Order = channel index.
$stems = @(
    @{ file = "Lead Vocal.mp3";                      name = "Vocal";   level = 0.90; pan =  0.00 }
    @{ file = "Drum Kit.mp3";                        name = "Drums";   level = 0.85; pan =  0.00 }
    @{ file = "Bass.mp3";                            name = "Bass";    level = 0.82; pan =  0.00 }
    @{ file = "Rhythm Electric Guitar (left).mp3";   name = "Gtr L";   level = 0.66; pan = -0.55 }
    @{ file = "Rhythm Electric Guitar (right).mp3";  name = "Gtr R";   level = 0.66; pan =  0.55 }
    @{ file = "Arr. Electric Guitar.mp3";            name = "Gtr Arr"; level = 0.60; pan = -0.25 }
    @{ file = "Synthesizer.mp3";                     name = "Synth";   level = 0.62; pan =  0.30 }
    @{ file = "Percussion.mp3";                      name = "Perc";    level = 0.58; pan =  0.40 }
)

$channels = @()
foreach ($s in $stems) {
    $src = Join-Path $Source $s.file
    if (-not (Test-Path $src)) { Write-Warning "missing stem: $($s.file)"; continue }
    $wav = "$($s.name).wav"
    $dst = Join-Path $Out $wav
    Write-Host "converting $($s.file) -> $wav"
    & $ffmpeg -y -hide_banner -loglevel error -i $src -ac 1 -ar 44100 -sample_fmt s16 $dst
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed on $($s.file)" }
    $channels += [ordered]@{ name = $s.name; file = $wav; level = $s.level; pan = $s.pan }
}

$manifest = [ordered]@{
    song       = "Sex on Fire"
    artist     = "Kings of Leon"
    bpm        = 120
    sampleRate = 44100
    channels   = $channels
}
$json = $manifest | ConvertTo-Json -Depth 5
Set-Content -Path (Join-Path $Out "stems.json") -Value $json -Encoding UTF8

Write-Host "`nPrepared $($channels.Count) stems in $Out" -ForegroundColor Green
Write-Host "stems.json written. Point the engine at: $Out" -ForegroundColor Green
