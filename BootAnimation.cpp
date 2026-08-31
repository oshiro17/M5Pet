#include "BootAnimation.h"
#include "PetState.h"
#include "AppleBitmap.h"
#include "Character.h"

// Length of each phase, indexed by Phase. Total ~3.6 s.
static const uint16_t PHASE_MS[] = {
  /* APPLE_IN    */ 300,
  /* ENTER       */ 520,
  /* APPROACH    */ 240,
  /* MOUTH       */ 220,
  /* BITE        */ 240,
  /* CHEW        */ 480,
  /* HAPPY       */ 220,
  /* INHALE_OPEN */ 260,
  /* INHALE      */ 520,
  /* GULP        */ 260,
  /* EXIT        */ 380,
  /* FADE        */ 240,
};

// Centre of the apple where it sits before it gets eaten.
static const float APPLE_HOME_X = APPLE_ORIGIN_X + APPLE_BMP_W * 0.5f;
static const float APPLE_HOME_Y = APPLE_ORIGIN_Y + APPLE_BMP_H * 0.5f;

// ---------------------------------------------------------------------------
// Apple
// ---------------------------------------------------------------------------
// Walks the packed 1bpp mask row by row and emits horizontal runs. The bite is
// subtracted analytically per row, so a partially eaten apple costs the same as
// a whole one and never leaves seams.
static void drawApple(M5Canvas& cv, int ox, int oy, uint16_t color, float biteR) {
  for (int y = 0; y < APPLE_BMP_H; ++y) {
    // x range removed by the bite on this row (empty when bx0 > bx1)
    int bx0 = 1, bx1 = 0;
    if (biteR > 0.5f) {
      const float dy = (y + 0.5f) - APPLE_BITE_CY;
      const float d2 = biteR * biteR - dy * dy;
      if (d2 > 0.0f) {
        const float dx = sqrtf(d2);
        bx0 = (int)ceilf(APPLE_BITE_CX - dx);
        bx1 = (int)floorf(APPLE_BITE_CX + dx);
      }
    }

    const uint8_t* row = &APPLE_BMP[y * APPLE_BMP_STRIDE];
    int runStart = -1;
    for (int x = 0; x <= APPLE_BMP_W; ++x) {
      bool on = false;
      if (x < APPLE_BMP_W) {
        on = (row[x >> 3] >> (7 - (x & 7))) & 1;
        if (on && x >= bx0 && x <= bx1) on = false;
      }
      if (on) {
        if (runStart < 0) runStart = x;
      } else if (runStart >= 0) {
        cv.drawFastHLine(ox + runStart, oy + y, x - runStart, color);
        runStart = -1;
      }
    }
  }
}

// Same mask, but drawn through an inverse rotate/scale so the apple can spiral
// into the mouth. Inverse mapping means no gaps, and runs are still batched
// into horizontal lines.
static void drawAppleXf(M5Canvas& cv, float cx, float cy, float scale,
                        float ang, uint16_t color, float biteR) {
  if (scale <= 0.03f) return;
  const float hw = APPLE_BMP_W * 0.5f, hh = APPLE_BMP_H * 0.5f;
  const float reach = sqrtf(hw * hw + hh * hh) * scale + 2.0f;
  const float ca = cosf(ang) / scale, sa = sinf(ang) / scale;

  int y0 = (int)floorf(cy - reach), y1 = (int)ceilf(cy + reach);
  int x0 = (int)floorf(cx - reach), x1 = (int)ceilf(cx + reach);
  if (y0 < 0) y0 = 0;
  if (x0 < 0) x0 = 0;
  if (y1 > SCREEN_H - 1) y1 = SCREEN_H - 1;
  if (x1 > SCREEN_W - 1) x1 = SCREEN_W - 1;

  for (int dy = y0; dy <= y1; ++dy) {
    const float uy = dy - cy;
    int runStart = -1;
    for (int dx = x0; dx <= x1 + 1; ++dx) {
      bool on = false;
      if (dx <= x1) {
        const float ux = dx - cx;
        const float fx = ca * ux + sa * uy + hw;
        const float fy = -sa * ux + ca * uy + hh;
        if (fx >= 0.0f && fy >= 0.0f) {
          const int ix = (int)fx, iy = (int)fy;
          if (ix < APPLE_BMP_W && iy < APPLE_BMP_H) {
            on = (APPLE_BMP[iy * APPLE_BMP_STRIDE + (ix >> 3)] >> (7 - (ix & 7))) & 1;
            if (on && biteR > 0.5f) {
              const float bx = (ix + 0.5f) - APPLE_BITE_CX;
              const float by = (iy + 0.5f) - APPLE_BITE_CY;
              if (bx * bx + by * by <= biteR * biteR) on = false;
            }
          }
        }
      }
      if (on) {
        if (runStart < 0) runStart = dx;
      } else if (runStart >= 0) {
        cv.drawFastHLine(runStart, dy, dx - runStart, color);
        runStart = -1;
      }
    }
  }
}

// The pale blue streaks that fly into the mouth while it inhales.
static void drawSuction(M5Canvas& cv, float mx, float my, float f,
                        float t, float strength) {
  for (int i = 0; i < 4; ++i) {
    float ph = t * 2.2f + i * 0.27f;
    ph -= floorf(ph);
    const float dist = 66.0f * (1.0f - ph);
    const float spread = (i - 1.5f) * 15.0f * (0.35f + 0.65f * ph);
    const int xa = (int)lroundf(mx + f * (dist + 16.0f));
    const int xb = (int)lroundf(mx + f * dist);
    const int ya = (int)lroundf(my + spread);
    const int yb = (int)lroundf(my + spread * 0.45f);
    const uint16_t c = mixColor(COL_BG, COL_SUCTION,
                                strength * (0.30f + 0.70f * ph));
    cv.drawLine(xa, ya, xb, yb, c);
    cv.drawLine(xa, ya + 1, xb, yb + 1, c);
  }
}

// ---------------------------------------------------------------------------
// Character (pink, round, big vertical eyes, red feet)
// ---------------------------------------------------------------------------
// The character itself now lives in Character.cpp, shared with the game.


// ---------------------------------------------------------------------------
// Timeline
// ---------------------------------------------------------------------------
void BootAnimation::begin(uint32_t now) {
  _phase = PH_APPLE_IN;
  _phaseStart = now;
  _currentX = _targetX = CHAR_X_START;
  _currentY = _targetY = CHAR_GROUND_Y;
  _biteR = 0.0f;
  _appleFade = 0.0f;
  _appleScale = 1.0f;
  _appleAngle = 0.0f;
  _appleCX = APPLE_HOME_X;
  _appleCY = APPLE_HOME_Y;
  _appleGone = false;
  _mouth = _puff = _squash = _hopY = _walk = _suction = 0.0f;
  _expr = EXPR_NORMAL;
}

void BootAnimation::skip() {
  _phase = PH_DONE;
  _appleGone = true;
  _appleFade = 0.0f;
}

// Where the mouth sits on screen -- the apple is inhaled towards this point.
float BootAnimation::mouthX() const {
  return _currentX + _face * CHAR_RADIUS * 0.36f;
}
float BootAnimation::mouthY() const {
  return _currentY + _hopY + CHAR_RADIUS * 0.36f;
}

void BootAnimation::enterPhase(Phase p, uint32_t now) {
  _phase = p;
  _phaseStart = now;
}

float BootAnimation::phaseProgress(uint32_t now) const {
  if (_phase >= PH_DONE) return 1.0f;
  const uint32_t dur = PHASE_MS[_phase];
  if (dur == 0) return 1.0f;
  return clampf((float)(now - _phaseStart) / (float)dur, 0.0f, 1.0f);
}

void BootAnimation::update(uint32_t now) {
  if (_phase >= PH_DONE) return;

  // advance the timeline
  while (_phase < PH_DONE && (now - _phaseStart) >= PHASE_MS[_phase]) {
    const uint32_t spent = PHASE_MS[_phase];
    enterPhase((Phase)(_phase + 1), _phaseStart + spent);
    if (_phase >= PH_DONE) return;
  }

  const float p = phaseProgress(now);

  // defaults each frame; phases below override what they care about
  _squash = 0.0f;
  _hopY = 0.0f;
  _expr = EXPR_NORMAL;
  _mouth = 0.0f;
  _suction = 0.0f;
  _smoothK = 0.22f;
  _animT = now * 0.001f;
  _face = -1;                     // faces the apple for everything but the exit

  switch (_phase) {
    case PH_APPLE_IN:
      _appleFade = easeOutCubic(p);
      _targetX = CHAR_X_START;
      break;

    case PH_ENTER: {
      _appleFade = 1.0f;
      _targetX = CHAR_X_APPROACH;
      // three bounces, flattening out as it lands
      const float hop = fabsf(sinf(p * PI * 3.0f));
      _hopY = -hop * 15.0f;
      if (hop < 0.18f) _squash = (0.18f - hop) * 0.9f;
      _walk = p * 3.0f;
      break;
    }

    case PH_APPROACH: {
      _targetX = CHAR_X_MOUTH;
      const float hop = fabsf(sinf(p * PI));
      _hopY = -hop * 6.0f;
      _walk = 3.0f + p;
      break;
    }

    case PH_MOUTH:
      _targetX = CHAR_X_MOUTH;
      _targetY = CHAR_BITE_Y;         // rise to the height of the bite
      _mouth = easeOutCubic(p);
      _expr = EXPR_EATING;
      break;

    case PH_BITE: {
      _targetY = CHAR_BITE_Y;
      _biteR = APPLE_BITE_R * easeOutCubic(p);
      _mouth = 1.0f - easeOutCubic(p);            // chomps shut on the apple
      _expr = EXPR_EATING;
      // drive the head into the apple and back out again
      _smoothK = 0.45f;
      _targetX = CHAR_X_MOUTH - (CHAR_X_MOUTH - CHAR_X_BITE) * sinf(p * PI);
      _squash = 0.10f * sinf(p * PI);
      break;
    }

    case PH_CHEW: {
      _biteR = APPLE_BITE_R;
      // back off so the missing chunk is plainly visible while it chews
      _targetX = CHAR_X_CHEW;
      _targetY = CHAR_GROUND_Y;       // settle back down while chewing
      const float c = p * 3.0f;       // three chews
      const float ph = c - floorf(c);
      _puff = 0.5f - 0.5f * cosf(ph * TWO_PI);
      _squash = 0.09f * sinf(ph * TWO_PI);
      _hopY = -3.0f * fabsf(sinf(ph * PI));
      _mouth = 0.10f * (1.0f - ph);
      _expr = EXPR_EATING;
      break;
    }

    case PH_HAPPY: {
      _biteR = APPLE_BITE_R;
      _targetX = CHAR_X_CHEW;
      _targetY = CHAR_GROUND_Y;
      _expr = EXPR_HAPPY;
      _puff = 0.0f;
      _mouth = 0.0f;
      const float hop = fabsf(sinf(p * PI));
      _hopY = -hop * 9.0f;
      _squash = -0.08f * hop;
      break;
    }

    case PH_INHALE_OPEN: {
      _biteR = APPLE_BITE_R;
      _targetX = CHAR_X_INHALE;
      _targetY = CHAR_Y_INHALE;
      _mouth = 2.0f * easeOutCubic(p);      // yawns right open
      _expr = EXPR_EATING;
      _suction = p * p;
      _squash = -0.06f * p;                 // rears up
      break;
    }

    case PH_INHALE: {
      _biteR = APPLE_BITE_R;
      _targetX = CHAR_X_INHALE;
      _targetY = CHAR_Y_INHALE;
      _mouth = 2.0f;
      _expr = EXPR_EATING;
      _suction = 1.0f;
      // the apple accelerates in, spinning and shrinking as it goes
      // eases in, but not as sharply as a pure square: the apple should be
      // visibly drifting before it gets yanked in
      const float e = 0.35f * p + 0.65f * p * p;
      _appleCX = lerpf(APPLE_HOME_X, mouthX(), e);
      _appleCY = lerpf(APPLE_HOME_Y, mouthY(), e);
      _appleScale = 1.0f - 0.94f * e;
      _appleAngle = e * 5.0f * PI;
      _squash = 0.05f * sinf(p * PI * 6.0f);   // straining
      break;
    }

    case PH_GULP: {
      _appleGone = true;
      _targetX = CHAR_X_INHALE;
      _targetY = CHAR_GROUND_Y;
      _mouth = 2.0f * (1.0f - easeOutCubic(clampf(p * 1.7f, 0.0f, 1.0f)));
      _expr = (p > 0.55f) ? EXPR_HAPPY : EXPR_EATING;
      _squash = 0.17f * sinf(p * PI);          // the swallow
      _suction = (1.0f - p) * 0.5f;
      break;
    }

    case PH_EXIT: {
      _appleGone = true;
      _targetX = CHAR_X_EXIT;
      _expr = EXPR_HAPPY;
      _face = 1;                                   // turns round to walk off
      const float hop = fabsf(sinf(p * PI * 2.0f));
      _hopY = -hop * 13.0f;
      _walk = 4.0f + p * 2.0f;
      break;
    }

    case PH_FADE:
      _appleGone = true;
      _appleFade = 1.0f - easeInQuad(p);
      break;

    default:
      break;
  }

  // current/target interpolation -- this is what keeps the character motion
  // smooth even though the timeline snaps between phases
  smoothTowards(_currentX, _targetX, _smoothK);
  smoothTowards(_currentY, _targetY, 0.24f);
}

void BootAnimation::draw(M5Canvas& cv) {
  if (_phase >= PH_DONE) return;

  const uint16_t appleCol = mixColor(COL_BG, COL_APPLE, _appleFade);

  if (!_appleGone) {
    if (_phase == PH_INHALE) {
      drawAppleXf(cv, _appleCX, _appleCY, _appleScale, _appleAngle,
                  appleCol, _biteR);
    } else {
      drawApple(cv, APPLE_ORIGIN_X, APPLE_ORIGIN_Y, appleCol, _biteR);
    }
  }

  if (_suction > 0.02f) {
    drawSuction(cv, mouthX(), mouthY(), (float)_face, _animT,
                _suction * _appleFade);
  }

  // character on top -- it hides the bite while chomping, then backs off and
  // reveals it, which is what sells the "it just took a bite" read
  if (_currentX < 288.0f) {
    drawCharacterProfile(cv, _currentX, _currentY + _hopY, _squash,
                         _mouth, _puff, _walk, _expr, _face);
  }
}
