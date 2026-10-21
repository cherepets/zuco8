#ifndef QUICK_RESUME_H
#define QUICK_RESUME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../z8lua/fix32.h"
#include "../../z8lua/lua.h"

#define QUICK_RESUME_CARTDATA_ID_MAX 64

typedef struct
{
    char cartdata_id[QUICK_RESUME_CARTDATA_ID_MAX + 1];

} quick_resume_info_t;

typedef bool (*quick_resume_write_fn)(void* userdata, const void* data, size_t size);

uint32_t quick_resume_crc32(uint32_t crc, const void* data, size_t size);

bool quick_resume_register_permanents(lua_State* L);

bool quick_resume_save(lua_State* L, int extra, const quick_resume_info_t* info,
    quick_resume_write_fn write, void* userdata);

bool quick_resume_load(lua_State* L, const uint8_t* data, size_t size,
    quick_resume_info_t* info);

typedef struct
{
    fix32_t seed[2];
    uint32_t ticks;
    uint8_t held_frames[12];

} api_runtime_state_t;

void api_get_runtime_state(api_runtime_state_t* runtime);
void api_set_runtime_state(const api_runtime_state_t* runtime);

#endif
