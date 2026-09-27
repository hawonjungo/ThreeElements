#pragma once
#ifndef MAIN_OBJECT_H_
#define MAIN_OBJECT_H_
#include "BaseObject.h"
#include "Core/Invoker.h"

// Player sprite + the SDL keyboard -> logical Invoker action mapping.
// The Invoker rules live in Core/Invoker.h, the practice rules in Practice/Practice.h.
class MainPlayer : public BaseObject
{
public:



	MainPlayer();
	~MainPlayer();




	// SDL keyboard event -> logical Invoker action. Returns false for keys other than Q/W/E/R/D/F, for key
	// releases and for auto-repeat KEYDOWN events (holding a key must not flood the orbs).
	static bool TranslateKey(const SDL_Event& e, invoker::InputAction& action);






};


#endif // !MAIN_OBJECT_H_
