#include "BootAnimation.h"
#include "PetState.h"
#include "AppleBitmap.h"

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
static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

// Drawn in three-quarter profile: both eyes, the blush and the mouth crowd
// onto the leading side of the face, so the character reads as looking at --
// and biting into -- whatever is in front of it. face = -1 faces left.
static void drawCharacter(M5Canvas& cv, float x, float y, float squash,
                          float mouth, float puff, float walk, uint8_t expr,
                          int face) {
  const float rx = CHAR_RADIUS * (1.0f + squash);
  const float ry = CHAR_RADIUS * (1.0f - squash);
  const float f  = (float)face;

  const float step = sinf(walk * TWO_PI);

  // --- far foot: drawn first, so the body hides all but the heel
  cv.fillEllipse(ri(x - f * rx * 0.38f - step * 3.0f), ri(y + ry * 0.90f),
                 rr(rx * 0.36f), rr(ry * 0.22f), COL_CHAR_FOOT_DK);

  // --- body
  cv.fillEllipse(ri(x), ri(y), rr(rx), rr(ry), COL_CHAR_BODY);

  // shaded underside: an ellipse that sits wholly inside the body, so the
  // boundary is a curve rather than the straight edge a clip rect would give
  cv.fillEllipse(ri(x), ri(y + ry * 0.56f),
                 rr(rx * 0.86f), rr(ry * 0.40f), COL_CHAR_SHADE);

  // soft sheen on the crown. Kept small, low-contrast and near the top: out on
  // the flank it breaks the silhouette and reads as a second arm.
  cv.fillEllipse(ri(x - f * rx * 0.16f), ri(y - ry * 0.66f),
                 rr(rx * 0.17f), rr(ry * 0.09f), COL_CHAR_LIGHT);

  // --- near foot: big, planted forward, overlapping the body
  cv.fillEllipse(ri(x + f * rx * 0.42f + step * 3.0f), ri(y + ry * 0.98f),
                 rr(rx * 0.40f), rr(ry * 0.24f), COL_CHAR_FOOT);

  // --- near arm only. Drawing the far arm as well is exactly what collapses a
  //     profile back into a front view. It also swings back behind the body as
  //     the mouth opens, which keeps it clear of the bite and reads as a
  //     wind-up.
  const float armOut = 1.0f - mouth * 0.85f;
  cv.fillEllipse(ri(x + f * rx * (0.30f + 0.72f * armOut)),
                 ri(y + ry * (0.50f - 0.10f * armOut)),
                 rr(rx * 0.25f), rr(ry * 0.32f), COL_CHAR_BODY);

  // --- blush on the visible cheek; it swells while chewing
  const float cheekRx = 5.5f + puff * 4.0f;
  cv.fillEllipse(ri(x + f * rx * 0.02f), ri(y + ry * 0.08f),
                 rr(cheekRx), rr(cheekRx * 0.58f), COL_CHEEK);

  // --- eyes, both pushed towards the leading side. The far one is narrower,
  //     which is what makes the head read as turned rather than flat-on.
  // Past mouth = 1 the character is inhaling, not biting: the mouth takes over
  // the face, so the eyes retreat to the crown and shrink to make room.
  const float wide  = clampf(mouth - 1.0f, 0.0f, 1.0f);
  const float eNear = f * rx * 0.10f;   // closer to us
  const float eFar  = f * rx * 0.56f;   // further round the cheek
  const float ey    = y - ry * (0.30f + 0.32f * wide);
  const float eScale = 1.0f - 0.24f * wide;
  if (expr == /*EXPR_HAPPY*/ 2) {
    for (int t = 0; t < 3; ++t) {
      const int cxs[2] = { ri(x + eNear), ri(x + eFar) };
      for (int i = 0; i < 2; ++i) {
        const int cx = cxs[i];
        cv.drawLine(cx - 5, ri(ey) + 3 + t, cx, ri(ey) - 4 + t, COL_EYE_NAVY);
        cv.drawLine(cx, ri(ey) - 4 + t, cx + 5, ri(ey) + 3 + t, COL_EYE_NAVY);
      }
    }
  } else {
    const float offs[2]   = { eNear, eFar };
    const float narrow[2] = { 1.0f, 0.68f };
    for (int i = 0; i < 2; ++i) {
      const int cx = ri(x + offs[i]), cy = ri(ey);
      const int erx = rr(5.2f * narrow[i] * eScale), ery = rr(10.0f * eScale);
      // three bands, as in the reference: white cap, navy middle, blue floor
      cv.fillEllipse(cx, cy, erx, ery, COL_EYE_NAVY);
      cv.setClipRect(cx - erx, cy + rr(ery * 0.28f), erx * 2 + 1, ery + 1);
      cv.fillEllipse(cx, cy, rr(erx * 0.78f), rr(ery * 0.88f), COL_EYE_BLUE);
      cv.clearClipRect();
      cv.fillEllipse(cx, ri(cy - ery * 0.48f),
                     rr(erx * 0.62f), rr(ery * 0.26f), COL_WHITE);
    }
  }

  // --- mouth, on the leading edge of the face so it meets the apple
  const float mx = x + f * rx * (0.50f - 0.14f * wide);
  const float my = y + ry * (0.26f + 0.06f * wide);
  if (mouth > 0.03f) {
    const float mrx = 3.0f + mouth * rx * 0.36f;
    const float mry = 3.0f + mouth * ry * 0.29f;
    cv.fillEllipse(ri(mx), ri(my), rr(mrx), rr(mry), COL_MOUTH_DK);
    // throat: the tan oval that fills the bottom of a wide-open mouth
    cv.fillEllipse(ri(mx + f * mrx * 0.10f), ri(my + mry * 0.34f),
                   rr(mrx * 0.66f), rr(mry * 0.42f), COL_MOUTH_IN);
  } else {
    // resting: the small oval mouth from the reference
    cv.fillEllipse(ri(mx), ri(my), rr(3), rr(4), COL_MOUTH_DK);
  }
}

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
    drawCharacter(cv, _currentX, _currentY + _hopY, _squash,
                  _mouth, _puff, _walk, _expr, _face);
  }
}
