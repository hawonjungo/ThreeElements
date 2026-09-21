
#ifndef ENEMY_OBJECT_H_
#define ENEMY_OBJECT_H_

#include "BaseObject.h"
#include <vector>



class EnemyObject : public BaseObject 
{
public:

	EnemyObject();
	~EnemyObject();

	bool LoadImg(std::string path, SDL_Renderer* screen,int frame_num);
	void set_clips();
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) {
		rect_.x = x; rect_.y = y;
	}
private:
	int currentFrame_;
	int frame_num_;
	std::vector<SDL_Rect> frame_clip_;
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
};


#endif // ENEMY_OBJECT_H_
