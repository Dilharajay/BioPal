# Multi-Vital Health Monitor 🩺

A comprehensive, real-time wearable health monitor built on the **ESP32** utilizing **FreeRTOS** for robust multitasking. It reads vitals from various I2C sensors and streams the data over WiFi to a beautiful local **Node.js Dashboard**.

## 🌟 Features

- **Heart Rate & SpO2:** High-precision pulse oximetry using the MAX30102 / MAX30105.
- **Body Temperature:** Clinical-grade temperature tracking via the MAX30205 sensor.
- **Motion & Fall Detection:** 6-DoF IMU (MPU6050) tracking acceleration and gyroscope data to detect sudden falls.
- **Real-Time OLED Display:** On-device SSD1306 OLED screen for instant local feedback.
- **FreeRTOS Architecture:** Dedicated concurrent tasks for sensors, display, API networking, and Over-The-Air (OTA) updates using queues and mutexes.
- **Interactive UART CLI:** A fully-featured, reliable command-line interface over Serial (`med_mon>`) for dynamically configuring WiFi and API settings without recompiling the code, featuring live sensor streaming and diagnostic controls.
- **Live Web Dashboard:** A Python Flask backend that receives JSON payloads from the ESP32 and broadcasts them to a modern, vibrant web interface via Server-Sent Events (SSE).

## 📁 Project Structure

- `src/` - ESP32 C++ source code organized into modular FreeRTOS tasks.
- `include/` - Core configuration headers (`config.h`, `types.h`).
- `lib/` - Custom or modified libraries.
- `server/` - Python Flask backend and static frontend dashboard files.
- `platformio.ini` - PlatformIO build configuration.

## 🚀 Getting Started

### 1. Hardware Setup
Connect the following sensors to the ESP32 I2C pins (Default: **SDA = 21, SCL = 22**):
- MAX30105 / MAX30102 (Address `0x57`)
- MAX30205 (Address `0x58`)
- MPU6050 (Address `0x68`)
- SSD1306 OLED (Address `0x3C`)

### 2. Flash the ESP32
Use **PlatformIO** to build and upload the firmware to your ESP32:
```bash
pio run -t upload
```

### 3. Device Configuration via UART CLI
You no longer need to hardcode credentials in your code! Once the device boots up, open your Serial Monitor (at `115200` baud) to access the interactive CLI.

At the `med_mon>` prompt, you can easily set up your device:
```text
med_mon> set wifi Your_WiFi_SSID Your_WiFi_Password
med_mon> set api http://<YOUR_COMPUTER_IP>:3000/api/vitals Your_Token
med_mon> show
med_mon> reboot
```
*Note: Any settings changed will automatically be saved to Non-Volatile Storage (NVS) across reboots.*

### 4. Run the Dashboard Server
Navigate to the `server/` directory, install dependencies, and start the local server:
```bash
cd server
pip install -r requirements.txt
python server.py
```
Open your browser and navigate to **`http://localhost:3000`** to view the live dashboard!

## 💻 CLI Commands Reference

- `help` - Show the help menu
- `show` / `status` - Display current config and system health
- `read` - Read a single sensor snapshot
- `stream` / `monitor` - Continuously output live sensor readings to the console (press any key to stop)
- `start` / `debug on` - Enable live diagnostic logging output
- `stop` / `debug off` - Disable live diagnostic logging output
- `logs` - Show EEPROM log history
- `reboot` / `restart` - Reboot the ESP32 chip
- `set wifi <ssid> <password>` - Configure WiFi network
- `set api <url> <token>` - Configure the API telemetry endpoint
- `set time <YYYY-MM-DD> <HH:MM:SS>` - Manually sync the on-device RTC

## 🛠️ Built With

- [PlatformIO](https://platformio.org/)
- [FreeRTOS](https://freertos.org/)
- [Python](https://www.python.org/)
- [Flask](https://flask.palletsprojects.com/)
- Vanilla CSS & HTML5

## 📝 License
This project is open-source and available under the MIT License.
