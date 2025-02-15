
#ifndef ENEMY_OBJECT_H_
#define ENEMY_OBJECT_H_

#include "BaseObject.h"
#include <iostream>
#include <vector>



class EnemyObject : public BaseObject 
{
public:

	EnemyObject();
	~EnemyObject();

	void keyHandle(SDL_Event event);
	bool LoadImg(std::string path, SDL_Renderer* screen,int frame_num);
	void set_clips();
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) {
		rect_.x = x; rect_.y = y;
		std::cout << "Enemy set to position: (" << x << ", " << y << ")" << std::endl;
	}
	void SetVal(int xv, int yv) { x_val_ = xv; y_val_ = yv; }
	void UpdatePos();
	std::string GetPath() const;
private:
	int currentFrame_;
	int frame_num_;
	std::vector<SDL_Rect> frame_clip_;
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
	int x_val_;
	int y_val_;
	std::string path_;
};


#endif // ENEMY_OBJECT_H_
