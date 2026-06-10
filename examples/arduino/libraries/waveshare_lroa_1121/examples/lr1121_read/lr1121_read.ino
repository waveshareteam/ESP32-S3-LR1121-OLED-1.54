#include "lr1121_read.h" // Include the LR1121 read library

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lora_read_init();     // Initialize the LR1121 read module
}

void loop() 
{
  if(irq_flag)          // Check if the interrupt flag is set
    lora_irq_process( &lr1121,IRQ_MASK); // Process the interrupt event for the LR1121 module
  
  delay(1);             // Short delay to prevent excessive CPU usage
}