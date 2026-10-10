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
#include "BatteryGauge.h"
#include "Config.h"
#include "RadioRuntime.h"
#include "Sensors.h"
#include "Settings.h"
#include "Ui.h"

static const gpio_num_t WAKE_BUTTON_GPIOS[] = {
    (gpio_num_t)PIN_KEY_PTT,
    (gpio_num_t)PIN_KEY_MENU,
    (gpio_num_t)PIN_KEY_PWR,
    (gpio_num_t)PIN_KEY_BOOT,
};
static const gpio_num_t LORA_WAKE_GPIO = (gpio_num_t)PIN_LR_INT;
static uint32_t lowPowerLastActivityMs = 0;
static uint32_t lowPowerLastButtonMask = 0;

static bool logSleepError(const char *step, esp_err_t err) {
  if (err != ESP_OK) {
    WALKIE_LOG_PRINTF("[Sleep] %s failed: %s\n", step, esp_err_to_name(err));
  }
  return err == ESP_OK;
}

static bool isWakeButtonPressed(gpio_num_t gpio) {
  return gpio_get_level(gpio) == (gpio == PIN_KEY_PWR ? POWER_BUTTON_ACTIVE_LEVEL : LOW);
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

static bool enableLightSleepWakeupSources() {
  bool ok = true;
  for (gpio_num_t gpio : WAKE_BUTTON_GPIOS) {
    // Preserve the input/pull configuration established by the button code.
    uint8_t activeLevel = gpio == PIN_KEY_PWR ? POWER_BUTTON_ACTIVE_LEVEL : LOW;
    ok &= logSleepError("button wake disable", gpio_wakeup_disable(gpio));
    ok &= logSleepError("button wake enable", gpio_wakeup_enable(gpio,
        activeLevel == HIGH ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL));
  }

  detachRadioIrqForSleep();
  ok &= logSleepError("LoRa IRQ disable", gpio_intr_disable(LORA_WAKE_GPIO));
  ok &= logSleepError("LoRa IRQ type", gpio_set_intr_type(LORA_WAKE_GPIO, GPIO_INTR_DISABLE));
  ok &= logSleepError("LoRa wake disable", gpio_wakeup_disable(LORA_WAKE_GPIO));
  ok &= logSleepError("LoRa wake enable", gpio_wakeup_enable(LORA_WAKE_GPIO, GPIO_INTR_HIGH_LEVEL));
  ok &= logSleepError("GPIO wake source", esp_sleep_enable_gpio_wakeup());
  return ok;
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
  // The configuration worker and any packet in progress must finish first.
  if (batteryGaugeInitInProgress() || radioRuntimeIsBusy() || transmissionMode || readWakeButtonMask() != 0) {
    return;
  }
  if (isRadioIrqAsserted()) {
    setRadioIrqPending();
    return;
  }

  int16_t radioState = startRadioReceiveForSleep();
  if (radioState != RADIOLIB_ERR_NONE) {
    WALKIE_LOG_PRINTF("[Sleep] receive before sleep failed: %d\n", radioState);
    return;
  }

  WALKIE_LOG_PRINTF("[Sleep] idle for %ums, entering light sleep\n", (unsigned)LOW_POWER_IDLE_TIMEOUT_MS);
  resetRadioRuntimeAfterSleep(false, false);
  clearAudioBuffers();
  prepareAudioSleep();
  sleepDisplay();
  delay(50);

  bool wakeSourcesReady = enableLightSleepWakeupSources();
  esp_err_t sleepRet = ESP_ERR_INVALID_STATE;
  if (wakeSourcesReady) {
#if ENABLE_DEBUG_LOG
    Serial.flush();
#endif
    sleepRet = esp_light_sleep_start();
  }
  bool loraIrqPending = isRadioIrqAsserted();
  disableLightSleepWakeupSources();

  if (wakeSourcesReady && sleepRet != ESP_OK) {
    WALKIE_LOG_PRINTF("[Sleep] light sleep failed: %s\n", esp_err_to_name(sleepRet));
  }

  wakeDisplay();
  waitForWakeButtonsRelease();
  resetUiButtonsAfterWake();

  resumeAudioFromSleep();
  clearAudioBuffers();
  // A packet may arrive while the wake key is held or the codecs are resuming.
  loraIrqPending = loraIrqPending || isRadioIrqAsserted();
  resetRadioRuntimeAfterSleep(false, loraIrqPending);
  if (!loraIrqPending) {
    radioState = startRadioReceiveForSleep();
    if (radioState != RADIOLIB_ERR_NONE) {
      fatalError("Listen", radioState);
    }
  }

  resetUiTimers();
  resetSensorTimers();
  readRtc();
  readBattery();
  markUiDirty();
  WALKIE_LOG_PRINTF("[Sleep] %s%s\n", sleepRet == ESP_OK ? "woke from light sleep" : "sleep cancelled",
                loraIrqPending ? ", LoRa IRQ pending" : "");
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

  if (buttonMask != lowPowerLastButtonMask || buttonMask != 0 ||
      batteryGaugeInitInProgress() || radioRuntimeIsBusy() || transmissionMode) {
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
  uint32_t startedMs = millis();
#if ENABLE_DEBUG_LOG
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  // A closed/slow serial monitor must not stall PTT, audio or screen refresh.
  // Diagnostic output may be dropped when the USB transmit buffer is full.
  Serial.setTxTimeoutMs(0);
#endif
#endif
  delay(100);
  WALKIE_LOG_PRINTLN();
  WALKIE_LOG_PRINTLN("ESP32-S3 LR1121 OLED walkie-talkie Arduino example");

  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000)) {
    fatalError("I2C init", -1);
  }

  if (!initDisplay()) {
    fatalError("OLED init", -1);
  }
  initUiButtons();
  pinMode(PIN_KEY_PTT, INPUT_PULLUP);

  loadRadioSettings();
  startBatteryGaugeInit();
  readRtc();

  if (!initAudioHardware()) {
    fatalError("Audio init", -1);
  }
  if (!startAudioTasks()) {
    fatalError("Audio task", audioStartupError());
  }

  initRadioSpi();
  int16_t state = applyRadioConfig(true);
  if (state != RADIOLIB_ERR_NONE) {
    fatalError("Radio init", state);
  }
  if (!startRadioConfigTask()) {
    fatalError("Radio task", -1);
  }

  WALKIE_LOG_PRINTF("[LR1121] modem=%s freq=%.1f\n", radioModem == RADIO_MODEM_GFSK ? "GFSK" : "LoRa", radioFrequencyMhz);
  resetUiTimers();
  resetSensorTimers();
  renderUi();
  resetLowPowerIdle();
  WALKIE_LOG_PRINTF("[Boot] ready=%ums, battery check continues in loop\n", (unsigned)(millis() - startedMs));
}

void loop() {
  if (audioStartupError() != 0) {
    fatalError("Audio startup", audioStartupError());
  }
  handleUiButtons();
  handlePtt();
  handleRadioReconfigure();
  handleRadioTransmission();
  handleLowPowerIdle();
  handleRadioIrq();
  serviceBatteryGaugeInit();
  periodicSensors();
  periodicUi();
  logRadioDiagnostics();
  logAudioDiagnostics();
  delay(10);
}
