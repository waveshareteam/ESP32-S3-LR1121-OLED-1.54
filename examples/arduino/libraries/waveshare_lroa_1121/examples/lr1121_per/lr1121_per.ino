/*Perform a Packet Error Rate (PER) test - both Tx and Rx roles*/
#include "lr1121_per.h"

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  per_test();
}

void loop() {
  delay(1000);
}