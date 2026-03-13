
#include "Arduino.h"
#include "yboard.h"
#include "yboard_helper.h"

void setup() {
    Serial.begin(9600);
    Yboard.setup();
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

    yboard_effect_rainbow();
    delay(100);
}
