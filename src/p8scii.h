/** @file p8scii.h
 *
 *  A portable PICO-8 emulator written in C.
 *
 *  Copyright (c) 2025-2026, Michael Fitzmayer. All rights reserved.
 *  SPDX-License-Identifier: MIT
 *
 **/

#ifndef P8SCII_H
#define P8SCII_H

#include <stdint.h>

#define P8SCII_LINE_HEIGHT 6
#define P8SCII_TAB_WIDTH 16

uint8_t p8scii_char_width(uint8_t char_index);
typedef struct p8scii_style
{
    uint8_t fg;
    int bg;
    uint8_t scale_x;
    uint8_t scale_y;
    uint8_t stripey;
    uint8_t invert;
    uint8_t outline_mask;
    uint8_t outline_color;
    uint8_t outline_hollow;
} p8scii_style_t;

void p8scii_draw_char(uint8_t char_index, int x, int y, int cell_w, int cell_h, const p8scii_style_t* style);
void p8scii_draw_bitmap(int x, int y, const uint8_t* rows, int fg, int bg);
void blit_char_to_screen(uint8_t char_index, int x, int y, uint8_t color, uint8_t* w, uint8_t* h);

#endif // P8SCII_H
