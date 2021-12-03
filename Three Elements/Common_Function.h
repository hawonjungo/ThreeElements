#pragma once
#ifndef COMMON_FUNCTION_H_
#define COMMON_FUNCTION_H_

//Using SDL, SDL_image, standard IO, and strings
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <string>

#include "Image.h"


//Screen dimension constants
const int SCREEN_WIDTH = 928;
const int SCREEN_HEIGHT = 544;


//Loads individual image as texture
SDL_Texture* loadTexture(std::string path);

//The window we'll be rendering to
SDL_Window* gWindow = NULL;

//The window renderer
SDL_Renderer* gRenderer = NULL;

//Current displayed texture
SDL_Texture* bgTexture = NULL;
SDL_Texture* gTexture = NULL;
//Walking animation

//------------RECT-----------------
SDL_Rect srcRect;
SDL_Rect destRect;


//Load image at specified path
SDL_Surface* loadedSurface;


namespace SDLCommonFunc {





    //Loads image at specified path
    bool loadFromFile(std::string path);

    //Deallocates texture
    void free();

    //Renders texture at given point
    void render(int x, int y, SDL_Rect* clip = NULL);





}

#endif