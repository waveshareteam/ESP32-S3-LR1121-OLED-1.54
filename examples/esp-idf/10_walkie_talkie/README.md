# Walkie-Talkie Example

This example implements a simple half-duplex voice walkie-talkie using the onboard microphone, speaker, OLED display, and LR1121 radio.

The application captures audio from I2S, compresses it with Codec2, sends compressed voice frames through the radio, receives voice frames from the other node, decodes them, and plays them through the speaker. The OLED UI shows radio and system status and allows radio settings to be changed.

## How to use example

### Hardware Required

* Two ESP32-S3-LR1121-OLED-1.54 boards
* Antennas connected to the LR1121 RF ports
* USB cable for flashing and serial monitor

This example requires two boards running compatible settings.

### Configure the project

No menuconfig changes are required for the default board configuration.

Default radio settings are defined in `main/main.cpp` and the UI state in `main/ui/ui.cpp`. The example supports LoRa and GFSK configuration paths. Both boards must use the same modem and RF settings to communicate.

The default LoRa settings include:

* Frequency: 868.0 MHz
* Bandwidth: 125 kHz
* Spreading factor: 7
* Coding rate: 5
* Output power: 22 dBm

Make sure the frequency is allowed in your region before transmitting.

### Build and Flash

Run the following commands from this example directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port used by your board.

To exit the serial monitor, use `Ctrl+]`.

## Expected Operation

After startup, the board initializes:

* Battery monitor
* RTC
* SD card
* LR1121 radio
* Audio codec
* OLED UI
* Codec2 audio task
* Audio playback task

The main loop refreshes the cached RTC and battery information every 200 ms. The LVGL UI timer reads the cached values and updates the on-screen time and battery status every 300 ms.

When the push-to-talk state is active, the board captures and transmits compressed voice frames. When push-to-talk is released, the board returns to receive mode and plays received voice frames.

## Example Output

The serial monitor prints radio and audio status:

```text
I (1234) app_main: Radio modem: LoRa
Codec2 init OK | sample:160 compress:8
[LR1121] Sending packet...
[LR1121] Packet received! Length: 240 bytes
```

## Troubleshooting

* If there is no received audio, make sure both boards use the same modem, frequency, bandwidth, spreading factor, and coding rate.
* If radio initialization fails, check the LR1121 SPI and control pins in the board BSP.
* If audio is noisy or missing, verify the onboard codec initialization and speaker output.
* Always connect an antenna before transmitting.
