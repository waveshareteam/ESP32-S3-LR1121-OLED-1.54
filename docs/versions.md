# Versions and support evidence

Framework and driver versions were resolved on 2026-10-08 and are pinned in `config/frameworks.json` and each example's local adapter manifest. Waveshare Boards uses `version: "*"` to select the latest compatible release; this policy was updated on 2026-10-10.

| Surface | Version | Primary-source evidence | Decision |
|---|---|---|---|
| ESP-IDF 5.5 | v5.5.5 | [official version/target index](https://dl.espressif.com/dl/esp-idf/idf_versions.js) lists ESP32-S3; latest stable 5.5 patch | 12 source projects |
| ESP-IDF 6 stable | v6.1 | [stable ESP32-S3 guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/) and official index identify v6.1 | 12 source projects |
| ESP-IDF 6.2 development | `624ded6069c4399e190d62b39ea67d2789ea496f` | [official commit](https://github.com/espressif/esp-idf/commit/624ded6069c4399e190d62b39ea67d2789ea496f); version metadata is 6.2.0 and ESP32-S3 is a supported target | Separate compatibility lane for all 12 projects; not a stable release |
| Arduino-ESP32 | 3.3.12 | [official release](https://github.com/espressif/arduino-esp32/releases/tag/3.3.12), based on IDF 5.5.5; [stable package index](https://espressif.github.io/arduino-esp32/package_esp32_index.json) | 11 sketches |
| Waveshare Boards | Latest compatible (`*`) | [registry](https://components.espressif.com/components/waveshare/waveshare_boards); product profile verified with 0.1.2 | Online product YAML for the 11 IDF examples; factory source retains its local profile |
| Board Manager | 0.7.3~1 | [registry](https://components.espressif.com/components/espressif/esp_board_manager/versions/0.7.3~1); official generator and ESP32-S3 capability catalogs inspected | Local product YAML and adapters; custom I2C OLED / sensor / audio composition entries |
| LVGL Adapter | 0.7.0 | [registry](https://components.espressif.com/components/espressif/esp_lvgl_adapter/versions/0.7.0); manifest requires IDF >=5.5 and LVGL >=8,<10 | Display builds only |
| LVGL | 9.5.0 | [registry](https://components.espressif.com/components/lvgl/lvgl/versions/9.5.0) | Same upstream release in IDF and Arduino; major-9 compile guard |
| ESP32_Display_Panel | 1.0.4 assessed | [registry](https://components.espressif.com/components/espressif/esp32_display_panel/versions/1.0.4); no SSD1309 driver in official sources | Not used; retain bundled Arduino_GFX 1.6.5 OLED transport |

Stable managed chip drivers are pinned to SSD1309 1.0.2, PCF85063A 2.1.0, BQ27220 0.1.2, I2C bus 1.4.2 and codec 1.5.11. Board Manager's generic audio-codec dependency starts at a 2.0 beta; that path is disabled. The product audio composition uses the stable codec API and Board Manager I2S handles instead.

The four audio projects pin [`chmorgan/esp-audio-player` 1.1.0](https://components.espressif.com/components/chmorgan/esp-audio-player/versions/1.1.0). This stable release explicitly requires `esp_driver_i2s` and `esp_ringbuf` on IDF 5.3 and later. Version 1.0.7 relied on transitive dependencies from `driver`, which IDF 6 removed. The existing write/mute/clock callbacks remain supported; new configuration fields default to zero.

Local compatibility changes are documented in [ESP-IDF 6.2 Compatibility](idf-6.2.md): RadioLib overload declarations, a build-directory patch for the pinned audio player's bounded name copy, and normalization of the generated CMake paths for this YAML-only board. Downloaded components, including the official Waveshare board pack, remain unchanged.

The [5.5 → 6.0](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/migration-guides/release-6.x/6.0/index.html) and [6.0 → 6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/migration-guides/release-6.x/6.1/index.html) guides were reviewed, including peripheral changes. New driver dependencies are explicit. I2C uses the modern master driver; the fuel-gauge bridge uses the managed I2C wrapper's external-bus mode with backward configuration disabled.

The generic ESP32-S3 FQBN and each menu option were checked against the official Arduino-ESP32 3.3.12 [boards.txt](https://github.com/espressif/arduino-esp32/blob/3.3.12/boards.txt): 16 MB flash, OPI PSRAM, hardware USB CDC and the 3 MB application / 9 MB FAT partition scheme.

Target support and dependency metadata have been checked. This is not a claim that any project has compiled on these versions. The explicitly requested 6.2 line is covered by a pinned development job, separate from the stable jobs. See [ESP-IDF 6.2 Compatibility](idf-6.2.md) for the 6.1 → 6.2 migration review and remaining validation. All IDF manifests allow `>=5.5.5,<6.3`.

The official [Waveshare Boards](https://components.espressif.com/components/waveshare/waveshare_boards) package is downloaded through Component Manager without an exact version constraint. The dependency policy is recorded in `config/waveshare_boards.json`; an existing `dependencies.lock` retains its resolved version until `idf.py update-dependencies` is run. The IDF action helper is `esp-bmgr-assist==0.8.3`. Component Manager pins are 2.5.0 for IDF 5.5.5, 3.0.3 for IDF 6.1 and 3.1.2 for the pinned IDF 6.2 development environment. These Python tools load and generate board configuration; the runtime Board Manager remains the managed component listed above.
