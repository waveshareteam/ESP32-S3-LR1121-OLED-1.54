# LoRaWAN Starter Example

This example shows how to connect the LR1121 radio to a LoRaWAN network using RadioLib OTAA.

The application initializes the radio, restores LoRaWAN session data from NVS when available, joins the network with OTAA, periodically sends a small uplink payload, waits for a downlink, and stores updated LoRaWAN state back to NVS.

## How to use example

### Hardware Required

* ESP32-S3-LR1121-OLED-1.54 board
* Antenna connected to the LR1121 RF port
* LoRaWAN gateway coverage for the configured region
* USB cable for flashing and serial monitor

### Configure the project

Edit `main/config.h` before building.

Set the LoRaWAN join EUI, device EUI, app key, and network key:

```c
#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000
#define RADIOLIB_LORAWAN_DEV_EUI   0x0000000000000000
#define RADIOLIB_LORAWAN_APP_KEY   0x00, 0x00, 0x00, 0x00, ...
#define RADIOLIB_LORAWAN_NWK_KEY   0x00, 0x00, 0x00, 0x00, ...
```

The default region is:

```c
const LoRaWANBand_t Region = EU868;
const uint8_t subBand = 0;
```

Change these values if your LoRaWAN network uses a different region or sub-band.

Make sure the selected LoRaWAN region is allowed in your country or region.

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

On startup, the monitor prints setup, join, and uplink status:

```text
I (1234) main: [Setup] Starting...
I (1240) main: [Radio] Initializing...
I (2345) main: [LoRaWAN] Joining network...
I (6789) main: [NVS] State saved
I (6790) main: [LoRaWAN] Ready!
I (12345) main: [LoRaWAN] Sending uplink...
```

If a downlink is received, the example prints the receive window, length, HEX dump, and ASCII dump:

```text
I (13000) main: [LoRaWAN] Downlink received
I (13001) main: RX Window: 1
I (13002) main: Length: 4
[HEX]: 01 02 03 04
[ASCII]: ....
```

If no downlink is received:

```text
I (13000) main: [LoRaWAN] No downlink received
```

## Troubleshooting

* If joining fails, verify the join EUI, device EUI, app key, network key, and LoRaWAN region.
* If the device joins once but fails after reset, erase NVS or update the device registration in the network server.
* If no uplinks appear in the network server, check gateway coverage and antenna connection.
* Always connect an antenna before transmitting.
