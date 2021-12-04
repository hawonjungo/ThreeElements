
#include "GameObject.h"

BaseObject g_background;
bool  GameObject::loadBackground() {

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



bool GameObject::init() {
    //Initialization flag
    bool success = true;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        success = false;
    }
    else {
        //Set texture filtering to linear
        if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"))
        {
            printf("Warning: Linear texture filtering not enabled!");
        }

        //Create window
        gWindow = SDL_CreateWindow("Three Elements", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (gWindow == NULL) {
            printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
            //Create renderer for window
            gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
            if (gRenderer == NULL)
            {
                printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
                success = false;
            }
            else
            {
                //Initialize renderer color
                SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

                //Initialize PNG loading
                int imgFlags = IMG_INIT_PNG;
                if (!(IMG_Init(imgFlags) & imgFlags))
                {
                    printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
                    success = false;
                }
            }
        }
    }
    return success;
}

bool GameObject::loadMedia() {
    //Loading success flag
    bool success = true;


    ////Load PNG texture
    //bgTexture = loadTexture("assets/bg.png");
    //if (bgTexture == NULL)
    //{
    //    printf("Failed to load texture image!\n");
    //    success = false;
    //}

    //Load sprite sheet texture
    bool bLoadFile = loadFromFile("assets/run.bmp");
    if (!bLoadFile)
    {
        printf("Failed to load walking animation texture!\n");
        success = false;
    }
    else
    {
        loadedSurface = IMG_Load("assets/run.bmp");
        gTexture = SDL_CreateTextureFromSurface(gRenderer, loadedSurface);
        SDL_FreeSurface(loadedSurface);
    }



    return success;
}

void GameObject::close()
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


void GameObject::draw(int x, int y, SDL_Renderer* renderer) {
    posX = x;
    posY = y;
    gTexture = loadTexture("assets/run.png");
    SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);

}

GameObject::GameObject()
{
    //Initialize
    mTexture = NULL;
    mWidth = 0;
    mHeight = 0;

   
    posX = 0 ;
    posY = 0;
}

GameObject::~GameObject()
{
    //Deallocate
    free();
}



bool GameObject::loadFromFile(std::string path)
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


void GameObject::free()
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

void GameObject::render(int x, int y, SDL_Rect* clip)
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

int GameObject::getWidth()
{
    return mWidth;
}

int GameObject::getHeight()
{
    return mHeight;
}



SDL_Texture* GameObject::loadTexture(std::string path) {
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

void GameObject::clearScreen() {
    SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(gRenderer);
}

void GameObject::renderScreen() {
    SDL_RenderCopy(gRenderer, bgTexture, NULL, NULL);
    SDL_RenderCopy(gRenderer, gTexture, &srcRect, &destRect);
}

void GameObject::animateSprite() {
    Uint32 ticks = SDL_GetTicks();
    Uint32 sprite = (ticks / 80) % 8;

    srcRect = { static_cast<int>(sprite) * 224, 0, 224, 112 };
    destRect = { 10, 375, 224, 112 };
}

void GameObject::updateScreen() {
    SDL_RenderPresent(gRenderer);
}


