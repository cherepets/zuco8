#include "renderer_gles2.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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

static char* WideToUtf8(const wchar_t* value)
{
    int size;
    char* result;

    if (!value)
    {
        return 0;
    }
    size = WideCharToMultiByte(CP_UTF8, 0, value, -1, 0, 0, 0, 0);
    if (size <= 0)
    {
        return 0;
    }
    result = (char*)malloc(size);
    if (!result)
    {
        return 0;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, value, -1, result, size, 0, 0) <= 0)
    {
        free(result);
        return 0;
    }
    return result;
}

static wchar_t* Utf8ToWide(const char* value)
{
    int size;
    wchar_t* result;

    if (!value)
    {
        return 0;
    }
    size = MultiByteToWideChar(CP_UTF8, 0, value, -1, 0, 0);
    if (size <= 0)
    {
        return 0;
    }
    result = (wchar_t*)malloc(size * sizeof(wchar_t));
    if (!result)
    {
        return 0;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, value, -1, result, size) <= 0)
    {
        free(result);
        return 0;
    }
    return result;
}

static bool GetModuleDirectory(wchar_t* directory, int capacity)
{
    DWORD length;
    int index;

    if (!directory || capacity <= 1)
    {
        s_error = "invalid module path buffer";
        return false;
    }
    length = GetModuleFileNameW(0, directory, capacity);
    if (length == 0 || length >= (DWORD)capacity)
    {
        s_error = "could not get module path";
        return false;
    }
    directory[length] = 0;
    for (index = (int)length - 1; index >= 0; --index)
    {
        if (directory[index] == L'\\' || directory[index] == L'/')
        {
            directory[index + 1] = 0;
            return true;
        }
    }
    s_error = "module path has no directory";
    return false;
}

bool SDL_ZuneSetWorkingDirectoryFromModule(void)
{
    wchar_t directory[MAX_PATH];
    HMODULE coredll;
    typedef BOOL (WINAPI *SetCurrentDirectoryWProc)(LPCWSTR directory);
    SetCurrentDirectoryWProc set_current_directory;

    if (!GetModuleDirectory(directory, MAX_PATH))
    {
        return false;
    }

    coredll = GetModuleHandle(L"coredll.dll");
    set_current_directory = coredll ? (SetCurrentDirectoryWProc)GetProcAddress(
        coredll, L"SetCurrentDirectoryW") : 0;
    if (!set_current_directory)
    {
        s_error = "SetCurrentDirectoryW is unavailable";
        return false;
    }
    if (!set_current_directory(directory))
    {
        s_error = "could not set module directory";
        return false;
    }
    return true;
}

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

char* SDL_GetBasePath(void)
{
    wchar_t directory[MAX_PATH];
    char* result;

    if (!GetModuleDirectory(directory, MAX_PATH))
    {
        return 0;
    }
    result = WideToUtf8(directory);
    if (!result)
    {
        s_error = "could not convert module directory";
    }
    return result;
}

int SDL_asprintf(char** strp, const char* format, ...)
{
    int capacity = 128;
    int written;
    char* result;
    va_list arguments;

    if (!strp || !format)
    {
        return -1;
    }
    *strp = 0;
    while (capacity <= 32768)
    {
        result = (char*)malloc(capacity);
        if (!result)
        {
            s_error = "out of memory";
            return -1;
        }
        va_start(arguments, format);
        written = _vsnprintf(result, capacity, format, arguments);
        va_end(arguments);
        if (written >= 0 && written < capacity)
        {
            *strp = result;
            return written;
        }
        free(result);
        capacity *= 2;
    }
    s_error = "formatted path too long";
    return -1;
}

void SDL_free(void* memory)
{
    free(memory);
}

bool SDL_EnumerateDirectory(const char* path,
    SDL_EnumerateDirectoryCallback callback, void* userdata)
{
    wchar_t* wide_path;
    wchar_t pattern[MAX_PATH];
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    char* utf8_directory;
    bool result = true;
    bool stopped_early = false;
    int length;

    if (!path || !callback)
    {
        s_error = "invalid directory enumeration";
        return false;
    }
    wide_path = Utf8ToWide(path);
    if (!wide_path)
    {
        s_error = "could not convert directory path";
        return false;
    }
    length = (int)wcslen(wide_path);
    if (length <= 0 || length + 2 >= MAX_PATH)
    {
        free(wide_path);
        s_error = "directory path too long";
        return false;
    }
    memcpy(pattern, wide_path, (length + 1) * sizeof(wchar_t));
    free(wide_path);
    if (pattern[length - 1] != L'\\' && pattern[length - 1] != L'/')
    {
        pattern[length++] = L'\\';
    }
    pattern[length] = 0;
    utf8_directory = WideToUtf8(pattern);
    if (!utf8_directory)
    {
        s_error = "could not convert directory name";
        return false;
    }
    pattern[length++] = L'*';
    pattern[length] = 0;
    find_handle = FindFirstFileW(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        free(utf8_directory);
        s_error = "could not open directory";
        return false;
    }
    do
    {
        char* utf8_name;
        SDL_EnumerationResult callback_result;

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (find_data.cFileName[0] == L'.' &&
            (find_data.cFileName[1] == 0 || (find_data.cFileName[1] == L'.' &&
            find_data.cFileName[2] == 0))))
        {
            continue;
        }
        utf8_name = WideToUtf8(find_data.cFileName);
        if (!utf8_name)
        {
            s_error = "could not convert file name";
            result = false;
            break;
        }
        callback_result = callback(userdata, utf8_directory, utf8_name);
        free(utf8_name);
        if (callback_result == SDL_ENUM_FAILURE)
        {
            s_error = "directory callback failed";
            result = false;
            break;
        }
        if (callback_result == SDL_ENUM_SUCCESS)
        {
            stopped_early = true;
            break;
        }
    } while (FindNextFileW(find_handle, &find_data));
    if (result && !stopped_early && GetLastError() != ERROR_NO_MORE_FILES)
    {
        s_error = "could not continue directory enumeration";
        result = false;
    }
    FindClose(find_handle);
    free(utf8_directory);
    return result;
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
