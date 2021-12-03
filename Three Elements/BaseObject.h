#pragma once

#include "GameObject.h"

#ifndef BASE_OBJECT_H_
#define BASE_OBHECT_H_

class BaseObject {

public:
	BaseObject();
	~BaseObject();

	

	void setRect(const int& x, const int& y) { rect_.x = x, rect_.y = y; }
	SDL_Rect getRect()const { 
		return rect_; 
	}
	SDL_Texture* getObject() {
		return p_object_;
	}

	void  render(SDL_Renderer* des, const SDL_Rect* clip);
	bool loadImg(std::string path, SDL_Renderer* screen);
	void free();


protected:
	SDL_Rect rect_;
	SDL_Texture* p_object_;
};



#endif