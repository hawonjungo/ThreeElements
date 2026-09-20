#pragma once
#ifndef SKILL_OBJECT_H_
#define SKILL_OBJECT_H_
#include "BaseObject.h"

using namespace std;
#define FRAME_NUM 1

// Icon sprite of one skill. The skill data itself (recipe, name, icon path) lives in Core/Invoker.h.
class Skill : public BaseObject
{
public:

	// MON 10/14/2024 First Setup
	Skill();
	~Skill();


	void SetActive(bool at) { m_active = at; }
	bool GetActive() const { return m_active; }
private:
	bool m_active;
	int currentFrame_;

};


#endif // SKILL_OBJECT_H_
