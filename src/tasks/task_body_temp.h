#pragma once
#include <Arduino.h>

// ================================================================
// task_body_temp.h
// Pattern: State Machine
//   MAX30205 clinical temperature sensor
//   States: TRIGGER → WAITING(50 ms) → READ → TRIGGER
//   Publishes every 2 seconds. Never blocks CPU for conversion time.
// ================================================================

void vBodyTempTask(void *pvParameters);
