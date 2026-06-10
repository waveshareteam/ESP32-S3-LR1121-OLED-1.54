#include "lr1121_write.h"

void setup() 
{
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  delay(1000);          // Wait for 1 second to ensure the system stabilizes
  lora_write_init();    // Initialize the LR1121 for writing
}

int i = 0; // Counter to control the transmission interval
uint8_t buf[] = "hello world!"; // Buffer containing the data to be transmitted

void loop() {
  if(i > 2)
  {
    i = 0; // Reset the counter
    // radio_tx_auto(&lr1121); // Uncomment to use automatic transmission mode
    radio_tx_custom(&lr1121, buf, sizeof(buf)); // Transmit custom data
  }

  if(irq_flag)
    lora_irq_process(&lr1121); // Process any pending interrupts

  i++; // Increment the counter
  delay(1); // Short delay to control the loop speed
}