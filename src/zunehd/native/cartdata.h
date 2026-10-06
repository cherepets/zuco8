#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool cartdata_select(const char* id, uint8_t* ram);

void cartdata_flush(void);

void cartdata_reset(void);

#ifdef __cplusplus
}
#endif
