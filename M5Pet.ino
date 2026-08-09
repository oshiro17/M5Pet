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
// Buttons: A = startle the eyes, B = wake it up. Either skips the intro.
// Holding the power button closes the eyes and shuts down.

#include <M5Unified.h>
#include <esp_system.h>

#include "Config.h"
#include "PetState.h"
#include "BootAnimation.h"
#include "Eyes.h"
#include "Stars.h"

static M5Canvas      canvas(&M5.Display);   // off-screen frame buffer
static PetState      petState;
static Stars         stars;
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
  stars.begin();
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

  // one clear per frame; the starfield is the backdrop everything sits on
  canvas.fillScreen(COL_BG);

  switch (petState.state()) {
    case AppState::BOOT:
      if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) bootAnim.skip();
      bootAnim.update(now);
      stars.draw(canvas, now, bootAnim.brightness());
      bootAnim.draw(canvas);
      if (bootAnim.finished()) {
        petState.set(AppState::EYES, now);
        eyes.begin(now);
      }
      break;

    case AppState::EYES:
      if (M5.BtnA.wasClicked()) eyes.surprise(now);
      if (M5.BtnB.wasClicked()) eyes.wake(now);

      // Power button: the eyes close for as long as it is held.
      eyes.setPowerHold(M5.BtnPWR.isPressed(), now);

      eyes.update(now);
      if (STARS_ON_EYES) stars.draw(canvas, now);
      eyes.draw(canvas);
      break;
  }

  canvas.pushSprite(0, 0);

  // Fully shut: dim the backlight the rest of the way, then cut power. Blocking
  // here is fine -- there is nothing left to animate.
  if (petState.state() == AppState::EYES && eyes.readyToPowerOff()) {
    for (int b = SCREEN_BRIGHTNESS; b >= 0; b -= 6) {
      M5.Display.setBrightness((uint8_t)(b < 0 ? 0 : b));
      delay(14);
    }
    M5.Display.fillScreen(COL_BG);
    M5.Power.powerOff();
  }
}
