#include "renderer_gles2.h"
#include "SDL3/SDL_zune_ext.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

static SDL_Window s_window;
static SDL_Renderer s_renderer;
static Uint32 s_initialized;
static const char* s_error = "";

static bool s_platform_initialized;
static bool s_input_initialized;
static bool s_graphics_initialized;

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

static void SuppressReboot(void)
{
    HKEY key = 0;
    DWORD value;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"System\\CurrentControlSet\\Control\\Power\\State\\Reboot", 0, 0,
        &key) == ERROR_SUCCESS)
    {
        value = 0x10000;
        RegSetValueEx(key, L"Flags", 0, REG_DWORD, (BYTE*)&value,
            sizeof(value));
        value = 0;
        RegSetValueEx(key, L"Default", 0, REG_DWORD, (BYTE*)&value,
            sizeof(value));
        RegCloseKey(key);
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

    if (!s_platform_initialized && (flags & SDL_INIT_VIDEO))
    {
        s_platform_initialized = true;
        SuppressReboot();
        ZDKSystem_ShowSplashScreen(false);
        SystemIdleTimerReset();
        SDL_ZuneSetWorkingDirectoryFromModule();
        if (FAILED(ZDKInput_Initialize()))
        {
            s_error = "ZDKInput_Initialize failed";
            return false;
        }
        s_input_initialized = true;
    }
    return true;
}

bool SDL_InitSubSystem(Uint32 flags)
{
    s_initialized |= flags;
    return true;
}

void SDL_Quit(void)
{
    if (s_graphics_initialized)
    {
        renderer_gles2_shutdown(&s_renderer);
        ZDKGL_Cleanup();
        s_graphics_initialized = false;
    }
    if (s_input_initialized)
    {
        ZDKInput_Shutdown();
        s_input_initialized = false;
    }
    s_platform_initialized = false;
    s_initialized = 0;
    s_renderer.window = 0;
    SDL_ZuneTouchReset();
}

Uint64 SDL_GetTicks(void)
{
    return (Uint64)GetTickCount();
}

Uint64 SDL_GetPerformanceCounter(void)
{
    return (Uint64)GetTickCount() * 1000ull;
}

void SDL_DelayNS(Uint64 nanoseconds)
{
    DWORD milliseconds = (DWORD)(nanoseconds / 1000000ull);

    if (milliseconds == 0 && nanoseconds != 0)
    {
        milliseconds = 1;
    }
    Sleep(milliseconds);
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

static bool s_touch_polled_this_frame;
static bool s_touch_coordinates_are_physical;
static bool s_orientation_reference_initialized;
static int s_orientation_reference_x;
static int s_orientation_reference_y;
static SDL_ZuneOrientation s_orientation = SDL_ZUNE_ORIENTATION_PORTRAIT;

static float NormalizeZuneCoordinate(float value, int extent)
{
    if (value > 1.0f)
    {
        s_touch_coordinates_are_physical = true;
    }
    if (s_touch_coordinates_are_physical && extent > 0)
    {
        return SDL_ZuneClampUnit(value / extent);
    }
    return SDL_ZuneClampUnit(value);
}

static void UpdateOrientation(const ZDK_ACCELEROMETER_STATE* accelerometer)
{
    long long x;
    long long y;
    long long z;
    long long in_plane_magnitude_squared;
    long long total_magnitude_squared;
    long long cosine;
    long long sine;

    x = accelerometer->X;
    y = accelerometer->Y;
    z = accelerometer->Z;
    in_plane_magnitude_squared = x * x + y * y;
    total_magnitude_squared = in_plane_magnitude_squared + z * z;
    if (total_magnitude_squared <= 0 ||
        in_plane_magnitude_squared * 25 < total_magnitude_squared)
    {
        return;
    }
    if (!s_orientation_reference_initialized)
    {
        s_orientation_reference_initialized = true;
        s_orientation_reference_x = accelerometer->X;
        s_orientation_reference_y = accelerometer->Y;
    }

    cosine = y * s_orientation_reference_y + x * s_orientation_reference_x;
    sine = x * s_orientation_reference_y - y * s_orientation_reference_x;
    if (sine >= 0 && sine > cosine)
    {
        s_orientation = SDL_ZUNE_ORIENTATION_LANDSCAPE;
    }
    else if (sine < 0 && -sine > cosine)
    {
        s_orientation = SDL_ZUNE_ORIENTATION_LANDSCAPE_FLIPPED;
    }
    else
    {
        s_orientation = SDL_ZUNE_ORIENTATION_PORTRAIT;
    }
}

static void PollZuneTouch(void)
{
    ZDK_INPUT_STATE input;
    int index;

    memset(&input, 0, sizeof(input));
    ZDKInput_GetState(&input);
    UpdateOrientation(&input.AccelerometerState);
    SDL_ZuneTouchBeginFrame();

    if (input.TouchState.Count < 0)
    {
        input.TouchState.Count = 0;
    }
    if (input.TouchState.Count > MAX_TOUCH_FINGERS)
    {
        input.TouchState.Count = MAX_TOUCH_FINGERS;
    }
    for (index = 0; index < input.TouchState.Count; ++index)
    {
        ZDK_TOUCH_LOCATION* location = &input.TouchState.Locations[index];
        float x = NormalizeZuneCoordinate(location->X, s_renderer.output_width);
        float y = NormalizeZuneCoordinate(location->Y, s_renderer.output_height);

        SDL_ZuneTouchUpdate(location->Id, x, y, location->Pressure);
    }
    SDL_ZuneTouchEndFrame();
}

SDL_ZuneOrientation SDL_ZuneGetOrientation(void)
{
    return s_orientation;
}

bool SDL_PollEvent(SDL_Event* event)
{
    if (!event)
    {
        return false;
    }
    if (s_event_count == 0 && !s_touch_polled_this_frame)
    {
        PollZuneTouch();
        s_touch_polled_this_frame = true;
    }
    if (s_event_count == 0)
    {
        return false;
    }
    *event = s_touch_events[s_event_head];
    s_event_head = (s_event_head + 1) % MAX_TOUCH_EVENTS;
    --s_event_count;
    return true;
}

void SDL_ZuneTouchPollReset(void)
{
    s_touch_polled_this_frame = false;
}

SDL_Renderer* SDL_ZuneGetRenderer(void)
{
    return &s_renderer;
}

void SDL_ZuneQuerySuspendState(bool* locked, bool* guide_visible)
{
    BOOL locked_state = FALSE;
    BOOL guide_state = FALSE;
    bool lock_ok = SUCCEEDED(ZDKSystem_GetLockswitchState(&locked_state));
    bool guide_ok = SUCCEEDED(ZDKSystem_IsShowingGuide(&guide_state));

    if (locked)
    {
        *locked = lock_ok && locked_state;
    }
    if (guide_visible)
    {
        *guide_visible = guide_ok && guide_state;
    }
}

bool SDL_ZuneQueryExitRequested(void)
{
    static HANDLE exit_event = 0;

    if (!exit_event)
    {
        exit_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ZAM/AppExitEvent");
        if (!exit_event)
        {
            return false;
        }
    }
    return WaitForSingleObject(exit_event, 0) == WAIT_OBJECT_0;
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

    if (!s_graphics_initialized)
    {
        ZDKGL_Initialize();
        if (!renderer_gles2_initialize(&s_renderer))
        {
            s_error = "renderer_gles2_initialize failed";
            return 0;
        }
        s_graphics_initialized = true;
    }
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
    static bool no_keys[512];

    if (count)
    {
        *count = 512;
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

SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID id)
{
    (void)id;
    return 0;
}

SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID id)
{
    (void)id;
    return 0;
}

void SDL_CloseGamepad(SDL_Gamepad* gamepad)
{
    (void)gamepad;
}

const char* SDL_GetGamepadName(SDL_Gamepad* gamepad)
{
    (void)gamepad;
    return "";
}

void SDL_Delay(Uint32 milliseconds)
{
    Sleep(milliseconds);
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
