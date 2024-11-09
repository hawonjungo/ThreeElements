#pragma once
#ifndef SKILL_OBJECT_H_
#define SKILL_OBJECT_H_
#include <map>
#include "BaseObject.h"


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
	//enum SkillType
	//{
	//	SKILL_TONADO = 0,
	//	SKILL_GREEN_VOLT = 1,
	//	SKILL_FREEZING = 2,
	//};
	// MON 10/14/2024 First Setup	
	Spell activeSpell;
	Element elements[3];
	Spell slotD;
	Spell slotF;

	// MON 10/14/2024 First Setup
	Skill();
	~Skill();
	void combine();
	void handleKeyPress(SDL_Event key);
	

	void keyHandle(SDL_Event event);
	bool LoadImg(std::string path, SDL_Renderer* screen);
	void set_clips();
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) { rect_.x = x; rect_.y = y; }
	int GetType() const { return m_type; }
	void SetType(int type) { m_type = type; }
	void SetActive(bool at) { m_active = at; }
	bool GetActive() const { return m_active; }
private:
	bool m_active;
	int m_type;
	int currentFrame_;
	SDL_Rect frame_clip_[FRAME_NUM];
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
	int x_val_;
	int y_val_;
	// MON 10/14/2024 First Setup
	std::map<std::string, Spell> spellMap;
	std::string getElementCombination();
	void initializeSpellMap();
	void saveSpellToSlot();
	int nextElementIndex;
};


#endif // SKILL_OBJECT_H_
