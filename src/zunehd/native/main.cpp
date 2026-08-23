#include "SDL3/SDL_main.h"
#include "touch_controls.h"
#include <zdkgl.h>

int main(int argc, char* argv[])
{
    void* appstate = 0;
    SDL_AppResult result = SDL_AppInit(&appstate, argc, argv);

    touch_controls_initialize(SDL_ZuneGetRenderer());

    while (result == SDL_APP_CONTINUE)
    {
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            result = SDL_AppEvent(appstate, &event);
            if (result != SDL_APP_CONTINUE)
            {
                break;
            }
        }
        if (result != SDL_APP_CONTINUE)
        {
            break;
        }
        ZDKGL_BeginDraw();
        result = SDL_AppIterate(appstate);
        touch_controls_render(SDL_ZuneGetRenderer());
        ZDKGL_EndDraw();
    }

    touch_controls_shutdown();
    SDL_AppQuit(appstate, result);
    SDL_Quit();
    return result == SDL_APP_FAILURE ? 1 : 0;
}
