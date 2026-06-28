#include "task_motion.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"
#include "../config_store.h"

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// ================================================================
// task_motion.cpp — MPU6050 accelerometer + gyroscope
//
// Runs at 10 Hz (every 100 ms).
// Reads 3-axis acceleration and gyroscope.
// Computes vector magnitude for fall detection.
// Publishes 1:1 — every read is published (motion changes faster
// than temperature, useful to stream raw accel data to API).
//
// FALL DETECTION:
//   A free-fall event produces near-zero magnitude (<2 m/s²),
//   followed by a high-impact event (>30 m/s² = 3g) on landing.
//   We detect the impact phase: simpler and more reliable.
//
// HARDWARE NOTE:
//   MPU6050 AD0 pin must be tied to 3.3V → I2C address 0x69
//   If left floating (or GND), address is 0x68 (conflicts with DS3231)
// ================================================================

static Adafruit_MPU6050 imu;
static bool imuOK = false;

// ──────────────────────────────────────────────────────────────────
static bool initSensor() {
    // begin() takes the I2C address. Default is 0x68 but we use 0x69.
    if (!imu.begin(ADDR_MPU6050, &Wire)) {
        Serial.println("[IMU] ERROR: MPU6050 not found. Check AD0 pin to 3.3V.");
        return false;
    }
    // ±2g is most sensitive range — best for subtle patient motion.
    // Use MPU6050_RANGE_8_G or 16_G for impact detection in sports contexts.
    imu.setAccelerometerRange(MPU6050_RANGE_2_G);

    // ±250°/sec gyro — sufficient for orientation tracking.
    imu.setGyroRange(MPU6050_RANGE_250_DEG);

    // 21 Hz digital low-pass filter — removes noise above human motion bandwidth.
    // Human motion is mostly below 10 Hz. This cuts motor and structural vibration.
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("[IMU] MPU6050 initialised.");
    return true;
}

// ──────────────────────────────────────────────────────────────────
void vMotionTask(void *pvParameters) {
    Serial.printf("[IMU] Task started on Core %d\n", xPortGetCoreID());

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        imuOK = initSensor();
        xSemaphoreGive(xI2CMutex);
    }
    if (!imuOK) {
        Serial.println("[IMU] Task terminating — sensor not found.");
        vTaskDelete(NULL);
        return;
    }

    VitalData_t motionData = {};
    TickType_t  xLastWake  = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_MOTION); // 100 ms

    for (;;) {

        // ── Read all three sensor event types in one call ─────────
        sensors_event_t accelEvent, gyroEvent, tempEvent;

        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
            // getEvent() performs three I2C burst reads sequentially.
            // It fills accelEvent.acceleration.{x,y,z} in m/s²
            // and gyroEvent.gyro.{x,y,z} in rad/s.
            imu.getEvent(&accelEvent, &gyroEvent, &tempEvent);
            xSemaphoreGive(xI2CMutex);
        } else {
            vTaskDelayUntil(&xLastWake, xPeriod);
            continue;
        }

        // ── Store acceleration (m/s²) ─────────────────────────────
        motionData.accelX = accelEvent.acceleration.x;
        motionData.accelY = accelEvent.acceleration.y;
        motionData.accelZ = accelEvent.acceleration.z;

        // ── Compute vector magnitude ──────────────────────────────
        // At rest: Z ≈ 9.81 m/s² (gravity), X ≈ Y ≈ 0.
        // magnitude ≈ 9.81. During fall impact: magnitude > FALL_THRESHOLD.
        motionData.accelMag = sqrtf(
            motionData.accelX * motionData.accelX +
            motionData.accelY * motionData.accelY +
            motionData.accelZ * motionData.accelZ
        );

        // ── Fall detection ────────────────────────────────────────
        // FALL_THRESHOLD (default: 30 m/s² = ~3g) detects high-impact landing.
        // A latch-and-clear pattern holds the flag for one publish cycle.
        if (motionData.accelMag > FALL_THRESHOLD) {
            motionData.fallDetected = true;
            if (serialLoggingEnabled) {
                Serial.printf("[IMU] FALL DETECTED! Magnitude: %.2f m/s²\n",
                              motionData.accelMag);
            }
        } else {
            motionData.fallDetected = false;
        }

        // ── Store gyroscope (convert rad/s to deg/s) ─────────────
        motionData.gyroX = gyroEvent.gyro.x * 57.2957795f;
        motionData.gyroY = gyroEvent.gyro.y * 57.2957795f;
        motionData.gyroZ = gyroEvent.gyro.z * 57.2957795f;

        VitalData_t currentData = {};
        xQueuePeek(xDisplayQueue, &currentData, 0);

        currentData.accelX       = motionData.accelX;
        currentData.accelY       = motionData.accelY;
        currentData.accelZ       = motionData.accelZ;
        currentData.accelMag     = motionData.accelMag;
        currentData.gyroX        = motionData.gyroX;
        currentData.gyroY        = motionData.gyroY;
        currentData.gyroZ        = motionData.gyroZ;
        currentData.fallDetected = motionData.fallDetected;
        currentData.uptime       = (uint32_t)millis();

        motionData = currentData;

        // ── Publish ───────────────────────────────────────────────
        xQueueOverwrite(xDisplayQueue, &motionData);
        xQueueSend(xAPIQueue, &motionData, pdMS_TO_TICKS(10));

        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}
