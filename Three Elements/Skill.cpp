#include "Skill.h"
#include <algorithm>

// MON 10/14/2024 First Setup
Skill::Skill() {

	

	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;

	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	currentFrame_ = -1;
	SetFrameNum(1);
	for (int i = 0; i < GetFrameNum(); ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}
	m_type = 0;
	m_active = false;
	initializeSpellMap();
}

Skill::~Skill()
{

}
void Skill::initializeSpellMap() {
	spellMap["QQQ"] = COLD_SNAP;
	spellMap["QQW"] = GHOST_WALK;
	spellMap["EQQ"] = ICE_WALL;
	spellMap["WWW"] = EMP;
	spellMap["QWW"] = TORNADO;
	spellMap["EWW"] = ALACRITY;
	spellMap["EEE"] = SUN_STRIKE;
	spellMap["EEQ"] = FORGE_SPIRIT;
	spellMap["EEW"] = CHAOS_METEOR;
	spellMap["EQW"] = DEAFENING_BLAST;
}