#include "task_heart_rate.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"

#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>       // checkForBeat() algorithm
#include <spo2_algorithm.h>  // maxim_heart_rate_and_oxygen_saturation()

// ================================================================
// task_heart_rate.cpp — MAX30105 heart rate and SpO2 sensor
//
// TWO-RATE DESIGN:
//   Inner loop runs at 25 Hz (every 40 ms).
//     → Drains one sample from FIFO each cycle.
//     → Feeds it to checkForBeat() to track the waveform peak.
//     → Appends IR + RED values to a rolling 100-sample buffer.
//   Every 100 samples (4 seconds):
//     → Calls maxim_heart_rate_and_oxygen_saturation() for SpO2.
//     → Shifts buffer: discards first 25 samples, keeps last 75.
//     → sampleCount resets to 75 so next 25 samples complete a new window.
//   Every 25 task cycles (1 second):
//     → Pushes the assembled VitalData_t to xDisplayQueue and xAPIQueue.
//
// WHY NOT JUST SAMPLE AT 1 Hz?
//   checkForBeat() analyses the IR waveform shape to detect the
//   systolic peak. At 1 Hz you capture 1 of 25 data points per second —
//   the algorithm sees a flat line, not a wave. Zero beats are detected.
// ================================================================

static MAX30105 sensor;

// ── Rolling 100-sample buffers for SpO2 algorithm ───────────────
// uint32_t instead of long: SpO2 algorithm signature requires uint32_t*
static uint32_t irBuffer[SPO2_BUF_LEN];
static uint32_t redBuffer[SPO2_BUF_LEN];
static uint8_t  sampleCount = 0;   // next free index in the buffer

// ── Beat detection state (checkForBeat is stateful) ─────────────
static byte  rateBuffer[HR_RATE_BUF] = {0}; // circular BPM history
static byte  rateIndex  = 0;
static long  lastBeat   = 0;     // millis() of most recent detected beat
static float beatBPM    = 0.0f;  // instantaneous BPM from last beat interval
static int   avgBPM     = 0;     // rolling average across HR_RATE_BUF beats

// ── SpO2 algorithm outputs ───────────────────────────────────────
static int32_t spo2Result    = 0;
static int8_t  spo2Valid     = 0;
static int32_t hrFromAlgo    = 0;
static int8_t  hrFromAlgoValid = 0;

// ── Publish data assembled by this task ─────────────────────────
// Only the HR-specific fields are filled here.
// Other fields (temp, motion, RTC) are filled by their own tasks,
// but the display/API consumers merge the latest values themselves.
// For simplicity, this task fills a struct from the last-known queue
// state and updates only its own fields before re-publishing.
static VitalData_t hrData = {};

// ──────────────────────────────────────────────────────────────────
static bool initSensor() {
    if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("[HR] ERROR: MAX30105 not found on I2C bus.");
        return false;
    }
    sensor.setup(
        HR_LED_POWER,    // LED brightness
        HR_SAMPLE_AVG,   // hardware averaging factor
        HR_LED_MODE,     // 2 = Red + IR (both channels needed for SpO2)
        HR_SAMPLE_RATE,  // raw samples per second before averaging
        HR_PULSE_WIDTH,  // LED pulse width (411 µs = 18-bit ADC resolution)
        HR_ADC_RANGE     // ADC full-scale range in nA
    );
    Serial.println("[HR] MAX30105 initialised.");
    return true;
}

// ──────────────────────────────────────────────────────────────────
void vHeartRateTask(void *pvParameters) {
    Serial.printf("[HR] Task started on Core %d\n", xPortGetCoreID());

    // ── Sensor init (inside task, owns the I2C transaction) ──────
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        bool ok = initSensor();
        xSemaphoreGive(xI2CMutex);
        if (!ok) {
            Serial.println("[HR] Task terminating — sensor not found.");
            vTaskDelete(NULL);
            return;
        }
    }

    // ── Timing setup for vTaskDelayUntil (no drift) ───────────────
    TickType_t xLastWake = xTaskGetTickCount();
    const TickType_t xSamplePeriod = pdMS_TO_TICKS(PERIOD_HR_SAMPLE); // 40 ms

    // ── Counter that triggers a 1 Hz publish ─────────────────────
    // PERIOD_HR_PUBLISH / PERIOD_HR_SAMPLE = 1000 / 40 = 25 cycles per publish
    const uint8_t PUBLISH_EVERY = PERIOD_HR_PUBLISH / PERIOD_HR_SAMPLE;
    uint8_t publishCounter = 0;

    // ─────────────────────────────────────────────────────────────
    for (;;) {

        // ═══ STEP 1: Read ONE sample from MAX30105 FIFO ══════════
        uint32_t irValue  = 0;
        uint32_t redValue = 0;

        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
            // check() queries the sensor once (non-blocking).
            // It reads any available samples into the library's software FIFO.
            sensor.check(); 
            
            if (sensor.available()) {
                irValue  = sensor.getFIFOIR();
                redValue = sensor.getFIFORed();
                sensor.nextSample(); // Advance software FIFO
            }
            // If no data is available yet, irValue and redValue remain 0.

            xSemaphoreGive(xI2CMutex);
        } else {
            // Bus busy. Skip this cycle. vTaskDelayUntil still fires
            // correctly so the next sample is on schedule.
            vTaskDelayUntil(&xLastWake, xSamplePeriod);
            continue;
        }

        // ═══ STEP 2: Finger detection ════════════════════════════
        hrData.fingerDetected = (irValue > HR_IR_THRESHOLD);

        // ═══ STEP 3: Real-time beat detection ════════════════════
        // checkForBeat() is stateful — it tracks the derivative of the
        // IR waveform internally between calls. Must be called on every
        // sample to see the complete rise-and-fall of each heartbeat.
        if (hrData.fingerDetected && checkForBeat(irValue)) {
            long now   = millis();
            long delta = now - lastBeat;
            lastBeat   = now;

            beatBPM = 60000.0f / (float)delta; // ms/beat → beats/min

            // Sanity check: valid human heart rate range
            if (beatBPM >= 20.0f && beatBPM <= 255.0f) {
                rateBuffer[rateIndex] = (byte)beatBPM;
                rateIndex = (rateIndex + 1) % HR_RATE_BUF;

                // Recompute rolling average across the circular buffer
                int sum = 0;
                for (byte i = 0; i < HR_RATE_BUF; i++) sum += rateBuffer[i];
                avgBPM = sum / HR_RATE_BUF;
                hrData.hrValid = true;
            }
        }

        hrData.heartRate = hrData.fingerDetected ? (float)avgBPM : 0.0f;

        // ═══ STEP 4: Accumulate rolling SpO2 buffer ══════════════
        if (hrData.fingerDetected) {
            irBuffer[sampleCount]  = irValue;
            redBuffer[sampleCount] = redValue;
            sampleCount++;
        }

        // ═══ STEP 5: Compute SpO2 when buffer is full ════════════
        if (sampleCount >= SPO2_BUF_LEN) {
            // This runs the full photoplethysmography signal processing
            // algorithm. It takes ~10 ms on ESP32 — acceptable since
            // this happens only every 4 seconds (100 samples at 25 Hz).
            maxim_heart_rate_and_oxygen_saturation(
                irBuffer, SPO2_BUF_LEN, redBuffer,
                &spo2Result, &spo2Valid,
                &hrFromAlgo, &hrFromAlgoValid
            );

            if (spo2Valid && spo2Result > 70 && spo2Result <= 100) {
                hrData.spO2     = (float)spo2Result;
                hrData.spo2Valid = true;
            }

            // ── Rolling shift: discard first 25, keep last 75 ────
            // On the next 25 samples, the buffer will be full again,
            // giving a fresh SpO2 calculation with 75% overlap.
            // This provides continuous SpO2 with new results every second.
            for (int i = 25; i < SPO2_BUF_LEN; i++) {
                irBuffer[i  - 25]  = irBuffer[i];
                redBuffer[i - 25]  = redBuffer[i];
            }
            sampleCount = SPO2_BUF_LEN - 25; // 75 — next 25 complete a window
        }

        // ═══ STEP 6: Publish at 1 Hz ════════════════════════════
        publishCounter++;
        if (publishCounter >= PUBLISH_EVERY) {
            publishCounter = 0;

            VitalData_t currentData = {};
            xQueuePeek(xDisplayQueue, &currentData, 0);

            currentData.heartRate      = hrData.heartRate;
            currentData.spO2           = hrData.spO2;
            currentData.hrValid        = hrData.hrValid;
            currentData.spo2Valid      = hrData.spo2Valid;
            currentData.fingerDetected = hrData.fingerDetected;
            currentData.uptime         = (uint32_t)millis();

            hrData = currentData;

            // xDisplayQueue: size-1, overwrite — always shows freshest data
            xQueueOverwrite(xDisplayQueue, &hrData);

            // xAPIQueue: size-30 FIFO — no data loss even if network is slow
            if (xQueueSend(xAPIQueue, &hrData, pdMS_TO_TICKS(20)) != pdTRUE) {
                Serial.println("[HR] WARN: API queue full — reading dropped.");
            }

            Serial.printf("[HR] BPM: %d | SpO2: %.0f%% | Finger: %s\n",
                          avgBPM,
                          hrData.spO2,
                          hrData.fingerDetected ? "YES" : "NO");
        }

        // ═══ STEP 7: Precise sleep ════════════════════════════════
        // vTaskDelayUntil updates xLastWake to (xLastWake + xSamplePeriod)
        // before sleeping. If this cycle took 5 ms, we sleep 35 ms.
        // If it took 38 ms, we sleep 2 ms. Period is always exactly 40 ms.
        vTaskDelayUntil(&xLastWake, xSamplePeriod);
    }
}
