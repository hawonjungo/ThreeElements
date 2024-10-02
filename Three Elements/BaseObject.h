
#ifndef BASE_OBJECT_H_
#define BASE_OBJECT_H_

#include"Define.h"

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
	void render(SDL_Renderer* des, const SDL_Rect* clip);
	void free();

protected:
	SDL_Texture* p_object_;
	SDL_Rect rect_;
};

#endif