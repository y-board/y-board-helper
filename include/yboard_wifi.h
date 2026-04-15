// ===== Y-Board WiFi Helpers =====
// Standard WiFi connectivity, HTTP requests, SD file downloads, and NTP
// time sync for Blockly-generated programs.
//
// Quick start:
//   setup(): yboard_wifi_connect("MyNet", "password");
//            yboard_wifi_time_sync(-7);  // UTC-7 (US Mountain)
//   loop():  String temp = yboard_http_get("http://api.example.com/temp");
//
// NOTE: WiFi and the Radio (ESP-NOW) are mutually exclusive — do not use
// yboard_radio_begin() and yboard_wifi_connect() in the same program.

#ifndef YBOARD_WIFI_H
#define YBOARD_WIFI_H

#include <Arduino.h>

// Connect to a WiFi network. Blocks until connected or timeout_ms elapses
// (default 30 s). Place this in setup().
void yboard_wifi_connect(const char *ssid, const char *password,
                         unsigned long timeout_ms = 30000);

// Returns true if the board is currently connected to WiFi.
bool yboard_wifi_connected();

// Returns the board's current IP address as a String (e.g. "192.168.1.42").
String yboard_wifi_ip();

// Perform an HTTP GET request and return the response body as a String.
// Returns "" on connection error. Supports HTTP and HTTPS (no cert check).
String yboard_http_get(const String &url);

// Perform an HTTP POST with a URL-encoded body and return the response body.
// Returns "" on connection error. Supports HTTP and HTTPS (no cert check).
String yboard_http_post(const String &url, const String &body);

// Download a file from a URL and save it to the SD card.
// filename: e.g. "sound.wav" — a leading "/" is added automatically.
// Supports HTTP and HTTPS (no cert check).
void yboard_wifi_download(const String &url, const String &filename);

// Sync the board's real-time clock from NTP (pool.ntp.org).
// timezone_offset_hours: whole-hour UTC offset, e.g. -7 for US Mountain Time.
// Blocks up to 10 s waiting for a response. Requires WiFi to be connected.
void yboard_wifi_time_sync(int timezone_offset_hours);

// Return one component of the current local time.
// component: 0=hour (0-23), 1=minute (0-59), 2=second (0-59),
//            3=day (1-31),  4=month (1-12),  5=year (e.g. 2025),
//            6=weekday (0=Sunday).
// Returns 0 if the clock has not been synced yet.
int yboard_wifi_get_time(int component);

#endif // YBOARD_WIFI_H
