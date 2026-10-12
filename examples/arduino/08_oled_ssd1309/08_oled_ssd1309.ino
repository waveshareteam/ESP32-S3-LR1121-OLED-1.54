/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lv_conf.h>
#include <lvgl.h>
#if LVGL_VERSION_MAJOR != 9
#error "This product requires LVGL_VERSION_MAJOR == 9"
#endif
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define I2C_SDA 13
#define I2C_SCL 14
#define EXAMPLE_LVGL_PALETTE_SIZE      8

/* ================== Arduino_GFX ================== */
Arduino_DataBus *bus = new Arduino_Wire(0x3C, 0x00, 0x40, &Wire);
Arduino_G *gfx = new Arduino_SSD1306(bus, GFX_NOT_DEFINED, 128, 64); //此库没有做ssd1309，用ssd1306也一样

/* ================== LVGL ================== */
lv_display_t *disp;
uint8_t *lvgl_buf;
uint8_t oled_buffer[128 * 64 / 8];

/* tick */
uint32_t millis_cb(void) {
  return millis();
}

/* ================== 关键：flush ================== */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    px_map += EXAMPLE_LVGL_PALETTE_SIZE;

    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            /* The order of bits is MSB first
                        MSB           LSB
               bits      7 6 5 4 3 2 1 0
               pixels    0 1 2 3 4 5 6 7
                        Left         Right
            */
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

            /* Write to the buffer as required for the display.
            * It writes only 1-bit for monochrome displays mapped vertically.*/
            uint8_t *buf = oled_buffer + hor_res * (y >> 3) + (x);
            if (chroma_color) {
                (*buf) &= ~(1 << (y % 8));
            } else {
                (*buf) |= (1 << (y % 8));
            }
        }
    }
    // pass the draw buffer to the driver

    gfx->drawBitmap(0, 0, oled_buffer, 128, 64, 1, 0);

    lv_display_flush_ready(disp);
}

void lvgl_oled_demo(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_text_font(scr, &lv_font_unscii_8, 0);

    // Title label
    lv_obj_t *label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "SSD1309 128x64");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 4);

    // WiFi icon
    lv_obj_t *img = lv_image_create(scr);
    lv_obj_set_style_text_font(img, &lv_font_montserrat_16, 0);
    lv_image_set_src(img, LV_SYMBOL_WIFI);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 16, 0);

    // Battery icon
    lv_obj_t *img2 = lv_image_create(scr);
    lv_obj_set_style_text_font(img2, &lv_font_montserrat_16, 0);
    lv_image_set_src(img2, LV_SYMBOL_BATTERY_FULL);
    lv_obj_align(img2, LV_ALIGN_RIGHT_MID, -16, 0);

    // Bottom info
    lv_obj_t *label_info = lv_label_create(scr);
    lv_label_set_text(label_info, "1.54 inch OLED");
    lv_obj_align(label_info, LV_ALIGN_BOTTOM_MID, 0, -4);
}

/* ================== setup ================== */
void setup()
{
  Serial.begin(115200);
  if (!Wire.begin(I2C_SDA, I2C_SCL)) {
    Serial.println("I2C initialization failed");
    while (true) delay(1000);
  }
  Wire.setClock(400000);

  if (!gfx->begin()) {
    Serial.println("Display begin failed");
    while (true) {
      yield();
    }
  }

  lv_init();
  lv_tick_set_cb(millis_cb);

  /* ⚠️ 1bit buffer（整屏） */
  uint32_t buf_size = 128 * 64 / 8 + EXAMPLE_LVGL_PALETTE_SIZE;
  lvgl_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
  if (!lvgl_buf) {
    Serial.println("LVGL buffer allocation failed");
    while (true) {
      yield();
    }
  }

  disp = lv_display_create(128, 64);
  if (!disp) {
    Serial.println("LVGL display allocation failed");
    while (true) delay(1000);
  }
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_I1);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, lvgl_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);

  /* UI */
  lvgl_oled_demo();
}

/* ================== loop ================== */
void loop()
{
  lv_timer_handler();   // LVGL9
  delay(5);
}
