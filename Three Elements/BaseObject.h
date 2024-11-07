
#ifndef BASE_OBJECT_H_
#define BASE_OBJECT_H_

#include"Define.h"
#define FRAME_NUM 2

class BaseObject
{
public:
	BaseObject();
	~BaseObject();
	void setRect(const int& x, const int& y) { rect_.x = x, rect_.y = y; }
	SDL_Rect getRect()const { return rect_; }

	SDL_Texture* getObject() 
	{
		return p_object_;
	}

	virtual bool LoadImg(std::string path, SDL_Renderer* screen);
	void render(SDL_Renderer* render, const SDL_Rect* clip);
	void set_clips();
	void free();

public:
	SDL_Texture* p_object_;
	SDL_Rect rect_;
	SDL_Rect frame_clip_[FRAME_NUM];
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
	int frame_;
	int x_val_;
	int y_val_;
};

#endif