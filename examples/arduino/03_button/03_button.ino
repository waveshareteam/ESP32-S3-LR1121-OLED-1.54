/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <OneButton.h>

// Button GPIO definitions
#define GPIO0_IO_NUM  0
#define GPIO4_IO_NUM  4
#define GPIO5_IO_NUM  5
#define GPIO6_IO_NUM  6

#define BUTTON_ACTIVE_LEVEL true

OneButton button1(GPIO0_IO_NUM, BUTTON_ACTIVE_LEVEL);
OneButton button2(GPIO5_IO_NUM, BUTTON_ACTIVE_LEVEL);
OneButton button3(GPIO4_IO_NUM, BUTTON_ACTIVE_LEVEL);
OneButton button4(GPIO6_IO_NUM, BUTTON_ACTIVE_LEVEL);

static const uint32_t SERIAL_WAIT_TIMEOUT_MS = 3000;
static const uint32_t BUTTON_POLL_INTERVAL_MS = 10;

void setup() {
  Serial.begin(115200);
  const uint32_t serial_wait_start_ms = millis();
  while (!Serial && (millis() - serial_wait_start_ms) < SERIAL_WAIT_TIMEOUT_MS) {
    delay(1);
  }
  Serial.println("Starting Buttons...");

  // Setup button 1
  button1.attachClick(click1);
  button1.attachDoubleClick(doubleclick1);
  button1.attachLongPressStart(longPressStart1);
  button1.attachLongPressStop(longPressStop1);
  button1.attachDuringLongPress(longPress1);

  // Setup button 2
  button2.attachClick(click2);
  button2.attachDoubleClick(doubleclick2);
  button2.attachLongPressStart(longPressStart2);
  button2.attachLongPressStop(longPressStop2);
  button2.attachDuringLongPress(longPress2);

  // Setup button 3
  button3.attachClick(click3);
  button3.attachDoubleClick(doubleclick3);
  button3.attachLongPressStart(longPressStart3);
  button3.attachLongPressStop(longPressStop3);
  button3.attachDuringLongPress(longPress3);

  // Setup button 4
  button4.attachClick(click4);
  button4.attachDoubleClick(doubleclick4);
  button4.attachLongPressStart(longPressStart4);
  button4.attachLongPressStop(longPressStop4);
  button4.attachDuringLongPress(longPress4);
}

void loop() {
  static uint32_t last_poll_ms = 0;
  const uint32_t now_ms = millis();

  if ((now_ms - last_poll_ms) >= BUTTON_POLL_INTERVAL_MS) {
    last_poll_ms = now_ms;
    button1.tick();
    button2.tick();
    button3.tick();
    button4.tick();
  }

  yield();
}

// Button 1
void click1() {
  Serial.println("Button 1 click.");
}

void doubleclick1() {
  Serial.println("Button 1 doubleclick.");
}

void longPressStart1() {
  Serial.println("Button 1 longPress start");
}

void longPress1() {
  Serial.println("Button 1 longPress...");
}

void longPressStop1() {
  Serial.println("Button 1 longPress stop");
}

// Button 2
void click2() {
  Serial.println("Button 2 click.");
}

void doubleclick2() {
  Serial.println("Button 2 doubleclick.");
}

void longPressStart2() {
  Serial.println("Button 2 longPress start");
}

void longPress2() {
  Serial.println("Button 2 longPress...");
}

void longPressStop2() {
  Serial.println("Button 2 longPress stop");
}

// Button 3
void click3() {
  Serial.println("Button 3 click.");
}

void doubleclick3() {
  Serial.println("Button 3 doubleclick.");
}

void longPressStart3() {
  Serial.println("Button 3 longPress start");
}

void longPress3() {
  Serial.println("Button 3 longPress...");
}

void longPressStop3() {
  Serial.println("Button 3 longPress stop");
}

// Button 4
void click4() {
  Serial.println("Button 4 click.");
}

void doubleclick4() {
  Serial.println("Button 4 doubleclick.");
}

void longPressStart4() {
  Serial.println("Button 4 longPress start");
}

void longPress4() {
  Serial.println("Button 4 longPress...");
}

void longPressStop4() {
  Serial.println("Button 4 longPress stop");
}
