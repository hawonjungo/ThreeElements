
#ifndef BASE_OBJECT_H_
#define BASE_OBJECT_H_

#include"Define.h"
#include <vector>

class BaseObject
{
public:
	BaseObject();
	~BaseObject();
	virtual bool LoadImg(std::string path, SDL_Renderer* screen);
	void Render(SDL_Renderer* screen);
	void SetPos(int x, int y) { rect_.x = x; rect_.y = y; }
	void SetFrameNum(int frameNum); 
	int GetFrameNum() const;
	void set_clips();
	void free();

public:
	SDL_Texture* p_object_;
	SDL_Rect rect_;
	std::vector<SDL_Rect> frame_clip_;
	int width_frame_;  // for 1 frame
	int height_frame_; // for 1 frame
	int totalFrame_;
	int currentFrame_;
	std::vector<int> iDelay_; 
	Uint32 passed_time_; // work better with SDL , 0 - 4m

};

#endif