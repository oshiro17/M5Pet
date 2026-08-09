// Eyes.h -- the whole 240x135 panel is one face: two very large eyes.
//
// Three layers, kept separate on purpose:
//
//   EyeStyle  (EyeStyles.h)  what an expression looks like -- pure data
//   EyeState                 the current/target shapes plus the shared gaze
//   AnimState                the behaviour clock: saccades, blinks, moods,
//                            the idle slide into sleep, the power-off lids
//
// Anything a future screen (clock, weather, charging...) needs is here:
// setStyle() to pose the face, and the animation keeps running underneath.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"
#include "EyeStyles.h"

class Eyes {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  void draw(M5Canvas& cv);

  // ---- expression -------------------------------------------------------
  void        setStyle(uint8_t idx, uint32_t now, bool instant = false);
  uint8_t     style() const      { return _styleIdx; }
  const char* styleName() const  { return EYE_STYLES[_styleIdx].name; }

  // Startle. dirX/dirY is where to snap the gaze -- pass the direction the
  // thing that startled it came from, or leave it at zero for a plain jump.
  void surprise(uint32_t now, float dirX = 0.0f, float dirY = 0.0f);
  void wake(uint32_t now);       // reset the "getting sleepy" timer

  // ---- power button -----------------------------------------------------
  // Feed the power button state every frame. While held the eyes close over
  // POWER_SLEEP_MS; let go and they drift back open.
  void  setPowerHold(bool held, uint32_t now);
  uint32_t idleFor(uint32_t now) const { return now - _lastActivity; }
  float sleepAmount() const { return _sleepL; }
  bool  readyToPowerOff() const { return _sleepL >= 1.0f && _sleepR >= 1.0f; }

 private:
  // ---- EyeState ---------------------------------------------------------
  EyeShape _cur[2];              // 0 = left, 1 = right
  EyeShape _tgt[2];
  float    _vW[2] = {0, 0};      // spring velocities, so size pops overshoot
  float    _vH[2] = {0, 0};

  // The gaze is one vector for BOTH eyes. There is deliberately nowhere to
  // put a per-eye gaze, so the two can never disagree.
  float _gazeX = 0.0f, _gazeY = 0.0f;
  float _gazeFromX = 0.0f, _gazeFromY = 0.0f;
  float _gazeToX = 0.0f, _gazeToY = 0.0f;
  uint32_t _gazeT0 = 0, _gazeDur = 1, _nextGazeAt = 0;
  float _roamX = 0.0f, _roamY = 0.0f;      // scheduler's wander, before bias
  float _microX = 0.0f, _microY = 0.0f;
  float _microToX = 0.0f, _microToY = 0.0f;
  uint32_t _nextMicroAt = 0;

  // Final combined gaze for this frame, computed once in update() and used by
  // both eyes in draw(). There is one of each -- not one per eye.
  float _gxOut = 0.0f, _gyOut = 0.0f;

  // The pair translates as a unit: the pupils lead, this follows a beat later,
  // which is what turns a glance into a head turn. Shared, so it cannot skew
  // the two eyes relative to each other's aim.
  float _headX = 0.0f, _headY = 0.0f, _spread = 0.0f, _converge = 0.0f;
  float _wobble = 0.0f, _wobbleT = 0.0f;

  // ---- AnimState --------------------------------------------------------
  uint8_t  _styleIdx = STYLE_NORMAL;
  uint8_t  _baseStyle = STYLE_NORMAL;   // what to fall back to
  uint32_t _styleUntil = 0;
  uint32_t _nextStyleAt = 0;
  uint32_t _lastActivity = 0;

  enum class BlinkKind : uint8_t { NORMAL, SLOW, DOUBLE_2ND };
  bool      _blinking = false;
  float     _blink[2] = {0.0f, 0.0f};    // 0 = open, 1 = shut; right lags slightly
  uint32_t  _blinkT0 = 0, _blinkDur = 220, _nextBlinkAt = 0;
  float     _blinkDepth = 1.0f;          // 1 = shuts fully, <1 = only droops
  BlinkKind _blinkKind = BlinkKind::NORMAL;
  bool      _blinkPairPending = false;

  float    _sleepL = 0.0f, _sleepR = 0.0f;   // power-button lids, 0..1
  float    _zzz = 0.0f;                      // fade of the floating z's
  bool     _powerHeld = false;
  uint32_t _lastFrameMs = 0;

  // ---- imu --------------------------------------------------------------
  float _imuX = 0.0f, _imuY = 0.0f, _imuBlend = 0.0f;
  bool  _imuMode = false;
  uint32_t _imuCalmSince = 0;
  // motion detection, kept separate from tilt
  float _prevAx = 0.0f, _prevAy = 0.0f, _prevAz = 0.0f;
  bool  _imuPrimed = false;
  uint32_t _nextStartleAt = 0;
  // spun-too-much state
  float    _spin = 0.0f;          // accumulated rotation, degrees
  float    _swirl = 0.0f;         // 0..1 how much the spiral has faded in
  float    _swirlPhase = 0.0f;
  float    _swirlDir = 1.0f;
  uint32_t _dizzyUntil = 0;

  // ---- internals --------------------------------------------------------
  void applyStyle(bool instant);
  void scheduleGaze(uint32_t now);
  void scheduleBlink(uint32_t now);
  void scheduleAmbient(uint32_t now);
  void updateImu(uint32_t now);
  void updateSleepLids(float dtSec);
  void drawOneEye(M5Canvas& cv, bool isLeft, int cx, float sleep);
  void drawSleepZ(M5Canvas& cv, uint32_t now);
  static float blinkCurve(float u, bool sleepy);
};
