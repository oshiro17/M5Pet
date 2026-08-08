// M5Pet -- M5StickC Plus2 desk pet.
//
//   power on -> a whole apple -> a pink round character bounces in, takes one
//   bite out of the right side, chews, looks pleased, leaves -> the bitten
//   apple flares white -> fade -> a face made of two very large eyes that look
//   around on their own.
//
// Board:  M5StickC Plus2   (m5stack:esp32:m5stack_stickc_plus2)
// Libs:   M5Unified (pulls in M5GFX)
//
// Buttons: A = startle the eyes / skip the intro, B = skip the intro.

#include <M5Unified.h>
#include <esp_system.h>

#include "Config.h"
#include "PetState.h"
#include "BootAnimation.h"
#include "Eyes.h"

static M5Canvas      canvas(&M5.Display);   // off-screen frame buffer
static PetState      petState;
static BootAnimation bootAnim;
static Eyes          eyes;
static uint32_t      lastFrame = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(SCREEN_ROTATION);
  M5.Display.setBrightness(SCREEN_BRIGHTNESS);
  M5.Display.fillScreen(COL_BG);

  // 16bpp is 64 KB; fall back to 8bpp if the allocation ever fails.
  canvas.setColorDepth(16);
  if (!canvas.createSprite(SCREEN_W, SCREEN_H)) {
    canvas.setColorDepth(8);
    canvas.createSprite(SCREEN_W, SCREEN_H);
  }

  randomSeed(esp_random());

  const uint32_t now = millis();
  petState.begin(now);
  bootAnim.begin(now);
  lastFrame = now;
}

void loop() {
  M5.update();

  // Frame pacing without blocking: no long delay() anywhere in here.
  const uint32_t now = millis();
  if ((uint32_t)(now - lastFrame) < FRAME_INTERVAL_MS) {
    delay(1);
    return;
  }
  lastFrame = now;

  switch (petState.state()) {
    case AppState::BOOT:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) bootAnim.skip();
      bootAnim.update(now);
      bootAnim.draw(canvas);
      if (bootAnim.finished()) {
        petState.set(AppState::EYES, now);
        eyes.begin(now);
      }
      break;

    case AppState::EYES:
      if (M5.BtnA.wasPressed()) eyes.surprise(now);
      if (M5.BtnB.wasPressed()) eyes.wake(now);
      eyes.update(now);
      eyes.draw(canvas);
      break;
  }

  canvas.pushSprite(0, 0);
}
