# WalkieTalkieArduino

Arduino port of the ESP-IDF walkie-talkie example in this repository.

Target board:

- ESP32-S3-LR1121-OLED-1.54
- OLED: 128x64 monochrome SSD1309-compatible I2C panel
- Radio: LR1121 through RadioLib
- Audio: ES7210 microphone ADC, ES8311 speaker DAC, Codec2 3200

Required Arduino libraries:

- `RadioLib`
- `esp32_codec2`
- `GFX_Library_for_Arduino`

File layout:

- `WalkieTalkieArduino.ino`: setup/loop entry point only
- `Config.h`: board pins and shared constants
- `AppState.*`: shared radio/PTT runtime state
- `Settings.*`: saved radio settings and option labels
- `Ui.*`: OLED drawing, button navigation, fatal screen
- `UiFonts.*`: LVGL-matched `unscii_8` small font and Montserrat 20 frequency digits
- `UiIcons.*`: 1-bit OLED icon masks converted from the ESP-IDF LVGL image assets
- `Sensors.*`: RTC and BQ27220 polling
- `CodecDrivers.*`: ES8311 and ES7210 I2C setup
- `AudioPipeline.*`: I2S, Codec2, audio/radio packet buffers
- `RadioRuntime.*`: LR1121 configuration, TX/RX, IRQ handling
- `DynamicBuffer.h`, `ButtonState.h`: small reusable helpers

Use the libraries from:

`Z:\esp32-s3-lr1121-oled-1.54\esp32-s3-lr1121-oled-1.54\libraries`

In Arduino IDE, the simplest setup is to set **File > Preferences > Sketchbook
location** to:

`Z:\esp32-s3-lr1121-oled-1.54\esp32-s3-lr1121-oled-1.54`

The IDE will then find the `libraries` folder directly.

Suggested Arduino IDE setup:

- Board package: Arduino-ESP32 3.x
- Board: ESP32S3 Dev Module or the matching local board definition
- USB CDC on boot: Enabled if you want serial logs
- Flash/PSRAM options: match your board variant

Behavior matched from the IDF example:

- Same LR1121 GFSK/LoRa radio settings and defaults
- Same 240-byte Codec2 voice packet size
- Same PTT TX/RX behavior
- Same BOOT/PWR/MENU navigation model
- Same 128x64 UI layout, with 20 ms animated/scrolling frames and 300 ms steady status refresh cadence
- Same 10 ms main loop delay
- Same RTC 1 s polling cadence, with 300 ms charge-current polling and 1 s battery percentage polling

Notes:

- The sketch uses `Arduino_Canvas_Mono` with `Arduino_SSD1306`. SSD1309 is command-compatible enough for this OLED path. If the panel is shifted on a specific module, try replacing `Arduino_SSD1306` with `Arduino_SH1106` in the sketch.
- The battery charge icon follows only the BQ27220 `Current` register: current greater than 0 mA means charging, otherwise it shows the normal battery icon. Set `BATTERY_CHARGE_DEBUG_LOG` to `true` to print the current value while tuning hardware.
- Battery icon changes use a direct OLED page update for the top-right status area, so charge/discharge changes do not wait for a full 128x64 redraw.
- The ES7210 path is initialized directly over I2C because there is no Arduino ES7210 library in the supplied library folder.
- The I2S code requires Arduino-ESP32 3.x because it uses the ESP-IDF v5 `driver/i2s_std.h` and `driver/i2s_tdm.h` APIs.
- While PTT is held, serial logs show the TX audio queue, encoded/gated Codec2 frames, mic read failures, raw/processed mic level, estimated noise floor, gate state, last I2S read result, and each LR1121 packet send.
