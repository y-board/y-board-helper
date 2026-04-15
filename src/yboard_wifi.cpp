// ===== Y-Board WiFi Helpers — Implementation =====

#include "yboard_wifi.h"
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

void yboard_wifi_connect(const char *ssid, const char *password, unsigned long timeout_ms) {
    WiFi.begin(ssid, password);
    unsigned long deadline = millis() + timeout_ms;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(500);
    }
}

bool yboard_wifi_connected() { return WiFi.status() == WL_CONNECTED; }

String yboard_wifi_ip() { return WiFi.localIP().toString(); }

// Shared helper: build a WiFiClientSecure that skips cert verification and
// run the already-begun HTTPClient request, returning the body or "".
static String _finish_request(HTTPClient &http) {
    int code = http.GET();
    String result = (code > 0) ? http.getString() : "";
    http.end();
    return result;
}

String yboard_http_get(const String &url) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int code = http.GET();
    String result = (code > 0) ? http.getString() : "";
    http.end();
    return result;
}

String yboard_http_post(const String &url, const String &body) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(body);
    String result = (code > 0) ? http.getString() : "";
    http.end();
    return result;
}

void yboard_wifi_download(const String &url, const String &filename) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    if (http.GET() == HTTP_CODE_OK) {
        String path = filename.startsWith("/") ? filename : "/" + filename;
        File f = SD.open(path, FILE_WRITE);
        if (f) {
            http.writeToStream(&f);
            f.close();
        }
    }
    http.end();
}

void yboard_wifi_time_sync(int timezone_offset_hours) {
    configTime((long)timezone_offset_hours * 3600, 0, "pool.ntp.org");
    struct tm t;
    unsigned long deadline = millis() + 10000;
    while (!getLocalTime(&t, 1000) && millis() < deadline) {
        // waiting for NTP sync
    }
}

int yboard_wifi_get_time(int component) {
    struct tm t;
    if (!getLocalTime(&t, 1000)) {
        return 0;
    }
    switch (component) {
    case 0: return t.tm_hour;
    case 1: return t.tm_min;
    case 2: return t.tm_sec;
    case 3: return t.tm_mday;
    case 4: return t.tm_mon + 1;
    case 5: return t.tm_year + 1900;
    case 6: return t.tm_wday;
    default: return 0;
    }
}
