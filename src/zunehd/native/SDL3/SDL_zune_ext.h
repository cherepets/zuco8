#pragma once

#include "SDL.h"

typedef enum SDL_ZuneOrientation
{
    SDL_ZUNE_ORIENTATION_PORTRAIT,
    SDL_ZUNE_ORIENTATION_LANDSCAPE,
    SDL_ZUNE_ORIENTATION_LANDSCAPE_FLIPPED
} SDL_ZuneOrientation;

void SDL_ZuneTouchBeginFrame(void);
void SDL_ZuneTouchUpdate(Uint64 finger_id, float x, float y, float pressure);
void SDL_ZuneTouchEndFrame(void);
void SDL_ZuneTouchReset(void);
float SDL_ZuneClampUnit(float value);
bool SDL_ZuneSetWorkingDirectoryFromModule(void);
void SDL_ZuneTouchPollReset(void);
SDL_Renderer* SDL_ZuneGetRenderer(void);
void SDL_ZuneQuerySuspendState(bool* locked, bool* guide_visible);
bool SDL_ZuneQueryExitRequested(void);
SDL_ZuneOrientation SDL_ZuneGetOrientation(void);
