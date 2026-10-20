#ifndef AppVersion
  #error AppVersion must be supplied by scripts/build-installer.ps1
#endif
#ifndef PayloadDir
  #error PayloadDir must be supplied by scripts/build-installer.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be supplied by scripts/build-installer.ps1
#endif

[Setup]
#ifdef InstallerTest
AppId={{59181A1D-C7BF-4B65-8EDC-2A9FDBD19C66}
AppName=ESP MIDI Bridge Installer Test
DefaultDirName={#TestRoot}\Application
DefaultGroupName=ESP MIDI Bridge Installer Test
PrivilegesRequired=lowest
OutputBaseFilename=ESP-MIDI-Bridge-Setup-{#AppVersion}-TEST-ONLY
SetupMutex=GengenAudio.ESPMidiBridge.InstallerTest
#else
AppId={{FE2A219A-495E-4CE3-9B98-0C75315E57EE}
AppName=ESP MIDI Bridge
DefaultDirName={autopf}\Gengen Audio\ESP MIDI Bridge
DefaultGroupName=Gengen Audio\ESP MIDI Bridge
PrivilegesRequired=admin
OutputBaseFilename=ESP-MIDI-Bridge-Setup-{#AppVersion}-win-x64
SetupMutex=GengenAudio.ESPMidiBridge.Setup
#endif
AppVersion={#AppVersion}
AppVerName=ESP MIDI Bridge {#AppVersion}
AppPublisher=Gengen Audio
VersionInfoVersion={#AppVersion}.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#OutputDir}
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
DisableDirPage=yes
DisableProgramGroupPage=yes
CloseApplications=no
RestartApplications=no
SetupLogging=yes
#ifdef InstallerTest
UninstallDisplayName=ESP MIDI Bridge Installer Test
#else
UninstallDisplayName=ESP MIDI Bridge
#endif
UninstallDisplayIcon={uninstallexe}
InfoBeforeFile={#PayloadDir}\BeforeInstall.txt

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
japanese.CloseDaw=ESP MIDI Bridgeを使用しているDAWを閉じて、もう一度実行してください。ファイルが書き込み可能かどうかも確認してください。
english.CloseDaw=Close all DAWs using ESP MIDI Bridge and try again. Also check that the plugin files are writable.
japanese.RuntimeFailed=Microsoft Visual C++ランタイムを準備できませんでした。終了コード: %1
english.RuntimeFailed=Could not prepare the Microsoft Visual C++ runtime. Exit code: %1
japanese.RuntimeRestart=Microsoft Visual C++ランタイムの準備に再起動が必要です。Windowsを再起動してから、このセットアップを再実行してください。
english.RuntimeRestart=The Microsoft Visual C++ runtime needs a restart. Restart Windows, then run this setup again.
japanese.OpenGuide=FL Studioなどへの接続ガイドを開く
english.OpenGuide=Open the DAW connection guide
japanese.PluginLocation=VST3のインストール先:
english.PluginLocation=VST3 installation folder:
japanese.AfterInstall=完了後、DAWのプラグイン一覧を再スキャンしてください。FL StudioではBridge側のMIDI Output portと音源側のMIDI Input portを同じ番号にします。
english.AfterInstall=After installation, rescan plugins in your DAW. In FL Studio, match the Bridge MIDI Output port to your instrument's MIDI Input port.

[Files]
; Extract the signed prerequisite only when it is missing. It is not uninstalled
; with the plugin, since other applications may share the Microsoft runtime.
Source: "{#PayloadDir}\vc_redist.x64.exe"; Flags: dontcopy
Source: "{#PayloadDir}\Plugin\Contents\x86_64-win\ESP MIDI Bridge.vst3"; DestDir: "{code:GetPluginDir}\Contents\x86_64-win"; Flags: ignoreversion
Source: "{#PayloadDir}\Plugin\Contents\Resources\moduleinfo.json"; DestDir: "{code:GetPluginDir}\Contents\Resources"; Flags: ignoreversion
Source: "{#PayloadDir}\Quick Start.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\DAW_SETUP.ja.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\MIDI_RECORDING.ja.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\VERIFICATION.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\SOURCE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\JUCE-LICENSE.md"; DestDir: "{app}\Licenses"; Flags: ignoreversion
Source: "{#PayloadDir}\VST3-LICENSE.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD-PARTY-LICENSES.txt"; DestDir: "{app}\Licenses"; Flags: ignoreversion

#ifndef InstallerTest
[Icons]
Name: "{group}\ESP MIDI Bridge - 接続ガイド"; Filename: "{app}\Quick Start.html"
Name: "{group}\ESP MIDI Bridge - アンインストール"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Quick Start.html"; Description: "{cm:OpenGuide}"; Flags: shellexec postinstall nowait skipifsilent unchecked
#endif

[Code]
function CreateFileW(FileName: String; DesiredAccess, ShareMode,
  SecurityAttributes, CreationDisposition, FlagsAndAttributes: LongWord;
  TemplateFile: THandle): THandle;
external 'CreateFileW@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
external 'CloseHandle@kernel32.dll stdcall';

function GetPluginDir(Param: String): String;
begin
#ifdef InstallerTest
  Result := '{#TestRoot}\VST3\ESP MIDI Bridge.vst3';
#else
  Result := ExpandConstant('{commoncf64}\VST3\ESP MIDI Bridge.vst3');
#endif
end;

function CanWriteFile(const FileName: String): Boolean;
var Handle: THandle;
begin
  Result := True;
  if not FileExists(FileName) then exit;
  { Read/write, exclusive sharing, OPEN_EXISTING: probe without modifying data.
    A mapped VST3 held by a DAW cannot be replaced. Never close the DAW here. }
  Handle := CreateFileW(FileName, $C0000000, 0, 0, 3, $80, 0);
  Result := Handle <> THandle(-1);
  if Result then CloseHandle(Handle);
end;

function CanModifyPlugin: Boolean;
begin
  Result := CanWriteFile(GetPluginDir('') + '\Contents\x86_64-win\ESP MIDI Bridge.vst3') and
    CanWriteFile(GetPluginDir('') + '\Contents\Resources\moduleinfo.json');
end;

function HasCompatibleRuntime: Boolean;
var Installed, Major, Minor, Build: Cardinal;
begin
  Result := RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and
    (Installed = 1);
  if not Result then exit;
  Result := RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Major', Major) and
    RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Minor', Minor) and
    RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Bld', Build);
  if Result then
    Result := (Major > 14) or ((Major = 14) and ((Minor > 29) or
      ((Minor = 29) and (Build >= 30133))));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code: Integer;
begin
  Result := '';
  if not CanModifyPlugin then begin
    Log('Plugin files are locked or not writable; stopping before install.');
    Result := CustomMessage('CloseDaw');
    exit;
  end;
  if HasCompatibleRuntime then begin
    Log('Compatible Visual C++ x64 runtime already installed; prerequisite skipped.');
    exit;
  end;
#ifdef InstallerTest
  { The test build must never change global system prerequisites. }
  Result := 'Installer test requires an existing compatible VC++ runtime.';
#else
  Log('Installing bundled Microsoft Visual C++ x64 runtime.');
  ExtractTemporaryFile('vc_redist.x64.exe');
  if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'),
    '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, Code) then begin
    Result := FmtMessage(CustomMessage('RuntimeFailed'), [IntToStr(Code)]);
    exit;
  end;
  if (Code = 3010) or (Code = 1641) then begin
    NeedsRestart := True;
    Result := CustomMessage('RuntimeRestart');
    exit;
  end;
  if ((Code <> 0) and (Code <> 1638)) or not HasCompatibleRuntime then
    Result := FmtMessage(CustomMessage('RuntimeFailed'), [IntToStr(Code)]);
#endif
end;

function InitializeUninstall: Boolean;
begin
  Result := CanModifyPlugin;
  if not Result then begin
    Log('Plugin files are locked or not writable; stopping before uninstall.');
    if not UninstallSilent then
      MsgBox(CustomMessage('CloseDaw'), mbError, MB_OK);
  end;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
  MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := CustomMessage('PluginLocation') + NewLine + Space + GetPluginDir('') +
    NewLine + NewLine + CustomMessage('AfterInstall');
end;
