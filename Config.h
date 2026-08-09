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

static const uint16_t COL_CHAR_BODY    = rgb565(255, 166, 196);  // kirby pink
static const uint16_t COL_CHAR_SHADE   = rgb565(240, 146, 180);
static const uint16_t COL_CHAR_LIGHT   = rgb565(255, 198, 220);
static const uint16_t COL_CHAR_FOOT    = rgb565(214,  38,  62);  // red feet
static const uint16_t COL_CHAR_FOOT_DK = rgb565(150,  22,  48);  // far foot
static const uint16_t COL_CHEEK        = rgb565(242, 140, 168);
static const uint16_t COL_EYE_NAVY     = rgb565( 18,  24,  70);
static const uint16_t COL_EYE_BLUE     = rgb565( 58, 150, 236);
static const uint16_t COL_MOUTH        = rgb565(196,  40,  74);
static const uint16_t COL_MOUTH_IN     = rgb565(232, 146,  92);  // throat/tongue
static const uint16_t COL_SUCTION      = rgb565(150, 220, 255);  // inhale streaks
static const uint16_t COL_STAR         = rgb565(255, 214,  64);
static const uint16_t COL_MOUTH_DK     = rgb565(104,  14,  40);

// ------------------------------------------------------- boot scene layout --
// Top-left corner of the apple bitmap on screen. The bite circle baked into
// AppleBitmap.h is relative to this origin.
static const int APPLE_ORIGIN_X   = 82;   // centres the 76px bitmap on 240
static const int APPLE_ORIGIN_Y   = 21;   // centres the 92px bitmap on 135

// The character eats in profile, facing left. Its mouth sits on the leading
// edge of its face, so these X values are chosen so that the mouth lands on
// the bite circle -- screen (152, 73) -- and not the middle of the apple.
static const float CHAR_X_START    = 294.0f;  // off-screen right
static const float CHAR_X_APPROACH = 210.0f;
static const float CHAR_X_MOUTH    = 192.0f;  // poised, mouth wide open
static const float CHAR_X_BITE     = 180.0f;  // lunged in, mouth on the bite
static const float CHAR_X_CHEW     = 206.0f;  // pulled back so the notch shows
static const float CHAR_X_INHALE   = 212.0f;  // rears back to inhale
static const float CHAR_Y_INHALE   =  78.0f;
static const float CHAR_X_EXIT     = 306.0f;
static const float CHAR_GROUND_Y   =  92.0f;  // body centre while standing
static const float CHAR_BITE_Y     =  64.0f;  // body centre while lunging up
static const float CHAR_RADIUS     =  29.0f;

// ----------------------------------------------------------------- stars ---
static const int  STAR_COUNT    = 7;
static const bool STARS_ON_EYES = false;  // boot scene only; plain black face

// ------------------------------------------------------------- eyes screen --
static const int EYE_BASE_W   =  84;
static const int EYE_BASE_H   = 104;
static const int EYE_L_CX     =  60;
static const int EYE_R_CX     = 180;
static const int EYE_CY       =  68;
static const int EYE_PUPIL_W  =  44;
static const int EYE_PUPIL_H  =  66;
static const int EYE_PUPIL_MARGIN = 2;   // px of sclera kept around the pupil

// Power button: how long a hold takes to close the eyes completely.
static const uint32_t POWER_SLEEP_MS = 2500;

// How far the pair translates as a unit. The eyes lead with the pupils and the
// "head" follows a beat later, which is what makes a glance read as a turn.
static const float EYE_HEAD_FOLLOW_X = 22.0f;   // px at full left/right gaze
static const float EYE_HEAD_FOLLOW_Y = 12.0f;
static const float EYE_HEAD_MAX_X    = 26.0f;   // hard cap; beyond this it peeks off-screen
static const float EYE_HEAD_MAX_Y    = 14.0f;
static const float EYE_HEAD_LAG      = 0.055f;  // smaller = more lag behind the gaze
static const float EYE_IMU_SLIDE_X   = 15.0f;   // extra slide when the unit is tilted
static const float EYE_IMU_SLIDE_Y   = 10.0f;
static const float EYE_SIDE_SQUASH   = 0.09f;   // eyes narrow when looking sideways

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
static const float IMU_TAKEOVER     =  0.30f;  // tilt magnitude that grabs the gaze
static const float IMU_RELEASE      =  0.18f;  // tilt magnitude that gives it back
static const uint32_t IMU_RELEASE_HOLD_MS = 1200;
