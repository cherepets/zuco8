#pragma once

#include "SDL3/SDL.h"

struct SDL_Window
{
    int width;
    int height;
};

struct SDL_Texture
{
    unsigned int handle;
    unsigned char* pixels;
    int width;
    int height;
    int access;
    int blend_mode;
    bool locked;
};

struct SDL_Renderer
{
    SDL_Window* window;
    int output_width;
    int output_height;
    int logical_width;
    int logical_height;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    unsigned char clear_red;
    unsigned char clear_green;
    unsigned char clear_blue;
    unsigned char clear_alpha;
    int diagnostic_blend_mode;
    bool diagnostic_blend_enabled;
    unsigned int diagnostic_gl_error;
    bool initialized;
};

bool renderer_gles2_initialize(SDL_Renderer* renderer);
void renderer_gles2_shutdown(SDL_Renderer* renderer);
void renderer_gles2_set_viewport(SDL_Renderer* renderer, int width, int height);
bool renderer_gles2_map_touch(const SDL_Renderer* renderer, float physical_x,
    float physical_y, float* logical_x, float* logical_y);
