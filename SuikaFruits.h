// SuikaFruits.h -- the eleven fruits, drawn small.
//
// Everything is a filled circle plus a little decoration, because at these
// sizes that is all that survives: the cherry is 10 px across. The decoration
// is not garnish -- it is what separates the two pairs that size cannot. The
// cherry and strawberry differ by 2 px and are both red, and the dekopon and
// persimmon differ by 3 px and are both orange, so those four get seeds, a
// leaf, a navel bump and a calyx respectively.
//
// Faces start at the dekopon (SUIKA_FACE_FROM). Below that the eyes land on the
// same pixel and read as a smudge. A face is two eyes, a shallow smile and a
// pair of cheeks -- and for a moment after a merge, a shut-eyed grin instead,
// which is the one place the drawing needs to know something about the game.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

// Draw tier `t` centred on (x, y), rolled by `ang` radians. `glee` is the merge
// face: eyes shut, wider smile. Tier 7 is the character standing in for the
// peach: it forwards to drawCharacterBall() and stays the right way up, since a
// thing with feet should not tumble.
void drawSuikaFruit(M5Canvas& cv, int t, float x, float y, float ang, bool glee = false);

// Body only -- no face, no roll. For the "next" chip in the HUD, where the
// fruit is drawn smaller than it will be in play.
void drawSuikaChip(M5Canvas& cv, int t, float x, float y, float r);
