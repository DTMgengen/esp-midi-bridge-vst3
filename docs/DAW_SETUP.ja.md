# DAWへの接続

2026-10-21。下記は公式のルーティング仕様を基にした設定手順／接続案です。自動テストでのVST3イベント出力と、実際の各DAWでの試奏は別です。[検証範囲](VERIFICATION.md)を参照。v0.2からのMIDIテイク保存とFLのPiano rollへの入力録音は[MIDI録音ガイド](MIDI_RECORDING.ja.md)を参照。

## FL Studio / Windows VST3

1. オーディオインターフェースのギター入力をMixerの入力に設定。BridgeをそのMixerへ挿入。BridgeのAUDIO INPUTをLeft／Rightなど実際の入力に合わせる。
2. **MIDI DESTINATION = Host MIDI**。BridgeのWrapper SettingsのMIDI **Output port**を、例えば **20** にする。
3. Channel Rackへ目的のVSTiを挿入。受信側Wrapper SettingsのMIDI **Input port**も **20** にする。受信チャンネルを1（またはAll）、BridgeのMIDI CHANNELを1にする。
4. ギターを弾く。Bridge上部のノート表示が変われば音声解析は動作している。受信音源のアタックが長いプリセットは発音が遅く感じられるので、最初は短いアタックの音色で試す。
5. MONO PITCHでLOOSE／STRICTを切り替える。POLYにすると和音が出力される。POLYでMONO PITCHの選択が保持されても、和音解析自体は同一。

音が出ない場合: MIDIのOutput/Inputを取り違えていないか、MIDI CHANNELが一致するか、WrapperにMIDI出力が認識されているかを確認。音声入力はBridgeのINPUTメーターで確認。ギター音も混ぜたいときだけDry guitar monitorをON。インターフェースのダイレクトモニターはこのスイッチとは独立。

**MIDI Outは別のMIDI発生源で、Bridgeの受信先ではありません。** Bridge→VSTiを直接接続します。この手順の対象はWrapperのポート設定があるVST版の音源。FLネイティブ音源は同じ設定画面とは限らず、ここでは互換性未確認です。

参考: [WrapperのMIDIポートとPitch Bend Range](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/wrapper.htm)、[MIDI Outの役割](https://ww2.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/MIDI%20Out.htm)。

## Cubase / VST3

1. ギターのオーディオトラックへBridgeをインサートし、入力モニターをON。
2. 音源のInstrumentトラックまたは音源へ接続するMIDIトラックを用意。
3. そのトラックのMIDI Input RoutingにBridgeのMIDI出力が現れる場合はそれを選択。受信トラックのモニター／録音待機をON。MIDIトラック経由ならOutput Routingを目的のVSTiへ設定。
4. Bridgeが入力候補に現れないホスト／バージョン／構成では、次のDevice MIDI経路を利用。今回Cubase実機での入力候補表示は未検証です。

公式の入力・出力設定: [Setting the MIDI Input](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/recording/recording_setting_the_midi_input_t.html)、[MIDI Channel and Output](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/recording/recording_setting_the_midi_channel_and_output_t.html)。これらはトラック設定の根拠で、この独自プラグインの実機互換性を保証する資料ではありません。

## Device MIDI / DAW内で直接ルーティングできない場合

BridgeはOSが列挙するMIDI出力先にも送信できます。仮想MIDIの**ループバック用ポート**を使えば、DAW側はその入力を普通のMIDIキーボードと同様に受け取れます。単なる実機のMIDI OUT端子は、そのままDAWのMIDI INに戻るものではありません。

1. OS側で使用可能な仮想MIDIループバックポートを準備する。Bridge自身はWindowsの仮想ドライバーを作成／インストールしません。
2. Bridgeの **MIDI DESTINATION = Device MIDI**。Refreshを押し、その出力先を選択。Device connectedと表示されることを確認。
3. DAWで対応するMIDI入力を有効化し、音源トラックの入力として選択。受信トラックのモニターをON。
4. **Host MIDI経路と同時に同じ音源へ戻さない**。Host + deviceは別々の受信先を使う場合の設定。Bridgeは入力MIDIを取り込まないため、MIDI THRUによる自己変換ループは作りません。

出力デバイスIDはプラグインの状態に保存します。不在／接続不能ならDevice connectedにはなりません。自動で別デバイスへ送信しないので、Refreshから選び直してください。設定変更・終了時には、そのインスタンスが使用したチャンネルに停止メッセージを送ります。

直接出力は**ライブ用**。音声スレッドから固定長キューに渡し、別スレッドでOSへ送ります。イベントのサンプル位置はホスト内出力では保持されますが、直接デバイス経路は受け取ったブロックから即時送信し、OSスケジューリング／最大1ブロック程度の位置ずれがあります。オフライン高速バウンス中はデバイスへ送りません。MIDIを録音するかHost MIDI経路でレンダリングします。

## Studio One / Device MIDI経由の接続案

1. 上記の仮想ループバックポートを用意し、BridgeからDevice MIDIで送る。
2. Studio OneのOptions（macOS: Preferences）→External Devices→Add→**New Keyboard**。
3. Receive Fromに対応する仮想ポートを設定。ノート・ピッチベンドの入力フィルターを無効にし、受信チャンネルを合わせる。
4. 目的の音源のInstrumentトラックの入力にそのKeyboardを選び、モニター／録音待機をON。

この接続はMIDI入力デバイスとして扱う方法です。Studio One内のオーディオFX→音源直接接続が全バージョンで使えるとするものではありません。音声FXはギター側トラックで入力モニターを有効にし、音源側トラックへの音声ルーティングは不要。

公式: [Studio One Pro 7 Basic MIDI Setup](https://support.presonus.com/hc/en-us/articles/29973355204237-Studio-One-Pro-7-Basic-Midi-Setup-New-Keyboard-Control-Surface-Instrument-Faderport)。実機未検証。

## Logic Pro / Macでの次の検証

**LogicはVST3を直接ロードしません。** [Apple公式の対応形式](https://support.apple.com/en-gb/102082)はAudio Unitsです。本プロジェクトはMac用AUオーディオエフェクトとStandaloneを生成する設定を持ちますが、このWindowsでMacのビルド・署名・auval／Logicでの検証はできていません。

接続案:

1. MacのAudio MIDI設定→MIDIスタジオ→IAC Driverをオンラインにし、ループバック用バスを作成。[Apple公式IAC手順](https://support.apple.com/en-ca/guide/audio-midi-setup/ams1013/mac)。
2. MacでビルドしたStandaloneにギター音声を入れるか、LogicのオーディオトラックにAU版Bridgeを挿入。これはMIDI FX枠のプラグインではなく、音声入力を解析するオーディオFX。
3. BridgeのDevice MIDIからIACバスへ送る。Logicの目的のソフト音源トラックでIAC入力を受け、録音待機／モニターする。
4. IACの出力を再び同じIACへTHRUするルーティングを作らない。Mac実機での音声入力、デバイス列挙、ノート／ベンド／停止動作の確認が必要。

Standaloneの標準JUCE画面で入力がミュートされている場合は、入力ミュートを解除し、Optionsで音声デバイスと入力チャンネルを選びます。Dry guitar monitorは最初はOFF。Windows版Standaloneでも音声デバイスの同時使用可否やドライバーの排他動作は環境依存です。

## ベンド・ベロシティ・停止

- **MONO Pitch Bend Rangeを受信音源と一致**させる。±2が初期値。RPN 0を受け取る音源ではSend bend rangeを任意にONにできるが、非対応の音源は手動設定。音源のプリセット変更で範囲が戻ることもある。
- POLYは同一チャンネルの最大6音。個別弦の連続ピッチベンド、MPE、サステインCC出力は未実装。MONOからPOLYへ切り替える際は先のベンドを中央へ戻す。
- Fixed velocityが0なら入力の強弱を使用。1〜127なら固定。音源ごとの音量差は受信音源側の設定にも依存する。
- NOTE HOLDはピッチが一時的に検出されない場合の保持時間。音声ゲートが閉じるとNote Offを送る。シンセのリリース時間を制御するものではない。
- PANICはNote Off／All Notes Offとベンド中央を送り、入力がゲート以下になるまで再発音を抑止。手を離してから再演奏。
- 停止・バイパス・モード変更を処理。ただしホストがプラグインを呼ばず即停止／削除すると、ホスト向けNote Offを後から返すことはできない。ホスト側の停止時All Notes Offを有効にし、持続音中に削除する前はPANICを使う。
