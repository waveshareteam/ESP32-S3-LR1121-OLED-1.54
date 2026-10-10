# SSD1309 OLED Example

This example shows the same static LVGL 9 interface as [Arduino 08](../../arduino/08_oled_ssd1309/08_oled_ssd1309.ino) on the onboard 128 × 64 OLED:

- Top: `SSD1309 128x64`, centered with a 4-pixel top offset.
- Left: Wi-Fi symbol, centered vertically with a 16-pixel left offset.
- Right: full-battery symbol, centered vertically with a 16-pixel right offset.
- Bottom: `1.54 inch OLED`, centered with a 4-pixel bottom offset.

Text uses `lv_font_unscii_8`; the two symbols use `lv_font_montserrat_16`. The symbols are demonstration graphics and do not show live Wi-Fi or battery status. The screen stays on this page; buttons do not switch images.

## Hardware

Use an ESP32-S3-LR1121-OLED-1.54 board and a USB cable. The onboard SSD1309 uses the I2C bus and display parameters from the product Board Manager YAML. No external display wiring or SD card is needed.

## Build and run

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md). With ESP-IDF activated, run these commands in Ubuntu Bash from this example directory:

```bash
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the board's serial port. Exit the serial monitor with `Ctrl+]`.

After startup, the serial log prints `OLED demo ready` and the display shows the four elements listed above.

## Implementation

The application obtains the display through `components/esp32_s3_lr1121_oled_1_54_support`, locks LVGL, creates the UI and releases the lock. Board Manager owns the OLED and I2C device handles. `esp_lvgl_adapter` owns the LVGL task, tick, buffers and display flush. The project pins LVGL 9.5.0 and has a major-version-9 compile guard.

The demo creates its UI directly and requires neither image conversion nor a BOOT polling timer.

## Troubleshooting

- If the display is blank, check the startup log for display initialization or lock errors.
- If text or symbols are missing after customizing LVGL configuration, enable `CONFIG_LV_FONT_UNSCII_8` and `CONFIG_LV_FONT_MONTSERRAT_16`; both are enabled in `sdkconfig.defaults`.
- Check OLED orientation and polarity against the product board configuration if the screen differs from the Arduino example.
