#include "Image.h"

int Image::getWidth()
{
    return mWidth;
}

int Image::getHeight()
{
    return mHeight;
}
SDL_Texture* Image::getTexture() {
    return mTexture;
}
void Image::setWidth(int w) {
    mWidth = w;
}
void Image::setHeight(int h) {
    mHeight = h;
}
void Image::setTexture(SDL_Texture* tex) {
    mTexture = tex;
}

SDL_Texture* Image::initImg(std::string path) {
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
void Image::draw(int x, int y, SDL_Renderer* renderer) {
    posX = x;
    posY = y;
    gTexture = loadTexture("assets/run.png");
    SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);

}