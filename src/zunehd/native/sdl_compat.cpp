#include "SDL3/SDL.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

struct SDL_Window
{
    int width;
    int height;
};

struct SDL_Renderer
{
    SDL_Window* window;
};

static SDL_Window s_window;
static SDL_Renderer s_renderer;
static Uint32 s_initialized;
static const char* s_error = "";

static void DebugOutput(const char* message)
{
    wchar_t wide_message[512];

    if (MultiByteToWideChar(CP_ACP, 0, message, -1, wide_message,
        sizeof(wide_message) / sizeof(wide_message[0])) > 0)
    {
        OutputDebugStringW(wide_message);
    }
}

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

void SDL_Log(const char* format, ...)
{
    char buffer[512];
    va_list arguments;

    va_start(arguments, format);
    _vsnprintf(buffer, sizeof(buffer) - 3, format, arguments);
    va_end(arguments);
    buffer[sizeof(buffer) - 3] = '\0';
    strcat(buffer, "\r\n");
    DebugOutput(buffer);
}

const char* SDL_GetError(void)
{
    return s_error;
}
