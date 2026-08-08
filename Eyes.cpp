#include "Eyes.h"
#include "PetState.h"

// Where the eyes can look, and how often each direction gets picked. Centre is
// heavily weighted so the gaze keeps coming home instead of ping-ponging.
static const int8_t  GAZE_DIR[9][2] = {
  { 0,  0}, {-1,  0}, { 1,  0}, { 0, -1}, { 0,  1},
  {-1, -1}, { 1, -1}, {-1,  1}, { 1,  1},
};
static const uint8_t GAZE_WEIGHT[9] = { 30, 13, 13, 8, 8, 6, 6, 4, 4 };
static const int     GAZE_WEIGHT_SUM = 92;

// ---------------------------------------------------------------------------
void Eyes::begin(uint32_t now) {
  _lastActivity = now;
  currentEyeOpen = 0.0f;
  targetEyeOpen = 1.0f;
  _mood = Mood::NORMAL;
  applyMood();
  scheduleGaze(now);
  scheduleBlink(now);
  _nextMoodAt = now + randMs(4000, 8000);
  _nextMicroAt = now + randMs(600, 1600);
}

void Eyes::wake(uint32_t now) {
  _lastActivity = now;
  if (_mood == Mood::SLEEPY) {
    _mood = Mood::NORMAL;
    applyMood();
  }
}

void Eyes::surprise(uint32_t now) {
  _lastActivity = now;
  _mood = Mood::SURPRISE;
  _moodUntil = now + randMs(220, 560);
  applyMood();
  // a startle also snaps the gaze forward
  _gazeFromX = currentLookX;
  _gazeFromY = currentLookY;
  targetLookX = 0.0f;
  targetLookY = 0.0f;
  _gazeT0 = now;
  _gazeDur = 90;
  _nextGazeAt = now + 700;
}

// ---------------------------------------------------------------------------
void Eyes::applyMood() {
  switch (_mood) {
    case Mood::NORMAL:
      targetEyeWidth = 1.00f; targetEyeHeight = 1.00f;
      targetPupilScale = 1.00f; targetEyeOpen = 1.00f; targetSmile = 0.0f;
      break;
    case Mood::INTEREST:                   // "ooh, what's that"
      targetEyeWidth = 1.08f; targetEyeHeight = 1.12f;
      targetPupilScale = 1.22f; targetEyeOpen = 1.00f; targetSmile = 0.0f;
      break;
    case Mood::SURPRISE:                   // eyes pop, pupils shrink
      targetEyeWidth = 1.22f; targetEyeHeight = 1.30f;
      targetPupilScale = 0.74f; targetEyeOpen = 1.00f; targetSmile = 0.0f;
      break;
    case Mood::SMILE:                      // lower lid lifts into a crescent
      targetEyeWidth = 1.02f; targetEyeHeight = 0.98f;
      targetPupilScale = 1.00f; targetEyeOpen = 1.00f; targetSmile = 1.0f;
      break;
    case Mood::SLEEPY:                     // upper lid sags, eyes get shorter
      targetEyeWidth = 0.98f; targetEyeHeight = 0.80f;
      targetPupilScale = 0.95f; targetEyeOpen = 0.62f; targetSmile = 0.0f;
      break;
  }
}

void Eyes::scheduleGaze(uint32_t now) {
  // weighted pick of one of the nine directions
  int r = (int)random(0, GAZE_WEIGHT_SUM);
  int idx = 0;
  for (int i = 0; i < 9; ++i) {
    r -= GAZE_WEIGHT[i];
    if (r < 0) { idx = i; break; }
  }

  const bool toCentre = (GAZE_DIR[idx][0] == 0 && GAZE_DIR[idx][1] == 0);
  const float mag = toCentre ? 0.0f : randRange(0.45f, 1.0f);
  _gazeFromX = currentLookX;
  _gazeFromY = currentLookY;
  targetLookX = GAZE_DIR[idx][0] * mag;
  targetLookY = GAZE_DIR[idx][1] * mag * 0.85f;   // vertical range is tighter

  // Saccade timing: bigger jumps take longer, but not proportionally, so large
  // moves read as "flick and settle" while small ones are just quick.
  const float dx = targetLookX - _gazeFromX;
  const float dy = targetLookY - _gazeFromY;
  const float dist = sqrtf(dx * dx + dy * dy);
  float dur = 70.0f + dist * 130.0f;
  float hold = toCentre ? randRange(700.0f, 2500.0f) : randRange(350.0f, 1500.0f);
  if (_mood == Mood::SLEEPY) { dur *= 2.0f; hold *= 1.6f; }

  _gazeT0 = now;
  _gazeDur = (uint32_t)dur;
  if (_gazeDur < 1) _gazeDur = 1;
  _nextGazeAt = now + _gazeDur + (uint32_t)hold;
}

void Eyes::scheduleBlink(uint32_t now) {
  _nextBlinkAt = now + randMs(3000, 8000);
}

void Eyes::scheduleMood(uint32_t now) {
  _nextMoodAt = now + randMs(4000, 9000);
  if (_mood == Mood::SLEEPY) return;

  const int r = (int)random(0, 100);
  if (r < 25) {
    _mood = Mood::INTEREST;
    _moodUntil = now + randMs(500, 900);
  } else if (r < 40) {
    _mood = Mood::SMILE;
    _moodUntil = now + randMs(700, 1300);
  } else if (r < 45) {
    surprise(now);
    return;
  } else {
    return;                     // most of the time: nothing happens
  }
  applyMood();
}

// 1.0 -> 0.0 -> 1.0. Closing is quick, the shut moment is brief, reopening
// starts fast and eases into place.
float Eyes::blinkCurve(float u) {
  if (u < 0.40f)  return 1.0f - easeInOutSine(u / 0.40f);
  if (u < 0.50f)  return 0.0f;
  return easeOutCubic((u - 0.50f) / 0.50f);
}

// ---------------------------------------------------------------------------
void Eyes::updateImu(uint32_t now) {
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) {
    // no IMU on this unit -- fall back to purely random gaze
    smoothTowards(_imuBlend, 0.0f, 0.06f);
    _imuMode = false;
    return;
  }

  float rawX = IMU_SWAP_AXES ? ax : ay;
  float rawY = IMU_SWAP_AXES ? ay : ax;
  rawX = clampf(rawX * IMU_SIGN_X * IMU_GAIN, -1.0f, 1.0f);
  rawY = clampf(rawY * IMU_SIGN_Y * IMU_GAIN, -1.0f, 1.0f);

  // low-pass, so the eyes glide instead of twitching with the sensor noise
  smoothTowards(_imuX, rawX, IMU_LOWPASS);
  smoothTowards(_imuY, rawY, IMU_LOWPASS);

  const float mag = sqrtf(_imuX * _imuX + _imuY * _imuY);
  if (!_imuMode) {
    if (mag > IMU_TAKEOVER) {          // tilted enough: IMU takes the gaze over
      _imuMode = true;
      _imuCalmSince = 0;
      _lastActivity = now;
    }
  } else {
    if (mag < IMU_RELEASE) {           // held level for a while: hand it back
      if (_imuCalmSince == 0) {
        _imuCalmSince = now;
      } else if (now - _imuCalmSince > IMU_RELEASE_HOLD_MS) {
        _imuMode = false;
        _imuCalmSince = 0;
        _nextGazeAt = now + 250;
      }
    } else {
      _imuCalmSince = 0;
    }
  }
  smoothTowards(_imuBlend, _imuMode ? 1.0f : 0.0f, 0.08f);
}

// ---------------------------------------------------------------------------
void Eyes::update(uint32_t now) {
  updateImu(now);

  // --- mood -------------------------------------------------------------
  if (_mood != Mood::NORMAL && _mood != Mood::SLEEPY && now >= _moodUntil) {
    _mood = Mood::NORMAL;
    applyMood();
  }
  if (now >= _nextMoodAt) scheduleMood(now);

  if (_mood == Mood::NORMAL && (now - _lastActivity) > EYE_SLEEPY_AFTER_MS) {
    _mood = Mood::SLEEPY;
    applyMood();
  }

  // --- gaze -------------------------------------------------------------
  if (_imuBlend > 0.5f) {
    _nextGazeAt = now + 400;          // don't fight the IMU for the gaze
  } else if (now >= _nextGazeAt) {
    scheduleGaze(now);
  }
  {
    const float u = (float)(now - _gazeT0) / (float)_gazeDur;
    const float e = easeOutCubic(u);
    currentLookX = lerpf(_gazeFromX, targetLookX, e);
    currentLookY = lerpf(_gazeFromY, targetLookY, e);
  }

  // --- micro drift: a couple of pixels, now and then, never a constant tremor
  if (now >= _nextMicroAt) {
    if (_microTargetX != 0.0f || _microTargetY != 0.0f) {
      _microTargetX = _microTargetY = 0.0f;
      _nextMicroAt = now + randMs(900, 2600);
    } else {
      _microTargetX = randRange(-0.035f, 0.035f);
      _microTargetY = randRange(-0.030f, 0.030f);
      _nextMicroAt = now + randMs(220, 500);
    }
  }
  smoothTowards(_microX, _microTargetX, 0.25f);
  smoothTowards(_microY, _microTargetY, 0.25f);

  // --- blink ------------------------------------------------------------
  if (!_blinking && now >= _nextBlinkAt) {
    _blinking = true;
    _blinkT0 = now;
    _blinkDur = randMs(180, 260);
    if (_blinkQueue == 0 && random(0, 100) < 25) _blinkQueue = 1;  // double blink
  }
  if (_blinking) {
    const float u = (float)(now - _blinkT0) / (float)_blinkDur;
    if (u >= 1.0f) {
      _blinking = false;
      currentEyeOpen = targetEyeOpen;
      if (_blinkQueue > 0) {
        _blinkQueue--;
        _nextBlinkAt = now + 90;
      } else {
        scheduleBlink(now);
      }
    } else {
      currentEyeOpen = targetEyeOpen * blinkCurve(u);
    }
  } else {
    smoothTowards(currentEyeOpen, targetEyeOpen, 0.30f);
  }

  // --- size channels: springs, so surprise pops and settles with a wobble --
  springTowards(currentEyeWidth,   _vW, targetEyeWidth,   0.30f, 0.62f);
  springTowards(currentEyeHeight,  _vH, targetEyeHeight,  0.30f, 0.62f);
  springTowards(currentPupilScale, _vP, targetPupilScale, 0.28f, 0.64f);
  smoothTowards(currentSmile, targetSmile, 0.18f);
}

// ---------------------------------------------------------------------------
void Eyes::draw(M5Canvas& cv) {
  cv.fillScreen(COL_BG);
  const float lx = lerpf(currentLookX + _microX, _imuX, _imuBlend);
  const float ly = lerpf(currentLookY + _microY, _imuY, _imuBlend);
  drawOneEye(cv, EYE_L_CX, lx, ly);
  drawOneEye(cv, EYE_R_CX, lx, ly);
}

void Eyes::drawOneEye(M5Canvas& cv, int cx, float lookX, float lookY) {
  const float ew = EYE_BASE_W * currentEyeWidth;
  const float eh = EYE_BASE_H * currentEyeHeight;
  int rx = (int)(ew * 0.5f); if (rx < 1) rx = 1;
  int ry = (int)(eh * 0.5f); if (ry < 1) ry = 1;

  // Lids clip the eye instead of squashing it -- that is what makes a blink
  // look like a blink. The upper lid does most of the travel.
  const float closed = clampf(1.0f - currentEyeOpen, 0.0f, 1.0f);
  const int lidTop = (int)(EYE_CY - ry + closed * eh * 0.70f);
  const int lidBot = (int)(EYE_CY + ry - closed * eh * 0.30f);

  if (lidBot - lidTop < 6) {                       // shut: a soft horizontal bar
    const int y = (lidTop + lidBot) / 2;
    int w = (int)(ew * 0.86f); if (w < 4) w = 4;
    cv.fillRoundRect(cx - w / 2, y - 2, w, 5, 2, COL_WHITE);
    return;
  }

  const int bandH = lidBot - lidTop + 1;
  cv.setClipRect(cx - rx, lidTop, rx * 2 + 1, bandH);
  cv.fillEllipse(cx, EYE_CY, rx, ry, COL_WHITE);

  // ---- pupil
  int pw = (int)(EYE_PUPIL_W * currentPupilScale * currentEyeWidth);
  int ph = (int)(EYE_PUPIL_H * currentPupilScale * currentEyeHeight);
  int prx = pw / 2; if (prx < 2) prx = 2;
  int pry = ph / 2; if (pry < 2) pry = 2;

  float maxDX = (float)rx - prx - 4.0f; if (maxDX < 0.0f) maxDX = 0.0f;
  float maxDY = (float)ry - pry - 4.0f; if (maxDY < 0.0f) maxDY = 0.0f;
  const int px = cx + (int)(clampf(lookX, -1.0f, 1.0f) * maxDX);
  const int py = EYE_CY + (int)(clampf(lookY, -1.0f, 1.0f) * maxDY);

  cv.fillEllipse(px, py, prx, pry, COL_EYE_NAVY);

  // blue wash across the lower part of the pupil (clip = lid band AND lower pupil)
  {
    const int by0 = (lidTop > py + pry / 5) ? lidTop : py + pry / 5;
    const int by1 = (lidBot < py + pry)     ? lidBot : py + pry;
    if (by1 > by0) {
      cv.setClipRect(px - prx, by0, prx * 2 + 1, by1 - by0 + 1);
      cv.fillEllipse(px, py, prx, pry, COL_EYE_BLUE);
    }
  }

  // ---- highlights: one big, one small
  cv.setClipRect(cx - rx, lidTop, rx * 2 + 1, bandH);
  {
    int hrx = (int)(prx * 0.40f); if (hrx < 1) hrx = 1;
    int hry = (int)(pry * 0.28f); if (hry < 1) hry = 1;
    cv.fillEllipse(px - prx / 3, py - pry / 3, hrx, hry, COL_WHITE);
    int srx = (int)(prx * 0.18f); if (srx < 1) srx = 1;
    int sry = (int)(pry * 0.13f); if (sry < 1) sry = 1;
    cv.fillEllipse(px + prx / 2, py + pry / 2, srx, sry, COL_WHITE);
  }
  cv.clearClipRect();

  // ---- smiling eye: lift the lower edge into a crescent
  if (currentSmile > 0.02f) {
    const int lift = (int)(eh * 0.46f * currentSmile);
    int srx = (int)(rx * 1.35f); if (srx < 1) srx = 1;
    int sry = (int)(ry * 0.90f); if (sry < 1) sry = 1;
    cv.fillEllipse(cx, EYE_CY + ry + (int)(ry * 0.62f) - lift, srx, sry, COL_BG);
  }
}
