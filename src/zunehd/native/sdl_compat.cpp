#include "renderer_gles2.h"

#include <string.h>

static SDL_Window s_window;
static SDL_Renderer s_renderer;
static Uint32 s_initialized;
static const char* s_error = "";

bool SDL_SetHint(const char* name, const char* value)
{
    (void)name;
    (void)value;
    return true;
}

void SDL_SetLogPriorities(int priority)
{
    (void)priority;
}

bool SDL_SetAppMetadata(const char* name, const char* version,
    const char* identifier)
{
    (void)name;
    (void)version;
    (void)identifier;
    return true;
}

bool SDL_SetAppMetadataProperty(const char* name, const char* value)
{
    (void)name;
    (void)value;
    return true;
}

bool SDL_Init(Uint32 flags)
{
    s_initialized |= flags;
    return true;
}

bool SDL_InitSubSystem(Uint32 flags)
{
    s_initialized |= flags;
    return true;
}

void SDL_Quit(void)
{
    s_initialized = 0;
    s_renderer.window = 0;
}

SDL_Window* SDL_CreateWindow(const char* title, int width, int height,
    Uint32 flags)
{
    (void)title;
    (void)flags;

    if ((s_initialized & SDL_INIT_VIDEO) == 0)
    {
        s_error = "video subsystem not initialized";
        return 0;
    }

    s_window.width = width;
    s_window.height = height;
    return &s_window;
}

SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, const char* name)
{
    (void)name;

    if (!window)
    {
        s_error = "window was not created";
        return 0;
    }

    s_renderer.window = window;
    return &s_renderer;
}

SDL_Window* SDL_GetRenderWindow(SDL_Renderer* renderer)
{
    return renderer ? renderer->window : 0;
}

bool SDL_GetWindowSize(SDL_Window* window, int* width, int* height)
{
    if (!window || !width || !height)
    {
        return false;
    }
    *width = window->width;
    *height = window->height;
    return true;
}

bool SDL_GetRenderOutputSize(SDL_Renderer* renderer, int* width, int* height)
{
    if (!renderer || !width || !height)
    {
        return false;
    }
    *width = renderer->output_width;
    *height = renderer->output_height;
    return true;
}

SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID device,
    const SDL_AudioSpec* spec)
{
    (void)device;
    (void)spec;

    /* TODO: is what feelings sound like */
    return 1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID device)
{
    (void)device;
}

SDL_PropertiesID SDL_GetRendererProperties(SDL_Renderer* renderer)
{
    return renderer;
}

void* SDL_GetPointerProperty(SDL_PropertiesID properties, const char* name,
    void* fallback)
{
    static const SDL_PixelFormat formats[] = { SDL_PIXELFORMAT_RGBA32,
        SDL_PIXELFORMAT_UNKNOWN };
    (void)properties;
    if (name && strcmp(name, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER) == 0)
    {
        return (void*)formats;
    }
    return fallback;
}

const SDL_PixelFormatDetails* SDL_GetPixelFormatDetails(SDL_PixelFormat format)
{
    static SDL_PixelFormatDetails details = { SDL_PIXELFORMAT_RGBA32 };
    return format == SDL_PIXELFORMAT_RGBA32 ? &details : 0;
}

unsigned int SDL_MapRGB(const SDL_PixelFormatDetails* details, void* palette,
    unsigned char red, unsigned char green, unsigned char blue)
{
    (void)details;
    (void)palette;
    return red | ((unsigned int)green << 8) | ((unsigned int)blue << 16) |
        0xff000000u;
}

void SDL_Log(const char* format, ...)
{
    (void)format;
}

const char* SDL_GetError(void)
{
    return s_error;
}
