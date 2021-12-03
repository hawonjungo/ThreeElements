#include "GameObject.h"



 GameObject* gameobject = new GameObject();




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