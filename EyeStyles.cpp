#include "EyeStyles.h"

// Field order, for reading the table below:
//   w, h, topLid, botLid, lidTilt, disc, blueFrom, offsetY, hiBig, hiSmall,
//   hiExtra, offsetX, topCurve, botCurve, lidTiltBot, innerSeal
//
// Every row is deliberately a little asymmetric: the right eye is usually a
// touch wider and shorter, sits a pixel lower, and carries a slightly smaller
// highlight. It is never enough to notice on its own, but a perfectly mirrored
// face reads as a printed graphic rather than something alive.
//
// Trailing fields on EyeStyle: gazeX, gazeY, gazeRoam, saccadeScale,
// blinkScale, holdMs.

const EyeStyle EYE_STYLES[] = {

// -- the five the code refers to by name, in enum order ---------------------
{ "NORMAL",
  { 1.00f, 1.00f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.97f, 0.04f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  0.0f, 0.0f, 1.00f, 1.00f, 1.00f, 4000 },

{ "SURPRISED",
  { 1.20f, 1.28f, 0.00f, 0.00f, -0.10f, 0.72f, 0.58f, -1.0f, 1.15f, 1.10f, 0.0f },
  { 1.24f, 1.23f, 0.00f, 0.00f, -0.06f, 0.70f, 0.60f,  0.0f, 1.05f, 1.15f, 0.0f },
  0.0f, -0.10f, 0.25f, 1.60f, 1.80f, 500 },

// The half-lidded family fills the panel: the eye stays big and the lid does
// the closing, rather than shrinking the whole eye. Both lids are curved, the
// two upper lids slant OPPOSITE ways, and wobble keeps them drifting so one
// eye is always a little more open than the other.
// Trailing EyeStyle fields here: spread, headX, headY, converge, wobble.
{ "SLEEPY",                                   // the long-idle resting face:
  // a tall ellipse with a nearly flat top cut across it, plus slow blinks
  { 0.98f, 1.24f, 0.54f, 0.00f,  0.12f, 0.94f, 0.58f,  1.0f, 0.80f, 0.60f, 0.0f, 0.0f, -0.07f, 0.0f, 0.0f, 0.0f },
  { 0.95f, 1.28f, 0.58f, 0.00f,  0.08f, 0.92f, 0.60f,  2.0f, 0.70f, 0.55f, 0.0f, 0.0f, -0.07f, 0.0f, 0.0f, 0.0f },
  0.0f, 0.24f, 0.30f, 2.60f, 0.45f, 6000,  0.0f, 0.0f, 0.0f, 0.0f, 0.30f },

{ "HALF",                                     // half-moon: the round bottom of
  // the sclera left alone, one gentle arc taken off the top. Expression comes
  // from the outer corner rising and falling, not from the lid shape.
  { 1.16f, 1.22f, 0.50f, 0.00f,  0.26f, 1.00f, 0.58f,  1.0f, 0.95f, 0.85f, 0.0f, 0.0f,  0.06f, 0.0f, 0.0f, 0.0f },
  { 1.12f, 1.26f, 0.56f, 0.00f,  0.18f, 0.98f, 0.60f,  2.0f, 0.85f, 0.90f, 0.0f, 0.0f,  0.10f, 0.0f, 0.0f, 0.0f },
  0.0f, 0.05f, 0.60f, 1.60f, 1.00f, 2600,  0.0f, 0.0f, 0.0f, 0.0f, 1.00f },

{ "ALMOST_ASLEEP",
  { 1.08f, 1.16f, 0.70f, 0.12f,  0.30f, 0.90f, 0.58f,  2.0f, 0.70f, 0.00f, 0.0f, 0.0f,  0.24f,  0.12f, -0.26f },
  { 1.05f, 1.22f, 0.76f, 0.09f, -0.20f, 0.88f, 0.60f,  3.0f, 0.60f, 0.00f, 0.0f, 0.0f,  0.30f,  0.04f,  0.20f },
  0.0f, 0.26f, 0.35f, 3.50f, 0.45f, 8000,  0.0f, 0.0f, 0.0f, 0.0f, 0.40f },

// -- fixed gaze directions (gazeRoam = 0 holds them still) -------------------
{ "LOOK_LEFT",
  { 1.00f, 1.00f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.97f, 0.04f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  -1.0f, 0.0f, 0.0f, 1.00f, 1.00f, 1600 },

{ "LOOK_RIGHT",
  { 1.00f, 1.00f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.97f, 0.04f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  1.0f, 0.0f, 0.0f, 1.00f, 1.00f, 1600 },

{ "LOOK_UP",
  { 1.00f, 1.02f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.99f, 0.03f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  0.0f, -1.0f, 0.0f, 1.00f, 1.00f, 1600 },

{ "LOOK_DOWN",
  { 1.00f, 0.98f, 0.10f, 0.00f,  0.05f, 1.00f, 0.64f,  0.0f, 0.95f, 1.00f, 0.0f },
  { 1.03f, 0.95f, 0.14f, 0.00f,  0.03f, 0.97f, 0.66f,  1.0f, 0.85f, 1.05f, 0.0f },
  0.0f, 1.0f, 0.0f, 1.00f, 1.00f, 1600 },

{ "LOOK_UP_LEFT",
  { 1.00f, 1.02f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.99f, 0.03f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  -0.82f, -0.82f, 0.0f, 1.00f, 1.00f, 1600 },

{ "LOOK_DOWN_RIGHT",
  { 1.00f, 0.98f, 0.08f, 0.00f,  0.04f, 1.00f, 0.64f,  0.0f, 0.95f, 1.00f, 0.0f },
  { 1.03f, 0.95f, 0.12f, 0.00f,  0.02f, 0.97f, 0.66f,  1.0f, 0.85f, 1.05f, 0.0f },
  0.82f, 0.82f, 0.0f, 1.00f, 1.00f, 1600 },

{ "DARTING",                                  // keeps flicking about
  { 1.00f, 1.00f, 0.00f, 0.00f,  0.00f, 0.96f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.97f, 0.05f, 0.00f, -0.03f, 0.93f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  0.0f, 0.0f, 1.00f, 0.32f, 0.80f, 2600 },

// -- bright and positive -----------------------------------------------------
{ "HAPPY",
  { 1.02f, 0.99f, 0.00f, 0.44f,  0.00f, 0.90f, 0.62f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.05f, 0.96f, 0.00f, 0.39f,  0.00f, 0.88f, 0.64f,  1.0f, 0.90f, 1.05f, 0.0f },
  0.0f, -0.05f, 0.55f, 1.30f, 1.10f, 2200 },

{ "EXCITED",
  { 1.12f, 1.16f, 0.00f, 0.10f, -0.05f, 1.20f, 0.56f, -1.0f, 1.30f, 1.15f, 0.70f },
  { 1.15f, 1.12f, 0.00f, 0.06f, -0.02f, 1.17f, 0.58f,  0.0f, 1.20f, 1.20f, 0.60f },
  0.0f, -0.15f, 0.80f, 0.50f, 1.00f, 1800 },

{ "SPARKLE",                                  // extra highlights
  { 1.05f, 1.06f, 0.00f, 0.06f,  0.00f, 1.08f, 0.54f,  0.0f, 1.28f, 1.22f, 0.80f },
  { 1.08f, 1.03f, 0.00f, 0.04f,  0.00f, 1.05f, 0.56f,  1.0f, 1.18f, 1.28f, 0.72f },
  0.0f, -0.10f, 0.45f, 1.40f, 1.20f, 2000 },

{ "CURIOUS",
  { 1.08f, 1.10f, 0.00f, 0.00f, -0.08f, 1.14f, 0.60f, -1.0f, 1.10f, 1.05f, 0.0f },
  { 1.10f, 1.06f, 0.06f, 0.00f, -0.02f, 1.10f, 0.62f,  1.0f, 1.00f, 1.10f, 0.0f },
  0.0f, -0.20f, 0.75f, 0.80f, 1.10f, 1800 },

{ "RELIEVED",
  { 1.00f, 0.94f, 0.22f, 0.44f,  0.06f, 0.82f, 0.62f,  0.0f, 0.90f, 0.80f, 0.0f },
  { 1.03f, 0.91f, 0.26f, 0.40f,  0.08f, 0.80f, 0.64f,  1.0f, 0.80f, 0.85f, 0.0f },
  0.0f, 0.10f, 0.35f, 1.80f, 0.85f, 2200 },

{ "SMUG",                                     // one lid lower than the other
  { 1.00f, 0.95f, 0.28f, 0.26f,  0.34f, 0.85f, 0.62f,  0.0f, 0.95f, 0.90f, 0.0f },
  { 1.04f, 0.90f, 0.40f, 0.22f,  0.40f, 0.82f, 0.64f,  2.0f, 0.85f, 0.95f, 0.0f },
  0.28f, -0.22f, 0.20f, 1.90f, 1.00f, 2000 },

{ "SHY",
  { 1.00f, 0.96f, 0.18f, 0.34f,  0.10f, 0.95f, 0.60f,  0.0f, 1.05f, 1.00f, 0.0f },
  { 1.03f, 0.92f, 0.24f, 0.30f,  0.14f, 0.92f, 0.62f,  2.0f, 0.95f, 1.05f, 0.0f },
  -0.45f, 0.35f, 0.25f, 1.70f, 0.90f, 2200 },

// -- winks -------------------------------------------------------------------
{ "WINK",                                     // left shut, right smiling
  { 1.00f, 1.00f, 0.76f, 0.26f,  0.06f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.05f, 0.98f, 0.00f, 0.30f, -0.04f, 0.95f, 0.62f,  1.0f, 1.10f, 1.05f, 0.0f },
  0.0f, -0.05f, 0.30f, 1.50f, 3.00f, 1400 },

{ "ONE_EYE_CLOSED",                           // right shut, left plain open
  { 1.01f, 1.00f, 0.00f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f },
  { 1.03f, 0.97f, 0.76f, 0.26f, -0.04f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f },
  0.0f, 0.0f, 0.40f, 1.40f, 3.00f, 1600 },

// -- pupil size --------------------------------------------------------------
{ "PUPIL_BIG",
  { 1.03f, 1.04f, 0.00f, 0.00f, -0.04f, 1.36f, 0.58f,  0.0f, 1.15f, 1.10f, 0.0f },
  { 1.06f, 1.01f, 0.03f, 0.00f, -0.02f, 1.33f, 0.60f,  1.0f, 1.05f, 1.15f, 0.0f },
  0.0f, -0.05f, 0.50f, 1.20f, 1.10f, 1800 },

{ "PUPIL_SMALL",
  { 1.02f, 1.00f, 0.00f, 0.00f,  0.00f, 0.54f, 0.70f,  0.0f, 0.80f, 0.70f, 0.0f },
  { 1.05f, 0.97f, 0.03f, 0.00f,  0.00f, 0.52f, 0.72f,  1.0f, 0.70f, 0.75f, 0.0f },
  0.0f, 0.0f, 0.30f, 1.60f, 1.30f, 1600 },

// -- sharp and negative ------------------------------------------------------
{ "ANGRY",
  { 1.02f, 0.92f, 0.30f, 0.10f,  0.85f, 0.86f, 0.68f,  0.0f, 0.85f, 0.70f, 0.0f },
  { 1.05f, 0.89f, 0.35f, 0.08f,  0.78f, 0.84f, 0.70f,  1.0f, 0.75f, 0.75f, 0.0f },
  0.0f, 0.08f, 0.30f, 1.10f, 1.40f, 2000 },

{ "SERIOUS",                                  // "本気" -- narrowed and locked on
  { 1.06f, 0.90f, 0.22f, 0.14f,  0.55f, 0.70f, 0.74f,  0.0f, 0.80f, 0.60f, 0.0f },
  { 1.08f, 0.88f, 0.26f, 0.12f,  0.50f, 0.68f, 0.76f,  1.0f, 0.70f, 0.65f, 0.0f },
  0.0f, 0.0f, 0.10f, 2.40f, 1.60f, 2400 },

{ "JITO",                                     // flat, unimpressed slit
  { 1.02f, 0.96f, 0.52f, 0.22f,  0.10f, 0.96f, 0.66f,  0.0f, 0.75f, 0.60f, 0.0f },
  { 1.05f, 0.93f, 0.56f, 0.20f,  0.12f, 0.94f, 0.68f,  1.0f, 0.65f, 0.65f, 0.0f },
  -0.38f, 0.08f, 0.12f, 2.60f, 1.50f, 2600 },

{ "SUSPICIOUS",                               // strongly uneven lids
  { 1.01f, 0.95f, 0.48f, 0.16f,  0.28f, 0.98f, 0.64f,  0.0f, 0.85f, 0.70f, 0.0f },
  { 1.05f, 1.00f, 0.16f, 0.10f,  0.20f, 1.00f, 0.66f,  1.0f, 0.95f, 0.80f, 0.0f },
  0.52f, -0.05f, 0.18f, 2.20f, 1.40f, 2400 },

{ "TROUBLED",
  { 1.00f, 0.96f, 0.26f, 0.16f, -0.72f, 0.96f, 0.62f,  0.0f, 1.00f, 0.85f, 0.0f },
  { 1.03f, 0.93f, 0.30f, 0.14f, -0.66f, 0.94f, 0.64f,  1.0f, 0.90f, 0.90f, 0.0f },
  -0.18f, 0.26f, 0.35f, 1.50f, 0.90f, 2400 },

{ "SAD",
  { 1.00f, 0.98f, 0.32f, 0.12f, -0.88f, 1.06f, 0.58f,  0.0f, 1.20f, 1.00f, 0.0f },
  { 1.03f, 0.95f, 0.36f, 0.10f, -0.82f, 1.04f, 0.60f,  1.0f, 1.10f, 1.05f, 0.0f },
  0.0f, 0.45f, 0.25f, 2.00f, 0.80f, 2600 },

{ "SCARED",
  { 1.16f, 1.20f, 0.00f, 0.06f, -0.20f, 0.56f, 0.66f, -1.0f, 0.90f, 1.20f, 0.0f },
  { 1.19f, 1.16f, 0.04f, 0.04f, -0.14f, 0.54f, 0.68f,  1.0f, 0.80f, 1.25f, 0.0f },
  0.0f, -0.12f, 0.60f, 0.45f, 1.60f, 1400 },

// -- vacant ------------------------------------------------------------------
{ "THINKING",
  { 1.00f, 1.00f, 0.20f, 0.06f, -0.12f, 0.94f, 0.62f,  0.0f, 0.95f, 0.85f, 0.0f },
  { 1.04f, 0.96f, 0.12f, 0.04f, -0.08f, 0.92f, 0.64f,  1.0f, 0.85f, 0.90f, 0.0f },
  -0.62f, -0.68f, 0.14f, 2.60f, 1.30f, 2600 },

{ "DAZED",
  { 1.00f, 0.97f, 0.30f, 0.08f,  0.06f, 0.80f, 0.72f,  0.0f, 0.60f, 0.00f, 0.0f },
  { 1.04f, 0.94f, 0.24f, 0.06f,  0.10f, 0.78f, 0.74f,  2.0f, 0.55f, 0.00f, 0.0f },
  0.10f, 0.16f, 0.40f, 3.00f, 0.75f, 3000,  0.0f, 0.0f, 0.0f, -0.16f },

{ "VOID",                                     // nobody home
  { 1.00f, 0.98f, 0.10f, 0.04f,  0.00f, 0.42f, 0.94f,  0.0f, 0.00f, 0.00f, 0.0f },
  { 1.03f, 0.95f, 0.14f, 0.02f,  0.02f, 0.40f, 0.96f,  1.0f, 0.00f, 0.00f, 0.0f },
  0.0f, 0.04f, 0.06f, 3.20f, 0.60f, 2400,  0.0f, 0.0f, 0.0f, -0.20f },

// -- big translations: these move the eyes themselves, not just the pupils ----
// Trailing three fields are spread, headX, headY.
{ "PEEK_LEFT",                                // crams itself against the edge
  { 1.00f, 1.00f, 0.02f, 0.00f,  0.00f, 1.00f, 0.64f,  0.0f, 1.00f, 1.00f, 0.0f, 0.0f },
  { 1.03f, 0.97f, 0.06f, 0.00f, -0.03f, 0.97f, 0.66f,  1.0f, 0.90f, 1.05f, 0.0f, 0.0f },
  -1.0f, 0.0f, 0.0f, 1.20f, 1.30f, 2000,   -4.0f, -10.0f, 0.0f },

{ "PEEK_DOWN_RIGHT",
  { 1.00f, 0.97f, 0.14f, 0.00f,  0.06f, 1.00f, 0.64f,  0.0f, 0.95f, 1.00f, 0.0f, 0.0f },
  { 1.03f, 0.94f, 0.18f, 0.00f,  0.04f, 0.97f, 0.66f,  1.0f, 0.85f, 1.05f, 0.0f, 0.0f },
  0.90f, 0.90f, 0.0f, 1.20f, 1.30f, 2000,   -2.0f, 8.0f, 6.0f },

{ "TILTED",                                   // reads as a cocked head
  { 1.00f, 1.00f, 0.06f, 0.00f, -0.18f, 1.00f, 0.64f, -7.0f, 1.00f, 1.00f, 0.0f, -3.0f },
  { 1.04f, 0.96f, 0.00f, 0.04f,  0.14f, 0.97f, 0.66f,  7.0f, 0.90f, 1.05f, 0.0f,  3.0f },
  0.0f, -0.10f, 0.55f, 1.10f, 1.00f, 2200,    0.0f, 0.0f, 0.0f },

{ "FOCUS",                                    // eyes draw together, pupils shrink
  { 1.02f, 0.94f, 0.20f, 0.12f,  0.30f, 0.76f, 0.72f,  0.0f, 0.80f, 0.60f, 0.0f, 0.0f },
  { 1.05f, 0.91f, 0.24f, 0.10f,  0.26f, 0.74f, 0.74f,  1.0f, 0.70f, 0.65f, 0.0f, 0.0f },
  0.0f, 0.0f, 0.08f, 2.20f, 1.50f, 2400,  -17.0f, 0.0f, 0.0f },

{ "WIDE_EYED",                                // and spring apart again
  { 1.14f, 1.18f, 0.00f, 0.00f, -0.08f, 0.80f, 0.60f, -1.0f, 1.10f, 1.15f, 0.0f, 0.0f },
  { 1.17f, 1.14f, 0.00f, 0.00f, -0.04f, 0.78f, 0.62f,  1.0f, 1.00f, 1.20f, 0.0f, 0.0f },
  0.0f, -0.10f, 0.35f, 1.30f, 1.60f, 1600,   15.0f, 0.0f, -3.0f },

{ "GLANCE_BACK",                              // both crowd right, unevenly
  { 1.00f, 0.98f, 0.10f, 0.06f,  0.20f, 0.98f, 0.64f,  0.0f, 0.90f, 0.95f, 0.0f,  7.0f },
  { 1.03f, 0.95f, 0.22f, 0.04f,  0.16f, 0.95f, 0.66f,  1.0f, 0.80f, 1.00f, 0.0f, -5.0f },
  1.0f, 0.10f, 0.0f, 1.60f, 1.40f, 2000,   -3.0f, 9.0f, 0.0f },

{ "LOPSIDED",                                 // deliberately off-axis pair
  { 1.06f, 0.94f, 0.14f, 0.10f,  0.22f, 1.02f, 0.62f,  5.0f, 1.00f, 0.85f, 0.0f, -9.0f },
  { 0.94f, 1.06f, 0.02f, 0.00f, -0.10f, 0.92f, 0.68f, -4.0f, 0.85f, 1.10f, 0.0f,  7.0f },
  -0.25f, 0.0f, 0.45f, 1.40f, 1.10f, 2200,    0.0f, -4.0f, 0.0f },

// -- cross-eyed. Trailing fields: spread, headX, headY, converge -------------
{ "CROSSEYED",                                // both pupils pull to the nose
  { 1.02f, 1.02f, 0.02f, 0.04f,  0.00f, 1.00f, 0.62f,  0.0f, 1.05f, 1.00f, 0.0f, 0.0f },
  { 1.05f, 0.99f, 0.06f, 0.02f, -0.03f, 0.97f, 0.64f,  1.0f, 0.95f, 1.05f, 0.0f, 0.0f },
  0.0f, 0.12f, 0.06f, 2.20f, 1.40f, 1700,   -6.0f, 0.0f, 0.0f,  0.92f },

{ "DIZZY",                                    // crossed, drooping, unfocused
  { 1.04f, 0.98f, 0.22f, 0.10f,  0.16f, 1.14f, 0.58f,  2.0f, 1.15f, 0.90f, 0.55f, -3.0f },
  { 1.00f, 1.04f, 0.10f, 0.06f, -0.12f, 1.10f, 0.60f, -2.0f, 0.95f, 1.10f, 0.45f,  3.0f },
  0.0f, 0.05f, 0.35f, 1.60f, 0.80f, 2200,    2.0f, 0.0f, 0.0f,  0.55f },

// -- leaf. The only row that uses innerSeal in anger, so it needs all 16 fields.
// Both eyes taper to a point at the INNER corner instead of closing on a curve:
// innerSeal drags the upper lid down onto the lower one there, and the two curve
// terms round the outer end off, which is what turns an ellipse into a leaf.
// The left one stays open with its highlight pushed down to the tapered end
// (hiBig off, hiSmall wound right up) and the right one is a thin crescent
// sitting lower -- an asymmetric pose, so gazeRoam is low to hold it still.
{ "LEAF_WINK",
  { 1.10f, 1.16f, 0.06f, 0.26f,  0.35f, 1.05f, 0.60f,  0.0f, 0.00f, 4.00f, 0.0f, 0.0f,  0.30f,  0.55f, -0.25f, 0.85f },
  { 1.05f, 1.05f, 0.62f, 0.22f,  0.30f, 1.00f, 0.66f,  6.0f, 0.00f, 2.20f, 0.0f, 0.0f,  0.22f,  0.62f,  0.30f, 0.90f },
  0.25f, 0.30f, 0.25f, 1.90f, 3.00f, 2200,   0.0f, 0.0f, 0.0f,  0.0f,  0.25f },
};

const int EYE_STYLE_COUNT = (int)(sizeof(EYE_STYLES) / sizeof(EYE_STYLES[0]));

// Keeps the enum in EyeStyles.h and the table above from drifting apart.
static_assert(sizeof(EYE_STYLES) / sizeof(EYE_STYLES[0]) == STYLE_COUNT,
              "EYE_STYLES table and the STYLE_* enum are out of sync");
