# Button Example

This example shows how to use the board buttons with the ESP-IDF button component.

The application creates GPIO button devices, registers click callbacks, and prints button events to the serial console. It is useful for verifying the button GPIO mapping and checking that the button driver is working correctly on the board.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* USB cable for power, flashing, and serial monitor

The example uses these GPIOs:

* GPIO0
* GPIO4
* GPIO5
* GPIO6

GPIO6 also registers a double-click callback.

### Configure the project

No menuconfig changes are required for the default board configuration.

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

After flashing, press the buttons on the board. The monitor prints the button event name and GPIO number:

```text
I (1234) main: BUTTON_SINGLE_CLICK :0
I (2345) main: BUTTON_SINGLE_CLICK :5
I (3456) main: BUTTON_DOUBLE_CLICK :6
I (4567) main: BUTTON_SINGLE_CLICK :4
```

For repeat, long-press, or multiple-click events, the callback also prints extra event information when those events are reported by the button driver.

## Troubleshooting

* If no events are printed, check that the board is powered correctly and the serial monitor is connected to the right port.
* If a button always appears pressed or never triggers, verify the GPIO active level in `main/main.c`.
* If the build cannot find the button component, run `idf.py reconfigure` or remove `managed_components` and build again.
