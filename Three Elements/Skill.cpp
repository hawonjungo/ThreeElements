#include "Skill.h"

// MON 10/14/2024 First Setup
Skill::Skill() {



	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.y = 0;

	width_frame_ = 0;
	height_frame_ = 0;
	SetFrameNum(1);
	for (int i = 0; i < GetFrameNum(); ++i)
	{
		frame_clip_[i].x = 0;
		frame_clip_[i].y = 0;
		frame_clip_[0].w = 0;
		frame_clip_[0].h = 0;
	}
}

Skill::~Skill()
{

}

bool Skill::LoadIcon(const std::string& path, SDL_Renderer* screen)
{
	free();
	SDL_Surface* surface = IMG_Load(path.c_str());
	if (surface == NULL)
	{
		printf("Unable to load skill icon %s: %s\n", path.c_str(), IMG_GetError());
		return false;
	}
	p_object_ = SDL_CreateTextureFromSurface(screen, surface);
	rect_.w = width_frame_ = surface->w;
	rect_.h = height_frame_ = surface->h;
	SDL_FreeSurface(surface);
	if (p_object_ == NULL)
		return false;
	SDL_SetTextureBlendMode(p_object_, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(p_object_, SDL_ScaleModeLinear);
	return true;
}

void Skill::RenderAt(SDL_Renderer* screen, int x, int y, int size)
{
	if (p_object_ == NULL)
		return;
	SDL_Rect dst = { x, y, size, size };
	SDL_RenderCopy(screen, p_object_, NULL, &dst);
}
