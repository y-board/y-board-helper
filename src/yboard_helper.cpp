// ===== Y-Board Helper Functions — Implementation =====

#include "yboard_helper.h"

// =====================================================================
// Microphone helpers
// =====================================================================

int yboard_microphone_level(const int sample_count) {
    static int16_t buffer[sample_count];
    size_t bytes_to_read = (size_t)sample_count * sizeof(int16_t);
    size_t bytes_read = Yboard.get_microphone_stream().readBytes((uint8_t *)buffer, bytes_to_read);
    int samples_read = (int)(bytes_read / sizeof(int16_t));
    if (samples_read <= 0) {
        return 0;
    }
    long total = 0;
    for (int i = 0; i < samples_read; i++) {
        int value = (int)buffer[i];
        if (value < 0) {
            value = -value;
        }
        total += value;
    }
    return (int)(total / samples_read);
}

bool yboard_is_loud_noise(int threshold) { return yboard_microphone_level(128) >= threshold; }

// =====================================================================
// Accelerometer helpers
// =====================================================================

float yboard_accel_magnitude() {
    accelerometer_data a = Yboard.get_accelerometer();
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

// =====================================================================
// LED effect helpers
// =====================================================================

static uint8_t gHue = 0; // rotating base color used by several effects

void yboard_effect_rainbow() {
    fill_rainbow(Yboard.leds, 35, gHue, 7);
    FastLED.show();
    gHue++;
}

void yboard_effect_confetti() {
    fadeToBlackBy(Yboard.leds, 35, 10);
    int pos = random16(35);
    Yboard.leds[pos] += CHSV(gHue + random8(64), 200, 255);
    FastLED.show();
    gHue++;
}

void yboard_effect_sinelon() {
    fadeToBlackBy(Yboard.leds, 35, 20);
    int pos = beatsin16(13, 0, 34);
    Yboard.leds[pos] += CHSV(gHue, 255, 192);
    FastLED.show();
    gHue++;
}

void yboard_effect_juggle() {
    fadeToBlackBy(Yboard.leds, 35, 20);
    uint8_t dothue = 0;
    for (int i = 0; i < 8; i++) {
        Yboard.leds[beatsin16(i + 7, 0, 34)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
    FastLED.show();
}

void yboard_effect_bpm() {
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
    for (int i = 0; i < 35; i++) {
        Yboard.leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
    FastLED.show();
    gHue++;
}

void yboard_effect_fire() {
    static uint8_t heat[35];
    for (int i = 0; i < 35; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / 35) + 2));
    }
    for (int k = 35 - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }
    if (random8() < 120) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }
    for (int j = 0; j < 35; j++) {
        uint8_t t192 = scale8(heat[j], 191);
        uint8_t heatramp = t192 & 0x3F;
        heatramp <<= 2;
        if (t192 & 0x80) {
            Yboard.leds[j] = CRGB(255, 255, heatramp);
        } else if (t192 & 0x40) {
            Yboard.leds[j] = CRGB(255, heatramp, 0);
        } else {
            Yboard.leds[j] = CRGB(heatramp, 0, 0);
        }
    }
    FastLED.show();
}
