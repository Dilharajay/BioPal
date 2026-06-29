#pragma once
#include <Arduino.h>
void vDisplayTask(void *pvParameters);
bool initDisplay();
void showBootScreen();
void showWiFiScreen();
void showOTAScreen(uint8_t progress);
