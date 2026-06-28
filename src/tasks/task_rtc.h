#pragma once
#include <Arduino.h>
void vRTCTask(void *pvParameters);
void setRTCTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);
