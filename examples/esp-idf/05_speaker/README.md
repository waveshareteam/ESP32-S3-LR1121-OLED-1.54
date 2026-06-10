# Speaker MP3 Player Example

This example shows how to play MP3 files from a microSD card through the onboard speaker output.

The application mounts the SD card, scans `/sdcard/music` for `.mp3` files, starts playback, and uses board buttons for track and volume control.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* microSD card
* MP3 files
* USB cable for power, flashing, and serial monitor

Prepare the SD card with this directory:

```text
/music
```

Copy one or more `.mp3` files into the `music` directory.

### Button Controls

The example uses these buttons:

* GPIO0: next track
* GPIO4: previous track
* GPIO5: play or pause
* GPIO6 single click: volume up
* GPIO6 double click: volume down

### Configure the project

No menuconfig changes are required for the default board configuration.

The default volume starts from `CODEC_DEFAULT_VOLUME` and is changed in steps of 5.

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

When the SD card is mounted and MP3 files are found, the monitor prints:

```text
=== Scanning directory: /sdcard/music ===
Found MP3 [0]: track01
Found MP3 [1]: track02
=== Scan done! Total MP3: 2 ===
I (1234) main: System ready, waiting key input...
```

Button actions print the selected track or playback state:

```text
I (2345) main: Switched to Next: track02 (Pause: 0)
I (3456) main: Playback Paused
I (4567) main: Playback Resumed
```

## Troubleshooting

* If no MP3 files are found, check that the files are under `/music` on the SD card and use the `.mp3` extension.
* If the SD card does not mount, check the card format and board SD card slot connection.
* If there is no sound, check the speaker connection and codec volume.
