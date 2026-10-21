# ESP MIDI Bridge

ギターの音声からピッチを検出し、既存のVSTi／ソフト音源をMIDIで演奏する専用コントローラー。シンセサイザー音源は内蔵しません。ESP String Labとは別のプラグインです。

- **MONO / last note** と **POLY / experimental**。POLYは最大6音、C2〜C6の半音単位。
- MONOには **LOOSE / original** と **STRICT / independent** の両方。STRICTは入力波形から独立に推定し、LOOSEの結果をオクターブ補正する方式ではありません。POLYでは専用の和音検出器を使い、MONOの品質選択を保持します。
- MONOのチョーキング／ビブラートを14bitピッチベンドへ変換。±2／12／24半音。ピッチベンドOFFでは半音のノートを出力。
- MIDIチャンネル、トランスポーズ、入力ゲイン、ゲート、MONO confidence、ノート保持時間、強弱連動／固定ベロシティ。
- ホストへのMIDI出力と、任意のMIDIデバイスへの直接出力。通常は **Host MIDI** を使用。直接出力は仮想MIDIポート経由の接続にも使用できます。
- Note Off、モード／品質／チャンネル変更、バイパス、PANICを処理。PANIC後はギターがゲート以下になるまでミュート。
- Dry guitar monitorは初期OFF。新しいシンセ音は生成しません。
- **MIDIテイク録音（v0.2）**：出力したノート・ベロシティ・ベンドを標準MIDIファイルへ保存。REC MIDIで手動録音でき、録音状態を通知するDAWでは開始・停止にも追従します。FL Studioでのファイル保存には手動録音を使ってください。

Windows x64 **VST3＋Standalone**。macOS向けにはVST3／AU／Standaloneのビルド設定を用意していますが、Macビルド／Logicでの検証は未実施です。[検証範囲](docs/VERIFICATION.md)と[DAW別の接続手順](docs/DAW_SETUP.ja.md)を参照してください。

**[Windowsインストーラーのダウンロード](https://github.com/DTMgengen/esp-midi-bridge-vst3/releases/latest)**：`ESP-MIDI-Bridge-Setup-0.2.0-win-x64.exe`。DAWを閉じて実行すると、VST3と接続ガイドを導入します。必要なVC++ x64ランタイムも同梱し、不足時だけ導入します。Windowsの「インストールされているアプリ」から削除できます。インストーラーにStandaloneや仮想MIDIドライバーは含まれません。[作成方法と検証範囲](docs/INSTALLER.ja.md)。セットアップは未署名です。

## MIDIと音声を録音する

**FL StudioのPiano rollへ直接録音する場合は、Device MIDIと仮想MIDI入力を使います。** WindowsのFL Studio 2026で、実ギター → Bridge → loopMIDI → Nexusの発音、Piano rollのノート／ベロシティ、同時のギター音声録音を確認済みです。

1. [loopMIDI（開発元）](https://www.tobias-erichsen.de/software/loopmidi.html)を別途起動し、専用ポート（例：**ESP Bridge Record**）を作る。
2. Bridgeを **Device MIDI** にし、出力先にそのポートを選ぶ。FLのMIDI settingsでは同名の **InputをEnable**、Controller typeを **(generic controller)** にする。
3. Channel Rackで録音先音源を選び、空のPatternを用意。FLのRecordボタン右クリックで **Notes** をONにし、Record＋Playで録音する。
4. 音声も残すなら対象Mixerトラックのディスク録音と **Audio** を有効化。ギター入力トラックを録れば生ギター、音源出力トラックを録れば音源の音声が残る。

loopMIDIは録音中も起動しておきます。FL側の同じポートへの出力／MIDI THRUは設定せず、以前のHost MIDI接続との二重発音を避けてください。**FLのMIDI Outプラグインは不要です。** [詳しい接続・録音手順とベンドの制約](docs/MIDI_RECORDING.ja.md)。

## MIDIファイルも残す

1. **Follow DAW recordをON**にして、DAW側で音源の音声を録音します。Bridgeがホストの録音状態を受信するとMIDIテイクも開始します。
2. DAWの録音を止めると、`ミュージック/ESP MIDI Bridge/MIDI Takes/` へ `.mid` を自動保存します。**MIDI Takes**ボタンでフォルダーを開けます。
3. 保存した `.mid` をFL StudioのPiano roll等へ読み込んで編集します。**Save MIDI...**で最後のテイクを別名保存できます。
4. DAWから録音状態が来ない場合、またはStandaloneでは、**REC MIDI → 演奏 → STOP MIDI**を使います。今回のFL Studio 26.1.7での試奏では自動開始しなかったため、FLで`.mid`も残す場合はこの手動操作を使ってください。

これはDAW内のPatternへ直接ノートを挿入する機能ではありません。**Piano rollへリアルタイム録音する場合**は、BridgeのDevice MIDI → 仮想MIDIループバック → FLの有効なMIDI入力へ接続し、FLのNotes録音を使います。音声はDAWで別に録音します。[具体的な設定とベンドの制約](docs/MIDI_RECORDING.ja.md)。

ノート・ベロシティ・14bitベンド・ベンド範囲・テンポ・拍子を保存します。録音中の解析アルゴリズム／ホストへのMIDI出力は同じです。単一の連続テイクとして記録し、DAWのループごとには分割しません。上限262,144イベントで安全停止・保存します（自動で無制限に継続しません）。保存失敗時はテイクを保持してSave MIDIで回復できます。次の録音で前のメモリ上のテイクを置き換えますが、自動保存済みファイルは残ります。

## FL Studioで演奏する：Host MIDI接続

以下は音源を鳴らすためのホスト内接続です。Piano rollへのリアルタイム録音は上記のDevice MIDI接続を使います。

1. Plugin Managerで **ESP MIDI Bridge** を再スキャンし、ギターを入力するMixerのエフェクトスロットへ挿入。
2. Bridge画面の **MIDI DESTINATION = Host MIDI**。Wrapperの歯車→Settings→MIDI **Output port = 20**。
3. Channel Rackに演奏したい**VST版音源**を読み込み、そのWrapperのMIDI **Input port = 20**。MIDIチャンネルは最初は1。
4. BridgeをMONOにし、MONO PITCHでLOOSE／STRICTを選んで演奏。和音はPOLY。音量・音色は受信側の音源で設定。
5. チョーキングを使うときは受信音源のPitch Bend RangeをBridgeと同じに設定。初期値は±2半音。RPN送信は対応音源で任意に有効化。

**FLのMIDI Outプラグインを間に挟む必要はありません。** Bridge自体がMIDI送信元です。20は例で、空いている同じ番号に合わせます。これはFLのWrapper内のポート番号で、BridgeのMIDI CHANNEL（1〜16）とは別です。Wrapperのポートについては[Image-Line公式マニュアル](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/wrapper.htm)を参照。

## ビルド

Windows: CMake 3.22以上、Visual Studio 2019 C++ Build Tools x64、Windows SDK。

```powershell
./scripts/build.ps1
./scripts/install.ps1
```

JUCE 8.0.12をコミット固定で取得します。既存のJUCEソースを使う場合は `./scripts/build.ps1 -JuceSource 'C:/path/to/JUCE'`。SDKキャッシュやバイナリはGitに含めません。既存Bridge更新時はDAWでBridgeを閉じてから `./scripts/install.ps1 -Update`（バックアップ付き）。

macOS用ソースビルド例（未検証）:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
```

LogicはAU形式を使用します。今回のAUはオーディオエフェクトで、LogicのMIDI FX枠に入る設計ではありません。Logicの汎用接続案は、Mac版AUまたはStandalone→IAC仮想ポート→Logicのソフト音源。AUのホスト内MIDI出力がそのままLogicの音源へ接続できるという検証はしていません。

## 検証用

```powershell
ctest --test-dir build -C Release --output-on-failure
python scripts/verify_engine.py ../esp-string-lab
./build/bridge_plugin_tests_artefacts/Release/bridge_plugin_tests.exe --snapshot local/editor.png
./build/bridge_vst3_tests_artefacts/Release/bridge_vst3_tests.exe 'C:/path/ESP MIDI Bridge.vst3' 'C:/path/Dexed.vst3'
```

元エンジンの由来と抽出後のSHA-256は [tracking-provenance.json](docs/tracking-provenance.json)。波形解析にはESP String Lab v0.5.0由来の数値処理を使用し、発振器・フィルター・VCA等を取り除いています。MIDIノート生成とノート保持は新規実装です。ハッシュ検証はこのリポジトリのみで実行できます。元プロジェクトのチェックアウトは不要です。

ホストへ報告する遅延は0サンプルですが、**ピッチ解析の待ち時間が0という意味ではありません**。入力バッファ、音程推定、ホスト内MIDI接続、受信音源のアタックが実際の反応に加算されます。POLYの解析窓はMONOより長く、独立した弦ごとのベンド／MPEはありません。

## License / source

Copyright (c) 2026 DTMgengen. **AGPL-3.0-only**。自由に閲覧・ビルド・変更・再配布できます。再配布時は[LICENSE](LICENSE)の条件に従い、対応するソースとライセンスを提供してください。JUCEも本配布ではAGPLv3の選択肢を使用します。[依存ソフトウェアの通知](THIRD_PARTY_NOTICES.md)。他社音源、仮想MIDIドライバーは配布しません。KORG、Behringerとの提携・公式製品ではありません。

対応するソースは[各リリース](https://github.com/DTMgengen/esp-midi-bridge-vst3/releases)の同じバージョンのタグ／Source ZIPと、同梱のJUCE依存ソースアーカイブから取得できます。[ソースの構成と再ビルド](docs/SOURCE.md)。

English: Audio-input-to-MIDI controller, with MONO LOOSE/STRICT, experimental six-note POLY, MIDI note/velocity/pitch-bend output and automatic/manual Standard MIDI File capture. No synthesizer is included. Download the Windows x64 VST3 installer from Releases. Match MIDI ports and pitch-bend ranges with your instrument. Direct FL Studio 26.1.7 Piano roll note/velocity recording was verified with a real guitar, Device MIDI, loopMIDI and Nexus, alongside dry guitar audio recording. Keep loopMIDI running. Use manual REC MIDI / STOP MIDI for a separate `.mid` backup in FL; automatic capture did not start in this live test. Recorded bend playback and macOS/other DAWs remain unverified. Licensed under AGPLv3.
