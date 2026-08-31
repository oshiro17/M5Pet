// SuikaGame.h -- the watermelon game.
//
// The solver, in one paragraph: fruits are circles of equal mass, so a contact
// is resolved by pushing both halfway out along the line between their centres
// and nothing needs an inverse-mass term. Positions are integrated Verlet-style
// -- each fruit remembers where it was last step and the velocity lives in that
// difference -- which means a contact resolved by moving a fruit also removes
// exactly the velocity that drove it there. That is why a pile settles instead
// of buzzing: no restitution or friction has to be tuned away, because a
// position solver has none to begin with, and the real game is very nearly
// frictionless and non-bouncy anyway.
//
// Everything else follows from that. Terminal speed is 2.2 px per step against
// a 5 px cherry, so a fixed two substeps per frame cannot tunnel and no
// distance-driven substepping is needed. Contacts are solved bottom-up, sorted
// by depth, because a stack is supported from below and Gauss-Seidel carries
// that support up the pile in one pass if it visits the bottom first.
//
// Coordinates are box-local: x runs 0..SUIKA_BOX_W and y runs 0..SUIKA_BOX_H
// from the top of the box, so the waiting fruit sits at a negative y and simply
// falls in. Drawing adds the box origin back on.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

class SuikaGame {
 public:
  void begin(uint32_t now);

  // tapA is a latched press from the .ino, not M5.BtnA: the button is read every
  // millisecond and the frame runs every 33, so the edge has to be caught up
  // there and handed down. One press, one fruit -- there is nothing to tell
  // apart, which is why this takes a single flag.
  void update(uint32_t now, bool tapA);
  void draw(M5Canvas& cv);

  bool over() const { return _phase == PH_OVER; }
  int  score() const { return _score; }
  int  highscore() const { return _high; }

 private:
  enum Phase : uint8_t {
    PH_AIM = 0,   // a fruit waits above the box and follows the tilt
    PH_DROP,      // one is on its way down; the next appears when it lands
    PH_OVER
  };

  struct Fruit {
    float x, y;        // current position, box-local
    float px, py;      // previous position; the velocity lives in the difference
    float ang, spin;   // decorative roll
    uint16_t glee;     // ms of merge face left
    float budget;      // separation left this step   -- see SUIKA_MAX_PUSH
    float vbudget;     // of which this much may become speed -- SUIKA_SOFT_PUSH
    uint8_t tier;
    uint8_t flags;
  };

  enum : uint8_t {
    F_MERGING = 1,   // claimed by a merge this step -- the three-way guard
    F_TOUCHED = 2,   // has hit something at least once
    F_FALLING = 4,   // the fruit the player just released
    F_DEAD    = 8,   // consumed by a merge; swept up in the compaction pass
  };

  // --- solver
  void integrate(float dt);
  void sortByDepth();
  // `last` marks the final relaxation pass. Only that pass may accumulate spin
  // or record a merge: both are once-per-contact facts, and running them on
  // every iteration counts the same contact four times over.
  void solveWalls(bool last);
  void solvePairs(bool last);
  void applyMerges();
  void step(float dt);

  // --- rules
  void readTilt(float realDt);
  void spawnPreview();
  void dropFruit(uint32_t now);
  void checkOver(uint32_t now, float realDt);
  int  addFruit(int tier, float x, float y);
  void loadHighscore();
  void saveHighscore();

  // A merge sparkle. Not a fruit and not in the solver: it has no radius, hits
  // nothing, and lives on the draw side of the game entirely. Keeping it out of
  // _f is what makes that true -- a particle in the physics array would be
  // sorted, solved and merge-tested every substep for nothing.
  struct Spark { float x, y, vx, vy, life; };

  void spawnSparks(float x, float y, int tier);
  void updateSparks(float dt);

  // --- drawing
  void drawField(M5Canvas& cv) const;
  void drawSparks(M5Canvas& cv) const;
  void drawWalls(M5Canvas& cv) const;
  void drawHud(M5Canvas& cv) const;
  void drawBanner(M5Canvas& cv) const;

  Phase    _phase      = PH_AIM;
  uint32_t _phaseStart = 0;
  uint32_t _lastMs     = 0;

  Fruit   _f[SUIKA_MAX_FRUITS];
  uint8_t _order[SUIKA_MAX_FRUITS];   // indices, deepest first
  int     _n       = 0;
  bool    _reorder = true;            // set when the array changes shape

  // Merges are collected during the last relaxation pass and applied after it,
  // never during: rewriting the array mid-solve would corrupt the pass that is
  // still walking it.
  struct MergePair { uint8_t a, b; };
  MergePair _merge[16];
  int       _nMerge = 0;

  float _roll   = 0.0f;
  float _aimX   = SUIKA_BOX_W * 0.5f;
  int   _cur = 0, _next = 0;

  uint32_t _overMs  = 0;      // how long something has been over the line
  bool     _lineHot = false;  // ...and therefore whether to show it

  Spark _spark[SUIKA_MAX_SPARKS];
  int   _nSpark = 0;

  int _score = 0;
  int _high  = 0;
};
