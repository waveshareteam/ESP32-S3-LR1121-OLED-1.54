#include "lr1121_ping_pong.h"

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lr1121_ping_pong_init();
}

void loop() {

  if(irq_flag)
      lora_irq_process( &lr1121, IRQ_MASK );
  delay(1); // Short delay to control the loop speed

}