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
static const uint16_t COL_SWIRL        = rgb565(  8,   8,  14);  // dizzy spiral
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

// Baseline lid curvature, as a fraction of the eye's half-height. EyeShape's
// topCurve/botCurve are added to these, so a zeroed row keeps the old shape.
static const float LID_TOP_CURVE = 0.10f;
static const float LID_BOT_CURVE = 0.15f;

// After this long with nothing moving it closes its eyes for good and drops
// into deep sleep. Button A wakes it.
static const bool     DEEP_SLEEP_ENABLED  = true;
static const uint32_t DEEP_SLEEP_AFTER_MS = 180000;   // 3 minutes

// A nap on this board is not free: there is no PMIC to switch rails off, so
// deep sleep still costs a few mA and a unit left asleep on battery runs its
// own 200mAh cell flat in a couple of days -- and then keeps pulling it under
// 3.0V, which is the damage nobody comes back from. So the nap is on a clock
// too: after this much sleeping it wakes itself up and cuts power for real
// (GPIO4 low, latch open, nothing drawing from the cell at all).
static const uint32_t DEEP_SLEEP_ESCALATE_MS = 1800000;   // 30 minutes

// ----------------------------------------------------------- over-discharge --
// A LiPo under ~3.0V is damaged for good: the negative electrode's copper
// starts dissolving, and what it plates out on the next charge is an internal
// short. Its protection IC opens somewhere near 2.5V, and once self-discharge
// walks the cell down to nothing from there, chargers refuse to touch it --
// which is exactly how this unit's battery died, reading a flat 284mV.
//
// The cutoff sits above both numbers, with room for the sag the LCD and buzzer
// put on the rail. It is measured in units of "still running", so it can only
// ever be a guess about the cell underneath; the streak below is what keeps a
// momentary dip from being read as an empty battery.
static const bool     BATT_GUARD_ENABLED = true;
static const uint16_t BATT_CUTOFF_MV     = 3300;
static const uint32_t BATT_SAMPLE_MS     = 1000;
static const uint8_t  BATT_LOW_STREAK    = 5;      // consecutive low samples before acting
static const uint32_t BATT_NOTICE_MS     = 2000;   // how long LOW BATT stays up

// Telling the two supplies apart, on a board with no VBUS pin to ask. Above
// 4.2V the node can only be held up by a charger. Below the protection IC's own
// cutoff there is no cell on the pins at all -- and since the sketch is still
// running to read it, USB must be carrying the board, which is the state this
// unit is in right now with its battery open. Both readings mean the guard has
// nothing to protect, and must stay out of the way: powerOff() while USB is
// attached does not switch anything off, it reboots (see the pulse loop in
// M5Unified's Power_Class), so a guard that fired here would be an endless
// restart loop instead of a shutdown.
static const uint16_t BATT_USB_MV    = 4250;
static const uint16_t BATT_ABSENT_MV = 2500;

// The gap those two thresholds leave: a deeply discharged cell that has just
// been plugged in sits below the cutoff and above the protection IC, so it
// reads exactly like an empty battery being run flat. Firing there would cut
// power to a cell that is charging -- and on USB a cut is a reboot, so it would
// do it again every few seconds. What separates the two cases is direction. A
// cell under load falls; a cell on a charger climbs, and a 200mAh pack taking
// even a modest charge current moves further than this in a second.
static const uint16_t BATT_RISE_MV = 15;

// How many times the apple story plays before the face takes over. Two is for
// filming it: one pass is over before a camera is pointed at the right thing.
// begin() resets every field the animation owns, so a replay is just calling it
// again -- there is no state left over from the pass before.
static const uint8_t BOOT_LOOPS = 2;

// Idle time before the eyes start to look sleepy.
static const uint32_t EYE_SLEEPY_AFTER_MS = 25000;

// ------------------------------------------------------------------- game ---
// Tilt-controlled block breaker. The character is the ball.
//
// The game runs PORTRAIT (135 x 240) while the pet screens stay landscape:
// a tall board gives the ball a long run at the paddle, which is what makes
// the fore/aft speed control worth having. The .ino flips the display rotation
// and rebuilds the canvas on the way in and out.
//
// Two independent tilt axes, held flat like a tray:
//   roll  (left/right) -> paddle, as an *acceleration*, so the board carries
//                         momentum instead of snapping to the tilt angle
//   pitch (fore/aft)   -> a time scale applied to the ball and its effects only.
//                         Nothing about the physics changes; dt does.

// --- screen ----------------------------------------------------------------
static const int   GAME_ROTATION     = 0;    // portrait
static const int   GAME_W            = 135;
static const int   GAME_H            = 240;

// --- paddle ---------------------------------------------------------------
static const float GAME_PAD_Y        = 216.0f;  // top edge of the paddle
static const int   GAME_PAD_H        = 5;
static const float GAME_PAD_W_BASE   = 34.0f;   // level 1 width
static const float GAME_PAD_W_MIN    = 20.0f;   // never narrower than this
static const float GAME_PAD_W_STEP   = 2.0f;    // shed per level

// Tilt -> paddle. DEADZONE kills sensor noise when held flat; EXPO > 1 flattens
// the response near centre, which is what makes small corrections possible
// without giving up top speed at full tilt.
//
// ACCEL and DRAG are a pair: terminal velocity is ACCEL / DRAG, so raising one
// without the other just changes how long it takes to get to the same speed.
// Both are scaled for a 135 px wide board -- roughly half the landscape values.
static const float GAME_TILT_GAIN    = 2.6f;    // g -> -1..1
static const float GAME_TILT_DEAD    = 0.07f;
static const float GAME_TILT_EXPO    = 2.0f;
static const float GAME_TILT_LOWPASS = 0.35f;   // per-frame smoothing of the raw tilt
static const float GAME_PAD_ACCEL    = 850.0f;  // px/s^2 at full tilt
static const float GAME_PAD_VMAX     = 195.0f;  // px/s
static const float GAME_PAD_DRAG     = 4.5f;    // 1/s -- how fast it coasts to a stop
static const float GAME_PAD_WALL_BOUNCE = 0.25f; // rebound off the screen edge

// --- fore/aft tilt -> game speed ------------------------------------------
static const float GAME_PITCH_GAIN   = 2.2f;
static const float GAME_PITCH_DEAD   = 0.10f;
static const float GAME_PITCH_LOWPASS = 0.12f;  // slower: speed should drift, not jump
static const float GAME_TIME_MIN     = 0.35f;   // tilted towards you: slow motion
static const float GAME_TIME_MAX     = 2.10f;   // tilted away: fast
// Flip either of these if your unit reads the opposite way round. Note the
// axes are swapped relative to the landscape screens: portrait screen-X is the
// device axis that was screen-Y in landscape.
static const float GAME_ROLL_SIGN    = -1.0f;
static const float GAME_PITCH_SIGN   = -1.0f;

// --- ball ------------------------------------------------------------------
static const float GAME_BALL_R       = 10.0f;
static const float GAME_BALL_SPEED0  = 108.0f;  // px/s at level 1
static const float GAME_BALL_SPEED_PER_LEVEL = 13.0f;
static const float GAME_BALL_SPEED_MAX = 230.0f;
static const float GAME_BALL_SPEED_PER_HIT  = 0.9f;  // creeps up within a level
static const float GAME_BOUNCE_MAX_ANGLE = 1.05f;    // rad from vertical (~60 deg)
static const float GAME_BOUNCE_MIN_VY    = 0.34f;    // fraction of speed kept vertical
static const float GAME_PAD_SPIN     = 0.28f;   // how much paddle motion is added to vx
static const float GAME_SUBSTEP_PX   = 3.5f;    // max travel per collision substep
static const float GAME_SQUASH_MAX   = 0.34f;
static const float GAME_SQUASH_DECAY = 7.0f;    // 1/s

// --- blocks ----------------------------------------------------------------
static const int   GAME_COLS         = 5;
static const int   GAME_ROWS         = 6;
static const int   GAME_BLOCK_W      = 23;
static const int   GAME_BLOCK_H      = 9;
static const int   GAME_BLOCK_GAP    = 2;
static const int   GAME_FIELD_X      = 6;    // 5*23 + 4*2 = 123, centred on 135
static const int   GAME_FIELD_Y      = 24;   // clear of the two-line HUD

// --- trail -----------------------------------------------------------------
static const int   GAME_TRAIL_POINTS = 14;   // ribbon length, in past positions
static const int   GAME_PARTICLES    = 44;
static const uint16_t COL_TRAIL_AQUA = rgb565(150, 232, 255);
static const uint16_t COL_TRAIL_PINK = rgb565(255, 176, 214);

// --- rules -----------------------------------------------------------------
static const int   GAME_LIVES        = 3;

// ------------------------------------------------------------------ suika ---
// Watermelon game. Shares the portrait 135x240 canvas with the block breaker.
//
// The sizes below are the real game's, not a clone's. Two independent sources
// agree: kairess/suika-game lists the eleven fruit diameters as 33, 48, 61, 69,
// 89, 114, 129, 156, 177, 220, 259, which are plainly pixel measurements off the
// original; measuring a screenshot of the real game gave the same ratios within
// 3% (grape 0.235 vs 0.236 of the watermelon, pear 0.479 vs 0.498, peach 0.588
// vs 0.602, pineapple 0.667 vs 0.683, melon 0.821 vs 0.849). The widely-linked
// moonfloof clone does *not* agree -- its peach is 17% small and its apple 15%
// -- so its radii are not used here, only its friction numbers.
//
// The box comes from the same screenshot: 400 px inner width holding a 234 px
// watermelon, so the watermelon is 0.585 of the box, and the box is 1 : 1.25.
//
// Keeping the box the full 135 px matters. Narrow it to make room for drawn
// walls and grape/dekopon land 1 px apart, which is not a difference anyone can
// see on this panel; at 135 every neighbouring pair differs by at least 2 px.
// The walls are therefore drawn *over* the play area as thin lines.

static const int   SUIKA_BOX_X      = 0;
static const int   SUIKA_BOX_W      = 135;
static const int   SUIKA_BOX_Y      = 40;    // drop zone above, HUD below
static const int   SUIKA_BOX_H      = 169;   // 135 * 1.25
static const int   SUIKA_LINE_Y     = 46;    // lose line, 6 px into the box
static const int   SUIKA_PREVIEW_Y  = 20;    // where the waiting fruit hovers
static const int   SUIKA_WALL_PX    = 2;     // wall line, drawn over the field

static const int   SUIKA_TIERS      = 11;
static const int   SUIKA_DROP_TIERS = 5;     // only the first five ever drop
static const int   SUIKA_MAX_FRUITS = 128;
static const int   SUIKA_FACE_FROM  = 3;     // dekopon up; below this a face is mush

// Radii: kairess' diameters scaled so the watermelon is 0.585 of a 135 px box.
static const float SUIKA_RADIUS[SUIKA_TIERS] = {
   5.03f,  7.32f,  9.30f, 10.52f, 13.57f, 17.38f,
  19.67f, 23.79f, 26.99f, 33.54f, 39.49f,
};

// Points awarded when two fruits of tier i merge -- i.e. the score of the fruit
// that comes out. Two watermelons make nothing, so they are worth nothing; the
// real game's published table simply stops at the watermelon's 55.
static const int   SUIKA_SCORE[SUIKA_TIERS] = {
  1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 0,
};

// --- solver ----------------------------------------------------------------
// Gravity is the reference clone's ~1000 px/s^2 scaled by 135/640, because the
// game should keep its tempo when the screen shrinks: hold time constant and
// gravity scales with length. A fruit crosses the box in ~1.27 s either way.
//
// That also settles the substep count. Terminal speed here is
// sqrt(2 * 210 * 169) = 267 px/s, which is 2.2 px per 1/120 s step -- well under
// the 5 px cherry, so nothing can tunnel and a fixed step count is enough. No
// distance-driven substepping like the block breaker needs.
static const float SUIKA_GRAVITY    = 210.0f;  // px/s^2
static const float SUIKA_DAMPING    = 0.998f;  // per substep; the real game has no air drag
static const float SUIKA_WALL_REST  = 0.10f;   // fruit-to-fruit restitution stays 0
static const int   SUIKA_SUBSTEPS   = 2;       // 60 Hz physics under a 30 fps draw
static const int   SUIKA_ITERATIONS = 4;       // relaxation passes per substep
static const float SUIKA_SLOP       = 0.60f;   // contact tolerance for merges
static const float SUIKA_MAX_SPEED  = 420.0f;  // px/s safety clamp

// How far one fruit may be separated per substep, summed over the relaxation
// passes. In Verlet the velocity *is* the change in position, so any separation
// the solver performs also becomes speed -- which is exactly what makes a pile
// settle, right up until something has to be moved a long way at once.
//
// A merge is that something. Two fruits are replaced by a bigger one at their
// midpoint, so whatever was resting against them is suddenly several pixels
// inside it. Uncapped, that one frame hands out 400 px/s and fires the pile out
// of the box. Capped at 2.5 px per substep -- 5 px a frame, still more than the
// 4.5 px a fruit can fall in one -- an ordinary landing is resolved in a single
// frame while a merge eases apart over two, which is the pop the game wants.
static const float SUIKA_MAX_PUSH   = 2.50f;   // px per fruit per substep

// ...and of that separation, this much may turn into speed. The rest is applied
// to the previous position as well, which moves the fruit without telling it
// that it moved. Separating a deep overlap is not a collision and should not
// read as one: at 2.5 px of silent recovery a merge still threw its neighbours
// hard enough to clear the lose line, which is a game over the player did not
// earn. 1.2 px per substep is 72 px/s -- a 12 px hop, a pop rather than a
// launch -- while still being twenty times what gravity adds in a step, so a
// resting pile is pinned just as firmly as before.
static const float SUIKA_SOFT_PUSH  = 1.20f;   // px per fruit per substep

// A ceiling on rising, and only on rising. A fruit wedged in a pile is pushed
// again every substep, and each push is allowed its own slice of speed, so over
// half a dozen substeps a trapped fruit winds itself up and then squirts out of
// the gap at the general speed limit. Falling legitimately reaches 282 px/s, so
// no symmetric clamp can tell the two apart -- but nothing in this game ever
// needs to travel *up* faster than a merge pops it. 120 px/s tops out 34 px
// above where it started, which reads as a pop and stays inside the box.
static const float SUIKA_MAX_RISE   = 120.0f;  // px/s
static const float SUIKA_SETTLE_V   = 14.0f;   // px/s under which a fruit counts as resting

// Rotation is decorative: a position solver has no torque, so spin is taken
// from the tangential slip at each contact and bled away every frame.
static const float SUIKA_SPIN_GAIN  = 0.55f;
static const float SUIKA_SPIN_DAMP  = 0.92f;

// --- aiming ----------------------------------------------------------------
// Tilt drives the waiting fruit's *speed*, not the paddle-style acceleration
// the block breaker uses. Placing a fruit is a positioning job, and momentum
// gets in the way of lining one up against a wall.
static const float SUIKA_AIM_SPEED  = 130.0f;  // px/s at full tilt

// --- rules -----------------------------------------------------------------
static const uint32_t SUIKA_OVER_MS       = 2000;  // over the line this long = out
static const uint32_t SUIKA_DROP_WAIT_MS  = 1500;  // fallback if a drop never touches down

// --- palette ---------------------------------------------------------------
// Pastels, and a warm brown-grey for the ink rather than black: the panel is
// small enough that saturated colours next to each other read as noise. The
// field stays a shade deeper than the page so the box has an edge even where a
// wall is not drawn, and the dots are barely off the field on purpose -- a
// pattern that competes with the fruits stops being background.
static const uint16_t COL_SUIKA_BG    = rgb565(255, 244, 238);  // pink cream, whole screen
// The field is black and the page around it stays cream. Pastels need something
// dark to sit on -- against pink they were all within a step or two of the
// background, and the canvas is 8bpp (3-3-2), so a near-miss in RGB565 lands on
// the same quantised colour as the field itself. The dots follow it down: a
// pale pattern that was texture on pink turns into confetti on black.
static const uint16_t COL_SUIKA_FIELD = rgb565(  0,   0,   0);  // inside the box
static const uint16_t COL_SUIKA_DOT   = rgb565( 48,  44,  40);  // polka dots on the field
static const uint16_t COL_SUIKA_WALL  = rgb565(246, 190, 176);  // soft peach
static const uint16_t COL_SUIKA_INK   = rgb565(124,  92,  84);  // faces and text
static const uint16_t COL_SUIKA_LINE  = rgb565(244, 148, 148);  // lose line, when shown
static const uint16_t COL_SUIKA_LEAF  = rgb565(142, 202, 130);
static const uint16_t COL_SUIKA_BLUSH = rgb565(255, 168, 178);  // cheeks
static const uint16_t COL_SUIKA_HEART = rgb565(248, 128, 150);  // HUD hearts
static const uint16_t COL_SUIKA_SPARK = rgb565(255, 252, 244);  // merge sparkle

// --- the cute bits ---------------------------------------------------------
// How long a fruit keeps its merge face. Long enough to notice at 30 fps, short
// enough that a busy pile is not all grinning at once.
static const uint32_t SUIKA_GLEE_MS   = 420;

// Sparkle burst on a merge. The cap is shared by the whole board rather than
// per fruit, so a chain reaction spends the same budget as a single merge and
// the frame cost has a ceiling.
static const int   SUIKA_MAX_SPARKS   = 12;
static const int   SUIKA_SPARKS_PER   = 5;
static const float SUIKA_SPARK_LIFE   = 0.34f;   // seconds
static const float SUIKA_SPARK_SPEED  = 58.0f;   // px/s, outward
static const float SUIKA_SPARK_GRAV   = 90.0f;   // px/s^2, so they arc instead of sliding

// Corner radius of the box, and the spacing of the dots inside it.
static const int   SUIKA_FIELD_R      = 8;
static const int   SUIKA_DOT_STEP     = 20;

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

// Motion, as opposed to tilt: the frame-to-frame change in acceleration. Tilt
// says which way up the unit is; this says whether anything just happened.
static const float    IMU_MOTION_WAKE    = 0.05f;  // g -- any handling counts
static const float    IMU_MOTION_STARTLE = 0.55f;  // g -- a distinct jolt
static const uint32_t IMU_STARTLE_COOLDOWN_MS = 1500;

// Spin. The gyro reports deg/s; anything above SPIN_MIN_DPS piles up in an
// accumulator that bleeds away at SPIN_DECAY_DPS, so a few brisk twirls trip it
// but ordinary handling never does.
static const float    SPIN_MIN_DPS   = 100.0f;
static const float    SPIN_DECAY_DPS =  90.0f;
static const float    SPIN_TRIGGER   = 320.0f;
static const float    SPIN_MAX       = 1800.0f;
static const uint32_t DIZZY_MS       = 3500;
static const float    SWIRL_SPEED    = 3.4f;    // rad/s the spiral turns
static const int      SWIRL_THICK    = 2;       // brush radius; 2 = a 5px stroke
