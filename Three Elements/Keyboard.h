#ifndef KEYBOARD_OBJECT_H_
#define KEYBOARD_OBJECT_H_

#include "BaseObject.h"


class Keyboard : public BaseObject
{
public:
	enum KeyType
	{
		KEY_NONE = -1,
		KEY_Q = 0,
		KEY_W = 1,
		KEY_E = 2,
		KEY_R = 3,
		KEY_D = 4,
	    KEY_F = 5,
	};
	Keyboard();
	~Keyboard();

private:

};


#endif // KEYBOARD_OBJECT_H_
