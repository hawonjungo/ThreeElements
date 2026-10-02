#pragma once
#ifndef FIELD_H_
#define FIELD_H_

// What the Practice-layer sessions share (split out of Practice.h for update 1.6, so that PracticeSession can own a
// BossSession for the IMMORTAL fights of spec §28 without a circular include): the play field, the enemy table, a
// box type and the Tornado projectile. No SDL, no clock, no globals, like the rest of the layer.

#include "../Core/Invoker.h"

#include <vector>

namespace practice
{
	// ---- play field (logical pixels of the 928x544 window) ----
	// Enemy positions are the x of the enemy's visible body, left edge (see EnemyDefinition::bodyLeft).
	const float SPAWN_X = 990.0f;        // sprite starts fully off-screen on the right
	const float HIT_LINE_X = 140.0f;     // an enemy whose body reaches this x has reached the player
	const float MAX_FRAME_TIME = 0.1f;   // dt is clamped to this so a hitch never teleports an enemy
	const int   START_HP = 3;

	// The window the game is drawn in (GameManager.h asserts that it matches SCREEN_WIDTH / SCREEN_HEIGHT).
	constexpr float FIELD_WIDTH = 928.0f;
	constexpr float FIELD_HEIGHT = 544.0f;
	const float GROUND_LINE_Y = 500.0f;  // y of the ground the enemies run on: their feetRow is drawn here

	// Where spells leave the player: the front of the player's body, at the hand holding the staff. The Injoker
	// sprite (assets/player/injoker.png) is drawn 82 x 120 px with its left edge at x 44 and its feet on the ground
	// (GameManager.h, PLAYER_DRAW_*), so it spans x 44..126, y 378..498; the staff hand is near (120, 450).
	const float PLAYER_CAST_X = 128.0f;
	const float PLAYER_CAST_Y = 450.0f;

	// ---- enemies ----
	const int ENEMY_TYPE_COUNT = 10;     // one visual identity per skill

	struct EnemyDefinition
	{
		int id;                          // equals its index in the table
		const char* name;                // for logs / the debug overlay only
		const char* sprite;              // asset path (data only, no textures here)
		int frames;                      // frames in the horizontal sprite sheet
		int bodyLeft;                    // first visible column of a drawn frame (after the horizontal flip)
		int feetRow;                     // row of the sprite that is drawn on GROUND_LINE_Y
		int bodyWidth;                   // visible body: width, and first / last visible row of a frame
		int bodyTop;                     //   (union over all animation frames); together with bodyLeft this is
		int bodyBottom;                  //   the hit box that spells collide with
		invoker::SkillId targetSkill;    // the skill this enemy requires; data, not derived from the sprite
		float speedMultiplier;           // applied on top of the difficulty speed
		float size;                      // drawn size and hit box, a whole number so the pixels stay square (§32 M-8):
		                                 // the sheets come from different packs and the small ones are drawn 2x or 3x
	};

	const EnemyDefinition& GetEnemyDefinition(int index);  // 0 <= index < ENEMY_TYPE_COUNT

	// Axis-aligned box in field pixels.
	struct Bounds
	{
		float x;
		float y;
		float w;
		float h;
	};

	// ---- Tornado: the first spell with a real effect ----
	// Casting Tornado (from D or F) launches a projectile from the player toward the enemy. The direction is
	// fixed at launch (no homing). The cast is judged when the projectile hits the enemy, not when it is cast.
	// INITIAL MVP TUNING VALUES, like the difficulty ones.
	const float TORNADO_SPEED = 700.0f;         // px/s
	const float TORNADO_HIT_RADIUS = 20.0f;     // hit circle around the projectile centre (the sprite is drawn 64 px wide)
	const float TORNADO_MAX_DISTANCE = 1200.0f; // it is removed after flying this far ...
	const float TORNADO_FIELD_MARGIN = 64.0f;   // ... or when its centre is this far outside the field
	const float TORNADO_MAX_STEP = 10.0f;       // px: longest move tested for a hit at once, so nothing tunnels
	// No limit on how many projectiles are in flight: every valid cast makes one. Each lives at most
	// TORNADO_MAX_DISTANCE / TORNADO_SPEED (about 1.7 s) and a cast needs its own key press, so the number
	// alive is bounded by how fast keys can be pressed and no technical cap is needed.
	const int   TORNADO_FRAME_COUNT = 16;       // frames of the sprite sheet (4 x 4 grid)
	const float TORNADO_ANIM_FPS = 10.0f;       // animation speed, independent of the projectile speed

	struct Tornado
	{
		bool active;
		float x;          // centre, field pixels
		float y;
		float dirX;       // unit vector, set once at launch
		float dirY;
		float travelled;  // px flown so far
		float animTime;   // seconds alive, drives the animation
		int enemyId;      // the enemy it was launched at (PracticeSession::SpawnCount() at that moment)
	};

	// (dx, dy) is only a direction (it is normalised); a zero vector falls back to "straight right".
	Tornado MakeTornado(float originX, float originY, float dx, float dy, int enemyId);
	void AdvanceTornado(Tornado& tornado, float dt);           // moves it by dir * speed * dt; deactivates it when done
	bool TornadoHits(const Tornado& tornado, const Bounds& target);  // hit circle against box
	int TornadoFrame(float animTime);                          // 0 .. TORNADO_FRAME_COUNT-1, looping
}

#endif // FIELD_H_
