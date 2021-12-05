
#ifndef SKILL_OBJECT_H_
#define SKILL_OBJECT_H_

#include "BaseObject.h"


#define FRAME_NUM 1

class Skill : public BaseObject
{
public:

	Skill();
	~Skill();

	void keyHandle(SDL_Event event);
	bool LoadImg(std::string path, SDL_Renderer* screen);
	void set_clips();
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) { rect_.x = x; rect_.y = y; }
private:
	int frame_;
	SDL_Rect frame_clip_[FRAME_NUM];
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
	int x_val_;
	int y_val_;
};


#endif // SKILL_OBJECT_H_
