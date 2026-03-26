// ===== Y-Board Radio Helpers =====
// ESP-NOW-based broadcast radio that mirrors the MakeCode micro:bit radio API.
// Uses the WiFi radio for low-latency group-filtered broadcasting between
// Y-Boards — no connection required.
//
// Quick start:
//   setup(): yboard_radio_begin();
//            yboard_radio_set_group(42);
//            yboard_radio_on_number([](float v){ Serial.println(v); });
//   loop():  yboard_radio_send_number(millis());

#ifndef YBOARD_RADIO_H
#define YBOARD_RADIO_H

#include <Arduino.h>

// Initialize the radio. Must be called once in setup() before any other
// radio functions. Sets WiFi to station mode internally; do not call
// WiFi.mode() or WiFi.begin() after this.
void yboard_radio_begin();

// Set the group (0–255). Packets are only delivered to devices in the same
// group. Default: 0.
void yboard_radio_set_group(uint8_t group);

// Set transmit power: 0 (lowest) to 7 (highest). Default: 6.
void yboard_radio_set_power(uint8_t power);

// Broadcast a number, string, or name+number pair to every device in the
// same group. Strings are truncated to 32 characters; names to 12 characters.
void yboard_radio_send_number(float value);
void yboard_radio_send_string(const char *str);
void yboard_radio_send_value(const char *name, float value);

// Register a callback that fires when a matching packet is received.
// Callbacks are invoked from the WiFi/ESP-NOW task, not the Arduino loop —
// keep them short and avoid modifying shared state without synchronization.
// The char* pointers passed to callbacks are only valid during the call.
void yboard_radio_on_number(void (*callback)(float value));
void yboard_radio_on_string(void (*callback)(const char *str));
void yboard_radio_on_value(void (*callback)(const char *name, float value));

// Returns the RSSI (dBm) of the most recently received packet.
// Best called from within a receive callback.
int yboard_radio_received_signal_strength();

#endif // YBOARD_RADIO_H
