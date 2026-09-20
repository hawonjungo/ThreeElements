#include "GameManager.h"
#include "BaseObject.h"



GameManager* g_GameMan = GameManager::getInstace();


 int main(int argc, char* args[])
 {

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