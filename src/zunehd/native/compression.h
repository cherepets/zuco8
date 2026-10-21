#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

size_t compression_bound(size_t size);

size_t compression_compress(const uint8_t* input, size_t input_size, uint8_t* output,
    size_t output_capacity);

bool compression_decompress(const uint8_t* input, size_t input_size, uint8_t* output,
    size_t output_size);

#ifdef __cplusplus
}
#endif
