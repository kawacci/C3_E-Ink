#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time.h"
#include <LittleFS.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

WiFiMulti wifiMulti;

unsigned long lastUpdateMillis = 0; // 最後にカレンダーを更新した時間を記録する変数
bool isFirstBoot = true;            // 初回起動フラグ
int lastUpdatedDay = -1;            // 最後に更新した「日」を覚えておく変数

int RX_PIN = 18;            // ESP32C3のRXピンはGPIO18、TXピンはGPIO19を使用
int TX_PIN = 19;            // ESP32C3のRXピンはGPIO18、TXピンはGPIO19を使用
#define EINK_SERIAL Serial1 // ESP32C3のシリアルポート1を使用（GPIO18, GPIO19）

LGFX_Sprite canvas;                // メインのキャンバス
LGFX_Sprite canvas_mini(&canvas);  // canvasを親に持つ小さいキャンバス
LGFX_Sprite canvas_ribon(&canvas); // canvasを親に持つリボン用キャンバス

const char *day_str[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"}; // 曜日ごとにリボンの色を変えるための配列
uint16_t day_matrix[42];                                                   // カレンダーの日付を格納する行列（6行7列=42マス）
int year;                                                                  // int month; // これらはグローバル変数として宣言
int month;                                                                 // int day; // これらはグローバル変数として宣言
int day;                                                                   // int wday; // 曜日を格納する変数（0:日曜, 1:月曜...）
int wday;                                                                  // これらはグローバル変数として宣言
int hour;                                                                  // これらはグローバル変数として宣言
int minute;                                                                // これらはグローバル変数として宣言
int second;                                                                // これらはグローバル変数として宣言

// 祝日かどうかを判定する関数（簡易版）
bool isHoliday(int y, int m, int d)
{
    char dateStr[11];
    sprintf(dateStr, "%04d-%02d-%02d", y, m, d);

    HTTPClient http;
    // ESP32-C3でHTTPSを簡単に扱う設定
    http.begin("https://holidays-jp.github.io/api/v1/date.json");

    int httpCode = http.GET();
    if (httpCode == 200)
    {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);

        // その日付がキーとして存在するかチェック
        if (doc.containsKey(dateStr))
        {
            http.end();
            return true;
        }
    }
    http.end();
    return false;
}

void setupWiFi()
{
    // 一旦、Wi-Fiを完全にリセット
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(100);

    // 接続先を登録
    // secrets.h で定義したリストをすべて登録
    for (int i = 0; i < myWiFiCount; i++)
    {
        wifiMulti.addAP(myWiFiList[i].ssid, myWiFiList[i].password);
        Serial.printf("Registered AP: %s\n", myWiFiList[i].ssid);
    }

    // Wi-Fiに接続

    Serial.println("Connecting WiFi via Multi-Scan...");

    // wifiMulti.run() を呼ぶ前に、一度だけ通常スキャンを走らせると
    // 混合モードのAPを見つけやすくなる場合があります
    WiFi.scanNetworks(true);

    // 接続を試行
    while (wifiMulti.run() != WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");

        // 10回（10秒）経ってもダメなら、一度スキャンをリセット
        static int retry_count = 0;
        if (++retry_count % 10 == 0)
        {
            Serial.println("\nRescan...");
            WiFi.scanNetworks(true);
        }
    }

    Serial.println("\nWiFi Connected!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID()); // どっちに繋がったか表示
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    configTime(9 * 3600, 0, "ntp.nict.jp", "time.google.com");
}

void getNowTime() // 現在の年月日と曜日を取得する関数
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    year = timeinfo.tm_year + 1900;
    month = timeinfo.tm_mon + 1;
    day = timeinfo.tm_mday;
    wday = timeinfo.tm_wday; // 0:日, 1:月...

    // 時刻の取得（追加分）
    hour = timeinfo.tm_hour;  // 0 - 23
    minute = timeinfo.tm_min; // 0 - 59
    second = timeinfo.tm_sec; // 0 - 59

    // デバッグ用にシリアル出力
    Serial.printf("Now: %02d:%02d:%02d\n", hour, minute, second);
    Serial.printf("Today: %d/%d/%d\n", year, month, day);
}

void updateDayMatrix()
{ // カレンダーの行列を更新する関数
    // 1. その月の1日の情報を取得
    struct tm first_day = {0};
    first_day.tm_year = year - 1900;
    first_day.tm_mon = month - 1;
    first_day.tm_mday = 1;
    mktime(&first_day); // これで tm_wday（曜日）が計算される

    // 2. その月が何日まであるか（2月や閏年も考慮）
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
    {
        days_in_month[1] = 29;
    }

    // 3. 行列をリセットして埋める
    for (int i = 0; i < 42; i++)
        day_matrix[i] = 0;

    int start_index = first_day.tm_wday; // 1日の曜日
    for (int d = 1; d <= days_in_month[month - 1]; d++)
    {
        day_matrix[start_index + d - 1] = d;
    }
}

// ハンドシェイク（'a'を送って'b'を待つ）
void send_begin()
{
    EINK_SERIAL.write('a');
    while (1)
    {
        if (EINK_SERIAL.available() > 0)
        {
            if (EINK_SERIAL.read() == 'b')
            {
                Serial.println("Handshake Success!");
                break;
            }
        }
    }
}

// 成功した転送ロジック（152x152, 1=白, 0=着色）
void send_canvas_data(int color_mode)
{
    uint8_t buffer[76];
    int buf_idx = 0;

    for (int y = 0; y < 152; y++)
    {
        for (int x_byte = 0; x_byte < 19; x_byte++)
        {
            uint8_t b = 0;
            for (int bit = 0; bit < 8; bit++)
            {
                int px = x_byte * 8 + bit;
                uint16_t color = canvas.readPixel(px, y);

                bool bit_value = true; // デフォルトは白(1)

                if (color_mode == 0)
                {
                    // 黒モード：黒(0x0000)なら着色(0)
                    if (color == 0x0000)
                        bit_value = false;
                }
                else
                {
                    // 赤モード：赤(0xF800)なら着色(0)
                    if (color == 0xF800)
                        bit_value = false;
                }

                if (bit_value)
                    b |= (0x80 >> bit);
            }
            buffer[buf_idx++] = b;

            if (buf_idx == 76)
            {
                EINK_SERIAL.write(buffer, 76);
                buf_idx = 0;
                delay(70);
            }
        }
    }
}

void drawOutlineString(const char *str, int x, int y) // 文字に白い縁取りを付ける関数
{
    canvas.setTextColor(TFT_WHITE);
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            canvas.drawString(str, x + dx, y + dy);
        }
    }
    canvas.setTextColor(TFT_BLACK);
    canvas.drawString(str, x, y);
}

void drawTodayCalendar(int i) // 今日の日付に赤枠を付ける
{
    if (day == i)
    {
        canvas_mini.fillScreen(TFT_RED);
        canvas_mini.fillRect(2, 2, 16, 14, TFT_WHITE);
    }
}

void drawCalendar() // カレンダー全体を描画する関数
{
    getNowTime(); // 現在の年月日と曜日を取得
    Serial.println("Time Updated!");
    Serial.printf("Today: %d/%d/%d\n", year, month, day);
    updateDayMatrix(); // カレンダーの行列を更新
    Serial.println("Day Matrix Updated!");

    // --- 描画開始 ---
    canvas.fillScreen(TFT_WHITE);

    // 年月を左上に
    canvas.setTextColor(TFT_BLACK);
    canvas.setTextSize(1);
    canvas.drawString(String(year) + " / " + String(month), 10, 5);
    char timeStr[10];
    sprintf(timeStr, "%02d : %02d", hour, minute); // 0埋めで2桁にする
    canvas.drawString(timeStr, 95, 5);

    // 曜日の表示
    for (int i = 0; i < 7; i++)
    {
        canvas_ribon.fillScreen(TFT_WHITE);
        if (i == 0)
        {
            canvas_ribon.setTextColor(TFT_RED);
            canvas_ribon.drawFastHLine(1, 13, 18, TFT_RED);
        }
        else
        {
            canvas_ribon.setTextColor(TFT_BLACK);
            canvas_ribon.drawFastHLine(1, 13, 18, TFT_BLACK);
        }
        canvas_ribon.setTextSize(1);
        canvas_ribon.setTextDatum(textdatum_t::middle_center);
        canvas_ribon.drawString(day_str[i], 10, 7);
        canvas_ribon.pushSprite(6 + i * 20, 19);
    }

    // 日付の表示
    int day_num = 0;
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 7; j++)
        {
            canvas_mini.fillScreen(TFT_WHITE);
            int current_day = day_matrix[day_num]; // 0かどうかの判定用

            if (current_day != 0)
            { // 日付があるときだけ描画
                canvas_mini.setTextSize(1);

                // 祝日または日曜日の判定
                if (j == 0 || isHoliday(year, month, current_day))
                {
                    canvas_mini.setTextColor(TFT_RED);
                    canvas_mini.drawFastHLine(1, 17, 19, TFT_RED); // 赤い線
                }
                else
                {
                    canvas_mini.setTextColor(TFT_BLACK);
                    canvas_mini.drawFastHLine(1, 17, 19, TFT_BLACK); // 黒い線
                }

                drawTodayCalendar(current_day); // 今日の赤枠判定

                canvas_mini.setTextDatum(textdatum_t::middle_center);
                canvas_mini.drawString(String(current_day), 10, 9);
            }

            // 0のときは真っ白な（fillScreenした直後の）状態が転送される
            canvas_mini.pushSprite(6 + j * 20, 33 + i * 20);
            day_num++;
        }
    }

    /*画像を表示する場合の例（コメントアウトしている部分を有効にしてください）
     * 画像は152x152ピクセルで、黒い部分がTFT_BLACK、赤い部分がTFT_REDとして描画されます。
     * 画像ファイルはLittleFSのルートディレクトリに「test.png」という名前で保存してください。
     * 画像が存在しない場合は「File Not Found」と表示されます。
     *
     * 画像を表示した後、文字を重ねて書くこともできます。
     * 文字の色やサイズはsetTextColorやsetTextSizeで調整してください。

    canvas.fillScreen(TFT_WHITE);

    // 【重要】PNG画像をキャンバスに描画
    // 画像内の黒い部分はTFT_BLACK、赤い部分はTFT_REDとして展開されます
    if (LittleFS.exists("/test.png")) // デバッグ用
    {
        canvas.drawPngFile(LittleFS, "/test.png", 0, 0); // デバッグ用
    }
    else
    {
        canvas.drawString("File Not Found", 10, 10); // デバッグ用
    }

    // その上から文字を書く
    canvas.setTextColor(TFT_BLACK);              // 黒色で文字を書く
    //canvas.setTextColor(TFT_BLACK, TFT_WHITE);   // 黒色で文字を書く（背景は白）
    canvas.setTextSize(2);                       // 文字サイズを2倍にする
    //canvas.drawString("Current Temp:", 10, 100); // 温度を表示する文字を書く
    drawOutlineString("Current Temp:", 10, 100);

    // 赤色で強調したい文字を書く

    canvas.setTextColor(TFT_RED);            // 赤色で文字を書く
    //canvas.setTextColor(TFT_RED, TFT_WHITE); // 赤色で文字を書く（背景は白）
    canvas.setTextSize(3);                   // 文字サイズを3倍にする
    //canvas.drawString("25.5 C", 10, 125);    // 温度を表示する文字を書く
    drawOutlineString("25.5 C", 10, 125);
    */

    // 転送シーケンス
    send_begin(); // ハンドシェイク
    delay(500);

    Serial.println("Sending Black...");
    send_canvas_data(0);
    delay(100);

    Serial.println("Sending Red...");
    send_canvas_data(1);
    delay(100);

    EINK_SERIAL.write(0x08); // 更新実行
    Serial.println("Update Done!");
}

void setup()
{
    Serial.begin(115200);
    delay(2000); // シリアルモニターが落ち着くのを待つ
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000)
    {
        delay(10);
    }
    Serial.println("WiFi Connecting...");
    setupWiFi();
    Serial.println("WiFi Connected!");

    EINK_SERIAL.begin(230400, SERIAL_8N1, RX_PIN, TX_PIN);

    // ファイルシステム開始
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS Mount Failed");
        return;
    }

    // キャンバス作成
    canvas.setColorDepth(16);
    canvas.createSprite(152, 152);
    canvas.setFont(&fonts::Font2); // デフォルトの小さいフォントを使用

    canvas_mini.setColorDepth(16);
    canvas_mini.createSprite(20, 18);
    canvas_mini.setFont(&fonts::Font0); // デフォルトの小さいフォントを使用

    canvas_ribon.setColorDepth(16);
    canvas_ribon.createSprite(20, 14);
    canvas_ribon.setFont(&fonts::Font0); // デフォルトの小さいフォントを使用
}

void loop()
{
    // --- 1. Wi-Fiの状態をチェック ---
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Disconnected. Reconnecting...");
        // 再接続を試みる（既存の設定 ssid, password を使用）
        if (wifiMulti.run() != WL_CONNECTED)
        {
            // 接続できない間は、1分待ってから再度 loop の最初に戻る
            delay(60000);
            return;
        }

        // 接続を待つ間、ループを抜けて次のチェックまで待機
        // これを入れないと、接続待ちで loop が止まってしまうのを防げます
        delay(5000);
        return;
    }

    // --- 2. 時刻を取得 ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        // Wi-Fiは繋がっているがNTPサーバーから時刻が取れない場合
        Serial.println("Time sync failed.");
        delay(1000);
        return;
    }

    // --- 3. 更新判定 ---
    bool shouldUpdate = false;

    // A: 00:05以降で、まだ今日更新していない場合
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min >= 5 && lastUpdatedDay != timeinfo.tm_mday)
    {
        shouldUpdate = true;
    }

    // B: 起動後、最初の1回目
    if (lastUpdatedDay == -1)
    {
        shouldUpdate = true;
    }

    // --- 4. 更新の実行 ---
    if (shouldUpdate)
    {
        // 180秒ルールガード（前回更新から3分経っていない場合はスキップ）
        if (lastUpdatedDay != -1 && millis() - lastUpdateMillis < 180000)
        {
            Serial.println("180s guard: update postponed.");
            return;
        }

        Serial.println("Updating Calendar...");
        drawCalendar(); // ここに全ての描画・送信処理が入っている前提

        lastUpdatedDay = timeinfo.tm_mday;
        lastUpdateMillis = millis();
        Serial.println("Display Update Completed.");
    }

    // --- 5. チェックの間隔 ---
    // 1分だと頻繁に感じるなら、ここを5分(300000)にするのがバランス良いです
    delay(180000);
}
