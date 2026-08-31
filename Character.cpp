#include "Character.h"
#include "PetState.h"

static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

// ---------------------------------------------------------------------------
// Three-quarter profile (boot animation)
// ---------------------------------------------------------------------------
// Both eyes, the blush and the mouth crowd onto the leading side of the face,
// so the character reads as looking at -- and biting into -- whatever is in
// front of it. face = -1 faces left.
void drawCharacterProfile(M5Canvas& cv, float x, float y, float squash,
                          float mouth, float puff, float walk, uint8_t expr,
                          int face, float radius) {
  const float rx = radius * (1.0f + squash);
  const float ry = radius * (1.0f - squash);
  const float f  = (float)face;
  const float s  = radius / CHAR_RADIUS;   // feature scale vs. the tuned size

  const float step = sinf(walk * TWO_PI);

  // --- far foot: drawn first, so the body hides all but the heel
  cv.fillEllipse(ri(x - f * rx * 0.38f - step * 3.0f * s), ri(y + ry * 0.90f),
                 rr(rx * 0.36f), rr(ry * 0.22f), COL_CHAR_FOOT_DK);

  // --- body
  cv.fillEllipse(ri(x), ri(y), rr(rx), rr(ry), COL_CHAR_BODY);

  // shaded underside: an ellipse that sits wholly inside the body, so the
  // boundary is a curve rather than the straight edge a clip rect would give
  cv.fillEllipse(ri(x), ri(y + ry * 0.56f),
                 rr(rx * 0.86f), rr(ry * 0.40f), COL_CHAR_SHADE);

  // soft sheen on the crown. Kept small, low-contrast and near the top: out on
  // the flank it breaks the silhouette and reads as a second arm.
  cv.fillEllipse(ri(x - f * rx * 0.16f), ri(y - ry * 0.66f),
                 rr(rx * 0.17f), rr(ry * 0.09f), COL_CHAR_LIGHT);

  // --- near foot: big, planted forward, overlapping the body
  cv.fillEllipse(ri(x + f * rx * 0.42f + step * 3.0f * s), ri(y + ry * 0.98f),
                 rr(rx * 0.40f), rr(ry * 0.24f), COL_CHAR_FOOT);

  // --- near arm only. Drawing the far arm as well is exactly what collapses a
  //     profile back into a front view. It also swings back behind the body as
  //     the mouth opens, which keeps it clear of the bite and reads as a
  //     wind-up.
  const float armOut = 1.0f - mouth * 0.85f;
  cv.fillEllipse(ri(x + f * rx * (0.30f + 0.72f * armOut)),
                 ri(y + ry * (0.50f - 0.10f * armOut)),
                 rr(rx * 0.25f), rr(ry * 0.32f), COL_CHAR_BODY);

  // --- blush on the visible cheek; it swells while chewing
  const float cheekRx = (5.5f + puff * 4.0f) * s;
  cv.fillEllipse(ri(x + f * rx * 0.02f), ri(y + ry * 0.08f),
                 rr(cheekRx), rr(cheekRx * 0.58f), COL_CHEEK);

  // --- eyes, both pushed towards the leading side. The far one is narrower,
  //     which is what makes the head read as turned rather than flat-on.
  // Past mouth = 1 the character is inhaling, not biting: the mouth takes over
  // the face, so the eyes retreat to the crown and shrink to make room.
  const float wide  = clampf(mouth - 1.0f, 0.0f, 1.0f);
  const float eNear = f * rx * 0.10f;   // closer to us
  const float eFar  = f * rx * 0.56f;   // further round the cheek
  const float ey    = y - ry * (0.30f + 0.32f * wide);
  const float eScale = (1.0f - 0.24f * wide) * s;
  if (expr == EXPR_HAPPY) {
    for (int t = 0; t < 3; ++t) {
      const int cxs[2] = { ri(x + eNear), ri(x + eFar) };
      for (int i = 0; i < 2; ++i) {
        const int cx = cxs[i];
        cv.drawLine(cx - 5, ri(ey) + 3 + t, cx, ri(ey) - 4 + t, COL_EYE_NAVY);
        cv.drawLine(cx, ri(ey) - 4 + t, cx + 5, ri(ey) + 3 + t, COL_EYE_NAVY);
      }
    }
  } else {
    const float offs[2]   = { eNear, eFar };
    const float narrow[2] = { 1.0f, 0.68f };
    for (int i = 0; i < 2; ++i) {
      const int cx = ri(x + offs[i]), cy = ri(ey);
      const int erx = rr(5.2f * narrow[i] * eScale), ery = rr(10.0f * eScale);
      // three bands, as in the reference: white cap, navy middle, blue floor
      cv.fillEllipse(cx, cy, erx, ery, COL_EYE_NAVY);
      cv.setClipRect(cx - erx, cy + rr(ery * 0.28f), erx * 2 + 1, ery + 1);
      cv.fillEllipse(cx, cy, rr(erx * 0.78f), rr(ery * 0.88f), COL_EYE_BLUE);
      cv.clearClipRect();
      cv.fillEllipse(cx, ri(cy - ery * 0.48f),
                     rr(erx * 0.62f), rr(ery * 0.26f), COL_WHITE);
    }
  }

  // --- mouth, on the leading edge of the face so it meets the apple
  const float mx = x + f * rx * (0.50f - 0.14f * wide);
  const float my = y + ry * (0.26f + 0.06f * wide);
  if (mouth > 0.03f) {
    const float mrx = 3.0f * s + mouth * rx * 0.36f;
    const float mry = 3.0f * s + mouth * ry * 0.29f;
    cv.fillEllipse(ri(mx), ri(my), rr(mrx), rr(mry), COL_MOUTH_DK);
    // throat: the tan oval that fills the bottom of a wide-open mouth
    cv.fillEllipse(ri(mx + f * mrx * 0.10f), ri(my + mry * 0.34f),
                   rr(mrx * 0.66f), rr(mry * 0.42f), COL_MOUTH_IN);
  } else {
    // resting: the small oval mouth from the reference
    cv.fillEllipse(ri(mx), ri(my), rr(3.0f * s), rr(4.0f * s), COL_MOUTH_DK);
  }
}

// ---------------------------------------------------------------------------
// Head-on ball (game)
// ---------------------------------------------------------------------------
// At this size (radius ~10-13 px) the profile's arms and separate feet turn to
// mush, so this is a deliberately simplified read: round pink body, two tall
// eyes, two blushes, two red feet peeking out below. `lean` slides the whole
// face a little towards where it is heading.
void drawCharacterBall(M5Canvas& cv, float x, float y, float radius,
                       float squash, float lean, uint8_t expr, float glow) {
  const float rx = radius * (1.0f + squash);
  const float ry = radius * (1.0f - squash);

  // Direction of travel, as a unit offset. Everything on the face shifts along
  // it slightly, which is what stops a perfectly symmetric ball from looking
  // like a sticker.
  const float lx = cosf(lean), ly = sinf(lean);

  // --- halo: a soft ring just outside the body. Cheap stand-in for a glow --
  //     one filled ellipse, blended most of the way back to the background.
  if (glow > 0.02f) {
    cv.fillEllipse(ri(x), ri(y), rr(rx + 3.0f * glow), rr(ry + 3.0f * glow),
                   mixColor(COL_BG, COL_CHAR_LIGHT, 0.30f * glow));
  }

  // --- feet, behind the body so only the toes show
  const float footY = y + ry * 0.82f;
  cv.fillEllipse(ri(x - rx * 0.46f), ri(footY), rr(rx * 0.36f), rr(ry * 0.24f),
                 COL_CHAR_FOOT_DK);
  cv.fillEllipse(ri(x + rx * 0.46f), ri(footY), rr(rx * 0.36f), rr(ry * 0.24f),
                 COL_CHAR_FOOT);

  // --- body
  cv.fillEllipse(ri(x), ri(y), rr(rx), rr(ry), COL_CHAR_BODY);
  cv.fillEllipse(ri(x), ri(y + ry * 0.54f), rr(rx * 0.84f), rr(ry * 0.40f),
                 COL_CHAR_SHADE);
  cv.fillEllipse(ri(x - lx * rx * 0.14f), ri(y - ry * 0.62f),
                 rr(rx * 0.26f), rr(ry * 0.14f), COL_CHAR_LIGHT);

  // --- face. Kept small relative to the body: at 11 px radius the eyes are
  //     only ~3x6, and any larger they merge into one dark band.
  const float fx = x + lx * rx * 0.10f;
  const float fy = y + ly * ry * 0.06f;
  const float eDx = rx * 0.30f;
  const float ey  = fy - ry * 0.18f;
  const float erx = radius * 0.16f, ery = radius * 0.34f;

  // blush first, so the eyes overlap it rather than the other way round
  cv.fillEllipse(ri(fx - rx * 0.60f), ri(fy + ry * 0.12f),
                 rr(radius * 0.17f), rr(radius * 0.10f), COL_CHEEK);
  cv.fillEllipse(ri(fx + rx * 0.60f), ri(fy + ry * 0.12f),
                 rr(radius * 0.17f), rr(radius * 0.10f), COL_CHEEK);

  if (expr == EXPR_HAPPY) {
    for (int i = 0; i < 2; ++i) {
      const int cx = ri(fx + (i ? eDx : -eDx));
      const int cy = ri(ey);
      const int w  = rr(radius * 0.20f), h = rr(radius * 0.16f);
      cv.drawLine(cx - w, cy + h, cx, cy - h, COL_EYE_NAVY);
      cv.drawLine(cx, cy - h, cx + w, cy + h, COL_EYE_NAVY);
      cv.drawLine(cx - w, cy + h + 1, cx, cy - h + 1, COL_EYE_NAVY);
      cv.drawLine(cx, cy - h + 1, cx + w, cy + h + 1, COL_EYE_NAVY);
    }
  } else {
    for (int i = 0; i < 2; ++i) {
      const int cx = ri(fx + (i ? eDx : -eDx)), cy = ri(ey);
      cv.fillEllipse(cx, cy, rr(erx), rr(ery), COL_EYE_NAVY);
      // blue floor, clipped to the lower half so it stays a band
      cv.setClipRect(cx - rr(erx), cy + rr(ery * 0.30f),
                     rr(erx) * 2 + 1, rr(ery) + 1);
      cv.fillEllipse(cx, cy, rr(erx * 0.78f), rr(ery * 0.86f), COL_EYE_BLUE);
      cv.clearClipRect();
      // white cap
      cv.fillEllipse(cx, ri(cy - ery * 0.44f),
                     rr(erx * 0.60f), rr(ery * 0.26f), COL_WHITE);
    }
  }

  // --- mouth: one small oval, or an open "o" while eating a block
  if (expr == EXPR_EATING) {
    cv.fillEllipse(ri(fx), ri(fy + ry * 0.34f),
                   rr(radius * 0.20f), rr(radius * 0.24f), COL_MOUTH_DK);
    cv.fillEllipse(ri(fx), ri(fy + ry * 0.40f),
                   rr(radius * 0.12f), rr(radius * 0.11f), COL_MOUTH_IN);
  } else {
    cv.fillEllipse(ri(fx), ri(fy + ry * 0.30f),
                   rr(radius * 0.11f), rr(radius * 0.13f), COL_MOUTH_DK);
  }
}
