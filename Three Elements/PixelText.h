#pragma once
#ifndef PIXEL_TEXT_H_
#define PIXEL_TEXT_H_

// Minimal HUD text: a built-in 5x7 pixel font drawn with SDL_RenderFillRect.
// Presentation only. It needs no font file and no SDL_ttf, so it works the same on every platform.
// Supported characters: A-Z (lower case is drawn as upper case), 0-9, space and : . % - / ?

#include "Define.h"

namespace pixeltext
{
	int Width(const char* text, int scale);   // pixel width of `text`
	int Height(int scale);                    // pixel height of one line

	// Draws `text` with its top-left corner at (x, y). Each font pixel becomes a scale x scale square.
	void Draw(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color);

	// Same, with a dark drop shadow so it stays readable on any background.
	void DrawShadowed(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color);

	// Centered horizontally in a field `fieldWidth` wide.
	void DrawCentered(SDL_Renderer* renderer, const char* text, int fieldWidth, int y, int scale, SDL_Color color);
}

#endif // PIXEL_TEXT_H_
