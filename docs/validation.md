# Validation boundary and inventory

Validation covers static checks, Board Manager generation, CMake configuration, compiler syntax checks and targeted optimized code generation to temporary assembly files; no complete firmware compilation, linking, flashing, serial-port access, RF transmission or hardware test was performed. Framework and component metadata establish target/API intent, not successful builds. Historical local checks below did not run full firmware builds. GitHub Actions results and the subsequent CI fixes are recorded separately below.

`libraries/lvgl` is intentionally retained for Arduino. Repository validation accepts this dependency; ESP-IDF display projects still require managed LVGL in their manifests. Earlier failure reports below about an offline LVGL tree describe the former validation rule, which has been removed. The library's existing edits and whitespace findings are outside this cleanup's scope.

## Online board integration (2026-10-12)

Registry board pack 0.1.2 includes `esp32_s3_lr1121_oled_1_54_xf`. All 50 published files passed size and SHA-256 checks, and the product YAML matches upstream revision `a226300a947a7edb5f31925666394a0a64079325`. All 12 maintained GPIO headers match that profile. The 11 deleted example board directories remain absent; the factory retains its local profile. Dependencies continue to request `*`.

Static CI now fetches and verifies the latest stable board profile before tests, generation checks, validation and inventory. CMake accepts the managed YAML path and retains the wrong-board/source-ownership checks. IDF 10 applies the Codec2 8 kHz/two-slot/slot-0 format through Board Manager's custom peripheral initializer, copying the immutable online configuration and retaining its wiring. Host tests exercise initialization/retry, codec-open failure, stop/resume and partial I/O. The factory Codec2 table now initializes all fields, as already done in IDF 10, addressing IDF 6's missing-field error without disabling warnings.

All 36 host tests pass, along with repository validation of 23 first-party entries, all 12 GPIO-header checks, actionlint, inventory, public-content scanning and maintained-source diff checks. The earlier missing-board failures below are historical and superseded by this integration. Full firmware builds run in GitHub Actions; local checks remain within the static/configuration boundary. Hardware speech quality, buttons, RF and flashing still require physical validation.

## CI and release repair (2026-10-12)

GitHub `main` at `a0bf739` matched the GitLab source. The first push ran Arduino CI: ten builds and packages succeeded, while `08_oled_ssd1309` failed during library discovery with `lv_conf.h: No such file or directory`. The ESP-IDF workflow was rejected before any build because job-level `env` used the unavailable `runner` expression context. Static tests failed on the two known missing-board-YAML accesses.

The OLED configuration is now an explicit Arduino library; the sketch includes it before LVGL. The bundled LVGL remains unchanged and the redundant Library Manager download is removed. Local Arduino CLI preprocessing passed using the installed 3.3.11 core; this is an include-discovery check, not a full build on the pinned CI core 3.3.12. ESP-IDF sets its tool directory in a step using `RUNNER_TEMP` and `GITHUB_ENV`. Actionlint 1.7.12 validates all four workflows and also runs as an independent CI job. External Actions are pinned by commit, with Dependabot updates enabled.

Stable version tags now call the same build workflows and require all 47 expected source archives. Packaging adds deterministic metadata, original IDF segments and a combined image, checksums and flash helpers. Independent validation rejects incomplete/duplicate matrices, stale commits, unsafe ZIP members, bad hashes, wrong targets and inconsistent combined images. Publication uploads to a draft, verifies remote asset digests and refuses to overwrite public Releases. No release tag or Release was created for this repair.

The full local suite has 33 tests: 31 pass and the same two audio tests fail because their board YAML is absent. CMake host tests run with an installed CMake tool. New release tests cover corrupted archives, image layout, matrix integrity, remote tag changes and publication refusal. Repackaging and independent validation also passed with the real Arduino 10 merged image downloaded from the initial GitHub run (16,777,216 bytes; the canonical Arduino padding is retained). Source inventory remains 12 IDF projects and 11 Arduino sketches. Public-content and maintained-diff checks pass.

The upstream board pack was rechecked: registry 0.1.1 and upstream `37f2df77734b2f894c0935d9d2cf57ddf23e1079` still do not contain this product. The 11 deleted board directories have not been restored. The two tests and repository validator continue to fail explicitly, and the new Release gate cannot publish until board publication and generator/check integration are complete. These failures are not waived or converted to skips. Full local firmware builds, flashing and hardware tests were not performed.

## Historical source synchronization (2026-10-10)

On 2026-10-10, all 11 local `boards` directories under `examples/esp-idf/` were removed by request, pending a product-profile contribution to `waveshare_boards`. The factory source retains its board YAML. Registry version 0.1.1 and upstream revision `37f2df77734b2f894c0935d9d2cf57ddf23e1079` do not yet include this product. Fresh example builds and the generator/check migration remain pending; see [Board Manager setup](board-manager.md#static-profile-checks).

At that synchronization, the checkout passed 18 of 20 host tests. Two audio tests and repository validation fail because they require the deleted YAML. The public-content scan passes, and discovery still finds 12 IDF source projects and 11 Arduino sketches. An isolated diagnostic copy supplied with the previous YAML passes all 20 tests and validation of all 23 entries; this isolates the known missing-board-data failure but does not establish success for the current checkout. No firmware build or hardware test was run for this synchronization.

The successful generation and compiler checks recorded below occurred before the board-data removal. They do not prove that a fresh checkout of the current source can build. The 11 example board directories remain absent; no generated components, build output or SDK configurations are part of the synchronized source.

## Inventory

- Before and after: 11 first-party IDF examples plus one distinct first-party source-firmware project; 11 Arduino sketches.
- Bundled-library examples remain outside the product matrix. The local LVGL tree is retained as an Arduino dependency. No product sketch was removed.
- Three third-party IDF projects (one RadioLib and two SensorLib), all bundled library examples, generated/downloaded components and nested build/reference/backup directories are excluded.
- The original factory binary remains under `firmware/` and is never selected as build input.
- IDF 08 now matches the Arduino static OLED interface. Its legacy generated image descriptors are excluded from the build. Generated/downloaded files and reference ZIPs stay outside the source matrix.
- No second discoverable old project remains after the renames. Each example downloads the official `waveshare/waveshare_boards` component and retains its C/H adapters in `components/esp32_s3_lr1121_oled_1_54_support`. Only the factory source currently retains local product YAML. Distinct factory-demo application logic and Codec2 source variants remain.

## Online board-pack migration

All 12 main manifests now request `waveshare/waveshare_boards` from the Component Registry with `version: "*"`. The initial online migration was checked with 0.1.1; later releases require their own compatibility validation. The local official snapshots were removed after verifying their upstream content, and all 36 product YAML files were moved unchanged to `components/esp32_s3_lr1121_oled_1_54_support/boards/esp32_s3_lr1121_oled_1_54_xf/`.

Clean temporary copies passed online dependency resolution, `idf.py bmgr -l`, board selection and final CMake configuration for `01_i2c_scan` on ESP-IDF v5.5.5 and `10_walkie_talkie` on local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`. At that validation, both dependency locks resolved registry version 0.1.1 and its published component hash; every downloaded board-pack file matches the upstream checksums. The configured components include the downloaded pack and generated board code, without compiling other products' board sources. The scanner does not pull in LVGL Adapter. IDF 5.5 configuration still emits its existing CMake/Kconfig notices, including the unused `I2C_BUS_BACKWARD_CONFIG` setting.

The official generator also found the new local YAML path and generated board code in isolated copies of all 12 projects. All 19 host tests and the GPIO and public-content checks passed. Firmware compilation, linking and hardware testing were not performed. The product checkout remains free of build directories, downloaded/generated components, `sdkconfig` and `dependencies.lock`; `sdkconfig.defaults` is preserved. Full repository validation and diff checks still report the pre-existing offline LVGL tree and its whitespace issues.

## Product support component naming

All 12 IDF projects use `components/esp32_s3_lr1121_oled_1_54_support/` and declare that component in `main/CMakeLists.txt`. The rename preserved all 135 component files byte-for-byte, including product YAML and per-example audio/display settings. Scripts, generated-CMake normalization and documentation use the new component path.

The official generator found the renamed board directory in temporary copies of all 12 projects. Fresh configuration, board selection and final CMake configuration passed for `01_i2c_scan` on ESP-IDF v5.5.5 and `10_walkie_talkie` on local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`. Both configured builds select the new component and generated board code. All 19 host tests and the GPIO/public-content checks passed. Complete firmware compilation and hardware tests remain pending; the existing offline LVGL and whitespace acceptance issues remain.

## Product function naming

All 51 product functions and both CMake compatibility helpers now use the `board_` prefix. Declarations, definitions, calls, test fixtures and documentation were updated together across all 12 IDF projects. Comparing the modified source with its pre-rename contents confirmed that only function identifiers and the audio-interface comment changed.

All 19 host tests passed. Using the existing isolated project configurations, the actual ESP-IDF v5.5.5 compiler checked 8 application/support/generated sources for `01_i2c_scan`, and the local 6.2 development compiler checked 31 for `10_walkie_talkie`. Both runs generated temporary optimized assembly with zero warnings; neither linked firmware or accessed hardware. GPIO consistency and the public-content scan passed. Full repository acceptance still reports the existing offline LVGL tree and its whitespace issues.

The subsequent filename update renamed 47 product C/H files across all 12 projects to use `board_` names, including `board_support.h`, `board_hardware.h`, `board_radio.h`, `board_extra.h` and the audio/display/sensor source files. Includes, CMake source lists, generator paths, test fixtures and documentation were updated together. No old filenames or references remain in maintained files. All 19 host tests passed again; both isolated IDF configurations select the renamed files, and the same 39 translation units passed optimized assembly generation with zero warnings. Complete firmware linking and hardware validation remain pending.

## Static checks

The current checks retain 23 first-party entries. Unit tests, the maintained-content scan and GPIO-constant consistency checks are run after changes. Repository validation accepts the bundled Arduino LVGL library. Full `git diff --check` reports existing whitespace changes in that library; these user-owned files are preserved. Generated board components, builds and downloaded dependencies are ignored. Existing application `sdkconfig` files are backed up and restored during the first standard bmgr migration, then verified byte-for-byte.

Ubuntu Bash, from the repository root:

```bash
python3 -m unittest discover -s tests -v
python3 scripts/validate_repo.py
python3 scripts/scan_public_content.py
python3 scripts/inventory_repo.py
git diff --check
```

The tests cover complete stable and 6.2 development matrices, fixed development revisions, every name/path selector, invalid/empty selectors, duplicate projects, nested exclusions, Arduino naming mismatches and packaging from synthetic IDF/Arduino exports. Synthetic export tests verify archive structure, offsets and development framework revision metadata; they do not replace a real firmware build or flash test. Repository validation checks counts, project names, local adapter dependencies, the wildcard online board-pack dependency and local product YAML directories, target settings, display-only LVGL dependencies, major-version guards, GPIO ownership and YAML/Python syntax. The public scan checks maintained first-party text for stale paths, unrelated product text, local workstation paths, private-network URLs and credential patterns without printing matching values.

The official IDF 5.5 bmgr extension discovers this board by name and generates complete application-local `gen_bmgr_codes` components for all 12 projects. Each project now downloads the latest compatible `waveshare/waveshare_boards` release and keeps its three product YAML files in `components/esp32_s3_lr1121_oled_1_54_support/boards/esp32_s3_lr1121_oled_1_54_xf/`. Maintained C/H adapters live in `components/esp32_s3_lr1121_oled_1_54_support`. Repository validation checks the wildcard registry dependency and rejects local component copies that could shadow it. The transition to online downloads preserved all 36 product YAML files byte-for-byte, including the walkie-talkie audio correction below. YAML specifies the intended shared audio clocks. A prior host simulation using actual Adapter 0.7.0 and LVGL 9.5.0 reproduced inactive event-mode navigation and verified timer-mode navigation, confirmation, release and repeat. The same workaround is preserved in the local display adapters; the simulation does not test physical buttons or GPIO polarity.

### Configuration checks after importing the official board pack

On 2026-10-09, the official board-pack integration was checked using separate build directories and copies of application `sdkconfig` files:

| Project | Framework | Result |
|---|---|---|
| `01_i2c_scan` | ESP-IDF v5.5.5 | CMake configuration and 8 compiler syntax checks passed |
| `01_i2c_scan` | ESP-IDF 6.2 development, `08e0d30a74ad0bfd5a34933142b80f45619ee410` | CMake configuration and 8 compiler syntax checks passed |
| `10_walkie_talkie` | ESP-IDF v5.5.5 | CMake configuration and 31 compiler syntax checks passed |

Each minimal build contains five official generated C files, one local adapter C file and two application C files, with each included once. There are zero LVGL or Adapter source entries in these non-display builds. The display check covers eight application C++ files, thirteen image C files, five generated C files and five local adapter C files. It includes the managed LVGL/Adapter dependencies and compiles each adapter once. No source entry comes from the YAML directory.

All three configured builds include the official `waveshare_boards` component. None compiles sources from the other upstream board profiles. The initial 6.2 check reported an include-directory ownership warning because bmgr added the selected YAML directory to `gen_bmgr_codes`. The subsequent warning cleanup below normalizes those generated paths while preserving the upstream generator and board pack.

Syntax checks use the actual compiler and flags with `-fsyntax-only`; they do not link or produce firmware. The 6.2 check resolved a separate dependency lock, confirmed that no 5.5 SDK paths remained in the compiler commands. All ten pre-existing application `sdkconfig` files remain byte-for-byte unchanged. Original dependency locks were backed up; configuration checks resolved new locks for the local adapter dependencies.

The 13 Python unit tests, YAML-derived GPIO checks across 12 projects and the scan of 329 maintained text files passed. Full repository validation and full diff whitespace checks still report the preserved offline LVGL tree and its existing whitespace changes, respectively. Other tracked changes and the local adapter files pass whitespace checks. Imported upstream files retain their original formatting.

The four upstream board profiles are retained as vendor definitions and are excluded from the product build matrix. The imported pack has no nested Git repository.

The tested 6.2 revision differs from the pinned CI revision `624ded6069c4399e190d62b39ea67d2789ea496f`; these checks do not establish successful builds on that CI revision or for every application.

## Walkie-talkie audio correction

On 2026-10-09, comparison with the working Arduino example found that IDF 10 opened 16 kHz playback and four-channel capture while its Codec2 3200 pipeline expected 8 kHz mono. Its local YAML now selects 8 kHz in both directions, two physical clock slots and slot-0-only capture. Codec open/resume reads the microphone slot configuration and sample rate from Board Manager. The ADC gain is 24 dB, matching Arduino. Explicit I2S timeouts, error checks, per-frame PTT checks and worker-idle acknowledgements before codec shutdown preserve the audio task lifecycle. These changes require real audio and sleep/wakeup testing.

The HF frequency menu also used 2 MHz steps, so loading an Arduino-saved value such as 869 MHz silently selected 868 MHz. IDF now uses 1 MHz steps for HF and LF. Radio logs print the active settings, and audio logs report encoded frames, processed peak and microphone failures. Arduino source and settings remain unchanged.

Validation uses independent temporary project copies and leaves the application's existing SDK configuration and dependency lock unchanged:

- Board Manager generation succeeded; generated I2S configuration selects 8 kHz and microphone slot 0 of two slots. Shared MCLK/BCLK/WS warnings are expected for this duplex circuit.
- IDF v5.5.5 and local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410` passed CMake configuration and 31 application/generated/adapter syntax checks each.
- All 73 RadioLib source files passed syntax checks on both SDKs. The subsequent source-level overload fixes below replace the initial warning-compatibility settings. These checks are separate from linking or running the radio.
- All 16 unit tests passed. New host tests execute the real audio adapter with mocked codec/hardware APIs, checking 8 kHz format, mono slot selection, reopen after sleep, initialization failures and partial I/O timeouts. A C++ test executes the real radio settings code against mocked NVS for all 163 HF/LF frequencies, menu indexes, save/load and range limits.
- GPIO consistency passed for all 12 projects. Public-content scan and inventory passed. Full repository validation still reports the preserved offline LVGL directory; full diff checking still reports 906 existing whitespace diagnostics in five LVGL files. Other tracked changes pass diff checking.

These are configuration, syntax and host regression checks, not complete firmware builds. IDF-to-IDF and IDF-to-Arduino voice, actual slot waveforms, gain, latency, PTT switching and sleep/wakeup remain hardware acceptance items. Test with the same active radio settings on both peers, then regenerate the board configuration and build before flashing.

Subsequent device logs show both accepted 240-byte packets and RadioLib error `-7`. In the bundled LR11x0 implementation that error represents payload CRC failure or an invalid header, so the logs alone do not identify the RF or scheduling cause. IDF 10 now records raw IRQ bits before they are cleared, RSSI, LoRa-only SNR and handled-packet counters, and restarts reception before audio queueing and console output. Invalid payloads remain excluded from Codec2. The updated `main.cpp` passed syntax checks on both SDKs above, and all 16 unit tests passed. New device logs and a comparison of peer settings, spacing and transmit power are still needed to establish the cause of intermittent integrity failures.

## Wrong board selection in IDF 10

On 2026-10-10, the failing IDF 10 configuration was found to contain generated `esp_vocat_1_2` board data. It enabled Board Manager's generic `dev_audio_codec.c`, which expects Codec 2.x APIs absent from the product's pinned Codec 1.5.11. The product YAML defines a custom audio device handled by the local ES8311/ES7210 adapter; upgrading the codec is not required to correct this failure.

The generated files were backed up and regenerated for `esp32_s3_lr1121_oled_1_54_xf`. Existing SDK settings were restored with the board selection/device/peripheral options taken from the correct generated defaults. All unrelated values were preserved; the only other removed option belonged to the now-excluded CST816S touch driver. The three product YAML files remain byte-for-byte unchanged. CMake configuration and the conditional dependency graph were refreshed after restoring settings.

All 12 projects now reject another board or incomplete generated defaults before component registration, with an explicit command to select this product. Host regression tests verify that those failures leave the generated CMake file unchanged and that correct-board regeneration remains idempotent.

The actual IDF 10 directory passed configuration with local IDF 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`, Board Manager 0.7.3~1 and Codec 1.5.11. Its generated audio configuration uses 8 kHz, two clock slots and slot-0 capture. The compile database includes the custom device and excludes `dev_audio_codec.c`, ST77916, CST816S and sources from the wrong board. Board Manager's button dependency is present after refreshing the graph.

Optimized assembly generation using the actual compiler commands passed for 75 sources with zero errors and warnings: 21 application/UI files, five product adapters, five generated files, 13 Board Manager sources and 31 codec/player/decoder sources. This checks compilation through `-S` only; firmware linking, flashing and voice interoperability remain unverified.

All 20 host tests, GPIO checks for 12 projects and the public-content scan of 337 maintained files passed. Inventory remains 12 IDF projects and 11 Arduino sketches, with no repository-boundary warnings. Full repository validation still rejects the preserved offline LVGL directory; full diff checking still reports 906 existing whitespace diagnostics in its five modified files. Diff checks pass outside that tree. No branch, commit or push was created.

## Audio-player dependency fix for IDF 6

The earlier application/board syntax checks did not compile the managed audio player's own source. A subsequent IDF 6.2 build exposed `audio_player.cpp` failing to include `driver/i2s_std.h`. This exact failure was reproduced with the previous 1.0.7 dependency. IDF 6 removed `driver`'s transitive I2S dependency; upstream audio-player 1.1.0 explicitly requires `esp_driver_i2s` and `esp_ringbuf` and retains the callback API used here.

All four audio projects (`04_microphone`, `05_speaker`, `10_walkie_talkie` and `12_factory_demo`) now pin 1.1.0, with the version recorded in `config/frameworks.json` and enforced by repository validation. No downloaded component source is patched or committed.

Independent copies of IDF 10 resolved 1.1.0 and passed CMake configuration on v5.5.5 and local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`. On each SDK, 42 source files passed actual compiler syntax checks: the audio player (including its new mixer), file iterator, Helix decoder, codec device drivers, two local audio adapters and application audio pipeline. The player's compiler commands include both I2S and ring-buffer header directories. All 16 host unit tests passed. The manifests of the other three audio projects are checked statically; complete firmware builds, playback and RF tests remain pending. Existing application SDK configurations, dependency locks and downloaded component trees are preserved; users must reconfigure to resolve the new dependency in their own build directory.

The subsequent optimized build exposed `-Werror=stringop-truncation` in audio-player 1.1.0's `audio_mixer.cpp`, which syntax-only checks do not diagnose. Its 15-byte name copy is immediately followed by explicit termination of the 16-byte destination. The maintained CMake compatibility file now creates a build-local patched mixer with a bounded `strnlen`/`memcpy` copy and replaces the original source on the player target. All four audio projects load it after component targets exist; repository validation checks their include paths and order. Downloaded sources are preserved. Remove the patch after an upstream release fixes the copy.

The exact diagnostic was reproduced with the 6.2 compiler using the project's optimization flags and `-S`. The first compatibility check covered 42 audio-related source files on both SDKs. The subsequent warning cleanup below replaces the temporary warning downgrade with a source fix and includes those files in a larger optimized check. These checks generate temporary assembly only and do not establish a complete firmware build or hardware result.

## Codec2 FFT initialization fix

A later IDF 6.2 build stopped at `esp32_codec2/src/fsk.c`, not at the RadioLib overload warnings shown near the end of the log. GCC reports `fftin` as potentially uninitialized at the `kiss_fft` call in `fsk_demod_freq_est`: the heap buffer was initialized only by a loop over a runtime FFT length. Both IDF 10 and the factory source now allocate that input with `calloc`. Valid FFT windows still overwrite every input sample as before; no warning suppression or radio protocol change is introduced. The two copies of `fsk.c` are identical, and the distinct Codec2 source variants are preserved.

The original failure was reproduced with the local IDF 6.2 compiler and the configured `-O2` flags. After the fix, all 69 Codec2 C sources in IDF 10 passed optimized assembly generation on ESP-IDF v5.5.5 and local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410`, with warnings-as-errors retained. Outputs were temporary assembly files; no firmware was linked or flashed. All 16 host tests and the 12-project GPIO consistency check passed. Inventory remains 12 IDF projects and 11 Arduino sketches. The preserved offline LVGL tree and its existing whitespace issues still prevent claiming full repository validation success.

## Build warning cleanup

The warning cleanup fixes inherited overload visibility in RadioLib's modem and print classes, adds RF69 forwarding overrides with the original argument defaults, and removes the earlier RadioLib warning downgrade/system-header settings. The audio-player patch described above replaces its temporary warning downgrade. All compiler warning policies remain enabled for these sources.

All 12 project CMake files normalize bmgr's generated source/include lists for this product's YAML-only board before component registration. The normalizer checks that the board directory contains exactly the three YAML files, preserves other generated content, tolerates initial generation being absent and works again after bmgr regenerates. No upstream pack or managed generator source is edited. IDF 10 uses `MINIMAL_BUILD` with its declared dependencies, excluding the unused Wi-Fi and Bluetooth components.

Using isolated project copies, the final checks on ESP-IDF v5.5.5 and local 6.2 development revision `08e0d30a74ad0bfd5a34933142b80f45619ee410` each generated optimized assembly for 143 source files with zero errors and zero compiler warnings. Coverage includes all 73 RadioLib sources, application/generated/local board sources, the audio player including its patched mixer, the file iterator, Helix decoder and codec drivers. Each command database contains exactly one mixer translation unit from the build tree, no legacy warning-downgrade flags and no Wi-Fi or WPA source entries. These are not complete firmware builds; the other projects' new CMake includes are checked statically.

All 19 host tests passed. The added checks cover real RadioLib header call forms, generated-board CMake regeneration/idempotence, preservation of unrelated sources and managed files, private-header access after source relocation, stream-name lengths through a full non-terminated 16-byte input, and an explicit failure if the vendor copy statement changes. Inventory remains 12 IDF projects and 11 Arduino sketches; the public-content scan covers 335 maintained text files. Whitespace checks pass outside the five previously modified LVGL files. Repository validation still rejects the preserved offline LVGL directory.

The final IDF 6.2 configuration emitted no CMake warnings. IDF 5.5 still reports a Component Manager retry (`Missing kconfig option`) before successful configuration, and SDK Kconfig notifications may remain. Framework sources and existing application SDK configuration files were not modified to hide these messages. Complete linking and hardware validation after this cleanup remain outstanding.

## LoRaWAN starter NVS declarations

IDF 11 already declared its `nvs_flash` component dependency but omitted the `nvs.h` and `nvs_flash.h` headers in `main.cpp`. It now includes both explicitly. Its global handle is named `lorawan_nvs_handle` to avoid colliding with the SDK's deprecated `nvs_handle` type alias in C++. The existing NVS namespace, keys and LoRaWAN persistence behavior are unchanged. Using IDF 11's actual configured IDF 6.2 compiler commands, its application, local board adapter and five generated board sources (seven files total) passed optimized assembly generation with zero errors and warnings. This check does not link firmware or exercise LoRaWAN join/session restoration on hardware.

## OLED example alignment with Arduino

IDF 08 displays the same static UI as Arduino 08: the SSD1309 title, Wi-Fi symbol, full-battery symbol and size label, with matching text, fonts and alignment offsets. Its UI creation calls were compared directly with the Arduino sketch; only the explicit display-handle screen lookup differs. The application uses the existing Board Manager/Adapter startup and LVGL lock. BOOT polling and image cycling were removed from the application, and the image array is excluded from the component source list. Legacy image descriptors and conversion tools remain on disk.

A separate copy of the project passed IDF 6.2 configuration. The generated compile-command database contains only `main.c` for the application and no `oled_images.c`. All eight application, local adapter and generated board source files passed optimized assembly generation with zero errors and warnings. All 19 host tests passed. These checks do not link firmware or verify the physical OLED's orientation and pixel polarity; a side-by-side hardware comparison remains pending.

## CI behavior

IDF has 36 jobs: 24 stable builds (12 projects × two stable versions) and 12 separately labeled builds against the pinned 6.2 development revision. Arduino has 11. Manual dispatch accepts `all`, a name or a path. Build workflows trigger for source/config/library/build-tool changes; documentation-only changes use the static workflow. Source artifacts are packaged only after successful builds, with version/name/target-specific artifact names. The later CI repair adds a separate tag-triggered Release gate using these same matrices.

## Outstanding acceptance

1. Resolve all managed dependencies in clean ESP-IDF v5.5.5 and v6.1 environments plus the exact 6.2 development revision, then compile all 36 entries.
2. Install the pinned Arduino packages and compile all 11 sketches with the recorded FQBN.
3. Exercise the 128 × 64 OLED orientation/polarity and confirm that IDF 08's title, Wi-Fi/battery icons and size label match Arduino 08. Verify power-key inversion, LVGL navigation and sleep/resume in the interactive examples. The supplied schematic shows GPIO4 behind Q6 with a high-active signal, while the current user board YAML selects active-low. The IDF generated code and GPIO constants follow the YAML; confirm actual board revision and measured polarity before changing it. Arduino polarity must also be checked against the actual board.
4. Verify STD/TDM audio clock sharing, walkie-talkie's 8 kHz slot-0 capture, other examples' four-slot capture, speaker rates, and radio/audio operation together on a real board.
5. Verify RTC alarms, the correct installed battery capacity/calibration, and SD read/write with a disposable test card.
6. Confirm actual HF/LF module, local RF region and antenna before exercising radio or LoRaWAN. Credentials remain external.
7. Package and flash one real source output from each framework; compare factory source/binary provenance separately if needed.
