// ===== Y-Board Radio Helpers — Implementation =====
//
// Uses ESP-NOW for group-filtered broadcast between Y-Boards.
// ESP-NOW is built on the WiFi PHY and supports low-latency, non-blocking,
// connectionless broadcasts — the same model as the micro:bit radio.
//
// Packet layout (ESP-NOW payload, max 250 bytes):
//   [0-1]  Magic "BY" (0x42, 0x59) — filter out non-YBoard traffic
//   [2]    Group       (0–255, set by yboard_radio_set_group)
//   [3]    Counter     (increments each send; used for deduplication)
//   [4-7]  Serial      (lower 4 bytes of WiFi MAC; identifies the sender)
//   [8]    Type        (TYPE_NUMBER=0, TYPE_STRING=1, TYPE_VALUE=2)
//   [9+]   Payload:
//            NUMBER : float (4 bytes)
//            STRING : null-terminated, up to MAX_STRING_LEN chars
//            VALUE  : null-padded name (MAX_NAME_LEN bytes) + float (4 bytes)

#include "yboard_radio.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <string.h>

#define MAGIC_LO       0x42  // 'B'
#define MAGIC_HI       0x59  // 'Y'
#define TYPE_NUMBER    0
#define TYPE_STRING    1
#define TYPE_VALUE     2
#define HEADER_SIZE    9
#define MAX_STRING_LEN 32
#define MAX_NAME_LEN   12
#define MAX_PAYLOAD    (250 - HEADER_SIZE)

static const uint8_t kBroadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Module state ---

static uint8_t  s_group       = 0;
static uint8_t  s_pkt_counter = 0;
static uint32_t s_serial      = 0;
static int      s_last_rssi   = 0;

static void (*s_on_number)(float)              = nullptr;
static void (*s_on_string)(const char *)       = nullptr;
static void (*s_on_value)(const char *, float) = nullptr;

// --- Deduplication cache ---
// Prevents a single send() from triggering more than one callback if the
// sender is within range of multiple re-broadcasts (not currently used by
// ESP-NOW, but kept for forward compatibility and consistency with the API).

#define DEDUP_SIZE 16
static struct {
    uint32_t serial;
    uint8_t  counter;
    bool     valid;
} s_dedup[DEDUP_SIZE];

static bool dedup_is_new(uint32_t serial, uint8_t counter) {
    for (int i = 0; i < DEDUP_SIZE; i++) {
        if (s_dedup[i].valid && s_dedup[i].serial == serial) {
            if (s_dedup[i].counter == counter) return false;
            s_dedup[i].counter = counter;
            return true;
        }
    }
    for (int i = 0; i < DEDUP_SIZE; i++) {
        if (!s_dedup[i].valid) {
            s_dedup[i] = {serial, counter, true};
            return true;
        }
    }
    for (int i = 0; i < DEDUP_SIZE - 1; i++) s_dedup[i] = s_dedup[i + 1];
    s_dedup[DEDUP_SIZE - 1] = {serial, counter, true};
    return true;
}

// --- Packet dispatch (shared by both callback signatures) ---

static void dispatch(const uint8_t *data, int len) {
    if (len < HEADER_SIZE)                               return;
    if (data[0] != MAGIC_LO || data[1] != MAGIC_HI)     return;
    if (data[2] != s_group)                              return;

    uint32_t sender;
    memcpy(&sender, data + 4, 4);
    if (sender == s_serial)                              return;
    if (!dedup_is_new(sender, data[3]))                  return;

    uint8_t        type    = data[8];
    size_t         plen    = (size_t)len - HEADER_SIZE;
    const uint8_t *payload = data + HEADER_SIZE;

    if (type == TYPE_NUMBER && plen >= 4 && s_on_number) {
        float v;
        memcpy(&v, payload, 4);
        s_on_number(v);

    } else if (type == TYPE_STRING && s_on_string) {
        char str[MAX_STRING_LEN + 1];
        size_t n = plen < (size_t)MAX_STRING_LEN ? plen : (size_t)MAX_STRING_LEN;
        memcpy(str, payload, n);
        str[n] = '\0';
        s_on_string(str);

    } else if (type == TYPE_VALUE && plen >= (size_t)(MAX_NAME_LEN + 4) && s_on_value) {
        char name[MAX_NAME_LEN + 1];
        memcpy(name, payload, MAX_NAME_LEN);
        name[MAX_NAME_LEN] = '\0';
        float v;
        memcpy(&v, payload + MAX_NAME_LEN, 4);
        s_on_value(name, v);
    }
}

// --- ESP-NOW receive callback ---
// The callback signature changed in ESP-IDF 5.0; handle both.

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void on_receive(const esp_now_recv_info_t *info,
                       const uint8_t *data, int len) {
    s_last_rssi = info->rx_ctrl->rssi;
    dispatch(data, len);
}
#else
static void on_receive(const uint8_t * /*mac*/, const uint8_t *data, int len) {
    dispatch(data, len);
}
#endif

// --- Public API ---

void yboard_radio_begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Pin all boards to channel 1 so ESP-NOW frames are received without an AP.
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    esp_now_init();
    esp_now_register_recv_cb(on_receive);

    // Register the broadcast peer so esp_now_send() reaches all nearby boards.
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcastAddr, 6);
    peer.channel = 1;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    // Derive a stable 32-bit serial from the lower 4 bytes of the WiFi MAC.
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    memcpy(&s_serial, mac + 2, 4);
}

void yboard_radio_set_group(uint8_t group) { s_group = group; }

void yboard_radio_set_power(uint8_t power) {
    // esp_wifi_set_max_tx_power() units: 0.25 dBm, valid range 8–84 (2–21 dBm).
    static const int8_t kMap[] = {8, 18, 28, 38, 48, 58, 68, 78};
    esp_wifi_set_max_tx_power(kMap[power < 8 ? power : 7]);
}

static void send_packet(uint8_t type, const uint8_t *payload, size_t plen) {
    if (plen > (size_t)MAX_PAYLOAD) plen = MAX_PAYLOAD;

    uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
    buf[0] = MAGIC_LO;
    buf[1] = MAGIC_HI;
    buf[2] = s_group;
    buf[3] = ++s_pkt_counter;
    memcpy(buf + 4, &s_serial, 4);
    buf[8] = type;
    memcpy(buf + HEADER_SIZE, payload, plen);

    esp_now_send(kBroadcastAddr, buf, HEADER_SIZE + plen);
}

void yboard_radio_send_number(float value) {
    uint8_t p[4];
    memcpy(p, &value, 4);
    send_packet(TYPE_NUMBER, p, 4);
}

void yboard_radio_send_string(const char *str) {
    size_t len = strnlen(str, MAX_STRING_LEN) + 1;
    send_packet(TYPE_STRING, reinterpret_cast<const uint8_t *>(str), len);
}

void yboard_radio_send_value(const char *name, float value) {
    uint8_t payload[MAX_NAME_LEN + 4] = {};
    strncpy(reinterpret_cast<char *>(payload), name, MAX_NAME_LEN);
    memcpy(payload + MAX_NAME_LEN, &value, 4);
    send_packet(TYPE_VALUE, payload, MAX_NAME_LEN + 4);
}

void yboard_radio_on_number(void (*cb)(float))              { s_on_number = cb; }
void yboard_radio_on_string(void (*cb)(const char *))       { s_on_string = cb; }
void yboard_radio_on_value(void (*cb)(const char *, float)) { s_on_value  = cb; }

int yboard_radio_received_signal_strength()                 { return s_last_rssi; }
