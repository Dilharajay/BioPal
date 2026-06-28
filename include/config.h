#pragma once
// ================================================================
// config.h — All compile-time constants in one place
// Edit this file to match your hardware wiring and network setup.
// ================================================================

// ── I2C BUS ─────────────────────────────────────────────────────
#define PIN_SDA             21
#define PIN_SCL             22

// ── I2C DEVICE ADDRESSES ────────────────────────────────────────
// CRITICAL: MPU6050 AD0 pin must be tied to 3.3V
//           This changes its address from 0x68 → 0x69
//           and avoids a collision with DS3231 (fixed at 0x68)
#define ADDR_SSD1306        0x3C
#define ADDR_MAX30102       0x57
#define ADDR_MAX30205       0x48
#define ADDR_MPU6050        0x69  // AD0 to 3.3V !
#define ADDR_DS1307         0x68

// ── OLED ────────────────────────────────────────────────────────
#define OLED_WIDTH          128
#define OLED_HEIGHT          64
#define OLED_RESET_PIN       -1   // -1 = shared reset via software

// ── WIFI ────────────────────────────────────────────────────────
#define WIFI_SSID           "SLT-4G-87D7"
#define WIFI_PASS           "HJBT5JD1NY0"
#define WIFI_TIMEOUT_MS     15000

// ── API ENDPOINT ────────────────────────────────────────────────
#define API_ENDPOINT        "http://192.168.1.100:3000/api/vitals"
#define API_AUTH_TOKEN      "Bearer your-token-here"
#define API_TIMEOUT_MS      5000

// ── OTA ─────────────────────────────────────────────────────────
#define OTA_HOSTNAME        "health-monitor"
#define OTA_PASSWORD        "otapassword"

// ── TASK STACK SIZES (bytes) ────────────────────────────────────
// Tune by checking uxTaskGetStackHighWaterMark() at runtime.
// Safe rule: headroom > 100 words (400 bytes). Double if unsure.
#define STACK_HEART_RATE    4096
#define STACK_BODY_TEMP     4096
#define STACK_MOTION        2048
#define STACK_RTC           2048
#define STACK_DISPLAY       3072
#define STACK_API           8192   // HTTP client needs deep stack
#define STACK_OTA           4096

// ── TASK PRIORITIES ─────────────────────────────────────────────
// Range: 0 (idle) to 24 (max). Arduino loop() runs at priority 1.
// Higher = runs first when competing for CPU.
#define PRIO_HEART_RATE     5   // Highest: exact 25 Hz waveform capture
#define PRIO_BODY_TEMP      4   // High: state-machine check every 10 ms
#define PRIO_MOTION         4   // High: 10 Hz IMU sampling
#define PRIO_RTC            3   // Medium: 1 Hz timestamp update
#define PRIO_DISPLAY        3   // Medium: 2 Hz screen refresh
#define PRIO_API            2   // Low: network latency overwhelms jitter
#define PRIO_OTA            2   // Low: rare event

// ── CORE PINNING ────────────────────────────────────────────────
// Core 1: sensor tasks — isolated from WiFi radio interrupts on Core 0
// Core 0: WiFi, API, OTA, display (WiFi driver requires Core 0)
#define CORE_HEART_RATE     1
#define CORE_BODY_TEMP      1
#define CORE_MOTION         1
#define CORE_RTC            0
#define CORE_DISPLAY        0
#define CORE_API            0   // WiFi requires Core 0
#define CORE_OTA            0   // WiFi requires Core 0

// ── TIMING PERIODS (ms) ─────────────────────────────────────────
#define PERIOD_HR_SAMPLE    40      // 25 Hz — waveform capture rate
#define PERIOD_HR_PUBLISH   1000    // 1 Hz  — push result to queues
#define PERIOD_TEMP_CHECK   10      // state-machine poll period
#define PERIOD_TEMP_CONV    50      // MAX30205 one-shot conversion time
#define PERIOD_TEMP_HOLD    2000    // publish temperature every 2 s
#define PERIOD_MOTION       100     // 10 Hz IMU reads
#define PERIOD_RTC          1000    // 1 Hz RTC reads
#define PERIOD_DISPLAY      500     // 2 Hz OLED refresh
#define PERIOD_API_MIN      2000    // min gap between HTTP POSTs
#define PERIOD_OTA          100     // OTA handler poll rate
#define PERIOD_PAGE_FLIP    5000    // auto-advance display page every 5 s

// ── MAX30102 SENSOR CONFIG ──────────────────────────────────────
#define HR_LED_POWER        60      // LED brightness 0–255
#define HR_SAMPLE_AVG       4       // hardware averaging: 4 raw → 1 output
#define HR_LED_MODE         2       // 2 = Red + IR (required for SpO2)
#define HR_SAMPLE_RATE      100     // raw samples/sec before averaging
#define HR_PULSE_WIDTH      411     // µs — max width = 18-bit ADC resolution
#define HR_ADC_RANGE        4096    // nA full-scale
#define HR_IR_THRESHOLD     50000UL // IR > threshold → finger detected
#define SPO2_BUF_LEN        100     // sample buffer length (4 seconds at 25 Hz)
#define HR_RATE_BUF         4       // rolling average over last N beats

// ── MPU6050 SENSOR CONFIG ───────────────────────────────────────
#define FALL_THRESHOLD      30.0f   // m/s² magnitude → fall alert (3g ≈ 29.4)

// ── QUEUE DEPTHS ────────────────────────────────────────────────
#define Q_DISPLAY_LEN       1       // display: always-latest (overwrite mode)
#define Q_API_LEN           30      // API: buffer 30 s of data during outages
