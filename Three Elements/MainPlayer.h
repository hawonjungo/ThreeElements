#pragma once
#ifndef MAIN_OBJECT_H_
#define MAIN_OBJECT_H_
#include "BaseObject.h"
#include "Keyboard.h"
#include "Core/Invoker.h"
using namespace std;

// Player sprite + the SDL keyboard -> logical Invoker action mapping.
// The Invoker rules live in Core/Invoker.h, the practice rules in Practice/Practice.h.
class MainPlayer : public BaseObject
{
public:



	MainPlayer();
	~MainPlayer();




	int GetQKey() { return m_QKeyNum; }
	int GetWKey() { return m_WKeyNum; }
	int GetEKey() { return m_EKeyNum; }
	int GetKeyPress() const { return m_KeyDown; }
	// SDL keyboard event -> logical Invoker action. Returns false for keys other than Q/W/E/R/D/F, for key
	// releases and for auto-repeat KEYDOWN events (holding a key must not flood the orbs).
	static bool TranslateKey(const SDL_Event& e, invoker::InputAction& action);




	int m_KeyDown;

	int m_QKeyNum;
	int m_WKeyNum;
	int m_EKeyNum;


};


#endif // !MAIN_OBJECT_H_
