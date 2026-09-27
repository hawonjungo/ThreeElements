#include "PixelText.h"

namespace pixeltext
{
	namespace
	{
		const int GLYPH_W = 5;
		const int GLYPH_H = 7;
		const int ADVANCE = GLYPH_W + 1;  // one blank column between characters

		// 7 rows per glyph, 5 bits per row, the highest bit is the leftmost pixel.
		struct Glyph { char c; unsigned char rows[GLYPH_H]; };

		const Glyph kGlyphs[] =
		{
			{ ' ', { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 } },
			{ ':', { 0b00000, 0b00100, 0b00000, 0b00000, 0b00000, 0b00100, 0b00000 } },
			{ '.', { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100 } },
			{ '%', { 0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011 } },
			{ '-', { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 } },
			{ '/', { 0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000 } },
			{ '?', { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100 } },

			{ '0', { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 } },
			{ '1', { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 } },
			{ '2', { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 } },
			{ '3', { 0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110 } },
			{ '4', { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 } },
			{ '5', { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 } },
			{ '6', { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 } },
			{ '7', { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 } },
			{ '8', { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 } },
			{ '9', { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 } },

			{ 'A', { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 } },
			{ 'B', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 } },
			{ 'C', { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 } },
			{ 'D', { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 } },
			{ 'E', { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 } },
			{ 'F', { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 } },
			{ 'G', { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111 } },
			{ 'H', { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 } },
			{ 'I', { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 } },
			{ 'J', { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 } },
			{ 'K', { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 } },
			{ 'L', { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 } },
			{ 'M', { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 } },
			{ 'N', { 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001 } },
			{ 'O', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 } },
			{ 'P', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 } },
			{ 'Q', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 } },
			{ 'R', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 } },
			{ 'S', { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 } },
			{ 'T', { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 } },
			{ 'U', { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 } },
			{ 'V', { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 } },
			{ 'W', { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 } },
			{ 'X', { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 } },
			{ 'Y', { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 } },
			{ 'Z', { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 } }
		};

		const Glyph* FindGlyph(char c)
		{
			if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
			const int count = static_cast<int>(sizeof(kGlyphs) / sizeof(kGlyphs[0]));
			for (int i = 0; i < count; ++i)
			{
				if (kGlyphs[i].c == c)
					return &kGlyphs[i];
			}
			return FindGlyph('?');
		}
	}

	int Width(const char* text, int scale)
	{
		int n = 0;
		while (text[n] != '\0')
			++n;
		return n > 0 ? (n * ADVANCE - 1) * scale : 0;
	}

	int Height(int scale)
	{
		return GLYPH_H * scale;
	}

	void Draw(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
		for (int i = 0; text[i] != '\0'; ++i)
		{
			const Glyph* g = FindGlyph(text[i]);
			for (int row = 0; row < GLYPH_H; ++row)
			{
				for (int col = 0; col < GLYPH_W; ++col)
				{
					if (g->rows[row] & (1 << (GLYPH_W - 1 - col)))
					{
						SDL_Rect r = { x + (i * ADVANCE + col) * scale, y + row * scale, scale, scale };
						SDL_RenderFillRect(renderer, &r);
					}
				}
			}
		}
	}

	void DrawShadowed(SDL_Renderer* renderer, const char* text, int x, int y, int scale, SDL_Color color)
	{
		SDL_Color shadow = { 0, 0, 0, 255 };
		Draw(renderer, text, x + scale, y + scale, scale, shadow);
		Draw(renderer, text, x, y, scale, color);
	}

	void DrawCentered(SDL_Renderer* renderer, const char* text, int fieldWidth, int y, int scale, SDL_Color color)
	{
		DrawShadowed(renderer, text, (fieldWidth - Width(text, scale)) / 2, y, scale, color);
	}
}
