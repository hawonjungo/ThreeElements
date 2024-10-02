
#include "BaseObject.h"



BaseObject::BaseObject() 
{
	
	p_object_ = NULL;
	rect_.x = 0;
	rect_.y = 0;
	rect_.w = 0;
	rect_.h = 0;
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
		}
		//Get rid of old loaded surface
		SDL_FreeSurface(load_surface);
	}
	//Return success
	p_object_ = new_Texture;
	return p_object_ != NULL;
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


void BaseObject::free() 
{
	if (p_object_ != NULL)
	{
		SDL_DestroyTexture(p_object_);
		p_object_ = NULL;
		rect_.x = 0;
		rect_.y = 0;
	}
}
