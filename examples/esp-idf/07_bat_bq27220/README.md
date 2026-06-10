# BQ27220 Battery Gauge Example

This example shows how to read battery information from the onboard BQ27220 fuel gauge.

The application initializes the battery gauge and prints voltage, current, state of charge, remaining capacity, full charge capacity, and temperature once per second.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* Battery connected to the board battery connector
* USB cable for power, flashing, and serial monitor

### Configure the project

No menuconfig changes are required for the default board configuration.

The battery gauge is initialized with a 1000 ms update interval:

```c
bsp_bat_init(1000);
```

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

The monitor prints battery data in this format:

```text
I (1234) main: [BAT] 3890mV -120mA SOC:82% REM:720mAH FCC:900mAH TMP:28 C
I (2234) main: [BAT] 3888mV -118mA SOC:82% REM:719mAH FCC:900mAH TMP:28 C
```

Current can be positive or negative depending on whether the battery is charging or discharging.

## Troubleshooting

* If all values are zero or invalid, check that the battery is connected and the gauge is powered.
* If the state of charge looks incorrect, the gauge may need a full charge/discharge learning cycle.
* If the example cannot communicate with the gauge, verify the board I2C bus and BQ27220 address.
