# Windowsインストーラー

v0.3.0は試用版です。FL終了時の停止が一度あり原因を切り分け中です。

Releasesから `ESP-MIDI-Bridge-Setup-0.3.0-win-x64.exe` をダウンロードして実行します。日本語／英語のInno Setupウィザードです。Windows 10（1809以降）／11のx64対応環境が対象。管理者権限で全ユーザー向けに導入します。

- VST3: `C:\Program Files\Common Files\VST3\ESP MIDI Bridge.vst3`
- 接続ガイド・ライセンス・アンインストーラー: `C:\Program Files\Gengen Audio\ESP MIDI Bridge`
- 同梱ランタイム: Microsoft署名付きVisual C++ x64 Redistributable。ビルド時に署名とバージョンを検査。既存ランタイムが14.29.30133以上ならスキップし、不足時だけ導入します。再起動が必要と返されたら、VST3を書き込む前に終了し、再起動後の再実行を案内します。
- Standalone、他の音源、仮想MIDIドライバーは含みません。設定／DAWプロジェクトも操作しません。

DAWを閉じてから実行し、導入後はプラグインを再スキャンしてください。Windowsの「設定 → アプリ → インストールされているアプリ → ESP MIDI Bridge」から削除できます。ガイドへのスタートメニュー項目も用意します。完了時にガイドを開く選択肢は初期OFFです。

更新／削除前に対象のVST3とメタデータを排他オープンできるか確認します。使用中または書込不可なら中止し、DAWの終了を案内します。DAWの強制終了や自動再起動は行いません。アンインストールはインストーラーが記録したファイルを削除し、他のプラグインや追加された未知のファイルを再帰削除しません。Microsoftの共有ランタイムは残します。

## 作成

Release VST3をビルドしてから、Inno Setup 6.3以降とMicrosoft VC++ x64 Redistributableのパスを指定します。SDK由来のライセンスも必要です。既存の`build/CMakeCache.txt`にJUCEソースパスがあれば、それを使います。

```powershell
./scripts/build-installer.ps1 `
  -Compiler 'C:/Tools/InnoSetup/ISCC.exe' `
  -VCRedist 'C:/Tools/vc_redist.x64.exe' `
  -JuceSource 'C:/Sources/JUCE'
```

コンパイラーは標準のInno Setup 6インストール先も検索します。VC++再頒布パッケージは、このPCのVisual Studio 2019 Build Tools内の署名付きファイルも使用できます。`-ExpectedModuleHash`で検証済みのVST3モジュールのSHA-256を指定すると、不一致で作成を止めます。ファイルバージョンとCMakeのバージョンも照合します。

生成するEXEと、そのSHA-256ファイル・manifest JSONは`dist/installer/`に保存します。ランタイム・SDK・コンパイラー・作業用payload・生成EXEはGitに含めません。公開バイナリはGitHub Releasesへ、AGPLv3の対応ソースは同じリリースのソース／JUCEアーカイブへ分けて配置します。[対応ソースの取得方法](SOURCE.md)。今回のセットアップはコード署名を付けていません。

## 検証

```powershell
./scripts/test-installer.ps1 -Compiler 'C:/Tools/InnoSetup/ISCC.exe'
```

同じISSから**別AppId・非管理者・プロジェクト内だけに導入するテスト版**を作成します。製品版の固定インストール先／権限だけを切り替え、普段使っているVST3やその登録には触れません。テストルートは必ずこのプロジェクトの`local/`以下に限定します。

検証する内容:

1. 無人インストール、VC++導入済み時のスキップ、モジュール／メタデータのハッシュ一致、ガイドと削除登録。
2. 使用中ファイルの更新・削除の拒否。更新は準備段階の終了コード7で停止。
3. 同じバージョンの再インストールで、検証用に破損させたコピーが元に戻ること。
4. 導入コピーを本物のVST3としてロードし、MONO LOOSE／MONO STRICT／POLYのMIDI出力を確認。
5. 無人アンインストールと登録解除。隣の別プラグインと、バンドル内の未管理ファイルを保持。
6. 普段のESP MIDI BridgeとESP String Labのモジュールハッシュが変わらないこと。

管理者昇格を伴う製品版そのものの導入と、VC++不足PCでの実際のランタイム導入／再起動、完全なクリーンPCでの検証は未実施です。テスト版は既存ランタイムがないと中止し、PC全体の実行環境を書き換えません。現在のPCでは新しい互換ランタイムを検出してスキップする経路を確認しています。

仕様の根拠: [Inno Setupの権限](https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm)、[アプリを自動終了しない設定](https://jrsoftware.org/ishelp/topic_setup_closeapplications.htm)、[VC++ランタイムの互換条件](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)。
