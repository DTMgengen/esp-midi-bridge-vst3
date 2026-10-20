# MIDI演奏と音声の録音

## まずMIDIファイルを残す

v0.2.0では、音源へのMIDI出力と並行して、同じノート・ベロシティ・ピッチベンドをBridge内部で記録できます。音声はこれまでどおりDAW側で録音します。

1. Bridgeの **Follow DAW record** をON。ギター入力をモニターし、受信VSTiから音が出る状態にする。
2. DAWで録音開始。Bridgeの表示が **Recording** になることを確認。
3. 演奏後、DAWで停止。**MIDI Takes** を押し、保存された `.mid` を確認する。
4. FL Studioの対象音源のPiano rollへ読み込む。最初の音を入力開始時刻からの相対位置で保存するので、DAWの録音開始位置へテイクを合わせる。

保存先はOSのミュージックフォルダー内 `ESP MIDI Bridge/MIDI Takes`。テスト／特殊環境では起動前の環境変数 `ESP_MIDI_BRIDGE_TAKES_DIR` で変更できます。通常の利用で設定は不要です。

録音状態を通知しないホストでは **REC MIDI** を押してからDAWで録音、演奏後 **STOP MIDI**。BridgeとDAWの開始位置は手動で合わせます。Standaloneでも手動録音を使えます。

**Save MIDI...** は最後のテイクのコピー保存。録音中は使えません。自動保存に失敗してもテイクをメモリに保持します。書けるフォルダーへSave MIDIで保存してください。失敗を解消するまで次のテイクを上書き開始しません。

ファイルには960 PPQN、ノート、強弱、14bitピッチベンド、RPNベンド範囲、テンポと拍子を保存。ホストがテンポを通知しなければ120 BPM・4/4を使用します。曲内の絶対位置は埋め込まず、録音開始を0とした連続テイクです。ループ／パンチ録音の自動レーン分割、オフライン書き出し時の自動MIDI録音はありません。停止時には保持中のノートを閉じます。

上限は262,144イベント。上限でノートを閉じて保存し、ライブMIDI／音声出力は継続します。表示でFullになったら新しいテイクを開始してください。ファイル書き込みは専用スレッドで処理します。追加の解析待ち時間は加えません。

## FL StudioのPiano rollへリアルタイム録音する

WrapperのOutput/Input portを合わせるHost MIDI接続では、音源が鳴っても、FLの入力録音に同じイベントが届くとは限りません。Piano rollへ直接録音するときは、MIDIキーボードと同じ入口へ戻す接続を使います。

1. Windowsで[loopMIDI（開発元）](https://www.tobias-erichsen.de/software/loopmidi.html)等の仮想MIDIループバックを別途用意。専用ポート（例：ESP Bridge Record）を1つ作り、起動しておく。Bridgeのインストーラーにはドライバーを含みません。
2. FLの **Options → MIDI settings → Input** でそのポートを選び、EnableをON、Controller typeをGeneric controllerにする。通常のノート録音だけならFL側InputのPort番号は不要です。Bridgeの出力デバイス名と同じループバックの入力を選びます。
3. Bridgeの **MIDI DESTINATION = Device MIDI**。Refreshし、作成したポートを選ぶ。**Device connected** を確認。
4. Channel Rackで録音先音源を選び、Piano rollを空にしたPatternを用意。複数コントローラーがある場合は、対象音源の **Receive notes from** でこの入力を割り当てる。
5. FLのRecordボタンを右クリックし **Notes** を有効化、録音開始してギターを弾く。停止後、PatternのPiano rollにノートがあることを確認。
6. 同時に音声も残すなら、FL側で音源が鳴るMixerトラックのディスク録音等を設定し、RecordのAudioも有効化する。Dry monitorは生ギターを通す設定で、音源の音声録音先ではありません。

同じ音源へHost MIDIとループバックの両方を接続すると二重発音します。Device MIDI経路を選び、以前のHostポート接続を外してください。ループバックへFL側から再送信するMIDI THRU構成は作らないでください。

**ベンドの注意：** FLのNotes録音と、外部MIDIピッチベンドの記録／再生の扱いは同じではありません。全14bitベンドと範囲を確実に残すにはBridgeのMIDIテイクも同時に保存してください。MIDIファイルを読み込むDAW／音源でもベンド範囲を一致させ、FLで読み込んだベンドの再生は実環境で確認してください。ベンドを不要とする半音単位の編集にはBridgeのMONO pitch bendをOFFにできます。ベンド対応を未検証のまま「Piano rollへ完全再現」とはしていません。

根拠：[FL MIDI録音の公式手順](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/recording_scores.htm)、[Channel Rackの入力割当](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm)、[MIDI設定](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_midi.htm)。このプラグインと実際のFLでの同時録音・ループバック試奏は別途確認が必要です。

## 他のDAW

プラグイン出力をMIDIトラック入力に選べるホストでは、そのトラックを録音待機にします。入力候補に出ない場合はDevice MIDIから仮想ポートへ返して受信トラックを録音します。音声録音とBridgeの`.mid`保存は併用できます。[DAW別ガイド](DAW_SETUP.ja.md)。macOSビルド／Logicの実機接続は未検証です。
