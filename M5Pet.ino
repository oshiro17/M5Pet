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
// Buttons: A or B skips the intro; neither does anything after that.
// The eyes wake and startle from the IMU instead -- sit still and they doze off.
// Holding the power button closes the eyes and shuts down.

#include <M5Unified.h>
#include <esp_system.h>
#include <esp_sleep.h>

#include "Config.h"
#include "PetState.h"
#include "BootAnimation.h"
#include "Eyes.h"

static M5Canvas      canvas(&M5.Display);   // off-screen frame buffer
static PetState      petState;
static BootAnimation bootAnim;
static Eyes          eyes;
static uint32_t      lastFrame = 0;
static bool          powerHeld = false;
static bool          dozingOff = false;

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
  // Waking from deep sleep goes straight back to the face; the apple story is
  // for a cold start only.
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
    petState.set(AppState::EYES, now);
    eyes.begin(now);
  } else {
    bootAnim.begin(now);
  }
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

  // one clear per frame
  canvas.fillScreen(COL_BG);

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
      // The eyes close while the power button is held -- and also, all by
      // themselves, once nothing has moved for long enough.
      dozingOff = DEEP_SLEEP_ENABLED &&
                  eyes.idleFor(now) > DEEP_SLEEP_AFTER_MS;
      powerHeld = M5.BtnPWR.isPressed();
      eyes.setPowerHold(powerHeld || dozingOff, now);

      eyes.update(now);
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
    if (powerHeld) {
      M5.Power.powerOff();
    } else {
      // drifted off on its own: sleep instead of dying, and let button A wake it
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);
      M5.Power.deepSleep();
    }
  }
}
