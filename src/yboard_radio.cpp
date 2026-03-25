// ===== Y-Board Radio Helpers — Implementation =====
//
// Packet layout inside BLE manufacturer-specific advertisement data:
//   [0–1]  Company ID  (COMPANY_ID_LO, COMPANY_ID_HI)
//   [2]    Group       (0–255, set by yboard_radio_set_group)
//   [3]    Counter     (increments each send; used for deduplication)
//   [4–7]  Serial      (lower 4 bytes of BT MAC; identifies the sender)
//   [8]    Type        (TYPE_NUMBER=0, TYPE_STRING=1, TYPE_VALUE=2)
//   [9+]   Payload
//            NUMBER : float (4 bytes)
//            STRING : null-terminated, up to MAX_STRING_LEN chars
//            VALUE  : 8-byte null-padded name + float (4 bytes)
//
// Total BLE legacy advertising payload: 31 bytes
//   − 3 bytes FLAGS AD structure
//   − 2 bytes manufacturer AD overhead (length + type)
//   − 9 bytes our header
//   = 17 bytes available for payload

#include "yboard_radio.h"
#include <BLEAdvertising.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <esp_bt.h>
#include <string.h>

// Company ID 0x4259 = ASCII "BY" (first two letters of BYU), little-endian in packet
#define COMPANY_ID_LO 0x42 // 'B'
#define COMPANY_ID_HI 0x59 // 'Y'
#define TYPE_NUMBER 0
#define TYPE_STRING 1
#define TYPE_VALUE 2
#define HEADER_SIZE 9 // company(2) + group(1) + counter(1) + serial(4) + type(1)
#define MAX_PAYLOAD 17
#define MAX_STRING_LEN 16 // MAX_PAYLOAD - 1 null terminator
#define MAX_NAME_LEN 8    // matches micro:bit MakeCode name field width

// --- Module state ---

static uint8_t s_group = 0;
static uint8_t s_pkt_counter = 0;
static uint32_t s_serial = 0;
static int s_last_rssi = 0;

static void (*s_on_number)(float) = nullptr;
static void (*s_on_string)(const char *) = nullptr;
static void (*s_on_value)(const char *, float) = nullptr;

// --- Deduplication cache ---
// Tracks the last packet counter seen from each remote serial so that the
// ~200 ms advertising burst (several identical advertisements) only triggers
// one callback.

#define DEDUP_SIZE 16
static struct {
    uint32_t serial;
    uint8_t counter;
    bool valid;
} s_dedup[DEDUP_SIZE];

static bool dedup_is_new(uint32_t serial, uint8_t counter) {
    for (int i = 0; i < DEDUP_SIZE; i++) {
        if (s_dedup[i].valid && s_dedup[i].serial == serial) {
            if (s_dedup[i].counter == counter) {
                return false; // already seen
            }
            s_dedup[i].counter = counter;
            return true;
        }
    }
    // New sender — find an empty slot.
    for (int i = 0; i < DEDUP_SIZE; i++) {
        if (!s_dedup[i].valid) {
            s_dedup[i] = {serial, counter, true};
            return true;
        }
    }
    // Cache full — evict the oldest entry (slot 0) and shift left.
    for (int i = 0; i < DEDUP_SIZE - 1; i++) {
        s_dedup[i] = s_dedup[i + 1];
    }
    s_dedup[DEDUP_SIZE - 1] = {serial, counter, true};
    return true;
}

// --- BLE scan callback ---

class RadioScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        if (!dev.haveManufacturerData()) {
            return;
        }

        std::string mfr = dev.getManufacturerData();
        const uint8_t *d = reinterpret_cast<const uint8_t *>(mfr.data());

        if ((int)mfr.size() < HEADER_SIZE) {
            return;
        }
        if (d[0] != COMPANY_ID_LO || d[1] != COMPANY_ID_HI) {
            return;
        }
        if (d[2] != s_group) {
            return;
        }

        uint32_t sender;
        memcpy(&sender, d + 4, 4);
        if (sender == s_serial) {
            return; // own packet
        }

        uint8_t counter = d[3];
        if (!dedup_is_new(sender, counter)) {
            return; // duplicate
        }

        uint8_t type = d[8];
        size_t plen = mfr.size() - HEADER_SIZE;
        const uint8_t *payload = d + HEADER_SIZE;

        s_last_rssi = dev.getRSSI();

        if (type == TYPE_NUMBER && plen >= 4 && s_on_number) {
            float v;
            memcpy(&v, payload, 4);
            s_on_number(v);

        } else if (type == TYPE_STRING && s_on_string) {
            char str[MAX_PAYLOAD + 1];
            size_t n = (plen < (size_t)MAX_PAYLOAD) ? plen : (size_t)MAX_PAYLOAD;
            memcpy(str, payload, n);
            str[n] = '\0';
            s_on_string(str);

        } else if (type == TYPE_VALUE && plen >= MAX_NAME_LEN + 4u && s_on_value) {
            char name[MAX_NAME_LEN + 1];
            memcpy(name, payload, MAX_NAME_LEN);
            name[MAX_NAME_LEN] = '\0';
            float v;
            memcpy(&v, payload + MAX_NAME_LEN, 4);
            s_on_value(name, v);
        }
    }
};

static RadioScanCallbacks s_scan_cb;
static BLEScan *s_scan = nullptr;

// --- Internal send helper ---

static void send_packet(uint8_t type, const uint8_t *payload, size_t plen) {
    if (plen > MAX_PAYLOAD) {
        plen = MAX_PAYLOAD;
    }

    uint8_t buf[HEADER_SIZE + MAX_PAYLOAD];
    buf[0] = COMPANY_ID_LO;
    buf[1] = COMPANY_ID_HI;
    buf[2] = s_group;
    buf[3] = ++s_pkt_counter;
    memcpy(buf + 4, &s_serial, 4);
    buf[8] = type;
    memcpy(buf + HEADER_SIZE, payload, plen);

    std::string mfr(reinterpret_cast<char *>(buf), HEADER_SIZE + plen);

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    BLEAdvertisementData data;
    data.setManufacturerData(mfr);
    adv->setAdvertisementData(data);
    adv->setScanResponse(false);
    adv->start();
    delay(200); // advertise long enough for nearby scanners to receive the packet
    adv->stop();
}

// --- Public API ---

void yboard_radio_begin() {
    BLEDevice::init("YBoard");

    // Derive a stable 32-bit serial from the lower 4 bytes of the BT MAC.
    BLEAddress addr = BLEDevice::getAddress();
    memcpy(&s_serial, addr.getNative() + 2, 4);

    s_scan = BLEDevice::getScan();
    s_scan->setAdvertisedDeviceCallbacks(&s_scan_cb, /*wantDuplicates=*/true);
    s_scan->setActiveScan(false); // passive scan — no scan requests, lower power
    s_scan->setInterval(100);
    s_scan->setWindow(99);            // listen 99% of the time
    s_scan->start(0, nullptr, false); // 0 = scan indefinitely
}

void yboard_radio_set_group(uint8_t group) { s_group = group; }

void yboard_radio_set_power(uint8_t power) {
    static const esp_power_level_t kMap[] = {
        ESP_PWR_LVL_N12, // 0 ≈ −12 dBm
        ESP_PWR_LVL_N9,  // 1 ≈ −9 dBm
        ESP_PWR_LVL_N6,  // 2 ≈ −6 dBm
        ESP_PWR_LVL_N3,  // 3 ≈ −3 dBm
        ESP_PWR_LVL_N0,  // 4 ≈   0 dBm
        ESP_PWR_LVL_P3,  // 5 ≈  +3 dBm
        ESP_PWR_LVL_P6,  // 6 ≈  +6 dBm
        ESP_PWR_LVL_P9,  // 7 ≈  +9 dBm
    };
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, kMap[power < 8 ? power : 7]);
}

void yboard_radio_send_number(float value) {
    uint8_t payload[4];
    memcpy(payload, &value, 4);
    send_packet(TYPE_NUMBER, payload, 4);
}

void yboard_radio_send_string(const char *str) {
    // Include the null terminator in the payload so the receiver always gets a
    // properly terminated string even if plen bytes are consumed verbatim.
    size_t len = strnlen(str, MAX_STRING_LEN) + 1;
    send_packet(TYPE_STRING, reinterpret_cast<const uint8_t *>(str), len);
}

void yboard_radio_send_value(const char *name, float value) {
    uint8_t payload[MAX_NAME_LEN + 4] = {}; // zero-initialise so name is null-padded
    strncpy(reinterpret_cast<char *>(payload), name, MAX_NAME_LEN);
    memcpy(payload + MAX_NAME_LEN, &value, 4);
    send_packet(TYPE_VALUE, payload, MAX_NAME_LEN + 4);
}

void yboard_radio_on_number(void (*cb)(float)) { s_on_number = cb; }
void yboard_radio_on_string(void (*cb)(const char *)) { s_on_string = cb; }
void yboard_radio_on_value(void (*cb)(const char *, float)) { s_on_value = cb; }

int yboard_radio_received_signal_strength() { return s_last_rssi; }
