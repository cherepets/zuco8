#include "renderer_gles2.h"

#include <string.h>

static SDL_Window s_window;
static SDL_Renderer s_renderer;
static Uint32 s_initialized;
static const char* s_error = "";

enum
{
    MAX_TOUCH_FINGERS = 4,
    MAX_TOUCH_EVENTS = 16
};

typedef struct
{
    SDL_Finger finger;
    bool active;
    bool seen_this_frame;
} ZuneFinger;

static ZuneFinger s_fingers[MAX_TOUCH_FINGERS];
static SDL_Finger* s_finger_list[MAX_TOUCH_FINGERS];
static SDL_Event s_touch_events[MAX_TOUCH_EVENTS];
static int s_event_head;
static int s_event_count;

float SDL_ZuneClampUnit(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static void QueueTouchEvent(Uint32 type, const SDL_Finger* finger,
    float previous_x, float previous_y)
{
    SDL_Event* event;
    int tail;

    if (s_event_count >= MAX_TOUCH_EVENTS)
    {
        return;
    }
    tail = (s_event_head + s_event_count) % MAX_TOUCH_EVENTS;
    event = &s_touch_events[tail];
    memset(event, 0, sizeof(*event));
    event->type = type;
    event->tfinger.type = type;
    event->tfinger.touchID = 0;
    event->tfinger.fingerID = finger->id;
    event->tfinger.x = finger->x;
    event->tfinger.y = finger->y;
    event->tfinger.dx = finger->x - previous_x;
    event->tfinger.dy = finger->y - previous_y;
    event->tfinger.pressure = finger->pressure;
    ++s_event_count;
}

static ZuneFinger* FindFinger(Uint64 finger_id)
{
    int index;

    for (index = 0; index < MAX_TOUCH_FINGERS; ++index)
    {
        if (s_fingers[index].active && s_fingers[index].finger.id == finger_id)
        {
            return &s_fingers[index];
        }
    }
    return 0;
}

static ZuneFinger* AllocateFinger(void)
{
    int index;

    for (index = 0; index < MAX_TOUCH_FINGERS; ++index)
    {
        if (!s_fingers[index].active)
        {
            return &s_fingers[index];
        }
    }
    return 0;
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
    SDL_ZuneTouchReset();
}

bool SDL_PollEvent(SDL_Event* event)
{
    if (!event || s_event_count == 0)
    {
        return false;
    }
    *event = s_touch_events[s_event_head];
    s_event_head = (s_event_head + 1) % MAX_TOUCH_EVENTS;
    --s_event_count;
    return true;
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

SDL_Finger** SDL_GetTouchFingers(Uint64 touch_id, int* count)
{
    int index;
    int active_count = 0;

    (void)touch_id;
    for (index = 0; index < MAX_TOUCH_FINGERS; ++index)
    {
        if (s_fingers[index].active)
        {
            s_finger_list[active_count++] = &s_fingers[index].finger;
        }
    }
    if (count)
    {
        *count = active_count;
    }
    return active_count ? s_finger_list : 0;
}

const bool* SDL_GetKeyboardState(int* count)
{
    static bool no_keys[1];

    if (count)
    {
        *count = 1;
    }
    return no_keys;
}

SDL_Gamepad* SDL_GetGamepadFromPlayerIndex(int player_index)
{
    (void)player_index;
    return 0;
}

bool SDL_GetGamepadButton(SDL_Gamepad* gamepad, int button)
{
    (void)gamepad;
    (void)button;
    return false;
}

void SDL_ZuneTouchBeginFrame(void)
{
    int index;

    for (index = 0; index < MAX_TOUCH_FINGERS; ++index)
    {
        s_fingers[index].seen_this_frame = false;
    }
}

void SDL_ZuneTouchUpdate(Uint64 finger_id, float x, float y, float pressure)
{
    ZuneFinger* finger = FindFinger(finger_id);
    float previous_x;
    float previous_y;

    x = SDL_ZuneClampUnit(x);
    y = SDL_ZuneClampUnit(y);
    pressure = SDL_ZuneClampUnit(pressure);
    if (!finger)
    {
        finger = AllocateFinger();
        if (!finger)
        {
            return;
        }
        memset(finger, 0, sizeof(*finger));
        finger->finger.id = finger_id;
        finger->finger.x = x;
        finger->finger.y = y;
        finger->finger.pressure = pressure;
        finger->active = true;
        finger->seen_this_frame = true;
        QueueTouchEvent(SDL_EVENT_FINGER_DOWN, &finger->finger, x, y);
        return;
    }

    previous_x = finger->finger.x;
    previous_y = finger->finger.y;
    finger->seen_this_frame = true;
    finger->finger.x = x;
    finger->finger.y = y;
    finger->finger.pressure = pressure;
    if (x != previous_x || y != previous_y)
    {
        QueueTouchEvent(SDL_EVENT_FINGER_MOTION, &finger->finger, previous_x,
            previous_y);
    }
}

void SDL_ZuneTouchEndFrame(void)
{
    int index;

    for (index = 0; index < MAX_TOUCH_FINGERS; ++index)
    {
        if (s_fingers[index].active && !s_fingers[index].seen_this_frame)
        {
            QueueTouchEvent(SDL_EVENT_FINGER_UP, &s_fingers[index].finger,
                s_fingers[index].finger.x, s_fingers[index].finger.y);
            s_fingers[index].active = false;
        }
    }
}

void SDL_ZuneTouchReset(void)
{
    memset(s_fingers, 0, sizeof(s_fingers));
    s_event_head = 0;
    s_event_count = 0;
}
