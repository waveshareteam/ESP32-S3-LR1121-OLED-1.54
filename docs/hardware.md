# Product hardware evidence

The local [schematic](../hardware/schematics/esp32-s3-lr1121-oled-1_54_schematics.pdf), the original product demos, and the supplied Waveshare HF product page were cross-checked. No definitions from unrelated LCD/touch products are used. The schematic labels the digital board ESP32-S3-LR1121-OLED-1.54; it does not establish a separate XF radio front-end SKU. Select the actual HF/LF module and regional RF parameters before transmitting.

| Function | Verified connection / setting |
|---|---|
| MCU/memory | ESP32-S3R8; external 128-Mbit Flash, 8 MB octal PSRAM |
| Shared I2C | SDA GPIO13, SCL GPIO14; OLED 0x3C, ES8311 0x18, ES7210 0x40, RTC 0x51, fuel gauge 0x55 (7-bit addresses) |
| OLED | SSD1309, I2C at 400 kHz, 128 × 64, 1 bpp, page/vertical tiled output; no touch or backlight |
| OLED reset/power | Reset pulled up in hardware; no MCU reset pin. Dedicated boost supply, no application GPIO power sequence |
| OLED orientation/buffers | Original driver orientation, application rotation 0; one full-screen LVGL I1 buffer (1024 + 8 palette bytes) and one adapter conversion buffer (1024 bytes); no RGB sync timings or tearing signal |
| LR1121 | SPI3: SCLK40, MOSI45, MISO46; CS42, IRQ38, reset39, BUSY41; mode 0 at 8 MHz from original demo |
| Audio | MCLK17, BCLK18, LRCLK21, ES8311 data out16, ES7210 data in15; amplifier GPIO8 |
| Audio slots | STD mono TX and TDM RX share I2S0 clock pins. Walkie-talkie uses 8 kHz with two physical slots and captures only slot 0, matching the working Arduino example; other audio examples retain their original four-slot format. Runtime clock alignment remains a hardware acceptance item |
| SDMMC | CLK12, CMD10, D0=11; one-bit bus, default 20 MHz; no automatic formatting |
| Buttons | BOOT0, menu5 and PTT6 active low; power4 active high through Q6 inverter |
| RTC alarm | GPIO7, active low |
| USB | D−19, D+20; hardware USB Serial/JTAG console |
| Exposed UART | TX43/RX44 on header; no maintained example consumes this UART, so it is not initialized automatically |
| Vibration | GPIO9 exists in the schematic; no maintained example uses it |

The RF-switch DIO5/DIO6 truth table comes from the original Core1121 demo. The external module's RF layout and antenna matching are outside the digital schematic. Frequency, bandwidth, spreading factor and transmit power are application settings, not common-board pin definitions.

The schematic shows a power-button inverter. The current user board YAML selects GPIO4 active-low; generated IDF button configuration and wakeup constants follow that setting. Confirm the actual board revision and measured level before changing the YAML or aligning Arduino polarity. GPIO4 uses the schematic's external R66 pull-up; internal pulls are disabled. The OLED's display-on/off commands are retained behind compatibility names containing `backlight`; there is no physical backlight to configure.
