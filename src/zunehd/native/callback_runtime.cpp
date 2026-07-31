#include "renderer_gles2.h"
#include "png_texture.h"

#include <string.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

enum
{
    LOGICAL_WIDTH = 136,
    LOGICAL_HEIGHT = 240,
    GAME_X = 4,
    GAME_Y = 4,
    GAME_SIZE = 128,
    MAX_TOUCHES = 4,
    DPAD_X = 0,
    DPAD_Y = 162,
    MENU_X = 44,
    MENU_Y = 121,
    O_X = 65,
    O_Y = 192,
    X_X = 88,
    X_Y = 145,
    DPAD_CELL_SIZE = 16,
    DPAD_DRAW_SIZE = 54,
    BUTTON_CELL_SIZE = 14,
    BUTTON_DRAW_SIZE = 48
};

static SDL_Texture* s_canvas;
static SDL_Texture* s_buttons_texture;
static SDL_Texture* s_dpad_texture;
static bool s_graphics_initialized;
static bool s_input_initialized;
static bool s_shutdown_requested;
static DWORD s_shutdown_started;
static bool s_coordinates_are_physical;
static unsigned int s_active_buttons;

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

static unsigned int ButtonMaskAt(float x, float y)
{
    unsigned int mask = 0;
    float dpad_x = x - DPAD_X;
    float dpad_y = y - DPAD_Y;

    if (dpad_x >= 13.0f && dpad_x < 41.0f && dpad_y >= 0.0f &&
        dpad_y < 20.0f) mask |= 4;
    if (dpad_x >= 34.0f && dpad_x < 54.0f && dpad_y >= 13.0f &&
        dpad_y < 41.0f) mask |= 2;
    if (dpad_x >= 13.0f && dpad_x < 41.0f && dpad_y >= 34.0f &&
        dpad_y < 54.0f) mask |= 8;
    if (dpad_x >= 0.0f && dpad_x < 20.0f && dpad_y >= 13.0f &&
        dpad_y < 41.0f) mask |= 1;
    if (x >= O_X && x < O_X + BUTTON_DRAW_SIZE && y >= O_Y &&
        y < O_Y + BUTTON_DRAW_SIZE) mask |= 16;
    if (x >= X_X && x < X_X + BUTTON_DRAW_SIZE && y >= X_Y &&
        y < X_Y + BUTTON_DRAW_SIZE) mask |= 32;
    if (x >= MENU_X && x < MENU_X + BUTTON_DRAW_SIZE && y >= MENU_Y &&
        y < MENU_Y + BUTTON_DRAW_SIZE) mask |= 64;
    return mask;
}

static void PollTouches(SDL_Renderer* renderer)
{
    ZDK_INPUT_STATE input;
    int index;

    memset(&input, 0, sizeof(input));
    ZDKInput_GetState(&input);
    SDL_ZuneTouchBeginFrame();
    s_active_buttons = 0;

    if (input.TouchState.Count < 0)
    {
        input.TouchState.Count = 0;
    }
    if (input.TouchState.Count > MAX_TOUCHES)
    {
        input.TouchState.Count = MAX_TOUCHES;
    }
    for (index = 0; index < input.TouchState.Count; ++index)
    {
        ZDK_TOUCH_LOCATION* location = &input.TouchState.Locations[index];
        float screen_x;
        float screen_y;
        float logical_x = 0.0f;
        float logical_y = 0.0f;
        bool inside;

        screen_x = NormalizeCoordinate(location->X, renderer->output_width);
        screen_y = NormalizeCoordinate(location->Y, renderer->output_height);
        inside = renderer_gles2_map_touch(renderer,
            screen_x * renderer->output_width,
            screen_y * renderer->output_height, &logical_x, &logical_y);
        if (!inside)
        {
            continue;
        }
        s_active_buttons |= ButtonMaskAt(logical_x, logical_y);
        SDL_ZuneTouchUpdate(location->Id, screen_x, screen_y,
            location->Pressure);
    }
    SDL_ZuneTouchEndFrame();
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
    case '.': return "000000000000010";
    case '-': return "000000111000000";
    case '\\': return "100010001000100";
    case '/': return "001001010100100";
    default: return 0;
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

static void DrawSprite(SDL_Renderer* renderer, SDL_Texture* texture,
    int source_x, int source_y, int source_width, int source_height,
    int destination_x, int destination_y, int destination_width,
    int destination_height)
{
    SDL_FRect source = { (float)source_x, (float)source_y,
        (float)source_width, (float)source_height };
    SDL_FRect destination = { (float)destination_x, (float)destination_y,
        (float)destination_width, (float)destination_height };

    SDL_RenderTexture(renderer, texture, &source, &destination);
}

static void DrawFrame(SDL_Renderer* renderer)
{
    void* locked_pixels;
    int pitch;
    unsigned char* pixels;
    SDL_FRect destination = { 0.0f, 0.0f, (float)LOGICAL_WIDTH,
        (float)LOGICAL_HEIGHT };

    if (!SDL_LockTexture(s_canvas, 0, &locked_pixels, &pitch))
    {
        return;
    }
    pixels = (unsigned char*)locked_pixels;
    memset(pixels, 0, LOGICAL_WIDTH * LOGICAL_HEIGHT * 4);
    FillRect(pixels, 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT, 0x00, 0x00, 0x00);
    FillRect(pixels, GAME_X - 1, GAME_Y - 1, GAME_SIZE + 2, GAME_SIZE + 2,
        0xff, 0xff, 0xff);
    FillRect(pixels, GAME_X, GAME_Y, GAME_SIZE, GAME_SIZE, 0x00, 0x00, 0x00);
    SDL_UnlockTexture(s_canvas);
    SDL_RenderTexture(renderer, s_canvas, 0, &destination);
    DrawSprite(renderer, s_dpad_texture, 0, 0, DPAD_CELL_SIZE,
        DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE, DPAD_DRAW_SIZE);
    if (s_active_buttons & 4)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (s_active_buttons & 2)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 2, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (s_active_buttons & 8)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 3, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (s_active_buttons & 1)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 4, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    DrawSprite(renderer, s_buttons_texture,
        (s_active_buttons & 64) ? BUTTON_CELL_SIZE : 0, BUTTON_CELL_SIZE * 2,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, MENU_X, MENU_Y,
        BUTTON_DRAW_SIZE, BUTTON_DRAW_SIZE);
    DrawSprite(renderer, s_buttons_texture,
        (s_active_buttons & 16) ? BUTTON_CELL_SIZE : 0, 0,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, O_X, O_Y, BUTTON_DRAW_SIZE,
        BUTTON_DRAW_SIZE);
    DrawSprite(renderer, s_buttons_texture,
        (s_active_buttons & 32) ? BUTTON_CELL_SIZE : 0, BUTTON_CELL_SIZE,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, X_X, X_Y, BUTTON_DRAW_SIZE,
        BUTTON_DRAW_SIZE);
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
    if (!LoadPngTexture(renderer, "buttons.png", BUTTON_CELL_SIZE * 2,
        BUTTON_CELL_SIZE * 3, &s_buttons_texture) || !LoadPngTexture(renderer,
        "dpad.png", DPAD_CELL_SIZE * 5, DPAD_CELL_SIZE, &s_dpad_texture))
    {
        SDL_DestroyTexture(s_buttons_texture);
        s_buttons_texture = 0;
        SDL_DestroyTexture(s_dpad_texture);
        s_dpad_texture = 0;
        return false;
    }
    return true;
}

bool handle_events(SDL_Renderer* renderer, SDL_Event* event)
{
    (void)renderer;
    (void)event;
    return true;
}

bool iterate_core(SDL_Renderer* renderer)
{
    DWORD now = GetTickCount();

    PollTouches(renderer);
    ZDKGL_BeginDraw();
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_RenderClear(renderer);
    DrawFrame(renderer);
    SDL_RenderPresent(renderer);
    ZDKGL_EndDraw();
    return !s_shutdown_requested || now - s_shutdown_started < 350;
}

void destroy_core(void)
{
    SDL_DestroyTexture(s_dpad_texture);
    s_dpad_texture = 0;
    SDL_DestroyTexture(s_buttons_texture);
    s_buttons_texture = 0;
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
