// Character.h -- the pink round character, drawn two ways.
//
// drawCharacterProfile() is the three-quarter view the boot animation eats the
// apple in: it lives here rather than in BootAnimation.cpp so the game can use
// the same palette and proportions without a second copy.
//
// drawCharacterBall() is the head-on view the game bounces around the screen.
// Same body, both eyes level, and a signed squash so it can flatten against
// whatever it just hit.
#pragma once
#include <M5Unified.h>
#include <stdint.h>
#include "Config.h"

// Expressions shared by both views.
enum CharExpr : uint8_t { EXPR_NORMAL = 0, EXPR_EATING, EXPR_HAPPY };

// Three-quarter profile. face = -1 faces left, +1 faces right. All of the
// small features scale with `radius`, so this is the boot scene at the default
// and still legible much smaller.
void drawCharacterProfile(M5Canvas& cv, float x, float y, float squash,
                          float mouth, float puff, float walk, uint8_t expr,
                          int face, float radius = CHAR_RADIUS);

// Head-on. squash is signed: + = wide and short (hit something above or below),
// - = narrow and tall (hit something to the side). `lean` tips the eyes and
// feet in the direction of travel, in radians, so it reads as flying rather
// than hovering.
void drawCharacterBall(M5Canvas& cv, float x, float y, float radius,
                       float squash, float lean, uint8_t expr, float glow);
