// include/secrets.h

struct WiFiConfig
{
    const char *ssid;
    const char *password;
};

// 接続先のリストを作成
const WiFiConfig myWiFiList[] = {
    {"SSID01", "PASS01"}, // **用
    {"SSID02", "PASS02"}, // **用
    {"SSID03", "PASS03"}  // **用
};

// リストの件数を計算しておく
const int myWiFiCount = sizeof(myWiFiList) / sizeof(myWiFiList[0]);