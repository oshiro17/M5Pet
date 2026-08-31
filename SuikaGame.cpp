#include "SuikaGame.h"
#include "PetState.h"
#include "SuikaFruits.h"
#include <Preferences.h>

static inline int ri(float v) { return (int)lroundf(v); }

// Box-local shorthand, plus the fixed step. The step is deliberately fixed
// rather than taken from the real frame time: a pile is only as stable as its
// timestep is predictable, and a frame that runs long should slow the game down
// a hair rather than shake the stack apart.
static const float BOX_W   = (float)SUIKA_BOX_W;
static const float BOX_H   = (float)SUIKA_BOX_H;
static const float LINE_Y  = (float)(SUIKA_LINE_Y - SUIKA_BOX_Y);
static const float ENTRY_Y = (float)(SUIKA_PREVIEW_Y - SUIKA_BOX_Y);
static const float DT      = 1.0f / (float)(TARGET_FPS * SUIKA_SUBSTEPS);

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void SuikaGame::begin(uint32_t now) {
  _n = 0;
  _nMerge = 0;
  _nSpark = 0;
  _reorder = true;
  _score = 0;
  _overMs = 0;
  _lineHot = false;
  _roll = 0.0f;
  _aimX = BOX_W * 0.5f;
  _lastMs = now;

  loadHighscore();
  M5.Speaker.setVolume(90);

  _next = (int)random(0, SUIKA_DROP_TIERS);
  spawnPreview();

  _phase = PH_AIM;
  _phaseStart = now;
}

void SuikaGame::loadHighscore() {
  Preferences p;
  if (p.begin("m5pet", true)) {
    _high = p.getInt("suikaHi", 0);
    p.end();
  }
}

void SuikaGame::saveHighscore() {
  if (_score <= _high) return;
  _high = _score;
  Preferences p;
  if (p.begin("m5pet", false)) {
    p.putInt("suikaHi", _high);
    p.end();
  }
}

// ---------------------------------------------------------------------------
// Solver
// ---------------------------------------------------------------------------
// Verlet: the velocity is implied by how far the fruit moved last step, so
// there is no velocity to store and nothing to keep in sync with the position.
void SuikaGame::integrate(float dt) {
  const float g    = SUIKA_GRAVITY * dt * dt;
  const float vmax = SUIKA_MAX_SPEED * dt;
  const float rmax = SUIKA_MAX_RISE * dt;

  for (int i = 0; i < _n; ++i) {
    Fruit& f = _f[i];
    float vx = (f.x - f.px) * SUIKA_DAMPING;
    float vy = (f.y - f.py) * SUIKA_DAMPING;

    // Safety net only. Gravity alone cannot reach this in a 169 px box; a bad
    // resolution during a heavy pile-up theoretically could.
    const float sp = sqrtf(vx * vx + vy * vy);
    if (sp > vmax) { const float k = vmax / sp; vx *= k; vy *= k; }
    if (vy < -rmax) vy = -rmax;   // rising only: see SUIKA_MAX_RISE

    f.px = f.x;  f.py = f.y;
    f.x += vx;
    f.y += vy + g;
    f.budget  = SUIKA_MAX_PUSH;
    f.vbudget = SUIKA_SOFT_PUSH;
  }
}

// Deepest first. Gauss-Seidel passes information along in the order it visits
// contacts, and a stack is held up from the bottom, so starting at the floor
// carries the support all the way up in a single pass. Started from the top it
// takes roughly twice the iterations to look as solid.
//
// Positions barely change between steps, so this insertion sort is O(n) in
// practice. It only degrades on the frames after a merge, when the order has to
// be rebuilt from scratch, and those are rare.
void SuikaGame::sortByDepth() {
  if (_reorder) {
    for (int i = 0; i < _n; ++i) _order[i] = (uint8_t)i;
    _reorder = false;
  }
  for (int i = 1; i < _n; ++i) {
    const uint8_t v = _order[i];
    const float   k = _f[v].y;
    int j = i - 1;
    while (j >= 0 && _f[_order[j]].y < k) { _order[j + 1] = _order[j]; --j; }
    _order[j + 1] = v;
  }
}

// Walls are position clamps. Restitution is applied by moving the *previous*
// position rather than a velocity: setting px to x + v*e leaves an implied
// velocity of -v*e, which is the bounce.
void SuikaGame::solveWalls(bool last) {
  for (int i = 0; i < _n; ++i) {
    Fruit& f = _f[i];
    const float r  = SUIKA_RADIUS[f.tier];
    const float vx = f.x - f.px;

    if (f.x < r) {
      f.x = r;
      f.px = f.x + vx * SUIKA_WALL_REST;
      f.flags |= F_TOUCHED;
    } else if (f.x > BOX_W - r) {
      f.x = BOX_W - r;
      f.px = f.x + vx * SUIKA_WALL_REST;
      f.flags |= F_TOUCHED;
    }

    if (f.y > BOX_H - r) {
      const float vy = f.y - f.py;
      f.y = BOX_H - r;
      f.py = f.y + vy * SUIKA_WALL_REST;
      f.flags |= F_TOUCHED;
      if (last) f.spin += SUIKA_SPIN_GAIN * vx / r;   // rolling along the floor
    }
  }
}

// The whole of the fruit-to-fruit physics. Equal masses, so each contact is
// half a push each way; no square root until two circles actually overlap.
//
// `last` marks the final relaxation pass, and only that pass records contacts,
// spin and merges -- doing it every iteration would count the same contact four
// times over and spin the fruits like tops.
void SuikaGame::solvePairs(bool last) {
  const float reachExtra = last ? SUIKA_SLOP : 0.0f;

  for (int a = 0; a < _n; ++a) {
    const int i = _order[a];
    Fruit& fi = _f[i];
    const float ir = SUIKA_RADIUS[fi.tier];

    for (int b = a + 1; b < _n; ++b) {
      const int j = _order[b];
      Fruit& fj = _f[j];
      const float jr  = SUIKA_RADIUS[fj.tier];
      const float sum = ir + jr;

      const float dx = fj.x - fi.x, dy = fj.y - fi.y;
      const float d2 = dx * dx + dy * dy;
      const float reach = sum + reachExtra;
      if (d2 >= reach * reach) continue;          // the common case, and no sqrt

      const float d = sqrtf(d2);
      float nx, ny;
      if (d > 0.0001f) { nx = dx / d; ny = dy / d; }
      else             { nx = 0.0f;   ny = -1.0f; }   // exactly coincident: shove one up

      if (d < sum) {
        // Split the overlap evenly -- equal masses -- but never spend more
        // separation on a fruit than it has budget left for this substep. The
        // budget is what stops a merge from launching its neighbours.
        float push = (sum - d) * 0.5f;
        if (push > fi.budget) push = fi.budget;
        if (push > fj.budget) push = fj.budget;
        if (push > 0.0f) {
          // Move the previous position along with the current one for whatever
          // part of the push exceeds the speed allowance. Verlet reads velocity
          // out of that gap, so carrying it forward moves the fruit silently:
          // full separation, no launch.
          const float vi = fminf(push, fi.vbudget);
          const float vj = fminf(push, fj.vbudget);
          fi.x -= nx * push;         fi.y -= ny * push;
          fi.px -= nx * (push - vi); fi.py -= ny * (push - vi);
          fj.x += nx * push;         fj.y += ny * push;
          fj.px += nx * (push - vj); fj.py += ny * (push - vj);
          fi.budget -= push;  fi.vbudget -= vi;
          fj.budget -= push;  fj.vbudget -= vj;
        }
      }

      if (!last) continue;

      fi.flags |= F_TOUCHED;
      fj.flags |= F_TOUCHED;

      // Decorative roll, from how fast the two are sliding across each other.
      const float rvx = (fj.x - fj.px) - (fi.x - fi.px);
      const float rvy = (fj.y - fj.py) - (fi.y - fi.py);
      const float vt  = rvx * -ny + rvy * nx;
      fi.spin += SUIKA_SPIN_GAIN * vt / ir;
      fj.spin -= SUIKA_SPIN_GAIN * vt / jr;

      // Same size and neither one already claimed. That second test is the
      // whole three-way guard: without it, a cherry touching two others in the
      // same step merges twice and one of them vanishes into nothing.
      if (fi.tier == fj.tier &&
          !((fi.flags | fj.flags) & F_MERGING) &&
          _nMerge < (int)(sizeof(_merge) / sizeof(_merge[0]))) {
        fi.flags |= F_MERGING;
        fj.flags |= F_MERGING;
        _merge[_nMerge].a = (uint8_t)i;
        _merge[_nMerge].b = (uint8_t)j;
        ++_nMerge;
      }
    }
  }
}

// Fired at a merge, from the rim of the fruit that came out of it rather than
// from its centre, so the burst reads as coming off the seam instead of out of
// the middle of a face. Oldest sparks are overwritten once the cap is reached:
// a chain reaction should cost the last merge's sparkle, not the frame budget.
void SuikaGame::spawnSparks(float x, float y, int tier) {
  const float r = SUIKA_RADIUS[tier] * 0.72f;
  for (int i = 0; i < SUIKA_SPARKS_PER; ++i) {
    Spark& p = (_nSpark < SUIKA_MAX_SPARKS) ? _spark[_nSpark++]
                                            : _spark[i % SUIKA_MAX_SPARKS];
    const float a = randRange(0.0f, TWO_PI);
    const float v = SUIKA_SPARK_SPEED * randRange(0.6f, 1.25f);
    p.x = x + cosf(a) * r;
    p.y = y + sinf(a) * r;
    p.vx = cosf(a) * v;
    p.vy = sinf(a) * v;
    p.life = SUIKA_SPARK_LIFE * randRange(0.75f, 1.0f);
  }
}

// Plain ballistics on the real frame time, not the substep: nothing collides
// with these, so there is no reason to integrate them four times a frame.
void SuikaGame::updateSparks(float dt) {
  int w = 0;
  for (int i = 0; i < _nSpark; ++i) {
    Spark& p = _spark[i];
    p.life -= dt;
    if (p.life <= 0.0f) continue;
    p.vy += SUIKA_SPARK_GRAV * dt;
    p.x  += p.vx * dt;
    p.y  += p.vy * dt;
    if (w != i) _spark[w] = p;
    ++w;
  }
  _nSpark = w;
}

void SuikaGame::applyMerges() {
  if (_nMerge == 0) return;

  for (int m = 0; m < _nMerge; ++m) {
    Fruit& a = _f[_merge[m].a];
    Fruit& b = _f[_merge[m].b];
    const int t = a.tier;

    _score += SUIKA_SCORE[t];
    M5.Speaker.tone(330.0f * powf(1.09f, (float)t), 45);

    if (t >= SUIKA_TIERS - 1) {
      // Two watermelons leave nothing behind. This is the only merge that
      // shrinks the board by two -- and the only one with no fruit left to put
      // the grin on, so the sparkle is all the acknowledgement there is. Worth
      // twice as much of it.
      spawnSparks((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, SUIKA_TIERS - 1);
      spawnSparks((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, SUIKA_TIERS - 1);
      a.flags |= F_DEAD;
      b.flags |= F_DEAD;
      continue;
    }

    // Midpoint, and the mean velocity -- an equal-mass inelastic collision,
    // which is what a merge is.
    //
    // The new fruit is bigger than either of the two it replaces, so a merge
    // against a wall lands it partly outside the box. The solver would pull it
    // back next step, but not before it had been drawn there, so put it inside
    // now. Only the sides and floor: the top is open and fruits legitimately
    // stand above it.
    const float R  = SUIKA_RADIUS[t + 1];
    const float mx = clampf((a.x + b.x) * 0.5f, R, BOX_W - R);
    const float my = fminf((a.y + b.y) * 0.5f, BOX_H - R);
    const float vx = ((a.x - a.px) + (b.x - b.px)) * 0.5f;
    const float vy = ((a.y - a.py) + (b.y - b.py)) * 0.5f;

    a.tier  = (uint8_t)(t + 1);
    a.x = mx;  a.y = my;
    a.px = mx - vx;  a.py = my - vy;
    a.ang = (a.ang + b.ang) * 0.5f;
    a.spin = 0.0f;
    a.flags = F_TOUCHED;     // also clears MERGING and FALLING
    a.glee  = SUIKA_GLEE_MS;
    b.flags |= F_DEAD;

    spawnSparks(mx, my, t + 1);
  }
  _nMerge = 0;

  // Sweep out the dead in one stable pass. Indices move, so the depth order has
  // to be rebuilt next step.
  int w = 0;
  for (int i = 0; i < _n; ++i) {
    if (_f[i].flags & F_DEAD) continue;
    _f[i].flags &= (uint8_t)~F_MERGING;
    if (w != i) _f[w] = _f[i];
    ++w;
  }
  _n = w;
  _reorder = true;
}

void SuikaGame::step(float dt) {
  integrate(dt);
  sortByDepth();
  // Walls go last in each pass. They are hard clamps with no budget, so
  // finishing on them guarantees nothing is outside the box when the step ends
  // -- solving them first lets the very next contact shove a fruit back out.
  for (int k = 0; k < SUIKA_ITERATIONS; ++k) {
    const bool last = (k == SUIKA_ITERATIONS - 1);
    solvePairs(last);
    solveWalls(last);
  }
  applyMerges();
}

// ---------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------
int SuikaGame::addFruit(int tier, float x, float y) {
  if (_n >= SUIKA_MAX_FRUITS) return -1;
  Fruit& f = _f[_n];
  f.x = x;    f.y = y;
  f.px = x;   f.py = y;          // released from rest, like the real game
  f.ang = randRange(0.0f, TWO_PI);
  f.spin = 0.0f;
  f.glee = 0;
  f.budget  = SUIKA_MAX_PUSH;
  f.vbudget = SUIKA_SOFT_PUSH;
  f.tier = (uint8_t)tier;
  f.flags = 0;
  _reorder = true;
  return _n++;
}

void SuikaGame::spawnPreview() {
  _cur  = _next;
  _next = (int)random(0, SUIKA_DROP_TIERS);
}

void SuikaGame::dropFruit(uint32_t now) {
  // Two slots of headroom: a merge writes its result into one of the two it
  // consumed, so the array never actually grows, but leaving room means a full
  // board still cannot wedge.
  if (_n >= SUIKA_MAX_FRUITS - 2) return;

  const int i = addFruit(_cur, _aimX, ENTRY_Y);
  if (i >= 0) _f[i].flags |= F_FALLING;

  M5.Speaker.tone(320.0f, 40);
  _phase = PH_DROP;
  _phaseStart = now;
}

// The real game runs the merge first and only then asks whether anything is
// over the line, so a fruit that crosses it and immediately combines into
// something shorter is safe. Calling this after step() is what implements that.
void SuikaGame::checkOver(uint32_t now, float realDt) {
  bool hot = false;
  for (int i = 0; i < _n; ++i) {
    const Fruit& f = _f[i];
    // A fruit still on its way down passes through the line every single drop;
    // only ones that have landed can lose the game.
    if (!(f.flags & F_TOUCHED)) continue;
    if (f.y - SUIKA_RADIUS[f.tier] < LINE_Y) { hot = true; break; }
  }

  _lineHot = hot;
  if (!hot) { _overMs = 0; return; }

  _overMs += (uint32_t)(realDt * 1000.0f);
  if (_overMs < SUIKA_OVER_MS) return;

  _phase = PH_OVER;
  _phaseStart = now;
  saveHighscore();
  M5.Speaker.tone(523.0f, 130, 0, true);
  M5.Speaker.tone(392.0f, 130, 0, false);
  M5.Speaker.tone(262.0f, 280, 0, false);
}

void SuikaGame::readTilt(float realDt) {
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;

  // Portrait, so screen-X is the device's ax -- the same axis and sign the
  // block breaker's paddle uses.
  const float raw = clampf(ax * GAME_ROLL_SIGN * GAME_TILT_GAIN, -1.0f, 1.0f);
  const float k   = clampf(GAME_TILT_LOWPASS * realDt * TARGET_FPS, 0.0f, 1.0f);
  smoothTowards(_roll, raw, k);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void SuikaGame::update(uint32_t now, bool tapA) {
  float realDt = (float)(now - _lastMs) * 0.001f;
  _lastMs = now;
  if (realDt > 0.25f) realDt = 0.25f;

  if (_phase == PH_OVER) {
    if (tapA) begin(now);
    return;
  }

  readTilt(realDt);

  if (_phase == PH_AIM) {
    // Tilt aims, a press drops, and that is the whole input. The lock that used
    // to live here existed to keep a column steady while the drop was a hold;
    // with the drop on the press itself there is nothing left to hold still
    // for, and a mode you have to remember you are in is worse than none.
    _aimX += shapeTilt(_roll, GAME_TILT_DEAD, GAME_TILT_EXPO) * SUIKA_AIM_SPEED * realDt;

    const float r = SUIKA_RADIUS[_cur];
    _aimX = clampf(_aimX, r, BOX_W - r);

    if (tapA) dropFruit(now);
  } else {
    // Wait for the dropped fruit to land -- or to merge on the way, which
    // clears the flag just as effectively. The timeout only matters if it
    // somehow never settles.
    bool falling = false;
    for (int i = 0; i < _n; ++i) {
      if (_f[i].flags & F_FALLING) { falling = true; break; }
    }
    if (!falling || (uint32_t)(now - _phaseStart) > SUIKA_DROP_WAIT_MS) {
      spawnPreview();
      _phase = PH_AIM;
      _phaseStart = now;
    }
  }

  for (int s = 0; s < SUIKA_SUBSTEPS; ++s) step(DT);

  const uint16_t dms = (uint16_t)(realDt * 1000.0f);
  for (int i = 0; i < _n; ++i) {
    Fruit& f = _f[i];
    f.ang += f.spin;
    f.spin *= SUIKA_SPIN_DAMP;
    if (f.flags & F_TOUCHED) f.flags &= (uint8_t)~F_FALLING;
    f.glee = (f.glee > dms) ? (uint16_t)(f.glee - dms) : 0;
  }

  updateSparks(realDt);

  checkOver(now, realDt);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
// A tiny heart, hand-placed rather than scaled: two lobes and a point, 9 px
// across. Anything smaller loses the notch that makes it a heart at all.
static void drawHeart(M5Canvas& cv, int x, int y, uint16_t col) {
  cv.fillCircle(x - 2, y - 1, 2, col);
  cv.fillCircle(x + 2, y - 1, 2, col);
  cv.fillTriangle(x - 4, y, x + 4, y, x, y + 4, col);
}

void SuikaGame::drawField(M5Canvas& cv) const {
  const int x0 = SUIKA_BOX_X, y0 = SUIKA_BOX_Y;
  const int w = SUIKA_BOX_W, h = SUIKA_BOX_H, R = SUIKA_FIELD_R;

  cv.fillRoundRect(x0, y0, w, h, R, COL_SUIKA_FIELD);

  // Staggered dots, every other row offset by half a step, which is what stops
  // a grid this coarse from reading as a grid. Inset by the corner radius so
  // none of them land in the rounded corners where the field is not there.
  int row = 0;
  for (int y = y0 + R; y < y0 + h - R; y += SUIKA_DOT_STEP, ++row) {
    const int off = (row & 1) ? SUIKA_DOT_STEP / 2 : 0;
    for (int x = x0 + R + off; x < x0 + w - R; x += SUIKA_DOT_STEP) {
      cv.fillRect(x, y, 2, 2, COL_SUIKA_DOT);
    }
  }
}

// Over the fruits, under the walls: a sparkle that came off a merge against the
// side of the box should be clipped by the box, or it looks like it escaped.
void SuikaGame::drawSparks(M5Canvas& cv) const {
  for (int i = 0; i < _nSpark; ++i) {
    const Spark& p = _spark[i];
    // Shrinking is the only fade available on an opaque canvas: 2 px while it
    // is fresh, 1 px for the last third of its life.
    const int s = (p.life > SUIKA_SPARK_LIFE * 0.34f) ? 2 : 1;
    cv.fillRect(ri(SUIKA_BOX_X + p.x), ri(SUIKA_BOX_Y + p.y), s, s, COL_SUIKA_SPARK);
  }
}

// Drawn *over* the fruits rather than inside the field, so the box keeps its
// full 135 px of play. Losing 4 px to walls would put the grape and the dekopon
// 1 px apart, and nobody can see a 1 px difference on this panel.
void SuikaGame::drawWalls(M5Canvas& cv) const {
  const int x0 = SUIKA_BOX_X, y0 = SUIKA_BOX_Y;
  const int w = SUIKA_BOX_W, h = SUIKA_BOX_H, t = SUIKA_WALL_PX;
  const int R = SUIKA_FIELD_R;

  // Straight runs first, stopping short of every corner, then the corners as
  // arcs struck from the same centres the field's rounded corners use -- so the
  // wall sits exactly on the edge of the field rather than cutting the curve.
  // Angles are degrees clockwise from 3 o'clock, and the arc is t thick because
  // its inner radius is t short of its outer one.
  cv.fillRect(x0,         y0 + R,     t, h - 2 * R, COL_SUIKA_WALL);
  cv.fillRect(x0 + w - t, y0 + R,     t, h - 2 * R, COL_SUIKA_WALL);
  cv.fillRect(x0 + R,     y0 + h - t, w - 2 * R, t, COL_SUIKA_WALL);

  cv.fillArc(x0 + R,     y0 + h - R, R - t, R,  90, 180, COL_SUIKA_WALL);
  cv.fillArc(x0 + w - R, y0 + h - R, R - t, R,   0,  90, COL_SUIKA_WALL);

  // The top stays open -- the fruits fall in through it -- so each top corner
  // gets its arc and a short lip inward and nothing else. That reads as a box
  // with a mouth rather than a sealed rectangle.
  cv.fillArc(x0 + R,     y0 + R, R - t, R, 180, 270, COL_SUIKA_WALL);
  cv.fillArc(x0 + w - R, y0 + R, R - t, R, 270, 360, COL_SUIKA_WALL);
  cv.fillRect(x0 + R,         y0, 7, t, COL_SUIKA_WALL);
  cv.fillRect(x0 + w - R - 7, y0, 7, t, COL_SUIKA_WALL);

  if (_lineHot) {
    for (int x = x0 + 3; x < x0 + w - 3; x += 6) {
      cv.drawFastHLine(x, SUIKA_LINE_Y, 3, COL_SUIKA_LINE);
    }
  }
}

void SuikaGame::drawHud(M5Canvas& cv) const {
  const int y0 = SUIKA_BOX_Y + SUIKA_BOX_H;

  cv.setTextFont(1);
  cv.setTextDatum(top_left);
  cv.setTextColor(COL_SUIKA_INK);
  cv.setTextSize(2);
  cv.drawString(String(_score), 4, y0 + 4);
  cv.setTextSize(1);
  drawHeart(cv, 8, y0 + 26, COL_SUIKA_HEART);
  cv.drawString(String(_high), 16, y0 + 22);

  cv.setTextDatum(middle_right);
  cv.drawString("NEXT", SUIKA_BOX_W - 26, y0 + 15);
  drawSuikaChip(cv, _next, SUIKA_BOX_W - 13.0f, (float)(y0 + 15), 8.0f);

  cv.setTextDatum(top_left);
}

void SuikaGame::drawBanner(M5Canvas& cv) const {
  cv.fillRoundRect(6, 84, SUIKA_BOX_W - 12, 72, 8, COL_SUIKA_BG);
  cv.drawRoundRect(6, 84, SUIKA_BOX_W - 12, 72, 8, COL_SUIKA_WALL);

  cv.setTextFont(1);
  cv.setTextDatum(middle_center);
  cv.setTextColor(COL_SUIKA_INK);
  cv.setTextSize(2);
  cv.drawString("GAME", SUIKA_BOX_W / 2, 102);
  cv.drawString("OVER", SUIKA_BOX_W / 2, 120);
  cv.setTextSize(1);
  const bool best = _score >= _high;
  cv.drawString(best ? "NEW BEST!" : "A:RETRY  B:BACK", SUIKA_BOX_W / 2, 142);
  if (best) {
    drawHeart(cv, SUIKA_BOX_W / 2 - 34, 142, COL_SUIKA_HEART);
    drawHeart(cv, SUIKA_BOX_W / 2 + 34, 142, COL_SUIKA_HEART);
  }
  cv.setTextDatum(top_left);
}

void SuikaGame::draw(M5Canvas& cv) {
  cv.fillScreen(COL_SUIKA_BG);
  drawField(cv);

  for (int i = 0; i < _n; ++i) {
    const Fruit& f = _f[i];
    drawSuikaFruit(cv, f.tier, SUIKA_BOX_X + f.x, SUIKA_BOX_Y + f.y, f.ang,
                   f.glee > 0);
  }

  drawSparks(cv);
  drawWalls(cv);

  if (_phase == PH_AIM) {
    // The dotted drop line is always up now. It used to be the feedback for
    // being locked on; with no lock it goes back to being what it looks like --
    // where this fruit lands if it is released now.
    for (int y = SUIKA_BOX_Y + 2; y < SUIKA_BOX_Y + 30; y += 5) {
      cv.drawFastVLine(ri(SUIKA_BOX_X + _aimX), y, 2, COL_SUIKA_WALL);
    }
    drawSuikaFruit(cv, _cur, SUIKA_BOX_X + _aimX, (float)SUIKA_PREVIEW_Y, 0.0f);
  }

  drawHud(cv);
  if (_phase == PH_OVER) drawBanner(cv);
}
