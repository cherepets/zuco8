#include "renderer_gles2.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

#include "../../core.h"

enum
{
    LOGICAL_WIDTH = 160,
    LOGICAL_HEIGHT = 283,
    GAME_X = 16,
    GAME_Y = 64,
    GAME_SIZE = 128,
    MAX_TOUCHES = 4,
    TRACE_POINTS = 12
};

typedef struct
{
    bool active;
    bool seen;
    bool inside_viewport;
    unsigned int id;
    float raw_x;
    float raw_y;
    float pressure;
    float logical_x;
    float logical_y;
    float trace_x[TRACE_POINTS];
    float trace_y[TRACE_POINTS];
    int trace_count;
} TouchDiagnostic;

static SDL_Texture* s_canvas;
static bool s_graphics_initialized;
static bool s_input_initialized;
static bool s_shutdown_requested;
static DWORD s_shutdown_started;
static bool s_coordinates_are_physical;
static unsigned int s_down_events;
static unsigned int s_motion_events;
static unsigned int s_up_events;
static int s_input_count;
static int s_margin_touches;
static unsigned int s_active_buttons;
static bool s_exit_region_active;
static TouchDiagnostic s_touches[MAX_TOUCHES];

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

static float NormalizeCoordinate(float value, int extent)
{
    if (value > 1.0f)
    {
        s_coordinates_are_physical = true;
    }
    if (s_coordinates_are_physical && extent > 0)
    {
        return SDL_ZuneClampUnit(value / extent);
    }
    return SDL_ZuneClampUnit(value);
}

static TouchDiagnostic* FindDiagnosticTouch(unsigned int id)
{
    int index;

    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        if (s_touches[index].active && s_touches[index].id == id)
        {
            return &s_touches[index];
        }
    }
    return 0;
}

static TouchDiagnostic* AllocateDiagnosticTouch(void)
{
    int index;

    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        if (!s_touches[index].active)
        {
            return &s_touches[index];
        }
    }
    return 0;
}

static void AddTrace(TouchDiagnostic* touch, float x, float y)
{
    int index;

    if (!touch->inside_viewport)
    {
        return;
    }
    if (touch->trace_count == TRACE_POINTS)
    {
        for (index = 1; index < TRACE_POINTS; ++index)
        {
            touch->trace_x[index - 1] = touch->trace_x[index];
            touch->trace_y[index - 1] = touch->trace_y[index];
        }
        --touch->trace_count;
    }
    touch->trace_x[touch->trace_count] = x;
    touch->trace_y[touch->trace_count] = y;
    ++touch->trace_count;
}

static unsigned int ButtonMaskAt(float x, float y)
{
    unsigned int mask = 0;

    if (x >= 7.0f && x < 29.0f && y >= 234.0f && y < 248.0f) mask |= 1;
    if (x >= 43.0f && x < 65.0f && y >= 234.0f && y < 248.0f) mask |= 2;
    if (x >= 29.0f && x < 43.0f && y >= 212.0f && y < 234.0f) mask |= 4;
    if (x >= 29.0f && x < 43.0f && y >= 248.0f && y < 270.0f) mask |= 8;
    if (x >= 92.0f && x < 120.0f && y >= 235.0f && y < 263.0f) mask |= 16;
    if (x >= 122.0f && x < 150.0f && y >= 223.0f && y < 251.0f) mask |= 32;
    return mask;
}

static bool IsExitRegion(float x, float y)
{
    return x >= 145.0f && x < 160.0f && y >= 0.0f && y < 23.0f;
}

static void PollTouches(SDL_Renderer* renderer)
{
    ZDK_INPUT_STATE input;
    int index;

    memset(&input, 0, sizeof(input));
    ZDKInput_GetState(&input);
    SDL_ZuneTouchBeginFrame();
    s_margin_touches = 0;
    s_active_buttons = 0;
    s_exit_region_active = false;
    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        s_touches[index].seen = false;
    }

    if (input.TouchState.Count < 0)
    {
        input.TouchState.Count = 0;
    }
    if (input.TouchState.Count > MAX_TOUCHES)
    {
        input.TouchState.Count = MAX_TOUCHES;
    }
    s_input_count = input.TouchState.Count;
    for (index = 0; index < input.TouchState.Count; ++index)
    {
        ZDK_TOUCH_LOCATION* location = &input.TouchState.Locations[index];
        TouchDiagnostic* touch = FindDiagnosticTouch(location->Id);
        float screen_x;
        float screen_y;
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        bool inside;

        if (!touch)
        {
            touch = AllocateDiagnosticTouch();
            if (!touch)
            {
                continue;
            }
            memset(touch, 0, sizeof(*touch));
            touch->active = true;
            touch->id = location->Id;
        }
        screen_x = NormalizeCoordinate(location->X, renderer->output_width);
        screen_y = NormalizeCoordinate(location->Y, renderer->output_height);
        inside = renderer_gles2_map_touch(renderer,
            screen_x * renderer->output_width,
            screen_y * renderer->output_height, &logical_x, &logical_y);
        touch->seen = true;
        touch->raw_x = location->X;
        touch->raw_y = location->Y;
        touch->pressure = location->Pressure;
        touch->inside_viewport = inside;
        touch->logical_x = logical_x;
        touch->logical_y = logical_y;
        if (!inside)
        {
            ++s_margin_touches;
            continue;
        }
        AddTrace(touch, logical_x, logical_y);
        s_active_buttons |= ButtonMaskAt(logical_x, logical_y);
        if (IsExitRegion(logical_x, logical_y))
        {
            s_exit_region_active = true;
        }
        SDL_ZuneTouchUpdate(location->Id, screen_x, screen_y,
            location->Pressure);
    }
    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        if (s_touches[index].active && !s_touches[index].seen)
        {
            s_touches[index].active = false;
        }
    }
    SDL_ZuneTouchEndFrame();
}

static const char* Glyph(char character)
{
    switch (character)
    {
    case 'A': return "010101111101101";
    case 'B': return "110101110101110";
    case 'C': return "111100100100111";
    case 'D': return "110101101101110";
    case 'E': return "111100110100111";
    case 'F': return "111100110100100";
    case 'G': return "011100101101011";
    case 'H': return "101101111101101";
    case 'I': return "111010010010111";
    case 'J': return "001001001101010";
    case 'K': return "101101110101101";
    case 'L': return "100100100100111";
    case 'M': return "101111111101101";
    case 'N': return "101111111111101";
    case 'O': return "010101101101010";
    case 'P': return "110101110100100";
    case 'Q': return "010101101111011";
    case 'R': return "110101110101101";
    case 'S': return "011100010001110";
    case 'T': return "111010010010010";
    case 'U': return "101101101101111";
    case 'V': return "101101101101010";
    case 'W': return "101101111111101";
    case 'X': return "101101010101101";
    case 'Y': return "101101010010010";
    case 'Z': return "111001010100111";
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "110001010100111";
    case '3': return "110001010001110";
    case '4': return "101101111001001";
    case '5': return "111100110001110";
    case '6': return "011100110101010";
    case '7': return "111001010010010";
    case '8': return "010101010101010";
    case '9': return "010101010011110";
    case ':': return "000010000010000";
    case ',': return "000000000010100";
    case '-': return "000000111000000";
    default: return 0;
    }
}

static void SetPixel(unsigned char* pixels, int x, int y, unsigned char red,
    unsigned char green, unsigned char blue)
{
    unsigned char* pixel;

    if (x < 0 || x >= LOGICAL_WIDTH || y < 0 || y >= LOGICAL_HEIGHT)
    {
        return;
    }
    pixel = &pixels[(y * LOGICAL_WIDTH + x) * 4];
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = 0xff;
}

static void FillRect(unsigned char* pixels, int x, int y, int width, int height,
    unsigned char red, unsigned char green, unsigned char blue)
{
    int row;
    int column;

    for (row = 0; row < height; ++row)
    {
        for (column = 0; column < width; ++column)
        {
            SetPixel(pixels, x + column, y + row, red, green, blue);
        }
    }
}

static void DrawText(unsigned char* pixels, int x, int y, const char* text,
    int scale, unsigned char red, unsigned char green, unsigned char blue)
{
    int character;
    int row;
    int column;

    for (character = 0; text[character]; ++character)
    {
        const char* glyph = Glyph(text[character]);
        if (glyph)
        {
            for (row = 0; row < 5; ++row)
            {
                for (column = 0; column < 3; ++column)
                {
                    if (glyph[row * 3 + column] == '1')
                    {
                        FillRect(pixels, x + character * 4 * scale + column * scale,
                            y + row * scale, scale, scale, red, green, blue);
                    }
                }
            }
        }
    }
}

static void DrawControl(unsigned char* pixels, int x, int y, int width,
    int height, bool active, const char* label)
{
    unsigned char shade = active ? 0xc0 : 0x35;

    FillRect(pixels, x, y, width, height, shade, active ? 0xd0 : 0x55, 0x50);
    FillRect(pixels, x + 1, y + 1, width - 2, height - 2, 0x18, 0x18, 0x30);
    DrawText(pixels, x + 2, y + 3, label, 1, shade, active ? 0xff : 0xaa, 0x60);
}

static void DrawDiagnostic(SDL_Renderer* renderer)
{
    void* locked_pixels;
    int pitch;
    unsigned char* pixels;
    int index;
    char line[64];
    SDL_FRect destination = { 0.0f, 0.0f, (float)LOGICAL_WIDTH,
        (float)LOGICAL_HEIGHT };

    if (!SDL_LockTexture(s_canvas, 0, &locked_pixels, &pitch))
    {
        return;
    }
    pixels = (unsigned char*)locked_pixels;
    memset(pixels, 0, LOGICAL_WIDTH * LOGICAL_HEIGHT * 4);
    FillRect(pixels, 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT, 0x08, 0x0d, 0x20);
    FillRect(pixels, GAME_X - 1, GAME_Y - 1, GAME_SIZE + 2, GAME_SIZE + 2,
        0x50, 0x80, 0xb0);
    FillRect(pixels, GAME_X, GAME_Y, GAME_SIZE, GAME_SIZE, 0x00, 0x00, 0x00);
    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        int point;
        TouchDiagnostic* touch = &s_touches[index];
        if (!touch->active || !touch->inside_viewport)
        {
            continue;
        }
        for (point = 0; point < touch->trace_count; ++point)
        {
            unsigned char intensity = (unsigned char)(60 + point * 14);
            FillRect(pixels, (int)touch->trace_x[point] - 1,
                (int)touch->trace_y[point] - 1, 3, 3, intensity, intensity,
                0xff);
        }
    }
    DrawText(pixels, 3, 3, "TOUCH DIAG", 1, 0x60, 0xf0, 0xff);
    DrawText(pixels, 115, 3, "PASS", 1, 0x60, 0xff, 0x80);
    _snprintf(line, sizeof(line) - 1, "OUT %dX%d", renderer->output_width,
        renderer->output_height);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 11, line, 1, 0xd0, 0xd0, 0x80);
    _snprintf(line, sizeof(line) - 1, "IN%d OUT%d D%u M%u U%u",
        s_input_count, s_margin_touches, s_down_events,
        s_motion_events, s_up_events);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 27, line, 1, 0xff, 0xff, 0xff);
    for (index = 0; index < MAX_TOUCHES; ++index)
    {
        TouchDiagnostic* touch = &s_touches[index];
        int raw_x;
        int raw_y;
        int pressure;

        if (!touch->active)
        {
            continue;
        }
        raw_x = s_coordinates_are_physical ? (int)touch->raw_x :
            (int)(touch->raw_x * 1000.0f);
        raw_y = s_coordinates_are_physical ? (int)touch->raw_y :
            (int)(touch->raw_y * 1000.0f);
        pressure = (int)(touch->pressure * 100.0f);
        if (touch->inside_viewport)
        {
            _snprintf(line, sizeof(line) - 1, "F%d I%u R%d,%d L%d,%d P%d",
                index, touch->id, raw_x, raw_y, (int)touch->logical_x,
                (int)touch->logical_y, pressure);
        }
        else
        {
            _snprintf(line, sizeof(line) - 1, "F%d I%u R%d,%d OUT P%d", index,
                touch->id, raw_x, raw_y, pressure);
        }
        line[sizeof(line) - 1] = '\0';
        DrawText(pixels, 3, 35 + index * 7, line, 1, 0xff,
            touch->inside_viewport ? 0xff : 0x80, 0x80);
    }
    DrawText(pixels, 3, 202, "PAD L R U D O X", 1, 0xa0, 0xd0, 0xff);
    DrawControl(pixels, 7, 234, 22, 14, (s_active_buttons & 1) != 0, "L");
    DrawControl(pixels, 43, 234, 22, 14, (s_active_buttons & 2) != 0, "R");
    DrawControl(pixels, 29, 212, 14, 22, (s_active_buttons & 4) != 0, "U");
    DrawControl(pixels, 29, 248, 14, 22, (s_active_buttons & 8) != 0, "D");
    DrawControl(pixels, 92, 235, 28, 28, (s_active_buttons & 16) != 0, "O");
    DrawControl(pixels, 122, 223, 28, 28, (s_active_buttons & 32) != 0, "X");
    DrawControl(pixels, 145, 0, 15, 23, s_exit_region_active, "E");
    DrawText(pixels, 115, 25, "HOME EXIT", 1, 0xff, 0xc0, 0x60);
    SDL_UnlockTexture(s_canvas);
    SDL_RenderTexture(renderer, s_canvas, 0, &destination);
    SDL_RenderPresent(renderer);
}

void handle_resize(SDL_Renderer* renderer)
{
    if (renderer && renderer->initialized)
    {
        renderer_gles2_set_viewport(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);
    }
}

bool init_core(SDL_Renderer* renderer)
{
    SuppressReboot();
    ZDKSystem_ShowSplashScreen(false);
    SystemIdleTimerReset();
    if (FAILED(ZDKInput_Initialize()))
    {
        return false;
    }
    s_input_initialized = true;
    ZDKGL_Initialize();
    s_graphics_initialized = true;
    if (!renderer_gles2_initialize(renderer))
    {
        return false;
    }
    renderer_gles2_set_viewport(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);
    s_canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, LOGICAL_WIDTH, LOGICAL_HEIGHT);
    if (!s_canvas || !SDL_SetTextureScaleMode(s_canvas, SDL_SCALEMODE_NEAREST))
    {
        return false;
    }
    return true;
}

bool handle_events(SDL_Renderer* renderer, SDL_Event* event)
{
    float logical_x;
    float logical_y;

    if (!renderer || !event)
    {
        return true;
    }
    if (event->type == SDL_EVENT_FINGER_DOWN)
    {
        ++s_down_events;
        if (renderer_gles2_map_touch(renderer,
            event->tfinger.x * renderer->output_width,
            event->tfinger.y * renderer->output_height, &logical_x, &logical_y) &&
            IsExitRegion(logical_x, logical_y))
        {
            s_shutdown_requested = true;
            s_shutdown_started = GetTickCount();
        }
    }
    else if (event->type == SDL_EVENT_FINGER_MOTION)
    {
        ++s_motion_events;
    }
    else if (event->type == SDL_EVENT_FINGER_UP)
    {
        ++s_up_events;
    }
    return true;
}

bool iterate_core(SDL_Renderer* renderer)
{
    DWORD now = GetTickCount();

    PollTouches(renderer);
    ZDKGL_BeginDraw();
    SDL_SetRenderDrawColor(renderer, 0x08, 0x0d, 0x20, 0xff);
    SDL_RenderClear(renderer);
    DrawDiagnostic(renderer);
    ZDKGL_EndDraw();
    return !s_shutdown_requested || now - s_shutdown_started < 350;
}

void destroy_core(void)
{
    SDL_DestroyTexture(s_canvas);
    s_canvas = 0;
    SDL_ZuneTouchReset();
    if (s_graphics_initialized)
    {
        renderer_gles2_shutdown(0);
        ZDKGL_Cleanup();
        s_graphics_initialized = false;
    }
    if (s_input_initialized)
    {
        ZDKInput_Shutdown();
        s_input_initialized = false;
    }
}
