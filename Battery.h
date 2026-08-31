// Battery.h -- the cell's voltage, and the one question worth asking about it:
// is this thing running its own battery flat?
//
// There is no PMIC and no VBUS pin on a Plus2, so everything here is inferred
// from a single ADC reading on GPIO38. See Config.h for what the thresholds
// mean and why the guard has to stand down when the reading looks impossible.
#pragma once
#include <stdint.h>

class Battery {
 public:
  // One reading, so onBattery() can be asked immediately. The streak is left
  // empty on purpose: judging it needs samples a second apart, which is the
  // only spacing that can tell a falling cell from a charging one.
  void begin();
  void update(uint32_t now);   // samples on its own schedule; call every frame

  uint16_t mv() const { return _mv; }

  // True while the cell is the only thing keeping the lights on. A charged node
  // reads above 4.2V; a reading below the protection IC's cutoff means there is
  // no cell on the pins -- and we are still running, so USB is carrying us.
  bool onBattery() const;

  // Low for BATT_LOW_STREAK samples in a row while on battery. One dip does not
  // count: the backlight and the buzzer pull the rail down hard enough to fake
  // an empty cell for a moment.
  bool spent() const;

  // Low right now, one reading, no history. Enough to say something to whoever
  // is holding it; not enough to cut power on.
  bool low() const;

 private:
  void sample();

  uint16_t _mv     = 0;
  uint16_t _prev    = 0;
  uint8_t  _streak = 0;
  uint32_t _next   = 0;
};

extern Battery battery;
