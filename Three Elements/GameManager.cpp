
#include "GameManager.h"

GameManager g_background;
bool  GameManager::loadBackground() {

    bool success = true;
    ////Load PNG texture
    bgTexture = loadTexture("assets/bg.png");
    if (bgTexture == NULL)
    {
        printf("Failed to load texture image!\n");
        success = false;

              

        return success;
    }
}






void GameManager::close()
{
    //Free loaded image
    SDL_DestroyTexture(gTexture);
    gTexture = NULL;

    //Destroy window	
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;
    gRenderer = NULL;

    //Quit SDL subsystems
    IMG_Quit();
    SDL_Quit();
}


void GameManager::draw(int x, int y, SDL_Renderer* renderer) {
    posX = x;
    posY = y;
    gTexture = loadTexture("assets/run.png");
    SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);

}

GameManager::GameManager()
{
    //Initialize
    mTexture = NULL;
    mWidth = 0;
    mHeight = 0;

   
    posX = 0 ;
    posY = 0;
}

GameManager::~GameManager()
{
    //Deallocate
    free();
}



bool GameManager::loadFromFile(std::string path)
{
    //Get rid of preexisting texture
    free();

    //The final texture
    SDL_Texture* newTexture = NULL;
    //Load image at specified path
    loadedSurface = IMG_Load(path.c_str());
    if (loadedSurface == NULL)
    {
        printf("Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError());
    }
    else
    {
        //Color key image
        SDL_SetColorKey(loadedSurface, SDL_TRUE, SDL_MapRGB(loadedSurface->format, 0, 0xFF, 0xFF));

        //Create texture from surface pixels
        newTexture = SDL_CreateTextureFromSurface(gRenderer, loadedSurface);
        if (newTexture == NULL)
        {
            printf("Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
        }
        else
        {
            //Get image dimensions
            mWidth = loadedSurface->w;
            mHeight = loadedSurface->h;
        }

        //Get rid of old loaded surface
        SDL_FreeSurface(loadedSurface);
    }
    //Return success
    mTexture = newTexture;
    return mTexture != NULL;
}


void GameManager::free()
{
    //Free texture if it exists
    if (mTexture != NULL)
    {
        SDL_DestroyTexture(mTexture);
        mTexture = NULL;
        mWidth = 0;
        mHeight = 0;
    }
}

void GameManager::render(int x, int y, SDL_Rect* clip)
{
    //Set rendering space and render to screen
    SDL_Rect renderQuad = { x, y, mWidth, mHeight };

    //Set clip rendering dimensions
    if (clip != NULL)
    {
        renderQuad.w = clip->w;
        renderQuad.h = clip->h;
    }
    //Render to screen
    SDL_RenderCopy(gRenderer, mTexture, clip, &renderQuad);
}

int GameManager::getWidth()
{
    return mWidth;
}

int GameManager::getHeight()
{
    return mHeight;
}



SDL_Texture* GameManager::loadTexture(std::string path) {
    //The final texture
    SDL_Texture* newTexture = NULL;

    //Load image at specified path
    SDL_Surface* loadedSurface = IMG_Load(path.c_str());
    if (loadedSurface == NULL)
    {
        printf("Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError());
    }
    else
    {
        //Create texture from surface pixels
        newTexture = SDL_CreateTextureFromSurface(gRenderer, loadedSurface);
        if (newTexture == NULL)
        {
            printf("Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
        }

        //Get rid of old loaded surface
        SDL_FreeSurface(loadedSurface);
    }

    return newTexture;
}

void GameManager::clearScreen() {
    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
}

void GameManager::renderScreen() {
    SDL_RenderCopy(gRenderer, bgTexture, NULL, NULL);
    SDL_RenderCopy(gRenderer, gTexture, &srcRect, &destRect);
}

void GameManager::animateSprite() {
    Uint32 ticks = SDL_GetTicks();
    Uint32 sprite = (ticks / 80) % 8;

    srcRect = { static_cast<int>(sprite) * 224, 0, 224, 112 };
    destRect = { 10, 375, 224, 112 };
}

void GameManager::updateScreen() {
    SDL_RenderPresent(gRenderer);
}


