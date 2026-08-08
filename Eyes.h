// Eyes.h -- the whole 240x135 panel is one face: two very large eyes.
//
// Every visual property is a current/target pair. Targets are set by the
// behaviour layer (gaze scheduler, blink, mood, IMU) and the current values
// chase them every frame, so nothing ever snaps.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

class Eyes {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  void draw(M5Canvas& cv);

  void surprise(uint32_t now);   // hook for a button, a loud noise, a shake...
  void wake(uint32_t now);       // reset the "getting sleepy" timer

 private:
  enum class Mood : uint8_t { NORMAL, INTEREST, SURPRISE, SMILE, SLEEPY };

  void scheduleGaze(uint32_t now);
  void scheduleBlink(uint32_t now);
  void scheduleMood(uint32_t now);
  void applyMood();
  void updateImu(uint32_t now);
  void drawOneEye(M5Canvas& cv, int cx, float lookX, float lookY);
  static float blinkCurve(float u);

  // ---- current / target pairs -------------------------------------------
  float currentEyeWidth   = 1.0f, targetEyeWidth   = 1.0f;
  float currentEyeHeight  = 1.0f, targetEyeHeight  = 1.0f;
  float currentPupilScale = 1.0f, targetPupilScale = 1.0f;
  float currentEyeOpen    = 0.0f, targetEyeOpen    = 1.0f;  // starts shut: it "wakes up"
  float currentLookX      = 0.0f, targetLookX      = 0.0f;  // -1..1
  float currentLookY      = 0.0f, targetLookY      = 0.0f;
  float currentSmile      = 0.0f, targetSmile      = 0.0f;

  // spring velocities for the size channels (these are what overshoot)
  float _vW = 0.0f, _vH = 0.0f, _vP = 0.0f;

  // ---- saccade ------------------------------------------------------------
  float    _gazeFromX = 0.0f, _gazeFromY = 0.0f;
  uint32_t _gazeT0 = 0, _gazeDur = 1, _nextGazeAt = 0;

  // ---- micro drift --------------------------------------------------------
  float    _microX = 0.0f, _microY = 0.0f;
  float    _microTargetX = 0.0f, _microTargetY = 0.0f;
  uint32_t _nextMicroAt = 0;

  // ---- blink --------------------------------------------------------------
  bool     _blinking = false;
  uint32_t _blinkT0 = 0, _blinkDur = 220, _nextBlinkAt = 0;
  uint8_t  _blinkQueue = 0;

  // ---- mood ---------------------------------------------------------------
  Mood     _mood = Mood::NORMAL;
  uint32_t _moodUntil = 0, _nextMoodAt = 0;
  uint32_t _lastActivity = 0;

  // ---- imu ----------------------------------------------------------------
  float    _imuX = 0.0f, _imuY = 0.0f, _imuBlend = 0.0f;
  bool     _imuMode = false;
  uint32_t _imuCalmSince = 0;
};
