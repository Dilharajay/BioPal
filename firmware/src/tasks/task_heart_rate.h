#pragma once
#include <Arduino.h>

// ================================================================
// task_heart_rate.h
// Pattern: Two-Rate Task
//   - Samples MAX30105 at 25 Hz (every 40 ms) — captures full waveform
//   - Runs checkForBeat() on every sample — real-time BPM
//   - Runs SpO2 algorithm on every 100-sample buffer — clinical SpO2
//   - Publishes to queues at 1 Hz (every 25th sample)
// ================================================================

void vHeartRateTask(void *pvParameters);
