
#include "Enemy.h"


EnemyObject::EnemyObject() {
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;
	
	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	currentFrame_ = -1;

	for (int i = 0; i < FRAME_NUM; ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}
}

EnemyObject::~EnemyObject()
{

}

void EnemyObject::keyHandle(SDL_Event event)
{
}

void EnemyObject::UpdatePos()
{
	rect_.x -= x_val_;
	if (rect_.x < 0)
	{
		// TODO:
		rect_.x = 800;
	}

}



bool EnemyObject::LoadImg(std::string path, SDL_Renderer* screen)
{
	bool ret = BaseObject::LoadImg(path, screen);
	if (ret == true)
	{
		width_frame_ = rect_.w / FRAME_NUM;
		height_frame_ = rect_.h;
	}

	return ret;
}

void EnemyObject::set_clips()
{
    if (width_frame_ > 0 && height_frame_ > 0)
    {
        for (int i = 0; i < FRAME_NUM; ++i)
        {
            frame_clip_[i].x = i*width_frame_;
            frame_clip_[i].y = 0;
            frame_clip_[i].w = width_frame_;
            frame_clip_[i].h = height_frame_;
        }
    }
}

void EnemyObject::Render(SDL_Renderer* screen)
{
	currentFrame_++;
	if (currentFrame_ == FRAME_NUM)
	{
		currentFrame_ = 0;
	}

	SDL_Rect* current_clip = &frame_clip_[currentFrame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };

	SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_HORIZONTAL);
	
}
