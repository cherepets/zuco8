#include "renderer_gles2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

extern "C"
{
#include "../../z8lua/lua.h"
#include "../../z8lua/lauxlib.h"
#include "../../z8lua/lualib.h"
}

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

typedef struct
{
    unsigned long bytes;
    unsigned long blocks;
    unsigned long peak_bytes;
} LuaMemoryTracker;

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
static TouchDiagnostic s_touches[MAX_TOUCHES];
static char s_asset_base_path[48];
static char s_asset_first_cart[32];
static unsigned int s_asset_cart_count;
static long s_buttons_size;
static long s_dpad_size;
static const char* s_asset_error;
static bool s_assets_ready;
static bool s_working_directory_ready;
static int s_lua_embedded_status;
static int s_lua_file_status;
static int s_lua_syntax_status;
static int s_lua_runtime_status;
static int s_lua_embedded_result;
static int s_lua_file_result;
static unsigned int s_lua_cycles;
static unsigned long s_lua_retained_bytes;
static unsigned long s_lua_retained_blocks;
static unsigned long s_lua_peak_bytes;
static bool s_lua_cycles_clean;
static bool s_lua_ready;
static char s_lua_embedded_error[26];
static char s_lua_file_error[26];
static char s_lua_syntax_error[26];
static char s_lua_runtime_error[26];

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

static void RecordAssetError(const char* error)
{
    if (!s_asset_error)
    {
        s_asset_error = error;
    }
}

static long FileSize(const char* file_name)
{
    FILE* file;
    long size;

    file = fopen(file_name, "rb");
    if (!file)
    {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return -1;
    }
    size = ftell(file);
    fclose(file);
    return size;
}

static long ModuleFileSize(const char* base_path, const char* file_name)
{
    char* path = 0;
    long size = -1;

    if (!base_path)
    {
        return FileSize(file_name);
    }
    if (SDL_asprintf(&path, "%s%s", base_path, file_name) >= 0 && path)
    {
        size = FileSize(path);
    }
    SDL_free(path);
    return size;
}

static SDL_EnumerationResult CountCart(void* userdata, const char* dirname,
    const char* file_name)
{
    (void)userdata;
    (void)dirname;

    ++s_asset_cart_count;
    if (!s_asset_first_cart[0])
    {
        strncpy(s_asset_first_cart, file_name,
            sizeof(s_asset_first_cart) - 1);
        s_asset_first_cart[sizeof(s_asset_first_cart) - 1] = '\0';
    }
    return SDL_ENUM_CONTINUE;
}

static void CopyAssetPathForDisplay(char* destination, int capacity,
    const char* source)
{
    int index;

    if (!destination || capacity <= 0)
    {
        return;
    }
    for (index = 0; source && source[index] && index < capacity - 1; ++index)
    {
        char character = source[index];

        if (character >= 'a' && character <= 'z')
        {
            character = (char)(character - 'a' + 'A');
        }
        destination[index] = character;
    }
    destination[index] = '\0';
}

static void ProbePackagedAssets(void)
{
    char* base_path;
    char* carts_path;

    s_asset_base_path[0] = '\0';
    s_asset_first_cart[0] = '\0';
    s_asset_cart_count = 0;
    s_buttons_size = -1;
    s_dpad_size = -1;
    s_asset_error = 0;
    s_assets_ready = false;

    base_path = SDL_GetBasePath();
    if (!base_path)
    {
        RecordAssetError("BASE PATH");
    }
    carts_path = 0;
    if (base_path)
    {
        CopyAssetPathForDisplay(s_asset_base_path, sizeof(s_asset_base_path),
            base_path);
        if (SDL_asprintf(&carts_path, "%scarts", base_path) < 0 ||
            !carts_path)
        {
            RecordAssetError("CART PATH");
        }
        else if (!SDL_EnumerateDirectory(carts_path, CountCart, 0))
        {
            RecordAssetError("CART ENUM");
        }
    }
    SDL_free(carts_path);

    if (s_asset_cart_count != 18)
    {
        RecordAssetError("CART COUNT");
    }
    s_buttons_size = ModuleFileSize(base_path, "buttons.png");
    if (s_buttons_size < 0)
    {
        RecordAssetError("BUTTONS READ");
    }
    s_dpad_size = ModuleFileSize(base_path, "dpad.png");
    if (s_dpad_size < 0)
    {
        RecordAssetError("DPAD READ");
    }
    SDL_free(base_path);
    s_assets_ready = s_asset_error == 0;
}

static void CopyLuaError(char* destination, int capacity, const char* source)
{
    int index;

    if (!destination || capacity <= 0)
    {
        return;
    }
    for (index = 0; source && source[index] && index < capacity - 1; ++index)
    {
        char character = source[index];

        if (character >= 'a' && character <= 'z')
        {
            character = (char)(character - 'a' + 'A');
        }
        if (!((character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == ' ' ||
            character == ':' || character == '-'))
        {
            character = ' ';
        }
        destination[index] = character;
    }
    destination[index] = '\0';
}

static void* LuaAllocate(void* userdata, void* pointer, size_t old_size,
    size_t new_size)
{
    LuaMemoryTracker* tracker = (LuaMemoryTracker*)userdata;
    void* result;

    if (new_size == 0)
    {
        if (pointer)
        {
            tracker->bytes = tracker->bytes >= old_size ?
                tracker->bytes - (unsigned long)old_size : 0;
            if (tracker->blocks)
            {
                --tracker->blocks;
            }
        }
        free(pointer);
        return 0;
    }
    result = realloc(pointer, new_size);
    if (!result)
    {
        return 0;
    }
    if (!pointer)
    {
        ++tracker->blocks;
    }
    if (new_size >= old_size)
    {
        tracker->bytes += (unsigned long)(new_size - old_size);
    }
    else
    {
        tracker->bytes -= (unsigned long)(old_size - new_size);
    }
    if (tracker->bytes > tracker->peak_bytes)
    {
        tracker->peak_bytes = tracker->bytes;
    }
    return result;
}

static int RunLua(const char* source, const char* name, bool file_source,
    int* result_value, char* error, int error_capacity,
    LuaMemoryTracker* tracker)
{
    lua_State* state;
    int status;
    int is_number;
    lua_Number numeric_result;

    *result_value = -1;
    error[0] = '\0';
    state = lua_newstate(LuaAllocate, tracker);
    if (!state)
    {
        CopyLuaError(error, error_capacity, "STATE ALLOCATION FAILED");
        return LUA_ERRMEM;
    }
    luaL_openlibs(state);
    status = file_source ? luaL_loadfile(state, source) :
        luaL_loadbuffer(state, source, strlen(source), name);
    if (status == LUA_OK)
    {
        status = lua_pcall(state, 0, 1, 0);
    }
    if (status == LUA_OK)
    {
        numeric_result = lua_tonumberx(state, -1, &is_number);
        *result_value = fix32_to_int((fix32_t)numeric_result);
        if (!is_number)
        {
            status = LUA_ERRRUN;
            CopyLuaError(error, error_capacity, "RESULT IS NOT A NUMBER");
        }
    }
    else
    {
        CopyLuaError(error, error_capacity, lua_tostring(state, -1));
    }
    lua_close(state);
    return status;
}

static void RecordLuaMemory(const LuaMemoryTracker* tracker)
{
    if (tracker->peak_bytes > s_lua_peak_bytes)
    {
        s_lua_peak_bytes = tracker->peak_bytes;
    }
    if (tracker->bytes > s_lua_retained_bytes)
    {
        s_lua_retained_bytes = tracker->bytes;
    }
    if (tracker->blocks > s_lua_retained_blocks)
    {
        s_lua_retained_blocks = tracker->blocks;
    }
}

static void RunLuaDiagnostic(void)
{
    LuaMemoryTracker tracker;
    char* base_path;
    char* script_path;
    char cycle_error[26];
    int result;
    int cycle;

    s_lua_embedded_status = LUA_ERRRUN;
    s_lua_file_status = LUA_ERRFILE;
    s_lua_syntax_status = LUA_ERRRUN;
    s_lua_runtime_status = LUA_ERRRUN;
    s_lua_embedded_result = -1;
    s_lua_file_result = -1;
    s_lua_cycles = 0;
    s_lua_retained_bytes = 0;
    s_lua_retained_blocks = 0;
    s_lua_peak_bytes = 0;
    s_lua_cycles_clean = true;
    s_lua_ready = false;
    s_lua_embedded_error[0] = '\0';
    s_lua_file_error[0] = '\0';
    s_lua_syntax_error[0] = '\0';
    s_lua_runtime_error[0] = '\0';

    memset(&tracker, 0, sizeof(tracker));
    s_lua_embedded_status = RunLua("return 6 * 7", "EMBEDDED", false,
        &s_lua_embedded_result, s_lua_embedded_error,
        sizeof(s_lua_embedded_error), &tracker);
    RecordLuaMemory(&tracker);

    base_path = SDL_GetBasePath();
    script_path = 0;
    if (!base_path || SDL_asprintf(&script_path, "%slua_diag.lua", base_path) < 0 ||
        !script_path)
    {
        CopyLuaError(s_lua_file_error, sizeof(s_lua_file_error), "SCRIPT PATH");
    }
    else
    {
        memset(&tracker, 0, sizeof(tracker));
        s_lua_file_status = RunLua(script_path, "LUA FILE", true,
            &s_lua_file_result, s_lua_file_error,
            sizeof(s_lua_file_error), &tracker);
        RecordLuaMemory(&tracker);
    }

    memset(&tracker, 0, sizeof(tracker));
    s_lua_syntax_status = RunLua("return +", "SYNTAX", false, &result,
        s_lua_syntax_error, sizeof(s_lua_syntax_error), &tracker);
    RecordLuaMemory(&tracker);

    memset(&tracker, 0, sizeof(tracker));
    s_lua_runtime_status = RunLua("error('RUNTIME CHECK')", "RUNTIME", false,
        &result, s_lua_runtime_error, sizeof(s_lua_runtime_error), &tracker);
    RecordLuaMemory(&tracker);

    for (cycle = 0; cycle < 4; ++cycle)
    {
        memset(&tracker, 0, sizeof(tracker));
        if (RunLua("return 6 * 7", "CYCLE", false, &result,
            cycle_error, sizeof(cycle_error), &tracker) != LUA_OK ||
            result != 42 || tracker.bytes || tracker.blocks)
        {
            s_lua_cycles_clean = false;
        }
        RecordLuaMemory(&tracker);
        ++s_lua_cycles;
        if (script_path)
        {
            memset(&tracker, 0, sizeof(tracker));
            if (RunLua(script_path, "CYCLE FILE", true, &result,
                cycle_error, sizeof(cycle_error), &tracker) != LUA_OK ||
                result != 42 || tracker.bytes || tracker.blocks)
            {
                s_lua_cycles_clean = false;
            }
            RecordLuaMemory(&tracker);
            ++s_lua_cycles;
        }
    }

    SDL_free(script_path);
    SDL_free(base_path);
    s_lua_ready = s_lua_embedded_status == LUA_OK &&
        s_lua_embedded_result == 42 && s_lua_file_status == LUA_OK &&
        s_lua_file_result == 42 && s_lua_syntax_status == LUA_ERRSYNTAX &&
        s_lua_runtime_status == LUA_ERRRUN && s_lua_cycles_clean &&
        !s_lua_retained_bytes && !s_lua_retained_blocks;
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

static void PollTouches(SDL_Renderer* renderer)
{
    ZDK_INPUT_STATE input;
    int index;

    memset(&input, 0, sizeof(input));
    ZDKInput_GetState(&input);
    SDL_ZuneTouchBeginFrame();
    s_margin_touches = 0;
    s_active_buttons = 0;
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
    case '.': return "000000000000010";
    case '-': return "000000111000000";
    case '\\': return "100010001000100";
    case '/': return "001001010100100";
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
    DrawText(pixels, 3, 3, "LUA DIAG", 1, 0x60, 0xf0, 0xff);
    DrawText(pixels, 124, 3, s_lua_ready ? "PASS" : "FAIL", 1,
        s_lua_ready ? 0x60 : 0xff, s_lua_ready ? 0xff : 0x80, 0x80);
    _snprintf(line, sizeof(line) - 1, "EMB %d %s", s_lua_embedded_result,
        s_lua_embedded_status == LUA_OK ? "OK" : s_lua_embedded_error);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 11, line, 1, 0xd0, 0xd0, 0x80);
    _snprintf(line, sizeof(line) - 1, "FILE %d %s", s_lua_file_result,
        s_lua_file_status == LUA_OK ? "OK" : s_lua_file_error);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 19, line, 1, 0xff, 0xff, 0xff);
    _snprintf(line, sizeof(line) - 1, "SYN %d %s", s_lua_syntax_status,
        s_lua_syntax_status == LUA_ERRSYNTAX ? s_lua_syntax_error :
        "NO ERROR");
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 27, line, 1, 0xa0, 0xd0, 0xff);
    _snprintf(line, sizeof(line) - 1, "RUN %d %s", s_lua_runtime_status,
        s_lua_runtime_status == LUA_ERRRUN ? s_lua_runtime_error :
        "NO ERROR");
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 35, line, 1, 0xff, 0x80, 0x80);
    _snprintf(line, sizeof(line) - 1, "LOOP %u MEM %u", s_lua_cycles,
        (unsigned int)s_lua_retained_bytes);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 43, line, 1, 0xa0, 0xd0, 0xff);
    _snprintf(line, sizeof(line) - 1, "ALLOC %u PEAK %u",
        (unsigned int)s_lua_retained_blocks,
        (unsigned int)s_lua_peak_bytes);
    line[sizeof(line) - 1] = '\0';
    DrawText(pixels, 3, 51, line, 1, 0xa0, 0xd0, 0xff);
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
        DrawText(pixels, 3, 59 + index * 7, line, 1, 0xff,
            touch->inside_viewport ? 0xff : 0x80, 0x80);
    }
    DrawText(pixels, 3, 202, "PAD L R U D O X", 1, 0xa0, 0xd0, 0xff);
    DrawControl(pixels, 7, 234, 22, 14, (s_active_buttons & 1) != 0, "L");
    DrawControl(pixels, 43, 234, 22, 14, (s_active_buttons & 2) != 0, "R");
    DrawControl(pixels, 29, 212, 14, 22, (s_active_buttons & 4) != 0, "U");
    DrawControl(pixels, 29, 248, 14, 22, (s_active_buttons & 8) != 0, "D");
    DrawControl(pixels, 92, 235, 28, 28, (s_active_buttons & 16) != 0, "O");
    DrawControl(pixels, 122, 223, 28, 28, (s_active_buttons & 32) != 0, "X");
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
    s_working_directory_ready = SDL_ZuneSetWorkingDirectoryFromModule();
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
    ProbePackagedAssets();
    RunLuaDiagnostic();
    return true;
}

bool handle_events(SDL_Renderer* renderer, SDL_Event* event)
{
    if (!renderer || !event)
    {
        return true;
    }
    if (event->type == SDL_EVENT_FINGER_DOWN)
    {
        ++s_down_events;
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
