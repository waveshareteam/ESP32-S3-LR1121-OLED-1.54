/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  Arduino port of the ESP-IDF walkie-talkie example for ESP32-S3-LR1121-OLED-1.54.

  Required libraries from the sibling libraries directory:
  - RadioLib
  - esp32_codec2
  - GFX_Library_for_Arduino

  Build with Arduino-ESP32 3.x. The sketch uses the ESP-IDF I2S v5 driver
  bundled with Arduino-ESP32 3.x.
*/

#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_sleep.h"

#include "AudioPipeline.h"
#include "AppState.h"
#include "Config.h"
#include "RadioRuntime.h"
#include "Sensors.h"
#include "Settings.h"
#include "Ui.h"

static constexpr uint32_t LOW_POWER_IDLE_TIMEOUT_MS = 30000;
static const gpio_num_t WAKE_BUTTON_GPIOS[] = {
    (gpio_num_t)PIN_KEY_PTT,
    (gpio_num_t)PIN_KEY_MENU,
    (gpio_num_t)PIN_KEY_PWR,
    (gpio_num_t)PIN_KEY_BOOT,
};
static const gpio_num_t LORA_WAKE_GPIO = (gpio_num_t)PIN_LR_INT;
static uint32_t lowPowerLastActivityMs = 0;
static uint32_t lowPowerLastButtonMask = 0;

static void logSleepError(const char *step, esp_err_t err) {
  if (err != ESP_OK) {
    Serial.printf("[Sleep] %s failed: %s\n", step, esp_err_to_name(err));
  }
}

static bool isWakeButtonPressed(gpio_num_t gpio) {
  return gpio_get_level(gpio) == 0;
}

static uint32_t readWakeButtonMask() {
  uint32_t mask = 0;
  for (size_t i = 0; i < countOf(WAKE_BUTTON_GPIOS); i++) {
    if (isWakeButtonPressed(WAKE_BUTTON_GPIOS[i])) {
      mask |= (1UL << i);
    }
  }
  return mask;
}

static void waitForWakeButtonsRelease() {
  while (readWakeButtonMask() != 0) {
    delay(20);
  }
  delay(50);
}

static void enableLightSleepWakeupSources() {
  for (gpio_num_t gpio : WAKE_BUTTON_GPIOS) {
    logSleepError("button wake disable", gpio_wakeup_disable(gpio));
    logSleepError("button direction", gpio_set_direction(gpio, GPIO_MODE_INPUT));
    logSleepError("button pullup", gpio_pullup_en(gpio));
    logSleepError("button pulldown", gpio_pulldown_dis(gpio));
    logSleepError("button wake enable", gpio_wakeup_enable(gpio, GPIO_INTR_LOW_LEVEL));
  }

  detachRadioIrqForSleep();
  (void)gpio_intr_disable(LORA_WAKE_GPIO);
  (void)gpio_set_intr_type(LORA_WAKE_GPIO, GPIO_INTR_DISABLE);
  logSleepError("LoRa wake disable", gpio_wakeup_disable(LORA_WAKE_GPIO));
  logSleepError("LoRa wake enable", gpio_wakeup_enable(LORA_WAKE_GPIO, GPIO_INTR_HIGH_LEVEL));
  logSleepError("GPIO wake source", esp_sleep_enable_gpio_wakeup());
}

static void disableLightSleepWakeupSources() {
  (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  for (gpio_num_t gpio : WAKE_BUTTON_GPIOS) {
    (void)gpio_wakeup_disable(gpio);
    (void)gpio_set_intr_type(gpio, GPIO_INTR_DISABLE);
  }
  (void)gpio_intr_disable(LORA_WAKE_GPIO);
  (void)gpio_wakeup_disable(LORA_WAKE_GPIO);
  (void)gpio_set_intr_type(LORA_WAKE_GPIO, GPIO_INTR_DISABLE);
  restoreRadioIrqAction();
}

static void enterLightSleep() {
  Serial.printf("[Sleep] idle for %ums, entering light sleep\n", (unsigned)LOW_POWER_IDLE_TIMEOUT_MS);

  resetRadioRuntimeAfterSleep(false, false);
  clearAudioBuffers();
  int16_t radioState = startRadioReceiveForSleep();
  if (radioState != RADIOLIB_ERR_NONE) {
    Serial.printf("[Sleep] receive before sleep failed: %d\n", radioState);
  }

  prepareAudioSleep();
  sleepDisplay();
  delay(50);

  enableLightSleepWakeupSources();
  esp_err_t sleepRet = esp_light_sleep_start();
  bool loraIrqPending = isRadioIrqAsserted();
  disableLightSleepWakeupSources();

  if (sleepRet != ESP_OK) {
    Serial.printf("[Sleep] light sleep failed: %s\n", esp_err_to_name(sleepRet));
  }

  wakeDisplay();
  waitForWakeButtonsRelease();
  resetUiButtonsAfterWake();

  resumeAudioFromSleep();
  clearAudioBuffers();
  resetRadioRuntimeAfterSleep(false, loraIrqPending);
  if (!loraIrqPending) {
    radioState = startRadioReceiveForSleep();
    if (radioState != RADIOLIB_ERR_NONE) {
      fatalError("Listen", radioState);
    }
  }

  resetUiTimers();
  resetSensorTimers();
  markUiDirty();
  Serial.printf("[Sleep] woke from light sleep%s\n", loraIrqPending ? " by LoRa IRQ" : "");
}

static void resetLowPowerIdle() {
  lowPowerLastActivityMs = millis();
  lowPowerLastButtonMask = readWakeButtonMask();
}

static void handleLowPowerIdle() {
  uint32_t now = millis();
  uint32_t buttonMask = readWakeButtonMask();

  if (isRadioIrqAsserted()) {
    setRadioIrqPending();
    lowPowerLastActivityMs = now;
    lowPowerLastButtonMask = buttonMask;
    return;
  }

  if (buttonMask != lowPowerLastButtonMask || buttonMask != 0 || radioRuntimeIsBusy() || transmissionMode) {
    lowPowerLastActivityMs = now;
    lowPowerLastButtonMask = buttonMask;
    return;
  }

  if (now - lowPowerLastActivityMs < LOW_POWER_IDLE_TIMEOUT_MS) {
    return;
  }

  enterLightSleep();
  resetLowPowerIdle();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP32-S3 LR1121 OLED walkie-talkie Arduino example");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setClock(400000);

  initDisplay();
  initUiButtons();
  pinMode(PIN_KEY_PTT, INPUT_PULLUP);

  loadRadioSettings();
  readRtc();
  readBattery();

  if (!initAudioHardware()) {
    fatalError("Audio init", -1);
  }
  startAudioTasks();

  initRadioSpi();
  int16_t state = applyRadioConfig(true);
  if (state != RADIOLIB_ERR_NONE) {
    fatalError("Radio init", state);
  }

  Serial.printf("[LR1121] modem=%s freq=%.1f\n", radioModem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa", radioFrequencyMhz);
  resetUiTimers();
  resetSensorTimers();
  renderUi();
  resetLowPowerIdle();
}

void loop() {
  handleUiButtons();
  handlePtt();
  handleRadioReconfigure();
  handleRadioTransmission();
  handleLowPowerIdle();
  handleRadioIrq();
  periodicSensors();
  periodicUi();
  delay(10);
}