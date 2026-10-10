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
- `BatteryGauge.*`: checked BQ27220 capacity initialization and I2C access
- `CodecDrivers.*`: ES8311 and ES7210 I2C setup
- `AudioPipeline.*`: I2S, Codec2, audio/radio packet buffers
- `RadioRuntime.*`: LR1121 configuration, TX/RX, IRQ handling
- `WalkieRadio.h`: local LR1121 TX-start BUSY timeout
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
- Flash Size: 16MB (128Mb)
- PSRAM: OPI PSRAM
- Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)

Behavior matched from the IDF example:

- Same LR1121 GFSK/LoRa radio settings and defaults
- Same 240-byte Codec2 voice packet size
- Same TX silence threshold, automatic gain control, and playback scaling as the IDF example. Quiet TX frames are zero-filled and still encoded, keeping packet assembly continuous while PTT is held. Playback has no extra noise gate that could suppress quiet received speech.
- Same PTT TX/RX behavior
- Same BOOT/PWR/MENU navigation model
- Same 128x64 UI layout, with 20 ms animated/scrolling frames and 300 ms steady status refresh cadence
- Same 10 ms main loop delay
- RTC, battery percentage, and charge-current polling at 1 s intervals

Notes:

- `ENABLE_DEBUG_LOG` at the top of `Config.h` is the master switch for sketch serial logs: **0 = off (default), 1 = on**. Change it to `1`, rebuild/upload, and open the serial monitor at **115200 baud** to debug startup, PTT, audio, radio, battery, and sleep/wake behavior. With `0`, log calls and their arguments are compiled out, and sketch serial initialization/flush are skipped. ESP32 ROM/bootloader messages and separately enabled framework/library logs are outside this switch.
- As in the ESP-IDF example, 30 seconds without button or radio activity enters light sleep. The OLED turns off, the amplifier and audio codecs suspend, and the LR1121 remains in receive mode. Button presses, active transmission, and pending/background configuration keep the board awake.
- BOOT, MENU, power, or PTT can wake the board; the first press only wakes it. Release the key, then press again to navigate or transmit. An LR1121 receive interrupt also wakes the board so incoming voice can be processed.
- `LOW_POWER_IDLE_TIMEOUT_MS` in `Config.h` controls the idle timeout. `POWER_BUTTON_ACTIVE_LEVEL` is shared by menu input and sleep/wake detection, matching the current IDF example's low-active power-key configuration. Serial logs beginning with `[Sleep]` report entry, wakeup, or cancellation if wake sources cannot be configured.
- MENU confirms the selected option even while it is scrolling, then immediately returns to the main page. Confirming an unchanged option skips saving and radio reconfiguration.
- A single background task handles radio settings, saving, PTT mode changes, and TX/RX commands, so radio waits do not block menu input or OLED refresh. Further confirmations while it is busy are combined into the latest settings snapshot. Light sleep waits for all pending radio work; a packet already being transmitted is allowed to finish first.
- TX startup has a one-second BUSY timeout, and a missing TX-done interrupt is recovered using a deadline based on the configured packet airtime. Runtime radio errors are logged and retried without stopping the UI.
- Audio processing yields and checks PTT/sleep state on every Codec2 frame. The outgoing audio queue is capped at ten packets to avoid unbounded allocation during a long press at a slow radio data rate.
- USB serial writes do not wait for the host to consume logs. Output may be dropped when the serial monitor is closed or slow.
- Parameter changes within the same modem update only the changed LR1121 parameters without resetting it. Switching between GFSK and LoRa still performs a full initialization. Frequency changes retain RadioLib's image calibration and PA setup.
- Only changed settings are saved; switching bands also saves the resulting frequency. Serial reconfiguration logs report `config=update` or `config=full`, `elapsed=...ms` for radio configuration, and `save=...ms` for persistence. Wait for this log before powering off after changing settings.
- The sketch uses `Arduino_Canvas_Mono` with `Arduino_SSD1306`. SSD1309 is command-compatible enough for this OLED path. If the panel is shifted on a specific module, try replacing `Arduino_SSD1306` with `Arduino_SH1106` in the sketch.
- The battery charge icon follows the BQ27220 signed net `Current` register: current greater than 0 mA means charging. A failed percentage read shows `--%` without suppressing a valid charging indication; a failed current read clears the charging indication until readings recover. Zero current alone does not establish that the battery is charging.
- SOC and current are read together once per second, with a 100 microsecond bus-free guard before and after each gauge transaction. Startup/wake reads and failed attempts share the same rate limit. This follows the [BQ27220 datasheet, section 7.3.1.3](https://www.ti.com/lit/ds/symlink/bq27220.pdf), which requires at least 66 microseconds between packets and warns that reading standard commands more than twice per second can cause a gauge watchdog reset. The previous separate 300 ms current polling path exceeded that limit.
- `BATTERY_DESIGN_CAPACITY_MAH` is set to **1000 mAh** to match the installed battery label. Setup schedules the gauge check, then starts the display and radio without waiting for it. The main loop performs one short initialization step at a time; settling and polling waits use deadlines so buttons and PTT remain responsive. The percentage initially shows `--%` and starts updating after the check completes. Light sleep waits for the check and cleanup to finish. `[Boot] ready=...ms` measures startup separately from `[Battery] ... elapsed=...ms`.
- The gauge check only seeds design/full-charge capacity when the design capacity differs. Subsequent boots preserve learned full-charge capacity. It enables CEDV `CSYNC` (remaining capacity follows full-charge capacity at valid charge termination) and `SC` (independent charger), preserving other gauge parameters. Each write temporarily uses 100 kHz and restores normal I2C speed before returning to the main loop. Readback verification, rollback and access-state cleanup also run in steps. The check runs once at startup, not on light-sleep wake.
- Capacity configuration is held in gauge RAM and is checked again at startup because removing battery power can restore the gauge defaults. The charge LED belongs to the charger; it does not directly set the gauge's SOC or full-charge flag. A correctly configured battery may still need a normal discharge/recharge cycle for the gauge to recognize charge termination. The UI continues to display measured SOC, without forcing 100% at zero current. Battery-specific chemistry and taper thresholds are not changed by the capacity setting.
- `BATTERY_CHARGE_DEBUG_LOG` follows the master switch and enables `[Battery]` diagnostics when logging is on: `current` is in mA, `current_ok` and `soc_ok` indicate successful reads, and `charging` controls the icon. `voltage`, `remaining`, `fcc`, and `full` report battery voltage, remaining capacity, full-charge capacity, and the gauge's FC flag; `details_ok` validates those fields. These values share the current's sequential register read, without increasing polling frequency. A value whose corresponding `_ok` is 0 is an old sample. Startup logs also report the previous capacity settings and the verified design capacity. Logs appear on state changes and once per second while awake. Set the option to `false` after checking the charging indication.
- Battery icon changes use a direct OLED page update for the top-right status area, so charge/discharge changes do not wait for a full 128x64 redraw.
- The ES7210 path is initialized directly over I2C because there is no Arduino ES7210 library in the supplied library folder.
- The I2S code requires Arduino-ESP32 3.x because it uses the ESP-IDF v5 `driver/i2s_std.h` and `driver/i2s_tdm.h` APIs.
- While PTT is held, serial logs show the TX audio queue, encoded Codec2 frames, muted frames, mic read failures, raw/processed mic level, silence threshold, voice detection, and last I2S read result. `muted` frames are encoded as silence, not dropped; failed microphone reads are skipped. Radio failures include the operation number (0: configure, 1: PTT mode, 2: start TX, 3: finish TX, 4: receive), error code, and BUSY/IRQ pin levels.
- `VOICE_DEBUG_LOG` follows the master switch and, when logging is on, adds diagnostics every two seconds while awake. Compare both peers' `[LinkConfig]` lines to check the modem, frequency, SF/BW/CR or GFSK parameters. `[Link]` reports successful TX starts/finishes, TX completion timeouts, accepted/dropped RX packets, and the last received length/read error. A successful TX finish is local cleanup, not an acknowledgement from the other device. RX errors and unexpected lengths are logged before resuming receive mode.
- `[AudioStatus]` reports Codec2/speaker readiness, encoded/decoded frame counts, I2S write failures, and free heap/PSRAM. `play_frames` includes silence written while idle. Audio startup failures are also shown on the OLED instead of silently leaving a working UI without voice. Error codes: 1 = Codec2 allocation, 2 = PCM/bit-buffer allocation, 3 = Codec2 task creation, 4 = speaker task creation. PSRAM disabled is a configuration difference, not proof of an allocation failure.
