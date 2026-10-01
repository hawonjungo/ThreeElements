#include "TouchLayout.h"

namespace touchlayout
{
	namespace
	{
		bool Intersect(const Box& a, const Box& b)
		{
			return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
		}

		bool Used(const Layout& layout, int block) { return block != BLOCK_CAST || layout.split; }

		int SizeOf(const Layout& layout)
		{
			return SIZES[layout.size >= 0 && layout.size < SIZE_COUNT ? layout.size : DEFAULT_SIZE];
		}
	}

	Layout Default(bool split, int size)
	{
		Layout l;
		l.split = split;
		l.size = size >= 0 && size < SIZE_COUNT ? size : DEFAULT_SIZE;
		for (int b = 0; b < BLOCK_COUNT; ++b)
			l.x[b] = l.y[b] = 0;
		if (!split)
		{
			// the cluster at the left edge, the item bar at the right: where they always were
			l.x[BLOCK_MAIN] = 16;
			int h = BlockBox(l, BLOCK_MAIN).h;
			l.y[BLOCK_MAIN] = 184 + h <= AREA_BOTTOM ? 184 : AREA_BOTTOM - h;
			l.x[BLOCK_ITEMS] = AREA_RIGHT + 8 - 16 - ITEM_BLOCK_W;
			l.y[BLOCK_ITEMS] = 292;
			l.x[BLOCK_CAST] = l.x[BLOCK_MAIN];  // not used; kept somewhere sensible
			l.y[BLOCK_CAST] = l.y[BLOCK_MAIN];
		}
		else
		{
			// Q W E low on the left, R / D F low on the right, the items above Q W E
			l.x[BLOCK_MAIN] = 16;
			l.y[BLOCK_MAIN] = AREA_BOTTOM - BlockBox(l, BLOCK_MAIN).h;
			Box cast = BlockBox(l, BLOCK_CAST);
			l.x[BLOCK_CAST] = AREA_RIGHT + 8 - 16 - cast.w;
			l.y[BLOCK_CAST] = AREA_BOTTOM - cast.h;
			l.x[BLOCK_ITEMS] = 16;
			l.y[BLOCK_ITEMS] = l.y[BLOCK_MAIN] - 16 - ITEM_BLOCK_H;
		}
		return l;
	}

	Box BlockBox(const Layout& layout, int block)
	{
		int size = SizeOf(layout), step = size + GAP;
		Box b = { 0, 0, 0, 0 };
		if (block < 0 || block >= BLOCK_COUNT)
			return b;
		b.x = layout.x[block];
		b.y = layout.y[block];
		if (block == BLOCK_ITEMS)
		{
			b.w = ITEM_BLOCK_W;
			b.h = ITEM_BLOCK_H;
		}
		else if (block == BLOCK_MAIN)
		{
			b.w = layout.split ? 2 * step + size : 3 * step + step / 2 + size;  // F ends half a step right of R
			b.h = layout.split ? size : step + size;
		}
		else if (layout.split)
		{
			b.w = step + size;
			b.h = step + size;
		}
		return b;
	}

	Box ButtonBox(const Layout& layout, int button)
	{
		int size = SizeOf(layout), step = size + GAP, half = step / 2;
		// places inside the whole cluster, in half steps: D sits under E / R, F half a step past R (a keyboard's stagger)
		const int column[BUTTON_COUNT] = { 0, 2, 4, 6, 5, 7 };
		const int row[BUTTON_COUNT] = { 0, 0, 0, 0, 1, 1 };
		Box b = { 0, 0, size, size };
		if (button < 0 || button >= BUTTON_COUNT)
			return b;
		if (!layout.split || button < 3)
		{
			b.x = layout.x[BLOCK_MAIN] + column[button] * half;
			b.y = layout.y[BLOCK_MAIN] + row[button] * step;
		}
		else  // R, D, F keep the same places relative to each other: R above, half a step in
		{
			b.x = layout.x[BLOCK_CAST] + (column[button] - 5) * half;
			b.y = layout.y[BLOCK_CAST] + row[button] * step;
		}
		return b;
	}

	void Clamp(Layout& layout, int block)
	{
		if (block < 0 || block >= BLOCK_COUNT)
			return;
		Box b = BlockBox(layout, block);
		if (b.x > AREA_RIGHT - b.w) b.x = AREA_RIGHT - b.w;
		if (b.x < AREA_LEFT) b.x = AREA_LEFT;
		if (b.y > AREA_BOTTOM - b.h) b.y = AREA_BOTTOM - b.h;
		if (b.y < AREA_TOP) b.y = AREA_TOP;
		layout.x[block] = b.x;
		layout.y[block] = b.y;
	}

	bool Valid(const Layout& layout)
	{
		if (layout.size < 0 || layout.size >= SIZE_COUNT)
			return false;
		for (int a = 0; a < BLOCK_COUNT; ++a)
		{
			if (!Used(layout, a))
				continue;
			Box box = BlockBox(layout, a);
			if (box.x < AREA_LEFT || box.x + box.w > AREA_RIGHT || box.y < AREA_TOP || box.y + box.h > AREA_BOTTOM)
				return false;
			for (int b = a + 1; b < BLOCK_COUNT; ++b)
				if (Used(layout, b) && Intersect(box, BlockBox(layout, b)))
					return false;
		}
		return true;
	}

	void Sanitize(Layout& layout)
	{
		if (!Valid(layout))
			layout = Default(layout.split, layout.size);
	}

	int HudShift(const Layout& layout)
	{
		const int shifts[3] = { 0, HUD_SHIFT, -HUD_SHIFT };
		for (int shift : shifts)
		{
			Box hud = { HUD_X + shift, HUD_Y, HUD_W, HUD_H };
			bool clear = true;
			for (int b = 0; b < BLOCK_COUNT; ++b)
				clear = clear && !(Used(layout, b) && Intersect(hud, BlockBox(layout, b)));
			if (clear)
				return shift;
		}
		return 0;
	}
}
