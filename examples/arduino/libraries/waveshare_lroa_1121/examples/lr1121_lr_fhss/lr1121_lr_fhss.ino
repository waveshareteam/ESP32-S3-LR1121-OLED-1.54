#include "lr1121_lr_fhss.h"

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lr1121_lr_fhss_test();
}

void loop() {
  delay(1000); // Short delay to control the loop speed

}