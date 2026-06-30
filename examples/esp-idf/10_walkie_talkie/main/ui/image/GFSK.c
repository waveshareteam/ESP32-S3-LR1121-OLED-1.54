/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_GFSK
#define LV_ATTRIBUTE_IMAGE_GFSK
#endif

#define P_W 0xff, 0xff
#define P_K 0x00, 0x00
#define P_D 0x20, 0x00

#define ROW_TOP     P_K, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K
#define ROW_FILL_A  P_D, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_K
#define ROW_FILL_B  P_K, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_D
#define ROW_FILL_C  P_D, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_W, P_K
#define ROW_BOTTOM  P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D, P_K, P_D

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_GFSK uint8_t ICON_BR_map[] = {
  ROW_TOP,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_FILL_B,
  P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_D,
  P_D, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_D,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_K, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_D,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_BOTTOM,
};

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_GFSK uint8_t ICON_DV_map[] = {
  ROW_TOP,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_FILL_B,
  P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_D,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_D,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_D,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_BOTTOM,
};

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_GFSK uint8_t ICON_RX_map[] = {
  ROW_TOP,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_FILL_B,
  P_K, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K,
  P_D, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_D,
  P_D, P_W, P_W, P_K, P_K, P_K, P_K, P_W, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_W, P_W, P_K, P_W, P_W, P_W, P_D,
  P_D, P_W, P_W, P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_K,
  P_K, P_W, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_K, P_W, P_W, P_W, P_K, P_W, P_D,
  ROW_FILL_A,
  ROW_FILL_B,
  ROW_FILL_C,
  ROW_BOTTOM,
};

#undef ROW_TOP
#undef ROW_FILL_A
#undef ROW_FILL_B
#undef ROW_FILL_C
#undef ROW_BOTTOM
#undef P_W
#undef P_K
#undef P_D

const lv_image_dsc_t ICON_BR = {
  .header.cf = LV_COLOR_FORMAT_RGB565,
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 16,
  .header.h = 16,
  .data_size = 256 * 2,
  .data = ICON_BR_map,
};

const lv_image_dsc_t ICON_DV = {
  .header.cf = LV_COLOR_FORMAT_RGB565,
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 16,
  .header.h = 16,
  .data_size = 256 * 2,
  .data = ICON_DV_map,
};

const lv_image_dsc_t ICON_RX = {
  .header.cf = LV_COLOR_FORMAT_RGB565,
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.w = 16,
  .header.h = 16,
  .data_size = 256 * 2,
  .data = ICON_RX_map,
};
