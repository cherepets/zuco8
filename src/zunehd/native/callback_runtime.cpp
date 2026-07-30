#include "renderer_gles2.h"

#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

#include "../../core.h"

static SDL_Texture* s_checker;
static SDL_Texture* s_sprite;
static SDL_Texture* s_overlay;
static SDL_Texture* s_streaming;
static SDL_Texture* s_text;
static bool s_graphics_initialized;
static bool s_shutdown_requested;
static DWORD s_shutdown_started;
static DWORD s_start_tick;

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

static SDL_Texture* CreateTexture(SDL_Renderer* renderer, int width, int height,
    int access)
{
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, access, width,
        height);
}

static void FillChecker(SDL_Texture* texture)
{
    unsigned char pixels[64 * 64 * 4];
    int x;
    int y;

    for (y = 0; y < 64; ++y)
    {
        for (x = 0; x < 64; ++x)
        {
            unsigned char* pixel = &pixels[(y * 64 + x) * 4];
            bool light = ((x / 8) + (y / 8)) & 1;
            pixel[0] = light ? 0xe0 : 0x30;
            pixel[1] = light ? 0xe0 : 0x60;
            pixel[2] = light ? 0x40 : 0xb0;
            pixel[3] = 0xff;
        }
    }
    SDL_UpdateTexture(texture, 0, pixels, 64 * 4);
}

static void FillSprite(SDL_Texture* texture, bool translucent)
{
    unsigned char pixels[32 * 32 * 4];
    int x;
    int y;

    for (y = 0; y < 32; ++y)
    {
        for (x = 0; x < 32; ++x)
        {
            unsigned char* pixel = &pixels[(y * 32 + x) * 4];
            int distance = (x - 16) * (x - 16) + (y - 16) * (y - 16);
            pixel[0] = distance < 180 ? 0xff : 0x20;
            pixel[1] = distance < 180 ? 0x50 : 0x20;
            pixel[2] = distance < 180 ? 0x20 : 0x80;
            pixel[3] = translucent ? 0x90 : (distance < 180 ? 0xff : 0x00);
        }
    }
    SDL_UpdateTexture(texture, 0, pixels, 32 * 4);
}

static const char* Glyph(char character)
{
    switch (character)
    {
    case 'A': return "010101111101101";
    case 'B': return "110101110101110";
    case 'D': return "110101101101110";
    case 'E': return "111100110100111";
    case 'F': return "111100110100100";
    case 'G': return "011100101101011";
    case 'H': return "101101111101101";
    case 'M': return "101111111101101";
    case 'S': return "011100010001110";
    case 'V': return "101101101101010";
    case 'W': return "101101111111101";
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "110001010100111";
    case '3': return "110001010001110";
    case '4': return "101101111001001";
    case '5': return "111100110001110";
    case '6': return "011100110101010";
    case '7': return "111001010010010";
    case '8': return "010101010101010";
    case '9': return "010101011001110";
    default: return 0;
    }
}

static void FillDiagnosticText(SDL_Texture* texture, SDL_Renderer* renderer)
{
    char text[20];
    unsigned char pixels[112 * 10 * 4];
    int character;
    int row;
    int column;

    _snprintf(text, sizeof(text) - 1, "A0 M%d G%d E%d",
        renderer->diagnostic_blend_mode, renderer->diagnostic_blend_enabled,
        renderer->diagnostic_gl_error == GL_NO_ERROR ? 0 : 1);
    text[sizeof(text) - 1] = '\0';
    for (character = 0; character < sizeof(pixels); character += 4)
    {
        pixels[character] = 0;
        pixels[character + 1] = 0xff;
        pixels[character + 2] = 0;
        pixels[character + 3] = 0;
    }
    for (character = 0; text[character]; ++character)
    {
        const char* glyph = Glyph(text[character]);
        if (!glyph)
        {
            continue;
        }
        for (row = 0; row < 5; ++row)
        {
            for (column = 0; column < 3; ++column)
            {
                if (glyph[row * 3 + column] == '1')
                {
                    int x = character * 8 + column * 2;
                    int y = row * 2;
                    int dx;
                    int dy;
                    for (dy = 0; dy < 2; ++dy)
                    {
                        for (dx = 0; dx < 2; ++dx)
                        {
                            unsigned char* pixel = &pixels[((y + dy) * 112 +
                                x + dx) * 4];
                            pixel[0] = 0xff;
                            pixel[1] = 0xff;
                            pixel[2] = 0xff;
                            pixel[3] = 0xff;
                        }
                    }
                }
            }
        }
    }
    SDL_UpdateTexture(texture, 0, pixels, 112 * 4);
}

static bool InitDemo(SDL_Renderer* renderer)
{
    s_checker = CreateTexture(renderer, 64, 64, SDL_TEXTUREACCESS_STATIC);
    s_sprite = CreateTexture(renderer, 32, 32, SDL_TEXTUREACCESS_STATIC);
    s_overlay = CreateTexture(renderer, 32, 32, SDL_TEXTUREACCESS_STATIC);
    s_streaming = CreateTexture(renderer, 128, 32, SDL_TEXTUREACCESS_STREAMING);
    s_text = CreateTexture(renderer, 112, 10, SDL_TEXTUREACCESS_STATIC);
    if (!s_checker || !s_sprite || !s_overlay || !s_streaming || !s_text)
    {
        return false;
    }
    FillChecker(s_checker);
    FillSprite(s_sprite, false);
    FillSprite(s_overlay, true);
    FillDiagnosticText(s_text, renderer);
    SDL_SetTextureScaleMode(s_checker, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(s_sprite, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureScaleMode(s_overlay, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(s_sprite, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(s_overlay, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(s_text, SDL_BLENDMODE_BLEND);
    return true;
}

static void UpdateStreamingTexture(void)
{
    void* pixels;
    int pitch;
    int x;
    int y;
    DWORD tick = GetTickCount() - s_start_tick;

    if (!SDL_LockTexture(s_streaming, 0, &pixels, &pitch))
    {
        return;
    }
    for (y = 0; y < 32; ++y)
    {
        unsigned char* row = (unsigned char*)pixels + y * pitch;
        for (x = 0; x < 128; ++x)
        {
            unsigned char* pixel = row + x * 4;
            pixel[0] = (unsigned char)(x + tick / 8);
            pixel[1] = (unsigned char)(y * 8);
            pixel[2] = (unsigned char)(255 - x);
            pixel[3] = 0xff;
        }
    }
    SDL_UnlockTexture(s_streaming);
}

static bool DrawDemo(SDL_Renderer* renderer)
{
    SDL_FRect checker = { 8.0f, 24.0f, 96.0f, 96.0f };
    SDL_FRect sprite = { 42.0f, 58.0f, 48.0f, 48.0f };
    SDL_FRect overlay = { 64.0f, 32.0f, 32.0f, 32.0f };
    SDL_FRect stream = { 16.0f, 130.0f, 128.0f, 32.0f };
    SDL_FRect text = { 8.0f, 8.0f, 112.0f, 10.0f };

    if (!SDL_SetRenderDrawColor(renderer, 0xa0, 0x00, 0xa0, 0xff))
    {
        return false;
    }
    if (!SDL_RenderClear(renderer))
    {
        return false;
    }
    UpdateStreamingTexture();
    FillDiagnosticText(s_text, renderer);
    if (!SDL_RenderTexture(renderer, s_checker, 0, &checker) ||
        !SDL_RenderTexture(renderer, s_sprite, 0, &sprite) ||
        !SDL_RenderTexture(renderer, s_overlay, 0, &overlay) ||
        !SDL_RenderTexture(renderer, s_streaming, 0, &stream) ||
        !SDL_RenderTexture(renderer, s_text, 0, &text))
    {
        return false;
    }
    if (!SDL_RenderPresent(renderer))
    {
        return false;
    }
    return true;
}

void handle_resize(SDL_Renderer* renderer)
{
    /* app.c calls this before init_core creates the GLES2 context. */
    if (renderer && renderer->initialized)
    {
        renderer_gles2_set_viewport(renderer, 160, 205);
    }
}

bool init_core(SDL_Renderer* renderer)
{
    ZDK_INPUT_STATE input;

    SuppressReboot();
    ZDKSystem_ShowSplashScreen(false);
    SystemIdleTimerReset();
    ZDKGL_Initialize();
    s_graphics_initialized = true;
    if (!renderer_gles2_initialize(renderer) || !InitDemo(renderer))
    {
        return false;
    }
    ZDKInput_GetState(&input);
    s_start_tick = GetTickCount();
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
    ZDK_INPUT_STATE input;
    DWORD now;

    ZDKInput_GetState(&input);
    now = GetTickCount();
    if (input.TouchState.Count > 0 && !s_shutdown_requested)
    {
        s_shutdown_requested = true;
        s_shutdown_started = now;
    }
    ZDKGL_BeginDraw();
    if (!DrawDemo(renderer))
    {
        ZDKGL_EndDraw();
        return false;
    }
    ZDKGL_EndDraw();
    return !s_shutdown_requested || now - s_shutdown_started < 350;
}

void destroy_core(void)
{
    SDL_DestroyTexture(s_checker);
    SDL_DestroyTexture(s_sprite);
    SDL_DestroyTexture(s_overlay);
    SDL_DestroyTexture(s_streaming);
    SDL_DestroyTexture(s_text);
    s_checker = 0;
    s_sprite = 0;
    s_overlay = 0;
    s_streaming = 0;
    s_text = 0;
    if (s_graphics_initialized)
    {
        renderer_gles2_shutdown(0);
        ZDKGL_Cleanup();
        s_graphics_initialized = false;
    }
}
