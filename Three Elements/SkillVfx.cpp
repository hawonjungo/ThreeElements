#include "SkillVfx.h"
#include "Draw.h"
#include "Practice/Practice.h"
#include <cmath>

using invoker::SkillId;

namespace skillvfx
{
	float Duration(SkillId skill);

	namespace
	{
		const float PI = 3.14159265f;
		const float PROJECTILE_SPEED = 700.0f;  // px/s, same as Tornado (Practice.h TORNADO_SPEED)

		struct Spec
		{
			SkillId skill;
			float duration;
			bool atPlayer;
		};
		const Spec kSpecs[] =
		{
			{ SkillId::ColdSnap,       0.8f, false },
			{ SkillId::IceWall,        2.5f, true  },
			{ SkillId::EMP,            1.0f, false },
			{ SkillId::Alacrity,       2.0f, true  },
			{ SkillId::SunStrike,      0.9f, false },
			{ SkillId::ForgeSpirit,    1.4f, true  },
			{ SkillId::ChaosMeteor,    1.2f, false },
			{ SkillId::DeafeningBlast, 1.2f, true  },
		};

		const Spec* Find(SkillId skill)
		{
			for (const Spec& s : kSpecs)
				if (s.skill == skill)
					return &s;
			return NULL;
		}

		Uint8 Alpha(float a)  // 0..1 -> 0..255, clamped
		{
			return static_cast<Uint8>(a <= 0.0f ? 0 : a >= 1.0f ? 255 : a * 255.0f);
		}

		void Color(SDL_Renderer* r, int red, int green, int blue, float alpha)
		{
			SDL_SetRenderDrawColor(r, static_cast<Uint8>(red), static_cast<Uint8>(green), static_cast<Uint8>(blue), Alpha(alpha));
		}

		// ---------------------------------------------------------------- the eight effects
		// `t` = age in seconds, `p` = t / duration (0 -> 1).

		// Cold Snap: the enemy is frozen for a moment: ice shards burst out, crystals hang around it, a frost ring.
		void ColdSnap(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
			Color(r, 150, 220, 255, 0.35f * (1.0f - p));
			draw::FillCircle(r, e.x, e.y, 34);
			Color(r, 200, 240, 255, 1.0f - p);
			draw::Ring(r, e.x, e.y, 20 + static_cast<int>(40 * p), 3);
			for (int i = 0; i < 10; ++i)
			{
				float a = 2.0f * PI * (i + draw::Hash(i, e.seed) * 0.5f) / 10.0f;
				float len = 18.0f + 34.0f * (t < 0.2f ? t / 0.2f : 1.0f) * (0.6f + 0.4f * draw::Hash(i + 20, e.seed));
				Color(r, 220, 245, 255, 1.0f - p);
				for (int w = -1; w <= 1; ++w)
					SDL_RenderDrawLine(r, e.x + w, e.y, e.x + w + static_cast<int>(len * std::cos(a)), e.y + static_cast<int>(len * std::sin(a)));
			}
			for (int i = 0; i < 6; ++i)
			{
				float a = 2.0f * PI * draw::Hash(i + 40, e.seed);
				float d = 24.0f + 20.0f * draw::Hash(i + 50, e.seed);
				Color(r, 170, 225, 255, 0.9f * (1.0f - p));
				draw::Diamond(r, e.x + static_cast<int>(d * std::cos(a)), e.y + static_cast<int>(d * std::sin(a)), 4, 8);
			}
		}

		// Ice Wall: a row of ice pillars rises from the ground in front of the player, stands, then melts away.
		void IceWall(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			const int ground = static_cast<int>(practice::GROUND_LINE_Y);
			float fade = p > 0.8f ? (1.0f - p) / 0.2f : 1.0f;
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
			for (int i = 0; i < 7; ++i)
			{
				float rise = (t - i * 0.05f) / 0.25f;
				if (rise <= 0.0f)
					continue;
				if (rise > 1.0f)
					rise = 1.0f;
				int h = static_cast<int>((50 + 40 * draw::Hash(i, e.seed)) * rise);
				int x = e.x + 70 + i * 42;
				SDL_Rect body = { x - 14, ground - h, 28, h };
				Color(r, 120, 200, 245, 0.75f * fade);
				SDL_RenderFillRect(r, &body);
				Color(r, 225, 248, 255, 0.9f * fade);   // light face and pointed top
				SDL_Rect face = { x - 12, ground - h + 4, 6, h > 8 ? h - 8 : 0 };
				SDL_RenderFillRect(r, &face);
				draw::Diamond(r, x, ground - h, 14, 8);
				Color(r, 40, 90, 140, 0.8f * fade);     // outline
				SDL_RenderDrawRect(r, &body);
			}
			Color(r, 200, 240, 255, 0.25f * fade);      // frost on the ground
			SDL_Rect frost = { e.x + 50, ground - 3, 7 * 42 + 20, 6 };
			SDL_RenderFillRect(r, &frost);
		}

		// EMP: a violet electric charge gathers on the enemy (with crackling arcs), then bursts outward.
		void Emp(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			const float charge = 0.25f;
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			int frame = static_cast<int>(t * 30.0f);  // the arcs jump 30 times per second
			if (t < charge)
			{
				float c = t / charge;
				Color(r, 150, 80, 255, 0.5f);
				draw::FillCircle(r, e.x, e.y, 10 + static_cast<int>(22 * c));
				Color(r, 230, 200, 255, 0.9f);
				draw::FillCircle(r, e.x, e.y, 5 + static_cast<int>(8 * c));
				for (int i = 0; i < 5; ++i)  // jagged lightning arcs around the charge
				{
					float a = 2.0f * PI * draw::Hash(i, e.seed + frame);
					int px = e.x, py = e.y;
					Color(r, 210, 170, 255, 0.9f);
					for (int s = 1; s <= 4; ++s)
					{
						float d = s * 12.0f * (0.6f + c);
						float j = (draw::Hash(i * 7 + s, e.seed + frame) - 0.5f) * 0.9f;
						int nx = e.x + static_cast<int>(d * std::cos(a + j)), ny = e.y + static_cast<int>(d * std::sin(a + j));
						SDL_RenderDrawLine(r, px, py, nx, ny);
						px = nx; py = ny;
					}
				}
			}
			else
			{
				float b = (t - charge) / (Duration(SkillId::EMP) - charge);  // burst progress 0 -> 1
				int radius = 20 + static_cast<int>(90 * b);
				Color(r, 120, 60, 220, 0.35f * (1.0f - b));
				draw::FillCircle(r, e.x, e.y, radius);
				Color(r, 220, 180, 255, 1.0f - b);
				draw::Ring(r, e.x, e.y, radius, 4);
			}
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}

		// Alacrity: an orange and violet aura swirls up around the player.
		void Alacrity(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			float fade = p < 0.1f ? p / 0.1f : (p > 0.8f ? (1.0f - p) / 0.2f : 1.0f);
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			Color(r, 255, 140, 40, 0.18f * fade);
			draw::FillCircle(r, e.x, e.y, 46);
			for (int i = 0; i < 18; ++i)
			{
				float cycle = std::fmod(t * 0.9f + draw::Hash(i, e.seed), 1.0f);  // each spark rises, then starts again
				float a = t * 5.0f + 2.0f * PI * i / 18.0f;
				int x = e.x + static_cast<int>(34.0f * std::cos(a));
				int y = e.y + 40 - static_cast<int>(90.0f * cycle);
				bool orange = i % 2 == 0;
				Color(r, orange ? 255 : 190, orange ? 150 : 110, orange ? 40 : 255, fade * (1.0f - cycle));
				draw::FillCircle(r, x, y, 3);
			}
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}

		// Sun Strike: a thin targeting line, then a column of golden light slams down on the enemy.
		void SunStrike(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			const int ground = static_cast<int>(practice::GROUND_LINE_Y);
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			if (t < 0.1f)
			{
				Color(r, 255, 220, 120, 0.6f);
				SDL_Rect line = { e.x - 1, 0, 3, ground };
				SDL_RenderFillRect(r, &line);
			}
			else
			{
				float b = (t - 0.1f) / (Duration(SkillId::SunStrike) - 0.1f);  // beam progress 0 -> 1
				int width = static_cast<int>(56 * (1.0f - 0.6f * b));
				Color(r, 255, 170, 40, 0.55f * (1.0f - b));
				SDL_Rect outer = { e.x - width / 2, 0, width, ground };
				SDL_RenderFillRect(r, &outer);
				Color(r, 255, 250, 210, 0.9f * (1.0f - b));
				SDL_Rect core = { e.x - width / 6, 0, width / 3 + 1, ground };
				SDL_RenderFillRect(r, &core);
				Color(r, 255, 200, 80, 0.7f * (1.0f - b));  // flash where it lands
				for (int dy = -8; dy <= 8; ++dy)
				{
					int dx = static_cast<int>((50 + 30 * b) * std::sqrt(1.0f - dy * dy / 64.0f));
					SDL_RenderDrawLine(r, e.x - dx, ground + dy, e.x + dx, ground + dy);
				}
			}
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}

		// Forge Spirit: a fireball flies from the player toward the enemy, trailing embers.
		void ForgeSpirit(SDL_Renderer* r, const Effect& e, float t, float)
		{
			float d = PROJECTILE_SPEED * t;
			int x = e.x + static_cast<int>(e.dirX * d), y = e.y + static_cast<int>(e.dirY * d);
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			for (int k = 8; k >= 1; --k)  // trail, dimmer and smaller further back
			{
				float back = k * 11.0f;
				float wobble = (draw::Hash(k, e.seed + static_cast<int>(t * 20)) - 0.5f) * 8.0f;
				Color(r, 255, 90 + 10 * (8 - k), 20, 0.6f * (1.0f - k / 9.0f));
				draw::FillCircle(r, x - static_cast<int>(e.dirX * back), y - static_cast<int>(e.dirY * back + wobble), 12 - k);
			}
			Color(r, 255, 120, 30, 0.7f);
			draw::FillCircle(r, x, y, 16);
			Color(r, 255, 230, 120, 1.0f);
			draw::FillCircle(r, x, y, 8);
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}

		// Chaos Meteor: a burning rock falls from the upper left onto the enemy, then bursts into flame.
		void ChaosMeteor(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			const float fall = 0.25f;
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			if (t < fall)
			{
				float f = t / fall;
				int sx = e.x - 260, sy = -40;
				int x = sx + static_cast<int>((e.x - sx) * f), y = sy + static_cast<int>((e.y - sy) * f);
				for (int k = 1; k <= 7; ++k)  // flame trail up and to the left
				{
					Color(r, 255, 110, 20, 0.5f * (1.0f - k / 8.0f));
					draw::FillCircle(r, x - k * 14, y - k * 12, 18 - k * 2);
				}
				Color(r, 255, 140, 40, 0.8f);
				draw::FillCircle(r, x, y, 22);
				SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
				Color(r, 90, 30, 20, 1.0f);  // the rock itself
				draw::FillCircle(r, x, y, 13);
				Color(r, 160, 60, 30, 1.0f);
				draw::FillCircle(r, x - 4, y - 4, 5);
			}
			else
			{
				float b = (t - fall) / (Duration(SkillId::ChaosMeteor) - fall);  // burst progress 0 -> 1
				Color(r, 255, 90, 20, 0.5f * (1.0f - b));
				draw::FillCircle(r, e.x, e.y, 20 + static_cast<int>(50 * b));
				Color(r, 255, 200, 80, 1.0f - b);
				draw::Ring(r, e.x, e.y, 24 + static_cast<int>(60 * b), 3);
				for (int i = 0; i < 12; ++i)  // flames thrown up and out
				{
					float a = -PI * draw::Hash(i, e.seed);
					float d = 20.0f + 70.0f * b * (0.5f + draw::Hash(i + 30, e.seed));
					Color(r, 255, 120 + static_cast<int>(100 * draw::Hash(i + 60, e.seed)), 30, 1.0f - b);
					draw::FillCircle(r, e.x + static_cast<int>(d * std::cos(a)), e.y + static_cast<int>(d * std::sin(a)), 5);
				}
			}
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}

		// Deafening Blast: a widening wave of white, blue and pink arcs rolls from the player toward the enemy.
		void DeafeningBlast(SDL_Renderer* r, const Effect& e, float t, float p)
		{
			float d = PROJECTILE_SPEED * t;
			int x = e.x + static_cast<int>(e.dirX * d), y = e.y + static_cast<int>(e.dirY * d);
			float facing = std::atan2(e.dirY, e.dirX);
			const int colours[3][3] = { { 255, 255, 255 }, { 120, 200, 255 }, { 255, 140, 220 } };
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
			for (int k = 0; k < 3; ++k)
			{
				int radius = 40 + static_cast<int>(60 * p) - k * 14;
				Color(r, colours[k][0], colours[k][1], colours[k][2], 0.9f * (1.0f - p));
				draw::Arc(r, x - static_cast<int>(e.dirX * k * 14), y - static_cast<int>(e.dirY * k * 14), radius,
					facing - 1.1f, facing + 1.1f, 6);
			}
			SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
		}
	}

	bool HasEffect(SkillId skill) { return Find(skill) != NULL; }
	float Duration(SkillId skill) { const Spec* s = Find(skill); return s ? s->duration : 0.0f; }
	bool StartsAtPlayer(SkillId skill) { const Spec* s = Find(skill); return s != NULL && s->atPlayer; }
	bool NeedsEnemy(SkillId skill)
	{
		// placed on the enemy, or aimed at it (Forge Spirit, Deafening Blast); Ice Wall and Alacrity need none
		return HasEffect(skill) && skill != SkillId::IceWall && skill != SkillId::Alacrity;
	}

	void Render(SDL_Renderer* renderer, SkillId skill, const Effect& e)
	{
		float duration = Duration(skill);
		if (e.left <= 0.0f || duration <= 0.0f)
			return;
		float t = duration - e.left;
		float p = t / duration;
		switch (skill)
		{
		case SkillId::ColdSnap:       ColdSnap(renderer, e, t, p); break;
		case SkillId::IceWall:        IceWall(renderer, e, t, p); break;
		case SkillId::EMP:            Emp(renderer, e, t, p); break;
		case SkillId::Alacrity:       Alacrity(renderer, e, t, p); break;
		case SkillId::SunStrike:      SunStrike(renderer, e, t, p); break;
		case SkillId::ForgeSpirit:    ForgeSpirit(renderer, e, t, p); break;
		case SkillId::ChaosMeteor:    ChaosMeteor(renderer, e, t, p); break;
		case SkillId::DeafeningBlast: DeafeningBlast(renderer, e, t, p); break;
		default: break;
		}
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	}
}
