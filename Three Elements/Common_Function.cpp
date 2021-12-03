#include "Common_Function.h"

Image* img = new Image();

bool SDLCommonFunc::loadFromFile(std::string path)
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
            img->setWidth(loadedSurface->w);
            img->setHeight(loadedSurface->h);
            /*mWidth = loadedSurface->w;
            mHeight = loadedSurface->h;*/
        }

        //Get rid of old loaded surface
        SDL_FreeSurface(loadedSurface);
    }
    //Return success
    img->setTexture(newTexture);
    /*mTexture = newTexture;*/
    /*return mTexture != NULL;*/
    return img->getTexture() != NULL;
}

void SDLCommonFunc::free()
{
    //Free texture if it exists
    if (img->getTexture() != NULL)
    {
        SDL_DestroyTexture(img->getTexture());

        img->setTexture(NULL);
        //mTexture = NULL;
        img->setWidth(0);
        img->setHeight(0);
    }
}
void SDLCommonFunc::render(int x, int y, SDL_Rect* clip)
{
    //Set rendering space and render to screen
    SDL_Rect renderQuad = { x, y,img->getWidth(), img->getHeight() };

    //Set clip rendering dimensions
    if (clip != NULL)
    {
        renderQuad.w = clip->w;
        renderQuad.h = clip->h;
    }
    //Render to screen
    SDL_RenderCopy(gRenderer, img->getTexture(), clip, &renderQuad);
}