// Trail.h -- the magic light trail behind the ball, plus the burst a broken
// block throws off.
//
// Two layers that read as one effect:
//
//   ribbon    a ring buffer of the ball's recent positions, drawn as shrinking
//             circles that fade towards the background. This is what gives the
//             trail its *length*: the faster the ball, the further apart the
//             samples, so the same 14 points stretch further across the screen.
//
//   particles stars and sparkles thrown off the back of the ball and out of
//             broken blocks. White, aqua and pink, spun and faded over their
//             own lifetime.
//
// Everything here is fed the already-scaled dt, so slow motion slows the
// effects to match without any special-casing.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

class Trail {
 public:
  void begin();
  void clear();

  // Record where the ball is now. `speed` is px/s and drives how much of the
  // ribbon is drawn; `intensity` (0..1, from the level) thickens it.
  void track(float x, float y, float speed, float intensity, float dt);

  // A block just broke: throw stars outwards from (x, y).
  void burst(float x, float y, uint16_t tint, int count, float power);

  void update(float dt);
  void drawRibbon(M5Canvas& cv) const;      // behind the ball
  void drawParticles(M5Canvas& cv) const;   // in front of it

 private:
  struct Particle {
    float x, y, vx, vy;
    float life, life0;
    float size, rot, spin;
    uint16_t col;
    uint8_t  kind;   // 0 = star, 1 = sparkle (4-point), 2 = dot
  };

  void spawn(float x, float y, float vx, float vy, float life, float size,
             uint16_t col, uint8_t kind);

  struct Point { float x, y, w; };
  Point   _pts[GAME_TRAIL_POINTS];
  int     _head  = 0;
  int     _count = 0;
  float   _emitAcc = 0.0f;   // fractional particles owed since the last frame

  Particle _p[GAME_PARTICLES];
  int      _next = 0;
};
