param(
    [string]$Compiler = '',
    [string]$VCRedist = '',
    [string]$JuceSource = '',
    [string]$ExpectedModuleHash = '',
    [switch]$TestBuild,
    [string]$TestRoot = ''
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskRoot = [IO.Path]::GetFullPath($taskRoot)
if (-not $Compiler) {
    $candidates = @(
        (Join-Path $taskRoot 'local\tools\InnoSetup\ISCC.exe'),
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    $Compiler = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Compiler -or -not (Test-Path -LiteralPath $Compiler)) { throw 'Provide -Compiler with an Inno Setup 6.3+ ISCC.exe path.' }
if (-not $VCRedist) { $VCRedist = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\VC\Redist\MSVC\v142\vc_redist.x64.exe" }
if (-not (Test-Path -LiteralPath $VCRedist)) { throw 'Provide -VCRedist with the signed Microsoft VC++ x64 redistributable.' }
$signature = Get-AuthenticodeSignature -LiteralPath $VCRedist
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') { throw 'VC++ redistributable must have a valid Microsoft signature.' }
$runtimeVersion = [version](Get-Item -LiteralPath $VCRedist).VersionInfo.FileVersion
if ($runtimeVersion -lt [version]'14.29.30133.0') { throw 'The VC++ redistributable is older than the build tools.' }
if (-not $JuceSource) {
    $cache = Join-Path $taskRoot 'build\CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) {
        $entry = Select-String -LiteralPath $cache -Pattern '^ESP_JUCE_SOURCE:PATH=(.+)$' | Select-Object -First 1
        if ($entry) { $JuceSource = $entry.Matches[0].Groups[1].Value }
    }
    if (-not $JuceSource) { $JuceSource = Join-Path $taskRoot 'build\_deps\juce-src' }
}
$juceLicense = Join-Path $JuceSource 'LICENSE.md'
$sdkLicense = Join-Path $JuceSource 'modules\juce_audio_processors_headless\format_types\VST3_SDK\LICENSE.txt'
foreach ($file in @($juceLicense, $sdkLicense)) { if (-not (Test-Path -LiteralPath $file)) { throw "Dependency licence missing: $file" } }
$cmakeText = Get-Content -LiteralPath (Join-Path $taskRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(ESPMidiBridge VERSION (\d+\.\d+\.\d+)') { throw 'Project version not found.' }
$appVersion = $Matches[1]
$bundle = Join-Path $taskRoot 'build\ESPMidiBridge_artefacts\Release\VST3\ESP MIDI Bridge.vst3'
$module = Join-Path $bundle 'Contents\x86_64-win\ESP MIDI Bridge.vst3'
if (-not (Test-Path -LiteralPath $module)) { throw 'Build the Release VST3 first.' }
if ((Get-Item -LiteralPath $module).VersionInfo.FileVersion -ne $appVersion) { throw 'Built module version differs from CMake project version.' }
$moduleHash = (Get-FileHash -LiteralPath $module -Algorithm SHA256).Hash
if ($ExpectedModuleHash -and $moduleHash -ne $ExpectedModuleHash) { throw 'The plugin does not match the expected verified module.' }
$expectedFiles = @('Contents\Resources\moduleinfo.json', 'Contents\x86_64-win\ESP MIDI Bridge.vst3')
$actualFiles = @(Get-ChildItem -LiteralPath $bundle -Recurse -File | ForEach-Object { $_.FullName.Substring($bundle.Length + 1) } | Sort-Object)
if (Compare-Object $expectedFiles $actualFiles) { throw 'Unexpected bundle file set; review the installer payload list.' }

$staging = Join-Path $taskRoot ('local\installer-payloads\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging -Force | Out-Null
Copy-Item -LiteralPath $bundle -Destination (Join-Path $staging 'Plugin') -Recurse
Copy-Item -LiteralPath $VCRedist -Destination (Join-Path $staging 'vc_redist.x64.exe')
Copy-Item -LiteralPath (Join-Path $taskRoot 'THIRD_PARTY_NOTICES.md') -Destination $staging
Copy-Item -LiteralPath (Join-Path $taskRoot 'LICENSE') -Destination $staging
Copy-Item -LiteralPath (Join-Path $taskRoot 'licenses\THIRD-PARTY-LICENSES.txt') -Destination $staging
foreach ($guide in @('MIDI_RECORDING.ja.md', 'VERIFICATION.md', 'SOURCE.md')) {
    Copy-Item -LiteralPath (Join-Path $taskRoot ('docs\' + $guide)) -Destination $staging
}
$dawGuide = Get-Content -LiteralPath (Join-Path $taskRoot 'docs\DAW_SETUP.ja.md') -Raw -Encoding UTF8
$dawGuide = $dawGuide.Replace('[PROJECT_STATUS.md](../PROJECT_STATUS.md)', 'Quick Start.html')
[IO.File]::WriteAllText((Join-Path $staging 'DAW_SETUP.ja.md'), $dawGuide, [Text.UTF8Encoding]::new($true))
Copy-Item -LiteralPath (Join-Path $taskRoot 'installer\Quick Start.html') -Destination $staging
Copy-Item -LiteralPath $juceLicense -Destination (Join-Path $staging 'JUCE-LICENSE.md')
Copy-Item -LiteralPath $sdkLicense -Destination (Join-Path $staging 'VST3-LICENSE.txt')
$beforeText = Get-Content -LiteralPath (Join-Path $taskRoot 'installer\BeforeInstall.txt') -Raw -Encoding UTF8
[IO.File]::WriteAllText((Join-Path $staging 'BeforeInstall.txt'), $beforeText, [Text.UTF8Encoding]::new($true))

$outputDir = Join-Path $taskRoot 'dist\installer'
$argsForCompiler = @('/Qp', "/DAppVersion=$appVersion", "/DPayloadDir=$staging", "/DOutputDir=$outputDir")
if ($TestBuild) {
    if (-not $TestRoot) { throw 'Test builds require -TestRoot under this project local directory.' }
    $TestRoot = [IO.Path]::GetFullPath($TestRoot)
    $allowedRoot = [IO.Path]::GetFullPath((Join-Path $taskRoot 'local')) + [IO.Path]::DirectorySeparatorChar
    if (-not $TestRoot.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'TestRoot must stay inside this project local directory.' }
    $outputDir = Join-Path $taskRoot 'local\installer-tests\packages'
    $argsForCompiler = @('/Qp', "/DAppVersion=$appVersion", "/DPayloadDir=$staging", "/DOutputDir=$outputDir", '/DInstallerTest=1', "/DTestRoot=$TestRoot")
    $baseName = "ESP-MIDI-Bridge-Setup-$appVersion-TEST-ONLY"
} else { $baseName = "ESP-MIDI-Bridge-Setup-$appVersion-win-x64" }
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
& $Compiler @argsForCompiler (Join-Path $taskRoot 'installer\ESP-MIDI-Bridge.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }
$installer = Join-Path $outputDir ($baseName + '.exe')
$setupHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
$manifest = [ordered]@{
    product = 'ESP MIDI Bridge'; version = $appVersion; architecture = 'Windows x64'
    testBuild = [bool]$TestBuild; setupFile = $baseName + '.exe'; setupSHA256 = $setupHash
    pluginSHA256 = $moduleHash; vcRuntimeVersion = $runtimeVersion.ToString()
    vcRuntimeSHA256 = (Get-FileHash -LiteralPath $VCRedist -Algorithm SHA256).Hash
    payload = @($expectedFiles)
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputDir ($baseName + '.manifest.json')) -Encoding UTF8
($setupHash + '  ' + $baseName + '.exe') | Set-Content -LiteralPath (Join-Path $outputDir ($baseName + '.sha256')) -Encoding ASCII
Write-Output "Installer: $installer"
Write-Output "Size: $((Get-Item -LiteralPath $installer).Length) bytes; SHA256: $setupHash"
