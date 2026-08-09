#include "Stars.h"
#include "PetState.h"

// Fixed positions, chosen by hand to frame the apple (which occupies roughly
// x 82..158, y 21..113) without sitting on top of the action.
static const struct { int16_t x, y; uint8_t r; } STAR_POS[STAR_COUNT] = {
  {  22,  30, 7 },
  {  34,  84, 5 },
  {  72, 118, 5 },
  { 120,  10, 5 },
  { 200,  24, 7 },
  { 226,  62, 5 },
  { 150, 124, 4 },
};

// A five-pointed star as a triangle fan from its centre: 10 alternating
// outer/inner vertices, 10 slices. Cheap enough at this size, and it can be
// drawn at any angle, which is what lets them spin.
static void drawStar5(M5Canvas& cv, float cx, float cy, float R, float ang,
                      uint16_t col) {
  if (R < 1.5f) return;
  int px[10], py[10];
  for (int i = 0; i < 10; ++i) {
    const float a = ang - HALF_PI + i * (PI / 5.0f);
    const float rr = (i & 1) ? R * 0.45f : R;
    px[i] = (int)lroundf(cx + cosf(a) * rr);
    py[i] = (int)lroundf(cy + sinf(a) * rr);
  }
  const int icx = (int)lroundf(cx), icy = (int)lroundf(cy);
  for (int i = 0; i < 10; ++i) {
    const int j = (i + 1) % 10;
    cv.fillTriangle(icx, icy, px[i], py[i], px[j], py[j], col);
  }
}

void Stars::begin() {
  for (int i = 0; i < STAR_COUNT; ++i) {
    _s[i].ang   = randRange(0.0f, TWO_PI);
    _s[i].spin  = randRange(0.7f, 1.5f) * ((i & 1) ? 1.0f : -1.0f);
    _s[i].phase = randRange(0.0f, TWO_PI);
  }
}

void Stars::draw(M5Canvas& cv, uint32_t now, float scale) {
  if (scale <= 0.02f) return;
  const float t = now * 0.001f;

  for (int i = 0; i < STAR_COUNT; ++i) {
    const float tw = 0.60f + 0.40f * sinf(t * 2.1f + _s[i].phase);
    drawStar5(cv, STAR_POS[i].x, STAR_POS[i].y,
              STAR_POS[i].r * (0.86f + 0.14f * tw),
              _s[i].ang + _s[i].spin * t,
              mixColor(COL_BG, COL_STAR, tw * scale));
  }
}
