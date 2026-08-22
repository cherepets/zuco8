#include "renderer_gles2.h"
#include "SDL3/SDL_zune_ext.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define FRAGMENT_SHADER_LOCATION "\\gametitle\\584E07D1\\Content\\fragment.nvbf"
#define VERTEX_SHADER_LOCATION   "\\gametitle\\584E07D1\\Content\\vertex.nvbv"

enum
{
    FLOATS_PER_VERTEX = 4,
    VERTICES_PER_QUAD = 6,
    MAX_QUADS = 128
};

static GLuint s_program;
static GLuint s_vbo;
static GLint s_texture_uniform;
static GLint s_rotation_uniform;
static GLint s_rotation_pivot_uniform;
static GLfloat s_vertices[MAX_QUADS * VERTICES_PER_QUAD * FLOATS_PER_VERTEX];
static int s_vertex_count;
static SDL_Texture* s_current_texture;
static int s_rotation_quarter_turns;
static float s_rotation_pivot_x;
static float s_rotation_pivot_y;
static GLfloat s_rotation[3] = { 1.0f, 0.0f, 0.0f };
static GLfloat s_pivot_ndc[2];

static void UpdateRotationUniforms(const SDL_Renderer* renderer)
{
    static const float k_cos[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
    static const float k_sin[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
    int index = s_rotation_quarter_turns % 4;

    if (index < 0)
    {
        index += 4;
    }
    s_rotation[0] = k_cos[index];
    s_rotation[1] = k_sin[index] * renderer->logical_height /
        renderer->logical_width;
    s_rotation[2] = k_sin[index] * renderer->logical_width /
        renderer->logical_height;
    s_pivot_ndc[0] = s_rotation_pivot_x /
        (renderer->logical_width * 0.5f) - 1.0f;
    s_pivot_ndc[1] = 1.0f - s_rotation_pivot_y /
        (renderer->logical_height * 0.5f);
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

bool renderer_gles2_initialize(SDL_Renderer* renderer)
{
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLint linked = 0;

    if (!renderer)
    {
        return false;
    }

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    s_program = glCreateProgram();
    glAttachShader(s_program, vertex_shader);
    glAttachShader(s_program, fragment_shader);
    if (!LoadBinaryShader(vertex_shader, VERTEX_SHADER_LOCATION) ||
        !LoadBinaryShader(fragment_shader, FRAGMENT_SHADER_LOCATION))
    {
        return false;
    }

    glBindAttribLocation(s_program, 0, "pos_attr");
    glBindAttribLocation(s_program, 1, "uv_attr");
    glLinkProgram(s_program);
    glGetProgramiv(s_program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        return false;
    }

    glUseProgram(s_program);
    s_texture_uniform = glGetUniformLocation(s_program, "texture_sampler");
    s_rotation_uniform = glGetUniformLocation(s_program, "rotation_transform");
    s_rotation_pivot_uniform = glGetUniformLocation(s_program,
        "rotation_pivot");
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_vertices), 0, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
        FLOATS_PER_VERTEX * sizeof(GLfloat), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
        FLOATS_PER_VERTEX * sizeof(GLfloat), (const void*)(2 * sizeof(GLfloat)));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderer->output_width = GetSystemMetrics(SM_CXSCREEN);
    renderer->output_height = GetSystemMetrics(SM_CYSCREEN);
    if (renderer->output_width <= 0 || renderer->output_height <= 0)
    {
        return false;
    }
    renderer->initialized = true;
    s_rotation_quarter_turns = 0;
    s_rotation_pivot_x = 0.0f;
    s_rotation_pivot_y = 0.0f;
    renderer_gles2_set_viewport(renderer, renderer->output_width,
        renderer->output_height);
    return glGetError() == GL_NO_ERROR;
}

void renderer_gles2_shutdown(SDL_Renderer* renderer)
{
    if (s_vbo)
    {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }
    if (s_program)
    {
        glDeleteProgram(s_program);
        s_program = 0;
    }
    if (renderer)
    {
        renderer->initialized = false;
    }
}

void renderer_gles2_set_viewport(SDL_Renderer* renderer, int width, int height)
{
    float scale;
    int viewport_width;
    int viewport_height;

    if (!renderer)
    {
        return;
    }

    if (width <= 0 || height <= 0 || renderer->output_width <= 0 ||
        renderer->output_height <= 0)
    {
        return;
    }
    renderer->logical_width = width;
    renderer->logical_height = height;
    scale = renderer->output_width / (float)width;
    if (height * scale > renderer->output_height)
    {
        scale = renderer->output_height / (float)height;
    }
    viewport_width = (int)(width * scale);
    viewport_height = (int)(height * scale);
    renderer->viewport_x = (renderer->output_width - viewport_width) / 2;
    renderer->viewport_y = (renderer->output_height - viewport_height) / 2;
    renderer->viewport_width = viewport_width;
    renderer->viewport_height = viewport_height;
    glViewport(renderer->viewport_x, renderer->viewport_y, viewport_width,
        viewport_height);
    UpdateRotationUniforms(renderer);
}

bool renderer_gles2_map_touch(const SDL_Renderer* renderer, float physical_x,
    float physical_y, float* logical_x, float* logical_y)
{
    if (!renderer || !logical_x || !logical_y || renderer->viewport_width <= 0 ||
        renderer->viewport_height <= 0 || physical_x < renderer->viewport_x ||
        physical_y < renderer->viewport_y ||
        physical_x >= renderer->viewport_x + renderer->viewport_width ||
        physical_y >= renderer->viewport_y + renderer->viewport_height)
    {
        return false;
    }

    *logical_x = (physical_x - renderer->viewport_x) *
        renderer->logical_width / renderer->viewport_width;
    *logical_y = (physical_y - renderer->viewport_y) *
        renderer->logical_height / renderer->viewport_height;
    return true;
}

void renderer_gles2_set_rotation(SDL_Renderer* renderer, int quarter_turns,
    float pivot_x, float pivot_y)
{
    if (!renderer || !renderer->initialized)
    {
        return;
    }
    if (s_rotation_quarter_turns == quarter_turns &&
        s_rotation_pivot_x == pivot_x &&
        s_rotation_pivot_y == pivot_y)
    {
        return;
    }
    SDL_RenderPresent(renderer);
    s_rotation_quarter_turns = quarter_turns;
    s_rotation_pivot_x = pivot_x;
    s_rotation_pivot_y = pivot_y;
    UpdateRotationUniforms(renderer);
}

static void AddVertex(float x, float y, float u, float v)
{
    GLfloat* vertex;
    if (s_vertex_count >= MAX_QUADS * VERTICES_PER_QUAD)
    {
        return;
    }
    vertex = &s_vertices[s_vertex_count * FLOATS_PER_VERTEX];
    vertex[0] = x;
    vertex[1] = y;
    vertex[2] = u;
    vertex[3] = v;
    ++s_vertex_count;
}

bool SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture,
    const SDL_FRect* source, const SDL_FRect* destination)
{
    SDL_FRect full_source;
    SDL_FRect full_destination;
    float left;
    float right;
    float top;
    float bottom;
    float u0;
    float u1;
    float v0;
    float v1;

    if (!renderer || !renderer->initialized || !texture)
    {
        return false;
    }

    full_source.x = 0.0f;
    full_source.y = 0.0f;
    full_source.w = (float)texture->width;
    full_source.h = (float)texture->height;
    full_destination = full_source;
    if (!source)
    {
        source = &full_source;
    }
    if (!destination)
    {
        destination = &full_destination;
    }

    left = destination->x / (renderer->logical_width * 0.5f) - 1.0f;
    right = (destination->x + destination->w) /
        (renderer->logical_width * 0.5f) - 1.0f;
    top = 1.0f - destination->y / (renderer->logical_height * 0.5f);
    bottom = 1.0f - (destination->y + destination->h) /
        (renderer->logical_height * 0.5f);
    u0 = source->x / texture->width;
    u1 = (source->x + source->w) / texture->width;
    v0 = source->y / texture->height;
    v1 = (source->y + source->h) / texture->height;
    if (s_current_texture && s_current_texture != texture)
    {
        if (!SDL_RenderPresent(renderer))
        {
            return false;
        }
    }
    s_current_texture = texture;
    AddVertex(left, top, u0, v0);
    AddVertex(right, top, u1, v0);
    AddVertex(left, bottom, u0, v1);
    AddVertex(right, top, u1, v0);
    AddVertex(right, bottom, u1, v1);
    AddVertex(left, bottom, u0, v1);
    return true;
}

bool SDL_RenderPresent(SDL_Renderer* renderer)
{
    GLenum error;
    if (s_vertex_count == 0)
    {
        return true;
    }
    glUseProgram(s_program);
    glUniform1i(s_texture_uniform, 0);
    glUniform3fv(s_rotation_uniform, 1, s_rotation);
    glUniform2fv(s_rotation_pivot_uniform, 1, s_pivot_ndc);
    glBindTexture(GL_TEXTURE_2D, s_current_texture->handle);
    if (s_current_texture->blend_mode == SDL_BLENDMODE_BLEND)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    renderer->diagnostic_blend_mode = s_current_texture->blend_mode;
    renderer->diagnostic_blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
        s_vertex_count * FLOATS_PER_VERTEX * sizeof(GLfloat), s_vertices);
    glDrawArrays(GL_TRIANGLES, 0, s_vertex_count);
    s_vertex_count = 0;
    s_current_texture = 0;
    error = glGetError();
    renderer->diagnostic_gl_error = error;
    SDL_ZuneTouchPollReset();
    return error == GL_NO_ERROR;
}

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format,
    int access, int width, int height)
{
    SDL_Texture* texture;

    if (!renderer || !renderer->initialized || format != SDL_PIXELFORMAT_RGBA32 ||
        width <= 0 || height <= 0)
    {
        return 0;
    }

    texture = (SDL_Texture*)calloc(1, sizeof(SDL_Texture));
    if (!texture)
    {
        return 0;
    }
    texture->pixels = (unsigned char*)calloc(width * height, 4);
    if (!texture->pixels)
    {
        free(texture);
        return 0;
    }
    texture->width = width;
    texture->height = height;
    texture->access = access;
    texture->blend_mode = SDL_BLENDMODE_NONE;
    glGenTextures(1, &texture->handle);
    glBindTexture(GL_TEXTURE_2D, texture->handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, texture->pixels);
    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &texture->handle);
        free(texture->pixels);
        free(texture);
        return 0;
    }
    return texture;
}

void SDL_DestroyTexture(SDL_Texture* texture)
{
    if (!texture)
    {
        return;
    }
    if (texture->handle)
    {
        glDeleteTextures(1, &texture->handle);
    }
    free(texture->pixels);
    free(texture);
}

bool SDL_SetTextureScaleMode(SDL_Texture* texture, int scale_mode)
{
    (void)scale_mode;
    return texture != 0;
}

bool SDL_SetTextureBlendMode(SDL_Texture* texture, int blend_mode)
{
    if (!texture)
    {
        return false;
    }
    texture->blend_mode = blend_mode;
    return true;
}

bool SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rectangle,
    const void* pixels, int pitch)
{
    SDL_Rect full;
    int row;

    if (!texture || !pixels || pitch <= 0)
    {
        return false;
    }
    full.x = 0;
    full.y = 0;
    full.w = texture->width;
    full.h = texture->height;
    if (!rectangle)
    {
        rectangle = &full;
    }
    if (rectangle->x < 0 || rectangle->y < 0 ||
        rectangle->x + rectangle->w > texture->width ||
        rectangle->y + rectangle->h > texture->height)
    {
        return false;
    }
    for (row = 0; row < rectangle->h; ++row)
    {
        memcpy(texture->pixels + ((rectangle->y + row) * texture->width +
            rectangle->x) * 4, (const unsigned char*)pixels + row * pitch,
            rectangle->w * 4);
    }

    glBindTexture(GL_TEXTURE_2D, texture->handle);
    if (rectangle->x == 0 && rectangle->y == 0 &&
        rectangle->w == texture->width && rectangle->h == texture->height)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width,
            texture->height, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);
    }
    else for (row = 0; row < rectangle->h; ++row)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, rectangle->x, rectangle->y + row,
            rectangle->w, 1, GL_RGBA, GL_UNSIGNED_BYTE,
            texture->pixels + ((rectangle->y + row) * texture->width +
            rectangle->x) * 4);
    }
    return glGetError() == GL_NO_ERROR;
}

bool SDL_LockTexture(SDL_Texture* texture, const SDL_Rect* rectangle,
    void** pixels, int* pitch)
{
    (void)rectangle;
    if (!texture || texture->access != SDL_TEXTUREACCESS_STREAMING ||
        texture->locked || !pixels || !pitch)
    {
        return false;
    }
    texture->locked = true;
    *pixels = texture->pixels;
    *pitch = texture->width * 4;
    return true;
}

void SDL_UnlockTexture(SDL_Texture* texture)
{
    if (!texture || !texture->locked)
    {
        return;
    }
    texture->locked = false;
    glBindTexture(GL_TEXTURE_2D, texture->handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height,
        GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);
}

bool SDL_SetRenderDrawColor(SDL_Renderer* renderer, unsigned char red,
    unsigned char green, unsigned char blue, unsigned char alpha)
{
    if (!renderer)
    {
        return false;
    }
    renderer->clear_red = red;
    renderer->clear_green = green;
    renderer->clear_blue = blue;
    renderer->clear_alpha = alpha;
    return true;
}

bool SDL_RenderClear(SDL_Renderer* renderer)
{
    if (!renderer || !renderer->initialized)
    {
        return false;
    }
    s_vertex_count = 0;
    s_current_texture = 0;
    glClearColor(renderer->clear_red / 255.0f, renderer->clear_green / 255.0f,
        renderer->clear_blue / 255.0f, renderer->clear_alpha / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return glGetError() == GL_NO_ERROR;
}
