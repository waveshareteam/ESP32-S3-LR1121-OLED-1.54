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


uint32_t intervalue = 0;
uint8_t nextHour = 22;
uint8_t nextMonth = 1;
uint8_t nextDay = 1;
uint8_t nextMinute = 59;
uint8_t nextSecond = 20;


void setup()
{
    Serial.begin(115200);
    while (!Serial);

    if (!rtc.begin(Wire, SENSOR_SDA, SENSOR_SCL)) {
        Serial.println("Failed to find PCF85063 - check your wiring!");
        while (1) {
            delay(1000);
        }
    }

    pinMode(SENSOR_IRQ, INPUT_PULLUP);

    rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);

    nextSecond = 55;

    // First test alarm seconds
    rtc.setAlarmBySecond(30);

    rtc.enableAlarm();

}

void printDateTime()
{
    if (millis() - intervalue > 1000) {
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

        intervalue = millis();
    }
}

// Test seconds timing
void  testAlarmSeconds()
{
    while (1) {
        if (digitalRead(SENSOR_IRQ) == LOW) {
            Serial.println("testAlarmSeconds Interrupt .... ");
            if (rtc.isAlarmActive()) {
                Serial.println("Alarm active");
                rtc.resetAlarm();
                rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);
                rtc.setAlarmByMinutes(0);
                return;
            }
        }
        printDateTime();
    }
}


// Test minute timing
void  testAlarmMinute()
{
    while (1) {
        if (digitalRead(SENSOR_IRQ) == LOW) {
            Serial.println("testAlarmMinute Interrupt .... ");
            if (rtc.isAlarmActive()) {
                Serial.println("Alarm active");
                rtc.resetAlarm();
                rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);
                nextHour++;
                if (nextHour >= 24) {
                    nextHour = 23;
                    nextDay = 25;
                    rtc.setAlarmByHours(0);
                    Serial.println("setAlarmByHours");
                    return;
                }
            }
        }
        printDateTime();
    }
}

// Test hour timing
void  testAlarmHour()
{
    while (1) {
        if (digitalRead(SENSOR_IRQ) == LOW) {
            Serial.println("testAlarmHour Interrupt .... ");
            if (rtc.isAlarmActive()) {
                Serial.println("Alarm active");
                rtc.resetAlarm();
                rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);
                nextDay++;
                if (nextDay >= 30) {
                    nextMonth = 1;
                    nextHour = 23;
                    nextMinute = 59;
                    nextSecond = 55;
                    nextDay = rtc.getDaysInMonth(nextMonth, 2026);
                    rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);
                    rtc.setAlarmByDays(1);
                    Serial.println("setAlarmByDays");
                    return;
                }
            }
        }
        printDateTime();
    }
}

// Test day timing
void  testAlarmDay()
{
    while (1) {
        if (digitalRead(SENSOR_IRQ) == LOW) {
            Serial.println("testAlarmDay Interrupt .... ");
            if (rtc.isAlarmActive()) {
                Serial.println("Alarm active");
                rtc.resetAlarm();
                nextDay = rtc.getDaysInMonth(nextMonth, 2026);
                rtc.setDateTime(2026, nextMonth, nextDay, nextHour, nextMinute, nextSecond);
                nextMonth++;
                if (nextMonth >= 12) {
                    return;
                }
            }
        }
        printDateTime();
    }
}

void loop()
{
    testAlarmSeconds();
    testAlarmMinute();
    testAlarmHour();
    testAlarmDay();

    Serial.println("Test done ...");
    while (1) {
        delay(100);
    }
}



