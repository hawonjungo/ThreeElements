

#ifndef KEYBOARD_OBJECT_H_
#define KEYBOARD_OBJECT_H_

#include "BaseObject.h"


#define FRAME_NUM 2

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


	void set_clips();
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) { rect_.x = x; rect_.y = y; }
	void SetType(int type) { m_type = type; }
	int GetType() const { return m_type; }
private:
	SDL_Rect frame_clip_[FRAME_NUM];
	unsigned int iDelay[FRAME_NUM];
	unsigned long passed_time_;
	int m_type;



};


#endif // KEYBOARD_OBJECT_H_
