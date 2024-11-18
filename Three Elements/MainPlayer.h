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

	


	int GetQKey() { return m_QKeyNum; }
	int GetWKey() { return m_WKeyNum; }
	int GetEKey() { return m_EKeyNum; }
	bool GetRState() const { return m_KeyRactive; }
	int GetKeyPress() const { return m_KeyDown; }
	void ResetQR() { m_QKeyNum = 0;  m_KeyRactive = false; }
	void ResetWR() { m_WKeyNum = 0;  m_KeyRactive = false; }
	void handleKeyPress(SDL_Event key);

	string getElementComb();
	string getCombineComb();
	void saveSpellToSlot();



	
private:

	Skill skill_;

	int m_KeyDown;

	int m_QKeyNum;
	int m_WKeyNum;
	int m_EKeyNum;

	bool m_KeyRactive;
	vector<Element> elements;
	Spell activeSpell;
	Spell slotD;
	Spell slotF;
	Skill skill;


};


#endif // !MAIN_OBJECT_H_
