#include "task_rtc.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"
#include "../config_store.h"

#include <Wire.h>
#include <RTClib.h>

// ================================================================
// task_rtc.cpp — DS1307 Real-Time Clock
//
// Reads current date and time from DS3231 once per second.
// Formats strings for display and computes Unix epoch timestamp.
// Updates xDisplayQueue and xAPIQueue with timestamped data.
//
// DS1307 CHARACTERISTICS:
//   - General purpose I2C RTC
//   - Less accurate than DS3231 (depends on external crystal)
//   - Maintains time through power outages via coin cell
//   - I2C address 0x68 (fixed)
//
// FIRST TIME SETUP:
//   If the RTC has never been set or lost its battery, it will
//   report Jan 1 2000 00:00:00. Set it by calling:
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   This sets it to the compile time of this firmware.
// ================================================================

static RTC_DS1307 rtc;
static bool       rtcOK = false;

// ──────────────────────────────────────────────────────────────────
static bool initRTC() {
    if (!rtc.begin(&Wire)) {
        Serial.println("[RTC] ERROR: DS1307 not found on I2C bus.");
        return false;
    }
    // DS1307 uses isrunning() instead of lostPower()
    if (!rtc.isrunning()) {
        Serial.println("[RTC] WARN: RTC is NOT running. Setting to compile time.");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.println("[RTC] DS1307 initialised.");
    return true;
}

// ──────────────────────────────────────────────────────────────────
void vRTCTask(void *pvParameters) {
    Serial.printf("[RTC] Task started on Core %d\n", xPortGetCoreID());

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        rtcOK = initRTC();
        xSemaphoreGive(xI2CMutex);
    }
    if (!rtcOK) {
        Serial.println("[RTC] Task terminating — RTC not found.");
        vTaskDelete(NULL);
        return;
    }

    VitalData_t rtcData  = {};
    TickType_t  xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_RTC); // 1000 ms

    for (;;) {

        // ── Read current time from DS1307 ─────────────────────────
        DateTime now;

        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
            // now() performs a burst I2C read of the 7 time registers.
            // Returns a DateTime object with year, month, day, hour, min, sec.
            now = rtc.now();
            xSemaphoreGive(xI2CMutex);
        } else {
            vTaskDelayUntil(&xLastWake, xPeriod);
            continue;
        }

        rtcData.rtcValid = true;

        // ── Format "HH:MM:SS" ─────────────────────────────────────
        // snprintf writes at most (size-1) chars and always null-terminates.
        // %02d pads single-digit values with a leading zero: 9 → "09"
        snprintf(rtcData.timeStr, sizeof(rtcData.timeStr),
                 "%02d:%02d:%02d",
                 now.hour(), now.minute(), now.second());

        // ── Format "YYYY-MM-DD" ───────────────────────────────────
        snprintf(rtcData.dateStr, sizeof(rtcData.dateStr),
                 "%04d-%02d-%02d",
                 now.year(), now.month(), now.day());

        // ── Unix epoch timestamp ──────────────────────────────────
        // unixtime() returns seconds since 1970-01-01 00:00:00 UTC.
        // DS1307 stores local time — no timezone conversion here.
        // Apply UTC offset in your server if needed.
        rtcData.epoch  = now.unixtime();
        
        VitalData_t currentData = {};
        xQueuePeek(xDisplayQueue, &currentData, 0);

        currentData.rtcValid = rtcData.rtcValid;
        currentData.epoch    = rtcData.epoch;
        strncpy(currentData.dateStr, rtcData.dateStr, sizeof(currentData.dateStr));
        strncpy(currentData.timeStr, rtcData.timeStr, sizeof(currentData.timeStr));
        currentData.uptime   = (uint32_t)millis();

        rtcData = currentData;

        // ── Publish ───────────────────────────────────────────────
        xQueueOverwrite(xDisplayQueue, &rtcData);
        xQueueSend(xAPIQueue, &rtcData, pdMS_TO_TICKS(10));

        if (serialLoggingEnabled) {
            Serial.printf("[RTC] %s %s\n", rtcData.dateStr, rtcData.timeStr);
        }

        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}

void setRTCTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        rtc.adjust(DateTime(year, month, day, hour, min, sec));
        xSemaphoreGive(xI2CMutex);
        Serial.println("[RTC] Time updated successfully via CLI.");
    } else {
        Serial.println("[RTC] ERROR: Could not acquire I2C mutex to set time.");
    }
}
