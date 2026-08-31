#include "Game.h"
#include "PetState.h"
#include "Character.h"

static inline int ri(float v) { return (int)lroundf(v); }
static inline int rr(float v) { const int i = (int)lroundf(v); return i < 1 ? 1 : i; }

// Row colours, top to bottom. Warm at the top so the rows the ball reaches
// last read as the "deep" ones.
static const uint16_t ROW_COL[GAME_ROWS] = {
  rgb565(255, 118, 152),   // pink
  rgb565(255, 152, 118),   // coral
  rgb565(255, 200, 102),   // amber
  rgb565(150, 226, 168),   // mint
  rgb565(140, 214, 255),   // aqua
  rgb565(178, 152, 255),   // violet
};

// Signed distance helper: how far a and b overlap on one axis, given half
// widths. Negative means they do not overlap at all.
static inline float overlap1D(float ac, float ah, float bc, float bh) {
  return (ah + bh) - fabsf(ac - bc);
}

// shapeTilt() moved to PetState.h -- the suika game reads the same axis the
// same way, and one copy of the curve keeps the two games feeling alike.

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void Game::begin(uint32_t now) {
  _level = 1;
  _lives = GAME_LIVES;
  _score = 0;
  _padX  = GAME_W * 0.5f;
  _padV  = 0.0f;
  _roll = _pitch = 0.0f;
  _timeScale = 1.0f;
  _trail.begin();

  for (int i = 0; i < (int)(sizeof(_motes) / sizeof(_motes[0])); ++i) {
    _motes[i].x  = randRange(0.0f, (float)GAME_W);
    _motes[i].y  = randRange(0.0f, (float)GAME_H);
    _motes[i].sp = randRange(6.0f, 24.0f);
    _motes[i].ph = randRange(0.0f, TWO_PI);
  }
  _bgT = 0.0f;
  _lastMs = now;

  startLevel(1, now);
}

void Game::startLevel(int level, uint32_t now) {
  _level     = level;
  _speed     = fminf(GAME_BALL_SPEED0 + (level - 1) * GAME_BALL_SPEED_PER_LEVEL,
                     GAME_BALL_SPEED_MAX);
  _padW      = fmaxf(GAME_PAD_W_BASE - (level - 1) * GAME_PAD_W_STEP,
                     GAME_PAD_W_MIN);
  _intensity = clampf((level - 1) / 6.0f, 0.0f, 1.0f);
  layoutBlocks(level);
  serve();
  setPhase(PH_READY, now);
}

// Five patterns, cycled. They change what the board *feels* like -- a solid
// wall, gaps to thread, a peak to chip away at -- without needing more art.
void Game::layoutBlocks(int level) {
  const int pattern = (level - 1) % 5;
  // Two-hit blocks start appearing at level 3, and creep down the board.
  const int toughRows = (level < 3) ? 0 : ((level < 6) ? 1 : 2);

  _remaining = 0;
  for (int r = 0; r < GAME_ROWS; ++r) {
    for (int c = 0; c < GAME_COLS; ++c) {
      bool on = true;
      switch (pattern) {
        case 0: on = true;                              break;  // full wall
        case 1: on = ((r + c) & 1) == 0;                break;  // checker
        // Pyramid: narrows a step every *two* rows, so with six rows it still
        // reaches the bottom instead of running out of width halfway down.
        case 2: on = (c >= r / 2) && (c < GAME_COLS - r / 2); break;
        case 3: on = (c & 1) == 0 || r == GAME_ROWS - 1; break; // columns + floor
        default:                                                 // hollow box
          on = (r == 0) || (r == GAME_ROWS - 1) ||
               (c == 0) || (c == GAME_COLS - 1);
          break;
      }
      _hp[r][c] = on ? (uint8_t)(r < toughRows ? 2 : 1) : 0;
      if (on) ++_remaining;
    }
  }
}

// Park the ball on the paddle. It launches upwards, angled slightly away from
// dead vertical so the first volley is never a straight up-and-down loop.
void Game::serve() {
  _padV = 0.0f;
  _bx = _padX;
  _by = GAME_PAD_Y - GAME_BALL_R - 1.0f;
  const float a = randRange(-0.45f, 0.45f);
  _bvx = sinf(a) * _speed;
  _bvy = -cosf(a) * _speed;
  _squash = 0.0f;
  _lean = -HALF_PI;
  _trail.clear();
}

void Game::setPhase(Phase p, uint32_t now) {
  _phase = p;
  _phaseStart = now;
}

// ---------------------------------------------------------------------------
// Tilt
// ---------------------------------------------------------------------------
void Game::readTilt(float realDt) {
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;

  // Portrait, so the axes are the other way round from the landscape screens:
  // the eyes take screen-X from ay, but here screen-X is the short edge of the
  // panel, which is ax. Pitch is then ay.
  const float rawRoll  = clampf(ax * GAME_ROLL_SIGN  * GAME_TILT_GAIN,  -1.0f, 1.0f);
  const float rawPitch = clampf(ay * GAME_PITCH_SIGN * GAME_PITCH_GAIN, -1.0f, 1.0f);

  // Frame-rate independent smoothing: the constants are tuned at 30 fps, so
  // scale the blend factor if a frame runs long.
  const float kR = clampf(GAME_TILT_LOWPASS  * realDt * TARGET_FPS, 0.0f, 1.0f);
  const float kP = clampf(GAME_PITCH_LOWPASS * realDt * TARGET_FPS, 0.0f, 1.0f);
  smoothTowards(_roll,  rawRoll,  kR);
  smoothTowards(_pitch, rawPitch, kP);

  // Fore/aft -> time scale. Held flat this lands exactly on 1.0 thanks to the
  // deadzone, so "normal speed" is a real, findable detent rather than a
  // knife edge.
  const float p = shapeTilt(_pitch, GAME_PITCH_DEAD, 1.35f);
  _timeScale = (p >= 0.0f) ? 1.0f + p * (GAME_TIME_MAX - 1.0f)
                           : 1.0f + p * (1.0f - GAME_TIME_MIN);
}

// ---------------------------------------------------------------------------
// Paddle
// ---------------------------------------------------------------------------
// Deliberately driven by *real* dt, not the scaled one: slow motion is there
// to give the player more time to react, and it would give none of that back
// if it slowed their own hands down too.
void Game::movePaddle(float realDt) {
  const float t = shapeTilt(_roll, GAME_TILT_DEAD, GAME_TILT_EXPO);

  _padV += t * GAME_PAD_ACCEL * realDt;
  // Exponential drag rather than a flat subtraction, so it eases to a stop
  // instead of stopping dead -- this is most of what "inertia" feels like.
  _padV *= expf(-GAME_PAD_DRAG * realDt);
  _padV = clampf(_padV, -GAME_PAD_VMAX, GAME_PAD_VMAX);

  _padX += _padV * realDt;

  const float half = _padW * 0.5f;
  if (_padX < half) {
    _padX = half;
    _padV = fabsf(_padV) * GAME_PAD_WALL_BOUNCE;
  } else if (_padX > GAME_W - half) {
    _padX = GAME_W - half;
    _padV = -fabsf(_padV) * GAME_PAD_WALL_BOUNCE;
  }
}

// ---------------------------------------------------------------------------
// Ball
// ---------------------------------------------------------------------------
// Split the frame into substeps short enough that the ball can never pass
// through a 9 px block in one go. At the top speed and the fastest time scale
// that is about six substeps; at rest it is one.
void Game::moveBall(float dt) {
  const float dist = hypotf(_bvx, _bvy) * dt;
  int steps = (int)ceilf(dist / GAME_SUBSTEP_PX);
  if (steps < 1) steps = 1;
  if (steps > 8) steps = 8;
  const float sub = dt / steps;
  for (int i = 0; i < steps && _phase == PH_PLAY; ++i) stepBall(sub);
}

void Game::stepBall(float dt) {
  _bx += _bvx * dt;
  _by += _bvy * dt;

  const float R = GAME_BALL_R;

  // --- walls. A negative squash is a horizontal squeeze (narrow and tall),
  //     which is what hitting a vertical wall should look like.
  if (_bx < R)              { _bx = R;              _bvx =  fabsf(_bvx); _squash = -GAME_SQUASH_MAX; }
  else if (_bx > GAME_W - R) { _bx = GAME_W - R; _bvx = -fabsf(_bvx); _squash = -GAME_SQUASH_MAX; }
  if (_by < R)              { _by = R;              _bvy =  fabsf(_bvy); _squash =  GAME_SQUASH_MAX; }

  // --- blocks
  float nx = 0.0f, ny = 0.0f;
  if (hitBlock(nx, ny)) {
    if (nx != 0.0f) { _bvx = fabsf(_bvx) * nx; _squash = -GAME_SQUASH_MAX * 0.8f; }
    if (ny != 0.0f) { _bvy = fabsf(_bvy) * ny; _squash =  GAME_SQUASH_MAX * 0.8f; }
    // Creeps faster as the board empties, so the last few blocks are the
    // tense ones.
    _speed = fminf(_speed + GAME_BALL_SPEED_PER_HIT, GAME_BALL_SPEED_MAX);
  }

  // --- paddle. Only catches a descending ball, so a ball that has somehow got
  //     below the board cannot be batted back up from underneath.
  if (_bvy > 0.0f) {
    const float padTop = GAME_PAD_Y;
    if (_by + R >= padTop && _by - R <= padTop + GAME_PAD_H &&
        fabsf(_bx - _padX) <= _padW * 0.5f + R * 0.6f) {
      _by = padTop - R;
      bouncePaddle();
    }
  }
}

// Axis-aligned overlap test against every live block. Whichever axis is less
// deeply penetrated is the face the ball came in through, so that is the one
// that gets reflected -- this is what makes corner hits behave sensibly
// instead of flipping both components.
bool Game::hitBlock(float& nx, float& ny) {
  const float R = GAME_BALL_R;
  for (int r = 0; r < GAME_ROWS; ++r) {
    for (int c = 0; c < GAME_COLS; ++c) {
      if (!_hp[r][c]) continue;
      const float cx = blockX(c) + GAME_BLOCK_W * 0.5f;
      const float cy = blockY(r) + GAME_BLOCK_H * 0.5f;
      const float ox = overlap1D(_bx, R, cx, GAME_BLOCK_W * 0.5f);
      const float oy = overlap1D(_by, R, cy, GAME_BLOCK_H * 0.5f);
      if (ox <= 0.0f || oy <= 0.0f) continue;

      if (ox < oy) {
        nx = (_bx < cx) ? -1.0f : 1.0f;
        _bx = cx + nx * (GAME_BLOCK_W * 0.5f + R);
      } else {
        ny = (_by < cy) ? -1.0f : 1.0f;
        _by = cy + ny * (GAME_BLOCK_H * 0.5f + R);
      }

      if (--_hp[r][c] == 0) {
        --_remaining;
        _score += 10 * _level;
        _trail.burst(cx, cy, ROW_COL[r], 8 + (int)(6 * _intensity),
                     1.0f + 0.5f * _intensity);
      } else {
        _score += 4 * _level;
        _trail.burst(cx, cy, ROW_COL[r], 4, 0.7f);
      }
      _expr = EXPR_EATING;
      _exprUntil = millis() + 260;
      return true;
    }
  }
  return false;
}

// Where on the board it lands decides the angle; how fast the board is moving
// adds a little sideways bias on top. Speed is then renormalised, so a
// technical shot changes the ball's *direction* without quietly making it
// faster and faster.
void Game::bouncePaddle() {
  const float rel = clampf((_bx - _padX) / (_padW * 0.5f), -1.0f, 1.0f);
  const float ang = rel * GAME_BOUNCE_MAX_ANGLE;

  float vx = sinf(ang) * _speed + _padV * GAME_PAD_SPIN;
  float vy = -cosf(ang) * _speed;

  const float mag = hypotf(vx, vy);
  if (mag > 0.001f) { vx = vx / mag * _speed; vy = vy / mag * _speed; }

  // Never let it flatten out into a horizontal rally the player cannot end.
  const float minVy = _speed * GAME_BOUNCE_MIN_VY;
  if (fabsf(vy) < minVy) {
    vy = -minVy;
    const float rem = _speed * _speed - vy * vy;
    vx = (vx < 0.0f ? -1.0f : 1.0f) * sqrtf(rem > 0.0f ? rem : 0.0f);
  }

  _bvx = vx;
  _bvy = vy;
  _squash = GAME_SQUASH_MAX;
  _expr = EXPR_HAPPY;
  _exprUntil = millis() + 320;
}

void Game::loseLife(uint32_t now) {
  --_lives;
  _trail.burst(_bx, GAME_H - 4.0f, COL_CHAR_BODY, 14, 1.2f);
  setPhase(PH_DIE, now);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void Game::update(uint32_t now, bool btnA) {
  // Measured, not assumed: a long frame should not make the paddle jump.
  float realDt = (now - _lastMs) * 0.001f;
  _lastMs = now;
  realDt = clampf(realDt, 0.001f, 0.10f);

  readTilt(realDt);
  movePaddle(realDt);

  const float dt = realDt * _timeScale;
  _bgT += dt;

  if (_exprUntil && now >= _exprUntil) { _expr = EXPR_NORMAL; _exprUntil = 0; }
  smoothTowards(_squash, 0.0f, clampf(GAME_SQUASH_DECAY * dt, 0.0f, 1.0f));

  switch (_phase) {
    case PH_READY:
      // Rides the paddle until launch. Button A serves; so does simply waiting,
      // so the game is playable without ever pressing anything.
      _bx = _padX;
      _by = GAME_PAD_Y - GAME_BALL_R - 1.0f;
      _trail.clear();
      if (btnA || (now - _phaseStart) > 1800) {
        setPhase(PH_PLAY, now);
      }
      break;

    case PH_PLAY: {
      moveBall(dt);
      const float sp = hypotf(_bvx, _bvy);
      if (sp > 1.0f) _lean = atan2f(_bvy, _bvx);
      _trail.track(_bx, _by, sp, _intensity, dt);
      if (_by - GAME_BALL_R > GAME_H) loseLife(now);
      else if (_remaining <= 0) {
        _trail.burst(_bx, _by, COL_STAR, 18, 1.4f);
        setPhase(PH_CLEAR, now);
      }
      break;
    }

    case PH_DIE:
      // It keeps falling and spinning for a beat, then either serves again or
      // the run ends.
      _by += 90.0f * dt;
      _lean += 6.0f * dt;
      // Keep feeding the ribbon: without this it would hang in the air at the
      // point of death while the character falls out from under it.
      _trail.track(_bx, _by, 90.0f, _intensity, dt);
      if (now - _phaseStart > 900) {
        if (_lives > 0) { serve(); setPhase(PH_READY, now); }
        else            { setPhase(PH_OVER, now); }
      }
      break;

    case PH_CLEAR:
      _expr = EXPR_HAPPY;
      _by -= 40.0f * dt;
      _trail.track(_bx, _by, 40.0f, _intensity, dt);
      if ((now - _phaseStart) % 160 < 20) {
        _trail.burst(randRange(18.0f, GAME_W - 18.0f),
                     randRange(30.0f, 150.0f), COL_STAR, 4, 0.9f);
      }
      if (now - _phaseStart > 1400) {
        _score += 50 * _level;
        startLevel(_level + 1, now);
      }
      break;

    case PH_OVER:
      if (btnA) begin(now);
      break;
  }

  _trail.update(dt);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
// Motes drift downwards; past normal speed they stretch into streaks, which is
// the cheapest honest way to show that time itself has sped up.
void Game::drawBackdrop(M5Canvas& cv) {
  const int n = (int)(sizeof(_motes) / sizeof(_motes[0]));
  const int live = 10 + (int)(_intensity * (n - 10));
  const float streak = clampf((_timeScale - 1.0f) * 12.0f, 0.0f, 14.0f);
  const uint16_t tint = mixColor(COL_TRAIL_AQUA, COL_TRAIL_PINK,
                                 0.5f + 0.5f * sinf(_level * 0.7f));

  for (int i = 0; i < live; ++i) {
    Mote& m = _motes[i];
    m.y += m.sp * _timeScale * 0.033f;
    if (m.y > GAME_H) { m.y -= GAME_H; m.x = randRange(0.0f, (float)GAME_W); }
    const float tw = 0.45f + 0.55f * sinf(_bgT * 2.0f + m.ph);
    const uint16_t c = mixColor(COL_BG, tint, 0.10f + 0.22f * tw);
    if (streak > 1.0f) cv.drawFastVLine(ri(m.x), ri(m.y), rr(streak), c);
    else               cv.drawPixel(ri(m.x), ri(m.y), c);
  }
}

void Game::drawBlocks(M5Canvas& cv) const {
  for (int r = 0; r < GAME_ROWS; ++r) {
    for (int c = 0; c < GAME_COLS; ++c) {
      if (!_hp[r][c]) continue;
      const int x = ri(blockX(c)), y = ri(blockY(r));
      // A cracked two-hit block is drawn dimmer with a bright cap, so its
      // state is readable at a glance without a second colour.
      const uint16_t base = ROW_COL[r];
      const uint16_t body = (_hp[r][c] > 1) ? mixColor(COL_BG, base, 0.55f) : base;
      cv.fillRect(x, y, GAME_BLOCK_W, GAME_BLOCK_H, body);
      cv.drawFastHLine(x, y, GAME_BLOCK_W, mixColor(base, COL_WHITE, 0.45f));
      if (_hp[r][c] > 1) {
        cv.drawFastHLine(x + 4, y + GAME_BLOCK_H - 1, GAME_BLOCK_W - 8,
                         mixColor(base, COL_BG, 0.4f));
      }
    }
  }
}

void Game::drawPaddle(M5Canvas& cv) const {
  const int w = rr(_padW), x = ri(_padX - _padW * 0.5f), y = ri(GAME_PAD_Y);
  // Leading edge brightens with speed, so you can see the board is carrying
  // momentum even when the tilt is already back at centre.
  const float f = clampf(fabsf(_padV) / GAME_PAD_VMAX, 0.0f, 1.0f);
  cv.fillRoundRect(x, y, w, GAME_PAD_H, 2, COL_CHAR_BODY);
  cv.drawFastHLine(x + 1, y, w - 2, mixColor(COL_CHAR_LIGHT, COL_WHITE, f));
  if (f > 0.15f) {
    const int lead = (_padV > 0.0f) ? x + w - 1 : x;
    cv.drawFastVLine(lead, y, GAME_PAD_H, mixColor(COL_CHAR_BODY, COL_WHITE, f));
  }
}

// Two lines, because 135 px will not hold level, score, lives and the speed
// bar side by side. Top row is state you glance at mid-rally (level, speed,
// lives); the score sits underneath where it can be ignored.
void Game::drawHud(M5Canvas& cv) const {
  cv.setTextFont(1);
  cv.setTextSize(1);
  cv.setTextDatum(top_left);
  cv.setTextColor(mixColor(COL_BG, COL_WHITE, 0.65f));
  cv.drawString(String("L") + _level, 3, 2);
  cv.drawString(String(_score), 3, 12);

  // lives, as small pink dots
  for (int i = 0; i < _lives; ++i) {
    cv.fillCircle(GAME_W - 6 - i * 9, 6, 3, COL_CHAR_BODY);
  }

  // Time scale, as a short bar. Centre notch = normal speed.
  const int bx = GAME_W / 2 - 18, by = 4, bw = 36;
  cv.drawFastHLine(bx, by + 2, bw, mixColor(COL_BG, COL_WHITE, 0.25f));
  cv.drawFastVLine(bx + bw / 2, by, 5, mixColor(COL_BG, COL_WHITE, 0.45f));
  const float f = (_timeScale >= 1.0f)
      ? (_timeScale - 1.0f) / (GAME_TIME_MAX - 1.0f)
      : -(1.0f - _timeScale) / (1.0f - GAME_TIME_MIN);
  const int mx = bx + bw / 2 + (int)(f * bw * 0.5f);
  cv.fillCircle(mx, by + 2, 2, (f >= 0.0f) ? COL_STAR : COL_TRAIL_AQUA);
}

void Game::drawBanner(M5Canvas& cv) const {
  const char* line = nullptr;
  switch (_phase) {
    case PH_READY: line = "TILT TO MOVE";  break;
    case PH_CLEAR: line = "CLEAR!";        break;
    case PH_OVER:  line = "GAME OVER";     break;
    default: return;
  }
  cv.setTextFont(1);
  cv.setTextDatum(middle_center);
  cv.setTextColor(COL_WHITE);
  if (_phase == PH_READY) {
    // Just above the paddle, where the eye already is while waiting to serve.
    cv.setTextSize(1);
    cv.drawString(line, GAME_W / 2, 198);
  } else {
    // "GAME OVER" at size 2 is 108 px wide -- the widest thing that fits.
    cv.setTextSize(2);
    cv.drawString(line, GAME_W / 2, 112);
  }
  if (_phase == PH_OVER) {
    cv.setTextSize(1);
    cv.setTextColor(mixColor(COL_BG, COL_WHITE, 0.7f));
    cv.drawString(String("SCORE ") + _score, GAME_W / 2, 136);
    cv.drawString("A: RETRY", GAME_W / 2, 152);
    cv.drawString("B: BACK",  GAME_W / 2, 164);
  }
  cv.setTextDatum(top_left);
  cv.setTextSize(1);
}

void Game::draw(M5Canvas& cv) {
  drawBackdrop(cv);
  drawBlocks(cv);
  drawPaddle(cv);

  _trail.drawRibbon(cv);
  if (_phase != PH_OVER) {
    // Glow grows with speed and level: the faster it goes, the more it burns.
    const float sp = hypotf(_bvx, _bvy);
    const float glow = clampf(sp / GAME_BALL_SPEED_MAX, 0.0f, 1.0f) *
                       (0.5f + 0.5f * _intensity);
    drawCharacterBall(cv, _bx, _by, GAME_BALL_R, _squash, _lean, _expr, glow);
  }
  _trail.drawParticles(cv);

  drawHud(cv);
  drawBanner(cv);
}
