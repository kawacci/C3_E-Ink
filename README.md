# M5Stack Electronic Paper Calendar
M5Stamp C3 と Seeed Studio 製の 1.54インチ 3色電子ペーパー（E-Ink）を使用した、自動更新型のデスクトップカレンダーです。
![](/pic/c-ink.jpg)


## ✨ 主な機能
- 自動更新: 毎日深夜（00:05以降）に最新の日付と祝日情報を取得し、自動で画面を書き換えます。

- 時刻表示: 右上に最終更新時刻を表示し、同期状態を一目で確認できます。

- WiFiMulti 対応: 自宅、会社、モバイルテザリングなど、複数の接続先を優先順位に従って自動で切り替えます。

- 3色表示: 黒・白・赤の3色を活かし、日曜日や祝日を分かりやすく強調します。

- 省電力設計: 書き換え時以外は電力を消費しない電子ペーパーの特性を活かしています。

## 🛠 ハードウェア要件
- Controller: M5Stamp C3 (ESP32-C3)

- Display: [Grove - Triple Color E-Ink Display 1.54''](https://wiki.seeedstudio.com/Grove-Triple_Color_E-Ink_Display_1_54/)

- Case: 自作 3D プリントケース

## 📚 参考資料
本プロジェクトでは、電子ペーパーの制御に以下の公式factoryCodeを使用しています。

- [Seeed-Studio/Grove_Triple_Color_E-lnk_1.54](https://github.com/Seeed-Studio/Grove_Triple_Color_E-lnk_1.54)


また、描画の最適化には LovyanGFX を活用しています。

## 🚀 セットアップと使い方
1. **WiFi 設定の準備**
セキュリティのため、WiFi の SSID とパスワードは別ファイルで管理します。
`include/secrets.h` を作成し、以下の形式で設定を記述してください。

```c++
struct WiFiConfig {
    const char* ssid;
    const char* password;
};

const WiFiConfig myWiFiList[] = {
    {"YOUR_SSID_1", "YOUR_PASS_1"},
    {"YOUR_SSID_2", "YOUR_PASS_2"}
};

const int myWiFiCount = sizeof(myWiFiList) / sizeof(myWiFiList[0]);
```
2. **書き込み**
PlatformIO を使用して、M5Stamp C3 にプログラムを書き込みます。

## 💡 技術的な工夫
GitHub 公開時の注意: include/secrets.h を GitHub にアップロードしないよう、必ず .gitignore に追記してください。


## 👤 作成者
- Kawacci

- Planning and Development Department

## 📄 ライセンス
- MIT License