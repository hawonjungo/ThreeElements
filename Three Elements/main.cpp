#include "GameManager.h"
#include "BaseObject.h"



GameManager* g_GameMan = GameManager::getInstace();


 int main(int argc, char* args[])
 {

     bool ret = g_GameMan->InitSDL();
     if (ret)
     {
         g_GameMan->LoopGame();
     }
     

#if 0
     //Start up SDL and create window
     if (!baseObject->init())
     {
         printf("Failed to initialize!\n");
     }
     else
     {
         //Load media
         if (!baseObject->loadMedia())
         {
             printf("Failed to load media!\n");
         }
         else
         {

             baseObject->loadBackground();
             //gameManage->loadMainCharacter();


             



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
                 baseObject->clearScreen();


                 // animate sprite
                 baseObject->animateSprite();


                 //Render texture to screen
                 baseObject->renderScreen();



                 //Update screen
                 baseObject->updateScreen();


             }
         }
     }

     //Free resources and close SDL
     baseObject->close();


     system("pause");
#endif
     return 0;
 }