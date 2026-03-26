
#include "Arduino.h"
#include "yboard.h"
#include "yboard_helper.h"
#include "yboard_radio.h"

void setup() {
    Serial.begin(9600);
    Yboard.setup();

    // Radio: group 1, receive callbacks
    yboard_radio_begin();
    yboard_radio_set_group(1);
    yboard_radio_on_number([](float v) {
        Serial.print("Received number: ");
        Serial.println(v);
    });
    yboard_radio_on_string([](const char *s) {
        Serial.print("Received string: ");
        Serial.println(s);
    });
    yboard_radio_on_value([](const char *name, float v) {
        Serial.print("Received value: ");
        Serial.print(name);
        Serial.print(" = ");
        Serial.println(v);
    });
}

void loop() {
    // Test microphone helper
    int mic_level = yboard_microphone_level(128);
    Serial.print("Microphone level: ");
    Serial.println(mic_level);

    // Test accelerometer helper
    float accel_mag = yboard_accel_magnitude();
    Serial.print("Accelerometer magnitude: ");
    Serial.println(accel_mag);

    // Send sensor readings over radio (switch 1 = sender mode)
    if (Yboard.get_switch(1)) {
        yboard_radio_send_value("accel", accel_mag);
    }

    yboard_effect_rainbow();
    delay(100);
}
