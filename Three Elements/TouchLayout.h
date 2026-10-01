#pragma once
#ifndef TOUCH_LAYOUT_H_
#define TOUCH_LAYOUT_H_

// Where the touch buttons sit (GAMEPLAY_SPEC.md §30, owner 2026-10-01): the player may move them, split the
// Q W E R / D F cluster into two groups (Q W E for one thumb, R / D F for the other) and choose their size.
// Plain geometry, no SDL: GameManager draws and drags, the tests check the rules (Tests/PracticeTests.cpp).
//
// Three blocks: MAIN (the whole cluster, or Q W E when split), CAST (R above D F, only when split) and ITEMS (the
// 2 x 3 item bar of PLAY). The buttons keep their places inside a block; only the blocks move.
namespace touchlayout
{
	const int GAP = 8;                        // between two buttons
	const int SIZE_COUNT = 3;
	const int SIZES[SIZE_COUNT] = { 72, 88, 104 };  // small, medium (the size the game always had), large
	const int DEFAULT_SIZE = 1;
	// Blocks stay inside this area: below the HUD rows, above the lane where the enemies walk.
	const int AREA_LEFT = 8;
	const int AREA_RIGHT = 920;
	const int AREA_TOP = 124;
	const int AREA_BOTTOM = 404;
	const int ITEM_BLOCK_W = 168;             // 3 slots of 52 px with 6 px gaps
	const int ITEM_BLOCK_H = 110;             // 2 rows
	// The orb row in the middle of the screen; it moves sideways to stay clear of the blocks. (Until 1.7.1 the box
	// also held the D / F slots under the orbs; on a touch screen the D / F buttons are the slots now, spec §30 L-8.)
	const int HUD_X = 376;
	const int HUD_Y = 146;
	const int HUD_W = 176;
	const int HUD_H = 48;
	const int HUD_SHIFT = 160;

	enum Block { BLOCK_MAIN, BLOCK_CAST, BLOCK_ITEMS, BLOCK_COUNT };
	const int BUTTON_COUNT = 6;               // Q W E R D F, in this order

	struct Box { int x, y, w, h; };
	struct Layout
	{
		bool split;                // false: one cluster (Q W E R over D F); true: Q W E and R / D F apart
		int size;                  // index into SIZES
		int x[BLOCK_COUNT];        // top-left corner of each block (CAST is unused while not split)
		int y[BLOCK_COUNT];
	};

	// The layout a mode starts from. Default(false, DEFAULT_SIZE) is exactly where the buttons were before 1.7.1.
	Layout Default(bool split, int size);
	Box BlockBox(const Layout& layout, int block);    // w = h = 0 for CAST while not split
	Box ButtonBox(const Layout& layout, int button);  // 0..5 = Q W E R D F
	void Clamp(Layout& layout, int block);            // back inside the area
	bool Valid(const Layout& layout);                 // a known size, every block inside the area, none overlapping
	void Sanitize(Layout& layout);                    // anything not Valid becomes the default of its mode
	int HudShift(const Layout& layout);               // 0, +HUD_SHIFT or -HUD_SHIFT: the first that is clear of the blocks
}

#endif // TOUCH_LAYOUT_H_
