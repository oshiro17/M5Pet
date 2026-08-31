#include "GameMenu.h"
#include "PetState.h"
#include "SuikaFruits.h"

static const uint32_t BEAT_MS  = 700;              // per countdown digit
static const uint32_t COUNT_MS = BEAT_MS * 3;

static const int CARD_W = 60, CARD_H = 84, CARD_Y = 96;
static const int CARD_X[2] = { 6, GAME_W - 6 - CARD_W };

static const uint16_t COL_MENU_BG   = rgb565( 22,  24,  34);
static const uint16_t COL_MENU_CARD = rgb565( 44,  48,  64);
static const uint16_t COL_MENU_ON   = rgb565(255, 214,  64);
static const uint16_t COL_MENU_TEXT = rgb565(214, 218, 232);

void GameMenu::begin(uint32_t now) {
  _startedAt = 0;
  _lastMs    = now;
  _roll      = 0.0f;
  _armed     = true;
  _done      = false;
  _lastBeat  = -1;
}

void GameMenu::update(uint32_t now, bool tapA) {
  float realDt = (float)(now - _lastMs) * 0.001f;
  _lastMs = now;
  if (realDt > 0.25f) realDt = 0.25f;

  // Counting in: nothing to choose any more, just tick the beats out loud.
  if (_startedAt != 0) {
    const uint32_t el = now - _startedAt;
    const int beat = (int)(el / BEAT_MS);
    if (beat != _lastBeat && beat < 3) {
      _lastBeat = beat;
      M5.Speaker.tone(beat < 2 ? 660.0f : 990.0f, beat < 2 ? 60 : 140);
    }
    if (el >= COUNT_MS) _done = true;
    return;
  }

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (M5.Imu.getAccel(&ax, &ay, &az)) {
    const float raw = clampf(ax * GAME_ROLL_SIGN * GAME_TILT_GAIN, -1.0f, 1.0f);
    smoothTowards(_roll, raw, clampf(GAME_TILT_LOWPASS * realDt * TARGET_FPS, 0.0f, 1.0f));
  }

  // One step per deliberate tilt, not a continuous scroll: the list is two long
  // and a repeat rate would only ever overshoot it. `_armed` is cleared on a
  // step and only restored once the unit comes back near level.
  const float t = shapeTilt(_roll, GAME_TILT_DEAD, 1.0f);
  if (_armed && fabsf(t) > 0.45f) {
    const uint8_t want = (t < 0.0f) ? 0 : 1;
    if (want != _sel) {
      _sel = want;
      M5.Speaker.tone(760.0f, 25);
    }
    _armed = false;
  } else if (fabsf(t) < 0.20f) {
    _armed = true;
  }

  if (tapA) {
    _startedAt = now;
    M5.Speaker.tone(880.0f, 40);
  }
}

void GameMenu::drawCard(M5Canvas& cv, int slot, bool on) const {
  const int x = CARD_X[slot], y = CARD_Y;
  cv.fillRoundRect(x, y, CARD_W, CARD_H, 8, on ? COL_MENU_CARD : COL_MENU_BG);
  cv.drawRoundRect(x, y, CARD_W, CARD_H, 8, on ? COL_MENU_ON : COL_MENU_CARD);

  const int cx = x + CARD_W / 2, cy = y + 34;

  if (slot == BLOCKS) {
    static const uint16_t ROW[3] = {
      rgb565(255, 118, 152), rgb565(255, 200, 102), rgb565(140, 214, 255),
    };
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        cv.fillRect(cx - 21 + c * 15, cy - 22 + r * 8, 12, 5, ROW[r]);
      }
    }
    cv.fillCircle(cx, cy + 16, 6, COL_CHAR_BODY);
  } else {
    // A little stack, which is the whole idea of the game in one picture.
    drawSuikaChip(cv, 10, (float)cx,        (float)(cy + 12), 16.0f);
    drawSuikaChip(cv,  9, (float)(cx - 15), (float)(cy - 12), 11.0f);
    drawSuikaChip(cv,  4, (float)(cx + 15), (float)(cy - 14), 8.0f);
  }

  cv.setTextFont(1);
  cv.setTextSize(1);
  cv.setTextDatum(middle_center);
  cv.setTextColor(on ? COL_MENU_ON : COL_MENU_TEXT);
  cv.drawString(slot == BLOCKS ? "BLOCKS" : "SUIKA", cx, y + CARD_H - 12);
}

void GameMenu::draw(M5Canvas& cv) {
  cv.fillScreen(COL_MENU_BG);

  cv.setTextFont(1);
  cv.setTextDatum(middle_center);
  cv.setTextColor(COL_MENU_TEXT);
  cv.setTextSize(1);
  cv.drawString("SELECT A GAME", GAME_W / 2, 62);

  drawCard(cv, 0, _sel == 0);
  drawCard(cv, 1, _sel == 1);

  if (_startedAt == 0) {
    cv.setTextColor(rgb565(120, 126, 148));
    cv.drawString("TILT TO PICK", GAME_W / 2, 200);
    cv.drawString("A: START   B: BACK", GAME_W / 2, 214);
  } else {
    const int left = 3 - (int)((_lastMs - _startedAt) / BEAT_MS);
    cv.setTextSize(4);
    cv.setTextColor(COL_MENU_ON);
    cv.drawString(String(left < 1 ? 1 : left), GAME_W / 2, 206);
  }

  cv.setTextDatum(top_left);
  cv.setTextSize(1);
}
