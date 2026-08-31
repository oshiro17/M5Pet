#include <M5Unified.h>

#include "Config.h"
#include "Battery.h"

Battery battery;

// The divider on GPIO38 is noisy enough to cross a threshold on its own, so
// every sample is an average of a few reads rather than a single one.
void Battery::sample() {
  uint32_t sum = 0;
  for (int i = 0; i < 4; ++i) { sum += M5.Power.getBatteryVoltage(); }
  _mv = (uint16_t)(sum / 4);

  // Climbing means a charger is winning, whatever the absolute reading says --
  // so it breaks the streak. On the first sample _prev is 0 and this reads as a
  // rise, which is harmless: the streak starts empty anyway.
  const bool rising = _mv > (uint32_t)_prev + BATT_RISE_MV;
  _prev = _mv;

  // The streak only counts while the cell is actually the supply and actually
  // going down. Anything else resets it, so plugging in clears a shutdown that
  // was one sample away.
  if (onBattery() && !rising && _mv < BATT_CUTOFF_MV) {
    if (_streak < 255) { ++_streak; }
  } else {
    _streak = 0;
  }
}

void Battery::begin() {
  sample();
  _streak = 0;
  _next = millis() + BATT_SAMPLE_MS;
}

void Battery::update(uint32_t now) {
  if ((int32_t)(now - _next) < 0) { return; }
  _next = now + BATT_SAMPLE_MS;
  sample();
}

bool Battery::onBattery() const {
  return _mv >= BATT_ABSENT_MV && _mv <= BATT_USB_MV;
}

bool Battery::spent() const {
  return BATT_GUARD_ENABLED && _streak >= BATT_LOW_STREAK;
}

bool Battery::low() const {
  return BATT_GUARD_ENABLED && onBattery() && _mv < BATT_CUTOFF_MV;
}
