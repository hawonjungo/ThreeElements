#pragma once

//Using SDL, SDL_image, standard IO, and strings
#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <string>

#include "BaseObject.h"

//Screen dimension constants
const int SCREEN_WIDTH = 928;
const int SCREEN_HEIGHT = 544;


//The window we'll be rendering to
static  SDL_Window* gWindow = NULL;

//The window renderer
static SDL_Renderer* gRenderer = NULL;

//Current displayed texture
static SDL_Texture* bgTexture = NULL;




class GameObject {
public:
	//Initializes variables
	GameObject();
	//Deallocates memory
	~GameObject();



	//Deallocates texture
	void free();

	//Renders texture at given point
	void render(int x, int y, SDL_Rect* clip = NULL);


	void draw(int x, int y, SDL_Renderer* renderer);


	//Starts up SDL and creates window
	bool init();

	//Loads image at specified path
	bool loadFromFile(std::string path);

	//Loads media
	bool loadMedia();

	bool loadBackground();

	//Frees media and shuts down SDL
	void close();

	//Loads individual image as texture
	SDL_Texture* loadTexture(std::string path);


	//Gets image dimensions
	int getWidth();
	int getHeight();
	
	
	void clearScreen();

	void renderScreen();

	void animateSprite();

	void updateScreen();


protected:

	
	SDL_Texture* gTexture = NULL;
	//The actual hardware texture
	SDL_Texture* mTexture;

	//Walking animation


	//------------RECT-----------------
	SDL_Rect srcRect;
	SDL_Rect destRect;



	//Load image at specified path
	SDL_Surface* loadedSurface;

	//Image dimensions
	int mWidth;
	int mHeight;
	int posX;
	int posY;
};