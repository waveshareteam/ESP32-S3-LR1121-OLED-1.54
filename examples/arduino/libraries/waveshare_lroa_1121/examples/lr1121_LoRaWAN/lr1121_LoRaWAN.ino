#include "lr1121_LoRaWAN.h"

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  lr1121_LoRaWAN_init();

}

void loop() {

  lr1121_LoRaWAN_loop();
  delay(1);
}