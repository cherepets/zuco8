#include <string.h>

#include "../../z8lua/lua.h"
#include "../../z8lua/lauxlib.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include "../../eris/eris.h"
#ifdef __cplusplus
}
#endif
#include "../../memory.h"
#include "quick_resume.h"

#define QR_HEADER_SIZE 88
#define QR_TRAILER_SIZE 4
#define QR_MAX_RECURSION 1000
#define QR_NAME_MAX 160

#define QR_OFFSET_SEED 0
#define QR_OFFSET_TICKS 8
#define QR_OFFSET_HELD_FRAMES 12
#define QR_OFFSET_CARTDATA_ID 24

static char qr_forward_key;
static char qr_inverse_key;

static const uint32_t qr_crc_table[16] =
{
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

typedef struct
{
    quick_resume_write_fn write;
    void* userdata;
    uint32_t crc;
    bool failed;

} qr_writer_t;

typedef struct
{
    const uint8_t* data;
    size_t size;
    bool done;

} qr_reader_t;

uint32_t quick_resume_crc32(uint32_t crc, const void* data, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)data;

    crc = ~crc;
    while (size--)
    {
        crc ^= *bytes++;
        crc = (crc >> 4) ^ qr_crc_table[crc & 0x0f];
        crc = (crc >> 4) ^ qr_crc_table[crc & 0x0f];
    }
    return ~crc;
}

static void put_u32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void join_name(char* name, const char* prefix, const char* separator,
    const char* key)
{
    size_t used = 0;
    const char* parts[3];
    int i;

    parts[0] = prefix;
    parts[1] = separator;
    parts[2] = key;
    for (i = 0; i < 3; i++)
    {
        const char* part = parts[i];
        while (*part && used + 1 < QR_NAME_MAX)
        {
            name[used++] = *part++;
        }
    }
    name[used] = '\0';
}

static void add_permanent(lua_State* L, int forward, int inverse, const char* name)
{
    lua_pushvalue(L, -1);
    lua_pushstring(L, name);
    lua_rawset(L, forward);
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    lua_rawset(L, inverse);
}

static void add_iterator_permanent(lua_State* L, int forward, int inverse,
    bool with_table, const char* name)
{
    lua_getglobal(L, "all");
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return;
    }
    if (with_table)
    {
        lua_newtable(L);
    }
    else
    {
        lua_pushnil(L);
    }
    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_iscfunction(L, -1))
    {
        lua_pushcfunction(L, lua_tocfunction(L, -1));
        add_permanent(L, forward, inverse, name);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

static int register_permanents(lua_State* L)
{
    const int forward = 1;
    const int inverse = 2;
    const int globals = 3;
    char name[QR_NAME_MAX];

    lua_settop(L, 0);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushglobaltable(L);

    lua_pushnil(L);
    while (lua_next(L, globals))
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            const char* key = lua_tostring(L, -2);

            if (lua_iscfunction(L, -1))
            {
                add_permanent(L, forward, inverse, key);
            }
            else if (lua_istable(L, -1) && !lua_rawequal(L, -1, globals))
            {
                join_name(name, "lib", ":", key);
                add_permanent(L, forward, inverse, name);

                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    if (lua_type(L, -2) == LUA_TSTRING && lua_iscfunction(L, -1))
                    {
                        join_name(name, key, ".", lua_tostring(L, -2));
                        add_permanent(L, forward, inverse, name);
                    }
                    lua_pop(L, 1);
                }
            }
        }
        lua_pop(L, 1);
    }

    add_iterator_permanent(L, forward, inverse, true, "all:iter");
    add_iterator_permanent(L, forward, inverse, false, "all:nil");

    lua_pushlightuserdata(L, &qr_forward_key);
    lua_pushvalue(L, forward);
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, &qr_inverse_key);
    lua_pushvalue(L, inverse);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushboolean(L, 0);
    eris_set_setting(L, "debug", -1);
    lua_pop(L, 1);
    lua_pushnumber(L, (lua_Number)QR_MAX_RECURSION);
    eris_set_setting(L, "maxrec", -1);
    lua_pop(L, 1);
    return 0;
}

bool quick_resume_register_permanents(lua_State* L)
{
    int top;
    int status;

    if (!L)
    {
        return false;
    }
    top = lua_gettop(L);
    lua_pushcfunction(L, register_permanents);
    status = lua_pcall(L, 0, 0, 0);
    lua_settop(L, top);
    return status == LUA_OK;
}

static bool emit(qr_writer_t* writer, const void* data, size_t size)
{
    if (writer->failed)
    {
        return false;
    }
    if (size && !writer->write(writer->userdata, data, size))
    {
        writer->failed = true;
        return false;
    }
    writer->crc = quick_resume_crc32(writer->crc, data, size);
    return true;
}

static int snapshot_writer(lua_State* L, const void* data, size_t size, void* userdata)
{
    (void)L;
    return emit((qr_writer_t*)userdata, data, size) ? 0 : 1;
}

static int save_protected(lua_State* L)
{
    qr_writer_t* writer = (qr_writer_t*)lua_touserdata(L, 1);

    lua_createtable(L, 0, 2);
    lua_pushglobaltable(L);
    lua_setfield(L, -2, "g");
    lua_pushvalue(L, 2);
    lua_setfield(L, -2, "x");
    lua_replace(L, 2);
    lua_pushlightuserdata(L, &qr_forward_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (!lua_istable(L, -1))
    {
        return luaL_error(L, "quick resume permanents are not registered");
    }
    lua_replace(L, 1);
    lua_settop(L, 2);
    eris_dump(L, snapshot_writer, writer);
    return 0;
}

static void build_header(uint8_t* header, const quick_resume_info_t* info)
{
    api_runtime_state_t runtime;
    size_t id_length;

    memset(header, 0, QR_HEADER_SIZE);
    api_get_runtime_state(&runtime);

    put_u32(header + QR_OFFSET_SEED, (uint32_t)runtime.seed[0]);
    put_u32(header + QR_OFFSET_SEED + 4, (uint32_t)runtime.seed[1]);
    put_u32(header + QR_OFFSET_TICKS, runtime.ticks);
    memcpy(header + QR_OFFSET_HELD_FRAMES, runtime.held_frames, sizeof(runtime.held_frames));

    id_length = strlen(info->cartdata_id);
    if (id_length > QUICK_RESUME_CARTDATA_ID_MAX)
    {
        id_length = QUICK_RESUME_CARTDATA_ID_MAX;
    }
    memcpy(header + QR_OFFSET_CARTDATA_ID, info->cartdata_id, id_length);
}

bool quick_resume_save(lua_State* L, int extra, const quick_resume_info_t* info,
    quick_resume_write_fn write, void* userdata)
{
    uint8_t header[QR_HEADER_SIZE];
    uint8_t trailer[QR_TRAILER_SIZE];
    qr_writer_t writer;
    int top;
    int status;

    if (!L || !info || !write)
    {
        return false;
    }

    writer.write = write;
    writer.userdata = userdata;
    writer.crc = 0;
    writer.failed = false;

    build_header(header, info);
    if (!emit(&writer, header, sizeof(header)) || !emit(&writer, pico8_ram, RAM_SIZE))
    {
        return false;
    }

    if (extra != 0)
    {
        extra = lua_absindex(L, extra);
    }
    top = lua_gettop(L);
    lua_pushcfunction(L, save_protected);
    lua_pushlightuserdata(L, &writer);
    if (extra != 0)
    {
        lua_pushvalue(L, extra);
    }
    else
    {
        lua_pushnil(L);
    }
    status = lua_pcall(L, 2, 0, 0);
    lua_settop(L, top);
    if (status != LUA_OK || writer.failed)
    {
        return false;
    }

    put_u32(trailer, writer.crc);
    return write(userdata, trailer, sizeof(trailer));
}

static const char* snapshot_reader(lua_State* L, void* userdata, size_t* size)
{
    qr_reader_t* reader = (qr_reader_t*)userdata;

    (void)L;
    if (reader->done)
    {
        *size = 0;
        return NULL;
    }
    reader->done = true;
    *size = reader->size;
    return (const char*)reader->data;
}

static int load_protected(lua_State* L)
{
    qr_reader_t* reader = (qr_reader_t*)lua_touserdata(L, 1);

    lua_settop(L, 0);
    lua_pushlightuserdata(L, &qr_inverse_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    if (!lua_istable(L, 1))
    {
        return luaL_error(L, "quick resume permanents are not registered");
    }
    eris_undump(L, snapshot_reader, reader);
    if (!lua_istable(L, -1))
    {
        return luaL_error(L, "quick resume root is not a table");
    }
    lua_getfield(L, -1, "g");
    if (!lua_istable(L, -1))
    {
        return luaL_error(L, "quick resume globals are not a table");
    }
    lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    lua_getfield(L, -1, "x");
    return 1;
}

bool quick_resume_load(lua_State* L, const uint8_t* data, size_t size,
    quick_resume_info_t* info)
{
    qr_reader_t reader;
    api_runtime_state_t runtime;
    int top;
    int status;

    if (!L || !data || !info || size <= QR_HEADER_SIZE + RAM_SIZE + QR_TRAILER_SIZE ||
        quick_resume_crc32(0, data, size - QR_TRAILER_SIZE) !=
            get_u32(data + size - QR_TRAILER_SIZE))
    {
        return false;
    }

    reader.data = data + QR_HEADER_SIZE + RAM_SIZE;
    reader.size = size - QR_HEADER_SIZE - RAM_SIZE - QR_TRAILER_SIZE;
    reader.done = false;

    top = lua_gettop(L);
    lua_pushcfunction(L, load_protected);
    lua_pushlightuserdata(L, &reader);
    lua_gc(L, LUA_GCSTOP, 0);
    status = lua_pcall(L, 1, 1, 0);
    lua_gc(L, LUA_GCRESTART, 0);
    if (status != LUA_OK)
    {
        lua_settop(L, top);
        return false;
    }

    memcpy(pico8_ram, data + QR_HEADER_SIZE, RAM_SIZE);
    lua_setpico8memory(L, pico8_ram);

    runtime.seed[0] = (fix32_t)get_u32(data + QR_OFFSET_SEED);
    runtime.seed[1] = (fix32_t)get_u32(data + QR_OFFSET_SEED + 4);
    runtime.ticks = get_u32(data + QR_OFFSET_TICKS);
    memcpy(runtime.held_frames, data + QR_OFFSET_HELD_FRAMES, sizeof(runtime.held_frames));
    api_set_runtime_state(&runtime);

    memcpy(info->cartdata_id, data + QR_OFFSET_CARTDATA_ID, QUICK_RESUME_CARTDATA_ID_MAX);
    info->cartdata_id[QUICK_RESUME_CARTDATA_ID_MAX] = '\0';
    return true;
}
