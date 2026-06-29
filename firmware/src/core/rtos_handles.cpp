#include "rtos_handles.h"

// ================================================================
// rtos_handles.cpp — Actual storage for all shared RTOS objects
//
// All initialized to NULL here. setup() in main.cpp creates each
// object and assigns the handle before any task is launched.
// ================================================================

SemaphoreHandle_t xI2CMutex     = NULL;

QueueHandle_t     xDisplayQueue = NULL;
QueueHandle_t     xAPIQueue     = NULL;

TaskHandle_t      hHeartRateTask = NULL;
TaskHandle_t      hBodyTempTask  = NULL;
TaskHandle_t      hMotionTask    = NULL;
TaskHandle_t      hRTCTask       = NULL;
TaskHandle_t      hDisplayTask   = NULL;
TaskHandle_t      hAPITask       = NULL;
TaskHandle_t      hOTATask       = NULL;
