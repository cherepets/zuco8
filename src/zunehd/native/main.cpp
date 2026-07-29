#include "SDL3/SDL_main.h"

int main(int argc, char* argv[])
{
    void* appstate = 0;
    SDL_AppResult result = SDL_AppInit(&appstate, argc, argv);

    while (result == SDL_APP_CONTINUE)
        result = SDL_AppIterate(appstate);

    SDL_AppQuit(appstate, result);
    SDL_Quit();
    return result == SDL_APP_FAILURE ? 1 : 0;
}
