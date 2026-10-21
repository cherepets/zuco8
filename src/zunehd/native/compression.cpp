#include "compression.h"

#include <stdlib.h>
#include <string.h>

static const size_t kMinMatch = 4;
static const size_t kMaxMatch = 4 + 255;
static const size_t kWindow = 65536;
static const int kHashBits = 14;
static const int kChainDepth = 16;

static uint32_t Hash(const uint8_t* p)
{
    uint32_t value = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    return (value * 2654435761u) >> (32 - kHashBits);
}

static void Insert(const uint8_t* input, size_t input_size, size_t position,
    int32_t* head, int32_t* previous)
{
    uint32_t hash;

    if (position + kMinMatch > input_size)
    {
        return;
    }
    hash = Hash(input + position);
    previous[position & (kWindow - 1)] = head[hash];
    head[hash] = (int32_t)position;
}

extern "C" size_t compression_bound(size_t size)
{
    return size + size / 8 + 16;
}

extern "C" size_t compression_compress(const uint8_t* input, size_t input_size,
    uint8_t* output, size_t output_capacity)
{
    int32_t* head;
    int32_t* previous;
    size_t in_pos = 0;
    size_t out_pos = 0;
    size_t flag_pos = 0;
    int bit = 8;
    size_t i;

    head = (int32_t*)malloc(sizeof(int32_t) << kHashBits);
    previous = (int32_t*)malloc(sizeof(int32_t) * kWindow);
    if (!head || !previous)
    {
        free(head);
        free(previous);
        return 0;
    }
    for (i = 0; i < ((size_t)1 << kHashBits); ++i)
    {
        head[i] = -1;
    }

    while (in_pos < input_size)
    {
        size_t best_length = 0;
        size_t best_offset = 0;

        if (bit == 8)
        {
            if (out_pos >= output_capacity)
            {
                out_pos = 0;
                break;
            }
            flag_pos = out_pos++;
            output[flag_pos] = 0;
            bit = 0;
        }

        if (in_pos + kMinMatch <= input_size)
        {
            size_t limit = input_size - in_pos;
            int32_t candidate = head[Hash(input + in_pos)];
            int depth = kChainDepth;

            if (limit > kMaxMatch)
            {
                limit = kMaxMatch;
            }
            while (candidate >= 0 && (size_t)candidate < in_pos &&
                in_pos - (size_t)candidate <= kWindow && depth-- > 0)
            {
                const uint8_t* a = input + candidate;
                const uint8_t* b = input + in_pos;
                size_t length = 0;

                while (length < limit && a[length] == b[length])
                {
                    ++length;
                }
                if (length > best_length)
                {
                    best_length = length;
                    best_offset = in_pos - (size_t)candidate;
                    if (length == limit)
                    {
                        break;
                    }
                }
                candidate = previous[(size_t)candidate & (kWindow - 1)];
            }
        }

        if (best_length >= kMinMatch)
        {
            if (out_pos + 3 > output_capacity)
            {
                out_pos = 0;
                break;
            }
            output[flag_pos] |= (uint8_t)(1 << bit);
            output[out_pos++] = (uint8_t)(best_offset - 1);
            output[out_pos++] = (uint8_t)((best_offset - 1) >> 8);
            output[out_pos++] = (uint8_t)(best_length - kMinMatch);
            for (i = 0; i < best_length; ++i)
            {
                Insert(input, input_size, in_pos + i, head, previous);
            }
            in_pos += best_length;
        }
        else
        {
            if (out_pos >= output_capacity)
            {
                out_pos = 0;
                break;
            }
            output[out_pos++] = input[in_pos];
            Insert(input, input_size, in_pos, head, previous);
            ++in_pos;
        }
        ++bit;
    }

    free(head);
    free(previous);
    return out_pos;
}

extern "C" bool compression_decompress(const uint8_t* input, size_t input_size,
    uint8_t* output, size_t output_size)
{
    size_t in_pos = 0;
    size_t out_pos = 0;

    while (out_pos < output_size)
    {
        uint8_t flags;
        int bit;

        if (in_pos >= input_size)
        {
            return false;
        }
        flags = input[in_pos++];
        for (bit = 0; bit < 8 && out_pos < output_size; ++bit)
        {
            if (flags & (1 << bit))
            {
                size_t offset;
                size_t length;

                if (in_pos + 3 > input_size)
                {
                    return false;
                }
                offset = ((size_t)input[in_pos] | ((size_t)input[in_pos + 1] << 8)) + 1;
                length = (size_t)input[in_pos + 2] + kMinMatch;
                in_pos += 3;
                if (offset > out_pos || length > output_size - out_pos)
                {
                    return false;
                }
                while (length--)
                {
                    output[out_pos] = output[out_pos - offset];
                    ++out_pos;
                }
            }
            else
            {
                if (in_pos >= input_size)
                {
                    return false;
                }
                output[out_pos++] = input[in_pos++];
            }
        }
    }
    return in_pos == input_size;
}
