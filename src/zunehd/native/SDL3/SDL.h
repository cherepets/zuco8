#pragma once

typedef unsigned int Uint32;
typedef unsigned long long Uint64;
typedef unsigned int SDL_AudioDeviceID;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Event SDL_Event;
typedef struct SDL_FRect
{
    float x;
    float y;
    float w;
    float h;
} SDL_FRect;
struct SDL_Event
{
    int unused;
};
typedef struct SDL_AudioSpec
{
    int channels;
    int format;
    int freq;
} SDL_AudioSpec;
typedef enum SDL_AppResult
{
    SDL_APP_CONTINUE,
    SDL_APP_SUCCESS,
    SDL_APP_FAILURE
} SDL_AppResult;

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_GAMEPAD 0x00002000u
#define SDL_WINDOW_HIGH_PIXEL_DENSITY 0x00002000u
#define SDL_WINDOW_RESIZABLE 0x00000020u
#define SDL_AUDIO_S16 0x8010
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0u
#define SDL_LOG_PRIORITY_INFO 3
#define SDL_HINT_MOUSE_TOUCH_EVENTS "SDL_MOUSE_TOUCH_EVENTS"
#define SDL_PROP_APP_METADATA_URL_STRING "SDL.app.metadata.url"

bool SDL_SetHint(const char* name, const char* value);
void SDL_SetLogPriorities(int priority);
bool SDL_SetAppMetadata(const char* name, const char* version, const char* identifier);
bool SDL_SetAppMetadataProperty(const char* name, const char* value);
bool SDL_Init(Uint32 flags);
bool SDL_InitSubSystem(Uint32 flags);
void SDL_Quit(void);
SDL_Window* SDL_CreateWindow(const char* title, int width, int height, Uint32 flags);
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, const char* name);
SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID device, const SDL_AudioSpec* spec);
void SDL_CloseAudioDevice(SDL_AudioDeviceID device);
void SDL_Log(const char* format, ...);
const char* SDL_GetError(void);
