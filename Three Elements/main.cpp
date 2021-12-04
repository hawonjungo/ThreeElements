#include "GameObject.h"
#include "BaseObject.h"

#include "MainObject.h"



 GameObject* gameobject = new GameObject();
 MainObject player;



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


 int main(int argc, char* args[])
 {
     //Start up SDL and create window
     if (!gameobject->init())
     {
         printf("Failed to initialize!\n");
     }
     else
     {
         //Load media
         if (!gameobject->loadMedia())
         {
             printf("Failed to load media!\n");
         }
         else
         {
             gameobject->loadBackground();
             

             player.setRect(10, 375);  
             bool ret = player.loadImg("assets/mushroom_run.png", gRenderer);

             if (!ret) {
                 return 0;
             }
             player.show(gRenderer);
             








             //Main loop flag
             bool quit = false;

             //Event handler
             SDL_Event e;

             //Current animation frame
             int frame = 0;

             //While application is running
             while (!quit)
             {
                 //Handle events on queue
                 while (SDL_PollEvent(&e) != 0)
                 {
                     //User requests quit
                     if (e.type == SDL_QUIT)
                     {
                         quit = true;
                     }
                 }
                 //Clear screen
                 gameobject->clearScreen();


                 // animate sprite
                 gameobject->animateSprite();


                 //Render texture to screen
                 gameobject->renderScreen();



                 //Update screen
                 gameobject->updateScreen();


             }
         }
     }

     //Free resources and close SDL
     gameobject->close();


     system("pause");
     return 0;
 }