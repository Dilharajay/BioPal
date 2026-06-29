#include "cli.h"
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "types.h"
#include "rtos_handles.h"
#include "config_store.h"
#include "logger.h"
#include "../tasks/task_rtc.h"

static String cliBuffer = "";
static uint32_t lastHealthReport = 0;
static bool isStreaming = false;
static uint32_t lastStreamTime = 0;

static void printStatus() {
    Serial.println("\n─── System Health ─────────────────────────────");
    Serial.printf("  Free heap   : %6u bytes\n", esp_get_free_heap_size());
    Serial.printf("  Active tasks: %6u\n",       uxTaskGetNumberOfTasks());
    Serial.printf("  WiFi RSSI   : %6d dBm\n",   WiFi.RSSI());
    Serial.printf("  Uptime      : %6lu s\n",    millis() / 1000);

    Serial.println("  Stack HWM (words = 4 bytes each):");
    if (hHeartRateTask) Serial.printf("    HeartRate : %4u words\n", uxTaskGetStackHighWaterMark(hHeartRateTask));
    if (hBodyTempTask)  Serial.printf("    BodyTemp  : %4u words\n", uxTaskGetStackHighWaterMark(hBodyTempTask));
    if (hMotionTask)    Serial.printf("    Motion    : %4u words\n", uxTaskGetStackHighWaterMark(hMotionTask));
    if (hRTCTask)       Serial.printf("    RTC       : %4u words\n", uxTaskGetStackHighWaterMark(hRTCTask));
    if (hDisplayTask)   Serial.printf("    Display   : %4u words\n", uxTaskGetStackHighWaterMark(hDisplayTask));
    if (hAPITask)       Serial.printf("    API       : %4u words\n", uxTaskGetStackHighWaterMark(hAPITask));
    if (hOTATask)       Serial.printf("    OTA       : %4u words\n", uxTaskGetStackHighWaterMark(hOTATask));

    Serial.printf("  API queue   : %4u / %u items waiting\n",
                  uxQueueMessagesWaiting(xAPIQueue), Q_API_LEN);
    Serial.println("───────────────────────────────────────────────");
}

void printPrompt() {
    Serial.print("biopal> ");
}

static void processCLI(const String& cmdLine) {
    String cmd = cmdLine;
    cmd.trim();
    if (cmd.startsWith("set wifi ")) {
        int space1 = cmd.indexOf(' ', 9);
        if (space1 != -1) {
            String ssid = cmd.substring(9, space1);
            String pass = cmd.substring(space1 + 1);
            saveConfig(ssid, pass, "", "");
            Serial.println("WiFi config saved. Reboot to apply.");
        }
    } else if (cmd.startsWith("set api ")) {
        int space1 = cmd.indexOf(' ', 8);
        if (space1 != -1) {
            String url = cmd.substring(8, space1);
            String token = cmd.substring(space1 + 1);
            saveConfig("", "", url, token);
            Serial.println("API config saved.");
        }
    } else if (cmd.startsWith("set time ")) {
        if (cmd.length() >= 28) {
            uint16_t year = cmd.substring(9, 13).toInt();
            uint8_t month = cmd.substring(14, 16).toInt();
            uint8_t day = cmd.substring(17, 19).toInt();
            uint8_t hour = cmd.substring(20, 22).toInt();
            uint8_t min = cmd.substring(23, 25).toInt();
            uint8_t sec = cmd.substring(26, 28).toInt();
            setRTCTime(year, month, day, hour, min, sec);
            addLog("Time manually set via CLI");
        } else {
            Serial.println("Usage: set time YYYY-MM-DD HH:MM:SS");
        }
    } else if (cmd == "show" || cmd == "status") {
        Serial.println("--- Current Config ---");
        Serial.println("WiFi SSID: " + current_wifi_ssid);
        Serial.println("WiFi Pass: " + current_wifi_pass);
        Serial.println("API Endpt: " + current_api_endpoint);
        Serial.println("API Token: " + current_api_token);
        Serial.println("----------------------");
        printStatus();
    } else if (cmd == "read") {
        VitalData_t currentData;
        if (xQueuePeek(xDisplayQueue, &currentData, 0) == pdTRUE) {
            Serial.println("--- Current Sensor Readings ---");
            if (currentData.rtcValid) {
                Serial.printf("Time  : %s %s\n", currentData.dateStr, currentData.timeStr);
            } else {
                Serial.println("Time  : RTC Unavailable");
            }
            if (currentData.hrValid) {
                Serial.printf("HR    : %.0f BPM\n", currentData.heartRate);
                Serial.printf("SpO2  : %.0f %%\n", currentData.spO2);
            } else {
                Serial.println("HR    : Calculating/Unavailable");
            }
            if (currentData.bodyTempValid) {
                Serial.printf("Temp  : %.2f °C\n", currentData.bodyTempC);
            } else {
                Serial.println("Temp  : Unavailable");
            }
            Serial.printf("Motion: Mag %.2f m/s² (Fall? %s)\n", currentData.accelMag, currentData.fallDetected ? "YES" : "NO");
            Serial.println("-------------------------------");
        } else {
            Serial.println("Error: No sensor data available yet.");
        }
    } else if (cmd == "stream" || cmd == "monitor") {
        isStreaming = true;
        Serial.println("\n[INFO] Sensor stream started. Press any key to stop.");
        return; 
    } else if (cmd == "start" || cmd == "debug on") {
        debugModeEnabled = true;
        saveConfig("", "", "", "");
        Logger::setLevel(LOG_LEVEL_DEBUG);
        Serial.println("Debug mode ON (live logging enabled).");
    } else if (cmd == "stop" || cmd == "debug off") {
        debugModeEnabled = false;
        saveConfig("", "", "", "");
        Logger::setLevel(LOG_LEVEL_NONE);
        Serial.println("Debug mode OFF (live logging paused).");
    } else if (cmd == "logs") {
        printLogs();
    } else if (cmd == "reboot" || cmd == "restart") {
        Serial.println("Rebooting...");
        esp_restart();
    } else if (cmd == "help") {
        Serial.println("\n--- Available Commands ---");
        Serial.println("help                    - Show this help menu");
        Serial.println("show / status           - Display current config and system health");
        Serial.println("read                    - Read single sensor snapshot");
        Serial.println("stream / monitor        - Output live sensor readings periodically");
        Serial.println("start / debug on        - Enable live diagnostic logging");
        Serial.println("stop / debug off        - Disable live diagnostic logging");
        Serial.println("logs                    - Show EEPROM log history");
        Serial.println("reboot / restart        - Reboot the ESP32 chip");
        Serial.println("\nConfiguration Commands:");
        Serial.println("set wifi <ssid> <password>");
        Serial.println("set api <url> <token>");
        Serial.println("set time <YYYY-MM-DD> <HH:MM:SS>");
        Serial.println("--------------------------");
    } else if (cmd.length() > 0) {
        Serial.println("Unknown command. Type 'help' for a list of commands.");
    }
    printPrompt();
}

void cli_loop() {
    if (isStreaming) {
        if (Serial.available() > 0) {
            while (Serial.available()) Serial.read(); 
            isStreaming = false;
            Serial.println("\n[INFO] Sensor stream stopped.");
            printPrompt();
            cliBuffer = "";
        } else {
            if (millis() - lastStreamTime > 1000) {
                lastStreamTime = millis();
                VitalData_t currentData;
                if (xQueuePeek(xDisplayQueue, &currentData, 0) == pdTRUE) {
                    Serial.printf("[Stream] HR: %.0f BPM | SpO2: %.0f%% | Temp: %.2f C | Accel: %.2f m/s2\n",
                                  currentData.hrValid ? currentData.heartRate : 0,
                                  currentData.hrValid ? currentData.spO2 : 0,
                                  currentData.bodyTempValid ? currentData.bodyTempC : 0,
                                  currentData.accelMag);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cliBuffer.length() > 0) {
                Serial.println();
                processCLI(cliBuffer);
                cliBuffer = "";
            } else {
                Serial.println();
                printPrompt();
            }
        } else if (c == '\b' || c == 127) { 
            if (cliBuffer.length() > 0) {
                cliBuffer.remove(cliBuffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (isprint(c)) {
            if (cliBuffer.length() < 100) {
                cliBuffer += c;
                Serial.print(c);
            }
        }
    }

    if (millis() - lastHealthReport > 10000) {
        lastHealthReport = millis();
        if (debugModeEnabled) {
            Serial.println(); 
            printStatus();
            printPrompt();    
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
}
