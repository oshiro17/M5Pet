// BootAnimation.h -- "a whole apple gets bitten, then inhaled" intro.
//
// The apple is a single 1bpp silhouette (AppleBitmap.h, traced from
// references/apple_whole.png). The bite is not a second asset: it is a circle
// -- fitted to references/apple_bitten.png with sub-pixel accuracy -- whose
// radius is animated from 0 to full while the character chomps. So the same
// mask renders "before", "mid-bite" and "after". For the finale that same mask
// is drawn through a rotate/scale transform as it spirals into the mouth.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"
#include "Character.h"

class BootAnimation {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  void draw(M5Canvas& cv);
  bool finished() const { return _phase >= PH_DONE; }
  // 0..1, so the starfield can fade in and out with the scene
  float brightness() const { return _appleFade; }
  void skip();                       // jump straight to the end

 private:
  enum Phase : uint8_t {
    PH_APPLE_IN = 0,   // the untouched apple fades up
    PH_ENTER,          // character bounces in from the right
    PH_APPROACH,       // sidles up to the apple
    PH_MOUTH,          // opens wide
    PH_BITE,           // *chomp* -- the bite circle grows
    PH_CHEW,           // three chews, cheeks puffing
    PH_HAPPY,          // satisfied face
    PH_INHALE_OPEN,    // rears back, mouth yawns right open
    PH_INHALE,         // the apple spins, shrinks and is sucked in
    PH_GULP,           // swallows, body bulges
    PH_EXIT,           // bounces off to the right
    PH_FADE,           // fade to black
    PH_DONE
  };

  // Character expressions come from Character.h (CharExpr).
  void enterPhase(Phase p, uint32_t now);
  float phaseProgress(uint32_t now) const;   // 0..1 within the current phase
  float mouthX() const;
  float mouthY() const;

  Phase    _phase      = PH_APPLE_IN;
  uint32_t _phaseStart = 0;
  float    _animT      = 0.0f;   // seconds, for the suction streaks

  // apple
  float _biteR      = 0.0f;    // current bite radius, px
  float _appleFade  = 0.0f;    // 0 = black, 1 = full grey
  float _appleCX    = 0.0f;    // only used while it is being inhaled
  float _appleCY    = 0.0f;
  float _appleScale = 1.0f;
  float _appleAngle = 0.0f;
  bool  _appleGone  = false;

  // character -- current/target pairs, smoothed every frame
  float _currentX = CHAR_X_START, _targetX = CHAR_X_START;
  float _currentY = CHAR_GROUND_Y, _targetY = CHAR_GROUND_Y;
  float _hopY     = 0.0f;      // additive bounce offset
  float _squash   = 0.0f;      // + = wide and short
  float _mouth    = 0.0f;      // 0..1 = bite, up to 2 = inhaling
  float _puff     = 0.0f;      // 0..1 cheeks
  float _walk     = 0.0f;      // foot phase
  float _suction  = 0.0f;      // 0..1 strength of the inhale streaks
  float _smoothK  = 0.22f;     // how hard currentX chases targetX this phase
  int   _face     = -1;        // -1 = facing left (towards the apple), +1 = right
  uint8_t _expr   = EXPR_NORMAL;
};
