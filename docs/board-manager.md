# Official board pack and product adapters

Each first-party IDF project downloads the official [Waveshare Boards](https://components.espressif.com/components/waveshare/waveshare_boards) package from the Component Registry. All 12 `main/idf_component.yml` manifests use:

```yaml
dependencies:
  waveshare/waveshare_boards:
    version: "*"
```

Component Manager installs it under `managed_components/waveshare__waveshare_boards/`. The wildcard in `config/waveshare_boards.json` and each manifest allows the latest compatible release when dependencies are first resolved. If `dependencies.lock` already exists, run `idf.py update-dependencies` from the application directory to refresh resolved versions. There is no local `components/waveshare_boards` copy to override the download.

The 11 projects under `examples/esp-idf/` use `boards/esp32_s3_lr1121_oled_1_54_xf` from the online pack, verified with registry 0.1.2. They contain no local YAML copies. Static CI downloads the latest stable pack, verifies every file against the registry SHA-256 checksums, and caches only the product YAML under ignored `.ci/board-profile/`. Missing, changed or corrupt profiles fail validation. The resolved version and component hash are included in the inventory artifact.

`firmware/12_factory_demo` still retains its three local YAML files. Its default audio configuration matches the online profile: 16 kHz with four microphone slots. IDF 10 selects its application-specific speech format during initialization. Downloaded component files are never edited.

```text
examples/esp-idf/10_walkie_talkie/
├── components/
│   ├── esp32_s3_lr1121_oled_1_54_support/  Product C/H adapters
│   │   ├── CMakeLists.txt
│   │   ├── idf_component.yml
│   │   ├── Kconfig
│   │   ├── include/
│   │   ├── setup_device.c
│   │   ├── board_sensors.c
│   │   ├── board_display.c
│   │   ├── board_audio.c
│   │   └── board_audio_player.c
│   └── gen_bmgr_codes/        Ignored official bmgr output
└── managed_components/
    └── waveshare__waveshare_boards/  Ignored online dependency
```

Product C/H adapters stay at the `esp32_s3_lr1121_oled_1_54_support` component level. The local adapter explicitly requires `waveshare__waveshare_boards` so minimal builds include the downloaded pack too. The factory source additionally retains `boards/esp32_s3_lr1121_oled_1_54_xf/`, containing its three YAML files.

Every local adapter has `setup_device.c` and the public board/GPIO headers. Sensor examples add `board_sensors.c`; display examples add `board_display.c`; audio examples add `board_audio.c`, `board_audio_player.c` and their header; radio examples add `EspHal.h` and `board_radio.h`. The local manifest contains that example's managed dependencies. Local Kconfig records its display/audio capabilities.

`idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf` selects it and generates a complete application-local `components/gen_bmgr_codes/`: C/header files, CMake registration, a dependency manifest, Kconfig, defaults and metadata. The generator does not copy or compile product adapters from the YAML directory. The generated component and the maintained `esp32_s3_lr1121_oled_1_54_support` component each compile their own sources once.

The application's `main` requires `esp32_s3_lr1121_oled_1_54_support`. Once generated, `esp32_s3_lr1121_oled_1_54_support` explicitly requires `gen_bmgr_codes`, so the I2C scanner's minimal build includes the board definitions. The conditional dependency permits initial dependency-resolution configuration before the first generation. `esp32_s3_lr1121_oled_1_54_support` uses whole-archive linking to retain the custom-device registrations.

Each example's `components/esp32_s3_lr1121_oled_1_54_support/include/board_hardware.h` retains the previously verified GPIO constants. The GPIO generator and validator compare these constants with the resolved online profile, or the verified static-check cache when managed components are absent. The application-facing functions in `board_support.h` obtain handles by name through Board Manager. Devices initialize lazily, so an absent SD card does not stop unrelated examples. Applications borrow bus/device handles; startup initialization is serialized before worker tasks.

Product functions use the `board_` prefix throughout: `board_init()`, `board_peripheral()`, `board_device()`, `board_button()`, `board_display_start()` and the `board_extra_codec_*` / `board_extra_player_*` audio helpers. Applications and test fixtures use the same public names.

| Name | Ownership / purpose |
|---|---|
| `i2c_main` | Manager-owned native I2C bus; used by OLED, codecs, RTC and battery bridge |
| `spi_radio`, `radio` | Manager SPI host plus named LR1121 device; RadioLib owns command CS timing but borrows the SPI device |
| `button_boot`, `button_power`, `button_menu`, `button_ptt`, `button_rtc_irq` | Manager button devices with hardware-specific active levels |
| `sdcard` | Built-in manager SDMMC FAT device; one-bit mount, no automatic erase/format |
| `i2s_speaker`, `i2s_microphone`, `audio` | Manager channels plus a product composition of ES8311 output and ES7210 input; walkie-talkie captures slot 0 at 8 kHz |
| `oled` | Manager custom device holding the native panel and panel-IO handles |
| `rtc`, `battery` | Named managed chip-driver handles on the common I2C bus |

## Product glue retained

- SSD1309 I2C panel construction: Board Manager 0.7.3~1 has no native I2C `display_lcd` subtype. The managed SSD1309 driver supplies the controller sequence. This OLED has no touch, backlight, IO expander, or MCU-controlled reset pin.
- `board_display.c` registers the named OLED with Adapter's monochrome profile, then registers manager-provided navigation buttons. Adapter owns LVGL initialization, its task, locks, flush callbacks and both draw/conversion buffers. There is no local LVGL port or display flush implementation in IDF.
- Adapter 0.7.0 creates navigation input in event mode but its button callbacks only update state; they never trigger an LVGL input read. The local display glue switches this input to `LV_INDEV_MODE_TIMER` under the Adapter lock. The Adapter's LVGL task then reads presses/releases and handles navigation, confirmation and key repeat. Recheck this workaround when upgrading Adapter.
- `board_audio.c` combines stable codec 1.5.11 with the manager's STD TX / TDM RX channels. `board_audio_player.c` retains MP3/file-iterator and codec-format helpers. Walkie-talkie uses `esp_board_periph_init_custom()` with a copy of the online I2S configuration, changing only the application stream rate and RX slots before calling the managed I2S initializer. It selects 8 kHz, two physical slots and slot-0-only capture for Codec2 and preserves that format during codec open/resume. GPIOs, bus ports and codec addresses remain defined by the online board. Generated const configurations and downloaded files are never modified; the other audio applications retain four-slot capture. The generic manager audio device would select a beta codec API; it is not enabled.
- The BQ27220 driver uses `i2c_bus` 1.4.2 V2 to borrow the existing native manager bus. RTC uses the native handle directly. Battery calibration parameters retain the original demo behavior; the supplied capacity must match the installed cell.
- RadioLib's GPIO/interrupt and transaction protocol bridge and the Core1121 RF-switch table remain local. They contain no independent bus initialization.
- Existing Codec2 sources remain as third-party algorithm components in the two source applications; their variants are preserved. They are not board components and do not own GPIOs.

Only `08_oled_ssd1309`, `10_walkie_talkie`, and `12_factory_demo` enable `CONFIG_WS_BOARD_DISPLAY`. Only those examples' local manifests pin Adapter and LVGL. `04_microphone`, `05_speaker`, `10_walkie_talkie`, and `12_factory_demo` enable audio. Other examples have neither a display task nor a LVGL dependency.

## Enable bmgr with ESP-IDF 5.5 or 6.x

`bmgr` is a Board Manager action extension, not a built-in ESP-IDF 5.5 command. Each IDF version has its own Python environment. Installing the helper in a 6.x environment does not enable it in the 5.5 environment.

Activate the intended IDF environment, then install its tools. Ubuntu Bash or Windows PowerShell, for **IDF 5.5.5**:

```text
python -m pip install idf-component-manager==2.5.0 esp-bmgr-assist==0.8.3
```

For **IDF 6.1**, use Component Manager **3.0.3**; the pinned **6.2 development** environment uses **3.1.2**. Keep the helper at **0.8.3**. Do not install Component Manager 2.x into IDF 6.x. CI selects these versions from `config/frameworks.json`.

Resolve dependencies without compiling, list the boards, then select this product from an application under `examples/esp-idf/`:

```text
idf.py reconfigure
idf.py bmgr -l
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py build
```

These board-selection commands also apply to `firmware/12_factory_demo`. Upstream board definitions remain available for discovery, while this product's CI selects only `esp32_s3_lr1121_oled_1_54_xf`.

All 12 projects check the generated board name before component registration. A different board or incomplete generated defaults stops configuration with the exact `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf` recovery command. Select by this name instead of choosing another ESP32-S3 board from the list: its peripherals and drivers are different. For example, `esp_vocat_1_2` enables a generic audio driver requiring Codec 2.x, while this product's custom audio adapter uses Codec 1.5.11.

On first selection, upstream bmgr backs up an existing `sdkconfig` as `components/gen_bmgr_codes/sdkconfig.bmgr_board.old` and removes the original. For an existing configuration of this same product, restore that backup before `idf.py build` if its settings must be kept. Re-selecting an already generated board preserves `sdkconfig`. Use a separate build directory when switching IDF installations.

When recovering from a different board, keep an additional backup outside the generated directory. Run `idf.py reconfigure` after board selection to create this product's configuration, then reapply only unrelated settings to it. Do not restore the other board's `CONFIG_ESP_BOARD_*` selection, peripheral or device options: those must come from this product's newly generated defaults. The upstream extension injects those defaults only when `sdkconfig` is absent; copying an old file back before configuration skips that step. Run `idf.py reconfigure` again after reapplying settings.

Dependency locks can retain SDK-relative paths too: the I2C scanner imports `cmd_system` from the active SDK. When switching SDKs, move the application's previous `dependencies.lock` to a backup outside the project before `reconfigure`, so Component Manager resolves those paths against the new SDK. A fresh build directory alone does not replace this project-level lock.

The helper is optional if dependencies are already downloaded. The official manual extension-loading fallback works with IDF 5.5 too. **Ubuntu Bash**, from the application directory:

```bash
export IDF_EXTRA_ACTIONS_PATH="$PWD/managed_components/espressif__esp_board_manager${IDF_EXTRA_ACTIONS_PATH:+:$IDF_EXTRA_ACTIONS_PATH}"
idf.py bmgr -l
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
```

**Windows PowerShell**, from the application directory:

```powershell
$bmgrPath = Join-Path (Get-Location) 'managed_components/espressif__esp_board_manager'
$env:IDF_EXTRA_ACTIONS_PATH = (@($bmgrPath, $env:IDF_EXTRA_ACTIONS_PATH) | Where-Object { $_ }) -join ';'
idf.py bmgr -l
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
```

## Static profile checks

From the repository root, with Python and PyYAML installed (Ubuntu Bash or Windows PowerShell):

```text
python scripts/fetch_board.py
python scripts/generate_board.py --check
python -m unittest discover -s tests -v
python scripts/validate_repo.py
python scripts/inventory_repo.py
```

The fetch step verifies every package file before caching the three YAML files. It records the resolved registry version, component hash and product file hashes in `.ci/board-profile/resolved.json`. No downloaded profile is committed. For a configured project, checks use its actual managed profile; the factory's local profile is only used by the factory project.

The CMake normalizer handles both managed and retained factory YAML paths. It removes only YAML-only source/include directories from the generated component and rejects unexpected board sources. The selected-board guard remains active before dependency resolution. Host audio tests execute the real IDF 10 adapter against mocked hardware, starting from the published 16 kHz/four-slot defaults and verifying 8 kHz/two-slot/slot-0 initialization, retries, reopening and partial-I/O timeouts.
