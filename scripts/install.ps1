param([string]$Destination = 'C:\Program Files\Common Files\VST3', [switch]$Update)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $taskRoot 'build\ESPMidiBridge_artefacts\Release\VST3\ESP MIDI Bridge.vst3'
if (-not (Test-Path -LiteralPath $source)) { throw 'Build the Release VST3 first.' }
$target = Join-Path $Destination 'ESP MIDI Bridge.vst3'
$backup = $null
if (Test-Path -LiteralPath $target) {
    if (-not $Update) { throw 'An existing Bridge is installed. Close the DAW and use -Update.' }
    $module = Join-Path $target 'Contents\x86_64-win\ESP MIDI Bridge.vst3'
    try {
        $probe = [System.IO.File]::Open($module, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $probe.Dispose()
    } catch { throw "Close applications using ESP MIDI Bridge before updating. $($_.Exception.Message)" }
    $backupRoot = Join-Path $taskRoot ('local\plugin-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
    $backup = Join-Path $backupRoot 'ESP MIDI Bridge.vst3'
    Copy-Item -LiteralPath $target -Destination $backup -Recurse
}
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
try {
    if (Test-Path -LiteralPath $target) {
        Get-ChildItem -LiteralPath $source -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $target -Recurse -Force }
    } else { Copy-Item -LiteralPath $source -Destination $target -Recurse }
} catch {
    if ($backup) { Get-ChildItem -LiteralPath $backup -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $target -Recurse -Force } }
    throw
}
Write-Host "Installed: $target"
Write-Host 'FL Studio: rescan plugins, load Bridge as a mixer effect, match wrapper MIDI Output/Input ports with your VSTi.'
