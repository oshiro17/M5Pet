// Game.h -- tilt-controlled block breaker. The character is the ball.
//
// Control, in one paragraph: the accelerometer gives two independent axes and
// each drives exactly one thing. Rolling the unit left/right feeds an
// *acceleration* into the paddle rather than a position, so the board has
// weight -- it takes a moment to get going and coasts when you level off, and
// an expo curve around centre buys fine control without costing top speed.
// Pitching it fore/aft scales dt for the ball and its effects: tilt it towards
// you for slow motion, away for a rush. The physics is untouched either way;
// only how much time passes per frame changes.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"
#include "Trail.h"

class Game {
 public:
  void begin(uint32_t now);
  // btnA is a latched edge from the .ino, not M5.BtnA.wasPressed(): that flag
  // only survives a single M5.update(), and M5.update() runs far more often
  // than this does.
  void update(uint32_t now, bool btnA);
  void draw(M5Canvas& cv);

  // The .ino asks these; the game never touches app state itself.
  bool over() const { return _phase == PH_OVER; }
  int  score() const { return _score; }

 private:
  enum Phase : uint8_t {
    PH_READY = 0,   // ball parked on the paddle, waiting to launch
    PH_PLAY,
    PH_DIE,         // ball lost: it tumbles away
    PH_CLEAR,       // board cleared: celebrate, then next level
    PH_OVER
  };

  // --- setup
  void startLevel(int level, uint32_t now);
  void layoutBlocks(int level);
  void serve();

  // --- per-frame
  void readTilt(float realDt);
  void movePaddle(float realDt);
  void moveBall(float dt);
  void stepBall(float dt);            // one substep, with all collisions
  bool hitBlock(float& nx, float& ny);
  void bouncePaddle();
  void loseLife(uint32_t now);
  void setPhase(Phase p, uint32_t now);

  // --- drawing
  void drawBackdrop(M5Canvas& cv);
  void drawBlocks(M5Canvas& cv) const;
  void drawPaddle(M5Canvas& cv) const;
  void drawHud(M5Canvas& cv) const;
  void drawBanner(M5Canvas& cv) const;

  static inline float blockX(int c) { return GAME_FIELD_X + c * (GAME_BLOCK_W + GAME_BLOCK_GAP); }
  static inline float blockY(int r) { return GAME_FIELD_Y + r * (GAME_BLOCK_H + GAME_BLOCK_GAP); }

  Phase    _phase      = PH_READY;
  uint32_t _phaseStart = 0;
  uint32_t _lastMs     = 0;

  // tilt, already smoothed and de-zoned; both -1..1
  float _roll = 0.0f, _pitch = 0.0f;
  float _timeScale = 1.0f;

  // paddle
  float _padX = SCREEN_W * 0.5f;
  float _padV = 0.0f;
  float _padW = GAME_PAD_W_BASE;

  // ball
  float _bx = 0.0f, _by = 0.0f;
  float _bvx = 0.0f, _bvy = 0.0f;
  float _speed = GAME_BALL_SPEED0;   // target magnitude for this level
  float _squash = 0.0f;              // signed; + = flattened, - = stretched
  float _lean = -HALF_PI;            // facing, radians
  uint8_t _expr = 0;
  uint32_t _exprUntil = 0;

  // board
  uint8_t _hp[GAME_ROWS][GAME_COLS];
  int     _remaining = 0;

  // run
  int   _level = 1;
  int   _lives = GAME_LIVES;
  int   _score = 0;
  float _intensity = 0.0f;   // 0..1, ramps with level; thickens every effect

  Trail _trail;

  // backdrop: fixed field of drifting motes, denser and faster with the level
  struct Mote { float x, y, sp, ph; };
  Mote _motes[28];
  float _bgT = 0.0f;
};
