#include "Keyboard.h"


Keyboard::Keyboard() {
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;

	x_val_ = 0;
	y_val_ = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	frame_ = -1;

	m_type = KeyType::KEY_NONE;

	for (int i = 0; i < FRAME_NUM; ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}

	passed_time_ = 0;
	iDelay[0] = 100;
	iDelay[1] = 100;
}

Keyboard::~Keyboard()
{

}

void Keyboard::set_clips()
{
	if (width_frame_ > 0 && height_frame_ > 0)
	{
		for (int i = 0; i < FRAME_NUM; ++i)
		{
			frame_clip_[i].x = i * width_frame_;
			frame_clip_[i].y = 0;
			frame_clip_[i].w = width_frame_;
			frame_clip_[i].h = height_frame_;
		}
	}
}

void Keyboard::Render(SDL_Renderer* screen)
{
	// ky thuat sieu hay nhe, giam do chuyen anh frame
	if (SDL_GetTicks() - iDelay[frame_] > passed_time_)
	{
		passed_time_ = SDL_GetTicks();
		++frame_;
		if (frame_ > FRAME_NUM - 1)
		{
			frame_ = 0;
		}
	}

	SDL_Rect* current_clip = &frame_clip_[frame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };

	//SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_NONE);
	SDL_RenderCopy(screen, p_object_, current_clip, &renderQuad);

}
