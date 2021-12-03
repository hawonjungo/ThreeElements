#pragma once
#include "Common_Function.h"
class Image {
public:
    //Initializes variables
    Image();
    //Deallocates memory
    ~Image();

    SDL_Texture* initImg(std::string path);
    void draw(int x, int y, SDL_Renderer* renderer);

   
    //Gets image dimensions
    int getWidth();
    int getHeight();
    SDL_Texture* getTexture();

    void setWidth(int w);
    void setHeight(int h);
    void setTexture(SDL_Texture* tex);
protected:

    //The actual hardware texture
    SDL_Texture* mTexture;

    //Image dimensions
    int mWidth;
    int mHeight;
    int posX;
    int posY;
};

