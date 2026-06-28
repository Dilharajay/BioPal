#include "task_display.h"
#include "../../include/config.h"
#include "../../include/types.h"
#include "../../include/rtos_handles.h"

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// ================================================================
// task_display.cpp — SSD1306 128×64 OLED display
//
// Refreshes at 2 Hz (every 500 ms). Auto-cycles through 4 pages:
//   PAGE_HEART  — heart rate, SpO2, finger status
//   PAGE_TEMP   — body temperature in °C and °F
//   PAGE_MOTION — X/Y/Z acceleration, fall detection
//   PAGE_CLOCK  — date, time, WiFi RSSI, uptime
//
// All drawing goes into the framebuffer first (clearDisplay + draw ops).
// display() flushes the entire 1024-byte framebuffer to the physical
// OLED in one I2C burst — minimising the time the I2C mutex is held.
// ================================================================

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
static bool              oledOK = false;

// ── Page cycling ─────────────────────────────────────────────────
static DisplayPage_t currentPage  = PAGE_HEART;
static TickType_t    lastPageFlip = 0;  // tick when we last switched pages

// ──────────────────────────────────────────────────────────────────
static bool initDisplay() {
    // Some OLEDs need a moment to boot after power-on before responding on I2C
    vTaskDelay(pdMS_TO_TICKS(100));

    if (!oled.begin(SSD1306_SWITCHCAPVCC, ADDR_SSD1306)) {
        Serial.printf("[DISP] ERROR: SSD1306 not found at 0x%02X. Trying 0x3D...\n", ADDR_SSD1306);
        if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
            Serial.println("[DISP] ERROR: SSD1306 not found at 0x3D either.");
            return false;
        }
        Serial.println("[DISP] SSD1306 found at 0x3D instead.");
    }
    
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);  // 1 = pixel on (OLED has no colour)
    oled.setTextSize(1);               // 6×8 pixel font (21 chars across 128px)
    oled.setCursor(0, 0);
    oled.println("  Health Monitor");
    oled.println("   Initialising...");
    oled.display();
    Serial.println("[DISP] SSD1306 initialised.");
    return true;
}

// ──────────────────────────────────────────────────────────────────
// PAGE: Heart Rate + SpO2
// Layout (128×64, text size 1 = 6×8 px per char):
//   Row 0 (y=0):  header bar        "  HEART RATE   [1/4]"
//   Row 1 (y=16): large HR value    "  72 bpm"  (textSize 2)
//   Row 2 (y=36): SpO2              "SpO2: 98%"
//   Row 3 (y=48): Finger status     "Finger: ON  Valid: YES"
// ──────────────────────────────────────────────────────────────────
static void drawPageHeart(const VitalData_t &d) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("HEART RATE      [1/4]");

    // Large BPM reading — textSize 2 = 12×16 px per char
    oled.setTextSize(2);
    oled.setCursor(4, 14);
    if (d.fingerDetected && d.hrValid) {
        oled.printf("%.0f bpm", d.heartRate);
    } else {
        oled.print("--- bpm");
    }

    oled.setTextSize(1);
    oled.setCursor(0, 36);
    if (d.spo2Valid) {
        oled.printf("SpO2: %.0f%%", d.spO2);
    } else {
        oled.print("SpO2: ---");
    }

    oled.setCursor(0, 48);
    oled.printf("Finger:%-3s Valid:%-3s",
                d.fingerDetected ? "ON"  : "OFF",
                d.hrValid        ? "YES" : "NO");
}

// ──────────────────────────────────────────────────────────────────
static void drawPageTemp(const VitalData_t &d) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("BODY TEMP       [2/4]");

    oled.setTextSize(2);
    oled.setCursor(4, 14);
    if (d.bodyTempValid) {
        oled.printf("%.1fC", d.bodyTempC);
    } else {
        oled.print("--.-C");
    }

    oled.setTextSize(1);
    oled.setCursor(0, 36);
    if (d.bodyTempValid) {
        oled.printf("     = %.1f F", d.bodyTempF);
    } else {
        oled.print("     = ---.- F");
    }

    oled.setCursor(0, 48);
    oled.printf("Status: %s",
                !d.bodyTempValid ? "Waiting..."  :
                d.bodyTempC < 35.0f ? "Hypothermia" :
                d.bodyTempC > 38.0f ? "Fever!"      : "Normal");
}

// ──────────────────────────────────────────────────────────────────
static void drawPageMotion(const VitalData_t &d) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("MOTION          [3/4]");

    oled.setCursor(0, 12);
    oled.printf("X: %+6.2f  Y: %+6.2f", d.accelX, d.accelY);

    oled.setCursor(0, 22);
    oled.printf("Z: %+6.2f  M: %6.2f",  d.accelZ, d.accelMag);

    oled.setCursor(0, 34);
    oled.printf("Gyro X: %+6.1f d/s",   d.gyroX);

    oled.setCursor(0, 44);
    oled.printf("Gyro Y: %+6.1f d/s",   d.gyroY);

    oled.setCursor(0, 56);
    if (d.fallDetected) {
        // Invert rectangle behind the text for visual alert
        oled.fillRect(0, 54, 128, 10, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(4, 56);
        oled.print("!! FALL DETECTED !!");
        oled.setTextColor(SSD1306_WHITE);
    } else {
        oled.print("Status: Normal");
    }
}

// ──────────────────────────────────────────────────────────────────
static void drawPageClock(const VitalData_t &d) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("DATE & TIME     [4/4]");

    // Large time display — textSize 2
    oled.setTextSize(2);
    oled.setCursor(4, 12);
    if (d.rtcValid) {
        oled.print(d.timeStr);
    } else {
        oled.print("--:--:--");
    }

    oled.setTextSize(1);
    oled.setCursor(0, 34);
    if (d.rtcValid) {
        oled.print(d.dateStr);
    } else {
        oled.print("RTC not set");
    }

    // Uptime formatted as Xd Xh Xm
    uint32_t sec  = d.uptime / 1000;
    uint32_t days = sec / 86400; sec %= 86400;
    uint32_t hrs  = sec / 3600;  sec %= 3600;
    uint32_t mins = sec / 60;
    oled.setCursor(0, 46);
    oled.printf("Up: %ud %uh %um", days, hrs, mins);

    oled.setCursor(0, 56);
    if (d.wifiRSSI != 0) {
        oled.printf("WiFi: %d dBm", d.wifiRSSI);
    } else {
        oled.print("WiFi: disconnected");
    }
}

// ──────────────────────────────────────────────────────────────────
void vDisplayTask(void *pvParameters) {
    Serial.printf("[DISP] Task started on Core %d\n", xPortGetCoreID());

    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        oledOK = initDisplay();
        xSemaphoreGive(xI2CMutex);
    }
    if (!oledOK) {
        Serial.println("[DISP] Task terminating — OLED not found.");
        vTaskDelete(NULL);
        return;
    }

    VitalData_t data     = {};
    TickType_t  xLastWake = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_DISPLAY); // 500 ms
    lastPageFlip = xTaskGetTickCount();

    for (;;) {

        // ── Get latest data (non-destructive peek) ────────────────
        // xQueuePeek reads without removing — SensorTask can overwrite
        // again on the next cycle. Timeout 10 ms handles cold start.
        xQueuePeek(xDisplayQueue, &data, pdMS_TO_TICKS(10));

        // ── Auto-advance page every PERIOD_PAGE_FLIP ms ──────────
        if ((xTaskGetTickCount() - lastPageFlip) >= pdMS_TO_TICKS(PERIOD_PAGE_FLIP)) {
            lastPageFlip = xTaskGetTickCount();
            currentPage  = (DisplayPage_t)((currentPage + 1) % PAGE_COUNT);
        }

        // ── Draw page into framebuffer, then flush to OLED ───────
        // Holding the I2C mutex only while writing to hardware.
        // All drawing is done inside the mutex block because the
        // SSD1306 library communicates over I2C during display().
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

            oled.clearDisplay(); // Zero the 1024-byte RAM framebuffer

            // Draw the appropriate page into the framebuffer
            switch (currentPage) {
                case PAGE_HEART:  drawPageHeart(data);  break;
                case PAGE_TEMP:   drawPageTemp(data);   break;
                case PAGE_MOTION: drawPageMotion(data); break;
                case PAGE_CLOCK:  drawPageClock(data);  break;
                default: break;
            }

            // Flush framebuffer → physical OLED (one I2C burst, ~15 ms)
            oled.display();

            xSemaphoreGive(xI2CMutex);
        }

        vTaskDelayUntil(&xLastWake, xPeriod);
    }
}
