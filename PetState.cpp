#include "PetState.h"

void PetState::begin(uint32_t now) {
  _state = AppState::BOOT;
  _since = now;
}

void PetState::set(AppState s, uint32_t now) {
  if (_state == s) return;
  _state = s;
  _since = now;
}

uint16_t mixColor(uint16_t a, uint16_t b, float t) {
  t = clampf(t, 0.0f, 1.0f);
  const int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  const int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  const int r = (int)(ar + (br - ar) * t + 0.5f);
  const int g = (int)(ag + (bg - ag) * t + 0.5f);
  const int c = (int)(ab + (bb - ab) * t + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | c);
}
