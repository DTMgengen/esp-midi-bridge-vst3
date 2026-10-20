param([string]$Configuration = 'Release', [string]$JuceSource = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$configure = @('-S', $taskRoot, '-B', "$taskRoot\build", '-G', 'Visual Studio 16 2019', '-A', 'x64')
if ($JuceSource) { $configure += "-DESP_JUCE_SOURCE=$JuceSource" }
cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
cmake --build "$taskRoot\build" --config $Configuration --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir "$taskRoot\build" -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
Write-Host "VST3: $taskRoot\build\ESPMidiBridge_artefacts\$Configuration\VST3\ESP MIDI Bridge.vst3"
