// EyeStyles.h -- the expression library.
//
// An expression is DATA, not code. Adding one means adding a row to the table
// in EyeStyles.cpp; nothing in Eyes.cpp needs to change.
//
// The one hard rule this file enforces structurally: EyeShape -- the per-eye
// part -- has no gaze field at all. Gaze lives on EyeStyle, outside L and R, so
// the two eyes cannot be aimed differently even by mistake. Shapes may differ
// left/right as much as you like; the direction they look never can.
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Shape of ONE eye. Every value is a plain number so expressions stay data.
// ---------------------------------------------------------------------------
struct EyeShape {
  float w;         // width  multiplier on EYE_BASE_W
  float h;         // height multiplier on EYE_BASE_H
  float topLid;    // 0 = wide open .. 1 = upper lid fully down
  float botLid;    // 0 = flat .. 1 = lower lid fully up (crescent)
  float lidTilt;   // -1..1  + = inner corner down (angry), - = outer down (sad)
  float disc;      // size of the iris+pupil disc
  float blueFrom;  // 0..1 fraction down the disc where the blue iris starts
  float offsetY;   // px nudge of the whole eye, purely for asymmetry
  float hiBig;     // main highlight scale (0 = none)
  float hiSmall;   // secondary highlight scale (0 = none)
  float hiExtra;   // third sparkle (0 = none)
  // Appended last on purpose: the existing rows list 11 values, so this stays
  // zero-initialised for them and no table row had to be rewritten.
  float offsetX;   // px nudge sideways -- lets the two eyes sit off-axis
  // Lid shaping, all added to a sensible baseline so 0 keeps the old look.
  float topCurve;   // + = upper lid hangs rounder in the middle
  float botCurve;   // + = lower lid arcs up harder in the middle
  float lidTiltBot; // -1..1 slant of the LOWER lid, independent of the upper
  float innerSeal;  // 0..1 pulls the upper lid down onto the lower one at the
                    // inner corner, so the two lines meet in a point there
};

// ---------------------------------------------------------------------------
// A named expression: two shapes plus the SHARED gaze and timing feel.
// ---------------------------------------------------------------------------
struct EyeStyle {
  const char* name;
  EyeShape    L;
  EyeShape    R;
  float   gazeX;         // -1..1  shared bias. Never per-eye.
  float   gazeY;
  float   gazeRoam;      // 0 = hold the bias still, 1 = wander normally
  float   saccadeScale;  // <1 = glances more often, >1 = slower and calmer
  float   blinkScale;    // <1 = blinks more often
  uint16_t holdMs;       // how long the ambient scheduler keeps this up
  // Also appended last, same reason. All three are SHARED, so they move the
  // pair together and cannot desynchronise the gaze.
  float spread;          // px added to the gap between the eyes (- = closer)
  float headX;           // px the pair leans, on top of the gaze follow
  float headY;
  // Cross-eye. ONE number, mirrored: the left pupil goes right by it and the
  // right pupil goes left by the same amount. Being symmetric and derived from
  // a single value is what makes it cross-eyed rather than a squint.
  float converge;        // 0 = parallel, 1 = fully crossed, <0 = unfocused
  // Slow drift of the lids, phase-shifted between the two eyes: the slant
  // rocks one way then the other and each eye opens a little more in turn.
  float wobble;          // 0 = still lids, 1 = plenty of drift
};

extern const EyeStyle EYE_STYLES[];
extern const int      EYE_STYLE_COUNT;

// Every style, in table order. EyeStyles.cpp static_asserts that this list and
// the table are the same length, so adding one to the table without adding it
// here (or vice versa) is a build error rather than a silent mix-up.
enum : uint8_t {
  STYLE_NORMAL = 0,
  STYLE_SURPRISED,
  STYLE_SLEEPY,
  STYLE_HALF,
  STYLE_ALMOST_ASLEEP,
  STYLE_LOOK_LEFT,
  STYLE_LOOK_RIGHT,
  STYLE_LOOK_UP,
  STYLE_LOOK_DOWN,
  STYLE_LOOK_UP_LEFT,
  STYLE_LOOK_DOWN_RIGHT,
  STYLE_DARTING,
  STYLE_HAPPY,
  STYLE_EXCITED,
  STYLE_SPARKLE,
  STYLE_CURIOUS,
  STYLE_RELIEVED,
  STYLE_SMUG,
  STYLE_SHY,
  STYLE_WINK,
  STYLE_ONE_EYE_CLOSED,
  STYLE_PUPIL_BIG,
  STYLE_PUPIL_SMALL,
  STYLE_ANGRY,
  STYLE_SERIOUS,
  STYLE_JITO,
  STYLE_SUSPICIOUS,
  STYLE_TROUBLED,
  STYLE_SAD,
  STYLE_SCARED,
  STYLE_THINKING,
  STYLE_DAZED,
  STYLE_VOID,
  STYLE_PEEK_LEFT,
  STYLE_PEEK_DOWN_RIGHT,
  STYLE_TILTED,
  STYLE_FOCUS,
  STYLE_WIDE_EYED,
  STYLE_GLANCE_BACK,
  STYLE_LOPSIDED,
  STYLE_CROSSEYED,
  STYLE_DIZZY,
  STYLE_LEAF_WINK,
  STYLE_COUNT
};
