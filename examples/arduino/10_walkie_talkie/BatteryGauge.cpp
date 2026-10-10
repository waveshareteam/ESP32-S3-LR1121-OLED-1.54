/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "BatteryGauge.h"

#include <Wire.h>
#include <cstring>

#include "Config.h"

namespace {
constexpr uint16_t BUS_FREE_US = 100; // BQ27220 minimum: 66 us.
constexpr uint16_t CFG_UPDATE = 0x0400;
constexpr uint16_t INIT_COMPLETE = 0x0020;
constexpr uint16_t PROFILE_ADDRESS = 0x929B;
constexpr uint16_t CSYNC_AND_SC = 0x0012;
constexpr uint8_t PROFILE_SIZE = 6; // CEDV config, FCC, design capacity.
static_assert(BATTERY_DESIGN_CAPACITY_MAH > 0 && BATTERY_DESIGN_CAPACITY_MAH <= 32767,
              "BQ27220 capacity must fit its signed mAh parameter");

bool writeRegisters(uint8_t reg, const uint8_t *data, uint8_t size) {
  // All gauge steps run on the main loop. Restore the OLED/codec bus speed
  // before returning, rather than holding it at 100 kHz throughout setup.
  uint32_t previousClock = Wire.getClock();
  bool clockOk = Wire.setClock(100000);
  if (!clockOk) {
    (void)Wire.setClock(previousClock);
    return false;
  }
  delayMicroseconds(BUS_FREE_US);
  Wire.beginTransmission(BQ27220_ADDR);
  bool queued = Wire.write(reg) == 1;
  queued = (Wire.write(data, size) == size) && queued;
  uint8_t result = Wire.endTransmission();
  delayMicroseconds(BUS_FREE_US);
  bool restored = Wire.setClock(previousClock);
  return queued && result == 0 && restored;
}

bool control(uint16_t command) {
  uint8_t data[] = {uint8_t(command), uint8_t(command >> 8)};
  bool ok = writeRegisters(0x00, data, sizeof(data));
  delay(15);
  return ok;
}

bool readU16(uint8_t reg, uint16_t &value) {
  uint8_t data[2];
  if (!readBatteryGaugeRegisters(reg, data, sizeof(data))) {
    return false;
  }
  value = batteryGaugeU16(data);
  return true;
}

uint8_t security(uint16_t status) {
  return (status >> 1) & 3; // 1: full access, 2: unsealed, 3: sealed.
}

bool sendUnsealKeys() {
  // TI's published default keys; boards with customized keys fail safely.
  return control(0x0414) && control(0x3672);
}

uint16_t bigEndian(const uint8_t *data) {
  return (uint16_t(data[0]) << 8) | data[1];
}

void putBigEndian(uint8_t *data, uint16_t value) {
  data[0] = value >> 8;
  data[1] = value;
}

bool readProfile(uint8_t *data) {
  uint8_t address[] = {uint8_t(PROFILE_ADDRESS), uint8_t(PROFILE_ADDRESS >> 8)};
  if (!writeRegisters(0x3E, address, sizeof(address))) {
    return false;
  }
  for (unsigned i = 0; i < 3; ++i) {
    delay(5);
    uint8_t response[PROFILE_SIZE + 2];
    if (!readBatteryGaugeRegisters(0x3E, response, sizeof(response))) {
      return false;
    }
    if (batteryGaugeU16(response) == PROFILE_ADDRESS) {
      memcpy(data, response + 2, PROFILE_SIZE);
      return true;
    }
  }
  return false;
}

bool writeProfile(const uint8_t *data, uint8_t size) {
  uint8_t payload[PROFILE_SIZE + 2] = {uint8_t(PROFILE_ADDRESS), uint8_t(PROFILE_ADDRESS >> 8)};
  memcpy(payload + 2, data, size);
  if (!writeRegisters(0x3E, payload, size + 2)) {
    return false;
  }
  delay(5);
  uint8_t sum = 0;
  for (uint8_t i = 0; i < size + 2; ++i) {
    sum += payload[i];
  }
  uint8_t commit[] = {uint8_t(0xFF - sum), uint8_t(size + 4)};
  bool ok = writeRegisters(0x60, commit, sizeof(commit));
  delay(5);
  return ok;
}

enum class InitStep : uint8_t {
  Idle, Device, InitialStatus, Unseal, CheckUnseal, Profile,
  FullAccess, CheckFullAccess, EnterConfig, WaitEnter, WriteProfile, Rollback,
  ExitConfig, WaitExit, CleanupExit, WaitCleanup,
  CheckSecurity, Seal, CheckSeal, RestoreUnseal, CheckRestoreUnseal,
  ReadDesign, Finish, Cooldown, Done
};

struct GaugeInitialization {
  InitStep step = InitStep::Idle;
  uint32_t dueMs = 0;
  uint32_t startedMs = 0;
  uint8_t originalSecurity = 0;
  uint8_t polls = 0;
  uint8_t writeSize = PROFILE_SIZE;
  uint8_t original[PROFILE_SIZE] = {};
  uint8_t desired[PROFILE_SIZE] = {};
  bool verified = false;
  bool configRequested = false;
  bool writeAttempted = false;
  bool capacityChanged = false;
  bool exitAcknowledged = false;
  bool designRead = false;
  uint16_t design = 0;
};
GaugeInitialization initialization;

void schedule(InitStep step, uint32_t waitMs = 0) {
  initialization.step = step;
  initialization.dueMs = millis() + waitMs;
  initialization.polls = 0;
}

void configFailed(const char *stage, uint32_t waitMs = 0) {
  auto &init = initialization;
  WALKIE_LOG_PRINTF("[Battery] init failed: %s\n", stage);
  init.verified = false;
  if (init.writeAttempted) {
    schedule(InitStep::Rollback, waitMs);
  } else if (init.configRequested) {
    schedule(InitStep::ExitConfig, waitMs);
  } else if (init.originalSecurity != 0) {
    schedule(InitStep::CheckSecurity, 500);
  } else {
    schedule(InitStep::Finish);
  }
}

void securityFailed() {
  WALKIE_LOG_PRINTF("[Battery] init failed: could not restore access state\n");
  initialization.verified = false;
  schedule(InitStep::ReadDesign, 500);
}

// Called no more than every 500 ms, including failed reads. Returns true once
// the requested mode is seen or the bounded poll count has been exhausted.
bool pollConfig(bool enabled, bool &matched) {
  uint16_t status = 0;
  matched = readU16(0x3A, status) && bool(status & CFG_UPDATE) == enabled;
  if (matched || ++initialization.polls >= 6) {
    return true;
  }
  initialization.dueMs = millis() + 500;
  return false;
}

void serviceInitialization() {
  auto &init = initialization;
  uint16_t value = 0;
  uint8_t actual[PROFILE_SIZE];
  bool matched = false;
  switch (init.step) {
    case InitStep::Device:
      if (!control(0x0001) || !readU16(0x40, value) || value != 0x0220) {
        configFailed("device missing or not BQ27220");
      } else {
        schedule(InitStep::InitialStatus, 500);
      }
      break;
    case InitStep::InitialStatus:
      if (!readU16(0x3A, value)) {
        configFailed("operation status unreadable");
      } else if (!(value & INIT_COMPLETE) && ++init.polls < 6) {
        init.dueMs = millis() + 500;
      } else if (!(value & INIT_COMPLETE) || (value & CFG_UPDATE) || security(value) == 0) {
        configFailed("gauge not ready");
      } else {
        init.originalSecurity = security(value);
        schedule(init.originalSecurity == 3 ? InitStep::Unseal : InitStep::Profile);
      }
      break;
    case InitStep::Unseal:
      if (!sendUnsealKeys()) {
        configFailed("unseal keys");
      } else {
        schedule(InitStep::CheckUnseal, 500);
      }
      break;
    case InitStep::CheckUnseal:
      if (!readU16(0x3A, value) || security(value) != 2) {
        configFailed("unseal");
      } else {
        schedule(InitStep::Profile);
      }
      break;
    case InitStep::Profile:
      if (!readProfile(init.original)) {
        configFailed("read capacity profile");
        break;
      }
      memcpy(init.desired, init.original, PROFILE_SIZE);
      value = bigEndian(init.original);
      WALKIE_LOG_PRINTF("[Battery] target=%umAh previous_design=%umAh previous_fcc=%umAh cedv=0x%04x\n",
                    BATTERY_DESIGN_CAPACITY_MAH, bigEndian(init.original + 4),
                    bigEndian(init.original + 2), value);
      init.capacityChanged = bigEndian(init.original + 4) != BATTERY_DESIGN_CAPACITY_MAH;
      putBigEndian(init.desired, value | CSYNC_AND_SC);
      if (init.capacityChanged) {
        // Seed FCC only when changing battery size; retain later learned FCC.
        putBigEndian(init.desired + 2, BATTERY_DESIGN_CAPACITY_MAH);
        putBigEndian(init.desired + 4, BATTERY_DESIGN_CAPACITY_MAH);
      } else {
        init.writeSize = 2; // Only change flags, not the learned capacity.
      }
      if (memcmp(init.original, init.desired, PROFILE_SIZE) == 0) {
        init.verified = true;
        schedule(InitStep::CheckSecurity, 500);
      } else {
        schedule(init.originalSecurity == 1 ? InitStep::EnterConfig : InitStep::FullAccess);
      }
      break;
    case InitStep::FullAccess:
      if (!control(0xFFFF) || !control(0xFFFF)) {
        configFailed("full access keys");
      } else {
        schedule(InitStep::CheckFullAccess, 1000);
      }
      break;
    case InitStep::CheckFullAccess:
      if (!readU16(0x3A, value) || security(value) != 1) {
        configFailed("full access");
      } else {
        schedule(InitStep::EnterConfig);
      }
      break;
    case InitStep::EnterConfig:
      init.configRequested = true; // Clean up even if the acknowledgement is lost.
      if (!control(0x0090)) {
        configFailed("enter config update", 2000);
      } else {
        // TRM section 2.2.21: let the gauge settle without blocking the UI.
        schedule(InitStep::WaitEnter, 2000);
      }
      break;
    case InitStep::WaitEnter:
      if (pollConfig(true, matched)) {
        if (matched) {
          schedule(InitStep::WriteProfile);
        } else {
          configFailed("enter config timeout");
        }
      }
      break;
    case InitStep::WriteProfile:
      init.writeAttempted = true;
      if (!writeProfile(init.desired, init.writeSize) || !readProfile(actual) ||
          memcmp(init.desired, actual, init.writeSize) != 0) {
        configFailed("write/read back capacity profile");
      } else {
        init.verified = true;
        schedule(InitStep::ExitConfig);
      }
      break;
    case InitStep::Rollback: {
      bool restored = writeProfile(init.original, init.writeSize) && readProfile(actual) &&
                      memcmp(init.original, actual, init.writeSize) == 0;
      WALKIE_LOG_PRINTF("[Battery] profile rollback=%s\n", restored ? "ok" : "FAILED");
      schedule(InitStep::ExitConfig);
      break;
    }
    case InitStep::ExitConfig:
      // Re-estimate SOC only for a verified capacity change.
      init.exitAcknowledged = control(init.verified && init.capacityChanged ? 0x0091 : 0x0092);
      schedule(InitStep::WaitExit, 2000);
      break;
    case InitStep::WaitExit:
      if (pollConfig(false, matched)) {
        if (matched && init.exitAcknowledged) {
          schedule(InitStep::CheckSecurity, 500);
        } else {
          init.verified = false;
          schedule(InitStep::CleanupExit);
        }
      }
      break;
    case InitStep::CleanupExit:
      init.exitAcknowledged = control(0x0092);
      schedule(InitStep::WaitCleanup, 500);
      break;
    case InitStep::WaitCleanup:
      if (pollConfig(false, matched)) {
        WALKIE_LOG_PRINTF("[Battery] config exit failed; cleanup=%s\n",
                      matched && init.exitAcknowledged ? "ok" : "FAILED");
        schedule(InitStep::CheckSecurity, 500);
      }
      break;
    case InitStep::CheckSecurity:
      if (readU16(0x3A, value) && security(value) == init.originalSecurity) {
        schedule(InitStep::ReadDesign, 500);
      } else if (init.originalSecurity == 1) {
        securityFailed();
      } else {
        schedule(InitStep::Seal);
      }
      break;
    case InitStep::Seal:
      // Returning from full access to unsealed requires seal, then unseal.
      init.exitAcknowledged = control(0x0030);
      schedule(InitStep::CheckSeal, 500);
      break;
    case InitStep::CheckSeal:
      if (!readU16(0x3A, value) || security(value) != 3 || !init.exitAcknowledged) {
        securityFailed();
      } else {
        schedule(init.originalSecurity == 3 ? InitStep::ReadDesign : InitStep::RestoreUnseal, 500);
      }
      break;
    case InitStep::RestoreUnseal:
      if (!sendUnsealKeys()) {
        securityFailed();
      } else {
        schedule(InitStep::CheckRestoreUnseal, 500);
      }
      break;
    case InitStep::CheckRestoreUnseal:
      if (!readU16(0x3A, value) || security(value) != 2) {
        securityFailed();
      } else {
        schedule(InitStep::ReadDesign, 500);
      }
      break;
    case InitStep::ReadDesign:
      init.designRead = readU16(0x3C, init.design);
      init.verified = init.verified && init.designRead && init.design == BATTERY_DESIGN_CAPACITY_MAH;
      schedule(InitStep::Finish);
      break;
    case InitStep::Finish:
      WALKIE_LOG_PRINTF("[Battery] init=%s design=%umAh design_ok=%u capacity_changed=%u elapsed=%ums\n",
                    init.verified ? "ok" : "FAILED", init.design, unsigned(init.designRead),
                    unsigned(init.capacityChanged), unsigned(millis() - init.startedMs));
      // Do not overlap normal polling with initialization status reads.
      schedule(InitStep::Cooldown, 1000);
      break;
    case InitStep::Cooldown:
      schedule(InitStep::Done);
      break;
    case InitStep::Idle:
    case InitStep::Done:
      break;
  }
}
} // namespace

bool readBatteryGaugeRegisters(uint8_t reg, uint8_t *data, uint8_t size) {
  delayMicroseconds(BUS_FREE_US);
  Wire.beginTransmission(BQ27220_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    delayMicroseconds(BUS_FREE_US);
    return false;
  }
  size_t received = Wire.requestFrom((uint8_t)BQ27220_ADDR, size);
  delayMicroseconds(BUS_FREE_US);
  if (received != size) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }
  for (uint8_t i = 0; i < size; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

void startBatteryGaugeInit() {
  if (batteryGaugeInitInProgress()) {
    return;
  }
  initialization = GaugeInitialization{};
  initialization.startedMs = millis();
  schedule(InitStep::Device);
}

bool batteryGaugeInitInProgress() {
  return initialization.step != InitStep::Idle && initialization.step != InitStep::Done;
}

void serviceBatteryGaugeInit() {
  if (!batteryGaugeInitInProgress() || int32_t(millis() - initialization.dueMs) < 0) {
    return;
  }
  // Run one short step per main-loop pass. Long settle/poll waits use deadlines.
  serviceInitialization();
}
