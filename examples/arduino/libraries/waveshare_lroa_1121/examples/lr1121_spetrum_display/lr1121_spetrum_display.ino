#include "lr1121_spetrum_display.h"

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lora_spetrum_display_init( );
}

void loop() {

  lora_spetrum_display_loop( );

}