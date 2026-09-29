
#include "Enemy.h"


EnemyObject::EnemyObject() : animTime_(0.0f) {}

EnemyObject::~EnemyObject()
{

}

// The frame count is set first, so BaseObject::LoadImg computes the frame width from it; BaseObject::set_clips()
// then cuts the sheet into those frames.
bool EnemyObject::LoadImg(std::string path, SDL_Renderer* screen, int frame_num)
{
	SetFrameNum(frame_num > 0 ? frame_num : 1);
	currentFrame_ = 0;
	return BaseObject::LoadImg(path, screen);
}

void EnemyObject::Update(float dt)
{
	if (p_object_ == NULL)
		return;
	const float frameTime = 1.0f / ENEMY_ANIM_FPS;
	animTime_ += dt;
	while (animTime_ >= frameTime)
	{
		animTime_ -= frameTime;
		currentFrame_ = (currentFrame_ + 1) % totalFrame_;
	}
}

void EnemyObject::Render(SDL_Renderer* screen)
{
	if (p_object_ == NULL)
		return;

	SDL_Rect* current_clip = &frame_clip_[currentFrame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };
	SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_HORIZONTAL);
}
