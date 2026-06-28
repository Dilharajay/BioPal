#include "task_ota.h"
#include "../../include/config.h"
#include "../../include/rtos_handles.h"
#include "../logger.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

// ================================================================
// task_ota.cpp — Over-The-Air firmware update handler
//
// Initialises ArduinoOTA once (requires WiFi to already be connected).
// Calls ArduinoOTA.handle() every 100 ms to service incoming update
// requests from the PlatformIO upload tool or the ESP32 OTA library.
//
// HOW TO FLASH OVER WIFI:
//   1. In platformio.ini, uncomment:
//        upload_protocol = espota
//        upload_port     = health-monitor.local
//        upload_flags    = --auth=otapassword
//   2. Run: pio run --target upload
//   3. The running ESP32 will receive the binary over WiFi and reboot.
//
// SECURITY NOTE:
//   OTA_PASSWORD in config.h prevents unauthorized firmware uploads.
//   For production devices, also consider disabling OTA after initial deploy.
//
// IMPORTANT: ArduinoOTA MUST run on Core 0 because it uses the
//            WiFi stack which is pinned to Core 0 on ESP32.
// ================================================================

void vOTATask(void *pvParameters) {
    Logger::info("OTA", "Task started on Core %d", xPortGetCoreID());

    // Wait for WiFi before setting up OTA — ArduinoOTA.begin() needs a valid IP
    Logger::info("OTA", "Waiting for WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    Logger::info("OTA", "Connected to WiFi.");

    // ── Configure OTA ─────────────────────────────────────────────
    // Hostname appears in mDNS as "health-monitor.local" on your network.
    ArduinoOTA.setHostname(OTA_HOSTNAME);

    // Password prevents unauthorized uploads to the device.
    ArduinoOTA.setPassword(OTA_PASSWORD);

    // ── OTA event callbacks ───────────────────────────────────────
    // These run during an active OTA session (not in the normal task loop).
    // Keep them brief — they execute in a high-priority context.
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Logger::info("OTA", "Update started: %s", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        Logger::info("OTA", "Update complete. Rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Print progress every 10% to avoid flooding Serial
        static uint8_t lastPct = 0;
        uint8_t pct = (progress * 100) / total;
        if (pct / 10 != lastPct / 10) {
            Logger::info("OTA", "Progress: %u%%", pct);
            lastPct = pct;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        const char* errStr = "Unknown";
        switch (error) {
            case OTA_AUTH_ERROR:    errStr = "Auth failed";    break;
            case OTA_BEGIN_ERROR:   errStr = "Begin failed";   break;
            case OTA_CONNECT_ERROR: errStr = "Connect failed"; break;
            case OTA_RECEIVE_ERROR: errStr = "Receive failed"; break;
            case OTA_END_ERROR:     errStr = "End failed";     break;
        }
        Logger::error("OTA", "ERROR[%u]: %s", error, errStr);
    });

    // ── Start the OTA service ──────────────────────────────────────
    // Registers the mDNS entry "health-monitor.local" and opens the
    // UDP port that the ArduinoOTA protocol uses.
    ArduinoOTA.begin();

    Logger::info("OTA", "Ready. Device: %s.local", OTA_HOSTNAME);

    // ── Poll loop ─────────────────────────────────────────────────
    // handle() returns immediately if no OTA request is in progress.
    // If an upload starts, handle() blocks here until it completes
    // (or fails), then the device reboots automatically.
    for (;;) {
        ArduinoOTA.handle();
        vTaskDelay(pdMS_TO_TICKS(PERIOD_OTA)); // 100 ms poll rate
    }
}
