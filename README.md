# Enhanced Timeline AviUtl プラグイン

拡張編集のタイムラインに対する操作を自由にカスタマイズできるプラグインです．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_enhanced_tl/releases) [紹介動画．](https://www.nicovideo.jp/watch/sm45190275)


## 機能紹介
以下の機能があります．各機能は[設定ファイル](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/設定ファイルについて)で個別に有効化 / 無効化できます．

詳しい使い方は [Wiki](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki) を参照してください．

1.  **タイムライン移動コマンド関連**

    https://github.com/user-attachments/assets/c3bd1051-cdf0-4e8d-a289-d3d24012c986

    ショートカットキー操作でオブジェクト境界への現在フレーム移動 (編集点への移動) や，タイムラインのスクロール，BPMグリッドへの移動などができます．

    [[詳細はこちら](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/タイムライン移動コマンドの機能)]

    - [TLショトカ移動](https://github.com/sigma-axis/aviutl_tl_walkaround) の機能移植です．

1.  **タイムライン右クリックメニューの追加機能**

    ![追加コンテキストメニュー](https://github.com/user-attachments/assets/04cb10e6-027f-480f-a513-e34cc6b9a559)

    タイムラインの右クリックメニューに一部機能を追加しています．

    [[詳細はこちら](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/タイムライン右クリックメニューの追加機能)]

1.  **ツールチップ**

    https://github.com/user-attachments/assets/3909f530-48c3-4b6c-9bee-4eba4befaa79

    タイムラインの各所にツールチップを表示するようになります．

    [[詳細はこちら](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/ツールチップ機能)]

1.  **マウス操作のカスタマイズ機能**

    https://github.com/user-attachments/assets/7e425680-bf04-4cf8-b800-6288c83ec505

    タイムラインに対するマウス入力を置き換え，新しいマウス操作も追加します．

    各種マウスボタンやホイールと Ctrl / Shift / Alt キーとの組み合わせで，マウス操作を自由に割り当てられます．

    [[詳細はこちら](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/マウス操作のカスタマイズ機能)]

    - [TLショトカ移動](https://github.com/sigma-axis/aviutl_tl_walkaround)，[可変幅レイヤー](https://github.com/sigma-axis/aviutl_layer_resize)，[レイヤー一括切り替え](https://github.com/sigma-axis/aviutl_toggle_layers)，[Layer Wheel 2](https://github.com/sigma-axis/aviutl_layer_wheel2) の一部機能を移植・統合・拡張したものです．

1.  **レイヤー幅を自由に調整する機能**

    https://github.com/user-attachments/assets/c9e1a77a-27dc-445a-8b6f-a2d1c83f38ab

    タイムラインのレイヤー幅を，既存の **大・中・小** よりも細かく調整できるようになります．

    ウィンドウのドラッグ操作で調整したり，メニューコマンドに割り当てたショートカットキーで調節したりできます．

    [[詳細はこちら](https://github.com/sigma-axis/aviutl_enhanced_tl/wiki/レイヤー幅を自由に調整する機能)]

    - [可変幅レイヤー](https://github.com/sigma-axis/aviutl_layer_resize) の機能移植です．


## 動作要件

- AviUtl 1.10 + 拡張編集 0.92

  https://spring-fragrance.mints.ne.jp/aviutl
  - 拡張編集 0.93rc1 等の他バージョンでは動作しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

## 導入方法

以下のフォルダのいずれかに `enhanced_tl.auf` と `enhanced_tl.ini` をコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ
1. (2) のフォルダにある任意のフォルダ

##  競合情報

このプラグインは，次の 4 つのプラグインを統合し，機能追加する目標で作成しました．一方で **これらのプラグインとは競合します．** 導入の際にはこれらのプラグインは削除してください．これらのプラグインでできることは Enhanced Timeline で可能です．

1.  [TLショトカ移動](https://github.com/sigma-axis/aviutl_tl_walkaround)
1.  [レイヤー一括切り替え](https://github.com/sigma-axis/aviutl_toggle_layers)
1.  [可変幅レイヤー](https://github.com/sigma-axis/aviutl_layer_resize)
1.  [Layer Wheel 2](https://github.com/sigma-axis/aviutl_layer_wheel2)

##  TIPS

1.  このプラグインは AviUtl のメニューコマンドに多数のコマンドを追加します．一方でこのコマンドには， *全プラグイン合わせて* 最大で 256 個という制限があります．他のプラグインなども合わさってこの上限を超えてしまうと，本来追加されるはずのコマンドが出てこなくなります．

    この場合，[sub_menu_item](https://github.com/nazonoSAUNA/sub_menu_item) というプラグインを導入し，メニューコマンドを取捨選択することで解決できます．


## 改版履歴

- **v1.01** (2025-08-09)

  - レイヤードラッグ操作時の自動スクロールが，条件によっては機能しなかったのを修正．

- **v1.00** (2025-07-15)

  - 初版．

## ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (C) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org/


#  Credits

##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#  連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://x.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
