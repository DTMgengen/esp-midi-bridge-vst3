param([Parameter(Mandatory = $true)][string]$Compiler, [string]$VCRedist = '', [string]$JuceSource = '')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$sandbox = [IO.Path]::GetFullPath((Join-Path $taskRoot 'local\installer-tests\sandbox'))
$allowed = [IO.Path]::GetFullPath((Join-Path $taskRoot 'local')) + [IO.Path]::DirectorySeparatorChar
if (-not $sandbox.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe installer sandbox path.' }
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{59181A1D-C7BF-4B65-8EDC-2A9FDBD19C66}_is1'
if (Test-Path -LiteralPath $uninstallKey) { throw 'An earlier isolated test is still registered; uninstall that test before retrying.' }
$cmakeText = Get-Content -LiteralPath (Join-Path $taskRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(ESPMidiBridge VERSION (\d+\.\d+\.\d+)') { throw 'Version not found.' }
$version = $Matches[1]
$moduleRelative = 'Contents\x86_64-win\ESP MIDI Bridge.vst3'
$resourceRelative = 'Contents\Resources\moduleinfo.json'
$sourceBundle = Join-Path $taskRoot 'build\ESPMidiBridge_artefacts\Release\VST3\ESP MIDI Bridge.vst3'
$installedBundle = Join-Path $sandbox 'VST3\ESP MIDI Bridge.vst3'
$application = Join-Path $sandbox 'Application'
$installedModule = Join-Path $installedBundle $moduleRelative
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $sourceBundle $moduleRelative) -Algorithm SHA256).Hash
$sourceResourceHash = (Get-FileHash -LiteralPath (Join-Path $sourceBundle $resourceRelative) -Algorithm SHA256).Hash
$liveFiles = @(
    'C:\Program Files\Common Files\VST3\ESP MIDI Bridge.vst3\Contents\x86_64-win\ESP MIDI Bridge.vst3',
    'C:\Program Files\Common Files\VST3\ESP String Lab.vst3\Contents\x86_64-win\ESP String Lab.vst3'
)
$liveHashes = @{}
foreach ($file in $liveFiles) { if (Test-Path -LiteralPath $file) { $liveHashes[$file] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash } }
$builderArgs = @{ Compiler = $Compiler; TestBuild = $true; TestRoot = $sandbox; ExpectedModuleHash = $sourceHash }
if ($VCRedist) { $builderArgs.VCRedist = $VCRedist }
if ($JuceSource) { $builderArgs.JuceSource = $JuceSource }
& (Join-Path $PSScriptRoot 'build-installer.ps1') @builderArgs
$setup = Join-Path $taskRoot "local\installer-tests\packages\ESP-MIDI-Bridge-Setup-$version-TEST-ONLY.exe"
$logs = Join-Path $taskRoot 'local\installer-tests\logs'
New-Item -ItemType Directory -Path $logs -Force | Out-Null

function Invoke-TestProcess([string]$Executable, [string]$Phase) {
    $logFile = Join-Path $logs ($Phase + '.log')
    $arguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG="' + $logFile + '"'
    $testProcess = Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $testProcess.WaitForExit(25000)) { $testProcess.Kill(); throw "Installer test timed out: $Phase" }
    $testProcess.Refresh()
    return $testProcess.ExitCode
}
function Assert-Test([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-Payload {
    Assert-Test (Test-Path -LiteralPath $installedModule) 'Installed module missing.'
    Assert-Test ((Get-FileHash -LiteralPath $installedModule -Algorithm SHA256).Hash -eq $sourceHash) 'Installed module differs from verified build.'
    Assert-Test ((Get-FileHash -LiteralPath (Join-Path $installedBundle $resourceRelative) -Algorithm SHA256).Hash -eq $sourceResourceHash) 'Installed moduleinfo differs.'
}

Assert-Test ((Invoke-TestProcess $setup '01-install') -eq 0) 'Isolated install failed.'
Assert-Payload
Assert-Test (Test-Path -LiteralPath $uninstallKey) 'Uninstaller registration missing.'
Assert-Test (Test-Path -LiteralPath (Join-Path $application 'Quick Start.html')) 'Connection guide missing.'
foreach ($guide in @('LICENSE','SOURCE.md','MIDI_RECORDING.ja.md','Licenses\THIRD-PARTY-LICENSES.txt')) {
    Assert-Test (Test-Path -LiteralPath (Join-Path $application $guide)) "Licence/recording guide missing: $guide"
}
Assert-Test ((Get-Content -LiteralPath (Join-Path $logs '01-install.log') -Raw) -match 'prerequisite skipped') 'Compatible runtime was not skipped.'

$otherPlugin = Join-Path $sandbox 'VST3\Other Instrument.vst3'
New-Item -ItemType Directory -Path $otherPlugin -Force | Out-Null
$otherSentinel = Join-Path $otherPlugin 'keep.txt'
$bundleSentinel = Join-Path $installedBundle 'user-note.txt'
Set-Content -LiteralPath $otherSentinel -Value 'Unrelated test instrument: preserve.' -Encoding UTF8
Set-Content -LiteralPath $bundleSentinel -Value 'Unowned note inside the bundle: preserve.' -Encoding UTF8
$otherHash = (Get-FileHash -LiteralPath $otherSentinel).Hash
$sentinelHash = (Get-FileHash -LiteralPath $bundleSentinel).Hash
$uninstaller = Join-Path $application 'unins000.exe'

$hold = [IO.File]::Open($installedModule, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
try {
    Assert-Test ((Invoke-TestProcess $setup '02-locked-update') -eq 7) 'Locked update was not stopped at preparation.'
    Assert-Test ((Invoke-TestProcess $uninstaller '03-locked-uninstall') -ne 0) 'Locked uninstall should fail.'
} finally { $hold.Dispose() }
Assert-Payload
Assert-Test (Test-Path -LiteralPath $uninstallKey) 'Locked uninstall changed registration.'

# Corrupt only this isolated copy; an idempotent reinstall must restore it.
$append = [IO.File]::Open($installedModule, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $append.WriteByte(42) } finally { $append.Dispose() }
Assert-Test ((Invoke-TestProcess $setup '04-reinstall') -eq 0) 'Same-version reinstall failed.'
Assert-Payload
Assert-Test ((Get-FileHash -LiteralPath $otherSentinel).Hash -eq $otherHash) 'Reinstall changed unrelated plugin.'
Assert-Test ((Get-FileHash -LiteralPath $bundleSentinel).Hash -eq $sentinelHash) 'Reinstall changed unowned bundle file.'

$hostTest = Join-Path $taskRoot 'build\bridge_vst3_tests_artefacts\Release\bridge_vst3_tests.exe'
& $hostTest $installedBundle
if ($LASTEXITCODE -ne 0) { throw 'Installed test VST3 failed actual MIDI-output integration.' }

Assert-Test ((Invoke-TestProcess $uninstaller '05-uninstall') -eq 0) 'Isolated uninstall failed.'
Assert-Test (-not (Test-Path -LiteralPath $installedModule)) 'Uninstall left plugin module.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $installedBundle $resourceRelative))) 'Uninstall left module metadata.'
Assert-Test (-not (Test-Path -LiteralPath $uninstallKey)) 'Uninstall left registration.'
Assert-Test (-not (Test-Path -LiteralPath (Join-Path $application 'Quick Start.html'))) 'Uninstall left owned guide.'
Assert-Test ((Get-FileHash -LiteralPath $otherSentinel).Hash -eq $otherHash) 'Uninstall removed unrelated plugin.'
Assert-Test ((Get-FileHash -LiteralPath $bundleSentinel).Hash -eq $sentinelHash) 'Uninstall removed unowned bundle file.'
foreach ($file in $liveHashes.Keys) { Assert-Test ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -eq $liveHashes[$file]) 'Live plugin changed during isolated tests.' }
Write-Output 'PASS isolated install, locked update/uninstall refusal, same-version repair, actual VST3 MIDI output, uninstall, unrelated-file/live-install preservation.'
