# PCF85063 RTC Example

Before the first build, complete the [Board Manager setup](../../../docs/board-manager.md) and select `esp32_s3_lr1121_oled_1_54_xf` with `idf.py bmgr -b esp32_s3_lr1121_oled_1_54_xf`.

This example shows how to use the onboard PCF85063 RTC.

The application initializes the RTC, sets a default date and time, configures an alarm, and prints the current time once per second. It also registers the RTC alarm GPIO as a button input so the alarm flag can be observed from the serial console.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* USB cable for power, flashing, and serial monitor

No external RTC wiring is required when using the onboard PCF85063 circuit.

### Configure the project

No menuconfig changes are required for the default board configuration.

The default time and alarm are set in `main/main.c`:

```c
pcf85063a_datetime_t rtc_time = {
    .year = 2026,
    .month = 04,
    .day = 16,
    .dotw = 4,
    .hour = 14,
    .min = 49,
    .sec = 26,
};
```

Update these values if you want to test a different date, time, or alarm point.

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

The monitor prints the RTC time every second:

```text
I (1234) main: RTC time: 2026-04-16 14:49:26
I (2234) main: RTC time: 2026-04-16 14:49:27
I (3234) main: RTC time: 2026-04-16 14:49:28
```

When the alarm GPIO is triggered, the callback prints:

```text
I (4234) main: Alarm flag pressed
```

## Troubleshooting

* If the time does not update, check that the RTC initialization succeeds.
* If the alarm callback does not print, verify the RTC alarm GPIO and active level in `main/main.c`.
* If the time resets after power loss, check the RTC backup power source.
