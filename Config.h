// Config.h -- tuning knobs for the whole sketch.
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- display --
static const int     SCREEN_W          = 240;
static const int     SCREEN_H          = 135;
static const int     SCREEN_ROTATION   = 1;    // landscape
static const uint8_t SCREEN_BRIGHTNESS = 110;

// ------------------------------------------------------------ frame pacing --
static const uint32_t TARGET_FPS         = 30;
static const uint32_t FRAME_INTERVAL_MS  = 1000 / TARGET_FPS;

// ----------------------------------------------------------------- palette --
static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static const uint16_t COL_BG           = 0x0000;
static const uint16_t COL_WHITE        = rgb565(255, 255, 255);
static const uint16_t COL_APPLE        = rgb565(230, 230, 232);
static const uint16_t COL_APPLE_HALO   = rgb565(120, 120, 130);

static const uint16_t COL_CHAR_BODY    = rgb565(255, 166, 196);  // kirby pink
static const uint16_t COL_CHAR_SHADE   = rgb565(226, 118, 160);
static const uint16_t COL_CHAR_LIGHT   = rgb565(255, 216, 230);
static const uint16_t COL_CHAR_FOOT    = rgb565(198,  32,  74);  // red feet
static const uint16_t COL_CHEEK        = rgb565(250, 118, 150);
static const uint16_t COL_EYE_NAVY     = rgb565( 18,  24,  70);
static const uint16_t COL_EYE_BLUE     = rgb565( 58, 150, 236);
static const uint16_t COL_MOUTH        = rgb565(196,  40,  74);
static const uint16_t COL_MOUTH_DK     = rgb565(104,  14,  40);

// ------------------------------------------------------- boot scene layout --
// Top-left corner of the apple bitmap on screen. The bite circle baked into
// AppleBitmap.h is relative to this origin.
static const int APPLE_ORIGIN_X   = 68;
static const int APPLE_ORIGIN_Y   = 24;

static const float CHAR_X_START    = 280.0f;  // off-screen right
static const float CHAR_X_APPROACH = 186.0f;
static const float CHAR_X_BITE     = 158.0f;
static const float CHAR_X_EXIT     = 292.0f;
static const float CHAR_GROUND_Y   =  94.0f;  // body centre while standing
static const float CHAR_BITE_Y     =  66.0f;  // body centre while lunging up
static const float CHAR_RADIUS     =  23.0f;

// ------------------------------------------------------------- eyes screen --
static const int EYE_BASE_W   =  84;
static const int EYE_BASE_H   = 104;
static const int EYE_L_CX     =  60;
static const int EYE_R_CX     = 180;
static const int EYE_CY       =  68;
static const int EYE_PUPIL_W  =  40;
static const int EYE_PUPIL_H  =  62;

// Idle time before the eyes start to look sleepy.
static const uint32_t EYE_SLEEPY_AFTER_MS = 25000;

// ----------------------------------------------------------------- imu ------
// Accelerometer -> gaze mapping. If the eyes look the wrong way on your unit,
// flip these signs; if the axes feel swapped, set IMU_SWAP_AXES to true.
static const bool  IMU_SWAP_AXES    = false;
static const float IMU_SIGN_X       = -1.0f;
static const float IMU_SIGN_Y       = -1.0f;
static const float IMU_GAIN         =  2.4f;   // g -> normalised gaze
static const float IMU_LOWPASS      =  0.10f;  // per-frame smoothing factor
static const float IMU_TAKEOVER     =  0.40f;  // tilt magnitude that grabs the gaze
static const float IMU_RELEASE      =  0.18f;  // tilt magnitude that gives it back
static const uint32_t IMU_RELEASE_HOLD_MS = 1200;
