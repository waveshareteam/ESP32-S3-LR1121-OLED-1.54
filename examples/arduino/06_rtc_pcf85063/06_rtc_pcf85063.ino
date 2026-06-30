/*
 * SPDX-FileCopyrightText: 2026-2027 Waveshare Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Wire.h>
#include <Arduino.h>
#include <time.h>
#include "SensorPCF85063.hpp"

#ifndef SENSOR_SDA
#define SENSOR_SDA  13
#endif

#ifndef SENSOR_SCL
#define SENSOR_SCL  14
#endif

#ifndef SENSOR_IRQ
#define SENSOR_IRQ  7
#endif


SensorPCF85063 rtc;

enum class AlarmTestStage : uint8_t {
    Seconds,
    Minute,
    Hour,
    Day,
    Done,
};

static constexpr uint32_t SERIAL_WAIT_TIMEOUT_MS = 3000;
static constexpr uint32_t PRINT_INTERVAL_MS = 1000;
static constexpr uint16_t TEST_YEAR = 2026;

uint32_t print_interval_ms = 0;
uint8_t next_hour = 22;
uint8_t next_month = 1;
uint8_t next_day = 1;
uint8_t next_minute = 59;
uint8_t next_second = 20;
AlarmTestStage alarm_stage = AlarmTestStage::Seconds;
bool done_reported = false;


void setup()
{
    Serial.begin(115200);
    const uint32_t serial_wait_start_ms = millis();
    while (!Serial && (millis() - serial_wait_start_ms < SERIAL_WAIT_TIMEOUT_MS)) {
        yield();
    }

    if (!rtc.begin(Wire, SENSOR_SDA, SENSOR_SCL)) {
        Serial.println("Failed to find PCF85063 - check your wiring!");
        while (1) {
            yield();
        }
    }

    pinMode(SENSOR_IRQ, INPUT_PULLUP);

    rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);

    next_second = 55;

    // First test alarm seconds
    rtc.setAlarmBySecond(30);

    rtc.enableAlarm();

}

void printDateTime()
{
    if (millis() - print_interval_ms > PRINT_INTERVAL_MS) {
        /**
        /// Format output time*
        Option:
            DT_FMT_HM,          // Format Style : Hour:Minute
            DT_FMT_HMS,         // Format Style : Hour:Minute:Second
            DT_FMT_YMD,         // Format Style : Year-Month-Day
            DT_FMT_MDY,         // Format Style : Month-Day-Year
            DT_FMT_DMY,         // Format Style : Day-Month-Year
            DT_FMT_YMD_HMS,     // Format Style : Year-Month-Day/Hour:Minute:Second
            Default : DT_FMT_YMD_HMS_WEEK // Format Style : Year-Month-Day/Hour:Minute:Second - Weekday
        */
        Serial.println(rtc.strftime());

        print_interval_ms = millis();
    }
}

// Test seconds timing
bool testAlarmSeconds()
{
    if (digitalRead(SENSOR_IRQ) == LOW) {
        Serial.println("testAlarmSeconds Interrupt .... ");
        if (rtc.isAlarmActive()) {
            Serial.println("Alarm active");
            rtc.resetAlarm();
            rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);
            rtc.setAlarmByMinutes(0);
            return true;
        }
    }
    printDateTime();
    return false;
}


// Test minute timing
bool testAlarmMinute()
{
    if (digitalRead(SENSOR_IRQ) == LOW) {
        Serial.println("testAlarmMinute Interrupt .... ");
        if (rtc.isAlarmActive()) {
            Serial.println("Alarm active");
            rtc.resetAlarm();
            rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);
            next_hour++;
            if (next_hour >= 24) {
                next_hour = 23;
                next_day = 25;
                rtc.setAlarmByHours(0);
                Serial.println("setAlarmByHours");
                return true;
            }
        }
    }
    printDateTime();
    return false;
}

// Test hour timing
bool testAlarmHour()
{
    if (digitalRead(SENSOR_IRQ) == LOW) {
        Serial.println("testAlarmHour Interrupt .... ");
        if (rtc.isAlarmActive()) {
            Serial.println("Alarm active");
            rtc.resetAlarm();
            rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);
            next_day++;
            if (next_day >= 30) {
                next_month = 1;
                next_hour = 23;
                next_minute = 59;
                next_second = 55;
                next_day = rtc.getDaysInMonth(next_month, TEST_YEAR);
                rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);
                rtc.setAlarmByDays(1);
                Serial.println("setAlarmByDays");
                return true;
            }
        }
    }
    printDateTime();
    return false;
}

// Test day timing
bool testAlarmDay()
{
    if (digitalRead(SENSOR_IRQ) == LOW) {
        Serial.println("testAlarmDay Interrupt .... ");
        if (rtc.isAlarmActive()) {
            Serial.println("Alarm active");
            rtc.resetAlarm();
            next_day = rtc.getDaysInMonth(next_month, TEST_YEAR);
            rtc.setDateTime(TEST_YEAR, next_month, next_day, next_hour, next_minute, next_second);
            next_month++;
            if (next_month >= 12) {
                return true;
            }
        }
    }
    printDateTime();
    return false;
}

void loop()
{
    switch (alarm_stage) {
        case AlarmTestStage::Seconds:
            if (testAlarmSeconds()) {
                alarm_stage = AlarmTestStage::Minute;
            }
            break;
        case AlarmTestStage::Minute:
            if (testAlarmMinute()) {
                alarm_stage = AlarmTestStage::Hour;
            }
            break;
        case AlarmTestStage::Hour:
            if (testAlarmHour()) {
                alarm_stage = AlarmTestStage::Day;
            }
            break;
        case AlarmTestStage::Day:
            if (testAlarmDay()) {
                alarm_stage = AlarmTestStage::Done;
            }
            break;
        case AlarmTestStage::Done:
            if (!done_reported) {
                Serial.println("Test done ...");
                done_reported = true;
            }
            break;
    }
    yield();
}

