# Privacy and safe use

ESP MIDI Bridge processes guitar audio on your computer to generate MIDI. The plug-in has no account, telemetry, advertising, automatic update service, upload function, or server component. Sharing its GitHub URL does not grant visitors access to the developer's PC or private repositories.

- **Audio:** the controller does not itself save raw audio. Dry monitoring is off initially. Your DAW may record audio separately.
- **MIDI files:** REC MIDI, or a DAW's recording flag when Follow DAW record is enabled, saves notes, velocities, bends, tempo and time signature under your Music folder, in `ESP MIDI Bridge/MIDI Takes`. These files stay local; they can reveal what you played if you share them or sync that folder using another service. The cleaned take preserves the original.
- **DAW state:** projects/presets contain plug-in parameter values and the selected MIDI device identifier. They do not contain a developer credential or remote control connection. Avoid sharing a whole DAW project if it contains your recordings, paths or other plug-ins' private data.
- **MIDI routing:** Device MIDI sends to the selected local MIDI port, including hardware or virtual ports. A third-party network MIDI driver can forward this data according to that driver's settings. Prefer a dedicated virtual bus and one route; Host + device with both returning to the same instrument can double notes. Do not feed an IAC/loopMIDI bus back into itself. PANIC/stop sends note-off and reset messages on the used MIDI channels.
- **Windows helper:** optional auto-start uses an already installed loopMIDI executable from its installation directory and checks its developer signature before launching it. No virtual MIDI driver is bundled or downloaded, no startup entry is added, and no firewall or antivirus settings are changed. The shared loopMIDI process can remain running for other applications after Bridge closes.
- **External links:** License / Source opens the public GitHub page in your browser only when clicked. GitHub and your browser have their own privacy rules.

## Downloads and permissions

Use only [this repository's Releases](https://github.com/DTMgengen/esp-midi-bridge-vst3/releases). Check each file against its release's `SHA256SUMS.txt`. A matching checksum detects accidental changes; it is not an independent malware audit or a substitute for a publisher signature.

The Windows setup is unsigned and may trigger SmartScreen. Mac preview bundles have local ad-hoc signatures, **not Developer ID signatures or Apple notarization**; Gatekeeper may block them. Do not disable Gatekeeper, SIP, SmartScreen or antivirus to use this project. A Mac source build is available. Apple explains [how it checks downloaded apps and plug-ins](https://support.apple.com/102445).

Windows setup writes the VST3 to its standard system folder and may install the signed Microsoft VC++ runtime. Updating replaces an existing ESP MIDI Bridge of the same identity: close the DAW first and keep the previous installer and a copy of your project. The Mac ZIP has no installer; copy only the desired bundle to your own user's plug-in folder. Do not overwrite a different or private Bridge build unintentionally.

The app or DAW needs macOS microphone/audio-input permission, including for a USB audio interface. It does not need contacts, screen recording, Accessibility or Full Disk Access. See [Apple's audio-input permission guide](https://support.apple.com/102071). The AU can use the user's Music folder for MIDI capture and CoreMIDI for output; it is not a sandboxed plug-in.

## Limits

This is a preview. Synthetic tests, native builds and Audio Unit validation do not prove that every Mac, DAW, interface or real-guitar performance works. Apple Silicon/Intel builds are validated on macOS 15; the deployment target is macOS 12, whose runtime has not been independently tested. Logic's actual live-guitar route remains unverified. POLY detection and offline cleanup can misclassify notes. An earlier FL shutdown stall remains unisolated. Save your project before trying it, start at low monitoring volume, and use PANIC or stop the receiving instrument if notes stick.

## 公開紹介前の確認（日本語）

プラグインから個人のPCへ接続する仕組み、録音を送信する機能、利用者アカウントはありません。MIDIテイクは利用者自身のMusicフォルダーへ保存されます。共有する場合は音源・録音・プロジェクトも含めた内容を確認してください。

Mac版は未公証、Windowsインストーラーは未署名です。セキュリティ機能を無効にする手順は案内していません。MacのLogic実演奏や全環境での無停止動作を保証する段階ではないため、紹介時は「試用版」であることと[検証範囲](VERIFICATION.md)を添えてください。
