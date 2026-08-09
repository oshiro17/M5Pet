#include "Eyes.h"
#include "PetState.h"

// Where the gaze can wander, and how often each direction gets picked. Centre
// is heavily weighted so it keeps coming home instead of ping-ponging.
static const int8_t  GAZE_DIR[9][2] = {
  { 0,  0}, {-1,  0}, { 1,  0}, { 0, -1}, { 0,  1},
  {-1, -1}, { 1, -1}, {-1,  1}, { 1,  1},
};
static const uint8_t GAZE_WEIGHT[9] = { 30, 13, 13, 8, 8, 6, 6, 4, 4 };
static const int     GAZE_WEIGHT_SUM = 92;

// Styles the ambient scheduler may drift into, with weights. The dramatic ones
// are in here too, just rarely -- a pet that is only ever content is boring,
// one that is constantly furious is exhausting.
struct AmbientPick { uint8_t style; uint8_t weight; };
static const AmbientPick AMBIENT[] = {
  { STYLE_HAPPY,      14 },
  { STYLE_CURIOUS,    12 },
  { STYLE_DARTING,    10 },
  { STYLE_THINKING,    9 },
  { STYLE_SPARKLE,     8 },
  { STYLE_SHY,         7 },
  { STYLE_PUPIL_BIG,   6 },
  { STYLE_SMUG,        6 },
  { STYLE_EXCITED,     6 },
  { STYLE_JITO,        5 },
  { STYLE_DAZED,       5 },
  { STYLE_SUSPICIOUS,  4 },
  { STYLE_WINK,        4 },
  { STYLE_RELIEVED,    4 },
  { STYLE_TROUBLED,    3 },
  { STYLE_SURPRISED,   3 },
  { STYLE_SERIOUS,     2 },
  { STYLE_ANGRY,       2 },
  { STYLE_SAD,         2 },
  { STYLE_VOID,        2 },
  // the ones that move the whole pair -- these are the ones you notice
  { STYLE_TILTED,      7 },
  { STYLE_GLANCE_BACK, 6 },
  { STYLE_PEEK_LEFT,   5 },
  { STYLE_PEEK_DOWN_RIGHT, 5 },
  { STYLE_WIDE_EYED,   5 },
  { STYLE_FOCUS,       4 },
  { STYLE_LOPSIDED,    3 },
  { STYLE_CROSSEYED,   5 },
  { STYLE_DIZZY,       4 },
  { STYLE_HALF,        7 },
};
static const int AMBIENT_COUNT = (int)(sizeof(AMBIENT) / sizeof(AMBIENT[0]));

// One partially covered pixel along a lid edge. Filling lids column by column
// leaves a staircase wherever the lid is steep; blending the boundary pixel
// against what is underneath hides it completely for the cost of a read.
static void aaPixel(M5Canvas& cv, int x, int y, float cover) {
  if (cover <= 0.03f) return;
  if (y < 0 || y >= SCREEN_H) return;
  if (cover >= 0.97f) { cv.drawPixel(x, y, COL_BG); return; }
  cv.drawPixel(x, y, mixColor(cv.readPixel(x, y), COL_BG, cover));
}

// The spiral that shows up when the unit has been twirled too much. An
// Archimedean spiral (radius proportional to angle) drawn as a polyline inside
// the pupil, turning the same way the unit was spun.
static void drawSwirl(M5Canvas& cv, int px, int py, float rx, float ry,
                      float phase, float dir, uint16_t col) {
  const int   SEG   = 52;
  const float TURNS = 2.5f;   // fewer turns, because a fat stroke needs the room
  float ax = px, ay = py;
  for (int i = 1; i <= SEG; ++i) {
    const float u  = (float)i / (float)SEG;
    const float th = u * TURNS * TWO_PI * dir + phase;
    const float bx = px + cosf(th) * u * rx * 0.92f;
    const float by = py + sinf(th) * u * ry * 0.92f;
    // round brush: stamp the segment once per offset inside the brush radius
    for (int oy = -SWIRL_THICK; oy <= SWIRL_THICK; ++oy) {
      for (int ox = -SWIRL_THICK; ox <= SWIRL_THICK; ++ox) {
        if (ox * ox + oy * oy > SWIRL_THICK * SWIRL_THICK + 1) continue;
        cv.drawLine((int)ax + ox, (int)ay + oy, (int)bx + ox, (int)by + oy, col);
      }
    }
    ax = bx; ay = by;
  }
}

// ---------------------------------------------------------------------------
// Shape interpolation
// ---------------------------------------------------------------------------
// Width and height run on springs so a "surprised" pop overshoots and settles.
// Everything else eases straight in; a lid that wobbles looks broken, not alive.
static void approachShape(EyeShape& c, const EyeShape& t, float k,
                          float& vW, float& vH) {
  springTowards(c.w, vW, t.w, 0.30f, 0.62f);
  springTowards(c.h, vH, t.h, 0.30f, 0.62f);
  smoothTowards(c.topLid,   t.topLid,   k);
  smoothTowards(c.botLid,   t.botLid,   k);
  smoothTowards(c.lidTilt,  t.lidTilt,  k);
  smoothTowards(c.disc,     t.disc,     k);
  smoothTowards(c.blueFrom, t.blueFrom, k);
  smoothTowards(c.offsetY,  t.offsetY,  k);
  smoothTowards(c.offsetX,  t.offsetX,  k);
  smoothTowards(c.topCurve, t.topCurve, k);
  smoothTowards(c.botCurve, t.botCurve, k);
  smoothTowards(c.lidTiltBot, t.lidTiltBot, k);
  smoothTowards(c.innerSeal, t.innerSeal, k);
  smoothTowards(c.hiBig,    t.hiBig,    k);
  smoothTowards(c.hiSmall,  t.hiSmall,  k);
  smoothTowards(c.hiExtra,  t.hiExtra,  k);
}

// ---------------------------------------------------------------------------
void Eyes::begin(uint32_t now) {
  _styleIdx = _baseStyle = STYLE_NORMAL;
  _cur[0] = _tgt[0] = EYE_STYLES[STYLE_NORMAL].L;
  _cur[1] = _tgt[1] = EYE_STYLES[STYLE_NORMAL].R;
  _cur[0].topLid = _cur[1].topLid = 1.0f;      // starts shut: it wakes up
  _lastActivity = now;
  _lastFrameMs = now;
  _styleUntil = 0;
  _nextStyleAt = now + randMs(3500, 7000);
  scheduleGaze(now);
  scheduleBlink(now);
  _nextMicroAt = now + randMs(600, 1600);
}

void Eyes::applyStyle(bool instant) {
  _tgt[0] = EYE_STYLES[_styleIdx].L;
  _tgt[1] = EYE_STYLES[_styleIdx].R;
  if (instant) {
    _cur[0] = _tgt[0];
    _cur[1] = _tgt[1];
    _vW[0] = _vW[1] = _vH[0] = _vH[1] = 0.0f;
  }
}

void Eyes::setStyle(uint8_t idx, uint32_t now, bool instant) {
  if (idx >= EYE_STYLE_COUNT) idx = 0;
  _styleIdx = idx;
  _styleUntil = now + EYE_STYLES[idx].holdMs;
  applyStyle(instant);
  // re-aim immediately so a fixed-gaze style takes effect at once
  scheduleGaze(now);
}

void Eyes::wake(uint32_t now) {
  _lastActivity = now;
  if (_baseStyle != STYLE_NORMAL) {
    _baseStyle = STYLE_NORMAL;
    setStyle(STYLE_NORMAL, now);
  }
}

void Eyes::surprise(uint32_t now, float dirX, float dirY) {
  _lastActivity = now;
  _baseStyle = STYLE_NORMAL;
  setStyle(STYLE_SURPRISED, now);
  // setStyle re-aims, so override the gaze afterwards: snap towards whatever
  // just moved and hold there for a beat.
  _gazeFromX = _gazeX;
  _gazeFromY = _gazeY;
  _gazeToX = clampf(dirX, -1.0f, 1.0f);
  _gazeToY = clampf(dirY, -1.0f, 1.0f);
  _gazeT0 = now;
  _gazeDur = 90;
  _nextGazeAt = now + 900;
}

// ---------------------------------------------------------------------------
void Eyes::scheduleGaze(uint32_t now) {
  const EyeStyle& st = EYE_STYLES[_styleIdx];

  int r = (int)random(0, GAZE_WEIGHT_SUM);
  int idx = 0;
  for (int i = 0; i < 9; ++i) {
    r -= GAZE_WEIGHT[i];
    if (r < 0) { idx = i; break; }
  }
  const bool toCentre = (GAZE_DIR[idx][0] == 0 && GAZE_DIR[idx][1] == 0);
  const float mag = toCentre ? 0.0f : randRange(0.45f, 1.0f);
  _roamX = GAZE_DIR[idx][0] * mag;
  _roamY = GAZE_DIR[idx][1] * mag * 0.85f;

  // Style bias plus however much wandering this style allows. One vector, used
  // by both eyes.
  _gazeFromX = _gazeX;
  _gazeFromY = _gazeY;
  _gazeToX = clampf(st.gazeX + _roamX * st.gazeRoam, -1.0f, 1.0f);
  _gazeToY = clampf(st.gazeY + _roamY * st.gazeRoam, -1.0f, 1.0f);

  // Saccade timing: bigger jumps take longer, but not proportionally, so large
  // moves read as "flick and settle" while small ones are just quick.
  const float dx = _gazeToX - _gazeFromX;
  const float dy = _gazeToY - _gazeFromY;
  const float dist = sqrtf(dx * dx + dy * dy);
  float dur  = (70.0f + dist * 130.0f) * (0.6f + 0.4f * st.saccadeScale);
  float hold = (toCentre ? randRange(700.0f, 2500.0f)
                         : randRange(350.0f, 1500.0f)) * st.saccadeScale;

  _gazeT0 = now;
  _gazeDur = (uint32_t)dur;
  if (_gazeDur < 1) _gazeDur = 1;
  _nextGazeAt = now + _gazeDur + (uint32_t)hold;
}

void Eyes::scheduleBlink(uint32_t now) {
  const float s = EYE_STYLES[_styleIdx].blinkScale;
  _nextBlinkAt = now + (uint32_t)(randMs(3000, 8000) * s);
}

void Eyes::scheduleAmbient(uint32_t now) {
  _nextStyleAt = now + randMs(3500, 8000);

  int total = 0;
  for (int i = 0; i < AMBIENT_COUNT; ++i) total += AMBIENT[i].weight;
  int r = (int)random(0, total + 55);          // the slack = "stay as you are"
  for (int i = 0; i < AMBIENT_COUNT; ++i) {
    r -= AMBIENT[i].weight;
    if (r < 0) {
      if (AMBIENT[i].style < EYE_STYLE_COUNT) setStyle(AMBIENT[i].style, now);
      return;
    }
  }
}

// 1.0 -> 0.0 -> 1.0 openness. Closing is quick, the shut moment is brief,
// reopening starts fast and eases into place.
float Eyes::blinkCurve(float u, bool sleepy) {
  if (sleepy) {
    // heavy: closes unhurriedly, dwells shut, drags itself back open
    if (u < 0.22f) return 1.0f - easeInOutSine(u / 0.22f);
    if (u < 0.72f) return 0.0f;                 // half the blink is spent shut
    return easeInOutSine((u - 0.72f) / 0.28f);
  }
  if (u < 0.40f) return 1.0f - easeInOutSine(u / 0.40f);
  if (u < 0.50f) return 0.0f;
  return easeOutCubic((u - 0.50f) / 0.50f);
}

// ---------------------------------------------------------------------------
void Eyes::updateImu(uint32_t now) {
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    smoothTowards(_imuBlend, 0.0f, 0.06f);
    _imuMode = false;
    return;
  }
  // --- motion: how much the acceleration changed since last frame ---------
  if (!_imuPrimed) {
    _prevAx = ax; _prevAy = ay; _prevAz = az;
    _imuPrimed = true;
  }
  const float dax = ax - _prevAx, day = ay - _prevAy, daz = az - _prevAz;
  _prevAx = ax; _prevAy = ay; _prevAz = az;
  const float jerk = sqrtf(dax * dax + day * day + daz * daz);

  if (jerk > IMU_MOTION_WAKE) _lastActivity = now;   // being handled = awake

  // --- spin: sustained rotation piles up until it makes the eyes swim ------
  {
    float gxr = 0.0f, gyr = 0.0f, gzr = 0.0f;
    if (M5.Imu.getGyro(&gxr, &gyr, &gzr)) {
      const float dps = sqrtf(gxr * gxr + gyr * gyr + gzr * gzr);
      const float dt = 1.0f / (float)TARGET_FPS;
      if (dps > SPIN_MIN_DPS) _spin += (dps - SPIN_MIN_DPS) * dt;
      _spin -= SPIN_DECAY_DPS * dt;
      _spin = clampf(_spin, 0.0f, SPIN_MAX);
      if (_spin > SPIN_TRIGGER) {
        _dizzyUntil = now + DIZZY_MS;
        // turn the spiral the way it was actually spun
        const float dom = (fabsf(gzr) > fabsf(gxr)) ? gzr : gxr;
        _swirlDir = (dom >= 0.0f) ? 1.0f : -1.0f;
        _lastActivity = now;
      }
    }
  }
  const bool dizzy = (now < _dizzyUntil);
  if (dizzy) return;                       // no startles while the room spins

  if (jerk > IMU_MOTION_STARTLE && now >= _nextStartleAt) {
    // look the way it was just shoved
    float dx = IMU_SWAP_AXES ? dax : day;
    float dy = IMU_SWAP_AXES ? day : dax;
    dx *= IMU_SIGN_X;
    dy *= IMU_SIGN_Y;
    const float m = sqrtf(dx * dx + dy * dy);
    if (m > 0.0001f) { dx /= m; dy /= m; } else { dx = dy = 0.0f; }
    _nextStartleAt = now + IMU_STARTLE_COOLDOWN_MS;
    surprise(now, dx, dy * 0.8f);
    return;                                          // the startle wins this frame
  }

  // --- tilt: which way up it is being held --------------------------------
  float rawX = IMU_SWAP_AXES ? ax : ay;
  float rawY = IMU_SWAP_AXES ? ay : ax;
  rawX = clampf(rawX * IMU_SIGN_X * IMU_GAIN, -1.0f, 1.0f);
  rawY = clampf(rawY * IMU_SIGN_Y * IMU_GAIN, -1.0f, 1.0f);
  smoothTowards(_imuX, rawX, IMU_LOWPASS);
  smoothTowards(_imuY, rawY, IMU_LOWPASS);

  const float mag = sqrtf(_imuX * _imuX + _imuY * _imuY);
  if (!_imuMode) {
    if (mag > IMU_TAKEOVER) { _imuMode = true; _imuCalmSince = 0; }
  } else if (mag < IMU_RELEASE) {
    if (_imuCalmSince == 0) {
      _imuCalmSince = now;
    } else if (now - _imuCalmSince > IMU_RELEASE_HOLD_MS) {
      _imuMode = false; _imuCalmSince = 0; _nextGazeAt = now + 250;
    }
  } else {
    _imuCalmSince = 0;
  }
  // A fixed-gaze style (LOOK_LEFT, THINKING...) should not be overruled.
  const float allow = EYE_STYLES[_styleIdx].gazeRoam;
  smoothTowards(_imuBlend, _imuMode ? allow : 0.0f, 0.08f);
}

// The power-button lids. Linear rather than eased, because the user is holding
// a button and wants to see steady progress; the right eye lags slightly.
void Eyes::updateSleepLids(float dtSec) {
  const float step = dtSec * 1000.0f / (float)POWER_SLEEP_MS;
  if (_powerHeld) {
    _sleepL = clampf(_sleepL + step,         0.0f, 1.0f);
    _sleepR = clampf(_sleepR + step * 0.86f, 0.0f, 1.0f);
  } else {
    // let go and it drifts back open, slower than it closed
    _sleepL = clampf(_sleepL - step * 0.62f, 0.0f, 1.0f);
    _sleepR = clampf(_sleepR - step * 0.58f, 0.0f, 1.0f);
  }
}

void Eyes::setPowerHold(bool held, uint32_t now) {
  if (held && !_powerHeld) _lastActivity = now;
  _powerHeld = held;
}

// ---------------------------------------------------------------------------
void Eyes::update(uint32_t now) {
  const float dt = (float)(now - _lastFrameMs) * 0.001f;
  _lastFrameMs = now;
  _wobbleT += (dt > 0.2f ? 0.2f : dt);
  updateSleepLids(dt > 0.2f ? 0.2f : dt);

  const bool dozing = (_sleepL > 0.02f);

  if (!dozing) updateImu(now);

  // --- spun too much: pin the dizzy face and fade the spiral in -----------
  const bool dizzy = (now < _dizzyUntil);
  _swirlPhase += (dt > 0.2f ? 0.2f : dt) * SWIRL_SPEED;
  smoothTowards(_swirl, dizzy ? 1.0f : 0.0f, 0.10f);
  if (dizzy && _styleIdx != STYLE_DIZZY) {
    _baseStyle = STYLE_NORMAL;
    setStyle(STYLE_DIZZY, now);
  }

  // --- expression --------------------------------------------------------
  if (!dozing && !dizzy) {
    if (_styleUntil && now >= _styleUntil && _styleIdx != _baseStyle) {
      setStyle(_baseStyle, now);
      _styleUntil = 0;
    }
    // while dozing, stop drifting through expressions -- it should hold still
    if (now >= _nextStyleAt && _baseStyle == STYLE_NORMAL) scheduleAmbient(now);
    else if (now >= _nextStyleAt) _nextStyleAt = now + 2000;

    // idle slide towards sleep, one step at a time
    // Left alone it settles at HALF and stays there. No third step: the point
    // is that it looks asleep until something actually moves it.
    const uint32_t idle = now - _lastActivity;
    uint8_t want = STYLE_NORMAL;
    if (idle > EYE_SLEEPY_AFTER_MS) want = STYLE_SLEEPY;
    if (want != _baseStyle) {
      _baseStyle = want;
      setStyle(want, now);
    }
  }
  applyStyle(false);

  // --- gaze --------------------------------------------------------------
  if (dozing) {
    _nextGazeAt = now + 400;                 // eyes settle while closing
  } else if (_imuBlend > 0.5f) {
    _nextGazeAt = now + 400;                 // don't fight the IMU
  } else if (now >= _nextGazeAt) {
    scheduleGaze(now);
  }
  {
    const float u = (float)(now - _gazeT0) / (float)_gazeDur;
    const float e = easeOutCubic(u);
    _gazeX = lerpf(_gazeFromX, _gazeToX, e);
    _gazeY = lerpf(_gazeFromY, _gazeToY, e);
  }

  // --- micro drift: a couple of pixels now and then, never a constant tremor
  if (now >= _nextMicroAt) {
    if (_microToX != 0.0f || _microToY != 0.0f) {
      _microToX = _microToY = 0.0f;
      _nextMicroAt = now + randMs(900, 2600);
    } else {
      _microToX = randRange(-0.035f, 0.035f);
      _microToY = randRange(-0.030f, 0.030f);
      _nextMicroAt = now + randMs(220, 500);
    }
  }
  smoothTowards(_microX, _microToX, 0.25f);
  smoothTowards(_microY, _microToY, 0.25f);

  // --- blink -------------------------------------------------------------
  const bool sleepy = (_baseStyle != STYLE_NORMAL);
  if (dozing) {
    _blinking = false;
    _blink[0] = _blink[1] = 0.0f;
    _nextBlinkAt = now + 1500;
  } else {
    if (!_blinking && now >= _nextBlinkAt) {
      _blinking = true;
      _blinkT0 = now;
      _blinkDepth = 1.0f;
      if (_blinkPairPending) {
        _blinkPairPending = false;
        if (sleepy) {                       // the follow-up always shuts fully
          _blinkKind = BlinkKind::SLOW;
          _blinkDur = randMs(1400, 2200);
        } else {
          _blinkKind = BlinkKind::DOUBLE_2ND;
          _blinkDur = randMs(150, 200);
        }
      } else if (sleepy) {
        // Three flavours, so dozing never looks like a loop: one that goes all
        // the way down to a bar, one that only gets halfway and gives up, and
        // one that gets halfway, opens, then shuts properly a moment later.
        _blinkKind = BlinkKind::SLOW;
        _blinkDur = randMs(1100, 1800);
        const int r = (int)random(0, 100);
        if (r < 40) {
          _blinkDepth = 1.0f;
        } else if (r < 75) {
          _blinkDepth = randRange(0.40f, 0.70f);
          _blinkPairPending = true;
        } else {
          _blinkDepth = randRange(0.28f, 0.55f);
        }
      } else {
        const int r = (int)random(0, 100);
        if (r < 15) {                       // a long, heavy-lidded one
          _blinkKind = BlinkKind::SLOW;
          _blinkDur = randMs(430, 620);
        } else {
          _blinkKind = BlinkKind::NORMAL;
          _blinkDur = randMs(180, 260);
          _blinkPairPending = (r >= 15 && r < 40);   // ~25%: blink twice
        }
      }
    }
    if (_blinking) {
      const float u = (float)(now - _blinkT0) / (float)_blinkDur;
      // the right eye trails ~50 ms behind, so they never snap shut in unison
      const float uR = (float)((int32_t)(now - _blinkT0) - 50) / (float)_blinkDur;
      if (u >= 1.0f && uR >= 1.0f) {
        _blinking = false;
        _blink[0] = _blink[1] = 0.0f;
        if (_blinkPairPending) _nextBlinkAt = now + (sleepy ? randMs(260, 480)
                                                          : randMs(90, 140));
        else                   scheduleBlink(now);
      } else {
        const bool slow = (_blinkKind == BlinkKind::SLOW);
        _blink[0] = (1.0f - blinkCurve(u, slow)) * _blinkDepth;
        _blink[1] = (1.0f - blinkCurve(uR < 0.0f ? 0.0f : uR, slow)) * _blinkDepth;
      }
    }
  }

  // --- one gaze vector, folded together here so draw() cannot re-derive it
  //     differently for the two eyes ---------------------------------------
  {
    float gx = _gazeX + _microX;
    float gy = _gazeY + _microY;
    gx = lerpf(gx, _imuX, _imuBlend);
    gy = lerpf(gy, _imuY, _imuBlend);
    if (_sleepL > 0.02f) gy = lerpf(gy, 0.55f, _sleepL);   // looks down as it dozes
    if (_swirl > 0.02f) {                 // pupils roll round together
      const float a = _swirlPhase * 0.55f * _swirlDir;
      gx = lerpf(gx, cosf(a) * 0.55f, _swirl);
      gy = lerpf(gy, sinf(a) * 0.55f, _swirl);
    }
    const float mag = sqrtf(gx * gx + gy * gy);
    if (mag > 1.0f) { gx /= mag; gy /= mag; }
    _gxOut = gx;
    _gyOut = gy;
  }

  // --- the pair translates: pupils lead, the "head" catches up -------------
  {
    const EyeStyle& st = EYE_STYLES[_styleIdx];
    float hx = _gxOut * EYE_HEAD_FOLLOW_X + st.headX
             + _imuX * _imuBlend * EYE_IMU_SLIDE_X;
    float hy = _gyOut * EYE_HEAD_FOLLOW_Y + st.headY
             + _imuY * _imuBlend * EYE_IMU_SLIDE_Y;
    if (_sleepL > 0.02f) {                 // settles square as it drops off
      hx *= (1.0f - _sleepL);
      hy += _sleepL * 5.0f;
    }
    smoothTowards(_headX, clampf(hx, -EYE_HEAD_MAX_X, EYE_HEAD_MAX_X), EYE_HEAD_LAG);
    smoothTowards(_headY, clampf(hy, -EYE_HEAD_MAX_Y, EYE_HEAD_MAX_Y), EYE_HEAD_LAG);
    smoothTowards(_spread, st.spread, 0.07f);
    smoothTowards(_converge, st.converge, 0.09f);
    smoothTowards(_wobble, st.wobble, 0.06f);
  }

  smoothTowards(_zzz, (_baseStyle == STYLE_SLEEPY && !dizzy) ? 1.0f : 0.0f, 0.05f);

  // --- shapes ------------------------------------------------------------
  approachShape(_cur[0], _tgt[0], 0.16f, _vW[0], _vH[0]);
  approachShape(_cur[1], _tgt[1], 0.16f, _vW[1], _vH[1]);
}

// ---------------------------------------------------------------------------
void Eyes::draw(M5Canvas& cv) {
  // _gxOut/_gyOut and the head offset were settled in update(); both eyes read
  // the same numbers here, so they always aim together however far apart the
  // shapes drift.
  const int cxL = EYE_L_CX + (int)lroundf(_headX - _spread + _cur[0].offsetX);
  const int cxR = EYE_R_CX + (int)lroundf(_headX + _spread + _cur[1].offsetX);
  drawOneEye(cv, true,  cxL, _sleepL);
  drawOneEye(cv, false, cxR, _sleepR);
  drawSleepZ(cv, millis());

}

void Eyes::drawOneEye(M5Canvas& cv, bool isLeft, int cx, float sleep) {
  const EyeShape& s = _cur[isLeft ? 0 : 1];
  const float gx = _gxOut, gy = _gyOut;

  // Looking sideways turns the eyeball away from us, so narrow it a little.
  const float rx = EYE_BASE_W * 0.5f * s.w * (1.0f - EYE_SIDE_SQUASH * fabsf(gx));
  const float ry = EYE_BASE_H * 0.5f * s.h;
  const int   cy = EYE_CY + (int)lroundf(_headY + s.offsetY);
  if (rx < 2.0f || ry < 2.0f) return;

  // Blinking and the power-off lids both push the lids in on top of whatever
  // the expression already asked for. The upper lid does most of the travel.
  const float bk = _blink[isLeft ? 0 : 1];
  const float tl = clampf(s.topLid + bk * 0.74f + sleep * 0.88f, 0.0f, 1.0f);
  const float bl = clampf(s.botLid + bk * 0.26f + sleep * 0.12f, 0.0f, 1.0f);

  // Slow drift, out of phase between the two eyes: the slant rocks back and
  // forth and each eye takes a turn being the more open one. This is what
  // keeps a half-lidded face from freezing into one pose.
  const float wph   = _wobbleT * 0.55f + (isLeft ? 0.0f : 2.1f);
  const float wTilt = ry * 0.20f * _wobble * sinf(wph);
  const float wOpen = ry * 0.085f * _wobble * sinf(wph * 0.73f + 1.3f);
  const float wCurve = 0.07f * _wobble * sinf(wph * 0.41f + 0.6f);

  const float lidTopY = (cy - ry) + 2.0f * ry * tl + wOpen;
  const float lidBotY = (cy + ry) - 2.0f * ry * bl;

  // Lid slant. mirror keeps "inner corner" meaningful on both sides of the
  // face, which is what makes an angry brow angry rather than lopsided.
  const float mirror = isLeft ? 1.0f : -1.0f;
  const float tilt   = ry * 0.40f * s.lidTilt * mirror + wTilt;

  const int xL = (int)(cx - rx) - 2;
  const int xR = (int)(cx + rx) + 2;

  // ---- shut: a soft slanted bar, nothing else ---------------------------
  if (lidBotY - lidTopY < 5.0f) {
    const float midY = (lidTopY + lidBotY) * 0.5f;
    const int   w    = (int)(rx * 0.90f);
    const int   yA   = (int)lroundf(midY - tilt * 0.60f);
    const int   yB   = (int)lroundf(midY + tilt * 0.60f);
    cv.fillTriangle(cx - w, yA - 2, cx + w, yB - 2, cx + w, yB + 2, COL_WHITE);
    cv.fillTriangle(cx - w, yA - 2, cx + w, yB + 2, cx - w, yA + 2, COL_WHITE);
    return;
  }

  const int clipX = xL < 0 ? 0 : xL;
  const int clipW = (xR > SCREEN_W - 1 ? SCREEN_W - 1 : xR) - clipX + 1;
  if (clipW <= 0) return;
  cv.setClipRect(clipX, cy - (int)ry - 4, clipW, (int)(2 * ry) + 9);

  // ---- sclera ------------------------------------------------------------
  cv.fillEllipse(cx, cy, (int)rx, (int)ry, COL_WHITE);

  // ---- iris + pupil disc -------------------------------------------------
  float drx = rx * 0.52f * s.disc;
  float dry = ry * 0.62f * s.disc;
  if (drx < 2.0f) drx = 2.0f;
  if (dry < 2.0f) dry = 2.0f;

  float travelX = rx - drx - EYE_PUPIL_MARGIN; if (travelX < 0.0f) travelX = 0.0f;
  float travelY = ry - dry - EYE_PUPIL_MARGIN; if (travelY < 0.0f) travelY = 0.0f;

  // Cross-eye: one shared amount, mirrored inwards. Left goes right, right
  // goes left, always by the same distance.
  float offX = gx * travelX + (isLeft ? 1.0f : -1.0f) * _converge * travelX;
  offX = clampf(offX, -travelX, travelX);
  const int px = cx + (int)lroundf(offX);

  // A raised lower lid would slice the disc off, so ride up with it -- and
  // more importantly, keep the disc inside the aperture whatever the lids are
  // doing. An eye that is open but showing nothing except sclera is a blank
  // stare, not a half-lidded look, so the pupil slides down under the lid
  // instead of vanishing behind it.
  float pyF = cy + gy * travelY - bl * ry * 0.62f;
  const float pyLo = lidTopY - dry * 0.30f;
  const float pyHi = lidBotY + dry * 0.30f;
  pyF = (pyLo > pyHi) ? (lidTopY + lidBotY) * 0.5f : clampf(pyF, pyLo, pyHi);
  const int py = (int)lroundf(pyF);

  // As the spiral fades in, the iris and pupil are mixed towards the sclera
  // until they have dissolved into it and only the swirl is left.
  const float discFade = 1.0f - _swirl;
  cv.fillEllipse(px, py, (int)drx, (int)dry,
                 mixColor(COL_WHITE, COL_EYE_NAVY, discFade));

  // bright blue across the lower part of the disc, inset so a navy rim survives
  {
    const int by0 = (int)(py - dry + 2.0f * dry * s.blueFrom);
    const int by1 = (int)(py + dry);
    if (by1 > by0) {
      cv.setClipRect((int)(px - drx), by0, (int)(2 * drx) + 1, by1 - by0 + 1);
      cv.fillEllipse(px, py, (int)(drx * 0.86f), (int)(dry * 0.92f),
                     mixColor(COL_WHITE, COL_EYE_BLUE, discFade));
      cv.setClipRect(clipX, cy - (int)ry - 4, clipW, (int)(2 * ry) + 9);
    }
  }

  if (_swirl > 0.02f) {
    drawSwirl(cv, cx + (int)(gx * rx * 0.12f), cy + (int)(gy * ry * 0.12f),
              rx * 0.70f, ry * 0.72f, _swirlPhase, _swirlDir,
              mixColor(COL_WHITE, COL_SWIRL, _swirl));
  }

  // ---- highlights --------------------------------------------------------
  const float hiFade = 1.0f - _swirl;
  if (s.hiBig * hiFade > 0.03f) {
    int hrx = (int)(drx * 0.40f * s.hiBig * hiFade); if (hrx < 1) hrx = 1;
    int hry = (int)(dry * 0.28f * s.hiBig * hiFade); if (hry < 1) hry = 1;
    cv.fillEllipse(px - (int)(drx * 0.32f), py - (int)(dry * 0.34f),
                   hrx, hry, COL_WHITE);
  }
  if (s.hiSmall * hiFade > 0.03f) {
    int hrx = (int)(drx * 0.18f * s.hiSmall * hiFade); if (hrx < 1) hrx = 1;
    int hry = (int)(dry * 0.13f * s.hiSmall * hiFade); if (hry < 1) hry = 1;
    cv.fillEllipse(px + (int)(drx * 0.46f), py + (int)(dry * 0.42f),
                   hrx, hry, COL_WHITE);
  }
  if (s.hiExtra * hiFade > 0.03f) {
    int hrx = (int)(drx * 0.16f * s.hiExtra); if (hrx < 1) hrx = 1;
    int hry = (int)(dry * 0.12f * s.hiExtra); if (hry < 1) hry = 1;
    cv.fillEllipse(px + (int)(drx * 0.14f), py - (int)(dry * 0.66f),
                   hrx, hry, COL_WHITE);
  }

  cv.clearClipRect();

  // ---- lids ---------------------------------------------------------------
  // Painted column by column rather than as polygons, so both lids can carry a
  // curve and a slant at the same time. y = base -/+ curve*t^2 +/- tilt*t, with
  // t running -1..1 across the eye: the t^2 term rounds the lid, the t term
  // lifts one corner and drops the other.
  {
    const float curveTop = ry * (LID_TOP_CURVE + s.topCurve + wCurve);
    const float curveBot = ry * (LID_BOT_CURVE + s.botCurve);
    const float tiltBot  = ry * 0.30f * s.lidTiltBot * mirror;
    const int   yLimTop  = cy - (int)ry - 5;
    const int   yLimBot  = cy + (int)ry + 5;

    for (int x = xL; x <= xR; ++x) {
      if (x < 0 || x >= SCREEN_W) continue;
      float t = (x - cx) / rx;
      if (t < -1.0f) t = -1.0f; else if (t > 1.0f) t = 1.0f;
      const float tt = t * t;
      float ytf = lidTopY - curveTop * tt + tilt * t;
      const float ybf = lidBotY + curveBot * tt + tiltBot * t;
      // Seal the inner corner: weight rises to 1 at the inner edge, so the
      // upper lid slides down onto the lower one and the two lines meet in a
      // point instead of leaving a gap.
      float wi = t * mirror;
      if (wi > 0.0f) ytf += (ybf - ytf) * s.innerSeal * wi * wi;
      const int yt = (int)floorf(ytf);
      const int yb = (int)floorf(ybf);
      if (yt > yLimTop) cv.drawFastVLine(x, yLimTop, yt - yLimTop, COL_BG);
      aaPixel(cv, x, yt, ytf - yt);
      if (yb + 1 <= yLimBot) cv.drawFastVLine(x, yb + 1, yLimBot - yb, COL_BG);
      aaPixel(cv, x, yb, 1.0f - (ybf - yb));
    }
  }
}

// Little z's drifting up out of the gap between the eyes while it dozes. Three
// of them, out of phase, each growing and fading as it rises.
void Eyes::drawSleepZ(M5Canvas& cv, uint32_t now) {
  if (_zzz < 0.03f) return;
  for (int i = 0; i < 3; ++i) {
    float u = now * 0.00034f + i * 0.334f;
    u -= floorf(u);
    const float fade = (u < 0.12f) ? (u / 0.12f) : (1.0f - (u - 0.12f) / 0.88f);
    const float a = _zzz * clampf(fade, 0.0f, 1.0f) * 0.92f;
    if (a < 0.05f) continue;
    const int size = 1 + (int)(u * 2.4f);          // small near the face, big up top
    cv.setTextSize(size);
    cv.setTextColor(mixColor(COL_BG, COL_WHITE, a));
    cv.drawString(size >= 3 ? "Z" : "z",
                  104 + (int)(u * 26.0f),
                  106 - (int)(u * 82.0f));
  }
  cv.setTextSize(1);
}
