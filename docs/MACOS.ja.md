# Macで使う

ESP MIDI Bridgeはギター入力をMIDIへ変換します。Macでは **VST3、Audio Unit（AU）、Standalone** を同じソースから作れます。Apple SiliconとIntel向けにネイティブビルドを行います。ビルドの最低対象OSはmacOS 12です。各DAWが要求するmacOSのバージョンも確認してください。

## ダウンロードして使う（試用版）

[v0.3.2 Releases](https://github.com/DTMgengen/esp-midi-bridge-vst3/releases/tag/v0.3.2)で **macos-arm64.zip（Apple Silicon）** または **macos-x86_64.zip（Intel）** を選びます。「このMacについて」で種類を確認してください。ZIP内の `READ-ME-FIRST.txt` と、このガイドを読んでから、使うバンドルだけを下の配置先へコピーします。管理者権限を要求するインストーラーは含まれません。

Mac配布版はアドホック署名のみで、Developer ID署名・Apple公証はありません。ダウンロードしたバンドルはGatekeeperに止められる場合があります。[Appleの説明](https://support.apple.com/102445)を確認し、セキュリティ機能を無効にする操作はしないでください。以下のソースから自分のMacでビルドする方法もあります。

## ソースからビルドする

1. MacにXcode、または `xcode-select --install` でCommand Line Toolsを導入し、CMake 3.22以上を用意します。
2. ターミナルで次を実行します。JUCE 8.0.12は指定コミットから取得します。

   ```sh
   git clone https://github.com/DTMgengen/esp-midi-bridge-vst3.git
   cd esp-midi-bridge-vst3
   bash scripts/build-macos.sh
   ```

3. DAWを終了し、`build-macos/ESPMidiBridge_artefacts/Release/` にある、使う形式の**バンドル全体**をコピーします。フォルダーがなければ作成します。

   | 形式 | ビルド結果 | コピー先 |
   | --- | --- | --- |
   | VST3 | `VST3/ESP MIDI Bridge.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
   | AU | `AU/ESP MIDI Bridge.component` | `~/Library/Audio/Plug-Ins/Components/` |
   | Standalone | `Standalone/ESP MIDI Bridge.app` | `~/Applications/` など |

4. DAWを起動し、プラグインを再スキャンします。Logic ProではAudio FXの **Audio Units → Gengen Audio → ESP MIDI Bridge** を使います。表示されない場合は[AppleのAU再スキャン手順](https://support.apple.com/122179)を参照してください。Standaloneはアプリを直接開きます。

スクリプトはMac上でビルドしたバンドルにローカル用のアドホック署名を付けます。Developer ID署名やAppleの公証は行いません。[配置先の参考: JUCE公式](https://juce.com/tutorials/tutorial_app_plugin_packaging/)。

ダウンロード版でも配置先は同じです。VST3とAUはZIP直下にある同名のバンドル全体をコピーします。Standaloneはプラグインではなく、単独で起動するアプリです。入力を解析するにはDAW側の入力モニタリングが必要です。Macのノートパソコンで画面が小さい場合、Bridgeの画面サイズを変更できます。

## ギターから音源へつなぐ

- **VST3対応DAW**：ギターを受けるオーディオトラックにBridgeを挿し、**Host MIDI** を選択します。DAWがエフェクトのMIDI出力を公開する場合は、音源トラックでBridgeをMIDI入力に選びます。出力を選べないDAWでは下のIACを使います。
- **Logic Pro**：AUをギターのオーディオトラックの **Audio FX** に挿し、正しい入力と入力モニタリングを選びます。Bridgeの **Device MIDI** からIACへ送り、別のソフト音源トラックで受けます。LogicのMIDI FX枠への挿入は不要です。[Logicが対応する形式: Apple公式](https://support.apple.com/en-gb/102082)。
- **Standalone**：Optionsでオーディオインターフェースと入力チャンネルを選び、JUCEの入力ミュートを解除します。**Device MIDI** からIACへ送ります。音が入らない場合は、macOSのプライバシー設定でStandaloneまたは使用中のDAWに[オーディオ入力へのアクセス](https://support.apple.com/102071)を許可してください。

### IACでMIDIを返す

1. macOSの **Audio MIDI設定 → MIDIスタジオ → IAC Driver** で「装置はオンライン」を有効にし、専用のバスを作ります。[AppleのIAC手順](https://support.apple.com/guide/audio-midi-setup/ams1013/mac)。
2. Bridgeで **Device MIDI** を選び、出力一覧からそのバスを指定します。表示されない場合は **Refresh** を押します。loopMIDIのインストールは不要です。
3. DAWでそのIACバスをMIDI入力として有効にします。Logicではソフト音源トラックのインスペクタで、**MIDI Input** からそのIACポートを選び、トラックを選択または録音待機にします。[Logicのトラック入力設定](https://support.apple.com/guide/logicpro/lgcpfe5986f5/mac)。
4. 演奏またはMIDI録音を開始します。同じIACバスにDAWからMIDIを返す設定は外し、二重発音やフィードバックを避けます。Bridgeで別途 `.mid` を保存する場合は **REC MIDI → STOP MIDI** を使います。

## 検証の範囲

[macOSのGitHub Actions](https://github.com/DTMgengen/esp-midi-bridge-vst3/actions/workflows/macos.yml)はApple Silicon・Intelそれぞれで3形式をビルドし、VST3のMIDI出力／録音、仮想CoreMIDIポートへのノートオン・オフ、AUの`auval`を検証します。各コミットの結果はActionsを参照してください。Logic等の実際のDAWで、ギターを入力して音源を鳴らす試奏は未実施です。
