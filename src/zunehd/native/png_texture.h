#pragma once

#include "renderer_gles2.h"

bool LoadPngTexture(SDL_Renderer* renderer, const char* file_name,
    int expected_width, int expected_height, SDL_Texture** texture);
