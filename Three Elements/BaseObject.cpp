
#include "BaseObject.h"



BaseObject::BaseObject() 
{	
	p_object_ = NULL;
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.h = 0;
	width_frame_ = 0;
	height_frame_ = 0;
	totalFrame_ = 1;
	currentFrame_ = 0;
	x_val_ = 0;
	y_val_ = 0;
	passed_time_ = 0;
	iDelay_.resize(totalFrame_, 100);
	frame_clip_.resize(totalFrame_);
}

BaseObject::~BaseObject() 
{
	free();
}


bool BaseObject::LoadImg(std::string path,SDL_Renderer* screen)
{
	free();
	SDL_Texture* new_Texture = NULL;
	SDL_Surface* load_surface = IMG_Load(path.c_str());
	if (load_surface != NULL) 
	{
		//Color key image
		SDL_SetColorKey(load_surface, SDL_TRUE, SDL_MapRGB(load_surface->format, 175,175, 175));
		//Create texture from surface pixels
		new_Texture = SDL_CreateTextureFromSurface(screen, load_surface);
		if (new_Texture == NULL)
		{
			printf("Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
		}
		else
		{
			//Get image dimensions
			rect_.w = load_surface->w;
			rect_.h = load_surface->h;
			width_frame_ = rect_.w / totalFrame_;
			height_frame_ = rect_.h;
		}
		//Get rid of old loaded surface
		SDL_FreeSurface(load_surface);
	}
	//Return success
	p_object_ = new_Texture;
	return p_object_ != NULL;
}
void BaseObject::set_clips()
{

	if (width_frame_ > 0 && height_frame_ > 0)
	{
		for (int i = 0; i < totalFrame_; ++i)
		{
			frame_clip_[i].x = i * width_frame_;
			frame_clip_[i].y = 0;
			frame_clip_[i].w = width_frame_;
			frame_clip_[i].h = height_frame_;
		}
	}
}

void  BaseObject::render(SDL_Renderer* render, const SDL_Rect* clip) 
{
	//Set rendering space and render to screen
	SDL_Rect renderQuad = { rect_.x, rect_.y, rect_.w,rect_.h };

	//Set clip rendering dimensions
	if (clip != NULL)
	{
		renderQuad.w = clip->w;
		renderQuad.h = clip->h;
	}
	//Render to screen
	SDL_RenderCopy(render, p_object_, clip, &renderQuad);
}

void BaseObject::Render(SDL_Renderer* screen)
{
	// ky thuat sieu hay nhe, giam do chuyen anh frame
	if (SDL_GetTicks() - iDelay_[currentFrame_] > passed_time_)
	{
		passed_time_ = SDL_GetTicks();
		++currentFrame_;
		if (currentFrame_ > totalFrame_ - 1)
		{
			currentFrame_ = 0;
		}
	}

	SDL_Rect* current_clip = &frame_clip_[currentFrame_];
	SDL_Rect renderQuad = { rect_.x, rect_.y, width_frame_, height_frame_ };

	//SDL_RenderCopyEx(screen, p_object_, current_clip, &renderQuad, 0, 0, SDL_FLIP_NONE);
	SDL_RenderCopy(screen, p_object_, current_clip, &renderQuad);

}
void BaseObject::SetFrameNum(int frameNum) { 
	totalFrame_ = frameNum; 
	frame_clip_.resize(totalFrame_);
}
int BaseObject::GetFrameNum() const {
	return totalFrame_;
}

void BaseObject::free() 
{
	if (p_object_ != NULL)
	{
		SDL_DestroyTexture(p_object_);
		p_object_ = NULL;
		rect_ = { 0, 0, 0, 0 };
	}
}
