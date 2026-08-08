// PetState.h -- top level app state plus the small math helpers everything
// else in the sketch shares.
#pragma once
#include <Arduino.h>
#include <stdint.h>

enum class AppState : uint8_t {
  BOOT,
  EYES,
  // reserved for later: CLOCK, WEATHER, WALKING, CHARGING, SLEEP
};

class PetState {
 public:
  void begin(uint32_t now);
  void set(AppState s, uint32_t now);
  AppState state() const { return _state; }
  uint32_t elapsed(uint32_t now) const { return now - _since; }

 private:
  AppState _state = AppState::BOOT;
  uint32_t _since = 0;
};

// ------------------------------------------------------------------ maths --
inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Fast start, soft landing. Used for saccades and most one-shot moves.
inline float easeOutCubic(float t) {
  t = clampf(t, 0.0f, 1.0f);
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

inline float easeInOutSine(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return 0.5f - 0.5f * cosf(t * PI);
}

inline float easeInQuad(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return t * t;
}

// Frame-rate independent exponential smoothing towards a target.
inline void smoothTowards(float& current, float target, float k) {
  current += (target - current) * k;
}

// Critically-ish damped spring. Overshoots a little, which is what makes the
// "surprised" pop and the return to normal feel alive.
inline void springTowards(float& current, float& velocity, float target,
                          float stiffness, float damping) {
  velocity += (target - current) * stiffness;
  velocity *= damping;
  current += velocity;
}

// ----------------------------------------------------------------- random --
inline float randf() { return (float)random(0, 10001) * 0.0001f; }

inline float randRange(float a, float b) { return a + (b - a) * randf(); }

inline uint32_t randMs(uint32_t a, uint32_t b) {
  return a + (uint32_t)random(0, (long)(b - a) + 1);
}

// ------------------------------------------------------------------ colour --
// Blend two RGB565 values. t=0 -> a, t=1 -> b.
uint16_t mixColor(uint16_t a, uint16_t b, float t);
