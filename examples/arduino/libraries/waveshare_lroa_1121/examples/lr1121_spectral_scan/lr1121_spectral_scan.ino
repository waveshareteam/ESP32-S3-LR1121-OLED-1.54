#include "lr1121_spectral_scan.h"

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lora_spectral_scan_init( );
}

void loop() {

  lora_spectral_scan_loop( );

}