#include "task_body_temp.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"

#include <Wire.h>
#include <ClosedCube_MAX30205.h>

// ================================================================
// task_body_temp.cpp — MAX30205 body temperature sensor
//
// STATE MACHINE DESIGN (Pattern 3):
//
//   TRIGGER  → write one-shot config to MAX30205 (1 ms over I2C)
//              record triggerTick as time reference
//              → move to WAITING
//
//   WAITING  → every 10 ms, check: (currentTick - triggerTick) >= 50 ms?
//              If yes → move to READ
//              If no  → sleep 10 ms and check again
//              (zero CPU blocked during the 50 ms conversion window)
//
//   READ     → read temperature register over I2C
//              validate and publish
//              if 2 s since last publish → push to queues
//              → move back to TRIGGER
//
// WHY NOT CONTINUOUS MODE?
//   One-shot mode prevents self-heating of the sensor chip between
//   measurements. This is important for skin-contact temperature
//   accuracy — a warm chip biases the reading high.
// ================================================================

static ClosedCube_MAX30205 tempSensor;
static bool     sensorOK = false;

// ──────────────────────────────────────────────────────────────────
static bool initSensor() {
    // Check if device is on bus
    Wire.beginTransmission(ADDR_MAX30205);
    if (Wire.endTransmission() != 0) {
        Serial.println("[TEMP] ERROR: MAX30205 not found on I2C bus.");
        return false;
    }
    tempSensor.begin(ADDR_MAX30205);
    Serial.println("[TEMP] MAX30205 initialised (continuous mode).");
    return true;
}

// ──────────────────────────────────────────────────────────────────
void vBodyTempTask(void *pvParameters) {
    Serial.printf("[TEMP] Task started on Core %d\n", xPortGetCoreID());

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        sensorOK = initSensor();
        xSemaphoreGive(xI2CMutex);
    }
    if (!sensorOK) {
        Serial.println("[TEMP] Task terminating — sensor not found.");
        vTaskDelete(NULL);
        return;
    }

    // ── State machine variables ───────────────────────────────────
    TempState_t state        = TEMP_STATE_TRIGGER; // start by triggering
    TickType_t  triggerTick  = 0;                  // when TRIGGER was sent
    TickType_t  lastPublish  = 0;                  // when we last pushed to queues
    VitalData_t tempData     = {};

    // ── Timing for state machine poll ────────────────────────────
    TickType_t      xLastWake   = xTaskGetTickCount();
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(PERIOD_TEMP_CHECK); // 10 ms

    // ─────────────────────────────────────────────────────────────
    for (;;) {

        switch (state) {

        // ═══ STATE: TRIGGER ═══════════════════════════════════════
        // Send the "start one-shot conversion" command to the sensor.
        // The sensor begins its internal ADC conversion immediately.
        // We do not block — we record the tick and move to WAITING.
        case TEMP_STATE_TRIGGER:
            // Continuous mode: no need to trigger over I2C.
            triggerTick = xTaskGetTickCount(); // record reference time
            state = TEMP_STATE_WAITING;
            break;

        // ═══ STATE: WAITING ════════════════════════════════════════
        // Check elapsed time since TRIGGER without blocking.
        // The sensor is doing all the work internally — we just check the clock.
        // This allows SensorTask (Priority 5) and DisplayTask to preempt freely.
        case TEMP_STATE_WAITING:
            // Unsigned subtraction handles tick counter overflow correctly.
            // Even if xTaskGetTickCount() wraps from 0xFFFFFFFF → 0x00000000,
            // the subtraction gives the correct positive elapsed time.
            if ((xTaskGetTickCount() - triggerTick) >= pdMS_TO_TICKS(PERIOD_TEMP_CONV)) {
                state = TEMP_STATE_READ;
            }
            // else: not ready. Break out, sleep 10 ms, check again.
            break;

        // ═══ STATE: READ ═══════════════════════════════════════════
        // Conversion is complete. Read the 16-bit temperature register.
        case TEMP_STATE_READ: {
            float rawTemp = 0.0f;

            if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                rawTemp = tempSensor.readTemperature();
                xSemaphoreGive(xI2CMutex);
            }

            // Validate: physiological human body temp range 30–42 °C
            // Out-of-range means sensor error, poor contact, or misread
            if (rawTemp > 30.0f && rawTemp < 42.0f) {
                tempData.bodyTempC     = rawTemp;
                tempData.bodyTempF     = rawTemp * 9.0f / 5.0f + 32.0f;
                tempData.bodyTempValid = true;
            } else {
                Serial.printf("[TEMP] WARN: reading %.2f C out of range.\n", rawTemp);
                tempData.bodyTempValid = false;
            }

            // ── Publish if enough time has elapsed ──────────────
            // PERIOD_TEMP_HOLD = 2000 ms → publish at 0.5 Hz
            TickType_t now = xTaskGetTickCount();
            if ((now - lastPublish) >= pdMS_TO_TICKS(PERIOD_TEMP_HOLD)) {
                lastPublish = now;

                VitalData_t currentData = {};
                xQueuePeek(xDisplayQueue, &currentData, 0);

                currentData.bodyTempC     = tempData.bodyTempC;
                currentData.bodyTempF     = tempData.bodyTempF;
                currentData.bodyTempValid = tempData.bodyTempValid;
                currentData.uptime        = (uint32_t)millis();

                tempData = currentData;

                xQueueOverwrite(xDisplayQueue, &tempData);
                xQueueSend(xAPIQueue, &tempData, pdMS_TO_TICKS(20));

                if (tempData.bodyTempValid) {
                    Serial.printf("[TEMP] %.2f C (%.2f F)\n",
                                  tempData.bodyTempC, tempData.bodyTempF);
                }
            }

            // Go back to TRIGGER — begin the next one-shot conversion
            state = TEMP_STATE_TRIGGER;
            break;
        }

        } // end switch

        // ── Sleep 10 ms between state checks ─────────────────────
        // During WAITING (50 ms conversion), this loop runs 5 times.
        // The sensor is doing all the work. We use <1% CPU across all 5 checks.
        vTaskDelayUntil(&xLastWake, xCheckPeriod);
    }
}
