# LR1121 LoRa Ping-Pong Example

This example shows how to use the LR1121 radio with RadioLib in LoRa packet mode.

One node starts by transmitting `Hello World!`. After each transmit operation finishes, it switches to receive mode and waits for a response. When a packet is received, the example prints the payload, RSSI, and SNR, waits briefly, and transmits again.

## How to use example

### Hardware Required

* Two ESP32-S3-LR1121-OLED-1.54 boards
* Antennas connected to the LR1121 RF ports
* USB cable for flashing and serial monitor

This example is most useful with two boards. One board should initiate the ping-pong exchange, and the other board should start in receive mode.

### Configure the project

By default, `INITIATING_NODE` is defined in `main/main.cpp`:

```c
#define INITIATING_NODE
```

Use this setting on one board only. For the second board, comment out this line so it starts by listening.

Default radio settings:

* Frequency: 868.0 MHz
* Bandwidth: 125 kHz
* Spreading factor: 7
* Coding rate: 5
* Output power: 22 dBm
* Preamble length: 8
* Sync word: private LoRa sync word

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

## Example Output

On the initiating node:

```text
I (1234) lr1121_pingpong: [LR1121] Initializing ...
I (1240) lr1121_pingpong: success!
I (1250) lr1121_pingpong: [LR1121] Sending first packet ...
I (2345) lr1121_pingpong: transmission finished!
```

When a packet is received:

```text
I (3456) lr1121_pingpong: [LR1121] Received packet!
I (3457) lr1121_pingpong: [LR1121] Data: Hello World!
I (3458) lr1121_pingpong: [LR1121] RSSI: -42.0 dBm
I (3459) lr1121_pingpong: [LR1121] SNR: 9.5 dB
```

## Troubleshooting

* If both boards only transmit and never receive, make sure `INITIATING_NODE` is enabled on only one board.
* If initialization fails, check the LR1121 SPI pins, reset pin, busy pin, and interrupt pin in the board BSP.
* If packets are not received, verify that both boards use the same frequency and LoRa settings.
* Always connect an antenna before transmitting.
