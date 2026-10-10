/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
   RadioLib LR11x0 Ping-Pong Example

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#lr11x0---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>
#include <SPI.h>

// uncomment the following only on one
// of the nodes to initiate the pings
#define INITIATING_NODE

// LR1121 has the following connections:
// NSS pin:   10
// IRQ pin:   2
// NRST pin:  3
// BUSY pin:  9

LR1121 radio = new Module(42, 38, 39, 41, SPI, SPISettings(8 * 1000 * 1000, MSBFIRST, SPI_MODE0));

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

// set RF switch configuration for Wio WM1110
// Wio WM1110 uses DIO5 and DIO6 for RF switching
// NOTE: other boards may be different!
static const uint32_t rfswitch_dio_pins[] = {
  RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
  RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  // mode                  DIO5  DIO6
  { LR1121::MODE_STBY,   { LOW,  LOW  } },
  { LR1121::MODE_RX,     { HIGH, LOW  } },
  { LR1121::MODE_TX,     { LOW,  HIGH } },
  { LR1121::MODE_TX_HP,  { LOW,  HIGH } },
  { LR1121::MODE_TX_HF,  { LOW,  LOW  } },
  { LR1121::MODE_GNSS,   { LOW,  LOW  } },
  { LR1121::MODE_WIFI,   { LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

static bool tx_pending = false;
static uint32_t tx_due_ms = 0;

static void halt_on_error(void) {
  while (true) {
    yield();
  }
}

static bool checkRadioState(int state, const __FlashStringHelper *operation) {
  if (state == RADIOLIB_ERR_NONE) {
    return true;
  }

  Serial.print(operation);
  Serial.print(F(" failed, code "));
  Serial.println(state);
  return false;
}

// this function is called when a complete packet
// is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we sent or received a packet, set the flag
  operationDone = true;
}

void setup() {
  Serial.begin(115200);

  SPI.begin(40, 46, 45, -1);

  // initialize LR1121 with default settings
  Serial.print(F("[LR1121] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    halt_on_error();
  }

  if (!checkRadioState(radio.setFrequency(868.0), F("setFrequency")) ||
      !checkRadioState(radio.setOutputPower(22, true, RADIOLIB_LRXXXX_PA_RAMP_48U), F("setOutputPower")) ||
      !checkRadioState(radio.setBandwidth(125.0, false), F("setBandwidth")) ||
      !checkRadioState(radio.setSpreadingFactor(7), F("setSpreadingFactor")) ||
      !checkRadioState(radio.setCodingRate(5), F("setCodingRate")) ||
      !checkRadioState(radio.setSyncWord(RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE), F("setSyncWord")) ||
      !checkRadioState(radio.setPreambleLength(8), F("setPreambleLength"))) {
    halt_on_error();
  }

  // set RF switch control configuration
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  // set the function that will be called
  // when new packet is received
  radio.setIrqAction(setFlag);

  #if defined(INITIATING_NODE)
    // send the first packet on this node
    Serial.print(F("[LR1121] Sending first packet ... "));
    transmissionState = radio.startTransmit("Hello World!");
    if (!checkRadioState(transmissionState, F("startTransmit"))) {
      halt_on_error();
    }
    transmitFlag = true;
  #else
    // start listening for LoRa packets on this node
    Serial.print(F("[LR1121] Starting to listen ... "));
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      halt_on_error();
    }
  #endif
}

void loop() {
  if (tx_pending && (millis() - tx_due_ms) < UINT32_MAX / 2) {
    tx_pending = false;
    Serial.print(F("[LR1121] Sending another packet ... "));
    transmissionState = radio.startTransmit("Hello World!");
    if (!checkRadioState(transmissionState, F("startTransmit"))) {
      halt_on_error();
    }
    transmitFlag = true;
  }

  // check if the previous operation finished
  if (operationDone) {
    // reset flag
    operationDone = false;

    if (transmitFlag) {
      // the previous operation was transmission, listen for response
      // print the result
      if (transmissionState == RADIOLIB_ERR_NONE) {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));

      } else {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);

      }

      // listen for response
      int state = radio.startReceive();
      if (!checkRadioState(state, F("startReceive"))) {
        halt_on_error();
      }
      transmitFlag = false;

    } else {
      // the previous operation was reception
      // print data and send another packet
      uint8_t rxBuffer[256];
      size_t rxLen = radio.getPacketLength();
      if (rxLen > sizeof(rxBuffer) - 1) {
        rxLen = sizeof(rxBuffer) - 1;
      }
      int state = radio.readData(rxBuffer, rxLen);
      rxBuffer[rxLen] = '\0';

      if (state == RADIOLIB_ERR_NONE) {
        // packet was successfully received
        Serial.println(F("[LR1121] Received packet!"));

        // print data of the packet
        Serial.print(F("[LR1121] Data:\t\t"));
        Serial.println((const char *)rxBuffer);

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[LR1121] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[LR1121] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

      }

      tx_due_ms = millis() + 1000;
      tx_pending = true;
    }

  }
  yield();
}
