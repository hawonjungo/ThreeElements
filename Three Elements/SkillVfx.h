#pragma once
#ifndef SKILL_VFX_H_
#define SKILL_VFX_H_

#include "Define.h"
#include "Core/Invoker.h"

// Code-drawn effects for the 8 skills that have no sprite effect of their own (Tornado and Ghost Walk keep their
// sprite sheets). Presentation only: Practice judges every cast exactly as before, these only show it.
// Each effect is recomputed every frame from its age and a seed (see draw::Hash), so nothing is stored per particle.
// Shapes and colours follow Dota 2's Invoker (owner 2026-09-29: option (b), replaces the TEST placeholder sheets).
namespace skillvfx
{
	struct Effect
	{
		float left;   // seconds left; <= 0 = not showing
		int x, y;     // origin: the enemy (placed effects) or the player (self / projectile effects)
		float dirX;   // unit vector toward the enemy, fixed at cast time (projectiles and the blast wave)
		float dirY;
		int seed;     // varies the particles from one cast to the next
	};

	bool HasEffect(invoker::SkillId skill);      // the 8 skills drawn here
	float Duration(invoker::SkillId skill);      // seconds
	bool StartsAtPlayer(invoker::SkillId skill); // false = placed on the enemy
	bool NeedsEnemy(invoker::SkillId skill);     // placed on it or aimed at it: nothing is shown without one

	void Render(SDL_Renderer* renderer, invoker::SkillId skill, const Effect& effect);
}

#endif
