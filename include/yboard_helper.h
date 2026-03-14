// ===== Y-Board Helper Functions =====
// Reusable utility functions for Blockly-generated programs.
// Not part of the core yboard library, but useful building blocks.

#ifndef YBOARD_HELPER_H
#define YBOARD_HELPER_H

#include "yboard.h"
#include <math.h>

// --- Microphone helpers ---

// Read current microphone level (average absolute amplitude over 128 samples).
// Returns 0 if no samples could be read.
int yboard_microphone_level(const int sample_count);

// Returns true if the current microphone level meets or exceeds the threshold.
// Typical thresholds: QUIET=900, MEDIUM=1500, LOUD=2200
bool yboard_is_loud_noise(int threshold);

// --- Accelerometer helpers ---

enum yboard_orientation {
    YBOARD_ORIENTATION_SHAKE,
    YBOARD_ORIENTATION_FACE_UP,
    YBOARD_ORIENTATION_FACE_DOWN,
    YBOARD_ORIENTATION_FLAT,
    YBOARD_ORIENTATION_ON_SIDE,
    YBOARD_ORIENTATION_TILT_LEFT,
    YBOARD_ORIENTATION_TILT_RIGHT,
    YBOARD_ORIENTATION_UPRIGHT,
};

// Compute the total acceleration magnitude in milliGs (1000 mG ≈ 1g at rest).
float yboard_accel_magnitude();

// Check whether the board matches a target orientation / motion state.
bool yboard_orientation_check(yboard_orientation orientation);

// --- LED effect helpers ---

// Call one of these repeatedly in loop() to animate the LEDs.
// They use FastLED primitives and a shared rotating hue counter.

void yboard_effect_rainbow();
void yboard_effect_confetti();
void yboard_effect_sinelon();
void yboard_effect_juggle();
void yboard_effect_bpm();
void yboard_effect_fire();

#endif // YBOARD_HELPER_H
