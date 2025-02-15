
#include "Enemy.h"


EnemyObject::EnemyObject() : currentFrame_(0), frame_num_(0), width_frame_(0), height_frame_(0), x_val_(0), y_val_(0) {}

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

std::string EnemyObject::GetPath() const
{
	return path_;
}



bool EnemyObject::LoadImg(std::string path, SDL_Renderer* screen, int frame_num)
{
	bool ret = BaseObject::LoadImg(path, screen);
	if (ret == true)
	{
		width_frame_ = rect_.w / frame_num;
		height_frame_ = rect_.h;

		frame_num_ = frame_num; // Store the frame number
		frame_clip_.resize(frame_num_);
		path_ = path;
	}
	
	return ret;
}

void EnemyObject::set_clips()
{
    if (width_frame_ > 0 && height_frame_ > 0)
    {
        for (int i = 0; i < frame_num_; ++i)
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
	if (currentFrame_ == frame_num_)
	{
		currentFrame_ = 0;
	}

	SDL_Rect* current_clip = &frame_clip_[currentFrame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };
	SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_HORIZONTAL);
	
}
