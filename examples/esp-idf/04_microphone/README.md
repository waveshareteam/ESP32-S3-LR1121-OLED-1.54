# Microphone Example

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md) and select `esp32_s3_lr1121_oled_1_54_xf` with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`.

This example shows how to initialize the onboard audio codec and read microphone samples through I2S.

The application configures the codec input gain, continuously reads audio frames, and prints a small set of left/right sample values to the serial console. It can be used to verify the microphone path before integrating audio recording or voice-processing features.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* USB cable for power, flashing, and serial monitor

No external microphone wiring is required when using the onboard microphone circuit.

### Configure the project

No menuconfig changes are required for the default board configuration.

The default example sets the codec gain in `main/main.c`:

```c
board_extra_codec_gain_set(30, NULL);
```

Adjust this value if the printed samples are too small or clipped.

### Build and Flash

Run the following commands from this example directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port used by your board.

To exit the serial monitor, use `Ctrl+]`.

## Example Output

The monitor prints a group of sampled values from the audio frame:

```text
Audio L/R: 12,-8 | 15,-9 | 20,-7 | 18,-6 |
Audio L/R: 11,-7 | 13,-8 | 19,-5 | 21,-6 |
```

The values change with sound near the microphone.

## Troubleshooting

* If the sample values do not change, check that the codec initialization succeeds and that the microphone input is enabled on the board.
* If samples are always near the maximum or minimum value, reduce the codec gain.
* If the build cannot find `board_extra.h`, check the example's `components/esp32_s3_lr1121_oled_1_54_support/include/` and the `esp32_s3_lr1121_oled_1_54_support` dependency in `main/CMakeLists.txt`.