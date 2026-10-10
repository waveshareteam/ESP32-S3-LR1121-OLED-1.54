# Walkie-Talkie Example

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md) and select `esp32_s3_lr1121_oled_1_54_xf` with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`.

This example implements a simple half-duplex voice walkie-talkie using the onboard microphone, speaker, OLED display, and LR1121 radio.

The application captures audio from I2S, compresses it with Codec2, sends compressed voice frames through the radio, receives voice frames from the other node, decodes them, and plays them through the speaker. The OLED UI shows radio and system status and allows radio settings to be changed.

## How to use example

### Hardware Required

* Two ESP32-S3-LR1121-OLED-1.54 boards
* Antennas connected to the LR1121 RF ports
* USB cable for flashing and serial monitor

This example requires two boards running compatible settings.

No SD card is required. Voice packets are buffered in RAM and UI images are compiled into the firmware. Startup does not initialize or mount the SD card, avoiding card-detection timeouts when no card is present.

### Configure the project

No menuconfig changes are required for the default board configuration.

Default radio settings are defined in `main/app/app_state.cpp` and `main/app/radio_settings.cpp`. Both boards must use the same modem and RF settings to communicate. The first-boot default is GFSK at 868 MHz, 50 kbps, 25 kHz deviation and 117.3 kHz RX bandwidth. Saved settings take precedence; the serial log prints the actual configuration after initialization and menu changes. HF and LF frequency selection both use 1 MHz steps, matching the Arduino example and its saved NVS values.

The default LoRa settings include:

* Frequency: 868.0 MHz
* Bandwidth: 125 kHz
* Spreading factor: 7
* Coding rate: 5
* Output power: 22 dBm

Make sure the frequency is allowed in your region before transmitting.

### Voice format

Codec2 mode 3200 requires 8 kHz, 16-bit mono PCM: 160 samples per 20 ms frame, 8 encoded bytes per frame, and 30 frames per 240-byte radio packet (600 ms of speech). This matches `examples/arduino/10_walkie_talkie`. Hold PTT while speaking for more than one second to allow the initial microphone settling frames and the first packet to complete.

This example's Board Manager YAML sets both I2S directions to 8 kHz. ES8311 STD output and ES7210 TDM input share the two-slot clock; only microphone slot 0 enters the capture buffer. The codec-open and wakeup paths preserve that selection. Four interleaved microphone channels must not be passed to the mono encoder. Regenerate the board component after updating YAML, including when rebuilding an existing checkout.

### Build and Flash

Run the following commands from this example directory:

```bash
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port used by your board.

To exit the serial monitor, use `Ctrl+]`.

## Expected Operation

After startup, the board initializes:

* Battery monitor
* RTC
* LR1121 radio
* Audio codec
* OLED UI
* Codec2 audio task
* Audio playback task

The main loop refreshes the cached RTC and battery information every 200 ms. The LVGL UI timer reads the cached values and updates the on-screen time and battery status every 300 ms.

When the push-to-talk state is active, the board captures and transmits compressed voice frames. When push-to-talk is released, the board returns to receive mode and plays received voice frames.

| Button | Function |
|---|---|
| BOOT / GPIO0 | Previous setting; decrease the selected value while editing |
| Power / GPIO4 | Next setting; increase the selected value while editing |
| Menu / GPIO5 | Open the focused setting; confirm and return to the dashboard |
| PTT / GPIO6 | Hold to transmit, release to receive |

Use short presses for menu navigation. The power key also connects to the hardware power controller. After automatic light sleep, the first press wakes the board; release it before navigating again.

## Example Output

The serial monitor prints radio and audio status:

```text
I (1234) radio: GFSK 868.0 MHz, bitrate=50.0 kbps, deviation=25.0 kHz, RX BW=117.3 kHz, power=22 dBm
Codec2 init OK | sample:160 compress:8
I (1500) audio_pipeline: PTT capture started: 8000 Hz, mono, 160 samples/frame
[LR1121] Sending packet...
[LR1121] Packet received! Length: 240 bytes
```

## Troubleshooting

If configuration reports `Wrong generated board`, or compilation fails in `dev_audio_codec.c` with unknown `audio_hw_sys_cfg_t` / `audio_codec_cfg_t` types, check the selected board. Selecting `esp_vocat_1_2` enables another product's generic audio driver, which requires Codec 2.x. This product uses the custom `audio` device and its local ES8311/ES7210 adapter with Codec 1.5.11. Select the product explicitly from this example directory (**Ubuntu Bash**):

```bash
idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf
idf.py reconfigure
idf.py build
```

Changing boards backs up and resets `sdkconfig`; see the [configuration preservation notes](../../../docs/board-manager.md#enable-bmgr-with-esp-idf-55-or-6x). Reapply unrelated custom settings without restoring another board's `CONFIG_ESP_BOARD_*` options. Do not upgrade the codec or edit downloaded drivers to accommodate the wrong board.

* If there is no received packet, compare the actual `radio:` logs on both boards: modem and frequency, plus bitrate/deviation/RX bandwidth for GFSK or SF/bandwidth/coding rate for LoRa. Flashing does not necessarily erase saved settings.
* `state: -7` is RadioLib's CRC/header integrity error. A 240-byte length does not prove the payload is valid; the packet is intentionally discarded. RX logs include RSSI, LoRa-only SNR, raw IRQ flags captured before `readData()` clears them, and cumulative accepted/integrity/other error counts. These counters describe handled receive events, not the number of transmissions lost on air. GFSK has no SNR measurement here and prints `n/a`.
* For intermittent integrity errors, compare both nodes' active settings and antenna connections. For a close-range bench test, use the menu to lower TX power to 0 or 5 dBm and separate the antennas by a few metres; compare the new diagnostics with the original setup. Excessive input level, weak signal, interference and receive servicing delays are hypotheses until measured. Keep integrity checking enabled; accepting damaged Codec2 frames can produce loud audio artifacts.
* If radio initialization fails, check the LR1121 SPI and control pins in this example's Board Manager YAML.
* If audio is noisy or missing, regenerate the board files and confirm 8 kHz in the I2S initialization logs, `total_slot: 2` and `slot_mask: 0x1` for RX. Codec initialization errors now stop startup instead of running silent tasks.
* During PTT, `TX frames`, `processed_peak` and `mic_fail` distinguish encoded audio from failed microphone reads. `processed_peak=0` throughout speech indicates missing or gated input. Received packet logs with speaker write errors indicate a playback problem. These counters are diagnostics, not proof of correct physical audio.
* If PTT works but all three navigation keys do not, rebuild with the current shared display component. It enables timer-based input reading for Adapter 0.7.0; PTT uses an independent button callback.
* Always connect an antenna before transmitting.
