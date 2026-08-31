// GameMenu.h -- pick one of the two games, then count in.
//
// There are two of them now, and the face only has one spare button: A used to
// go straight into the block breaker, so a second game needed somewhere to be
// chosen from. Tilt picks, A commits, and a three-beat countdown gives the
// player a moment to get their grip right before anything starts moving --
// which matters more than it sounds when the controls *are* the grip.
//
// Runs in the portrait 135x240 canvas, the same one both games use, so the
// .ino flips the display once on the way in and not again until the way out.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

class GameMenu {
 public:
  enum Choice : uint8_t { BLOCKS = 0, SUIKA = 1 };

  void begin(uint32_t now);
  void update(uint32_t now, bool tapA);
  void draw(M5Canvas& cv);

  bool   finished() const { return _done; }
  Choice choice() const { return (Choice)_sel; }

 private:
  void drawCard(M5Canvas& cv, int slot, bool on) const;

  uint32_t _startedAt = 0;   // when the countdown began; 0 while still choosing
  uint32_t _lastMs    = 0;
  float    _roll      = 0.0f;
  bool     _armed     = true;  // tilt has returned to centre, so it may step again
  uint8_t  _sel       = 0;
  bool     _done      = false;
  int      _lastBeat  = -1;
};
