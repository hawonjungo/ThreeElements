#include "GameManager.h"
#include <cstdlib>
#include <cstring>



GameManager* g_GameMan = GameManager::getInstace();


 int main(int argc, char* args[])
 {
     // --debug (development only): also print which skill the active enemy requires
     // --touch (development only): show the touch buttons on a PC, to try the phone layout with the mouse
     for (int i = 1; i < argc; ++i)
     {
         if (std::strcmp(args[i], "--touch") == 0)
             g_GameMan->SetForceTouch(true);
         if (std::strcmp(args[i], "--debug") == 0)
         {
             g_GameMan->SetDebug(true);
             setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered, so tools can follow the log while the game runs
         }
     }

     bool ret = g_GameMan->InitSDL();
     if (!ret)
     {
         printf("Initialization failed, exiting.\n");
         g_GameMan->Close();
         return 1;
     }

     g_GameMan->LoopGame();
 #ifdef __ANDROID__
     // Android keeps the process after the game ends (Back on the menu, or the system closing the activity), and the
     // next launch runs main() again in it: the GameManager singleton and the other statics would come back with
     // textures of the old, destroyed renderer (pale screen, wrong pictures on the buttons). Ending the process here
     // makes every launch start clean.
     exit(0);
 #endif
     return 0;
 }
