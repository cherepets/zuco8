#pragma once

#include "renderer_gles2.h"
#include "SDL3/SDL_zune_ext.h"

bool touch_controls_initialize(SDL_Renderer* renderer);
void touch_controls_begin_frame(SDL_Renderer* renderer);
void touch_controls_render(SDL_Renderer* renderer);
void touch_controls_shutdown(void);
