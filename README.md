# Multi-Vital Health Monitor 🩺

A comprehensive, real-time wearable health monitor built on the **ESP32** utilizing **FreeRTOS** for robust multitasking. It reads vitals from various I2C sensors and streams the data over WiFi to a beautiful local **Node.js Dashboard**.

## 🌟 Features

- **Heart Rate & SpO2:** High-precision pulse oximetry using the MAX30102 / MAX30105.
- **Body Temperature:** Clinical-grade temperature tracking via the MAX30205 sensor.
- **Motion & Fall Detection:** 6-DoF IMU (MPU6050) tracking acceleration and gyroscope data to detect sudden falls.
- **Real-Time OLED Display:** On-device SSD1306 OLED screen for instant local feedback.
- **FreeRTOS Architecture:** Dedicated concurrent tasks for sensors, display, API networking, and Over-The-Air (OTA) updates using queues and mutexes.
- **Live Web Dashboard:** A Node.js/Express backend that receives JSON payloads from the ESP32 and broadcasts them to a modern, vibrant web interface via Server-Sent Events (SSE).

## 📁 Project Structure

- `src/` - ESP32 C++ source code organized into modular FreeRTOS tasks.
- `include/` - Core configuration headers (`config.h`, `types.h`).
- `lib/` - Custom or modified libraries.
- `server/` - Node.js Express backend and static frontend dashboard files.
- `platformio.ini` - PlatformIO build configuration.

## 🚀 Getting Started

### 1. Hardware Setup
Connect the following sensors to the ESP32 I2C pins (Default: **SDA = 21, SCL = 22**):
- MAX30105 / MAX30102 (Address `0x57`)
- MAX30205 (Address `0x58`)
- MPU6050 (Address `0x68`)
- SSD1306 OLED (Address `0x3C`)

### 2. Configure the Firmware
Open `include/config.h` and update your WiFi credentials and your computer's local IP address:
```cpp
#define WIFI_SSID           "Your_WiFi_SSID"
#define WIFI_PASSWORD       "Your_WiFi_Password"
#define API_ENDPOINT        "http://<YOUR_COMPUTER_IP>:3000/api/vitals"
```

### 3. Flash the ESP32
Use **PlatformIO** to build and upload the firmware to your ESP32:
```bash
pio run -t upload
```

### 4. Run the Dashboard Server
Navigate to the `server/` directory, install dependencies, and start the local server:
```bash
cd server
npm install
npm start
```
Open your browser and navigate to **`http://localhost:3000`** to view the live dashboard!

## 🛠️ Built With

- [PlatformIO](https://platformio.org/)
- [FreeRTOS](https://freertos.org/)
- [Node.js](https://nodejs.org/)
- [Express](https://expressjs.com/)
- Vanilla CSS & HTML5

## 📝 License
This project is open-source and available under the MIT License.
