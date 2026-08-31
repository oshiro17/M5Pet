#include "Trail.h"
#include "PetState.h"

static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

// The three trail colours, cycled so no two consecutive sparks match.
static const uint16_t TRAIL_COLS[3] = {
  COL_WHITE, COL_TRAIL_AQUA, COL_TRAIL_PINK
};

// A five-pointed star as a fan of triangles from its centre. Same construction
// as Stars.cpp, but small and free to move, so it takes its radius as a float
// and bails out below ~1.5 px where the fan degenerates into noise.
static void starAt(M5Canvas& cv, float cx, float cy, float R, float ang,
                   uint16_t col) {
  if (R < 1.4f) { cv.drawPixel(ri(cx), ri(cy), col); return; }
  int px[10], py[10];
  for (int i = 0; i < 10; ++i) {
    const float a = ang - HALF_PI + i * (PI / 5.0f);
    const float r = (i & 1) ? R * 0.45f : R;
    px[i] = ri(cx + cosf(a) * r);
    py[i] = ri(cy + sinf(a) * r);
  }
  const int icx = ri(cx), icy = ri(cy);
  for (int i = 0; i < 10; ++i) {
    const int j = (i + 1) % 10;
    cv.fillTriangle(icx, icy, px[i], py[i], px[j], py[j], col);
  }
}

// Four-point sparkle: two tapered spikes crossed. Reads as a glint at sizes
// where a five-pointed star would just be a blob.
static void sparkleAt(M5Canvas& cv, float cx, float cy, float R, float ang,
                      uint16_t col) {
  const float c = cosf(ang), s = sinf(ang);
  const float lo = R * 0.30f;
  const int x = ri(cx), y = ri(cy);
  cv.fillTriangle(x, y, ri(cx + c * R - s * lo), ri(cy + s * R + c * lo),
                  ri(cx + c * R + s * lo), ri(cy + s * R - c * lo), col);
  cv.fillTriangle(x, y, ri(cx - c * R - s * lo), ri(cy - s * R + c * lo),
                  ri(cx - c * R + s * lo), ri(cy - s * R - c * lo), col);
  cv.fillTriangle(x, y, ri(cx - s * R - c * lo), ri(cy + c * R - s * lo),
                  ri(cx - s * R + c * lo), ri(cy + c * R + s * lo), col);
  cv.fillTriangle(x, y, ri(cx + s * R - c * lo), ri(cy - c * R - s * lo),
                  ri(cx + s * R + c * lo), ri(cy - c * R + s * lo), col);
}

void Trail::begin() {
  for (int i = 0; i < GAME_PARTICLES; ++i) _p[i].life = 0.0f;
  clear();
}

void Trail::clear() {
  _head = _count = 0;
  _emitAcc = 0.0f;
  for (int i = 0; i < GAME_PARTICLES; ++i) _p[i].life = 0.0f;
}

void Trail::spawn(float x, float y, float vx, float vy, float life, float size,
                  uint16_t col, uint8_t kind) {
  // Oldest-first replacement: _next just walks the array. A particle that is
  // still alive when its slot comes round is the one that has been visible
  // longest, so overwriting it is the least noticeable choice available.
  Particle& p = _p[_next];
  _next = (_next + 1) % GAME_PARTICLES;
  p.x = x; p.y = y; p.vx = vx; p.vy = vy;
  p.life = p.life0 = life;
  p.size = size;
  p.rot  = randRange(0.0f, TWO_PI);
  p.spin = randRange(-7.0f, 7.0f);
  p.col  = col;
  p.kind = kind;
}

void Trail::track(float x, float y, float speed, float intensity, float dt) {
  // Ribbon: one sample per frame. Because the samples are positions and not
  // times, a fast ball automatically leaves a longer ribbon.
  _pts[_head].x = x;
  _pts[_head].y = y;
  _pts[_head].w = GAME_BALL_R * (0.72f + 0.28f * intensity);
  _head = (_head + 1) % GAME_TRAIL_POINTS;
  if (_count < GAME_TRAIL_POINTS) ++_count;

  // Particles: emit at a rate proportional to speed, accumulating the
  // fractional remainder so slow motion still sparkles occasionally instead of
  // rounding down to nothing.
  const float rate = (2.0f + speed * 0.10f) * (0.6f + 0.8f * intensity);
  _emitAcc += rate * dt;
  while (_emitAcc >= 1.0f) {
    _emitAcc -= 1.0f;
    const float a  = randRange(0.0f, TWO_PI);
    const float sp = randRange(6.0f, 26.0f);
    const float life = randRange(0.28f, 0.62f) * (0.75f + 0.5f * intensity);
    const uint16_t col = TRAIL_COLS[random(0, 3)];
    // Mostly small sparkles with the occasional star, so the trail glitters
    // rather than turning into a stream of stars.
    const uint8_t kind = (random(0, 100) < 22) ? 0 : ((random(0, 100) < 70) ? 1 : 2);
    spawn(x + randRange(-3.0f, 3.0f), y + randRange(-3.0f, 3.0f),
          cosf(a) * sp, sinf(a) * sp - 8.0f,
          life, randRange(1.6f, 3.4f) * (0.8f + 0.4f * intensity), col, kind);
  }
}

void Trail::burst(float x, float y, uint16_t tint, int count, float power) {
  for (int i = 0; i < count; ++i) {
    const float a  = randRange(0.0f, TWO_PI);
    const float sp = randRange(30.0f, 110.0f) * power;
    const uint16_t col = (i % 3 == 0) ? tint : TRAIL_COLS[random(0, 3)];
    spawn(x, y, cosf(a) * sp, sinf(a) * sp,
          randRange(0.35f, 0.75f), randRange(2.0f, 4.4f), col,
          (i & 1) ? 0 : 1);
  }
}

void Trail::update(float dt) {
  for (int i = 0; i < GAME_PARTICLES; ++i) {
    Particle& p = _p[i];
    if (p.life <= 0.0f) continue;
    p.life -= dt;
    if (p.life <= 0.0f) continue;
    p.x  += p.vx * dt;
    p.y  += p.vy * dt;
    p.vy += 62.0f * dt;          // a little gravity, so sparks settle downwards
    p.vx *= 1.0f - 1.6f * dt;    // and drag, so they do not fly off forever
    p.vy *= 1.0f - 1.6f * dt;
    p.rot += p.spin * dt;
  }
}

void Trail::drawRibbon(M5Canvas& cv) const {
  // Walk from oldest to newest so the bright head is drawn last and sits on
  // top. t = 0 at the tail, 1 at the head.
  for (int i = 0; i < _count; ++i) {
    const int idx = (_head - _count + i + GAME_TRAIL_POINTS * 2) % GAME_TRAIL_POINTS;
    const float t = (float)(i + 1) / (float)_count;
    const Point& q = _pts[idx];
    const float r = q.w * t * 0.80f;
    if (r < 1.0f) continue;
    // Fade towards the background, and shift the hue along the ribbon:
    // pink at the tail through aqua to near-white just behind the ball.
    const uint16_t hue = mixColor(COL_TRAIL_PINK, COL_TRAIL_AQUA,
                                  clampf(t * 1.4f, 0.0f, 1.0f));
    cv.fillCircle(ri(q.x), ri(q.y), rr(r),
                  mixColor(COL_BG, hue, 0.10f + 0.55f * t * t));
  }
}

void Trail::drawParticles(M5Canvas& cv) const {
  for (int i = 0; i < GAME_PARTICLES; ++i) {
    const Particle& p = _p[i];
    if (p.life <= 0.0f) continue;
    const float t = p.life / p.life0;              // 1 -> 0 over its lifetime
    const uint16_t c = mixColor(COL_BG, p.col, clampf(t * 1.25f, 0.0f, 1.0f));
    const float r = p.size * (0.35f + 0.65f * t);
    switch (p.kind) {
      case 0: starAt(cv, p.x, p.y, r, p.rot, c); break;
      case 1: sparkleAt(cv, p.x, p.y, r, p.rot, c); break;
      default: cv.fillCircle(ri(p.x), ri(p.y), rr(r * 0.6f), c); break;
    }
  }
}
