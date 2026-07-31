#include "png_texture.h"

#include <stdio.h>
#include <stdlib.h>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../misc/stb_image.h"

static unsigned char* ReadFileBytes(const char* file_name, long* size)
{
    FILE* file;
    unsigned char* data;
    long length;

    *size = 0;
    file = fopen(file_name, "rb");
    if (!file)
    {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }
    data = (unsigned char*)malloc(length);
    if (!data || fread(data, 1, length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return 0;
    }
    fclose(file);
    *size = length;
    return data;
}

bool LoadPngTexture(SDL_Renderer* renderer, const char* file_name,
    int expected_width, int expected_height, SDL_Texture** texture)
{
    char* base_path;
    char* path;
    unsigned char* file_data;
    unsigned char* pixels;
    long file_size;
    int width;
    int height;
    SDL_Texture* result;

    if (!renderer || !file_name || !texture)
    {
        return false;
    }
    *texture = 0;
    base_path = SDL_GetBasePath();
    path = 0;
    if (!base_path || SDL_asprintf(&path, "%s%s", base_path, file_name) < 0 ||
        !path)
    {
        SDL_free(base_path);
        return false;
    }
    file_data = ReadFileBytes(path, &file_size);
    SDL_free(path);
    SDL_free(base_path);
    if (!file_data)
    {
        return false;
    }
    pixels = stbi_load_from_memory(file_data, (int)file_size, &width, &height,
        0, 4);
    free(file_data);
    if (!pixels || width != expected_width || height != expected_height)
    {
        stbi_image_free(pixels);
        return false;
    }
    result = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, width, height);
    if (!result || !SDL_SetTextureScaleMode(result, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND) ||
        !SDL_UpdateTexture(result, 0, pixels, width * 4))
    {
        SDL_DestroyTexture(result);
        stbi_image_free(pixels);
        return false;
    }
    stbi_image_free(pixels);
    *texture = result;
    return true;
}
