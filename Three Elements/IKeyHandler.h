#pragma once
#include <SDL.h>

class IKeyHandler {
public:
    virtual void handleKeyPress(SDL_Event key) = 0;
};
