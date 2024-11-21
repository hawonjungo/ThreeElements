#pragma once
#ifndef SKILL_OBJECT_H_
#define SKILL_OBJECT_H_
#include <map>
#include "BaseObject.h"

using namespace std;
#define FRAME_NUM 1
enum Spell {
	NO_SPELL,
	COLD_SNAP,
	GHOST_WALK,
	ICE_WALL,
	EMP,
	TORNADO,
	ALACRITY,
	SUN_STRIKE,
	FORGE_SPIRIT,
	CHAOS_METEOR,
	DEAFENING_BLAST
};

enum Element {
	QUAS,
	WEX,
	EXORT
};
class Skill : public BaseObject
{
public:	

	// MON 10/14/2024 First Setup
	Skill();
	~Skill();

	void initializeSpellMap();


	void SetActive(bool at) { m_active = at; }
	bool GetActive() const { return m_active; }
	map<string, Spell> spellMap;
private:
	bool m_active;
	int currentFrame_;

};


#endif // SKILL_OBJECT_H_
