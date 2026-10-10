# ESP-IDF 6.2 compatibility target

All 12 first-party ESP-IDF source projects, including `firmware/12_factory_demo`, target the 6.2 release line alongside the existing v5.5.5 and v6.1 stable checks. Arduino continues to use its own pinned core.

## Pinned development version

As checked on 2026-10-08, the [official release list](https://github.com/espressif/esp-idf/releases) and [version/target index](https://dl.espressif.com/dl/esp-idf/idf_versions.js) identify v6.1 as the latest stable release. There is no stable v6.2 tag to select yet. The explicitly requested 6.2 compatibility lane therefore uses this fixed official development revision:

- Source commit: [`624ded6069c4399e190d62b39ea67d2789ea496f`](https://github.com/espressif/esp-idf/commit/624ded6069c4399e190d62b39ea67d2789ea496f)
- Artifact version label: `v6.2-dev.624ded60`
- [Framework version](https://github.com/espressif/esp-idf/blob/624ded6069c4399e190d62b39ea67d2789ea496f/tools/cmake/version.cmake): 6.2.0
- [Target metadata](https://github.com/espressif/esp-idf/blob/624ded6069c4399e190d62b39ea67d2789ea496f/tools/idf_py_actions/constants.py): `esp32s3` is in `SUPPORTED_TARGETS`

The 12 application and local adapter manifests now require `>=5.5.5,<6.3`. This includes the 6.2 line and removes the previous upper-bound rejection. The version range and development revision are recorded in `config/frameworks.json`; static validation checks every project against that range.

## Migration review

The existing 5.5 → 6.0 → 6.1 review was extended through the [6.1 → 6.2 migration guide at the pinned revision](https://github.com/espressif/esp-idf/tree/624ded6069c4399e190d62b39ea67d2789ea496f/docs/en/migration-guides/release-6.x/6.2).

| Change in 6.2 | Effect on this product |
| --- | --- |
| UHCI / asynchronous memcpy / CRC DMA burst defaults | The maintained examples and local adapters do not use these APIs. I2S/SPI/LCD burst semantics are unchanged by this migration item. |
| Blocking PPA completion callbacks | ESP32-S3 display builds do not use PPA. The Adapter's PPA dependency is conditional on other targets. |
| RISC-V PMP setup moves from bootloader to application | Not applicable to the Xtensa ESP32-S3. |
| Image verification split into `esp_image_verify` | No first-party image-verification, partition-SHA or OTA API consumer was found. No new verification dependency is needed for these examples. |
| `bootloader_support` stops providing transitive crypto / OTA dependencies | First-party components do not rely on that dependency path. Adapter's conditional bootloader dependency is for flash-encryption detection, not image verification or crypto. |
| `port_start_app_hook` deprecation | Not used by the maintained examples or local adapters. |
| Rollback enabled with automatic confirmation by default | The examples keep the framework default. They provide no OTA update flow and do not override rollback confirmation. Source firmware packages retain matching bootloader and application output. |

The scan covered application code, the example-local adapter components, RadioLib and the retained Codec2 variants. The checked Board Manager, Adapter, I2C wrapper, RTC and battery manifests do not impose an upper bound excluding 6.2. No additional application API rewrite was indicated by the reviewed migration items. Dependency resolution, compilation and hardware behavior still need verification.

Bundled RadioLib now explicitly exposes inherited overloads in its intermediate modem and print classes. RF69 provides the exact `PhysicalLayer` power and sync-word overrides, forwarding to its existing implementations with the same default arguments. This resolves hidden-virtual warnings at the source. The earlier IDF warning downgrade and system-header workaround have been removed. The shared headers retain the existing concrete-radio call forms used by both frameworks.

Audio projects use `chmorgan/esp-audio-player` 1.1.0. The earlier `1.0.*` constraint resolved to 1.0.7, whose dependency on legacy `driver` did not expose `driver/i2s_std.h` under IDF 6. The upstream 1.1.0 release declares `esp_driver_i2s` and `esp_ringbuf` explicitly. The fix is in the four maintained dependency manifests, so component regeneration preserves it. See the official [IDF 6.0 peripheral dependency migration](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/migration-guides/release-6.x/6.0/peripherals.html).

Audio-player 1.1.0 triggers GCC's optimized `stringop-truncation` diagnostic for stream names. The four audio projects include `config/esp_idf_audio_compat.cmake` after `project()` to create a patched `audio_mixer.cpp` under the build directory and substitute that translation unit on the player target. The copy uses `strnlen` bounded to 15 bytes followed by `memcpy`; the zero-initialized destination and final NUL assignment are preserved. Vendor private headers remain available to that target. Managed component files and checksums remain intact, and warnings-as-errors remain enabled. Configuration fails with a patch-review message if the pinned upstream statement changes. Remove this compatibility patch when an upstream release fixes the copy. Validation must include optimized code generation, because `-fsyntax-only` does not diagnose this warning.

After updating the checkout, activate the required SDK and run `idf.py reconfigure` before building. Component Manager resolves the new exact version automatically. Use a separate build directory for each SDK, for example `idf.py -B build.idf62 reconfigure`; hand edits to downloaded component CMake files are unnecessary.

The retained Codec2 sources also triggered `-Werror=maybe-uninitialized` at `fsk_demod_freq_est`'s FFT call under optimized IDF 6.2 compilation. IDF 10 and the factory example now zero-initialize the FFT input allocation with `calloc`, while preserving the existing window-fill calculation. This fixes the diagnostic without relaxing warning checks. All 69 IDF 10 Codec2 source files passed optimized code generation on the tested 5.5.5 and local 6.2 SDKs; complete firmware and runtime validation remain pending.

All 12 projects include `config/esp_idf_board_compat.cmake` before `project()`. After checking that the selected board contains only its three YAML files, it removes that directory from the generated component's source/include paths. Regenerating with bmgr is supported; the upstream board pack and generator remain unchanged. IDF 10 also enables `MINIMAL_BUILD`, selecting the application's explicit dependencies and excluding unused Wi-Fi/Bluetooth components and their development-SDK include warnings. SDK Kconfig notes and the IDF 5.5 Component Manager's retry warnings may still appear; these are separate from C/C++ compilation warnings.

## CI and local use

The ESP-IDF workflow has a separate, visibly labeled 6.2 development job for every selected project. It checks out the exact framework commit, installs that revision's ESP32-S3 toolchain and builds the project. It does not request a nonexistent `v6.2` container tag or follow a moving `master` branch. The development jobs report failures normally; they do not use `continue-on-error`.

The complete IDF matrix is 36 jobs: 24 stable builds plus 12 development builds. Manual selectors apply to both groups. Ubuntu Bash, from the repository root, to inspect the development matrix without compiling:

```bash
python3 scripts/discover_examples.py --framework esp-idf --channel development --selector all
python3 scripts/discover_examples.py --framework esp-idf --channel development --selector 08_oled_ssd1309
```

For a future local build, activate the pinned ESP-IDF revision, then use the fresh-configuration commands in [Examples and Build Instructions](examples.md). Keep existing SDK configurations and downloaded/build directories when switching versions. Development firmware archives record the full framework commit as `framework_ref` in addition to the product repository's `git_sha`.

The I2C scanner's minimal build was configured and its application/generated/board sources passed compiler syntax checks on development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`. This is a different revision from the CI pin above. See [Validation](validation.md) for the exact scope and example-local adapter layout. No complete 6.2 firmware build or CI run has been executed, so successful linking and runtime compatibility are not yet established. When v6.2 becomes stable, recheck its migration notes and managed dependencies, replace the development pin with the exact stable tag, and compile every project before calling it verified stable support.
