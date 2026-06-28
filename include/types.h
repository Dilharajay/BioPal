#pragma once
#include <Arduino.h>

// ================================================================
// types.h — Shared data structures and enumerations
//
// VitalData_t is the single message that flows between all tasks.
// It is written by sensor tasks and consumed by display + API tasks.
// ================================================================

// ── DISPLAY PAGE ────────────────────────────────────────────────
typedef enum : uint8_t {
    PAGE_HEART  = 0,   // Heart rate + SpO2
    PAGE_TEMP   = 1,   // Body temperature
    PAGE_MOTION = 2,   // Accelerometer / fall detection
    PAGE_CLOCK  = 3,   // Date, time, system info
    PAGE_COUNT  = 4    // Sentinel — always last
} DisplayPage_t;

// ── TEMPERATURE STATE MACHINE ───────────────────────────────────
typedef enum : uint8_t {
    TEMP_STATE_TRIGGER,   // Send "start conversion" command to MAX30205
    TEMP_STATE_WAITING,   // Wait for internal ADC conversion to complete
    TEMP_STATE_READ       // Read result and publish
} TempState_t;

// ── MASTER VITAL DATA STRUCT ────────────────────────────────────
// All fields are written by exactly one task and read by consumers.
// DO NOT write to a field from multiple tasks — use a dedicated field
// per sensor task to avoid race conditions inside the struct itself.
typedef struct {

    // ── MAX30105: Heart Rate + SpO2 ─────────────────────────
    float    heartRate;      // Rolling average BPM (from checkForBeat)
    float    spO2;           // Blood oxygen % (from SpO2 algorithm buffer)
    bool     fingerDetected; // true if IR signal > threshold
    bool     hrValid;        // true once enough beats have been captured
    bool     spo2Valid;      // true once SpO2 algorithm buffer is filled

    // ── MAX30205: Body Temperature ──────────────────────────
    float    bodyTempC;      // Celsius (clinical accuracy ±0.1 °C)
    float    bodyTempF;      // Fahrenheit (computed from C)
    bool     bodyTempValid;  // false before first successful read

    // ── MPU6050: Motion + Orientation ───────────────────────
    float    accelX;         // m/s²  X-axis linear acceleration
    float    accelY;         // m/s²  Y-axis linear acceleration
    float    accelZ;         // m/s²  Z-axis linear acceleration (≈9.8 at rest)
    float    accelMag;       // m/s²  vector magnitude = sqrt(x²+y²+z²)
    float    gyroX;          // °/s   angular velocity X
    float    gyroY;          // °/s   angular velocity Y
    float    gyroZ;          // °/s   angular velocity Z
    bool     fallDetected;   // true if magnitude exceeded FALL_THRESHOLD

    // ── DS3231: Real-Time Clock ─────────────────────────────
    char     timeStr[9];     // null-terminated "HH:MM:SS"
    char     dateStr[11];    // null-terminated "YYYY-MM-DD"
    uint32_t epoch;          // Unix timestamp (seconds since 1970-01-01)
    bool     rtcValid;       // false if RTC has lost power

    // ── System metadata ─────────────────────────────────────
    uint32_t uptime;         // millis() at assembly time
    int8_t   wifiRSSI;       // dBm (e.g. -65). 0 = not connected.

} VitalData_t;
