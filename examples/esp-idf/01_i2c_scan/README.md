# Board I2C console

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md) and select `esp32_s3_lr1121_oled_1_54_xf` with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`.

This ESP32-S3 example borrows Board Manager's `i2c_main` bus. The product board YAML owns SDA/SCL. The console never deletes or rewires that bus.

At the `i2c-tools>` prompt use `i2cdetect` to scan, `i2cget` / `i2cdump` to read registers, and `i2cset` only when a deliberate register write is required. `i2cconfig --freq 100000` selects the console transfer clock (10–400 kHz); it accepts no GPIO or port arguments.

Expected onboard 7-bit addresses are OLED 0x3C, ES8311 0x18, ES7210 0x40, RTC 0x51 and battery gauge 0x55, subject to installed/powered hardware. The manual hardware test scans the product devices without resetting an unrelated sensor.

See [framework setup](../../../docs/examples.md) and [hardware evidence](../../../docs/hardware.md). No LVGL or Adapter dependency is enabled.
