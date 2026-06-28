#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ================================================================
// rtos_handles.h — extern declarations for all shared RTOS objects
//
// PATTERN:
//   - DEFINED once in rtos_handles.cpp  (one definition rule)
//   - DECLARED here with extern         (include in any file that needs them)
//   - All initialized to NULL in .cpp, assigned in setup() before task creation
// ================================================================

// ── MUTEXES ─────────────────────────────────────────────────────
extern SemaphoreHandle_t xI2CMutex;    // Guards the shared I2C bus

// ── QUEUES ──────────────────────────────────────────────────────
extern QueueHandle_t xDisplayQueue;    // SensorTasks → DisplayTask  (size 1, overwrite)
extern QueueHandle_t xAPIQueue;        // SensorTasks → APITask      (size 30, FIFO)

// ── TASK HANDLES ────────────────────────────────────────────────
// Used in loop() for stack watermark monitoring and diagnostics
extern TaskHandle_t hHeartRateTask;
extern TaskHandle_t hBodyTempTask;
extern TaskHandle_t hMotionTask;
extern TaskHandle_t hRTCTask;
extern TaskHandle_t hDisplayTask;
extern TaskHandle_t hAPITask;
extern TaskHandle_t hOTATask;
