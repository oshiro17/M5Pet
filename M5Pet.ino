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
// Buttons: A or B skips the intro. On the face, A opens the game menu and B
// does nothing; in the menu, tilt picks and A counts in; in a game, B goes back
// to the face. The eyes wake and startle from the IMU instead -- sit still and
// they doze off. Holding the power button closes the eyes and shuts down.

#include <M5Unified.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <driver/gpio.h>   // pad hold, for the power latch across deep sleep

#include "Config.h"
#include "Battery.h"
#include "PetState.h"
#include "BootAnimation.h"
#include "Eyes.h"
#include "Game.h"
#include "GameMenu.h"
#include "SuikaGame.h"

static M5Canvas      canvas(&M5.Display);   // off-screen frame buffer
static PetState      petState;
static BootAnimation bootAnim;
static Eyes          eyes;
static Game          game;
static GameMenu      menu;
static SuikaGame     suika;
static uint32_t      lastFrame = 0;
static bool          powerHeld = false;
static bool          dozingOff = false;
static uint8_t       bootLoop  = 0;      // passes of the intro finished so far
static bool          tapA      = false;   // button edges, latched between frames
static bool          tapB      = false;

// Point the panel a given way round and rebuild the frame buffer to match.
// The pet screens are laid out for 240x135 and the game for 135x240, so the
// two cannot share one sprite -- but they are the same number of pixels, so
// freeing one and allocating the other always fits where the first one did.
static void setOrientation(int rotation, int w, int h) {
  M5.Display.setRotation(rotation);
  M5.Display.fillScreen(COL_BG);

  canvas.deleteSprite();
  // 16bpp is 64 KB; fall back to 8bpp if the allocation ever fails.
  canvas.setColorDepth(16);
  if (!canvas.createSprite(w, h)) {
    canvas.setColorDepth(8);
    canvas.createSprite(w, h);
  }
  canvas.fillScreen(COL_BG);
}

// ------------------------------------------------------- shutting down --
// Two ways to stop, and the difference is the whole point of the battery guard.
// A nap keeps the 3.3V rail up and still costs a few mA; a cut opens the GPIO4
// latch and takes the cell out of the circuit entirely. Only the cut protects a
// battery. Both live up here because the escalation on wake, the guard in the
// loop, and the power button all have to choose between the same two.
static void cutPower() {
  M5.Display.setBrightness(0);
  M5.Display.sleep();
  M5.Display.waitDisplay();
  M5.Power.powerOff();
}

static void nap() {
  // No PMIC here -- on battery the 3.3V rail is held up by GPIO4 alone -- and
  // the ESP32 releases every pad on its way into deep sleep. M5Unified's
  // deepSleep() does not latch it, so without these two calls the nap is a
  // power cut: GPIO4 drops, the latch opens, and there is nothing left for a
  // button to wake. gpio_hold_en locks the pad and gpio_deep_sleep_hold_en is
  // what makes that lock survive the sleep itself.
  gpio_hold_en(GPIO_NUM_4);
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);
  // The timer is the over-discharge escalation: sleeping forever on battery is
  // a slow way of killing the cell, so the nap ends by itself and setup()
  // decides whether to turn the nap into a cut. touch_wakeup stays false
  // because ext0 holds exactly one pin -- left at its default, deepSleep()
  // re-registers the power button over the line above and button A stops waking
  // anything. A non-zero timer and our ext0 coexist; deepSleep() only adds the
  // timer source.
  M5.Power.deepSleep((uint64_t)DEEP_SLEEP_ESCALATE_MS * 1000ULL, false);
}

// Not a silent death: a unit that shuts itself down should say why, or the next
// person to pick it up files it as broken.
static void lowBattNotice() {
  M5.Display.setRotation(SCREEN_ROTATION);
  M5.Display.setBrightness(SCREEN_BRIGHTNESS);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(6, 22);
  M5.Display.print("LOW BATT");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 52);
  M5.Display.printf("%umV", battery.mv());
  M5.Display.setCursor(6, 66);
  M5.Display.print("charge me");
  delay(BATT_NOTICE_MS);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Let go of the power latch. Waking from deep sleep arrives with GPIO4 still
  // pad-locked by the sleep path below, and a locked pad ignores writes -- so
  // leaving it on would make the unit impossible to switch off ever again:
  // powerOff() works by pulling that same pin low. Releasing it after M5.begin()
  // rather than before is deliberate. A pad follows its output register the
  // moment the lock lifts, and M5.begin() is what drives GPIO4 high, so doing it
  // in this order hands the latch over without a gap. Both calls are harmless on
  // a cold start, where there is no hold to release.
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(GPIO_NUM_4);

  M5.Display.setBrightness(SCREEN_BRIGHTNESS);
  setOrientation(SCREEN_ROTATION, SCREEN_W, SCREEN_H);

  battery.begin();

  // The nap's timer went off. On battery that is the escalation firing: turn
  // the nap into a real cut, because a cell cannot afford to be slept on for
  // days. On USB there is nothing to protect and a cut would only reboot us, so
  // go straight back to sleep and let the timer ask again later.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    if (battery.onBattery()) { cutPower(); }
    nap();
  }

  // Booting on a cell this low, say so before the apple animation gets a look
  // in. Saying so is all this does: one reading cannot tell a dying cell from a
  // charging one, so the cut is left to the guard in the loop, which by then has
  // seen which way the voltage is going. The few seconds that costs are worth
  // less than the reboot loop a wrong cut on USB would start.
  if (battery.low()) {
    lowBattNotice();
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

  // M5Unified's wasPressed() is true for exactly one M5.update() -- the one
  // that saw the transition. M5.update() runs every millisecond here but the
  // frame below only runs every 33, so reading the buttons down there would
  // miss roughly 32 presses out of 33. Latch the edges up here instead and
  // consume them once the frame actually runs.
  if (M5.BtnA.wasPressed()) tapA = true;
  if (M5.BtnB.wasPressed()) tapB = true;

  // Frame pacing without blocking: no long delay() anywhere in here.
  const uint32_t now = millis();

  // The over-discharge guard sits ahead of the frame and ahead of the state
  // machine on purpose. It has to hold whatever is on screen -- the face, a
  // game, the menu -- because which screen is up has nothing to do with how
  // much charge is left, and every extra second of running is charge the cell
  // does not get back.
  battery.update(now);
  if (battery.spent()) {
    lowBattNotice();
    cutPower();
  }
  if ((uint32_t)(now - lastFrame) < FRAME_INTERVAL_MS) {
    delay(1);
    return;
  }
  lastFrame = now;

  // one clear per frame
  canvas.fillScreen(COL_BG);

  switch (petState.state()) {
    case AppState::BOOT:
      // A button skips the intro outright, not just the pass it is in the middle
      // of: whoever presses it wants the face, and making them press it once per
      // loop would be a worse version of no skip at all.
      if (tapA || tapB) {
        bootLoop = BOOT_LOOPS - 1;
        bootAnim.skip();
      }
      bootAnim.update(now);
      bootAnim.draw(canvas);
      if (bootAnim.finished()) {
        if (++bootLoop < BOOT_LOOPS) {
          bootAnim.begin(now);           // round two, from a clean slate
        } else {
          petState.set(AppState::EYES, now);
          eyes.begin(now);
        }
      }
      break;

    case AppState::EYES:
      // Button A hands the screen over to the game menu. Nothing else about the
      // face changes; it picks up where it left off on the way back.
      if (tapA) {
        petState.set(AppState::MENU, now);
        setOrientation(GAME_ROTATION, GAME_W, GAME_H);   // menu and games are portrait
        menu.begin(now);
        break;
      }

      // The eyes close while the power button is held -- and also, all by
      // themselves, once nothing has moved for long enough.
      dozingOff = DEEP_SLEEP_ENABLED &&
                  eyes.idleFor(now) > DEEP_SLEEP_AFTER_MS;
      powerHeld = M5.BtnPWR.isPressed();
      eyes.setPowerHold(powerHeld || dozingOff, now);

      eyes.update(now);
      eyes.draw(canvas);
      break;

    case AppState::MENU:
      if (tapB) {
        petState.set(AppState::EYES, now);
        setOrientation(SCREEN_ROTATION, SCREEN_W, SCREEN_H);
        eyes.begin(now);
        break;
      }
      menu.update(now, tapA);
      menu.draw(canvas);
      // The countdown has run out. Both games use the canvas the menu is
      // already holding, so there is no orientation change on this hop.
      if (menu.finished()) {
        if (menu.choice() == GameMenu::SUIKA) {
          petState.set(AppState::SUIKA, now);
          suika.begin(now);
        } else {
          petState.set(AppState::GAME, now);
          game.begin(now);
        }
      }
      break;

    case AppState::GAME:
      // No idle sleep in here: the player is holding the thing and tilting it,
      // and the eyes' activity timer is not running to notice.
      if (tapB) {
        petState.set(AppState::EYES, now);
        setOrientation(SCREEN_ROTATION, SCREEN_W, SCREEN_H);
        eyes.begin(now);
        break;
      }
      game.update(now, tapA);
      game.draw(canvas);
      break;

    case AppState::SUIKA:
      if (tapB) {
        petState.set(AppState::EYES, now);
        setOrientation(SCREEN_ROTATION, SCREEN_W, SCREEN_H);
        eyes.begin(now);
        break;
      }
      suika.update(now, tapA);
      suika.draw(canvas);
      break;
  }

  // Every edge is spent now, whether or not this frame's state used it. Letting
  // one survive would leak a press across a state change -- the A that starts
  // the countdown would arrive again as the first fruit's drop.
  tapA = tapB = false;

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
      cutPower();
    } else {
      // Drifted off on its own, so this is a nap and button A gets it back --
      // but a nap with an end to it now. See nap() for why sleeping forever is
      // not an option on a cell.
      nap();
    }
  }
}
