#include "SDL3/SDL.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

#include "../../core.h"

#define FRAGMENT_SHADER_LOCATION "\\gametitle\\584E07D1\\Content\\fragment.nvbf"
#define VERTEX_SHADER_LOCATION   "\\gametitle\\584E07D1\\Content\\vertex.nvbv"

enum
{
    VERTEX_FLOATS = 6,
    MAX_DIAGNOSTIC_VERTICES = 12288
};

static GLuint s_program;
static GLint s_rotation_uniform;
static GLfloat s_vertices[MAX_DIAGNOSTIC_VERTICES * VERTEX_FLOATS];
static int s_vertex_count;
static bool s_graphics_initialized;
static bool s_shutdown_requested;
static DWORD s_shutdown_started;
static DWORD s_start_tick;

static void LogStage(const char* stage)
{
    SDL_Log("zuco8: %s", stage);
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

static bool LoadBinaryShader(GLuint shader, const char* path)
{
    FILE* file = fopen(path, "rb");
    long length;
    char* bytes;

    if (!file)
    {
        return false;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (length <= 0)
    {
        fclose(file);
        return false;
    }

    bytes = (char*)malloc((size_t)length);
    if (!bytes)
    {
        fclose(file);
        return false;
    }

    if (fread(bytes, 1, (size_t)length, file) != (size_t)length)
    {
        free(bytes);
        fclose(file);
        return false;
    }

    glShaderBinary(1, &shader, GL_NVIDIA_PLATFORM_BINARY_NV, bytes, length);
    free(bytes);
    fclose(file);
    return true;
}

static bool InitDiagnosticGraphics(void)
{
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    GLint linked = 0;

    s_program = glCreateProgram();
    glAttachShader(s_program, vertex_shader);
    glAttachShader(s_program, fragment_shader);
    if (!LoadBinaryShader(vertex_shader, VERTEX_SHADER_LOCATION) ||
        !LoadBinaryShader(fragment_shader, FRAGMENT_SHADER_LOCATION))
    {
        return false;
    }

    glBindAttribLocation(s_program, 0, "pos_attr");
    glBindAttribLocation(s_program, 1, "col_attr");
    glLinkProgram(s_program);
    glGetProgramiv(s_program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        return false;
    }

    glUseProgram(s_program);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    s_rotation_uniform = glGetUniformLocation(s_program, "rot");
    return true;
}

// TEMP: 3x5 bitmap font
static const char* Glyph(char character)
{
    switch (character)
    {
    case 'A': return "010101111101101";
    case 'B': return "110101110101110";
    case 'C': return "011100100100011";
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
    case 'N': return "101111111101101";
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
    case '9': return "010101011001110";
    case ':': return "000010000010000";
    default: return 0;
    }
}

static void AddVertex(float x, float y, float r, float g, float b)
{
    GLfloat* vertex;

    if (s_vertex_count >= MAX_DIAGNOSTIC_VERTICES)
    {
        return;
    }

    vertex = &s_vertices[s_vertex_count * VERTEX_FLOATS];
    vertex[0] = x;
    vertex[1] = y;
    vertex[2] = r;
    vertex[3] = g;
    vertex[4] = b;
    vertex[5] = 1.0f;
    ++s_vertex_count;
}

static void AddCell(float x, float y, float size, float r, float g, float b)
{
    const float left = -0.94f + x * 0.012f;
    const float right = left + size * 0.012f;
    const float top = 0.92f - y * 0.020f;
    const float bottom = top - size * 0.020f;

    AddVertex(left, top, r, g, b);
    AddVertex(right, top, r, g, b);
    AddVertex(left, bottom, r, g, b);
    AddVertex(right, top, r, g, b);
    AddVertex(right, bottom, r, g, b);
    AddVertex(left, bottom, r, g, b);
}

static void DrawText(float x, float y, float size, const char* text, float r,
    float g, float b)
{
    const char* cursor = text;

    while (*cursor)
    {
        const char* glyph = Glyph(*cursor);
        int row;

        if (glyph)
        {
            for (row = 0; row < 5; ++row)
            {
                int column;

                for (column = 0; column < 3; ++column)
                {
                    if (glyph[row * 3 + column] == '1')
                    {
                        AddCell(x + column * size, y + row * size, size, r,
                            g, b);
                    }
                }
            }
        }

        x += 4.0f * size;
        ++cursor;
    }
}

static void DrawDiagnosticFrame(void)
{
    char tick_text[20];
    DWORD now = GetTickCount();
    float pulse = (float)(sin((now - s_start_tick) * 0.004) * 0.5 + 0.5);

    glClearColor(0.02f + 0.04f * pulse, 0.04f,
        0.08f + 0.10f * pulse, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    s_vertex_count = 0;
    DrawText(10.0f, 5.0f, 2.0f, "HELLO", 0.2f, 0.9f, 1.0f);
    DrawText(10.0f, 20.0f, 2.0f, "FROM", 0.2f, 1.0f, 0.4f);
    DrawText(10.0f, 35.0f, 1.8f, "SEATTLE", 1.0f, 0.9f, 0.2f);
    _snprintf(tick_text, sizeof(tick_text) - 1, "TICK %08lu",
        (unsigned long)(now - s_start_tick));
    tick_text[sizeof(tick_text) - 1] = '\0';
    DrawText(10.0f, 50.0f, 2.0f, tick_text, 1.0f, 1.0f, 1.0f);
    DrawText(10.0f, 65.0f, 1.8f,
        s_shutdown_requested ? "QUIT CLEANUP" : "TOUCH EXIT", 1.0f, 0.5f,
        0.3f);
    glUniform2f(s_rotation_uniform, 1.0f, 0.0f);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
        VERTEX_FLOATS * sizeof(GLfloat), s_vertices);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
        VERTEX_FLOATS * sizeof(GLfloat), s_vertices + 2);
    glDrawArrays(GL_TRIANGLES, 0, s_vertex_count);
}

void handle_resize(SDL_Renderer* renderer)
{
    (void)renderer;
}

bool init_core(SDL_Renderer* renderer)
{
    ZDK_INPUT_STATE input;

    (void)renderer;
    SuppressReboot();
    LogStage("reboot suppression configured");
    ZDKSystem_ShowSplashScreen(false);
    SystemIdleTimerReset();
    LogStage("splash hidden and user activity signalled");
    ZDKGL_Initialize();
    s_graphics_initialized = true;
    LogStage("GLES2 initialized");
    if (!InitDiagnosticGraphics())
    {
        LogStage("FAILURE loading diagnostic shaders");
        return false;
    }

    ZDKInput_GetState(&input);
    s_start_tick = GetTickCount();
    LogStage("input polling and callback iteration ready");
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

    (void)renderer;
    ZDKInput_GetState(&input);
    now = GetTickCount();
    if (input.TouchState.Count > 0 && !s_shutdown_requested)
    {
        s_shutdown_requested = true;
        s_shutdown_started = now;
        SystemIdleTimerReset();
        LogStage("shutdown requested by touch");
    }

    ZDKGL_BeginDraw();
    DrawDiagnosticFrame();
    ZDKGL_EndDraw();
    return !s_shutdown_requested || now - s_shutdown_started < 350;
}

void destroy_core(void)
{
    if (s_graphics_initialized)
    {
        if (s_program)
        {
            glDeleteProgram(s_program);
        }

        ZDKGL_Cleanup();
        s_program = 0;
        s_graphics_initialized = false;
    }

    LogStage("GLES2 cleanup complete");
}
