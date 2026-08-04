#include "SDL3/SDL_main.h"
#include "SDL3/SDL_zune_ext.h"
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
        bool locked = false;
        bool guide_visible = false;

        SDL_ZuneQuerySuspendState(&locked, &guide_visible);
        if (locked || guide_visible)
        {
            ZDKGL_BeginDraw();
            SDL_SetRenderDrawColor(SDL_ZuneGetRenderer(), 0x00, 0x00, 0x00,
                0xff);
            SDL_RenderClear(SDL_ZuneGetRenderer());
            ZDKGL_EndDraw();

            SDL_Delay(250);
            continue;
        }

        if (SDL_ZuneQueryExitRequested()) // NEW, Check if Menu Button Pressed
        {
            result = SDL_APP_SUCCESS;
            break;
        }

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
