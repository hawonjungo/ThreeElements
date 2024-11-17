#pragma once
#ifndef MAIN_OBJECT_H_
#define MAIN_OBJECT_H_
#include "BaseObject.h"
#include "Skill.h"
#include "Keyboard.h"
#include "Skill.h"
using namespace std;

class MainPlayer : public BaseObject
{
public:



	MainPlayer();
	~MainPlayer();

	void initializeSpellMap();


	void SetPos(int x, int y) { rect_.x = x; rect_.y = y; }
	int GetQKey() { return m_QKeyNum; }
	int GetWKey() { return m_WKeyNum; }
	int GetEKey() { return m_EKeyNum; }
	bool GetRState() const { return m_KeyRactive; }
	int GetKeyPress() const { return m_KeyDown; }
	void ResetQR() { m_QKeyNum = 0;  m_KeyRactive = false; }
	void ResetWR() { m_WKeyNum = 0;  m_KeyRactive = false; }
	void handleKeyPress(SDL_Event key);

	string getElementCombination();

	void combine();

	void saveSpellToSlot();



	vector<Element> elements;
	std::map<std::string, Spell> spellMap;
	Spell activeSpell;
	Spell slotD;
	Spell slotF;
private:

	Skill skill_;

	int m_KeyDown;

	int m_QKeyNum;
	int m_WKeyNum;
	int m_EKeyNum;

	bool m_KeyRactive;




};


#endif // !MAIN_OBJECT_H_
