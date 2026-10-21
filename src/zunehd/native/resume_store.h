#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool resume_store_exists(uint32_t cart_crc);

bool resume_store_begin(uint32_t cart_crc);

bool resume_store_write(void* userdata, const void* data, size_t size);

bool resume_store_commit(void);

void resume_store_abort(void);

uint8_t* resume_store_load(uint32_t cart_crc, size_t* size);

void resume_store_free(uint8_t* data);

void resume_store_discard(uint32_t cart_crc);

bool resume_store_evict_oldest(void);

#ifdef __cplusplus
}
#endif
