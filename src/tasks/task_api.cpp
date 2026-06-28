#include "task_api.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"
#include "../config_store.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ================================================================
// task_api.cpp — WiFi HTTP POST to REST API
//
// Blocks on xAPIQueue with portMAX_DELAY — uses ZERO CPU when idle.
// Wakes the instant a new VitalData_t is placed in the queue.
// Checks WiFi, reconnects if dropped, builds JSON payload, POSTs.
// Rate-limited to PERIOD_API_MIN (2 s) to avoid hammering the server.
// ================================================================

static uint32_t totalSent = 0;

// ──────────────────────────────────────────────────────────────────
static bool ensureWiFi() {
    if (WiFi.status() == WL_CONNECTED) return true;

    Serial.println("[API] WiFi lost. Reconnecting...");
    WiFi.reconnect();

    for (uint8_t i = 0; i < 20; i++) {          // 20 × 500 ms = 10 s timeout
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[API] Reconnected. IP: %s\n",
                          WiFi.localIP().toString().c_str());
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Serial.println("[API] Reconnect failed.");
    return false;
}

// ──────────────────────────────────────────────────────────────────
static void buildJSON(const VitalData_t &d, String &out) {
    // StaticJsonDocument: allocates on the stack (no heap fragmentation).
    // 384 bytes comfortably holds all fields; compute with ArduinoJson assistant.
    StaticJsonDocument<384> doc;

    // Heart rate
    doc["timestamp"]     = d.epoch;
    doc["uptime"]        = d.uptime;
    doc["heartRate"]     = serialized(String(d.heartRate, 1));
    doc["spO2"]          = serialized(String(d.spO2,      1));
    doc["fingerOn"]      = d.fingerDetected;
    doc["hrValid"]       = d.hrValid;
    doc["spo2Valid"]     = d.spo2Valid;

    // Body temperature
    doc["bodyTempC"]     = serialized(String(d.bodyTempC, 2));
    doc["bodyTempF"]     = serialized(String(d.bodyTempF, 2));
    doc["bodyTempValid"] = d.bodyTempValid;

    // Motion
    doc["accelX"]        = serialized(String(d.accelX,   3));
    doc["accelY"]        = serialized(String(d.accelY,   3));
    doc["accelZ"]        = serialized(String(d.accelZ,   3));
    doc["accelMag"]      = serialized(String(d.accelMag, 3));
    doc["gyroX"]         = serialized(String(d.gyroX,    2));
    doc["gyroY"]         = serialized(String(d.gyroY,    2));
    doc["gyroZ"]         = serialized(String(d.gyroZ,    2));
    doc["fallDetected"]  = d.fallDetected;

    // Time
    doc["time"]          = d.timeStr;
    doc["date"]          = d.dateStr;
    doc["wifiRSSI"]      = d.wifiRSSI;

    serializeJson(doc, out);
}

// ──────────────────────────────────────────────────────────────────
void vAPITask(void *pvParameters) {
    Serial.printf("[API] Task started on Core %d\n", xPortGetCoreID());

    VitalData_t data = {};

    for (;;) {

        // ── Block until new data arrives ──────────────────────────
        // portMAX_DELAY = wait forever.
        // This task uses 0% CPU while the queue is empty.
        // The instant a sensor task calls xQueueSend(), this unblocks.
        if (xQueueReceive(xAPIQueue, &data, portMAX_DELAY) != pdTRUE) continue;

        // ── WiFi check / reconnect ────────────────────────────────
        if (!ensureWiFi()) {
            Serial.println("[API] Skipping send — no WiFi.");
            continue;
        }

        // ── Update RSSI in data struct ────────────────────────────
        data.wifiRSSI = (int8_t)WiFi.RSSI();

        // ── Build JSON payload ────────────────────────────────────
        String payload;
        buildJSON(data, payload);

        // ── HTTP POST ─────────────────────────────────────────────
        HTTPClient http;
        http.begin(current_api_endpoint.c_str());
        http.setTimeout(API_TIMEOUT_MS);
        http.addHeader("Content-Type",  "application/json");
        http.addHeader("Authorization", current_api_token.c_str());

        int code = http.POST(payload);

        if (code > 0) {
            totalSent++;
            Serial.printf("[API] POST #%u → HTTP %d | HR:%.0f SpO2:%.0f T:%.1fC\n",
                          totalSent, code,
                          data.heartRate, data.spO2, data.bodyTempC);
            if (code >= 400) {
                // Server responded with an error — log first 120 chars of body
                String body = http.getString();
                Serial.printf("[API] Server error body: %s\n",
                              body.substring(0, 120).c_str());
            }
        } else {
            // Negative code = transport failure (DNS, TCP timeout, etc.)
            Serial.printf("[API] Connection failed: %s\n",
                          HTTPClient::errorToString(code).c_str());
        }

        http.end();  // MUST be called to close socket and free buffers

        // ── Rate limit ────────────────────────────────────────────
        // vTaskDelay here (not vTaskDelayUntil) because we want a fixed
        // gap AFTER the send completes — not a fixed period from start.
        vTaskDelay(pdMS_TO_TICKS(PERIOD_API_MIN));
    }
}
