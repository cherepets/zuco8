#pragma once

#include "renderer_gles2.h"

bool touch_controls_initialize(SDL_Renderer* renderer);
void touch_controls_render(SDL_Renderer* renderer);
void touch_controls_shutdown(void);
