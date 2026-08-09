// Stars.h -- yellow five-pointed stars scattered around the boot scene.
//
// They do not drift: each one sits at a hand-picked spot that frames the apple
// and simply spins where it stands, twinkling on its own clock.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

class Stars {
 public:
  void begin();
  // scale 0..1 dims the whole field, so it fades in and out with the scene
  void draw(M5Canvas& cv, uint32_t now, float scale = 1.0f);

 private:
  struct Star {
    float ang;     // starting rotation
    float spin;    // rad/s, sign alternates so they don't all turn together
    float phase;   // twinkle offset
  };
  Star _s[STAR_COUNT];
};
