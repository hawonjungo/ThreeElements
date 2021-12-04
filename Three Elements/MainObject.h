#pragma once
#ifndef MAIN_OBJECT_H_
#define MAIN_OBJECT_H_

#include "GameObject.h"
#include "BaseObject.h"

#define WIDTH_MAIN_OBJECT 224
#define HEIGHT_MAIN_OBJECT 112

class MainObject : public BaseObject {
public:

	MainObject();
	~MainObject();

	void keyHandle(SDL_Event event);

private:
	int x_val_;
	int y_val_;
};


#endif // !MAIN_OBJECT_H_
