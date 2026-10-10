# First-party examples

Names are lower-case snake_case with exactly two prefix digits. Each numbered row below has both an IDF project under `examples/esp-idf/` and an Arduino sketch under `examples/arduino/`.

| Name | Function |
|---|---|
| `01_i2c_scan` | Board I2C scan / IDF register console |
| `02_sd_mmc` | One-bit SDMMC file I/O |
| `03_button` | Product keys |
| `04_microphone` | ES7210 audio capture |
| `05_speaker` | ES8311 audio playback |
| `06_rtc_pcf85063` | RTC and alarm |
| `07_bat_bq27220` | Battery measurements |
| `08_oled_ssd1309` | Matching LVGL 9 OLED title, Wi-Fi/battery icons and size label in both frameworks |
| `09_lora_ping_pong` | LR1121 peer radio exchange |
| `10_walkie_talkie` | Audio/radio UI; IDF uses LVGL, Arduino retains its existing monochrome graphics UI |
| `11_lorawan_starter` | LoRaWAN activation and state persistence |

`firmware/12_factory_demo` is a twelfth IDF project. It preserves the factory demonstration's distinct application behavior and uses the same board YAML. It is not the provenance of the checked-in factory binary unless a reproducible firmware comparison is performed.

## ESP-IDF

Use one of the stable versions or the pinned 6.2 development revision in `config/frameworks.json`. All 12 IDF source projects are included in the [6.2 compatibility lane](idf-6.2.md); the development revision is not a stable release. Ubuntu Bash, from a selected project directory in a clean checkout with ESP-IDF activated:

The 11 numbered IDF examples currently lack their product board YAML, and the published board pack 0.1.1 does not supply it yet. Their clean builds are pending publication and integration of that profile. The commands below apply once the required board data is available; see [Board Manager setup](board-manager.md). The factory source still contains its local definition.

```bash
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py build
idf.py -p PORT flash monitor
```

Install the bmgr tools for your SDK first as described in [Board Manager setup](board-manager.md). The same commands apply to the factory source project.

For an existing checkout, preserve old build output and SDK configuration by using a fresh build directory and a new SDK configuration:

```bash
idf.py -B build.normalized -D SDKCONFIG=build.normalized/sdkconfig set-target esp32s3
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py -B build.normalized -D SDKCONFIG=build.normalized/sdkconfig build
```

Each example maintains its C/H adapters under `components/esp32_s3_lr1121_oled_1_54_support`. Each project's main manifest requests the latest compatible online `waveshare/waveshare_boards` release with `version: "*"`; the local adapter manifest declares its runtime dependencies. Product YAML is retained locally only in `firmware/12_factory_demo`. `esp32` is not a target for this product; generic upstream tests were adapted to `esp32s3`.

## Arduino

Install Arduino CLI and run from the repository root in Ubuntu Bash:

```bash
python3 scripts/build_arduino.py 08_oled_ssd1309 --prepare
python3 scripts/build_arduino.py all
```

The helper installs Arduino-ESP32 3.3.12 and LVGL 9.5.0 under ignored `.arduino/`, uses the preserved product libraries, and supplies `config/arduino/lv_conf.h` for both C and C++ compilation. The exact FQBN is in `config/frameworks.json`: ESP32-S3 Dev Module, 16 MB Flash, octal PSRAM, a 16 MB partition layout and USB Serial/JTAG CDC.

`libraries/lvgl` is also retained as a bundled Arduino dependency and is included through the helper's `--libraries` path. It is not a build leftover. ESP-IDF display examples resolve their own managed LVGL dependency independently.

Arduino uses neither IDF managed Board Manager nor LVGL Adapter. Official ESP32_Display_Panel 1.0.4 has no SSD1309 driver/board, so there is no fictitious `Board::init()` path. The existing Arduino_GFX SSD1306-compatible transport is retained for this 128 × 64 OLED. The LVGL sketch checks I2C/panel/allocation startup, sets `LV_COLOR_FORMAT_I1`, uses one full-frame buffer and a separate page conversion buffer, and runs the timer in `loop()` with an Arduino millisecond tick. There is no touch device to register.

## Discovery

Ubuntu Bash, from the repository root:

```bash
python3 scripts/discover_examples.py --framework esp-idf --selector all
python3 scripts/discover_examples.py --framework esp-idf --channel development --selector all
python3 scripts/discover_examples.py --framework esp-idf --selector 09_lora_ping_pong
python3 scripts/discover_examples.py --framework arduino --selector examples/arduino/11_lorawan_starter
```

Discovery inspects direct first-party directories only. A mismatch between an Arduino directory and its single `.ino`, a malformed name, a duplicate name, an invalid selector, or an empty result stops with an explicit error. Names can be reused between IDF and Arduino, since each framework has a separate matrix.
