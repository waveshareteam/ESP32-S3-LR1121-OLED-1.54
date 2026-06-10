# SSD1309 OLED Example

This example shows how to start the onboard 1.54-inch SSD1309 OLED display and draw a simple LVGL user interface.

The application starts the BSP display driver, locks the LVGL display, creates text and symbol objects, and then unlocks the display. It is useful for verifying the display panel, LVGL configuration, and board display wiring.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* USB cable for power, flashing, and serial monitor

No external display wiring is required when using the onboard OLED.

### Configure the project

No menuconfig changes are required for the default board configuration.

The UI is created in `lvgl_oled_demo()` in `main/main.c`. The default screen includes:

* Title text: `SSD1309 128x64`
* Wi-Fi symbol
* Battery symbol
* Bottom label: `1.54 inch OLED`

### Build and Flash

Run the following commands from this example directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port used by your board.

To exit the serial monitor, use `Ctrl+]`.

## Expected Result

After flashing, the OLED should show a simple LVGL screen with a title, Wi-Fi icon, battery icon, and bottom information label.

## Troubleshooting

* If the display is blank, check that `bsp_display_start()` succeeds and the board has the expected OLED hardware.
* If LVGL symbols do not appear, verify that the selected fonts and symbols are enabled in `sdkconfig.defaults`.
* If the screen is rotated or shifted, check the display rotation setting in the board BSP.
