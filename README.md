# Waveshare ESP32-S3-LR1121-OLED-1.54-XF

[中文](README_ZH.md)

The ESP32-S3-LR1121-OLED-1.54-XF is a LoRa and audio development board built around the ESP32-S3R8, with a dual-core Xtensa LX7 processor up to 240 MHz, 8 MB octal PSRAM, 16 MB Flash, 2.4 GHz Wi-Fi and Bluetooth LE. It integrates an LR1121 radio, a 1.54-inch 128 × 64 monochrome OLED, ES8311 speaker codec, ES7210 microphone ADC, RTC, microSD slot and battery interface for wireless sensing and voice applications.

- [Example Documentation](docs/examples.md)
- [Hardware Reference](docs/hardware.md)
- [Firmware and Factory Recovery](firmware/README.md)

<img src="assets/ESP32-S3-LR1121-OLED-1.54-DocHeader-1.webp" alt="Waveshare ESP32-S3-LR1121-OLED-1.54-XF" width="500">

## Overview

This repository contains first-party ESP-IDF and Arduino examples, factory demonstration source, the original factory recovery firmware, bundled Arduino libraries, schematics and mechanical files for the Waveshare ESP32-S3-LR1121-OLED-1.54-XF.

The display uses an SSD1309 controller over I2C, with no touchscreen. Other onboard peripherals include a PCF85063A RTC, BQ27220 fuel gauge and one-bit SDMMC interface. The common digital wiring is recorded in the [hardware reference](docs/hardware.md); confirm the actual HF/LF radio module and regional RF settings for your board.

## Getting Started

Choose a framework and open the README in the example directory:

- ESP-IDF projects: [`examples/esp-idf/`](examples/esp-idf/)
- Arduino sketches: [`examples/arduino/`](examples/arduino/)
- Bundled Arduino libraries: [`libraries/`](libraries/)
- Factory demonstration source and recovery image: [`firmware/`](firmware/)

ESP-IDF projects use their own `sdkconfig.defaults` and download the latest compatible `waveshare/waveshare_boards` release from the Component Registry. Product C/H adapters remain in `components/esp32_s3_lr1121_oled_1_54_support`. The configured framework versions are ESP-IDF **v5.5.5 / v6.1** and Arduino-ESP32 **3.3.12**, with an additional pinned **ESP-IDF 6.2 development** compatibility lane covering every IDF project. LVGL-based display examples use LVGL **9.5.0**; IDF display projects also use LVGL Adapter **0.7.0**.

**ESP-IDF board integration is pending.** Local board YAML has been removed from the 11 examples under `examples/esp-idf/`. The current online board pack, 0.1.1, does not contain this product yet. Clean builds of those examples require publication of the product profile and completion of the generator/check integration described in [Board Manager setup](docs/board-manager.md). The factory source retains its local board definition. Arduino dependencies, including bundled LVGL, are retained.

Before the first IDF build, follow [Board Manager setup](docs/board-manager.md#enable-bmgr-with-esp-idf-55-or-6x): install the helper in the active IDF environment, resolve dependencies, then select this board with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`. This also enables `bmgr` on IDF 5.5.

Arduino builds use the **ESP32S3 Dev Module** profile with 16 MB Flash, OPI PSRAM, the `app3M_fat9M_16MB` partition scheme and hardware USB CDC/JTAG. The exact FQBN and version pins are stored in [`config/frameworks.json`](config/frameworks.json). The Arduino OLED examples use Arduino_GFX; Board Manager and LVGL Adapter are used only by ESP-IDF projects.

See [Examples and Build Instructions](docs/examples.md) for setup commands, including how to preserve an existing SDK configuration and build directory.

## Examples

| ESP-IDF project | Focus |
| --- | --- |
| [01_i2c_scan](examples/esp-idf/01_i2c_scan/) | Shared I2C bus scan and register console |
| [02_sd_mmc](examples/esp-idf/02_sd_mmc/) | One-bit SDMMC file read/write |
| [03_button](examples/esp-idf/03_button/) | Onboard button events |
| [04_microphone](examples/esp-idf/04_microphone/) | ES7210 microphone capture |
| [05_speaker](examples/esp-idf/05_speaker/) | ES8311 speaker playback |
| [06_rtc_pcf85063](examples/esp-idf/06_rtc_pcf85063/) | Real-time clock and alarm |
| [07_bat_bq27220](examples/esp-idf/07_bat_bq27220/) | Battery measurements |
| [08_oled_ssd1309](examples/esp-idf/08_oled_ssd1309/) | LVGL 9 monochrome OLED UI, matching the Arduino example |
| [09_lora_ping_pong](examples/esp-idf/09_lora_ping_pong/) | LR1121 peer radio exchange |
| [10_walkie_talkie](examples/esp-idf/10_walkie_talkie/) | Audio/radio application with LVGL UI |
| [11_lorawan_starter](examples/esp-idf/11_lorawan_starter/) | LoRaWAN activation and state persistence |

| Arduino sketch | Focus |
| --- | --- |
| [01_i2c_scan](examples/arduino/01_i2c_scan/) | Shared I2C bus scan |
| [02_sd_mmc](examples/arduino/02_sd_mmc/) | One-bit SDMMC file read/write |
| [03_button](examples/arduino/03_button/) | Onboard button events |
| [04_microphone](examples/arduino/04_microphone/) | ES7210 microphone capture |
| [05_speaker](examples/arduino/05_speaker/) | ES8311 speaker playback |
| [06_rtc_pcf85063](examples/arduino/06_rtc_pcf85063/) | Real-time clock and alarm |
| [07_bat_bq27220](examples/arduino/07_bat_bq27220/) | Battery measurements |
| [08_oled_ssd1309](examples/arduino/08_oled_ssd1309/) | LVGL 9 monochrome OLED UI |
| [09_lora_ping_pong](examples/arduino/09_lora_ping_pong/) | LR1121 peer radio exchange |
| [10_walkie_talkie](examples/arduino/10_walkie_talkie/) | Audio/radio application with Arduino_GFX UI |
| [11_lorawan_starter](examples/arduino/11_lorawan_starter/) | LoRaWAN activation and state persistence |

[12_factory_demo](firmware/12_factory_demo/) is an additional ESP-IDF source project with the factory demonstration's distinct application behavior and LVGL UI. It uses the same board YAML and is included in the source build matrix.

First-party sketches live directly under `examples/arduino/`, and each directory matches its `.ino` filename. Examples inside bundled libraries, backups and downloaded components are excluded from product CI. The original factory binary is kept separately from source-build artifacts.

## Continuous Integration

The workflows discover projects and accept `all`, an example name or a repository-relative path for manual runs. The configured matrix contains **36 ESP-IDF builds**: 24 across two stable versions and 12 against the pinned 6.2 development revision, plus **11 Arduino builds**. The development jobs are labeled separately; see [ESP-IDF 6.2 Compatibility](docs/idf-6.2.md).

Each successful source build is packaged as a flashable archive with a manifest, binary hashes, flash arguments and Windows/POSIX flash helpers. IDF packages preserve the original binary segments and offsets; Arduino packages use the core's merged export. Workflows upload downloadable artifacts without automatically publishing Releases.

The current checkout passes 18 of 20 host tests; two tests and repository validation require the removed board YAML and currently fail. Earlier compiler checks were performed before that removal. The build workflows have not yet been run for this state. See [CI and Validation](docs/validation.md) and [Source Firmware Artifacts](releases/README.md) for details.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `examples/esp-idf/` | First-party ESP-IDF examples |
| Each IDF project's `components/` | C/H adapters in `esp32_s3_lr1121_oled_1_54_support`; official board pack downloaded automatically; example board integration pending |
| `examples/arduino/` | First-party Arduino sketches |
| `libraries/` | Bundled third-party Arduino dependencies, including LVGL 9.5.0 |
| `config/` | Framework versions, board options and Arduino LVGL configuration |
| `scripts/` | Example discovery, inventory, validation and build helpers |
| `tests/` | Discovery and firmware-packaging unit tests |
| `releases/` | Source-build packaging tools and instructions |
| `firmware/` | Factory demonstration source and original recovery binary |
| `hardware/` | Schematics and mechanical files |
| `docs/` | Hardware, framework, component and validation notes |

## Documentation

- [Examples and Build Instructions](docs/examples.md)
- [Hardware Reference](docs/hardware.md)
- [Framework and Component Versions](docs/versions.md)
- [ESP-IDF 6.2 Compatibility](docs/idf-6.2.md)
- [Board Manager Integration](docs/board-manager.md)
- [CI and Validation](docs/validation.md)
- [Firmware and Factory Recovery](firmware/README.md)
- [Source Firmware Artifacts](releases/README.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Security Policy](SECURITY.md)

## License

Repository-owned code is licensed under the Apache License 2.0. See [LICENSE](LICENSE). Third-party libraries and generated sources retain their own license notices.
