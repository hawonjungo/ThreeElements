#include "Draw.h"
#include <cmath>

namespace draw
{
	void FillCircle(SDL_Renderer* renderer, int cx, int cy, int radius)
	{
		for (int dy = -radius; dy <= radius; ++dy)
		{
			int dx = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
			SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
		}
	}

	void Arc(SDL_Renderer* renderer, int cx, int cy, int radius, float fromRad, float toRad, int thickness)
	{
		const int MAX_SEGMENTS = 48;
		int segments = static_cast<int>(MAX_SEGMENTS * std::fabs(toRad - fromRad) / 6.2831853f) + 2;
		if (segments > MAX_SEGMENTS)
			segments = MAX_SEGMENTS;
		SDL_Point points[MAX_SEGMENTS + 1];
		for (int w = 0; w < thickness; ++w)
		{
			for (int i = 0; i <= segments; ++i)
			{
				float a = fromRad + (toRad - fromRad) * i / segments;
				points[i] = { cx + static_cast<int>((radius + w) * std::cos(a)), cy + static_cast<int>((radius + w) * std::sin(a)) };
			}
			SDL_RenderDrawLines(renderer, points, segments + 1);
		}
	}

	void Ring(SDL_Renderer* renderer, int cx, int cy, int radius, int thickness)
	{
		Arc(renderer, cx, cy, radius, 0.0f, 6.2831853f, thickness);
	}

	void Diamond(SDL_Renderer* renderer, int cx, int cy, int halfWidth, int halfHeight)
	{
		for (int dy = -halfHeight; dy <= halfHeight; ++dy)
		{
			int dx = halfWidth - halfWidth * (dy < 0 ? -dy : dy) / (halfHeight > 0 ? halfHeight : 1);
			SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
		}
	}

	float Hash(int i, int seed)
	{
		unsigned h = static_cast<unsigned>(i) * 374761393u + static_cast<unsigned>(seed) * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= h >> 16;
		return (h & 0xFFFFFF) / 16777216.0f;
	}
}
