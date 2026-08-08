#include "BootAnimation.h"
#include "PetState.h"
#include "AppleBitmap.h"

// Length of each phase, indexed by Phase. Total ~3.6 s.
static const uint16_t PHASE_MS[] = {
  /* APPLE_IN */ 350,
  /* ENTER    */ 620,
  /* APPROACH */ 240,
  /* MOUTH    */ 200,
  /* BITE     */ 180,
  /* CHEW     */ 720,
  /* HAPPY    */ 300,
  /* EXIT     */ 420,
  /* GLOW     */ 260,
  /* FADE     */ 280,
};

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

// ---------------------------------------------------------------------------
// Character (pink, round, big vertical eyes, red feet)
// ---------------------------------------------------------------------------
static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

static void drawCharacter(M5Canvas& cv, float x, float y, float squash,
                          float mouth, float puff, float walk, uint8_t expr) {
  const float rx = CHAR_RADIUS * (1.0f + squash);
  const float ry = CHAR_RADIUS * (1.0f - squash);

  // --- feet: two red ovals that swap back and forth while moving
  const float step = sinf(walk * TWO_PI);
  cv.fillEllipse(ri(x - rx * 0.60f + step * 4.0f), ri(y + ry * 0.82f + 4),
                 rr(11), rr(7), COL_CHAR_FOOT);
  cv.fillEllipse(ri(x + rx * 0.50f - step * 4.0f), ri(y + ry * 0.86f + 4),
                 rr(12), rr(7), COL_CHAR_FOOT);

  // --- stubby arms, behind the body
  cv.fillEllipse(ri(x - rx * 0.95f), ri(y - ry * 0.05f), rr(8), rr(10), COL_CHAR_BODY);
  cv.fillEllipse(ri(x + rx * 0.95f), ri(y - ry * 0.10f), rr(8), rr(10), COL_CHAR_BODY);

  // --- body
  cv.fillEllipse(ri(x), ri(y), rr(rx), rr(ry), COL_CHAR_BODY);

  // shaded underside: same ellipse, clipped to the bottom third
  const int bodyTop = ri(y - ry), bodyBot = ri(y + ry);
  const int shadeTop = ri(y + ry * 0.34f);
  if (bodyBot > shadeTop) {
    cv.setClipRect(ri(x - rx) - 1, shadeTop, rr(rx * 2 + 2), bodyBot - shadeTop + 1);
    cv.fillEllipse(ri(x), ri(y), rr(rx), rr(ry), COL_CHAR_SHADE);
    cv.clearClipRect();
  }
  // soft highlight, upper left
  cv.fillEllipse(ri(x - rx * 0.42f), ri(y - ry * 0.50f),
                 rr(rx * 0.26f), rr(ry * 0.17f), COL_CHAR_LIGHT);

  // --- cheeks; they swell while chewing
  const float cheekRx = 5.0f + puff * 3.5f;
  cv.fillEllipse(ri(x - rx * 0.58f), ri(y + ry * 0.14f),
                 rr(cheekRx), rr(cheekRx * 0.62f), COL_CHEEK);
  cv.fillEllipse(ri(x + rx * 0.58f), ri(y + ry * 0.14f),
                 rr(cheekRx), rr(cheekRx * 0.62f), COL_CHEEK);

  // --- eyes
  const float ex = rx * 0.30f, ey = y - ry * 0.18f;
  if (expr == /*EXPR_HAPPY*/ 2) {
    // satisfied: two upward arcs
    for (int t = 0; t < 3; ++t) {
      for (int s = -1; s <= 1; s += 2) {
        const int cx = ri(x + s * ex);
        cv.drawLine(cx - 6, ri(ey) + 3 + t, cx, ri(ey) - 4 + t, COL_EYE_NAVY);
        cv.drawLine(cx, ri(ey) - 4 + t, cx + 6, ri(ey) + 3 + t, COL_EYE_NAVY);
      }
    }
  } else {
    for (int s = -1; s <= 1; s += 2) {
      const int cx = ri(x + s * ex), cy = ri(ey);
      const int erx = rr(4.5f), ery = rr(8.5f);
      cv.fillEllipse(cx, cy, erx, ery, COL_EYE_NAVY);
      // blue lower half
      cv.setClipRect(cx - erx, cy + ery / 4, erx * 2 + 1, ery + 1);
      cv.fillEllipse(cx, cy, erx, ery, COL_EYE_BLUE);
      cv.clearClipRect();
      // white glint near the top
      cv.fillEllipse(cx, cy - ery / 2, rr(2), rr(3), COL_WHITE);
    }
  }

  // --- mouth
  if (mouth > 0.03f) {
    const float mw = 4.0f + mouth * 10.0f;
    const float mh = 3.0f + mouth * 12.0f;
    const int my = ri(y + ry * 0.30f);
    cv.fillEllipse(ri(x), my, rr(mw), rr(mh), COL_MOUTH_DK);
    cv.fillEllipse(ri(x), my + rr(mh * 0.35f), rr(mw * 0.62f), rr(mh * 0.38f), COL_MOUTH);
  } else if (expr == /*EXPR_HAPPY*/ 2) {
    const int my = ri(y + ry * 0.30f);
    cv.fillEllipse(ri(x), my, rr(5), rr(3), COL_MOUTH);
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
  _appleGlow = 0.0f;
  _mouth = _puff = _squash = _hopY = _walk = 0.0f;
  _expr = EXPR_NORMAL;
}

void BootAnimation::skip() {
  _phase = PH_DONE;
  _biteR = APPLE_BITE_R;
  _appleFade = 0.0f;
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

  switch (_phase) {
    case PH_APPLE_IN:
      _appleFade = easeOutCubic(p);
      _targetX = CHAR_X_START;
      _mouth = 0.0f;
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
      _targetX = CHAR_X_BITE;
      const float hop = fabsf(sinf(p * PI));
      _hopY = -hop * 6.0f;
      _walk = 3.0f + p;
      break;
    }

    case PH_MOUTH:
      _targetX = CHAR_X_BITE;
      _targetY = CHAR_BITE_Y;         // lunge up to the bite height
      _mouth = easeOutCubic(p);
      _expr = EXPR_EATING;
      break;

    case PH_BITE: {
      _targetY = CHAR_BITE_Y;
      _biteR = APPLE_BITE_R * easeOutCubic(p);
      _mouth = 1.0f - 0.6f * p;
      _expr = EXPR_EATING;
      // short lunge into the apple, then back
      _targetX = CHAR_X_BITE - 6.0f * sinf(p * PI);
      _squash = 0.10f * sinf(p * PI);
      break;
    }

    case PH_CHEW: {
      _biteR = APPLE_BITE_R;
      _targetX = CHAR_X_BITE + 4.0f;
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
      _targetY = CHAR_GROUND_Y;
      _expr = EXPR_HAPPY;
      _puff = 0.0f;
      _mouth = 0.0f;
      const float hop = fabsf(sinf(p * PI));
      _hopY = -hop * 9.0f;
      _squash = -0.08f * hop;
      break;
    }

    case PH_EXIT: {
      _biteR = APPLE_BITE_R;
      _targetX = CHAR_X_EXIT;
      _expr = EXPR_HAPPY;
      const float hop = fabsf(sinf(p * PI * 2.0f));
      _hopY = -hop * 13.0f;
      _walk = 4.0f + p * 2.0f;
      break;
    }

    case PH_GLOW:
      _biteR = APPLE_BITE_R;
      _appleGlow = easeInOutSine(p);
      break;

    case PH_FADE:
      _biteR = APPLE_BITE_R;
      _appleGlow = 1.0f;
      _appleFade = 1.0f - easeInQuad(p);
      break;

    default:
      break;
  }

  // current/target interpolation -- this is what keeps the character motion
  // smooth even though the timeline snaps between phases
  smoothTowards(_currentX, _targetX, 0.22f);
  smoothTowards(_currentY, _targetY, 0.24f);
}

void BootAnimation::draw(M5Canvas& cv) {
  cv.fillScreen(COL_BG);
  if (_phase >= PH_DONE) return;

  // apple colour: grey -> white during the flare, then down to black
  uint16_t appleCol = mixColor(COL_APPLE, COL_WHITE, _appleGlow);
  appleCol = mixColor(COL_BG, appleCol, _appleFade);

  if (_appleGlow > 0.01f && _appleFade > 0.01f) {
    const float g = _appleGlow * _appleFade;
    cv.fillEllipse(APPLE_ORIGIN_X + APPLE_BMP_W / 2,
                   APPLE_ORIGIN_Y + APPLE_BODY_Y0 + APPLE_BODY_H / 2,
                   rr(APPLE_BODY_W * 0.60f + g * 10.0f),
                   rr(APPLE_BODY_H * 0.60f + g * 10.0f),
                   mixColor(COL_BG, COL_APPLE_HALO, g * 0.55f));
  }

  drawApple(cv, APPLE_ORIGIN_X, APPLE_ORIGIN_Y, appleCol, _biteR);

  // character on top -- it hides the bite while chomping, then walks off and
  // reveals it, which is what sells the "it just took a bite" read
  if (_currentX < 275.0f && _phase < PH_GLOW) {
    drawCharacter(cv, _currentX, _currentY + _hopY, _squash,
                  _mouth, _puff, _walk, _expr);
  }
}
