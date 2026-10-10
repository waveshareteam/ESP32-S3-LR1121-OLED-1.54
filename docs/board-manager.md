# Official board pack and product adapters

Each first-party IDF project downloads the official [Waveshare Boards](https://components.espressif.com/components/waveshare/waveshare_boards) package from the Component Registry. All 12 `main/idf_component.yml` manifests use:

```yaml
dependencies:
  waveshare/waveshare_boards:
    version: "*"
```

Component Manager installs it under `managed_components/waveshare__waveshare_boards/`. The wildcard in `config/waveshare_boards.json` and each manifest allows the latest compatible release when dependencies are first resolved. If `dependencies.lock` already exists, run `idf.py update-dependencies` from the application directory to refresh resolved versions. There is no local `components/waveshare_boards` copy to override the download.

The 11 projects under `examples/esp-idf/` no longer contain local board YAML. At the latest check, published pack 0.1.1 and upstream `main` do not yet contain `esp32_s3_lr1121_oled_1_54_xf`. Those examples cannot generate a fresh board configuration until the product profile is published and the generator/check integration is completed. This is a known pending migration; missing board data is not treated as a successful build or validation.

`firmware/12_factory_demo` still retains its three local YAML files. Its audio configuration is different from IDF 10's 8 kHz, two-slot profile and must not be used as a substitute for that example. Downloaded component files are never edited.

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

Once the product profile is available, `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf` selects it and generates a complete application-local `components/gen_bmgr_codes/`: C/header files, CMake registration, a dependency manifest, Kconfig, defaults and metadata. The generator does not copy or compile product adapters from the YAML directory. The generated component and the maintained `esp32_s3_lr1121_oled_1_54_support` component each compile their own sources once.

The application's `main` requires `esp32_s3_lr1121_oled_1_54_support`. Once generated, `esp32_s3_lr1121_oled_1_54_support` explicitly requires `gen_bmgr_codes`, so the I2C scanner's minimal build includes the board definitions. The conditional dependency permits initial dependency-resolution configuration before the first generation. `esp32_s3_lr1121_oled_1_54_support` uses whole-archive linking to retain the custom-device registrations.

Each example's `components/esp32_s3_lr1121_oled_1_54_support/include/board_hardware.h` retains the previously verified GPIO constants. The published board profile must be checked against these constants, device names and custom configuration types before integration. The application-facing functions in `board_support.h` obtain handles by name through Board Manager. Devices initialize lazily, so an absent SD card does not stop unrelated examples. Applications borrow bus/device handles; startup initialization is serialized before worker tasks.

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
- `board_audio.c` combines stable codec 1.5.11 with the manager's STD TX / TDM RX channels. `board_audio_player.c` retains MP3/file-iterator and codec-format helpers. Walkie-talkie uses the YAML sample rate and RX slot mask during codec open/resume, and reads/writes the manager-owned I2S handles with explicit timeouts. It selects 8 kHz, two physical slots and slot-0-only capture for Codec2; the other audio applications retain four-slot capture. The generic manager audio device would select a beta codec API; it is not enabled.
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

After the pending product profile has been published and integrated, resolve dependencies without compiling, list the boards, then select this product from an application under `examples/esp-idf/`:

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

## Pending board integration

Submit the product profile to the official board pack and publish it in the Component Registry. A GitHub merge alone does not update the registry dependency. Preserve the product's custom device configuration types, named handles and GPIO polarity, and provide IDF 10's 8 kHz, two-slot, slot-0 microphone configuration without changing the other audio examples' format.

The GPIO generator, YAML validation/audio tests and YAML-only CMake normalizer still target local board directories. Adapt them to the published profile before considering the online migration complete, then regenerate each application's board component and run its checks. These checks currently remain enabled and fail on the missing example YAML. Do not select another product to work around the missing profile.
