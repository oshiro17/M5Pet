#include "SuikaFruits.h"
#include "PetState.h"
#include "Character.h"

static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

// Body / highlight / outline per tier. The hues started as samples off a
// screenshot of the real game and have since been walked towards white, which
// is most of what makes this look soft rather than arcade.
//
// Pastels cost contrast, and contrast is what the two awkward pairs live on:
// the cherry and strawberry differ by 2 px and are both red, the dekopon and
// persimmon by 3 px and are both orange. So the lightening is not uniform --
// the cherry is held back a couple of steps deeper than the strawberry, and the
// persimmon deeper and redder than the dekopon. Lighten those four evenly and
// the game stops being readable at the sizes that matter most.
struct FruitSkin { uint16_t body, light, dark; };

static const FruitSkin SKIN[SUIKA_TIERS] = {
  { rgb565(228,  96, 110), rgb565(255, 168, 176), rgb565(184,  62,  80) },  // cherry
  { rgb565(252, 158, 152), rgb565(255, 206, 202), rgb565(212, 104, 108) },  // strawberry
  { rgb565(190, 158, 246), rgb565(224, 206, 255), rgb565(146, 116, 202) },  // grape
  { rgb565(255, 202, 132), rgb565(255, 230, 190), rgb565(222, 160,  96) },  // dekopon
  { rgb565(250, 158, 110), rgb565(255, 200, 168), rgb565(208, 116,  76) },  // persimmon
  { rgb565(246, 134, 142), rgb565(255, 188, 190), rgb565(200,  92, 104) },  // apple
  { rgb565(242, 236, 178), rgb565(255, 252, 228), rgb565(200, 194, 142) },  // pear
  { COL_CHAR_BODY,         COL_CHAR_LIGHT,        COL_CHAR_SHADE         },  // character
  { rgb565(252, 226, 134), rgb565(255, 244, 196), rgb565(214, 180,  96) },  // pineapple
  { rgb565(188, 220, 152), rgb565(224, 242, 198), rgb565(148, 184, 118) },  // melon
  { rgb565(112, 180, 112), rgb565(180, 222, 172), rgb565( 76, 134,  82) },  // watermelon
};

// Rotate an offset about the fruit's centre and round it to a pixel.
static inline int rx(float x, float ox, float oy, float c, float s) { return ri(x + ox * c - oy * s); }
static inline int ry(float y, float ox, float oy, float c, float s) { return ri(y + ox * s + oy * c); }

// Parallel bands across the disc, rolled with the fruit. A band `d` off the
// centre line spans a chord of 2*sqrt(r^2 - d^2), so working the length out
// from the offset keeps the ends inside the circle -- no clip rect, and the
// rounded caps of drawWideLine land on the rim rather than over it.
static void bands(M5Canvas& cv, float x, float y, float r, float ang,
                  int n, float w, uint16_t col) {
  const float ux = cosf(ang), uy = sinf(ang);
  const float nx = -uy, ny = ux;
  const float step = (2.0f * r) / (n + 1);
  for (int i = 1; i <= n; ++i) {
    const float d  = -r + step * i;
    const float h2 = r * r - d * d;
    if (h2 <= 1.0f) continue;
    const float L = sqrtf(h2) - w;
    if (L <= 0.5f) continue;
    const float cx = x + nx * d, cy = y + ny * d;
    cv.drawWideLine(ri(cx - ux * L), ri(cy - uy * L),
                    ri(cx + ux * L), ri(cy + uy * L), w, col);
  }
}

// Two eyes, a shallow smile and a pair of cheeks. Deliberately small and low: a
// face that fills the fruit stops reading as a fruit.
//
// The cheeks go on first so the eyes are never drawn over. They sit outside the
// eyes rather than under them -- at r=10 there is no room under anything, and
// wide cheeks are what makes a circle look pleased with itself.
static void drawFace(M5Canvas& cv, float x, float y, float r, float c, float s,
                     bool glee) {
  const float ex = r * 0.34f, ey = -r * 0.08f;

  const float bx = r * 0.60f, by = r * 0.16f;
  for (int i = 0; i < 2; ++i) {
    const float ox = i ? bx : -bx;
    cv.fillEllipse(rx(x, ox, by, c, s), ry(y, ox, by, c, s),
                   rr(r * 0.14f), rr(r * 0.10f), COL_SUIKA_BLUSH);
  }

  for (int i = 0; i < 2; ++i) {
    const float ox = i ? ex : -ex;
    if (glee) {
      // Shut eyes: a caret, drawn as two segments meeting above the eye line.
      // An arc would be nicer and is not available at 3 px across.
      const float w = r * 0.13f, hh = r * 0.11f;
      cv.drawLine(rx(x, ox - w, ey + hh, c, s), ry(y, ox - w, ey + hh, c, s),
                  rx(x, ox, ey - hh, c, s), ry(y, ox, ey - hh, c, s), COL_SUIKA_INK);
      cv.drawLine(rx(x, ox, ey - hh, c, s), ry(y, ox, ey - hh, c, s),
                  rx(x, ox + w, ey + hh, c, s), ry(y, ox + w, ey + hh, c, s), COL_SUIKA_INK);
    } else {
      cv.fillEllipse(rx(x, ox, ey, c, s), ry(y, ox, ey, c, s),
                     rr(r * 0.10f), rr(r * 0.15f), COL_SUIKA_INK);
    }
  }

  // The smile widens and drops with glee. Same two-segment shape either way, so
  // the merge face is the ordinary face read a little harder.
  const float g  = glee ? 1.45f : 1.0f;
  const float mx = r * 0.17f * g, my = r * 0.26f, md = r * 0.13f * g;
  cv.drawLine(rx(x, -mx, my, c, s), ry(y, -mx, my, c, s),
              rx(x, 0.0f, my + md, c, s), ry(y, 0.0f, my + md, c, s), COL_SUIKA_INK);
  cv.drawLine(rx(x, 0.0f, my + md, c, s), ry(y, 0.0f, my + md, c, s),
              rx(x, mx, my, c, s), ry(y, mx, my, c, s), COL_SUIKA_INK);
}

// Body, gloss and rim. Every tier starts here.
static void drawDisc(M5Canvas& cv, int t, float x, float y, float r, float c, float s) {
  const FruitSkin& k = SKIN[t];
  cv.fillCircle(ri(x), ri(y), rr(r), k.body);
  cv.fillEllipse(rx(x, -r * 0.34f, -r * 0.42f, c, s), ry(y, -r * 0.34f, -r * 0.42f, c, s),
                 rr(r * 0.22f), rr(r * 0.14f), k.light);
  cv.drawCircle(ri(x), ri(y), rr(r), k.dark);
}

// A stalk leaning out of the top of the fruit, and optionally a leaf beside it.
static void drawStem(M5Canvas& cv, float x, float y, float r, float c, float s,
                     float len, bool leaf) {
  cv.drawLine(rx(x, 0.0f, -r * 0.85f, c, s), ry(y, 0.0f, -r * 0.85f, c, s),
              rx(x, r * 0.18f, -r * (0.85f + len), c, s),
              ry(y, r * 0.18f, -r * (0.85f + len), c, s), rgb565(122, 84, 40));
  if (leaf) {
    cv.fillEllipse(rx(x, -r * 0.34f, -r * 0.86f, c, s),
                   ry(y, -r * 0.34f, -r * 0.86f, c, s),
                   rr(r * 0.30f), rr(r * 0.16f), COL_SUIKA_LEAF);
  }
}

void drawSuikaFruit(M5Canvas& cv, int t, float x, float y, float ang, bool glee) {
  if (t < 0 || t >= SUIKA_TIERS) return;
  const float r = SUIKA_RADIUS[t];

  // The character keeps its feet under it. A gentle lean off vertical, taken
  // from the roll it would otherwise have had, is enough to read as motion. It
  // has its own expressions, so glee routes to EXPR_HAPPY rather than to the
  // caret eyes above.
  if (t == 7) {
    drawCharacterBall(cv, x, y, r, 0.0f, -HALF_PI + 0.28f * sinf(ang),
                      glee ? EXPR_HAPPY : EXPR_NORMAL, 0.0f);
    return;
  }

  const float c = cosf(ang), s = sinf(ang);
  const FruitSkin& k = SKIN[t];
  drawDisc(cv, t, x, y, r, c, s);

  switch (t) {
    case 0:   // cherry -- plain and dark, with a stalk
      drawStem(cv, x, y, r, c, s, 0.55f, false);
      break;

    case 1: { // strawberry -- pale seeds and a leaf, so it never reads as a cherry
      const float seed[3][2] = { { -0.34f, 0.02f }, { 0.30f, -0.14f }, { 0.04f, 0.36f } };
      for (int i = 0; i < 3; ++i) {
        cv.drawPixel(rx(x, seed[i][0] * r, seed[i][1] * r, c, s),
                     ry(y, seed[i][0] * r, seed[i][1] * r, c, s), COL_WHITE);
      }
      drawStem(cv, x, y, r, c, s, 0.30f, true);
      break;
    }

    case 2:   // grape -- a few darker lobes hint at a bunch
      for (int i = 0; i < 3; ++i) {
        const float a  = ang + i * (TWO_PI / 3.0f);
        const float ox = cosf(a) * r * 0.40f, oy = sinf(a) * r * 0.40f;
        cv.fillCircle(ri(x + ox), ri(y + oy), rr(r * 0.32f), k.dark);
      }
      break;

    case 3:   // dekopon -- the navel bump is the whole point of the fruit
      cv.fillCircle(rx(x, 0.0f, -r * 0.82f, c, s), ry(y, 0.0f, -r * 0.82f, c, s),
                    rr(r * 0.26f), k.body);
      cv.fillEllipse(rx(x, 0.0f, -r * 0.98f, c, s), ry(y, 0.0f, -r * 0.98f, c, s),
                     rr(r * 0.16f), rr(r * 0.10f), COL_SUIKA_LEAF);
      break;

    case 4:   // persimmon -- four-lobed calyx, deeper orange body
      for (int i = 0; i < 4; ++i) {
        const float a  = ang - HALF_PI + (i - 1.5f) * 0.42f;
        const float ox = cosf(a) * r * 0.72f, oy = sinf(a) * r * 0.72f;
        cv.fillEllipse(ri(x + ox), ri(y + oy), rr(r * 0.20f), rr(r * 0.13f), COL_SUIKA_LEAF);
      }
      break;

    case 5:   // apple -- dimple at the stalk
      cv.fillEllipse(rx(x, 0.0f, -r * 0.78f, c, s), ry(y, 0.0f, -r * 0.78f, c, s),
                     rr(r * 0.20f), rr(r * 0.10f), k.dark);
      drawStem(cv, x, y, r, c, s, 0.30f, true);
      break;

    case 6:   // pear -- freckles
      for (int i = 0; i < 5; ++i) {
        const float a = ang + i * 1.257f;
        cv.fillCircle(ri(x + cosf(a) * r * 0.46f), ri(y + sinf(a) * r * 0.46f),
                      1, k.dark);
      }
      break;

    case 8:   // pineapple -- crosshatch and a crown
      bands(cv, x, y, r, ang + 0.79f, 3, 1.4f, k.dark);
      bands(cv, x, y, r, ang - 0.79f, 3, 1.4f, k.dark);
      for (int i = 0; i < 3; ++i) {
        const float a = ang - HALF_PI + (i - 1) * 0.40f;
        cv.drawWideLine(ri(x + cosf(a) * r * 0.80f), ri(y + sinf(a) * r * 0.80f),
                        ri(x + cosf(a) * r * 1.24f), ri(y + sinf(a) * r * 1.24f),
                        1.6f, COL_SUIKA_LEAF);
      }
      break;

    case 9:   // melon -- netting
      bands(cv, x, y, r, ang + 0.60f, 3, 1.6f, k.light);
      bands(cv, x, y, r, ang - 0.60f, 3, 1.6f, k.light);
      break;

    case 10:  // watermelon -- the stripes are what makes it unmistakable
      bands(cv, x, y, r, ang + HALF_PI, 5, 3.4f, k.light);
      break;

    default: break;
  }

  if (t >= SUIKA_FACE_FROM) drawFace(cv, x, y, r, c, s, glee);
}

void drawSuikaChip(M5Canvas& cv, int t, float x, float y, float r) {
  if (t < 0 || t >= SUIKA_TIERS) return;
  const FruitSkin& k = SKIN[t];
  cv.fillCircle(ri(x), ri(y), rr(r), k.body);
  cv.drawCircle(ri(x), ri(y), rr(r), k.dark);
}
