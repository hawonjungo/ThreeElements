#pragma once
#ifndef DRAW_H_
#define DRAW_H_

#include "Define.h"

// Small drawing primitives on top of SDL_Renderer (SDL2 has no circles), shared by the HUD, the feedback effects
// and the code-drawn skill effects. Every function uses the renderer's current draw colour and blend mode.
namespace draw
{
	void FillCircle(SDL_Renderer* renderer, int cx, int cy, int radius);
	void Ring(SDL_Renderer* renderer, int cx, int cy, int radius, int thickness);            // circle outline
	void Arc(SDL_Renderer* renderer, int cx, int cy, int radius, float fromRad, float toRad, int thickness);
	void Diamond(SDL_Renderer* renderer, int cx, int cy, int halfWidth, int halfHeight);     // filled, like a crystal

	// Deterministic "random" number in [0, 1) for particle i of effect `seed`: effects need no stored particles,
	// every frame recomputes each particle from its index and the effect's age.
	float Hash(int i, int seed);
}

#endif
