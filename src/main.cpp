#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "../include/config.h"
#include "../include/types.h"
#include "../include/rtos_handles.h"
#include "config_store.h"

#include "tasks/task_heart_rate.h"
#include "tasks/task_body_temp.h"
#include "tasks/task_motion.h"
#include "tasks/task_rtc.h"
#include "tasks/task_display.h"
#include "tasks/task_api.h"
#include "tasks/task_ota.h"

// ================================================================
// main.cpp — System entry point
//
// Execution order (single-threaded, before scheduler starts):
//   1. Serial + I2C init
//   2. Create I2C mutex          ← must exist before any task touches I2C
//   3. Create queues             ← must exist before producer tasks start
//   4. Connect WiFi              ← must complete before OTA/API tasks start
//   5. Create all FreeRTOS tasks ← scheduler takes over after setup() returns
//
// loop() runs as a low-priority Arduino task (priority 1).
// It prints a diagnostic health report every 10 seconds.
// ================================================================

// ──────────────────────────────────────────────────────────────────
// Stack overflow hook — called by FreeRTOS when a task's stack is
// exhausted. Placed in IRAM so it can run even if flash is paused.
// ──────────────────────────────────────────────────────────────────
extern "C" void IRAM_ATTR vApplicationStackOverflowHook(
        TaskHandle_t xTask, char *pcTaskName) {
    Serial.printf("\n[FATAL] Stack overflow in task: \"%s\"\n", pcTaskName);
    Serial.println("[FATAL] Increase that task's stack size in config.h");
    // Cannot recover — halt and reboot after a short delay for Serial flush
    delay(3000);
    esp_restart();
}

// ──────────────────────────────────────────────────────────────────
static void connectWiFi() {
    Serial.printf("[WiFi] Connecting to \"%s\"", current_wifi_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(current_wifi_ssid.c_str(), current_wifi_pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_TIMEOUT_MS) {
            Serial.println("\n[WiFi] Timeout — continuing without WiFi.");
            Serial.println("[WiFi] API and OTA will be unavailable.");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] Connected. IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

// ──────────────────────────────────────────────────────────────────
// Helper: create a task and halt if it fails.
// xTaskCreatePinnedToCore() returns pdFAIL if there is not enough
// heap for the task's stack + Task Control Block (~300 bytes).
// ──────────────────────────────────────────────────────────────────
static void createTask(
        TaskFunction_t  fn,
        const char     *name,
        uint32_t        stack,
        UBaseType_t     priority,
        TaskHandle_t   *handle,
        BaseType_t      core) {

    BaseType_t result = xTaskCreatePinnedToCore(
        fn,       // function to run as task
        name,     // debug name (visible in FreeRTOS trace tools)
        stack,    // stack in bytes (note: ESP32 uses bytes, not words)
        NULL,     // pvParameters — we use globals instead
        priority, // FreeRTOS priority (0=idle, 24=max on ESP32)
        handle,   // output: handle for diagnostics
        core      // 0 or 1
    );

    if (result != pdPASS) {
        Serial.printf("[FATAL] Failed to create task \"%s\". "
                      "Not enough heap? Free: %u bytes\n",
                      name, esp_get_free_heap_size());
        // Halt — partial task set causes unpredictable behaviour
        while (true) { delay(1000); }
    }
    Serial.printf("[Setup] Task %-16s created. Stack: %5u B  "
                  "Priority: %u  Core: %u\n",
                  name, stack, (unsigned)priority, (unsigned)core);
}

// ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300); // Let host serial monitor attach
    Serial.println("\n\n========================================");
    Serial.println("    Multi-Vital Health Monitor");
    Serial.println("    ESP32 + FreeRTOS");
    Serial.println("========================================");

    // Load configuration from NVS
    loadConfig();

    // ── Step 1: I2C bus init ─────────────────────────────────────
    // Wire.begin() configures the hardware I2C peripheral.
    // All sensors and OLED share this single bus instance.
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000); // 100 kHz Standard Mode — safer for breadboards and some OLEDs
    Serial.printf("[Setup] I2C init. SDA=%d SCL=%d @ 100 kHz\n", PIN_SDA, PIN_SCL);

    // ── Step 2: I2C Mutex ─────────────────────────────────────────
    // MUST be created before any task that uses I2C.
    // If this fails: heap is too small to even start. Fatal.
    xI2CMutex = xSemaphoreCreateMutex();
    if (xI2CMutex == NULL) {
        Serial.println("[FATAL] Cannot allocate I2C mutex. Halting.");
        while (true) { delay(1000); }
    }
    Serial.println("[Setup] I2C mutex created.");

    // ── Step 3: Queues ────────────────────────────────────────────
    // xQueueCreate(length, itemSizeBytes) allocates from the FreeRTOS heap.
    // DisplayQueue: 1 slot — sensor tasks overwrite it, display peeks.
    xDisplayQueue = xQueueCreate(Q_DISPLAY_LEN, sizeof(VitalData_t));
    if (xDisplayQueue == NULL) {
        Serial.println("[FATAL] Cannot allocate DisplayQueue. Halting.");
        while (true) { delay(1000); }
    }

    // APIQueue: 30 slots — buffers readings during network outages.
    // Memory cost: 30 × sizeof(VitalData_t) ≈ 30 × 72 = ~2160 bytes
    xAPIQueue = xQueueCreate(Q_API_LEN, sizeof(VitalData_t));
    if (xAPIQueue == NULL) {
        Serial.println("[FATAL] Cannot allocate APIQueue. Halting.");
        while (true) { delay(1000); }
    }
    Serial.printf("[Setup] Queues created. VitalData_t size: %u bytes\n",
                  sizeof(VitalData_t));

    // ── Step 4: WiFi ──────────────────────────────────────────────
    // Connect before creating OTA and API tasks.
    // Those tasks will also handle reconnection internally, but starting
    // connected avoids a race condition during first OTA.begin() call.
    connectWiFi();

    // ── Step 5: Create tasks ──────────────────────────────────────
    Serial.println("[Setup] Creating tasks...");

    //        function          name              stack             priority        handle          core
    createTask(vHeartRateTask, "HeartRateTask", STACK_HEART_RATE, PRIO_HEART_RATE, &hHeartRateTask, CORE_HEART_RATE);
    createTask(vBodyTempTask,  "BodyTempTask",  STACK_BODY_TEMP,  PRIO_BODY_TEMP,  &hBodyTempTask,  CORE_BODY_TEMP);
    createTask(vMotionTask,    "MotionTask",    STACK_MOTION,     PRIO_MOTION,     &hMotionTask,    CORE_MOTION);
    createTask(vRTCTask,       "RTCTask",       STACK_RTC,        PRIO_RTC,        &hRTCTask,       CORE_RTC);
    createTask(vDisplayTask,   "DisplayTask",   STACK_DISPLAY,    PRIO_DISPLAY,    &hDisplayTask,   CORE_DISPLAY);
    createTask(vAPITask,       "APITask",       STACK_API,        PRIO_API,        &hAPITask,       CORE_API);
    createTask(vOTATask,       "OTATask",       STACK_OTA,        PRIO_OTA,        &hOTATask,       CORE_OTA);

    Serial.println("[Setup] All tasks created. Scheduler is now in control.");
    Serial.printf("[Setup] Free heap after task creation: %u bytes\n",
                  esp_get_free_heap_size());
    Serial.println("========================================\n");
    // setup() returns here. The FreeRTOS scheduler immediately starts
    // dispatching the highest-priority ready task.
}

// ──────────────────────────────────────────────────────────────────
// loop() runs as the Arduino "loopTask" at priority 1.
// It only wakes every 10 seconds to print diagnostics.
// It has no sensor or network work to do — that is all in the tasks.
// ──────────────────────────────────────────────────────────────────
String cliBuffer = "";
uint32_t lastHealthReport = 0;

void processCLI(const String& cmd) {
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
    } else if (cmd == "show") {
        Serial.println("--- Current Config ---");
        Serial.println("WiFi SSID: " + current_wifi_ssid);
        Serial.println("WiFi Pass: " + current_wifi_pass);
        Serial.println("API Endpt: " + current_api_endpoint);
        Serial.println("API Token: " + current_api_token);
        Serial.println("----------------------");
    } else if (cmd == "reboot") {
        Serial.println("Rebooting...");
        esp_restart();
    } else {
        Serial.println("Unknown command. Available:");
        Serial.println("  set wifi <ssid> <password>");
        Serial.println("  set api <url> <token>");
        Serial.println("  show");
        Serial.println("  reboot");
    }
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cliBuffer.length() > 0) {
                processCLI(cliBuffer);
                cliBuffer = "";
            }
        } else {
            cliBuffer += c;
        }
    }

    if (millis() - lastHealthReport > 10000) {
        lastHealthReport = millis();
        // ── System health report ─────────────────────────────────────
        Serial.println("\n─── System Health ─────────────────────────────");
        Serial.printf("  Free heap   : %6u bytes\n", esp_get_free_heap_size());
        Serial.printf("  Active tasks: %6u\n",       uxTaskGetNumberOfTasks());
        Serial.printf("  WiFi RSSI   : %6d dBm\n",   WiFi.RSSI());
        Serial.printf("  Uptime      : %6lu s\n",    millis() / 1000);

        // ── Stack high-water marks ───────────────────────────────────
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
    
    // Sleep briefly to keep CLI responsive
    vTaskDelay(pdMS_TO_TICKS(50));
}
