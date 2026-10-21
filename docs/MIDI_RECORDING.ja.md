# MIDI演奏と音声の録音

## クリーン版MIDIを使う（v0.3・推奨）

演奏用MIDIはそのまま送り、**録音を停止した後**に別のMIDIファイルへノート整理を行います。

1. Bridgeの **Auto-save cleaned copy**をON。設定はテイク開始時に確定します。
2. **REC MIDI**を押す。FLでも必要なオーディオ録音を開始して演奏する。
3. **STOP MIDI**で停止。OSのミュージックフォルダー内 `ESP MIDI Bridge/MIDI Takes`に、同じテイクの **-original.mid** と **-clean.mid** が残る。
4. **Drag clean MIDI**を音源のPiano rollへドラッグして読み込む。**Save cleaned...**はクリーン版の別名保存、**Save original...**は元データの保存。MIDI Takesからも取得できる。

DAWが録音状態を通知する場合はFollow DAW recordも使えます。確認済みのFL構成では自動開始しなかったため、FLではREC MIDI／STOP MIDIを使ってください。**この録音・保存はBridge内部で行うため、loopMIDIがなくても使えます。**

判定は保守的です。18ms未満の短い音に対し、同じチャンネルの前後350ms以内に重ならない安定音があり、周囲より明らかに弱い場合を候補にします。12ms未満で前後両方から1オクターブ以上離れる音も、周囲より弱いときだけ候補にします。強い短音・18ms以上の音・重なっている和音・前後の根拠が足りない音はこの判定では残します。低音／高音への一方向の補正、音程の丸め、ライブ側のゲート変更はありません。

演奏意図の完全な識別はできません。小さく短い装飾音が候補になる場合や、長い誤検出・固定ベロシティの誤検出が残る場合があります。元データは必ず保存し、削除対象のNote On／Offをペアで除きます。残すノートの時刻・音長・強弱・ピッチベンド・テンポ・拍子・RPNは変更しません。テンポ変化も秒へ換算して判定します。

FLのPiano rollへ直接入力したノートをプラグインから書き換える機能ではありません。クリーン版を別の空のPatternへ読み込み、元の録音と聴き比べてください。

**FL StudioのPiano rollへノートを直接残す場合は、下の「リアルタイム録音する」を使ってください。** Bridge内のMIDIファイル保存は、Piano rollへの直接録音とは別の機能です。

## まずMIDIファイルを残す

v0.2.0では、音源へのMIDI出力と並行して、同じノート・ベロシティ・ピッチベンドをBridge内部で記録できます。音声はこれまでどおりDAW側で録音します。

1. Bridgeの **Follow DAW record** をON。ギター入力をモニターし、受信VSTiから音が出る状態にする。
2. DAWで録音開始。Bridgeの表示が **Recording** になることを確認。
3. 演奏後、DAWで停止。**MIDI Takes** を押し、保存された `.mid` を確認する。
4. FL Studioの対象音源のPiano rollへ読み込む。最初の音を入力開始時刻からの相対位置で保存するので、DAWの録音開始位置へテイクを合わせる。

保存先はOSのミュージックフォルダー内 `ESP MIDI Bridge/MIDI Takes`。テスト／特殊環境では起動前の環境変数 `ESP_MIDI_BRIDGE_TAKES_DIR` で変更できます。通常の利用で設定は不要です。

録音状態を通知しないホストでは **REC MIDI** を押してからDAWで録音、演奏後 **STOP MIDI**。BridgeとDAWの開始位置は手動で合わせます。Standaloneでも手動録音を使えます。

**FL Studio 26.1.7での実ギター試奏では、Follow DAW recordによるファイル録音は自動開始しませんでした。** FLで`.mid`も残す場合はREC MIDI／STOP MIDIを操作し、Recording／Saved表示を確認してください。下記のPiano roll録音はこの自動開始に依存しません。

**Save original...** は最後の元テイクのコピー保存。録音中は使えません。自動保存に失敗してもテイクをメモリに保持します。書けるフォルダーへSave originalで保存してください。失敗を解消するまで次のテイクを上書き開始しません。クリーン版だけ保存に失敗した場合は元ファイルを保持し、Save cleanedから新しいファイル名へ保存できます。

ファイルには960 PPQN、ノート、強弱、14bitピッチベンド、RPNベンド範囲、テンポと拍子を保存。ホストがテンポを通知しなければ120 BPM・4/4を使用します。曲内の絶対位置は埋め込まず、録音開始を0とした連続テイクです。ループ／パンチ録音の自動レーン分割、オフライン書き出し時の自動MIDI録音はありません。停止時には保持中のノートを閉じます。

上限は262,144イベント。上限でノートを閉じて保存し、ライブMIDI／音声出力は継続します。表示でFullになったら新しいテイクを開始してください。ファイル書き込みは専用スレッドで処理します。追加の解析待ち時間は加えません。

## FL StudioのPiano rollへリアルタイム録音する

WrapperのOutput/Input portを合わせるHost MIDI接続では、音源が鳴っても、FLの入力録音に同じイベントが届くとは限りません。Piano rollへ直接録音するときは、MIDIキーボードと同じ入口へ戻す接続を使います。

1. Windowsで[loopMIDI（開発元）](https://www.tobias-erichsen.de/software/loopmidi.html)等の仮想MIDIループバックを別途用意。専用ポート（例：ESP Bridge Record）を1つ作り、録音中も起動しておく。設定画面を閉じてもトレイで動作しますが、アプリを終了するとポートも消えます。Bridgeのインストーラーにはドライバーを含みません。
2. FLの **Options → MIDI settings → Input** でそのポートを選び、EnableをON、Controller typeをGeneric controllerにする。通常のノート録音だけならFL側InputのPort番号は不要です。Bridgeの出力デバイス名と同じループバックの入力を選びます。
3. Bridgeの **MIDI DESTINATION = Device MIDI**。Refreshし、作成したポートを選ぶ。**Device connected** を確認。
4. Channel Rackで録音先音源を選び、Piano rollを空にしたPatternを用意。複数コントローラーがある場合は、対象音源の **Receive notes from** でこの入力を割り当てる。
5. FLのRecordボタンを右クリックし **Notes** を有効化。RecordをONにしてPlay、ギターを弾く。Songモードでは停止後にPatternがPlaylistへ配置され、録音先音源のPiano rollにノートが残る。ノートが見えない場合はPiano rollのズーム／スクロール位置も確認する。
6. 同時に音声も残すなら、FL側で対象Mixerトラックのディスク録音等を設定し、RecordのAudioも有効化する。ギター入力トラックを録音すれば生ギター、音源出力トラックを録音すれば音源の音声が残る。Dry monitorは生ギターを通す設定で、音源の音声録音先ではありません。

同じ音源へHost MIDIとループバックの両方を接続すると二重発音します。Device MIDI経路を選び、以前のHostポート接続を外してください。ループバックへFL側から再送信するMIDI THRU構成は作らないでください。

### loopMIDIを手動起動しない設定

Bridgeの **Auto-start installed loopMIDI**をONにします。WindowsでDevice MIDIを使うと、標準インストール先のloopMIDIを署名確認後バックグラウンド起動します。保存済みのポート設定はloopMIDI自身が復元し、Bridgeは選択済みのMIDI出力へ再接続します。複数のBridgeから重複起動せず、Bridge終了時にも共有のloopMIDIを強制終了しません。

loopMIDIの設定画面の×はトレイへ閉じる動作で、ポートを止めません。意図してloopMIDIを終了する場合は先にAuto-startをOFFにします。起動に失敗した場合はBridge下部の状態表示を確認してください。未インストールの場合の初回導入とポート作成は必要です。FLが起動時のデバイス一覧を使っている場合は、MIDI settingsのRefresh device listまたはDAW再起動が必要なことがあります。

他社ドライバー／virtualMIDI SDKは組み込まず、インストール済みの公式アプリを起動します。[開発元のloopMIDI説明](https://www.tobias-erichsen.de/software/loopmidi.html)と[SDKの配布条件](https://www.tobias-erichsen.de/software/virtualmidi/virtualmidi-sdk.html)。

**ベンドの注意：** FLのNotes録音と、外部MIDIピッチベンドの記録／再生の扱いは同じではありません。全14bitベンドと範囲を確実に残すにはBridgeのMIDIテイクも同時に保存してください。MIDIファイルを読み込むDAW／音源でもベンド範囲を一致させ、FLで読み込んだベンドの再生は実環境で確認してください。ベンドを不要とする半音単位の編集にはBridgeのMONO pitch bendをOFFにできます。ベンド対応を未検証のまま「Piano rollへ完全再現」とはしていません。

根拠：[FL MIDI録音の公式手順](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/recording_scores.htm)、[Channel Rackの入力割当](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm)、[MIDI設定](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_midi.htm)。

**実機確認（2026-10-21）：** Windows x64、FL Studio 2026 v26.1.7 build 5654、Bridge v0.2.0、loopMIDI 1.0.16.27。実ギターをMONO LOOSEで入力し、Device MIDI → 専用ループバック入力 → 選択中のNexusを演奏。ユーザーの発音確認に加え、停止後のNexus Piano rollでノートとベロシティ、Playlistで同時録音した生ギターのオーディオクリップを確認しました。プラグインの変更・再インストールは不要でした。14bitベンドの記録／再生、POLYでの同時録音、他DAWはこの試奏の検証範囲に含みません。

## 他のDAW

プラグイン出力をMIDIトラック入力に選べるホストでは、そのトラックを録音待機にします。入力候補に出ない場合はDevice MIDIから仮想ポートへ返して受信トラックを録音します。音声録音とBridgeの`.mid`保存は併用できます。[DAW別ガイド](DAW_SETUP.ja.md)。macOSビルド／Logicの実機接続は未検証です。
