#include "GameManager.h"
#include <cstring>



GameManager* g_GameMan = GameManager::getInstace();


 int main(int argc, char* args[])
 {
     // --debug (development only): also print which skill the active enemy requires
     for (int i = 1; i < argc; ++i)
     {
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
     return 0;
 }
