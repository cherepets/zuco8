#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int Uint32;
typedef unsigned long long Uint64;
typedef int Sint32;
typedef unsigned int SDL_AudioDeviceID;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Event SDL_Event;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Gamepad SDL_Gamepad;
typedef unsigned int SDL_PixelFormat;
typedef void* SDL_PropertiesID;
typedef Uint32 SDL_JoystickID;
typedef int SDL_Keycode;
typedef enum SDL_EnumerationResult
{
    SDL_ENUM_CONTINUE,
    SDL_ENUM_SUCCESS,
    SDL_ENUM_FAILURE
} SDL_EnumerationResult;
typedef SDL_EnumerationResult (*SDL_EnumerateDirectoryCallback)(void* userdata,
    const char* dirname, const char* fname);
typedef struct SDL_FRect
{
    float x;
    float y;
    float w;
    float h;
} SDL_FRect;
typedef struct SDL_Rect
{
    int x;
    int y;
    int w;
    int h;
} SDL_Rect;
typedef struct SDL_PixelFormatDetails
{
    SDL_PixelFormat format;
} SDL_PixelFormatDetails;
typedef struct SDL_Finger
{
    Uint64 id;
    float x;
    float y;
    float pressure;
} SDL_Finger;
typedef struct SDL_TouchFingerEvent
{
    Uint32 type;
    Uint64 touchID;
    Uint64 fingerID;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
} SDL_TouchFingerEvent;
typedef struct SDL_KeyboardEvent
{
    Uint32 type;
    bool repeat;
    SDL_Keycode key;
} SDL_KeyboardEvent;
typedef struct SDL_MouseButtonEvent
{
    Uint32 type;
    int button;
    float x;
    float y;
} SDL_MouseButtonEvent;
typedef struct SDL_GamepadDeviceEvent
{
    Uint32 type;
    SDL_JoystickID which;
} SDL_GamepadDeviceEvent;
typedef struct SDL_GamepadButtonEvent
{
    Uint32 type;
    SDL_JoystickID which;
    int button;
} SDL_GamepadButtonEvent;
struct SDL_Event
{
    Uint32 type;
    union
    {
        SDL_TouchFingerEvent tfinger;
        SDL_KeyboardEvent key;
        SDL_MouseButtonEvent button;
        SDL_GamepadDeviceEvent gdevice;
        SDL_GamepadButtonEvent gbutton;
    };
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
#define SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER "SDL.renderer.texture_formats"
#define SDL_PIXELFORMAT_UNKNOWN 0u
#define SDL_PIXELFORMAT_RGBA32 1u
#define SDL_TEXTUREACCESS_STATIC 0
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_SCALEMODE_NEAREST 0
#define SDL_BLENDMODE_NONE 0
#define SDL_BLENDMODE_BLEND 1
#define SDL_BYTESPERPIXEL(format) 4
#define SDL_ISPIXELFORMAT_PACKED(format) 1
#define SDL_NS_PER_US 1000ull
#define SDL_SCANCODE_5 34
#define SDL_SCANCODE_7 36
#define SDL_SCANCODE_C 6
#define SDL_SCANCODE_DOWN 81
#define SDL_SCANCODE_KP_5 93
#define SDL_SCANCODE_KP_7 95
#define SDL_SCANCODE_LEFT 80
#define SDL_SCANCODE_RIGHT 79
#define SDL_SCANCODE_UP 82
#define SDL_SCANCODE_V 25
#define SDL_SCANCODE_X 27
#define SDL_SCANCODE_Y 28
#define SDL_SCANCODE_Z 29
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN 12
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT 13
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT 14
#define SDL_GAMEPAD_BUTTON_DPAD_UP 15
#define SDL_GAMEPAD_BUTTON_EAST 1
#define SDL_GAMEPAD_BUTTON_SOUTH 0
#define SDL_EVENT_FINGER_DOWN 0x700u
#define SDL_EVENT_FINGER_UP 0x701u
#define SDL_EVENT_FINGER_MOTION 0x702u
#define SDL_EVENT_QUIT 0x100u
#define SDL_EVENT_WINDOW_RESIZED 0x200u
#define SDL_EVENT_KEY_DOWN 0x300u
#define SDL_EVENT_MOUSE_BUTTON_DOWN 0x401u
#define SDL_EVENT_MOUSE_BUTTON_UP 0x402u
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN 0x650u
#define SDL_EVENT_GAMEPAD_ADDED 0x653u
#define SDL_EVENT_GAMEPAD_REMOVED 0x654u
#define SDL_BUTTON_LEFT 1
#define SDL_GAMEPAD_BUTTON_BACK 4
#define SDL_GAMEPAD_BUTTON_START 6
#define SDLK_SOFTLEFT 1
#define SDLK_ESCAPE 2
#define SDLK_C 3
#define SDLK_X 4
#define SDLK_Z 5
#define SDLK_SELECT 6
#define SDLK_SPACE 7
#define SDLK_LEFT 8
#define SDLK_RIGHT 9
#define SDLK_EQUALS 10
#define SDL_PRIu32 "u"
#define SDL_arraysize(array) (sizeof(array) / sizeof((array)[0]))
#define SDL_malloc malloc
#define SDL_calloc calloc
#define SDL_realloc realloc
#define SDL_strstr strstr
#define SDL_memcmp memcmp
#define SDL_memcpy memcpy
#define SDL_memset memset
#define SDL_strtod strtod

bool SDL_SetHint(const char* name, const char* value);
void SDL_SetLogPriorities(int priority);
bool SDL_SetAppMetadata(const char* name, const char* version, const char* identifier);
bool SDL_SetAppMetadataProperty(const char* name, const char* value);
bool SDL_Init(Uint32 flags);
bool SDL_InitSubSystem(Uint32 flags);
void SDL_Quit(void);
Uint64 SDL_GetTicks(void);
Uint64 SDL_GetPerformanceCounter(void);
void SDL_DelayNS(Uint64 nanoseconds);
void SDL_Delay(Uint32 milliseconds);
char* SDL_GetBasePath(void);
bool SDL_EnumerateDirectory(const char* path,
    SDL_EnumerateDirectoryCallback callback, void* userdata);
int SDL_asprintf(char** strp, const char* format, ...);
void SDL_free(void* memory);
bool SDL_PollEvent(SDL_Event* event);
SDL_Window* SDL_CreateWindow(const char* title, int width, int height, Uint32 flags);
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, const char* name);
SDL_Window* SDL_GetRenderWindow(SDL_Renderer* renderer);
bool SDL_GetWindowSize(SDL_Window* window, int* width, int* height);
bool SDL_GetRenderOutputSize(SDL_Renderer* renderer, int* width, int* height);
SDL_AudioDeviceID SDL_OpenAudioDevice(SDL_AudioDeviceID device, const SDL_AudioSpec* spec);
void SDL_CloseAudioDevice(SDL_AudioDeviceID device);
SDL_PropertiesID SDL_GetRendererProperties(SDL_Renderer* renderer);
void* SDL_GetPointerProperty(SDL_PropertiesID properties, const char* name, void* fallback);
const SDL_PixelFormatDetails* SDL_GetPixelFormatDetails(SDL_PixelFormat format);
unsigned int SDL_MapRGB(const SDL_PixelFormatDetails* details, void* palette, unsigned char red, unsigned char green, unsigned char blue);
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format, int access, int width, int height);
void SDL_DestroyTexture(SDL_Texture* texture);
bool SDL_SetTextureScaleMode(SDL_Texture* texture, int scale_mode);
bool SDL_SetTextureBlendMode(SDL_Texture* texture, int blend_mode);
bool SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rectangle, const void* pixels, int pitch);
bool SDL_LockTexture(SDL_Texture* texture, const SDL_Rect* rectangle, void** pixels, int* pitch);
void SDL_UnlockTexture(SDL_Texture* texture);
bool SDL_SetRenderDrawColor(SDL_Renderer* renderer, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
bool SDL_RenderClear(SDL_Renderer* renderer);
bool SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect* source, const SDL_FRect* destination);
bool SDL_RenderPresent(SDL_Renderer* renderer);
void SDL_Log(const char* format, ...);
const char* SDL_GetError(void);
SDL_Finger** SDL_GetTouchFingers(Uint64 touch_id, int* count);
const bool* SDL_GetKeyboardState(int* count);
SDL_Gamepad* SDL_GetGamepadFromPlayerIndex(int player_index);
bool SDL_GetGamepadButton(SDL_Gamepad* gamepad, int button);
SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID id);
SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id);
void SDL_CloseGamepad(SDL_Gamepad* gamepad);
const char* SDL_GetGamepadName(SDL_Gamepad* gamepad);
