#pragma once
#ifndef SKILL_OBJECT_H_
#define SKILL_OBJECT_H_
#include "BaseObject.h"


// Icon sprite of one skill. The skill data itself (recipe, name, icon path) lives in Core/Invoker.h.
class Skill : public BaseObject
{
public:

	// MON 10/14/2024 First Setup
	Skill();
	~Skill();

	// Skill icons are RGBA art with real transparency (see art/make_skill_icons.py): loaded without the grey
	// colour key BaseObject::LoadImg applies, and smoothed when scaled (they are painted art, not 1:1 pixels).
	bool LoadIcon(const std::string& path, SDL_Renderer* screen);
	void RenderAt(SDL_Renderer* screen, int x, int y, int size);  // the whole icon into a size x size square


};


#endif // SKILL_OBJECT_H_
