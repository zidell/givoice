# Givoice

[![CI](https://github.com/zidell/givoice/actions/workflows/ci.yml/badge.svg)](https://github.com/zidell/givoice/actions/workflows/ci.yml) [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[English](README.md) | [한국어](README.ko.md) | [中文](README.zh-CN.md) | [日本語](README.ja.md) | [Español](README.es.md)

> 個人が作って無料で公開しているアプリです。自由に使っていただけますが、機能の提案、バグ報告、使い方のサポートは受け付けていません。必要な機能があれば、GPLv3に従ってフォークして改変してください。

Givoiceは、マイクの録音を文字起こしし、現在の入力欄へ貼り付ける音声入力アプリです。必要な機能だけをネイティブ実装し、待機時メモリは約20 MBを目標にしています。OpenAI・ElevenLabs・Groq APIを選べます。独自モデルは搭載していません（Groqは無料でも利用できます）。

## 機能

- 1つのキーで2通りに使えます。短く押して離すともう一度押すまで録音し、1秒以上押し続けると離したときに録音が終わります。Escによるキャンセル、録音時間制限に対応。
- 録音・文字起こし状態とライブ入力レベルをウィジェットで表示（既定の位置は画面下中央、**表示しない**設定も可能）。
- ウィジェットを消しても、メニューバー（トレイ）アイコンが録音中は赤、変換中はオレンジに変わります。
- 設定でショートカット、言語、モデル、開始音量、録音ウィジェットの位置、自動Enterを変更可能。
- **認識単語**で固有名詞の精度を上げ、**置換単語**（`探す語 => 置き換える語`）で直らない誤認識を貼り付け直前に修正できます。置き換える語に `[enter]`、`[cmd+k]` のように書くと、その位置で実際にキーを押します。
- ネットワークやAPIのエラーで文字起こしに失敗しても録音を失わないよう、録音の原本（WAV）を保管します。保存期間は設定で1時間・1日・7日・30日から選べ（既定は7日）、期間を過ぎたログと録音は自動で削除されます。メニューの**ログと録音原本フォルダ**から診断ログと一緒に開けます。APIキーと文字起こし内容はログに記録されません。
- フォーカスが移って結果が消えたり別の場所に貼り付けられたりしたときは、メニュー上部の**もう一度貼り付け**で最後の結果を今のカーソル位置に貼り付け直せます。結果はアプリを終了するまでメモリにだけ保持し、自動Enterは繰り返しません。

## 作った理由

音声入力アプリはすでに数多くあります。複数の有料・無料アプリを試し、Redditの利用体験も参考にしました。ローカルSTTモデルはまだ精度が十分ではなく、実用上はOpenAIとElevenLabsのSTT APIが最も正確で安定していると感じました。有料アプリは機能に比べて高価だったり、機能過多で修正が頻繁だったり、待機中に100 MB以上のメモリを使ったりします。無料アプリの一部は完成度や配布品質が期待に届きませんでした。

## プレビュー

以下はコントロールメニューと利用の流れです。

### コントロールメニュー

| macOS | Windows |
| --- | --- |
| ![macOSメニューバーで開いたGivoiceメニュー](assets/screenshots/macos-menu-preview.png) | ![Windowsトレイで開いたGivoiceメニュー](assets/screenshots/windows-menu-preview.png) |

### 利用の流れ

| macOS | Windows |
| --- | --- |
| ![macOSで録音、文字起こし、自動貼り付けを行うGivoice](assets/screenshots/givoice-flow.gif) | ![Windowsで録音、文字起こし、自動貼り付けを行うGivoice](assets/screenshots/givoice-windows-flow.gif) |

## インストール

ダウンロードとインストール方法は[Givoiceダウンロードページ](https://zidell.github.io/givoice)をご覧ください。すべてのファイルは[GitHub Releases](https://github.com/zidell/givoice/releases)に固定のファイル名で公開され、ダウンロードページから最新版を直接ダウンロードできます。新しいバージョンが出ると、macOS版はお知らせしたうえでインストールし、Ubuntu版はトレイメニューにダウンロード項目を表示します。Windows版はダウンロードページから新しいEXEを入手してください。

| OS | ファイル | インストールまたは実行 |
| --- | --- | --- |
| macOS Apple Silicon | `Givoice-macos-arm64.dmg` | DMGを開き、アプリをApplicationsへコピー |
| macOS Intel | `Givoice-macos-x64.dmg` | DMGを開き、アプリをApplicationsへコピー |
| Windows 10/11 x64 | `Givoice-windows-x64.exe` | EXEをそのまま実行（インストール不要） |
| Ubuntu 24.04+ amd64 | `Givoice-ubuntu-amd64.deb` | [Ubuntuネイティブ版のインストール](native/linux/README.md) |

## 最初の使用

1. メニューバーまたはトレイのGivoiceアイコンから**設定...**を開き、先頭のOpenAI・ElevenLabs・Groqから選びます。キー入力欄の右に、選択したサービスのキー作成ボタンを1つ表示します。Groqの既定モデルは`whisper-large-v3-turbo`です。
2. macOSではマイクと**システム設定 → プライバシーとセキュリティ → アクセシビリティ**を許可します。Windowsでは**設定 → プライバシーとセキュリティ → マイク**でデスクトップアプリのマイクアクセスを許可します。Ubuntuでは、設定のショートカット / 権限タブで、システムのダイアログに従ってグローバルショートカットの登録とキーボード操作（自動貼り付け）を許可します。
3. 入力したい欄にカーソルを置き、右Command（macOS）、右Alt（Windows）、またはCtrl+Alt+Space（Ubuntu）を押したまま話します。キーを離すと文字起こし結果が貼り付けられます。

設定では、ショートカット、録音停止方法、録音時間（10・20・30・60分、既定30分）、認識言語、開始音量、自動送信を変更できます。自動送信を有効にすると、貼り付け後にEnterも押されます。管理者として実行されているWindowsアプリへ自動入力するには、Givoiceも同じ権限で実行する必要がある場合があります。

## Ubuntu対応

macOS版・Windows版とは別に、C/GTKでコンパイルしたUbuntuネイティブ版を提供しています。
[最新のUbuntuリリース](https://github.com/zidell/givoice/releases?q=linux-v&expanded=true)から`.deb`インストーラーをダウンロードでき、
ビルド・インストール・使い方は[Ubuntuガイド](native/linux/README.md)にまとめています。
PipeWire/PulseAudioでの録音と3つの文字起こしサービスに対応し、GNOMEとKDE Plasmaでは
デスクトップポータルでグローバルショートカットと自動貼り付けを使います。必要なポータルがない
デスクトップでは、アプリの録音ボタンで録音し、結果をコピーして貼り付けます。

## STTプロバイダー

macOS・Windowsでは設定の先頭でエンジンを選び、サービスごとにキーとモデルを保存します。Ubuntuと旧設定はAPIキーの接頭辞から選択します。

| プロバイダー | APIキー接頭辞 | 既定モデル |
| --- | --- | --- |
| OpenAI | `sk-` | `gpt-transcribe` |
| ElevenLabs | `sk_` | `scribe_v2` |
| Groq | `gsk_` | `whisper-large-v3-turbo` |

GroqはOpenAI互換の文字起こしAPIを使います。設定でGroqを選び、右側のキー作成ボタンからAPIキーを作成して入力すると、モデル一覧を読み込めます。

### APIキーの取得

以下の手順で作成したキーをGivoiceの**設定... → APIキー**に貼り付け、**更新**を押してください。キーはパスワードと同様に扱い、共有や公開はしないでください。

ChatGPTのサブスクリプションはOpenAI API料金とは別です。ElevenLabsで個人APIキーを作るにはFull Seatが必要です。Groqは無料で始めやすい一方、無料枠には使用量とレートの制限があります。

#### OpenAI

1. [OpenAI APIキーのページ](https://platform.openai.com/api-keys)でログインまたは登録します。
2. API Platformの**Billing**でAPIの支払い方法またはクレジットを設定します。ChatGPT Plus・ProだけではAPI利用料は含まれません。
3. 使用するプロジェクトを選択します。個人利用なら既定のプロジェクトのままで構いません。
4. **Create new secret key**を押し、名前を付けてキーを作成します。
5. 表示された`sk-…`キーをGivoiceにコピーします。

OpenAI APIキーはプロジェクト単位で作成され、権限と利用上限はプロジェクト設定で管理できます。[OpenAI公式ガイド](https://help.openai.com/en/articles/9186755)

#### ElevenLabs

1. [ElevenLabs APIキーのページ](https://elevenlabs.io/app/developers/api-keys)でログインまたは登録します。
2. 個人APIキーの作成に必要な**Full Seat**を持っていることを確認します。なければ適切なプランまたはワークスペース管理者の設定が必要です。
3. 個人APIキー一覧で新しいキーを作成し、分かりやすい名前を付けます。
4. 作成された`sk_…`キーをGivoiceにコピーします。

個人キーは個人APIキー設定で作成・更新できます。[ElevenLabs公式ガイド](https://elevenlabs.io/docs/overview/administration/workspaces/api-keys)

#### Groq

1. [Groq Console APIキーのページ](https://console.groq.com/keys)でログインまたは登録します。無料枠でもキーを作成できます。
2. 初回はプロジェクト選択からプロジェクトを作成するか、既定プロジェクトを選びます。
3. **Create API Key**を押し、名前を付けてキーを作成します。
4. 作成された`gsk_…`キーをGivoiceにコピーします。

Groqキーは選択したプロジェクトに属し、無料枠にはリクエストと音声処理の上限があります。上限を増やす有料Developer tierへの移行には支払い方法が必要です。[Groqプロジェクトガイド](https://console.groq.com/docs/projects)、[料金ガイド](https://console.groq.com/docs/billing-faqs)

## トラブルシューティング

アプリの状態とエラーはメニューバーまたはトレイメニューで確認できます。**ログと録音原本フォルダ**で診断ログ（`debug.log`）と録音原本（`recording-*.wav`）があるフォルダを開きます。

- macOS: `~/Library/Application Support/givoice/logs/`
- Windows: `%APPDATA%\givoice\logs\`
- Ubuntu: `~/.local/share/givoice/logs/`（または`$XDG_DATA_HOME/givoice/logs/`）
- 開発実行: `dist-native/logs/`

[プライバシー通知](docs/privacy.md)では音声送信とAPIキー保存について説明しています。ソースコードは[GNU GPL v3ライセンス](LICENSE)で公開しています。
